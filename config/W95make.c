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
#include <stdlib.h>
#include <string.h>
#include <process.h>

/*
 * A feeble attempt at recursive make on win95 - spider 1/98
 *
 * argv[1] == target
 * argv[2] == end directory (full)
 * argv[3...n] == list of source directories
 *
 */

void main(int argc, char **argv)
{
	char *args[7];
	int n = 0 ;
	int rc = 0 ;

   /* Set up parameters to be sent: Sorry for the hardcode!*/
   args[0] = "-nologo";
   args[1] = "-nologo";
   args[2] = "-S";
   args[3] = "-f";
   args[4] = "makefile.win";
   args[5] = argv[1] ;
   args[6] = NULL ;

   if (argc < 3) {
	   fprintf(stderr, "w95make: Not enough arguments, you figure it out\n");
 	   exit (666) ;
	}


   while(argv[n+3] != NULL) {

		if (_chdir(argv[n+3]) != 0) {
			fprintf(stderr, "w95make: Could not change to directory %s ... skipping\n", argv[n+3]);
		} else {
		
			fprintf(stdout, "w95make: Entering Directory %s\\%s with target %s\n", argv[2], argv[n+3], argv[1]);
			if ((rc = _spawnvp(_P_WAIT,"nmake", args)) != 0) {
				fprintf(stderr, "w95make: nmake failed in directory %s with error code %d\n", argv[n+3], rc);
				exit(rc);
			}

			if (_chdir(argv[2]) != 0) {
				fprintf(stderr, "w95make: Could not change back to directory %s\n", argv[2]);
				exit (666) ;
			}
			
			fprintf(stdout, "w95make: Leaving Directory %s\\%s with target %s\n", argv[2], argv[n+3], argv[1]);

		}
		
		n++;
	}

	
    exit(0);
}
