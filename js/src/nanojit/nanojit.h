/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is [Open Source Virtual Machine].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __nanojit_h__
#define __nanojit_h__

#include <stddef.h>
#include "avmplus.h"

#ifdef AVMPLUS_IA32
#define NANOJIT_IA32
#elif AVMPLUS_ARM
#define NANOJIT_ARM
#elif AVMPLUS_PPC
#define NANOJIT_PPC
#elif AVMPLUS_AMD64
#define NANOJIT_AMD64
#define NANOJIT_64BIT
#else
#error "unknown nanojit architecture"
#endif

namespace nanojit
{
	/**
	 * -------------------------------------------
	 * START AVM bridging definitions
	 * -------------------------------------------
	 */
	class Fragment;
	class LIns;
	struct SideExit;
	class RegAlloc;
	typedef avmplus::AvmCore AvmCore;
	typedef avmplus::OSDep OSDep;
	typedef avmplus::GCSortedMap<const void*,Fragment*,avmplus::LIST_GCObjects> FragmentMap;
	typedef avmplus::SortedMap<SideExit*,RegAlloc*,avmplus::LIST_GCObjects> RegAllocMap;
	typedef avmplus::List<LIns*,avmplus::LIST_NonGCObjects>	InsList;
	typedef avmplus::List<char*, avmplus::LIST_GCObjects> StringList;

	#if defined(_DEBUG)
		
		#ifndef WIN32
			#define DebugBreak() AvmAssert(0)
		#endif

		#define _NanoAssertMsg(a,m)		do { if ((a)==0) { AvmDebugLog(("%s", m)); DebugBreak(); } } while (0)
		#define NanoAssertMsg(x,y)				do { _NanoAssertMsg((x), (y)); } while (0) /* no semi */
		#define _NanoAssertMsgf(a,m)		do { if ((a)==0) { AvmDebugLog(m); DebugBreak(); } } while (0)
		#define NanoAssertMsgf(x,y)				do { _NanoAssertMsgf((x), y); } while (0) /* no semi */
		#define NanoAssert(x)					_NanoAssert((x), __LINE__,__FILE__)
		#define _NanoAssert(x, line_, file_)	__NanoAssert((x), line_, file_)
		#define __NanoAssert(x, line_, file_)	do { NanoAssertMsg((x), "Assertion failed: \"" #x "\" (" #file_ ":" #line_ ")"); } while (0) /* no semi */
	#else
		#define NanoAssertMsgf(x,y)	do { } while (0) /* no semi */
		#define NanoAssertMsg(x,y)	do { } while (0) /* no semi */
		#define NanoAssert(x)		do { } while (0) /* no semi */
	#endif

	/**
	 * -------------------------------------------
	 * END AVM bridging definitions
	 * -------------------------------------------
	 */
}

#ifdef AVMPLUS_VERBOSE
#define NJ_VERBOSE 1
#define NJ_PROFILE 1
#endif

#ifdef NJ_VERBOSE
	#include <stdio.h>
	#define verbose_output						if (verbose_enabled()) Assembler::output
	#define verbose_outputf						if (verbose_enabled()) Assembler::outputf
	#define verbose_enabled()					(_verbose)
	#define verbose_only(x)						x
#else
	#define verbose_output
	#define verbose_outputf
	#define verbose_enabled()
	#define verbose_only(x)
#endif /*NJ_VERBOSE*/

#ifdef _DEBUG
	#define debug_only(x)			x
#else
	#define debug_only(x)
#endif /* DEBUG */

#ifdef NJ_PROFILE
	#define counter_struct_begin()  struct {
	#define counter_struct_end()	} _stats;
	#define counter_define(x) 		int32_t x
	#define counter_value(x)		_stats.x
	#define counter_set(x,v)		(counter_value(x)=(v))
	#define counter_adjust(x,i)		(counter_value(x)+=(int32_t)(i))
	#define counter_reset(x)		counter_set(x,0)
	#define counter_increment(x)	counter_adjust(x,1)
	#define counter_decrement(x)	counter_adjust(x,-1)
	#define profile_only(x)			x
#else
	#define counter_struct_begin()
	#define counter_struct_end()
	#define counter_define(x) 		
	#define counter_value(x)
	#define counter_set(x,v)
	#define counter_adjust(x,i)
	#define counter_reset(x)
	#define counter_increment(x)	
	#define counter_decrement(x)	
	#define profile_only(x)	
#endif /* NJ_PROFILE */

#define isS8(i)  ( int32_t(i) == int8_t(i) )
#define isU8(i)  ( int32_t(i) == uint8_t(i) )
#define isS16(i) ( int32_t(i) == int16_t(i) )
#define isU16(i) ( int32_t(i) == uint16_t(i) )

#define alignTo(x,s)		((((uintptr_t)(x)))&~(((uintptr_t)s)-1))
#define alignUp(x,s)		((((uintptr_t)(x))+(((uintptr_t)s)-1))&~(((uintptr_t)s)-1))

#define pageTop(x)			( (int*)alignTo(x,NJ_PAGE_SIZE) )
#define pageBottom(x)		( (int*)(alignTo(x,NJ_PAGE_SIZE)+NJ_PAGE_SIZE)-1 )
#define samepage(x,y)		(pageTop(x) == pageTop(y))

#include "Native.h"
#include "LIR.h"
#include "RegAlloc.h"
#include "Fragmento.h"
#include "Assembler.h"
#include "TraceTreeDrawer.h"

#endif // __nanojit_h__
