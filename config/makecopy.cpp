/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Modified by David.Gardiner@unisa.edu.au
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

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <sys/stat.h>
#include <sys/utime.h>
#include <io.h>
#include <fcntl.h>

/*
	Unicode calls are linked at run-time, so that the application can run under
	Windows NT and 95 (which doesn't support the Unicode calls)

	The following APIs are linked:
		BackupWrite
		CreateFileW
		GetFullPathNameW
*/

//static const char *prog;

BOOL insertHashLine = FALSE;
BOOL trySymlink = FALSE;
BOOL recurse = FALSE;

typedef WINBASEAPI BOOL (WINAPI* LPFNBackupWrite)(HANDLE, LPBYTE, DWORD, LPDWORD, BOOL, BOOL, LPVOID *);
typedef WINBASEAPI HANDLE (WINAPI* LPFNCreateFileW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef WINBASEAPI DWORD (WINAPI* LPFNGetFullPathNameW)(LPCWSTR, DWORD, LPWSTR, LPWSTR *);

// Function pointers (used for NTFS hard links)
LPFNBackupWrite lpfnDllBackupWrite = NULL;    
LPFNCreateFileW lpfnDllCreateFileW = NULL;
LPFNGetFullPathNameW lpfnDllGetFullPathNameW = NULL;

// Handle to DLL
HINSTANCE hDLL = NULL;               

/*
** Flip any "unix style slashes" into "dos style backslashes" 
*/
inline void FlipSlashes(char *name)
{
    for( int i=0; name[i]; i++ ) {
        if( name[i] == '/' ) name[i] = '\\';
    }
}

/*
 * Flip any "dos style backslashes" into "unix style slashes"
 */
inline void UnflipSlashes(char *name)
{
    for( int i=0; name[i]; i++ ) {
        if( name[i] == '\\' ) name[i] = '/';
    }
}

int MakeDir( char *path )
{
    char *cp, *pstr;
    struct stat sb;

    pstr = path;
    while( cp = strchr(pstr, '\\') ) {
        *cp = '\0';
        
        if( !(stat(path, &sb) == 0 && (sb.st_mode & _S_IFDIR) )) {
            /* create the new sub-directory */
            printf("+++ makecopy: creating directory %s\n", path);
            if( mkdir(path) < 0 ) {
                return -1;
            }
        } /* else sub-directory already exists.... */

        *cp = '\\';
        pstr = cp+1;
    }

	return 0;
}

/*
 * Display error code and message for last error
 */
int ReportError()
{        
	LPVOID lpMsgBuf = NULL;

	DWORD err = GetLastError();
		
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, 
		NULL,
		err,
		0,
		(LPTSTR) &lpMsgBuf,
		0,
		NULL);

	fprintf(stderr, "%u, %s\n", err, (LPCTSTR) lpMsgBuf ) ;

	LocalFree( lpMsgBuf );

	return -1;
}

int ReportError(const char* msg)
{
	fprintf(stderr, "%Error: s\n", msg);
	return ReportError();
}


/*
	Creates an NTFS hard link of src at dest.
	NT5 will have a CreateHardLink API which will do the same thing, but a lot simpler
	This is based on the MSDN code sample Q153181

 */
