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
   dns-stub.c --- SunOS sucks and then you die.
   This is linked into the SunOS executable which is NOT linked with -lresolv.
   Created: Jamie Zawinski <jwz@netscape.com>, 31-Oct-94.
 */


#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

struct state _res;

int 
res_init (void)
{
  return 0;
}
