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

/*
 * Export definitions.
 */

#ifdef XP_PC
  #define NS_NATIVECALL(inReturnType) inReturnType __stdcall
  #define NS_EXPORT __declspec(dllexport)

  #ifdef IMPORTING_VM_FILES    /* Defined by all modules that import Vm header files */
    #define NS_EXTERN __declspec(dllimport)
  #else
    #define NS_EXTERN __declspec(dllexport)
  #endif

 #pragma warning(disable: 4251)
#elif defined(LINUX) || defined(FREEBSD)
  #define NS_NATIVECALL(inReturnType) __attribute__((stdcall)) inReturnType
  #define NS_EXPORT 
  #define NS_EXTERN 
#else
  #define NS_NATIVECALL(inReturnType) inReturnType
  #define NS_EXPORT 
  #define NS_EXTERN 
#endif
