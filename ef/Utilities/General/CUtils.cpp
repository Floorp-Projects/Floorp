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

#include "CUtils.h"

#ifndef NO_NSPR
#include "plstr.h"
#endif

#include <string.h>

char *Strtok::get(const char *pattern)
{
  char *start;
  char *curr;
  
  for (start = string, curr = string; *curr; curr++) 
#ifdef NO_NSPR
    if (strchr(pattern, *curr))
#else
    if (PL_strchr(pattern, *curr)) 
#endif
      break;
      
  if (*curr) {
    *curr = 0;
    string = curr+1;
  } else
    string = curr;

  return (*start) ? start : 0;
}


char *dupString(const char *str, Pool &pool)
{
  if (!str)
    return 0;

#ifdef NO_NSPR
  char *dup = new (pool) char[strlen(str)+1];
  strcpy(dup, str);
#else
  char *dup = new (pool) char[PL_strlen(str)+1];
  PL_strcpy(dup, str);
#endif
  
  return dup;
}

char *append(char *&s, Uint32 &strSize, const char *t)
{
#ifdef NO_NSPR
  Uint32 slen = strlen(s);
  Uint32 tlen = strlen(t);
#else
  Uint32 slen = PL_strlen(s);
  Uint32 tlen = PL_strlen(t);
#endif
  
  if (slen+tlen+1 > strSize) {
    strSize += tlen+256;
    char *temp = new char[strSize];
#ifdef NO_NSPR
    strcpy(temp, s);
#else
    PL_strcpy(temp, s);
#endif
    delete [] s;
    s = temp;
  } 
  
  strcat(s, t);
  return s;
}

