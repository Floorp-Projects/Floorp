/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

void usage( char *pMsg ){
    if( pMsg ){
        fprintf( stderr, "waitfor: %s\n\n", pMsg );
    }
    fprintf( stderr, 
            "waitfor - wait for a file to exist\n"
            "\n"
            "usage: waitfor [-a] <file> [<file>..] \n"
            "\n"
            "       Waits for file <file> to exist and returns 0, if more than one file is\n"
            "       specified, waitfor waits for the first one found.\n"
            "\n"
            "Parameters:"
            "\n"
            "   -a  Waits for all files to be created \n"
            );
    exit(-1);
}

#define FILEMAX 300

void waitfor( int iFileCount, char**ppFiles, int bAll ){
    int bDone = 0;
    
    while( !bDone ){
        struct stat statbuf;
               
        int i = 0; 
        while( i < iFileCount ){
            if( stat( ppFiles[i], &statbuf ) == 0 ){
                if( !bAll ){
                    bDone = 1;
                }
            }
            else {
                goto SLEEP;
            }
            i++;
        }
        bDone = 1;
        
SLEEP:
        if( !bDone ){
            _sleep( 100 * 1 );
        }
    }

}


int main( int argc, char** argv ){
    if( argc == 0 ){
        usage(0);
    }
    int bAll = 0;

    int i = 1;
    char* ppFiles[FILEMAX];
    int iFileCount = 0;

    while( i < argc ){
        if( argv[i][0] == '-' ){
            switch( argv[i][1] ){
            case 'a':
                bAll = 1;
            defaut:
                usage("Unknown option");
            }
        }
        else {
            ppFiles[iFileCount] = argv[i];
            iFileCount++;
        }
        if( iFileCount >= FILEMAX || iFileCount == 0 ){
            usage("Too many files");
        }
        i++;
    }
    waitfor( iFileCount, ppFiles, bAll );
    return 0;
}
