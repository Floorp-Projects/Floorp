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
#include "NativeMethodDispatcher.h"

#include <ctype.h>

#include "prio.h"
#include "plstr.h"
#include "prprf.h"
#include "prmem.h"

#include "CUtils.h"
#include "StringUtils.h"
#include "Address.h"
#include "NameMangler.h"

/* Each of these manglers must be registered using 
 * NativeMethodDispatcher::register
 */
#include "JNIManglers.h"
#include "NetscapeManglers.h"

Vector<NameMangler *> NativeMethodDispatcher::manglers;

/*
 * create a string for the native function name by adding the
 * appropriate system-specific decorations.
 *
 * On Win32, "__stdcall" functions are exported differently, depending
 * on the compiler. In MSVC 4.0, they are decorated with a "_" in the 
 * beginning, and @nnn in the end, where nnn is the number of bytes in 
 * the arguments (in decimal). Borland C++ exports undecorated names.
 *
 * buildFunName handles different encodings depending on the value 
 * of callType. It returns false when handed a callType it can't handle
 * or which does not apply to the current platform.
 */
#if defined(XP_PC)
bool NativeMethodDispatcher::buildFunName(char *name, Int32 nameMax, 
					  Method &m,
					  NativeCallingConvention callType,
					  NameMangler &mangler)
#else
/* Avoid those *$#&*&* unused variable warnings */
bool NativeMethodDispatcher::buildFunName(char *, Int32,
					  Method &,
					  NativeCallingConvention callType,
					  NameMangler &)
#endif
{
#if defined(XP_PC)
  if (callType == nativeCallingConventionStdCallMSVC) {
    /* For Microsoft MSVC 4.0/5.0 */
    char suffix[6];    /* This is enough since Java never has more than 
			  256 words of arguments. */
    int realArgsSize;
    int nameLen;
    int i;
    
    realArgsSize = m.getArgsSize() + 
      mangler.getExtraArgsSize((m.getModifiers() & CR_METHOD_STATIC) != 0);
    
    PR_snprintf(suffix, sizeof(suffix), "@%d", realArgsSize);
    
    nameLen = PL_strlen(name);
  
    if (nameLen >= nameMax - 7)
      return false;

    for(i = nameLen; i > 0; i--)
      name[i] = name[i-1];
    name[0] = '_';

    PR_snprintf(name + nameLen + 1, sizeof(suffix), "%s", suffix);
    return true;
  }   
#endif

  if (callType == nativeCallingConventionDefault)
    return true;
  else
    return false;
}

addr NativeMethodDispatcher::resolve(Method &method)
{
  const char *methodName = method.getName();
  const char *className = method.getDeclaringClass()->getName();

  const char *signature = method.getSignatureString();

  /* XXX This is not a correct upper-bound on the length */
  Int32 len = PL_strlen(methodName) + PL_strlen(className)*2 +
    PL_strlen(signature)*2 + sizeof("Netscape_Java___");
  
  TemporaryBuffer buf(len);
  char *name = buf;

  /* Try all the name-mangling schemes at our disposal, one after another..*/
  for (Uint32 i = 0; i < manglers.size(); i++) {
    if (!manglers[i]->mangle(className, methodName, signature, name, len))
      continue;
    
    Int32 numConventions = manglers[i]->numNativeConventions();
    const NativeCallingConvention *conventions = 
      manglers[i]->getNativeConventions();
    
    /* Try all the calling conventions that this mangling scheme supports */
    for (Int32 j = 0; j < numConventions; j++) {
      if (!buildFunName(name, len, method, conventions[j], 
			*manglers[i]))
	continue;
      
      void *address;
      PRLibrary *lib;

      if ((address = PR_FindSymbolAndLibrary(name, &lib)) != NULL)
	return functionAddress((void (*)())(manglers[i]->getCode(method, address)));
    }
  }

  return functionAddress((void (*)()) 0);
}
  

void NativeMethodDispatcher::staticInit()
{
  registerMangler(*new JNIShortMangler());
  registerMangler(*new JNILongMangler());
  registerMangler(*new NetscapeShortMangler());
  registerMangler(*new NetscapeLongMangler());
}

PRLibrary *NativeMethodDispatcher::loadLibrary(const char *simpleLibName)
{
  /* For now, we parse the path *each* time we load a library. In the future, 
   * NativeMethodDispatcher must be capable of storing a set of paths, and
   * each path could potentially be a directory, zip or jar file.
   */
  char *libPath = PR_GetLibraryPath();
  char *libName = 0;
  PRLibrary *lib = 0;
#if defined(XP_PC)
  static const char* PATH_SEPARATOR = ";";
#else
  static const char* PATH_SEPARATOR = ":";
#endif
  
  if (libPath) {
    Strtok tok(libPath);
    for (const char *token = tok.get(PATH_SEPARATOR); token; token = tok.get(PATH_SEPARATOR))
      if ((libName = PR_GetLibraryName(token, simpleLibName)) != 0) {
	lib = PR_LoadLibrary(libName);
	//free(libName);
	libName = 0;

	if (lib)
	  break;
      }
  } 
  
  /* Always search in the current directory */
  if (!lib && (libName = PR_GetLibraryName(".", simpleLibName)) != 0) {
    lib = PR_LoadLibrary(libName);
    //free(libName);
  }

  /* XXX The NSPR doc says it's our responsibility to free this, but in 
   * reality PR_GetLibraryPath() returns a pointer to the actual path. 
   * So until they fix this or clarify how PR_GetLibraryPath() should
   * behave, this line shall remain commented out -- Sriram, 8/12/97
   */
  /* if (libPath) PR_DELETE(libPath); */

  return lib;
}


#if 0
char *NativeMethodDispatcher::mangle(const char *className,
				     const char *methodName,
				     const char *signature,
				     char *buffer, Int32 len,
			       NativeMethodDispatcher::MangleType mangleType)
{
  char *bufptr = buffer;
  char *bufend = buffer + len; 

  /* For internal (Netscape-style) mangling, prefix a Netscape_ to the JNI-
   * style mangled name
   */
  if (mangleType == mangleTypeNetscapeShort || 
      mangleType == mangleTypeNetscapeLong) {
    PR_snprintf(bufptr, bufend - bufptr, "Netscape_");
    bufptr += PL_strlen(bufptr);
    mangleType = (mangleType == mangleTypeNetscapeShort) ? 
      mangleTypeJNIShort: mangleTypeJNILong;
      
  } else
    *bufptr = 0;
  
  /* Copy over Java_ */
  PL_strcat(bufptr, "Java_");
  bufptr += PL_strlen(bufptr);

  bufptr += NativeMethodDispatcher::mangleUTFString(className, bufptr, 
						    bufend - bufptr, 
						    mangleUTFJNI);

  if (bufend - bufptr > 1) 
    *bufptr++ = '_';
  
  bufptr += NativeMethodDispatcher::mangleUTFString(methodName, bufptr, 
						    bufend - bufptr, 
						    mangleUTFJNI);

  /* JNI long name: includes a mangled signature. */
  if (mangleType == mangleTypeJNILong) {
    char sig[1024];
    int i;
    if (bufend - bufptr > 2) {
      *bufptr++ = '_';
      *bufptr++ = '_';
    }

    for (i = 0; i<1023; i++)
      if ((sig[i] = signature[i]) == ')')
	break;
    
    sig[i] = 0;
    bufptr += NativeMethodDispatcher::mangleUTFString(sig, bufptr, 
						       bufend - bufptr, 
						       mangleUTFJNI);
  }

  return buffer;
}
#endif







