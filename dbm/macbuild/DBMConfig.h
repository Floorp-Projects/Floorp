/* 
 *           CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF
 *              NETSCAPE COMMUNICATIONS CORPORATION
 * Copyright © 1996, 1997 Netscape Communications Corporation.  All Rights
 * Reserved.  Use of this Source Code is subject to the terms of the
 * applicable license agreement from Netscape Communications Corporation.
 * The copyright notice(s) in this Source Code does not indicate actual or
 * intended publication of this Source Code.
 */

// we have to do this here because ConditionalMacros.h will be included from
// within OpenTptInternet.h and will stupidly define these to 1 if they
// have not been previously defined. The new PowerPlant (CWPro1) requires that
// this be set to 0. (pinkerton)
#define OLDROUTINENAMES 0
#ifndef OLDROUTINELOCATIONS
	#define OLDROUTINELOCATIONS	0
#endif

#define XP_MAC 1
#define _PR_NO_PREEMPT 1
#define _NO_FAST_STRING_INLINES_ 1
#define NSPR20 1

// OpenTransport.h has changed to not include the error messages we need from
// it unless this is defined. Why? dunnno...(pinkerton)
#define OTUNIXERRORS 1

#include "IDE_Options.h"

/* Make sure that "macintosh" is defined. */
#ifndef macintosh
#define macintosh 1
#endif
