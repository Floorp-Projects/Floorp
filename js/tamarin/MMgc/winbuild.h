/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
 * "License"); you may not use this file except in compliance with the License. You may obtain 
 * a copy of the License at http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
 * language governing rights and limitations under the License. 
 * 
 * The Original Code is [Open Source Virtual Machine.] 
 * 
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
 * by the Initial Developer are Copyright (C)[ 1993-2006 ] Adobe Systems Incorporated. All Rights 
 * Reserved. 
 * 
 * Contributor(s): Adobe AS3 Team
 * 
 * Alternatively, the contents of this file may be used under the terms of either the GNU 
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this 
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions 
 * above and replace them with the notice and other provisions required by the GPL or the 
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
 * under the terms of any one of the MPL, the GPL or the LGPL. 
 * 
 ***** END LICENSE BLOCK ***** */


/**
 * Critical section on GCHeap allocations.
 */
#define GCHEAP_LOCK

/**
 * IA32 (Intel architecture)
 */
#define MMGC_IA32

/**
 * Define this to get stack traces.  Helps with memory leaks.
 */
#ifdef _DEBUG
#define MEMORY_INFO
#endif

/**
 * This turns on incremental collection as well as all of
 * the write barriers.
 */
#define WRITE_BARRIERS

/**
 * Define this if MMgc is being integrated with avmplus.
 * Activates dynamic profiling support, etc.
 */
#define MMGC_AVMPLUS

/**
 * Use VirtualAlloc to reserve/commit memory
 */
#define USE_MMAP

/**
 * Define this to track GC pause times
 */
// uncommenting requires you to link with C runtime
//#define GC_STATS

/**
 * Turn this on to decommit memory 
 */
#define DECOMMIT_MEMORY

/**
 * Controls whether DRC is in use
 */
#define MMGC_DRC

/**
 * This makes JIT code buffers read-only to reduce the probability of
 * heap overflow attacks.
 */
#define AVMPLUS_JIT_READONLY

/**
 * compiled with the /W4 warning level
 * which is quite picky.  Disable warnings we don't care about.
 */
#ifdef _MSC_VER

	#pragma warning(disable:4512) //assignment operator could not be generated
	#pragma warning(disable:4511) //can't generate copy ctor
	#pragma warning(disable:4127) //conditional expression is constant

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
