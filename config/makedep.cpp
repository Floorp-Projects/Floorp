/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Dependency building hack
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <ctype.h>
#include <afxcoll.h>
#include <afxtempl.h>

int mainReturn = 0;
BOOL b16 = FALSE;
BOOL bSimple = FALSE;

//	freopen won't work on stdout in win16
FILE *pAltFile = stdout;

CStringArray includeDirectories;

// turn a file, relative path or other into an absolute path
// This function copied from MFC 1.52
BOOL PASCAL _AfxFullPath(LPSTR lpszPathOut, LPCSTR lpszFileIn)
        // lpszPathOut = buffer of _MAX_PATH
        // lpszFileIn = file, relative path or absolute path
        // (both in ANSI character set)
{
        OFSTRUCT of;
        if (OpenFile(lpszFileIn, &of, OF_PARSE) != HFILE_ERROR)
        {
                // of.szPathName is in the OEM character set
                OemToAnsi(of.szPathName, lpszPathOut);
                AnsiUpper(lpszPathOut); // paths in upper case just to be sure
                return TRUE;
        }
        else
        {
                TRACE1("Warning: could not parse the path %Fs\n", lpszFileIn);
                lstrcpy(lpszPathOut, lpszFileIn);  // take it literally
                AnsiUpper(lpszPathOut); // paths in upper case just to be sure
                return FALSE;
        }
}

void AddIncludeDirectory( char *pString ){
    CString s = pString;
	int len = s.GetLength();
    if(len > 0 && s[len - 1] != '\\' ){
        s += "\\";
    }
    includeDirectories.Add(s);
}

BOOL FileExists( const char *pString ){
    struct _stat buf;
    int result;

    result = _stat( pString, &buf );
    return (result == 0);
}

void FixPathName(CString& str) {
	str.MakeUpper();		// all upper case

	// now switch all forward slashes to back slashes
	int index;
	while ((index = str.Find('/')) != -1) {
		str.SetAt(index, '\\');
	}
}

void FATName(CString& csLONGName)
{
    //  Only relevant for 16 bits.
    if(b16) {
        //  Convert filename to FAT (8.3) equivalent.
        char aBuffer[2048];

        if(GetShortPathName(csLONGName, aBuffer, 2048)) {
            csLONGName = aBuffer;
        }
    }
}


class CFileRecord {
public:
    CString m_shortName;
    CString m_pathName;
    CPtrArray m_includes;  // pointers to CFileRecords in fileMap
    BOOL m_bVisited;
    BOOL m_bSystem;
    BOOL m_bSource;
    static CMapStringToPtr fileMap;      // contains all allocated CFileRecords
    static CStringArray orderedFileNames;
    static CMapStringToPtr includeMap;   // pointer to CFileRecords in fileMap
    static CMapStringToPtr noDependMap;

    CFileRecord( const char *shortName, const char* pFullName, BOOL bSystem, BOOL bSource):
                m_shortName( shortName ),
                m_pathName(),
                m_includes(),
                m_bVisited(FALSE),
                m_bSource( bSource ),
                m_bSystem(bSystem){

		m_pathName = pFullName;
		FixPathName(m_pathName);				// all upper case for consistency
		ASSERT(FindFileRecord(m_pathName) == NULL);	// make sure it's not already in the map
        fileMap[m_pathName] = this;			// add this to the filemap, using the appropriate name as the key
		if (bSource) {
			orderedFileNames.Add(m_pathName);	// remember the order we saw source files in
		}
    }

