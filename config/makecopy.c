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

void GetPathName(char *file, char *new_path)
{
    int i;

    i = strlen(file);
    for( i=strlen(file); i && file[i] != '\\'; i--);
    strncpy(new_path, file, i);
    if( new_path[i] != '\\' ) {
        new_path[i++] = '\\';
    }
    new_path[i] = '\0';
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

int CopyIfNecessary(char *oldFile, char *newFile, char *path)
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
    printf("+++ makecopy: Installing %s into directory %s\n", oldFile, path);
    if( ! CopyFile(oldFile, newFile, FALSE) ) {
        return 1;
    }
    return 0;
}


static char new_file[4096];
static char new_path[4096];

int main( int argc, char *argv[] ) 
{
    char fname[_MAX_FNAME];
    char ext[_MAX_EXT];

    if( argc != 3 ) {
        Usage();
        return 2;
    }

    _splitpath(argv[1], NULL, NULL, fname, ext);

    sprintf(new_file, "%s\\%s%s", argv[2], fname, ext);
    FlipSlashes(new_file);

    sprintf(new_path, "%s\\", argv[2]);
    FlipSlashes(new_path);


    if( MakeDir(new_path) < 0 ) {
        fprintf(stderr, "\n+++ makecopy: unable to create directory %s\n", new_path);
        return 1;
    }

    return CopyIfNecessary(argv[1], new_file, new_path);
}
