/*
 * Copyright (C) Maxim Zhuchkov - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Maxim Zhuchkov <q3.max.2011@ya.ru>, May 2019
 */

#include "wmt.h"

const char *TerrainTypesStrings[] = {"sand", "sandy brush", "baked earth", "green mud", "red brush", "pink rock", "road", "water", "cliffface", "rubble", "sheetice", "slush", "max"};
unsigned short maptileoffset = 0x01ff;




bool equalstr(char* trg, const char* chk) {
	if(strlen(trg)!=strlen(chk))
		return false;
	for(unsigned int c=0; c<strlen(trg); c++)
		if(trg[c]!=chk[c])
			return false;
	return true;
}

int str_cut(char *str, int begin, int len) {
    int l = strlen(str);
    if (len < 0) len = l - begin;
    if (begin + len > l) len = l - begin;
    memmove(str + begin, str + begin + len, l - len + 1);
    return len;
}

bool str_match(char* str, char* sub) {
	log_trace("String match between \"%s\" and \"%s\"", str, sub);
	int i, j=0, k;
	for(i=0; str[i]; i++) {
		if(str[i] == sub[j]) {
			for(k=i, j=0; str[k] && sub[j]; j++, k++)
				if(str[k]!=sub[j]) {
					log_trace("string constr fail in");
					log_trace("str1 %s", str);
					log_trace("     %*c^", k, ' ');
					log_trace("str2 %s", sub);
					log_trace("     %*c^", j, ' ');
					log_trace("on %d %d bc %d != %d", k, j, str[k], sub[j]);
					break;
				}
			if(!sub[j]) {
				return true;
			}
		}
	}
	if(strstr(str, sub)) {
		log_info("strstr worked!");
		return true;
	}
	return false;
}


int SearchFilename(char** arr, unsigned short sizearr, char* name, short urgent = 0) {
	log_trace("Serching filename \"%s\" in array of filenames...", name);
	int foundindex=-1;
	for(int index=0; index<sizearr; index++)
		if(strstr(arr[index], name))
			foundindex=index;
	if(foundindex==-1) {
		switch(urgent) {
			case 2:
				log_fatal("Failed to find \"%s\".", name);
				exit(0);
			break;
			case 1:
				log_warn("Failed to find \"%s\".", name);
				return -1;
			break;
			default:
				log_trace("Failed to find \"%s\".", name);
				return -1;
			break;
		}
	} else {
		log_trace("Serching filename \"%s\" in array of filenames... %d (%s)", name, foundindex, arr[foundindex]);
		return foundindex;
	}
}