BOOL hardSymLink(LPCSTR src, LPCSTR dest)
{ 
    WCHAR FileLink[ MAX_PATH + 1 ];
    WCHAR FileSource[ MAX_PATH + 1 ];
    WCHAR FileDest[ MAX_PATH + 1 ];
    LPWSTR FilePart;

    WIN32_STREAM_ID StreamId;
    DWORD dwBytesWritten;
    DWORD cbPathLen;

    BOOL bSuccess;

	// Convert src and dest to Unicode
    if (!MultiByteToWideChar(CP_ACP, 0, src, -1, FileSource, MAX_PATH)) {
        ReportError("Convert to WCHAR (source)");
        return FALSE;
    }

    if (!MultiByteToWideChar(CP_ACP, 0, dest, -1, FileDest, MAX_PATH)) {
        ReportError("Convert to WCHAR (destination)");
        return FALSE;
    }

    //
    // open existing file that we link to
    //

    HANDLE hFileSource = lpfnDllCreateFileW(
        FileSource,
        FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, // sa
        OPEN_EXISTING,
        0,
        NULL
        );

    if(hFileSource == INVALID_HANDLE_VALUE) {
        ReportError("CreateFile (source)");
        return FALSE;
    }

    //
    // validate and sanitize supplied link path and use the result
    // the full path MUST be Unicode for BackupWrite
    //

    cbPathLen = lpfnDllGetFullPathNameW( FileDest, MAX_PATH, FileLink, &FilePart); 

    if(cbPathLen == 0) {
        ReportError("GetFullPathName");
        return FALSE;
    }

    cbPathLen = (cbPathLen + 1) * sizeof(WCHAR); // adjust for byte count 

    //
    // it might also be a good idea to verify the existence of the link,
    // (and possibly bail), as the file specified in FileLink will be
    // overwritten if it already exists
    //

    //
    // prepare and write the WIN32_STREAM_ID out
    //

    LPVOID lpContext = NULL;

    StreamId.dwStreamId = BACKUP_LINK;
    StreamId.dwStreamAttributes = 0;
    StreamId.dwStreamNameSize = 0;
    StreamId.Size.HighPart = 0;
    StreamId.Size.LowPart = cbPathLen;

    //
    // compute length of variable size WIN32_STREAM_ID
    //

    DWORD StreamHeaderSize = (LPBYTE)&StreamId.cStreamName - (LPBYTE)&
        StreamId+ StreamId.dwStreamNameSize ;

    bSuccess = lpfnDllBackupWrite(
        hFileSource,
        (LPBYTE)&StreamId,  // buffer to write
        StreamHeaderSize,   // number of bytes to write
        &dwBytesWritten,
        FALSE,              // don't abort yet
        FALSE,              // don't process security
        &lpContext
        );

    if(bSuccess) {

        //
        // write out the buffer containing the path
        //

        bSuccess = lpfnDllBackupWrite(
            hFileSource,
            (LPBYTE)FileLink,   // buffer to write
            cbPathLen,          // number of bytes to write
            &dwBytesWritten,
            FALSE,              // don't abort yet
            FALSE,              // don't process security
            &lpContext
            );

        //
        // free context
        //

        lpfnDllBackupWrite(
            hFileSource,
            NULL,               // buffer to write
            0,                  // number of bytes to write
            &dwBytesWritten,
            TRUE,               // abort
            FALSE,              // don't process security
            &lpContext
            );
    }

    CloseHandle( hFileSource );

    if(!bSuccess) {
        ReportError("BackupWrite");
        return FALSE;
    }

    return TRUE;
} 