    // 
    // open the file and grab all the includes.
    //
    void ProcessFile(){
        FILE *f;
		CString fullName;
        BOOL bSystem;
		DWORD lineCntr = 0;
		char *a = new char[2048];
        memset(a, 0, 2048);
		char srcPath[_MAX_PATH];

		// construct the full path
		if (!_AfxFullPath(srcPath, m_pathName)) {
			strcpy(srcPath, m_pathName);
		}

		// strip off the source filename to end up with just the path
		LPSTR pTemp = strrchr(srcPath, '\\');
		if (pTemp) {
			*(pTemp + 1) = 0;
		}

        f = fopen(m_pathName, "r");
        if(f != NULL && f != (FILE *)-1)  {
			setvbuf(f, NULL, _IOFBF, 32768);		// use a large file buffer
            while(fgets(a, 2047, f)) {
				// if the string "//{{NO_DEPENDENCIES}}" is at the start of one of the 
				// first 10 lines of a file, don't build dependencies on it or any
				// of the files it includes
				if (lineCntr < 10) {
					static char* pDependStr = "//{{NO_DEPENDENCIES}}";
					if (strncmp(a, pDependStr, strlen(pDependStr)) == 0) {
						noDependMap[m_pathName] = 0;	// add it to the noDependMap
						break;							// no need for further processing of this file
					}
				}
				++lineCntr;
				// have to handle a variety of legal syntaxes that we find in our source files:
				//    #include
				// #   include
                // #include
				// if the first non-whitespace char is a '#', consider this line
				pTemp = a;
				pTemp += strspn(pTemp, " \t");			// skip whitespace
				if (*pTemp == '#') {
					++pTemp;							// skip the '#'
					pTemp += strspn(pTemp, " \t");		// skip more whitespace
					if( !strncmp(pTemp, "include", 7) ){
						pTemp += 7;						// skip the "include"
						pTemp += strspn(pTemp, " \t");	// skip more whitespace
						bSystem = (*pTemp == '<');		// mark if it's a system include or not
                        // forget system files -- we just have to search all the paths
                        // every time and never find them! This change alone speeds a full
                        // depend run on my system from 5 minutes to 3:15
						// if (bSystem || (*pTemp == '"')) {
                        if (*pTemp == '"') {
							LPSTR pStart = pTemp + 1;	// mark the start of the string
							pTemp = pStart + strcspn(pStart, ">\" ");	// find the end of the string
							*pTemp = 0;					// terminate the string

							// construct the full pathname from the path part of the 
							// source file and the name listed here
							fullName = srcPath;
							fullName += pStart;
							CFileRecord *pAddMe = AddFile( pStart, fullName, bSystem );
							if (pAddMe) {
								m_includes.Add(pAddMe);
							}
						}
					}
				}
            }
            fclose(f);
        }
        delete [] a;
    }

    void PrintIncludes(){
        int i = 0;
        while( i < m_includes.GetSize() ){
            CFileRecord *pRec = (CFileRecord*) m_includes[i];

            //  Don't write out files that don't exist or are not in the namespace
            //      of the programs using it (netscape_AppletMozillaContext.h doesn't
            //      mix well with 16 bits).
			// Also don't write out files that are in the noDependMap
			void*	lookupJunk;
            if( !pRec->m_bVisited && pRec->m_pathName.GetLength() != 0 && !noDependMap.Lookup(pRec->m_pathName, lookupJunk)) {

				// not supposed to have a file in the list that doesn't exist
				ASSERT(FileExists(pRec->m_pathName));

                CString csOutput;
                csOutput = pRec->m_pathName;
                FATName(csOutput);

				fprintf(pAltFile, "\\\n    %s ", (const char *) csOutput );

				// mark this one as done so we don't do it more than once
                pRec->m_bVisited = TRUE;

                pRec->PrintIncludes();
            }
            i++;
        }
    }

    void PrintDepend(){
        CFileRecord *pRec;
        BOOL bFound;
        POSITION next;
        CString name;

		// clear all the m_bVisisted flags so we can use it to keep track
		// of whether we've already output this file as a dependency
        next = fileMap.GetStartPosition();
        while( next ){
            fileMap.GetNextAssoc( next, name, *(void**)&pRec );
            pRec->m_bVisited = FALSE;
        }

        char fname[_MAX_FNAME];

		if (pRec->m_pathName.GetLength() != 0) {
            if( bSimple ){
    			fprintf(pAltFile, "\n\n\n%s:\t", m_pathName );
            }
            else {
                CString csOutput;
                csOutput = m_pathName;
                FATName(csOutput);

    			_splitpath( csOutput, NULL, NULL, fname, NULL );

    			fprintf(pAltFile, "\n\n\n$(OUTDIR)\\%s.obj: %s ", fname, (const char*) csOutput );
            }
	        m_bVisited = TRUE;		// mark it as done so we won't do it again
	        PrintIncludes();
		}
    }


    static CString NormalizeFileName( const char* pName ){
        return CString(pName);
    }

