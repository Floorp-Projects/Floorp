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
/* 
 * xfe_dns.h --- hooking X Mozilla into the portable nonblocking DNS code.
 * Created: Jamie Zawinski <jwz@netscape.com>, 24-Dec-96.
 */


#ifndef __XFE_DNS_H__
#define __XFE_DNS_H__

#include "xp.h"
#include <X11/Intrinsic.h>	/* for XtAppContext */

/* Call this first thing in main().
   (It might not return, but might exec some other program at the other
   end of a fork, so don't do *anything* before you call it.)
 */
void XFE_InitDNS_Early(int argc, char **argv);

/* Call this some time later, when it's finally safe to call XtAppAddInput().
 */
void XFE_InitDNS_Late(XtAppContext app);

#endif /* __XFE_DNS_H__ */
