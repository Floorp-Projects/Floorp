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
#ifndef _NAME_MANGLER_H_
#define _NAME_MANGLER_H_

#include "Fundamentals.h"
#include "FieldOrMethod.h"

/* Some platforms (ok, windows) have more than one native calling 
 * convention. This enumerates the different available types.
 */
enum NativeCallingConvention {
  nativeCallingConventionInvalid = 0,
  nativeCallingConventionStdCallMSVC,   /* Windows MS VC only: stdcall */
  nativeCallingConventionCCallMSVC,     /* Windows only: C call */
  nativeCallingConventionFastCallMSVC,  /* Windows only: fast call */
  nativeCallingConventionDefault        /* Default C calling convention on
					 * platforms other than windows.
					 */
};

/* A NameMangler class handles name-mangling for a particular type
 * of native-method dispatch. It does not do the platform-specific
 * name-mangling although it provides information about which calling
 * conventions are supported (if the native platform supports more than
 * one). 
 */
class NS_EXTERN NameMangler {
public:
  NameMangler(Int32 numCallingConventions, 
	      NativeCallingConvention *conventions) : 
  numCallingConventions(numCallingConventions),
  conventions(conventions) { } 

  /* Given a classname, method name and signature string of the method,
   * produce a mangled name that can be looked for in a shared lib/symbol
   * table. buffer must be preallocated by the caller and must be of length
   * len. If there is not enough space for the mangled name, this routine 
   * returns 0.
   */
  virtual char *mangle(const char *className,
		       const char *methodName,
		       const char *signature,
		       char *buffer,
		       Int32 len) = 0;

  /* Return extra size in bytes of any extra arguments that are to
   * be passed to the native method. Note that the VM has already taken into
   * account the "this" pointer to an object, which is passed as the
   * first argument to the method. if staticMethod is true, then the
   * method in question is a static method. Otherwise, it's an instance
   * method.
   */
  virtual Int32 getExtraArgsSize(bool staticMethod) = 0;

  /* Return a pointer to the first instruction of the "processed"
   * native method. "method" is the method in question, and actualCode
   * is a pointer to the native code as found in a shared library.
   * This method could potentially insert stubs and/or modify
   * the code appropriately so that the VM can call this 
   * method. 
   */
  virtual void *getCode(const Method &method, void *actualCode) = 0;

  /* return the number of native method conventions on the given
   * platform allowed for this mangling scheme.
   */
  Int32 numNativeConventions() { return numCallingConventions; }
  
  /* Get the native calling conventions supported by this mangling 
   * scheme.
   */
  const NativeCallingConvention *getNativeConventions() 
  { return conventions; }

  /* A MangleUtfType indicates type of transformation to apply to some UTF 
   * characters encountered in the string to be mangled. 
   */
  enum MangleUTFType {
    mangleUTFInvalid=0,
    mangleUTFClass, /* Mangle only the forward slashes in class names */
    mangleUTFFieldStub,
    mangleUTFSignature,
    mangleUTFJNI    /* Perform JNI-style mangling; underscore -> _1,
		     * semicolon -> _2, etc
		     */
  };
  
protected:
  static Int32 mangleUTFString(const char *name, char *buffer, 
			       int buflen, MangleUTFType type);

  static Int32 mangleUnicodeChar(Int32 ch, char *bufptr, char *bufend);
  
  Int32 numCallingConventions;
  NativeCallingConvention *conventions;
};
 

#endif /* _NAME_MANGLER_H_ */