struct WZmap ReadMap(char* filename) {
	struct WZmap map;
	map.path=filename;
	map.zip = zip_open(map.path, 0, 'r');
	if(map.zip == NULL) {
		log_fatal("Error opening/reading warzone map file! Exiting.");
		exit(0);
	}
	char* fullpath=filename;
	int fullpathlen=strlen(fullpath);
	int lastindex=-1;
	for(int c=0; c<fullpathlen; c++) {
		if(fullpath[c] == '/')
			lastindex=c;
	}
	if(lastindex!=-1)
		str_cut(fullpath, 0, lastindex+1);
	if(strlen(fullpath)>67)
		str_cut(fullpath, strlen(fullpath)-68, strlen(fullpath)-3);
	else
		str_cut(fullpath, strlen(fullpath)-3, -1);
	map.mapname = fullpath;
	log_info("Dealing with stupid magic of c strings... DONE!", map.mapname);
	printf("Map name: \"%s\"\n", map.mapname);
	map.totalentries = zip_total_entries(map.zip);
	map.filenames = (char**) calloc(map.totalentries, sizeof(char*));
	for(int i=0; i<map.totalentries; i++)
		map.filenames[i] = (char*) calloc(1024, sizeof(char));
	if(!map.filenames) {
		log_fatal("Filenames allocation fail! Exiting.");
		exit(0);
	}
	for(int i=0; i<map.totalentries; i++) {
		if(!map.filenames[i]) {
			log_fatal("Filename %d allocation fail! Exiting.");
			exit(0);
		}
	}
	log_trace("Listing all shit up (%d) ...", map.totalentries);
	for(int index=0; index<map.totalentries; index++) {
		log_trace("Trying index %d", index);
		int ret = zip_entry_openbyindex(map.zip, index);
		log_trace("Ret %d", ret);
		if(ret>=0) {
			snprintf(map.filenames[index], 1024, "%s", (char*)zip_entry_name(map.zip));
			log_trace("%d.\t%s", index, map.filenames[index]);
			zip_entry_close(map.zip);
		}
		else
			log_error("%d.\t Error!\n", index);
	}
	int indexttypes = SearchFilename(map.filenames, map.totalentries, (char*)"ttypes.ttp", 2);
	int openstatus = zip_entry_openbyindex(map.zip, indexttypes);
	if(openstatus<0) {
		log_fatal("Opening file by index FAIL!");
	} else {
		log_info("Reading ttypes.ttp ... ");
		size_t ttpfilesize = zip_entry_size(map.zip);
		size_t readlen;
		ssize_t readed = zip_entry_read(map.zip, &map.ttpcontents, &readlen);
		log_info("Reading ttypes.ttp ... (%ld/%ld)", readed, ttpfilesize);
		FILE* ttpf = fmemopen(map.ttpcontents, readlen, "r");
		if(ttpf==NULL) {
			log_fatal("Opening as file descriptor... ERROR!\n");
			exit(0);
		} else {
			if(fread(&map.ttphead, sizeof(char), 4, ttpf) != sizeof(char)*4) {
				log_fatal("Making shure that found file is a ttp file... ERROR! (reading headers)");
				exit(0);
			} else {
				if(map.ttphead[0] != 't' ||
				   map.ttphead[1] != 't' ||
				   map.ttphead[2] != 'y' ||
				   map.ttphead[3] != 'p')
					log_warn("Warning! File header not ttyp!!!");
				ssize_t ttypverreadret = fread(&map.ttypver, sizeof(unsigned int), 1, ttpf);
				if(ttypverreadret != 1) {
					log_trace("Fread return count (%d) not (%d)!", ttypverreadret, 1);
					if(ferror(ttpf)) {
						if(ftell(ttpf) == 8) {
							log_error("Return status of reading version bad!");
						} else {
							log_fatal("Fread read only %ld instead of %lu. Cursor not in needed position! (%ld) Readed: %u", ttypverreadret, sizeof(unsigned int), ftell(ttpf), map.ttypver);
							exit(0);
						}
					} else {
						if(ftell(ttpf) != 8)
							log_error("Cursor not in needed position!");
					}
				}
				ssize_t ttypnumreadret = fread(&map.ttypnum, sizeof(unsigned int), 1, ttpf);
				if(ttypnumreadret != 1) {
					log_trace("Fread return count (%d) not (%d)!", ttypnumreadret, 1);
					if(ferror(ttpf)) {
						if(ftell(ttpf) == 12) {
							log_warn("Return status of reading num bad!");
						} else {
							log_fatal("Fread read only %ld instead of %lu. Cursor not in needed position! (%ld) Readed: %u", ttypnumreadret, sizeof(unsigned int), ftell(ttpf), map.ttypnum);
							exit(0);
						}
					} else {
						if(ftell(ttpf) != 12)
							log_error("Cursor not in needed position!");
					}
				}
				ssize_t ttypttreadret = fread(&map.ttyptt, sizeof(unsigned short), map.ttypnum, ttpf);
				printf("\nResults of readyng ttypes.ttp:\n");
				printf("Header: \"%s\"\n", map.ttphead);
				printf("Version: %d\n", map.ttypver);
				printf("Number:  %d\n", map.ttypnum);	
			}
		}
		fclose(ttpf);
		free(map.ttpcontents);
		map.ttpcontents = NULL;
		zip_entry_close(map.zip);
	}
	int indexgamemap = SearchFilename(map.filenames, map.totalentries, (char*)"game.map", 1);
	if(indexgamemap==-1) {
		log_error("Searching for game.map file... FAIL!");
		log_info("Trying another method of finding files...");
		char mapnewpath[MAX_PATH_LEN];
		for(int i=0; i<MAX_PATH_LEN; i++)
			mapnewpath[i]='0';
		log_trace("Constructing path...");
		snprintf(mapnewpath, MAX_PATH_LEN, "multiplay/maps/%s/game.map", map.mapname);
		log_trace("New open path: \"%s\"", mapnewpath);
		openstatus = zip_entry_open(map.zip, mapnewpath);
		if(openstatus<0)
			log_error("New path not valid!");
	} else {
		openstatus = zip_entry_openbyindex(map.zip, indexgamemap);
		if(openstatus<0)
			log_error("File not opened!");
	}
	
