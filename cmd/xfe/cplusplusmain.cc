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
/* 
 *
 *  cplusplusmain.cc --- C++ main for compilers/linkers/link formats that require it.
 *
 *  This file is required when you are linking in C++ code. Some older C++
 *  linkers (HP, COFF I think, etc) require main() to be in a C++ file for
 *  the C++ runtime loader to be linked in or invoked. This file does nothing
 *  but make a call to to the real main in mozilla.c, but it will link the C++
 *  runtime loader.
 *
 *  Created: David Williams <djw@netscape.com>, May-26-1996
 *
 *  RCSID: "$Id: cplusplusmain.cc,v 3.2 1998/07/07 06:15:33 ramiro Exp $"
 */


//
// As of Communicator 4.0b2, the client (which is built on hpux9) does not run
// on hpux10 because of the usual incompatibility stuff.
//
// The hard working xheads have built and run succesfully on 10, but the
// build process did not get updated in time for the 4.0b2 release.
//
// So if the client is run on hpux 10, the user will get a nasty surprise.
//

#if defined(HPUX9) || defined(hpux9)
#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>

extern "C" char *XP_GetString(int i);
extern "C" int XFE_HPUX_VERSION_NONSENSE;

#endif

extern "C" int mozilla_main(int argc, char** argv);

int
main(int argc, char** argv)
{

#if defined(HPUX9) || defined(hpux9)

	struct utsname name;

	uname(&name);

	if ((name.release[2] == '1') && (name.release[3] == '0'))
	{
		fprintf(stdout,
				XP_GetString(XFE_HPUX_VERSION_NONSENSE),
				argv[0],
				name.sysname,
				name.release);
		
		exit(1);
	}

#endif

	return mozilla_main(argc, argv);
}