int CopyIfNecessary(char *oldFile, char *newFile)
{
    struct stat newsb;
	struct stat oldsb;	

	// Use stat to find file details
	if (stat(oldFile, &oldsb)) {
		return -1;
	}

    // skip directories unless recursion flag is set
    if ( oldsb.st_mode & _S_IFDIR ) {
        if (!recurse) {
    	    printf("    Skipping directory %s\n", oldFile);
            return 0;
        }
        else {
            char *lastDir;
            char *oldFileName; // points to where file name starts in oldFile
            char *newFileName; // points to where file name starts in newFile
            WIN32_FIND_DATA findFileData;

            // weed out special "." and ".." directories
            lastDir = strrchr(oldFile, '\\');
            if ( lastDir )
                ++lastDir;
            else
                lastDir = oldFile;
            if ( strcmp( lastDir, "." ) == 0 || strcmp( lastDir, ".." ) == 0 )
                return 0;

            // find and process the contents of the directory
            oldFileName = oldFile + strlen(oldFile);
            strcpy(oldFileName, "\\*");
            ++oldFileName;

            newFileName = newFile + strlen(newFile);
            strcpy(newFileName, "\\");
            ++newFileName;

            if( MakeDir(newFile) < 0 ) {
                fprintf(stderr, "\n+++ makecopy: unable to create directory %s\n", newFile);
                return 1;
            }

            HANDLE hFindFile = FindFirstFile(oldFile, &findFileData);
            if (hFindFile != INVALID_HANDLE_VALUE) {
                do {
                    strcpy(oldFileName, findFileData.cFileName);
                    strcpy(newFileName, findFileData.cFileName);
                    CopyIfNecessary(oldFile, newFile);
                } while (FindNextFile(hFindFile, &findFileData) != 0);
            } else {
                fprintf(stderr, "\n+++ makecopy: no such file: %s\n", oldFile);
            }
            FindClose(hFindFile);
        }
        // nothing more we can do with a directory
        return 0;
    }

	if (!stat(newFile, &newsb)) {
		// If file times are equal, don't copy
		if (newsb.st_mtime == oldsb.st_mtime) {
#if 0
			printf("+++ makecopy: %s is up to date\n", newFile);
#endif
			return 0;
		}
	}


	char   fullPathName[ MAX_PATH + 1 ];
	LPTSTR filenamePart = NULL;

	char buffer[8192];
	DWORD bytesRead = 0;
	DWORD bytesWritten = 0;

	// find out required size
	GetFullPathName(oldFile, MAX_PATH, fullPathName, &filenamePart);

	// If we need to insert #line, the copying is a bit involved.
	if (insertHashLine == TRUE) {
		struct _utimbuf utim;

	    printf("    #Installing %s into %s\n", oldFile, newFile);

		utim.actime = oldsb.st_atime;
		utim.modtime = oldsb.st_mtime;	// modification time 

		HANDLE hNewFile = CreateFile(newFile, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, 
						CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
						NULL);
		if (hNewFile == INVALID_HANDLE_VALUE) {
			return ReportError("CreateFile");
		}

		HANDLE hOldFile = CreateFile(oldFile, GENERIC_READ, FILE_SHARE_READ, NULL, 
							OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
							NULL);
		if (hOldFile == INVALID_HANDLE_VALUE) {
			return ReportError("CreateFile");
		}

		// Insert first line.
		sprintf(buffer, "#line 1 \"%s\"\r\n", fullPathName);

		// convert to unix.
		UnflipSlashes(buffer);

		WriteFile(hNewFile, buffer, strlen(buffer), &bytesWritten, NULL);

		// Copy file.
		do {
			if (!ReadFile(hOldFile, buffer, sizeof(buffer), &bytesRead, NULL)) {
				return ReportError("ReadFile");
			}

			if (!WriteFile(hNewFile, buffer, bytesRead, &bytesWritten, NULL)) {
				return ReportError("WriteFile");
			}

		} while (bytesRead > 0);

		CloseHandle(hNewFile);
		CloseHandle(hOldFile);

		// make copy have same time
		_utime(newFile, &utim);

	// If we don't need to do a #line, use an API to copy the file..
	} else {

		BOOL isNTFS = FALSE;
		
		// Find out what kind of volume this is.
		if ( trySymlink ) {
			char rootPathName[MAX_PATH];
			char *c = strchr(fullPathName, '\\');

			if (c != NULL) {
                TCHAR  fileSystemName[50];

				strncpy(rootPathName, fullPathName, (c - fullPathName) + 1);

				if (!GetVolumeInformation(rootPathName, NULL, 0, NULL, NULL, NULL, fileSystemName, sizeof(rootPathName))) {
					return ReportError("GetVolumeInformation");
				}

				isNTFS = (strcmp(fileSystemName, "NTFS") == 0);
			}
		}

		if (isNTFS) {
		    printf("    Symlinking %s into %s\n", oldFile, newFile);

			if (! hardSymLink(oldFile, newFile) ) {
				return 1;
			}
		} else {
		    printf("    Installing %s into %s\n", oldFile, newFile);

			if( ! CopyFile(oldFile, newFile, FALSE) ) {
				ReportError("CopyFile");
				return 1;
			}
		}
	}

    return 0;
}

