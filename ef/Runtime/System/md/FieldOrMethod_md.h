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
#ifndef _FIELD_OR_METHOD_MD_H_
#define _FIELD_OR_METHOD_MD_H_

#if defined(XP_PC) || defined(LINUX) || defined(FREEBSD)
#define PC_ONLY(x) x
#else
#define PC_ONLY(x) 
#endif

#if defined(XP_PC) 
#	define prepareArg(i, arg) _asm { push arg }     // Prepare the ith argument
#	define callFunc(func) _asm { call func }        // Call function func
#	define getReturnValue(ret) _asm {mov ret, eax}  // Put return value into ret
#elif defined(LINUX) || defined(FREEBSD)
#	define prepareArg(i, arg) __asm__ ("pushl %0" : /* no outputs */ : "g" (arg))     // Prepare the ith argument
#	define callFunc(func) __asm__ ("call *%0" : /* no outputs */ : "r" (func))        // Call function func
#	define getReturnValue(ret) __asm__ ("movl %%eax,%0" : "=g" (ret) : /* no inputs */)  // Put return value into ret
#elif defined(GENERATE_FOR_PPC)
#	ifdef XP_MAC
#		define prepareArg(i, arg) 
#		define callFunc(func) 
#		define getReturnValue(ret)  ret = 0
#	else
#		define prepareArg(i, arg) 
#		define callFunc(func) 
#		define getReturnValue(ret)  ret = 0
#	endif
#endif


#endif /* _FIELD_OR_METHOD_MD_H_ */
