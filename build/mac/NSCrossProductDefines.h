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

#error "Don’t use me!"

#define OLDROUTINELOCATIONS 0
#define XP_MAC 1
#define NSPR20 1
#define _NO_FAST_STRING_INLINES_ 1
#define HAVE_BOOLEAN 1
#define NETSCAPE 1
#define OTUNIXERRORS 1		/* We want OpenTransport error codes */

#define OJI 1

/*
	This compiles in heap dumping utilities and other good stuff
 for developers -- maybe we only want it in for a special SDK
 nspr/java runtime(?):
*/
#define DEVELOPER_DEBUG 1

#define	MAX(_a,_b)	((_a) < (_b) ? (_b) : (_a))
#define	MIN(_a,_b)	((_a) < (_b) ? (_a) : (_b))
