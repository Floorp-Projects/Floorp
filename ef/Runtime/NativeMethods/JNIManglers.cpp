/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "JNIManglers.h"
#include "NativeMethodStubs.h"

#include "prprf.h"
#include "plstr.h"

static NativeCallingConvention JNIConventions[] = {
#ifdef XP_PC
  nativeCallingConventionStdCallMSVC,
#endif
  nativeCallingConventionDefault
};

Int32 JNINumConventions = sizeof(JNIConventions) / sizeof(JNIConventions[0]);

JNIShortMangler::JNIShortMangler() : 
  NameMangler(JNINumConventions, JNIConventions) { }

char *JNIShortMangler::mangle(const char *className,
			      const char *methodName,
			      const char * /*signature*/,
			      char *buffer,
			      Int32 len)
{
  char *bufptr = buffer;
  char *bufend = buffer + len; 
      
  /* Copy over Java_ */
  PL_strcpy(bufptr, "Java_");
  bufptr += PL_strlen(bufptr);
  
  bufptr += NameMangler::mangleUTFString(className, bufptr, 
					 bufend - bufptr, 
					 NameMangler::mangleUTFClass);

  if (bufend - bufptr > 1) 
    *bufptr++ = '_';
  else
    return 0;

  bufptr += NameMangler::mangleUTFString(methodName, bufptr, 
					 bufend - bufptr, 
					 NameMangler::mangleUTFJNI);

  return buffer;

}

Int32 JNIShortMangler::getExtraArgsSize(bool staticMethod) 
{ 
  if (staticMethod)
    return 8; /* JNIEnv, jclass */
  else
    return 4; /* JNIEnv */
}

void *JNIShortMangler::getCode(const Method &method, void *actualCode)
{
  return generateJNIGlue(method, actualCode);
}

char *JNILongMangler::mangle(const char *className,
			     const char *methodName,
			     const char *signature,
			     char *buffer,
			     Int32 len)
{
  buffer = JNIShortMangler::mangle(className, methodName, signature,
				   buffer, len);
  len -= PL_strlen(buffer);
  char *bufptr = buffer+PL_strlen(buffer);
  char *bufend = buffer + len; 

  char sig[1024];
  int i;
  if (bufend - bufptr > 2) {
    *bufptr++ = '_';
    *bufptr++ = '_';
  } else
    return 0;

  for (i = 0; i<1023; i++)
    if ((sig[i] = signature[i]) == ')')
      break;
  
  sig[i] = 0;
  bufptr += NameMangler::mangleUTFString(sig, bufptr, 
					 bufend - bufptr, 
					 mangleUTFJNI);

  return buffer;
}

