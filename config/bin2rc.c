/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include <stdio.h>
#include <sys\stat.h>

int main(int iArgc, char **ppArgv) {
    int iRetval = 1;
    
    /*  First argument, the filename to convert.
     *  Output to stdout, redirect to save.
     */
    char *pFileName = ppArgv[1];
    if(pFileName) {
        FILE *pFile = fopen(pFileName, "rb");
        if(pFile) {
            struct stat sInfo;
            
            /*  Stat the file for size.
             */
            if(!fstat(fileno(pFile), &sInfo)) {
                int iChar;
                int iX = 0;
                int iFirsttime = 1;
                
                /*  Begin RCDATA
                 */
                printf("BEGIN\n");

                /*  First string identifies created via bin2rc.
                 *  Users of the RCDATA must check for this to
                 *      assume the format of the remainder of
                 *      the data.
                 */
                printf("\t\"bin2rc generated resource\\0\",\t// bin2rc identity string\n");

                /*  Next string is optional parameter on command
                 *      line.  If not present, an empty string.
                 *  Users of the RCDATA must understand this is
                 *      the optional string that can be used for
                 *      about any purpose they desire.
                 */
                printf("\t\"%s\\0\",\t// optional command line string\n", ppArgv[2] ? ppArgv[2] : "");
                
                /*  Next string is the size of the original file.
                 *  Users of the RCDATA must understand that this
                 *      is the size of the file's actual contents.
                 */
                printf("\t\"%ld\\0\"\t// data size header\n", sInfo.st_size);
                
                while(EOF != (iChar = fgetc(pFile))) {
                    /*  Comma?
                     */
                    if(0 == iFirsttime) {
                        iX += printf(",");
                    }
                    else {
                        iFirsttime = 0;
                    }
                    
                    /*  Newline?
                     */
                    if(iX >= 72) {
                        printf("\n");
                        iX = 0;
                    }
                    
                    /*  Tab?
                     */
                    if(0 == iX) {
                        printf("\t");
                        iX += 8;
                    }
                    
                    /*  Octal byte.
                     */
                    iX += printf("\"\\%.3o\"", iChar);
                    
                    
                }
                
                /*  End RCDATA
                 */
                if(0 != iX) {
                    printf("\n");
                }
                printf("END\n");
                
                /*  All is well.
                 */
                iRetval = 0;
            }
            fclose(pFile);
            pFile = NULL;
        }
    }
    
    return(iRetval);
}

