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
 * Portions created by the Initial Developer are Copyright (C) 1993-2006
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


#ifndef __avmplus_GrowableBuffer_H_
#define __avmplus_GrowableBuffer_H_

#ifdef AVMPLUS_MACH_EXCEPTIONS
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/thread_status.h>
#include <mach/exception.h>
#include <mach/task.h>
#include <pthread.h>
#endif /* AVMPLUS_MACH_EXCEPTIONS */

#ifdef AVMPLUS_LINUX
#include <pthread.h>
#include <stdio.h>
#endif /* AVMPLUS_LINUX */

namespace avmplus
{
	class GrowableBuffer : public MMgc::GCObject
	{
	public:
		GrowableBuffer(MMgc::GCHeap* heap);
		virtual ~GrowableBuffer();

		byte* reserve(size_t amt);
		void  free();
		byte* decommitUnused();
		byte* growBy(size_t amt);
		byte* grow();
		byte* shrinkTo(size_t amt);

		size_t	pageSize()		{ return heap->kNativePageSize; }
		byte*	start()			{ return first; }
		byte*	end()			{ return last; }
		size_t	size()			{ return last-first; }
		byte*	uncommitted()	{ return uncommit; }
		byte*	getPos()		{ return current; }
		void	setPos(byte* c)	{ current = c; /* AvmAssert(c >= start && c <= end); */ }

	private:
		void	init();
		byte*	pageFor(byte* addr)		{ return (byte*) ( (size_t)addr & ~(pageSize()-1) ); }
		byte*	pageAfter(byte* addr)	{ return (byte*) ( (size_t)(addr+pageSize()) & ~(pageSize()-1) ); }

		MMgc::GCHeap* const heap;
		byte* first;		// reservation starting address
		byte* last;			// reservation ending address
		byte* uncommit;		// next uncommitted page in region
		byte* current;		// current position in buffer

		friend class GrowthGuard;
	};

#ifdef FEATURE_BUFFER_GUARD

	// Generic Guard class
	class GenericGuard
	{
	public:
		void enable()	{ registerHandler(); }
		void disable()	{ unregisterHandler(); }

		#ifdef AVMPLUS_MACH_EXCEPTIONS
		static void staticInit();
		static void staticDestroy();
		#endif

		#ifdef AVMPLUS_LINUX
		GenericGuard *next;
		virtual bool handleException(byte*) = 0;
		#endif // AVMPLUS_LINUX

	protected:
		void init();
		void registerHandler();
		void unregisterHandler();
		
		#ifdef AVMPLUS_WIN32
		typedef struct _ExceptionRegistrationRecord
		{
			DWORD prev;
			DWORD handler;
			void* instance;
			DWORD terminator;
		}
		ExceptionRegistrationRecord;

		ExceptionRegistrationRecord record;

		// Platform specific code follows
		static int __cdecl guardRoutine(struct _EXCEPTION_RECORD *exceptionRecord,
										void *establisherFrame,
										struct _CONTEXT *contextRecord,
										void *dispatcherContext);

		virtual int handleException(struct _EXCEPTION_RECORD *exceptionRecord,
									void *establisherFrame,
									struct _CONTEXT *contextRecord,
									void *dispatcherContext) = 0;
        #endif /* AVMPLUS_WIN32 */
		
		#ifdef AVMPLUS_MACH_EXCEPTIONS
		static pthread_mutex_t mutex;
		static pthread_t exceptionThread;
		static mach_port_t exceptionPort;
		static volatile GenericGuard *guardList;		

		mach_port_t thread;
		volatile GenericGuard *next;
		
		const static int kMaxExceptionPorts = 16;
		
		struct SavedExceptionPorts
		{
			mach_msg_type_number_t count;
			exception_mask_t masks[kMaxExceptionPorts];
			exception_handler_t ports[kMaxExceptionPorts];
			exception_behavior_t behaviors[kMaxExceptionPorts];
			thread_state_flavor_t flavors[kMaxExceptionPorts];
		}
		savedExceptionPorts;

		virtual bool handleException(kern_return_t& returnCode) = 0;
		
		// since we have virtual functions, we probably need a virtual dtor
		virtual ~GenericGuard() {}
		
		static kern_return_t catch_exception_raise(mach_port_t exception_port,
												   mach_port_t thread,
												   mach_port_t task,
												   exception_type_t exception,
												   exception_data_t code,
												   mach_msg_type_number_t code_count);

		static kern_return_t forward_exception(mach_port_t thread,
											   mach_port_t task,
											   exception_type_t exception,
											   exception_data_t data,
											   mach_msg_type_number_t data_count,
											   SavedExceptionPorts *savedExceptionPorts);
											   
		static void* threadMain(void *arg);
		
		const static int kThreadExitMsg = 1;

		#ifdef AVMPLUS_ROSETTA
		static bool rosetta;
		#endif
		
		#endif // AVMPLUS_MACH_EXCEPTIONS

		bool registered;		
	};

	// Guards ABC buffer
	class BufferGuard : public GenericGuard
	{
	public:
		#ifdef AVMPLUS_LINUX
		BufferGuard(jmp_buf jmpBuf);
		#else
		BufferGuard(int *jmpBuf);
		#endif // AVMPLUS_LINUX
		virtual ~BufferGuard();

		#ifdef AVMPLUS_LINUX
		virtual bool handleException(byte*);
		#endif // AVMPLUS_LINUX

	protected:
        #ifdef AVMPLUS_WIN32
		virtual int handleException(struct _EXCEPTION_RECORD *exceptionRecord,
									void *establisherFrame,
									struct _CONTEXT *contextRecord,
									void *dispatcherContext);
        #endif /* AVMPLUS_WIN32 */

		#ifdef AVMPLUS_MACH_EXCEPTIONS
		virtual bool handleException(kern_return_t& returnCode);
		#endif
		
	private:
		#ifdef AVMPLUS_LINUX
		jmp_buf jmpBuf;
		#else
		int *jmpBuf;
		#endif // AVMPLUS_LINUX

	};

	// used to expand GrowableBuffer for generated code
	class GrowthGuard : public GenericGuard
	{
	public:
		GrowthGuard(GrowableBuffer* buffer);
		virtual ~GrowthGuard();

		#ifdef AVMPLUS_LINUX
		virtual bool handleException(byte*);
		#endif

	protected:
		#ifdef AVMPLUS_WIN32
		virtual int handleException(struct _EXCEPTION_RECORD *exceptionRecord,
									void *establisherFrame,
									struct _CONTEXT *contextRecord,
									void *dispatcherContext);
		#endif

		#ifdef AVMPLUS_MACH_EXCEPTIONS
		virtual bool handleException(kern_return_t& returnCode);
		#endif
		
	private:
		// pointer to buffer we are guarding
		GrowableBuffer* buffer;
	};
#endif /* FEATURE_BUFFER_GUARD */
}
#endif /*___avmplus_GrowableBuffer_H_*/
