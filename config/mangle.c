/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

HANDLE hMangleFile;

void Usage(void)
{
    fprintf(stderr, "MANGLE: <file>\n");
}

BOOL MangleFile( const char *real_name, const char *mangle_name ) 
{
    int len;
    DWORD dwWritten;
    char buffer[2048];

    if( mangle_name && *mangle_name && strcmpi(real_name, mangle_name) ) {
        printf("Mangle: renaming %s to %s\n", real_name, mangle_name);

        if( ! MoveFile(real_name, "X_MANGLE.TMP") ) {
            fprintf(stderr, "MANGLE: cannot rename %s to X_MANGLE.TMP\n", 
                    real_name);
                return FALSE;
        }

        if( ! MoveFile("X_MANGLE.TMP", mangle_name) ) {
            MoveFile("X_MANGLE.TMP", real_name);
            fprintf(stderr, "MANGLE: cannot rename X_MANGLE.TMP to %s\n", 
                    mangle_name);
            return FALSE;
        }

        len = sprintf(buffer, "mv %s %s\r\n", mangle_name, real_name);

        if( (WriteFile( hMangleFile, buffer, len, &dwWritten, NULL ) == FALSE) ||
            (dwWritten != len) ) {
            fprintf(stderr, "MANGLE: error writing to UNMANGLE.BAT\n");
            return FALSE;
        }
    }
    return TRUE;
}


int main( int argc, char *argv[] ) 
{
    WIN32_FIND_DATA find_data;
    HANDLE hFoundFile;

    if( argc != 1 ) {
        Usage();
        return 2;
    }


    hMangleFile = CreateFile("unmangle.bat",    /* name                */
                    GENERIC_READ|GENERIC_WRITE, /* access mode         */
                    0,                          /* share mode          */
                    NULL,                       /* security descriptor */
                    CREATE_NEW,                 /* how to create       */
                    FILE_ATTRIBUTE_NORMAL,      /* file attributes     */
                    NULL );                     /* template file       */

    if( hMangleFile == INVALID_HANDLE_VALUE ) {
        if( GetLastError() == ERROR_FILE_EXISTS ) {
            fprintf(stderr, "MANGLE: UNMANGLE.BAT already exists\n");
        } else {
            fprintf(stderr, "MANGLE: cannot open UNMANGLE.BAT\n");
        }
        return 1;
    }

    if( (hFoundFile = FindFirstFile("*.*", &find_data)) == INVALID_HANDLE_VALUE ) {
        fprintf(stderr, "MANGLE: cannot read directory\n");
        return 1;
    }

    do {
        if( !MangleFile(find_data.cFileName, find_data.cAlternateFileName) ) {
            fprintf(stderr, "MANGLE: cannot rename %s to %s\n",
                    find_data.cFileName, find_data.cAlternateFileName );
    
            FindClose( hFoundFile );
            CloseHandle( hMangleFile );
            return 1;
        }
    } while( FindNextFile(hFoundFile, &find_data) );
    FindClose( hFoundFile );

    {
        int len;
        DWORD dwWritten;
        char buffer[255];

        len = sprintf(buffer, "del unmangle.bat\r\n");
        WriteFile  ( hMangleFile, buffer, len, &dwWritten, NULL );
    }
    CloseHandle( hMangleFile );

    return 0;
}