    static CFileRecord* FindFileRecord( const char *pName ){
		CFileRecord* pRec = NULL;
		CString name(pName);
		FixPathName(name);
		fileMap.Lookup(name, (void*&)pRec);
		return(pRec);
    }
public:
    static CFileRecord* AddFile( const char* pShortName, const char* pFullName, BOOL bSystem = FALSE, 
                BOOL bSource = FALSE ){

		char fullName[_MAX_PATH];
		BOOL bFound = FALSE;
		CString foundName;
		CString fixedShortName;
        CString s;

        // normalize the name
        fixedShortName = pShortName;
        FixPathName(fixedShortName);
        pShortName = fixedShortName;

        // if it is source, we might be getting an obj file.  If we do,
        //  convert it to a c or c++ file.
        if( bSource && (strcmp(GetExt(pShortName),".obj") == 0) ){
            char path_buffer[_MAX_PATH];
            char fname[_MAX_FNAME] = "";
            CString s;

            _splitpath( pShortName, NULL, NULL, fname, NULL );
            if( FileExists( s = CString(fname) + ".cpp") ){
                pShortName = s;
                pFullName = s;
            }
            else if( FileExists( s = CString(fname) + ".c" ) ){
                pShortName = s;
                pFullName = s;
            }
            else {
                return 0;
            }
        }

		// if pFullName was not constructed, construct it here based on the current directory
		if (!pFullName) {
			_AfxFullPath(fullName, pShortName);
			pFullName = fullName;
		}
		
		// first check to see if we already have this exact file
		CFileRecord *pRec = FindFileRecord(pFullName);

        // if not found and not a source file check the header list --
        // all files we've found in include directories are in the includeMap.
        // we can save gobs of time by getting it from there
        if (!pRec && !bSource)
            includeMap.Lookup(fixedShortName, (void*&)pRec);

        if (!pRec) {
            // not in one of our lists, start scrounging on disk

            // check the fullname first
            if (FileExists(pFullName)) {
                foundName = pFullName;
                bFound = TRUE;
            }
            else {
                // if still not found, search the include paths
                int i = 0;
                while( i < includeDirectories.GetSize() ){
                    if( FileExists( includeDirectories[i] + pShortName ) ){
                        foundName = includeDirectories[i] + pShortName;
                        bFound = TRUE;
                        break;
                    }
                    i++;
                }
            }
        }
        else {
            // we found it
            bFound = TRUE;
        }

		// source files are not allowed to be missing
		if (bSource && !pRec && !bFound) {
			fprintf(stderr, "Source file: %s doesn't exist\n", pFullName);
			mainReturn = -1;		// exit with an error, don't write out the results
		}

#ifdef _DEBUG
		if (!pRec && !bFound && !bSystem) {
			fprintf(stderr, "Header not found: %s (%s)\n", pShortName, pFullName);
		}
#endif

		// if none of the above logic found it already in the list, 
        // must be a new file, add it to the list
        if (bFound && (pRec == NULL)) {
            pRec = new CFileRecord( pShortName, foundName, bSystem, bSource);

			// if this one isn't a source file add it to the includeMap
			// for performance reasons (so we can find it there next time rather
			// than having to search the file system again)
			if (!bSource) {
				includeMap[pShortName] = pRec;
			}
        }
        return pRec;
    }


    static void PrintDependancies(){
        CFileRecord *pRec;
        BOOL bFound;
        POSITION next;
        CString name;

		// use orderedFileNames to preserve order
		for (int pos = 0; pos < orderedFileNames.GetSize(); pos++) {
			pRec = FindFileRecord(orderedFileNames[pos]);
            if(pRec && pRec->m_bSource ){
                pRec->PrintDepend();
			}
		}
    }


    void PrintDepend2(){
        CFileRecord *pRec;
        int i;

        if( m_includes.GetSize() != 0 ){
			fprintf(pAltFile, "\n\n\n%s: \\\n",m_pathName );
            i = 0;
            while( i < m_includes.GetSize() ){
                pRec = (CFileRecord*) m_includes[i];
    			fprintf(pAltFile, "\t\t\t%s\t\\\n",pRec->m_pathName );
                i++;
            }
        }
    }

    static void PrintDependancies2(){
        CFileRecord *pRec;
        BOOL bFound;
        POSITION next;
        CString name;

        next = fileMap.GetStartPosition();
        while( next ){
            fileMap.GetNextAssoc( next, name, *(void**)&pRec );
            pRec->PrintDepend2();
        }
    }


