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
#ifndef _NATIVE_METHOD_DISPATCHER_H_
#define _NATIVE_METHOD_DISPATCHER_H_

#include "prlink.h"

#include "ClassCentral.h"
#include "ClassFileSummary.h"
#include "FieldOrMethod.h"
#include "Vector.h"
#include "NameMangler.h"

/* A quick note on "internal-style" native methods. Other than the JRI
 * and JNI, the VM also supports a third kind of native-method dispatch.
 * In this approach, native method names are mangled exactly as in the JNI,
 * but with a Netscape_ prefixed to the JNI-style mangled name. The 
 * arguments to the native methods are C++ counterparts to the Java Objects
 * used in the VM. The javah program will generate headers and stubs for 
 * these functions. Fields are accessed directly, via the C++ structures.
 * Methods (for now) are called via the reflection API.
 */


class NativeMethodDispatcher {
public:
  static void staticInit();

  /* Loads a shared library whose simple name (without a system-specific
   * extension) is given by simpleLibName. Returns a handle to the library
   * on success, NULL on failure.
   */
  static PRLibrary *loadLibrary(const char *simpleLibName); 
  
  /* Resolve the native-method whose information is given by m. This method
   * mangles the methodname, and returns the address of the method to call.
   * Platform-specific directives (such as the __stdcall suffix on windows)
   * are added by this method; the name should not contain those.
   */
  static addr resolve(Method &m);
  
  /* Register a native name mangler */
  static void registerMangler(NameMangler &mangler) {
    manglers.append(&mangler);
  }


private:

  /* Array of name-mangler classes, each of which implements a particular 
   * name-mangling scheme
   */
  static Vector<NameMangler *> manglers;
  
  /* Given a mangled function name, construct a platform and compiler-
   * specific symbol that can be searched for in the set of loaded libraries.
   * name is the mangled function name. method contains run-time information
   * about the method. callType is the native calling convention to use.
   * mangler is a class that mangles the function name and adds additional
   * arguments, if neccessary.  
   */
  static bool buildFunName(char *name, Int32 nameMax, 
			   Method &method, 
			   NativeCallingConvention callType,
			   NameMangler &mangler);
  
};

#endif /* _NATIVE_METHOD_DISPATCHER_H_ */



