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
#ifndef _MoreMixedMode_
#define _MoreMixedMode_

/*
	In a .h file, global (procedure declaration):
	extern RoutineDescriptor 	foo_info;
	PROCDECL(extern,foo);
	
	In the .h file, class scoped (member declaration):
	static RoutineDescriptor 	foo_info;
	PROCDECL(static,foo);
	
	In the .cp file, global (procedure definition):
	RoutineDescriptor 			foo_info = RoutineDescriptor (foo);
	PROCEDURE(foo,uppFooType);
	
	In the .cp file, class scoped (member definition):
	RoutineDescriptor 			Foo::foo_info = RoutineDescriptor (Bar::foo);
	PROCDEF(Foo::,Bar::,foo,uppFooType);
	
	References to the function:
		foo_info
		PROCPTR(foo)
*/

#include "MixedMode.h"
#if GENERATINGCFM
#	define PROCPTR(NAME)			NAME##_info
#	define PROCDECL(MOD,NAME)		MOD RoutineDescriptor PROCPTR(NAME);
#	define PROCDEF(S1,S2,NAME,INFO)	RoutineDescriptor S1##PROCPTR(NAME) = BUILD_ROUTINE_DESCRIPTOR (INFO, S2##NAME);
#	define PROCEDURE(NAME,INFO)		RoutineDescriptor PROCPTR(NAME) = BUILD_ROUTINE_DESCRIPTOR (INFO, NAME);
#else
#	define PROCPTR(NAME)			NAME
#	define PROCDECL(MOD,NAME)
#	define PROCDEF(S1,S2,NAME,INFO)
#	define PROCEDURE(NAME,INFO)	
#endif

#endif // _MoreMixedMode_

