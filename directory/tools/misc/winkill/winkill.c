/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 * 
 * The contents of this file are subject to the Mozilla Public License Version 
 * 1.1 (the "License"); you may not use this file except in compliance with 
 * the License. You may obtain a copy of the License at 
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1996-2003
 * the Initial Developer. All Rights Reserved.
 * 
 * Contributor(s):
 *	Mark Smith <MarkCSmithWork@aol.com>
 * 
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 * 
 * ***** END LICENSE BLOCK ***** */

/*
 * winkill.cpp : kill one Win32 process
 */

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <Windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


static int
usage( const char *progname )
{
	fprintf( stderr, "usage: %s [-signal] pid\n", progname );
	return 2;
}


int main(int argc, char* argv[])
{
	char			*progname, *prefix_msg, *pidstr;
	int				rc = 0;
	HANDLE			hProcess;
	unsigned long	pid;
	BOOL			checkStatusOnly = FALSE;

	progname = strrchr( argv[0], '\\' );
	if (NULL == progname) {
		progname = argv[0];
	} else {
		++progname;
	}

	if (argc < 2 || argc > 3) {
		return usage( progname );
	}

	if (argc == 3) {	/* check for nskill -0 case */
		pidstr = argv[2];
		if (argv[1][0] != '-') {
			return usage( progname );
		}
		checkStatusOnly = (argv[1][1] == '0');
	} else {
		pidstr = argv[1];
	}

	pid = strtoul( pidstr, NULL, 0 /* base is chosen based on leading chars. */ );
	hProcess = OpenProcess( PROCESS_TERMINATE, FALSE, pid );
	if (NULL == hProcess) {
		prefix_msg = "OpenProcess";
		rc = GetLastError();
	} else if (!checkStatusOnly) {
		if (!TerminateProcess( hProcess, -1 )) {
			prefix_msg = "TerminateProcess";
			rc = GetLastError();
		}
	}

	if ( 0 != rc ) {
		char *msg = "";
		(void) FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
							NULL, rc, 0, (char *)&msg, 0, NULL );
		fprintf( stderr, "%s: process %d error %d - %s", prefix_msg, pid, rc, msg );
		LocalFree(msg);
	}
	return rc;
}

