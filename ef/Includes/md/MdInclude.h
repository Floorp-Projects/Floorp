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

#ifndef _MD_INCLUDE_H_
#define _MD_INCLUDE_H_

/* This file contains the definition of platform specific macros. The variable
 * TARGET_CPU must be set before processing this file.
 *
 * The macros are:
 *
 *  - MD_INCLUDE_FILE(filename):
 *     #include MD_INCLUDE_FILE(Asm.h) with TARGET_CPU set to x86 
 *     will produce #include <md/x86/Asm.h>
 *
 */

/* String concatenation for the preprocessor. The XCAT* macros will cause the
 * inner CAT* macros to be evaluated first, producing still-valid pp-tokens.
 * Then the final concatenation can be done.
 */

#if defined(__STDC__)
 #define CAT(a,b) a##b
 #define CAT3(a,b,c) a##b##c
#else /* ! __STDC__ */
 #define CAT(a,b) a/**/b
 #define CAT3(a,b,c) a/**/b/**/c
#endif /* __STDC__ */

#define XCAT(a,b) CAT(a,b)
#define XCAT3(a,b,c) CAT3(a,b,c)

/* _MD_DIRNAME is the include directory name string for target architecture.
 * _MD_FILE_NAME is the complete path to the m.dep. file.
 */

#define _MD_DIR_NAME() XCAT3(md/,TARGET_CPU,/)
#define _MD_FILE_NAME(name) XCAT3(_MD_DIR_NAME(),TARGET_CPU,name)


/* Public:
 */

#define MD_INCLUDE_FILE(name) XCAT3(<,_MD_FILE_NAME(name),>) 

#endif /* _MD_INCLUDE_H_ */
