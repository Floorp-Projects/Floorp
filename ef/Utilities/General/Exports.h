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

/*
 * Export definitions.
 */

#ifdef GENERATE_FOR_X86
  #ifdef __GNUC__
    #define NS_NATIVECALL(inReturnType) __attribute__((stdcall)) inReturnType
    #define NS_EXPORT 
    #define NS_EXTERN
  #else /* MSVC */
    #define NS_NATIVECALL(inReturnType) inReturnType __stdcall
    #define NS_EXPORT __declspec(dllexport)
    #ifdef IMPORTING_VM_FILES    /* Defined by all modules that import Vm header files */
      #define NS_EXTERN __declspec(dllimport)
    #else
      #define NS_EXTERN __declspec(dllexport)
    #endif

    #pragma warning(disable: 4251)
  #endif
#else
  #define NS_NATIVECALL(inReturnType) inReturnType
  #define NS_EXPORT 
  #define NS_EXTERN 
#endif
