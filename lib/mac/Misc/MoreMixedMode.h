/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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

