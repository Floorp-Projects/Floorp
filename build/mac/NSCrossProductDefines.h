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

#error "Don’t use me!"

#define OLDROUTINELOCATIONS 0
#define XP_MAC 1
#ifndef NSPR20
#define NSPR20 1
#endif
#define _NSPR 1
#define _NO_FAST_STRING_INLINES_ 1
#define HAVE_BOOLEAN 1
#define NETSCAPE 1
#define OTUNIXERRORS 1		/* We want OpenTransport error codes */

#define OJI 1
#define MOCHA 1

/*
	This compiles in heap dumping utilities and other good stuff
 for developers -- maybe we only want it in for a special SDK
 nspr/java runtime(?):
*/
#define DEVELOPER_DEBUG 1

#define	MAX(_a,_b)	((_a) < (_b) ? (_b) : (_a))
#define	MIN(_a,_b)	((_a) < (_b) ? (_a) : (_b))
