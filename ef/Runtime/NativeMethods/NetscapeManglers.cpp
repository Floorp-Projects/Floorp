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
#include "NetscapeManglers.h"

#include "prprf.h"
#include "plstr.h"

char *NetscapeShortMangler::mangle(const char *className,
				   const char *methodName,
				   const char *signature,
				   char *buffer,
				   Int32 len)
{
  char *bufptr = buffer;
  char *bufend = buffer + len; 

  /* Prefix a Netscape_ to the JNI-mangled name */
  PR_snprintf(bufptr, bufend - bufptr, "Netscape_");
  Int32 bufptrLen = PL_strlen(bufptr);
  bufptr += bufptrLen;
  len -= bufptrLen;

  return (JNIShortMangler::mangle(className, methodName, signature, 
				  bufptr, len)) ? buffer : 0;
      
}

Int32 NetscapeShortMangler::getExtraArgsSize(bool)
{
  return 0;  /* Our call stack is the same as for a Java method */
} 

void *NetscapeShortMangler::getCode(const Method &, void *actualCode)
{
  return actualCode;
}


char *NetscapeLongMangler::mangle(const char *className,
				  const char *methodName,
				  const char *signature,
				  char *buffer,
				  Int32 len)
{
  char *bufptr = buffer;
  char *bufend = buffer + len; 

  /* Prefix a Netscape_ to the JNI-mangled name */
  PR_snprintf(bufptr, bufend - bufptr, "Netscape_");
  Int32 bufptrLen = PL_strlen(bufptr);
  bufptr += bufptrLen;
  len -= bufptrLen;

  return (JNILongMangler::mangle(className, methodName, signature, 
				 bufptr, len)) ? buffer : 0;
}


Int32 NetscapeLongMangler::getExtraArgsSize(bool)
{
  return 0;  /* Our call stack is the same as for a Java method */
} 

void *NetscapeLongMangler::getCode(const Method &, void *actualCode)
{
  return actualCode;
}