	if(openstatus<0) {
		log_fatal("Opening file... FAIL! (game.map)");
		exit(0);
	} else {
		size_t mapfilesize = zip_entry_size(map.zip);
		void *mapcontents;
		size_t readlen;
		ssize_t readed = zip_entry_read(map.zip, &mapcontents, &readlen);
		log_info("Reading game.map ... (%ld/%ld)", readed, mapfilesize);
		FILE* mapf = fmemopen(mapcontents, readlen, "r");
		if(mapf==NULL) {
			log_fatal("Opening as file descriptor... ERROR!\n");
			exit(0);
		} else {
			if(fread(&map.maphead, sizeof(char), 4, mapf) != sizeof(char)*4) {
				log_fatal("ERROR!\n");
				exit(0);
			} else {
				if(map.maphead[0] != 'm' ||
				   map.maphead[1] != 'a' ||
				   map.maphead[2] != 'p')
					log_warn("Warning! File header not map!!!");
				ssize_t mapverreadret = fread(&map.mapver, sizeof(unsigned int), 1, mapf);
				if(mapverreadret != 1) {
					log_trace("Fread return count (%d) not (%d)!", mapverreadret, 1);
					if(ferror(mapf)) {
						if(ftell(mapf) == 8) {
							log_warn("Return status of reading version bad!");
						} else {
							log_fatal("Fread read only %ld instead of %lu. Cursor not in needed position! (%ld) Readed: %u", mapverreadret, sizeof(unsigned int), ftell(mapf), map.mapver);
							exit(0);
						}
					} else {
						if(ftell(mapf) != 8)
							log_error("Cursor not in needed position!");
					}
				}
				ssize_t mapwreadret = fread(&map.maptotalx, sizeof(unsigned int), 1, mapf);
				if(mapwreadret != 1) {
					log_trace("Fread return count (%d) not (%d)!", mapverreadret, 1);
					if(ferror(mapf)) {
						if(ftell(mapf) == 12) {
							log_warn("Warning return status of reading width bad!");
						} else {
							log_fatal("ERROR!\nFread read only %ld instead of %lu. Cursor not in needed position! (%ld) Readed: %u", mapwreadret, sizeof(unsigned int), ftell(mapf), map.maptotalx);
							exit(0);
						}
					} else {
						if(ftell(mapf) != 12)
							log_error("Cursor not in needed position!");
					}
				}
				ssize_t maphreadret = fread(&map.maptotaly, sizeof(unsigned int), 1, mapf);
				if(maphreadret != 1) {
					log_trace("Fread return count (%d) not (%d)!", mapverreadret, 1);
					if(ferror(mapf)) {
						if(ftell(mapf) == 16) {
							log_warn("Return status of reading height bad!");
						}
						else {
							log_fatal("Fread read only %ld instead of %lu. Cursor not in needed position! (%ld) Readed: %u", maphreadret, sizeof(unsigned int), ftell(mapf), map.maptotaly);
							exit(0);
						}
					} else {
						if(ftell(mapf) != 16)
							log_error("Cursor not in needed position!");
					}
				}
				printf("\nResults of reading game.map\n");
				printf("Version: %d\n", map.mapver);
				printf("Width:   %d\n", map.maptotaly);
				printf("Height:  %d\n", map.maptotalx);
				
				int maparraysize = map.maptotaly*map.maptotalx;
				map.mapheight = (unsigned short*) calloc(maparraysize, sizeof(unsigned short));
				if(map.mapheight==NULL)
					log_error("Height array allocation failed!");
				log_trace("Allocating memory for height... DONE!");
				unsigned short maptileinfo = 0;
				ssize_t mapreadret = -1;
				short maptiletexture = -1;
				TerrainTypes maptileterrain = ttsand;
				int nowmapy=-1, nowmapx=-1;
				//maparrsize = maparraysize;
				for(int counter=0; counter<maparraysize; counter++)
				{
					nowmapy = counter/map.maptotaly;
					nowmapx = counter-(nowmapy*map.maptotalx);
					mapreadret = fread(&maptileinfo, 2, 1, mapf);
					if(mapreadret != 1)
						log_warn("Fread scanned %d elements instead of %d (tileinfo)", mapreadret, 1);
					maptiletexture = (maptileinfo & maptileoffset);
					maptileterrain = (TerrainTypes)map.ttyptt[maptiletexture];
					if(maptileterrain == ttwater) {
						map.mapwater[counter]=true;
					}
					if(maptileterrain == ttclifface) {
						map.mapcliff[counter]=true;
					}
					mapreadret = fread(&map.mapheight[counter], 1, 1, mapf);
					if(mapreadret != 1)
						log_warn("Fread scanned %d elements instead of %d (height)", mapreadret, 1);
				}
			}
		}
		free(map.mapcontents);
		map.mapcontents = NULL;
		zip_entry_close(map.zip);
	}
	
