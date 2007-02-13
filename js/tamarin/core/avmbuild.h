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
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
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

#ifndef __avmbuild__
#define __avmbuild__


#ifdef _MAC
  #ifndef AVMPLUS_MAC
    #define AVMPLUS_MAC
  #endif
#endif

#ifdef WIN32
  #ifndef AVMPLUS_WIN32
    #define AVMPLUS_WIN32
  #endif
#endif

#ifdef AVMPLUS_MAC
  // Are we PowerPC or i386 (Macintel) or x86_64 (64-bit)?
  #if __i386__
    #ifndef AVMPLUS_IA32
      #define AVMPLUS_IA32
    #endif
    #ifndef AVMPLUS_CDECL
      #define AVMPLUS_CDECL
    #endif
  #elif (__x86_64__)
    #ifndef AVMPLUS_AMD64
      #define AVMPLUS_AMD64
    #endif
    #ifndef AVMPLUS_64BIT
	  #define AVMPLUS_64BIT
    #endif
  #elif (__ppc__)
    #ifndef AVMPLUS_PPC
      #define AVMPLUS_PPC
    #endif	
  #endif
#endif

#ifdef AVMPLUS_WIN32
  #ifndef AVMPLUS_IA32
    #define AVMPLUS_IA32
  #endif
#endif

/// START: CRUFT 
//
// guard pages are created for buffers rather than explicit overflow checks
// this define exists only for the interm In a few days I'll toast the rest
// of the non-guard logic and we'll see the end of the days of estimatation!
// [rickr-Jun16,05]

#define FEATURE_BUFFER_GUARD

// FEATURE_BUFFER_GUARD not yet supported on ARM
#ifdef AVMPLUS_ARM
  #undef FEATURE_BUFFER_GUARD
#endif

// FEATURE_BUFFER_GUARD not supported on Mac CFM
#ifdef AVMPLUS_MAC
  #if !TARGET_RT_MAC_MACHO
    #undef FEATURE_BUFFER_GUARD
  #endif
  // not supported yet in 64-bit mode
  #ifdef AVMPLUS_AMD64
    #undef FEATURE_BUFFER_GUARD
  #endif
#endif

#define AVMPLUS_MIR
#define AVMPLUS_INTERP

// if a function meets the E4 criteria for being unchecked, then make
// all its parameters optional and add a rest arg.  asc should do this
// at compile time so we don't have to do it in avmplus.
#define AVMPLUS_UNCHECKED_HACK

#ifdef DEBUGGER
#define AVMPLUS_VERIFYALL
#endif

#if defined(DEBUG) || defined(_DEBUG) || defined(DEBUGGER)
#define AVMPLUS_VERBOSE
#define AVMPLUS_PROFILE
#endif

// #undef verify, a Mac thing
#undef verify

#ifdef AVMPLUS_MAC
#if !TARGET_RT_MAC_MACHO
#define AVMPLUS_MAC_CARBON
#endif
#endif

#ifdef AVMPLUS_MAC
  #ifdef FEATURE_BUFFER_GUARD
    #define AVMPLUS_MACH_EXCEPTIONS
  #endif
#endif

#ifdef AVMPLUS_MACH_EXCEPTIONS
  #ifdef AVMPLUS_PPC
    // Support for running the PowerPC version under Rosetta
    #define AVMPLUS_ROSETTA
  #endif
#endif

/**
 * We have avmplus.vcproj compiled with the /W4 warning level
 * which is quite picky.  Disable warnings we don't care about.
 */
#ifdef _MSC_VER
	#pragma warning(disable:4512) //assignment operator could not be generated
	#pragma warning(disable:4511) //can't generate copy ctor
	#pragma warning(disable:4127) //conditional expression is constant
	#pragma warning(disable:4611) //interaction between _setjmp and destruct
	#pragma warning(disable:4725) //instruction may be inaccurate on some Pentiums

	// enable some that are off even in /W4 mode, but are still handy
	#pragma warning(error:4265)	// 'class' : class has virtual functions, but destructor is not virtual
	#pragma warning(error:4905)	// wide string literal cast to 'LPSTR'
	#pragma warning(error:4906)	// string literal cast to 'LPWSTR'
	#pragma warning(error:4263)	// 'function' : member function does not override any base class virtual member function
	#pragma warning(error:4264)	// 'virtual_function' : no override available for virtual member function from base 'class'; function is hidden
	#pragma warning(error:4266)	// 'function' : no override available for virtual member function from base 'type'; function is hidden
	#pragma warning(error:4242) // 'identifier' : conversion from 'type1' to 'type2', possible loss of data
	#pragma warning(error:4263) // member function does not override any base class virtual member function

	// some that might be useful to turn on someday, but would require too much twiddly code tweaking right now
//	#pragma warning(error:4296)	// expression is always true (false) (Generally, an unsigned variable was used in a comparison operation with zero.)

#endif

// extra safety checks during parsing
// this will be turned off for desktop players once we get the gaurd pages in
// Mobile players that may not be able to use guard pages(exceptions) so will 
//want to turn this on
#define SAFE_PARSE

// Enable interfacing Java
#define FEATURE_JNI
#endif /* __avmbuild__ */
