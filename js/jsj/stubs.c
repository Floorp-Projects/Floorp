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
/* ** */
/*
  !!! WARNING: If you add files here, be sure to:
	1. add them to the Metrowerks project
	2. edit Makefile and makefile.win to add a dependency on the .c file
  !!! HEED THE WARNING.
*/

#if defined (JAVA)
#define IMPLEMENT_netscape_javascript_JSObject
#define IMPLEMENT_netscape_javascript_JSException

#ifndef XP_MAC
#include "_jri/netscape_javascript_JSObject.c"
#include "_jri/netscape_javascript_JSException.c"

#else
#include "netscape_javascript_JSObject.c"
#include "n_javascript_JSException.c"

#endif

void _java_javascript_init(void) { }

#endif /* defined (JAVA) */
