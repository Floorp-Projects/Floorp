/* 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *  
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *  
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 */

/*--------------------------------------------------------------------------
/                                                                           
/   Name: Netscape File Version Generator                                   
/   Platforms: WIN32                                                        
/   ......................................................................  
/   This program generates an ascii format of the 64-bit FILEVERSION        
/   resource identifier used by Windows executable binaries.                
/                                                                           
/   Usage Syntax:                                                           
/   fversion <major.minor.patch> [mm/dd/yyyy] [outfile]                     
/   If date is not specified, the current GMT date is used.  yyyy must be   
/   greater than 1980                                                       
/                                                                           
/   Usage Example:                                                          
/   fversion 3.0.0                                                          
/   fversion 6.5.4 1/30/2001                                                
/   fversion 6.5.4 1/30/2001 fileversion.h                                  
/                                                                           
/   see http://ntsbuild/sd/30ver.htm for specification                      
/   ......................................................................  
/   Revision History:                                                       
/   01-30-97  Initial Version, Andy Hakim (ahakim@netscape.com)             
/ --------------------------------------------------------------------------*/
#ifdef _WIN32
#include <windows.h>
#endif
#ifdef macintosh
#include <console.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

unsigned _CalcVersion(unsigned nMajor, unsigned nMinor, unsigned nPatch)
{
    unsigned nVersion;
    nVersion = nMajor;
    nVersion <<= 5;
    nVersion += nMinor;
    nVersion <<= 7;
    nVersion += nPatch;
    nVersion &= 0xFFFF;
    return(nVersion);
}


static void _GetVersions(char *szVer, unsigned *nMajor, unsigned *nMinor,
						 unsigned *nPatch)
{
    char szVersion[128];
    unsigned nReturn = 0;
    char *szToken;
    *nMajor = 0;
    *nMinor = 0;
    *nPatch = 0;

    strcpy(szVersion, szVer);
    if(szToken = strtok(szVersion, ".\n"))
    {
        *nMajor = atoi(szToken);
        if(szToken = strtok(NULL, ".\n"))
        {
            *nMinor = atoi(szToken);
            if(szToken = strtok(NULL, ".\n"))
            {
                *nPatch = atoi(szToken);
            }
        }
    }
}



unsigned _CalcBuildDate(unsigned nYear, unsigned nMonth, unsigned nDay)
{
    unsigned nBuildDate = 0;

    if(nYear < 1900) /* they really mean 1900 + nYear */
        nYear += 1900;

    nYear -= 1980;
    nBuildDate = nYear;
    /*
    nBuildDate <<= 5;
    */
    nBuildDate <<= 4;
    nBuildDate += nMonth;
    /* nBuildDate <<= 4; */
    nBuildDate <<= 5; 
    nBuildDate += nDay;
    nBuildDate &= 0xFFFF;
    return(nBuildDate);
}



unsigned _GenBuildDate(char *szBuildDate)
{
    unsigned nReturn = 0;
    char *szToken;
    unsigned nYear = 0;
    unsigned nMonth = 0;
    unsigned nDay = 0;

    if((szBuildDate) && (strchr(szBuildDate, '\\') || strchr(szBuildDate, '/')) && (szToken = strtok(szBuildDate, "\\/")))
    {
        nMonth = atoi(szToken);
	nMonth--;	/* use months in the range [0..11], as in struct tm */
        if(szToken = strtok(NULL, "\\/"))
        {
            nDay = atoi(szToken);
            if(szToken = strtok(NULL, "\\/"))
            {
                nYear = atoi(szToken);
		if(nYear < 70) { /* handle 2 digit years like (20)00 */	
		    nYear += 100;
		}
		else if (nYear < 100) {
		}
		else if (nYear > 1900){
		    nYear -= 1900;
		}
            }
        }
    }
    else
    {
		struct tm *newtime;
		time_t ltime;

		time( &ltime );

        /* Obtain coordinated universal time: */
		newtime = gmtime( &ltime );
        nYear = newtime->tm_year;
        nMonth = newtime->tm_mon;
        nDay = newtime->tm_mday;
    }

    nReturn = _CalcBuildDate(nYear, nMonth, nDay);
    return(nReturn);
}



static void ShowHelp(char *szFilename)
{
    fprintf(stdout, "%s: Generates ascii format #define for FILEVERSION\n", szFilename);
    fprintf(stdout, "   resource identifier used by Windows executable binaries.\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "Usage: %s <major.minor.patch> [mm/dd/yy] [outfile]\n", szFilename);
    fprintf(stdout, "\n");
    fprintf(stdout, "Examples:\n");
    fprintf(stdout, "%s 3.0.0\n", szFilename);
    fprintf(stdout, "%s 6.5.2 1/30/2001\n", szFilename);
    fprintf(stdout, "%s 6.5.2 1/30/2001 fileversion.h\n", szFilename);
}



main(int nArgc, char **lpArgv)
{
    int nReturn = 0;
    unsigned nVersion = 0;
    unsigned nBuildDate = 0;

#ifdef macintosh
	nArgc = ccommand( &lpArgv );
#endif

    if(nArgc < 2)
    {
        ShowHelp(lpArgv[0]);
        nReturn = 1;
    }
    else
    {
        char *szVersion = NULL;
        char *szDate = NULL;
        char *szOutput = NULL;
        FILE *f = stdout;
		unsigned nMajor = 0;
		unsigned nMinor = 0;
		unsigned nPatch = 0;

        szVersion = (char *)lpArgv[1];
        szDate = (char *)lpArgv[2];
        szOutput = (char *)lpArgv[3];
		_GetVersions( szVersion, &nMajor, &nMinor, &nPatch );
		nVersion = _CalcVersion(nMajor, nMinor, nPatch);
        nBuildDate = _GenBuildDate(szDate);

        if(nArgc >= 4) {
            if (( f = fopen(szOutput, "w")) == NULL ) {
		perror( szOutput );
		exit( 1 );
	    }
	}

        fprintf(f, "#define VI_PRODUCTVERSION %u.%u\n", nMajor, nMinor);
        fprintf(f, "#define PRODUCTTEXT \"%s\"\n", szVersion );
        fprintf(f, "#define VI_FILEVERSION %u, 0, 0,%u\n",
				nVersion, nBuildDate);
        fprintf(f, "#define VI_FileVersion \"%s Build %u\\0\"\n",
				szVersion, nBuildDate);

        if(nArgc >= 4)
            fclose(f);
        nReturn = (nVersion && !nBuildDate);
    }
    return(nReturn);
}