	log_trace("Checking that struct.json does not exist...");                                  //FIXME parse json
	int indexjsonstructs = SearchFilename(map.filenames, map.totalentries, (char*)"struct.json", 0);  
	if(indexjsonstructs != -1) {
		log_warn("struct.json exists!!! (%d)", indexjsonstructs);
		log_warn("Sorry, but this tool still too dumb to read structs...");
	}
	int indexstructs = SearchFilename(map.filenames, map.totalentries, (char*)"struct.ini", 2);       //FIXME parse INI
	if(indexstructs != -1) {                                                                  //FIXME parse old maps
		log_warn("struct.ini exists!!! (%d)", indexstructs);
		log_warn("Sorry, but this tool still too dumb to read structs...");
	}
	
	log_info("Map reading done!");
	return map;
}

char* WriteImage(struct WZmap map, bool CustomPath, char* CustomOutputPath, int picturezoom) {
	log_info("Drawing preview...");
	char *pngfilename = (char*)malloc(sizeof(char)*MAX_PATH_LEN);
	if(CustomPath) {
		snprintf(pngfilename, MAX_PATH_LEN, "%s", CustomOutputPath);
	} else {
		snprintf(pngfilename, MAX_PATH_LEN, "output/%s.png", map.mapname);
	}
	PngImage OutputImg((unsigned int)map.maptotalx*picturezoom+picturezoom, (unsigned int)map.maptotaly*picturezoom+picturezoom);
	for(unsigned short counterx=0; counterx<map.maptotalx; counterx++) {
		for(unsigned short countery=0; countery<map.maptotaly; countery++) {
			for(unsigned short zoomcounterx=counterx*picturezoom; zoomcounterx<counterx*picturezoom+picturezoom; zoomcounterx++) {
				for(unsigned short zoomcountery=countery*picturezoom; zoomcountery<countery*picturezoom+picturezoom; zoomcountery++) {
					int nowposinarray = countery*map.maptotalx+counterx;
					if(map.mapwater[nowposinarray]) {
						OutputImg.PutPixel(zoomcounterx, zoomcountery, map.mapheight[nowposinarray]/4, map.mapheight[nowposinarray]/4, map.mapheight[nowposinarray]);
					}
					else if(map.mapcliff[nowposinarray]) {
						OutputImg.PutPixel(zoomcounterx, zoomcountery, map.mapheight[nowposinarray], map.mapheight[nowposinarray]/4, map.mapheight[nowposinarray]/4);   //FIXME not the best way
					} else {
						OutputImg.PutPixel(zoomcounterx, zoomcountery, map.mapheight[nowposinarray], map.mapheight[nowposinarray], map.mapheight[nowposinarray]);
					}
				}
			}
		}
	}
	OutputImg.WriteImage(pngfilename);
	printf("\nHeightmap written to %s\n", pngfilename);
	return pngfilename;
}