void Usage(void)
{
    fprintf(stderr, "makecopy: [-cisx] <file1> [file2 ... fileN] <dir-path>\n");
    fprintf(stderr, "     -c  copy [default], cancels -s\n");
    fprintf(stderr, "     -i  add #line directive\n");
    fprintf(stderr, "     -r  recurse subdirectories\n");
    fprintf(stderr, "     -s  use symlinks on NT when possible\n");
    fprintf(stderr, "     -x  cancel -i\n");
}


int main( int argc, char *argv[] ) 
{
    char old_path[4096];
    char new_path[4096];
    char *oldFileName; // points to where file name starts in old_path
    char *newFileName; // points to where file name starts in new_path
    WIN32_FIND_DATA findFileData;
    int rv = 0;
	int i = 1;

    if (argc < 3) {
        Usage();
        return 2;
    }

    // parse option flags
    for ( ; *argv[i] == '-' ; ++i) {
        char *opt = argv[i]+1;
        for ( ; *opt; ++opt) {
            switch (*opt) {
            case 'c':
                trySymlink = FALSE;
                break;
            case 'i':
                insertHashLine = TRUE;
                break;
            case 'r':
                recurse = TRUE;
                break;
            case 's':
                trySymlink = TRUE;
                break;
            case 'x':
                insertHashLine = FALSE;
                break;
            default:
                Usage();
                return 2;
            }
        }
    }

    if ( trySymlink ) {
		OSVERSIONINFO osvi;

        // Symlinking supported only on WinNT, not Win9x
		// Is this Windows NT?
		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

		if (!GetVersionEx(&osvi)) {
			return ReportError();
		}

		trySymlink = (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT);

		if ( trySymlink ) {

			hDLL = LoadLibrary("Kernel32");
			if (hDLL != NULL)
			{
				lpfnDllBackupWrite = (LPFNBackupWrite)GetProcAddress(hDLL, "BackupWrite");
				lpfnDllCreateFileW = (LPFNCreateFileW)GetProcAddress(hDLL, "CreateFileW");
				lpfnDllGetFullPathNameW = (LPFNGetFullPathNameW) GetProcAddress(hDLL, "GetFullPathNameW");

				if ((!lpfnDllBackupWrite) || (!lpfnDllCreateFileW) || (!lpfnDllGetFullPathNameW))
				{
					// handle the error
					int r = ReportError("GetProcAddress");

					FreeLibrary(hDLL);       
					return r;
				}
			} else {
				return ReportError();
			}
		}
	}

	// destination path is last argument
	strcpy(new_path, argv[argc-1]);

	// append backslash to path if not already there
	if (new_path[strlen(new_path)] != '\\') {
		strcat(new_path, "\\");
	}

    //sprintf(new_path, "%s\\", argv[i+1]);
    FlipSlashes(new_path);
    newFileName = new_path + strlen(new_path);

    if( MakeDir(new_path) < 0 ) {
        fprintf(stderr, "\n+++ makecopy: unable to create directory %s\n", new_path);
        return 1;
    }

	// copy all named source files
	while (i < (argc - 1)) {
		strcpy(old_path, argv[i]);

		FlipSlashes(old_path);
		oldFileName = strrchr(old_path, '\\');
		if (oldFileName) {
			oldFileName++;
		} else {
			oldFileName = old_path;
		}

		HANDLE hFindFile = FindFirstFile(old_path, &findFileData);

		if (hFindFile != INVALID_HANDLE_VALUE) {
			do {
				strcpy(oldFileName, findFileData.cFileName);
				strcpy(newFileName, findFileData.cFileName);
				rv = CopyIfNecessary(old_path, new_path);

			} while (FindNextFile(hFindFile, &findFileData) != 0);
		} else {
			fprintf(stderr, "\n+++ makecopy: no such file: %s\n", old_path);
		}

		FindClose(hFindFile);
		i++;
	}
	if ( trySymlink ) {
		FreeLibrary(hDLL);
	}
    return 0;
}
