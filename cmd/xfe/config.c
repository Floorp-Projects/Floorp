/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   config.c --- link-time configuration of the X front end.
   Created: Jamie Zawinski <jwz@netscape.com>, 26-Feb-95.

   Parameters:

    - whether this is the "net" version or the "for sale" version
      this is done by changing the value of `fe_version' and `fe_version_short'
      based on -DVERSION=1.1N
    - whether this is linked with DNS or NIS for name resolution
      this is done by changing the value of `fe_HaveDNS'
      based on optional -DHAVE_NIS
    - whether high security or low security is being used
      this is done by linking in a particular version of libsec.a 
      along with -DUS_VERSION or -DEXPORT_VERSION (for verification.)
    - which animation to use
      this is done by linking in a particular version of icondata.o
      along with optional -DVENDOR_ANIM
 */


#include "name.h"

const char fe_BuildConfiguration[] = cpp_stringify(CONFIG);

const char fe_version[] = cpp_stringify(VERSION);
const char fe_long_version[] =
 "@(#)" XFE_NAME_STRING " "

#ifndef MOZ_COMMUNICATOR_NAME
 "Lite "
#endif

cpp_stringify(VERSION)

#ifdef EXPORT_VERSION
 "/Export"
#endif

#ifdef FRANCE_VERSION
 "/France"
#endif

#ifdef US_VERSION
 "/U.S."
#endif

#ifdef HAVE_NIS
 "/NIS"
#endif

#ifdef DEBUG
 "/DEBUG"
#endif

 ", " cpp_stringify(DATE) "; " XFE_LEGALESE
;

/* initially set to plain version, without locale string */
char *fe_version_and_locale = (char *) fe_version;

/* If no policy.jar, then no security at all. */
int fe_SecurityVersion = 0;

#ifdef HAVE_NIS
int fe_HaveDNS = 0;
#else
int fe_HaveDNS = 1;
#endif

#ifdef VENDOR_ANIM
int fe_VendorAnim = 1;
#else
int fe_VendorAnim = 0;
#endif