    static void PrintTargets(const char *pMacroName, const char *pDelimeter){
        CFileRecord *pRec;
        BOOL bFound;
        POSITION next;
        CString name;

        BOOL bNeedDelimeter = FALSE;
		fprintf(pAltFile, "%s = ", pMacroName);        

		// use orderedFileNames to preserve target order
		for (int pos = 0; pos < orderedFileNames.GetSize(); pos++) {
			pRec = FindFileRecord(orderedFileNames[pos]);
			ASSERT(pRec);

            if( pRec && pRec->m_bSource && pRec->m_pathName.GetLength() != 0){
                char fname[_MAX_FNAME];

                CString csOutput;
                csOutput = pRec->m_pathName;
                FATName(csOutput);

                _splitpath( csOutput, NULL, NULL, fname, NULL );

                if(bNeedDelimeter)  {
                    fprintf(pAltFile, "%s\n", pDelimeter);
                    bNeedDelimeter = FALSE;
                }

				fprintf(pAltFile, "     $(OUTDIR)\\%s.obj   ", fname );
                bNeedDelimeter = TRUE;
            }
        }
		fprintf(pAltFile, "\n\n\n");        
    }

    static CString DirDefine( const char *pPath ){
        char path_buffer[_MAX_PATH];
        char dir[_MAX_DIR] = "";
        char dir2[_MAX_DIR] = "";
        char fname[_MAX_FNAME] = "";
        char ext[_MAX_EXT] = "";
        CString s;

        _splitpath( pPath, 0, dir, 0, ext );

        BOOL bDone = FALSE;

        while( dir && !bDone){
            // remove the trailing slash
            dir[ strlen(dir)-1] = 0;
            _splitpath( dir, 0, dir2, fname, 0 );
            if( strcmp( fname, "SRC" ) == 0 ){
                strcpy( dir, dir2 );
            }
            else {
                bDone = TRUE;
            }
        }
        s = CString(fname) + "_" + (ext+1);
        return s;
    }


    static void PrintSources(){
        int i;
        CString dirName, newDirName;

        for( i=0; i< orderedFileNames.GetSize(); i++ ){
            newDirName= DirDefine( orderedFileNames[i] );
            if( newDirName != dirName ){
                fprintf( pAltFile, "\n\n\nFILES_%s= $(FILES_%s) \\", 
                        (const char*)newDirName, (const char*)newDirName );
                dirName = newDirName;
            }
            fprintf( pAltFile, "\n\t%s^", (const char*)orderedFileNames[i] );
        }
    }

    static CString SourceDirName( const char *pPath, BOOL bFileName){
        char path_buffer[_MAX_PATH];
        char drive[_MAX_DRIVE] = "";
        char dir[_MAX_DIR] = "";
        char fname[_MAX_FNAME] = "";
        char ext[_MAX_EXT] = "";
        CString s;

        _splitpath( pPath, drive, dir, fname, ext );

        s = CString(drive) + dir;
        if( bFileName ){
            s += CString("FNAME") + ext;
        }
        else {
            // remove the trailing slash
            s = s.Left( s.GetLength() - 1 );
        }
        return s;
    }


    static CString GetExt( const char *pPath){
        char ext[_MAX_EXT] = "";

        _splitpath( pPath, 0,0,0, ext );

        CString s = CString(ext);
        s.MakeLower();
        return s;
    }

