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
#include "NameMangler.h"

#include <ctype.h>

#include "prprf.h"
#include "plstr.h"

typedef Uint16 unicode;

Int32 NameMangler::mangleUTFString(const char *name, 
				   char *buffer, 
				   int buflen, 
				   NameMangler::MangleUTFType type)
{
    // IMPLEMENT
    PR_ASSERT(0);
    return 0;
}

Int32 NameMangler::mangleUnicodeChar(Int32 ch, char *bufptr, 
				     char *bufend) 
{
  char temp[10];
  PR_snprintf(temp, 10, "_%.5x", ch);  /* six characters always plus null */
  (void) PR_snprintf(bufptr, bufend - bufptr, "%s", temp);
  return PL_strlen(bufptr);
}

