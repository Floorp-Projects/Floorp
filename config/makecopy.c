/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <sys/stat.h>
#include <io.h>
#include <fcntl.h>

static const char *prog;

void Usage(void)
{
    fprintf(stderr, "makecopy: <file> <dir-path>\n");
}

void FlipSlashes(char *name)
{
    int i;

    /*
    ** Flip any "unix style slashes" into "dos style backslashes" 
    */
    for( i=0; name[i]; i++ ) {
        if( name[i] == '/' ) name[i] = '\\';
    }
}

int MakeDir( char *path )
{
    char *cp, *pstr;
    struct stat sb;

    pstr = path;
    while( cp = strchr(pstr, '\\') ) {
        *cp = '\0';
        
        if( stat(path, &sb) == 0 && (sb.st_mode & _S_IFDIR) ) {
            /* sub-directory already exists.... */
        } else {
            /* create the new sub-directory */
            printf("+++ makecopy: creating directory %s\n", path);
            if( mkdir(path) < 0 ) {
                return -1;
            }
        }
        *cp = '\\';
        pstr = cp+1;
    }
}

int CopyIfNecessary(char *oldFile, char *newFile)
{
    BY_HANDLE_FILE_INFORMATION hNewInfo;
    BY_HANDLE_FILE_INFORMATION hOldInfo;

    HANDLE hFile;

    /* Try to open the destination file */
    if ( (hFile = CreateFile(newFile, GENERIC_READ, FILE_SHARE_WRITE, NULL, 
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                    NULL)) != INVALID_HANDLE_VALUE ) {
        if (GetFileInformationByHandle(hFile, &hNewInfo) == FALSE) {
            goto copy_file;
        }
        CloseHandle(hFile);

        /* Try to open the source file */
        if ( (hFile = CreateFile(oldFile, GENERIC_READ, FILE_SHARE_WRITE, NULL, 
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                        NULL)) != INVALID_HANDLE_VALUE ) {
            if (GetFileInformationByHandle(hFile, &hOldInfo) == FALSE) {
                goto copy_file;
            }
        }
        CloseHandle(hFile);

        /*
        ** If both the source and destination were created at the same time
        ** and have the same size then do not copy...
        */
        if ((hOldInfo.ftLastWriteTime.dwLowDateTime == hNewInfo.ftLastWriteTime.dwLowDateTime) &&
            (hOldInfo.ftLastWriteTime.dwHighDateTime == hNewInfo.ftLastWriteTime.dwHighDateTime) &&
            (hOldInfo.nFileSizeLow == hNewInfo.nFileSizeLow) &&
            (hOldInfo.nFileSizeHigh == hNewInfo.nFileSizeHigh)) {
            return 0;
        }
    }

copy_file:
    if( ! CopyFile(oldFile, newFile, FALSE) ) {
        return 1;
    }
    return 0;
}

int main( int argc, char *argv[] ) 
{
    char old_path[4096];
    char new_path[4096];
    char *oldFileName; /* points to where file name starts in old_path */
    char *newFileName; /* points to where file name starts in new_path */
    WIN32_FIND_DATA findFileData;
    HANDLE hFindFile;
    int rv;

    if( argc != 3 ) {
        Usage();
        return 2;
    }

    strcpy(old_path, argv[1]);
    FlipSlashes(old_path);
    oldFileName = strrchr(old_path, '\\');
    if (oldFileName) {
        oldFileName++;
    } else {
        oldFileName = old_path;
    }

    sprintf(new_path, "%s\\", argv[2]);
    FlipSlashes(new_path);
    newFileName = new_path + strlen(new_path);

    if( MakeDir(new_path) < 0 ) {
        fprintf(stderr, "\n+++ makecopy: unable to create directory %s\n", new_path);
        return 1;
    }

    hFindFile = FindFirstFile(old_path, &findFileData);
    if (hFindFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "\n+++ makecopy: no such file: %s\n", argv[1]);
        return 1;
    }

    printf("+++ makecopy: Installing %s into directory %s\n", argv[1], argv[2]);

    do {
        strcpy(oldFileName, findFileData.cFileName);
        strcpy(newFileName, findFileData.cFileName);
        rv = CopyIfNecessary(old_path, new_path);
        if (rv != 0) {
            break;
        }
    } while (FindNextFile(hFindFile, &findFileData) != 0);

    FindClose(hFindFile);
    return rv;
}