    static void PrintBuildRules(){
        int i;
        CString dirName;
        
        CMapStringToPtr dirList;

        for( i=0; i< orderedFileNames.GetSize(); i++ ){
            dirList[ SourceDirName(orderedFileNames[i], TRUE) ]= 0;
        }

        POSITION next;
        CString name;
        void *pVal;

        next = dirList.GetStartPosition();
        while( next ){
            dirList.GetNextAssoc( next, name, pVal);
            CString dirDefine = DirDefine( name );
            CString ext = GetExt( name );
            name = SourceDirName( name, FALSE );
            CString response = dirDefine.Left(8);

            fprintf( pAltFile, 
                "\n\n\n{%s}%s{$(OUTDIR)}.obj:\n"
                "\t@rem <<$(OUTDIR)\\%s.cl\n"
                "\t$(CFILEFLAGS)\n"
                "\t$(CFLAGS_%s)\n"
                "<<KEEP\n"
                "\t$(CPP) @$(OUTDIR)\\%s.cl %%s\n",
                (const char*)name,
                (const char*)ext,
                (const char*)response,
                (const char*)dirDefine,
                (const char*)response
            );

            fprintf( pAltFile, 
                "\n\n\nBATCH_%s:\n"
                "\t@rem <<$(OUTDIR)\\%s.cl\n"
                "\t$(CFILEFLAGS)\n"
                "\t$(CFLAGS_%s)\n"
                "\t$(FILES_%s)\n"
                "<<KEEP\n"
                "\t$(TIMESTART)\n"
                "\t$(CPP) @$(OUTDIR)\\%s.cl\n"
                "\t$(TIMESTOP)\n",
                (const char*)dirDefine,
                (const char*)response,
                (const char*)dirDefine,
                (const char*)dirDefine,
                (const char*)response
            );
        }

        //
        // Loop through one more time and build the final batch build
        //  rule
        //
        fprintf( pAltFile, 
            "\n\n\nBATCH_BUILD_OBJECTS:\t\t\\\n");

        next = dirList.GetStartPosition();
        while( next ){
            dirList.GetNextAssoc( next, name, pVal);
            CString dirDefine = DirDefine( name );

            fprintf( pAltFile, 
                "\tBATCH_%s\t\t\\\n", dirDefine );
        }

        fprintf( pAltFile, 
            "\n\n");
    }
        

    static void ProcessFiles(){
        CFileRecord *pRec;
        BOOL bFound;
        POSITION next;
        CString name;

		// search all the files for headers, adding each one to the list when found
		// rather than do it recursively, it simple marks each one it's done
		// and starts over, stopping only when all are marked as done

        next = fileMap.GetStartPosition();
        while( next ){
            fileMap.GetNextAssoc( next, name, *(void**)&pRec );
            if( pRec->m_bVisited == FALSE && pRec->m_bSystem == FALSE ){
				// mark this file as already done so we don't read it again
				// to find its headers
                pRec->m_bVisited = TRUE;
                pRec->ProcessFile();
                // Start searching from the beginning again
				// because ProcessFile may have added new files 
				// and changed the GetNextAssoc order
                next = fileMap.GetStartPosition();       

            }
        }
    }


};

CMapStringToPtr CFileRecord::fileMap;           // contains all allocated CFileRecords
CStringArray CFileRecord::orderedFileNames;
CMapStringToPtr CFileRecord::includeMap;        // pointers to CFileRecords in fileMap
CMapStringToPtr CFileRecord::noDependMap;       // no data, just an index

int main( int argc, char** argv ){
    int i = 1;
    char *pStr;
    static int iRecursion = 0;	//	Track levels of recursion.
	static CString outputFileName;
    
    //	Entering.
    iRecursion++;

    while( i < argc ){
        if( argv[i][0] == '-' || argv[i][0] == '/' ){
            switch( argv[i][1] ){

            case 'i':
            case 'I':
                if( argv[i][2] != 0 ){
                    pStr = &(argv[i][2]);
                }
                else {
                    i++;
                    pStr = argv[i];
                }
                if( pStr == 0 || *pStr == '-' || *pStr == '/' ){
                    goto usage;
                }
                else {
                    AddIncludeDirectory( pStr );
                }
                break;

            case 'f':
            case 'F':
                if( argv[i][2] != 0 ){
                    pStr = &(argv[i][2]);
                }
                else {
                    i++;
                    pStr = argv[i];
                }
                if( pStr == 0 || *pStr == '-' || *pStr == '/'){
                    goto usage;
                }
                else {
                    CStdioFile f;
                    CString s;
                    if( f.Open( pStr, CFile::modeRead ) ){
                        while(f.ReadString(s)){
                            s.TrimLeft();
                            s.TrimRight();
                            if( s.GetLength() ){
                                CFileRecord::AddFile( s, NULL, FALSE, TRUE );
                            }
                        } 
                        f.Close();
                    }
                    else {
                        fprintf(stderr,"makedep: file not found: %s", pStr );
                        exit(-1);
                    }
                }
                break;

            case 'o':
            case 'O':
                if( argv[i][2] != 0 ){
                    pStr = &(argv[i][2]);
                }
                else {
                    i++;
                    pStr = argv[i];
                }
                if( pStr == 0 || *pStr == '-' || *pStr == '/'){
                    goto usage;
                }
                else {
                    CStdioFile f;
                    CString s;
					outputFileName = pStr;
					if(!(pAltFile = fopen(pStr, "w+")))	{
                        fprintf(stderr, "makedep: file not found: %s", pStr );
                        exit(-1);
                    }
                }
                break;

            case '1':
                if( argv[i][2] == '6')  {
                    b16 = TRUE;
                }
                break;

            case 's':
            case 'S':
                bSimple = TRUE;
                break;



            case 'h':
            case 'H':
            case '?':
            usage:
                fprintf(stderr, "usage: makedep -I <dirname> -F <filelist> <filename>\n"
                       "  -I <dirname>    Directory name, can be repeated\n"
                       "  -F <filelist>   List of files to scan, one per line\n"
                       "  -O <outputFile> File to write output, default stdout\n");
                exit(-1);
            }
        }
        else if( argv[i][0] == '@' ){
        	//	file contains our commands.
	        CStdioFile f;
    	    CString s;
    	    int iNewArgc = 0;
    	    char **apNewArgv = new char*[5000];
			memset(apNewArgv, 0, sizeof(apNewArgv));

			//	First one is always the name of the exe.
			apNewArgv[0] = argv[0];
			iNewArgc++;

			const char *pTraverse;
			const char *pBeginArg;
	        if( f.Open( &argv[i][1], CFile::modeRead ) ){
    	        while( iNewArgc < 5000 && f.ReadString(s) )	{
					//	Scan the string for args, and do the right thing.
					pTraverse = (const char *)s;
					while(iNewArgc < 5000 && *pTraverse)	{
						if(isspace(*pTraverse))	{
								pTraverse++;
								continue;
						}

						//	Extract to next space.
						pBeginArg = pTraverse;
						do	{
							pTraverse++;
						}
						while(*pTraverse && !isspace(*pTraverse));
						apNewArgv[iNewArgc] = new char[pTraverse - pBeginArg + 1];
						memset(apNewArgv[iNewArgc], 0, pTraverse - pBeginArg + 1);
						strncpy(apNewArgv[iNewArgc], pBeginArg, pTraverse - pBeginArg);
						iNewArgc++;
					}
	            } 
    	        f.Close();
        	}
        	
        	//	Recurse if needed.
        	if(iNewArgc > 1)	{
        		main(iNewArgc, apNewArgv);
        	}
        	
        	//	Free off the argvs (but not the very first one which we didn't allocate).
        	while(iNewArgc > 1)	{
        		iNewArgc--;
        		delete [] apNewArgv[iNewArgc];
        	}
        	delete [] apNewArgv;
        }
        else {
            CFileRecord::AddFile( argv[i], NULL, FALSE, TRUE );
        }
        i++;
    }
    
    //	Only of the very bottom level of recursion do we do this.
    if(iRecursion == 1)	{

		// only write the results out if no errors encountered
		if (mainReturn == 0) {
			CFileRecord::ProcessFiles();
            if( !bSimple ){
        		CFileRecord::PrintTargets("OBJ_FILES", "\\");
                if(b16) {
    			    CFileRecord::PrintTargets("LINK_OBJS", "+\\");
                }
                else    {
    			    CFileRecord::PrintTargets("LINK_OBJS", "^");
                }
                CFileRecord::PrintSources();
                CFileRecord::PrintBuildRules();
            }
    		CFileRecord::PrintDependancies();
		}
	    
		if(pAltFile != stdout)	{
			fclose(pAltFile);
			if (mainReturn != 0) {
				remove(outputFileName);	// kill output file if returning an error
			}
		}
	}
	iRecursion--;

    if (iRecursion == 0 )
    {
        // last time through -- clean up allocated CFileRecords!
        CFileRecord *pFRec;
        CString     name;
        POSITION    next;

        next = CFileRecord::fileMap.GetStartPosition();
        while( next ){
            CFileRecord::fileMap.GetNextAssoc( next, name, *(void**)&pFRec );
            delete pFRec;
        }
    }

    return mainReturn;
}
