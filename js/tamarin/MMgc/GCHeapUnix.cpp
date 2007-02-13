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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "GCDebug.h"
#include "MMgc.h"
#include "GC.h"

#ifdef USE_MMAP
#include <sys/mman.h>
#endif

#if defined(MEMORY_INFO) && defined(AVMPLUS_LINUX)
#define _GNU_SOURCE
#include <dlfcn.h>
#endif

// avmplus standalone uses UNIX  
#ifdef _MAC
#define MAP_ANONYMOUS MAP_ANON
#endif

namespace MMgc
{
#ifndef USE_MMAP
	void *aligned_malloc(size_t size, size_t align_size, GCMallocFuncPtr m_malloc)
	{
		char *ptr, *ptr2, *aligned_ptr;
		int align_mask = align_size - 1;

		int alloc_size = size + align_size + sizeof(int);
		ptr=(char *)m_malloc(alloc_size);

		if(ptr==NULL) return(NULL);

		ptr2 = ptr + sizeof(int);
		aligned_ptr = ptr2 + (align_size - ((size_t)ptr2 & align_mask));

		ptr2 = aligned_ptr - sizeof(int);
		*((int *)ptr2)=(int)(aligned_ptr - ptr);

		return(aligned_ptr);
	}

	void aligned_free(void *ptr, GCFreeFuncPtr m_free)
	{
		int *ptr2=(int *)ptr - 1;
		char *unaligned_ptr = (char*) ptr - *ptr2;
		m_free(unaligned_ptr);
	}
#endif /* !USE_MMAP */

#ifdef USE_MMAP
	int GCHeap::vmPageSize()
	{
		long v = sysconf(_SC_PAGESIZE);
		return v;
	}

	void* GCHeap::ReserveCodeMemory(void* address,
									size_t size)
	{
		return (char*) mmap(address,
							size,
							PROT_NONE,
							MAP_PRIVATE | MAP_ANONYMOUS,
							-1, 0);
	}

	void GCHeap::ReleaseCodeMemory(void* address,
								   size_t size)
	{
		if (munmap(address, size) != 0)
			GCAssert(false);
	}

	bool GCHeap::SetGuardPage(void *address)
	{
		return false;
	}

#ifdef AVMPLUS_JIT_READONLY
	/**
	 * SetExecuteBit changes the page access protections on a block of pages,
	 * to make JIT-ted code executable or not.
	 *
	 * If executableFlag is true, the memory is made executable and read-only.
	 *
	 * If executableFlag is false, the memory is made non-executable and
	 * read-write.
	 */
	void GCHeap::SetExecuteBit(void *address,
							   size_t size,
							   bool executableFlag)
	{
		// mprotect requires that the addresses be aligned on page boundaries
		void *endAddress = (void*) ((char*)address + size);
		void *beginPage = (void*) ((size_t)address & ~0xFFF);
		void *endPage   = (void*) (((size_t)endAddress + 0xFFF) & ~0xFFF);
		size_t sizePaged = (size_t)endPage - (size_t)beginPage;

		int retval = mprotect(beginPage, sizePaged,
							  executableFlag ? (PROT_READ|PROT_EXEC) : (PROT_READ|PROT_WRITE|PROT_EXEC));

		GCAssert(retval == 0);
	}
#endif /* AVMPLUS_JIT_READONLY */
	
	void* GCHeap::CommitCodeMemory(void* address,
								  size_t size)
	{
		void* res;
		if (size == 0)
			size = GCHeap::kNativePageSize;  // default of one page
			
#ifdef AVMPLUS_JIT_READONLY
		mmap(address,
			 size,
			 PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS,
			 -1, 0);
#else
		mmap(address,
			 size,
			 PROT_READ | PROT_WRITE | PROT_EXEC,
			 MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS,
			 -1, 0);
#endif /* AVMPLUS_JIT_READONLY */
		res = address;
		
		if (res == address)
			address = (void*)( (int)address + size );
		else
			address = 0;
		return address;
	}

	void* GCHeap::DecommitCodeMemory(void* address,
									 size_t size)
	{
		if (size == 0)
			size = GCHeap::kNativePageSize;  // default of one page

		// release and re-reserve it
		ReleaseCodeMemory(address, size);
		address = ReserveCodeMemory(address, size);
		return address;
	}
#else
	int GCHeap::vmPageSize()
	{
		return 4096;
	}

	void* GCHeap::ReserveCodeMemory(void* address,
									size_t size)
	{
		return aligned_malloc(size, 4096, m_malloc);
	}

	void GCHeap::ReleaseCodeMemory(void* address,
								   size_t size)
	{
		aligned_free(address, m_free);
	}

	bool GCHeap::SetGuardPage(void *address)
	{
		return false;
	}
	
	void* GCHeap::CommitCodeMemory(void* address,
								   size_t size)
	{
		return address;
	}	
	void* GCHeap::DecommitCodeMemory(void* address,
									 size_t size)
	{
		return address;
	}	
#endif /* USE_MMAP */	

#ifdef USE_MMAP
	char* GCHeap::ReserveMemory(char *address, size_t size)
	{
		char *addr = (char*)mmap(address,
					 size,
					 PROT_NONE,
					 MAP_PRIVATE | MAP_ANONYMOUS,
					 -1, 0);
		if(address && address != addr) {
			// behave like windows and fail if we didn't get the right address
			ReleaseMemory(addr, size);
			return NULL;
		}
		return addr;
	}

	bool GCHeap::CommitMemory(char *address, size_t size)
	{
		char *addr = (char*)mmap(address,
					size,
					PROT_READ | PROT_WRITE,
			                MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS,
					-1, 0);
		GCAssert(addr == address);
		return addr == address;
	}

	bool GCHeap::DecommitMemory(char *address, size_t size)
	{
		ReleaseMemory(address, size);
		// re-reserve it
		char *addr = (char*)mmap(address,
					 size,
					 PROT_NONE,
					 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
					 -1, 0);
		GCAssert(addr == address);
		return addr == address;
	}

	bool GCHeap::CommitMemoryThatMaySpanRegions(char *address, size_t size)
	{
		return CommitMemory(address, size);
	}

	bool GCHeap::DecommitMemoryThatMaySpanRegions(char *address, size_t size)
	{
		return DecommitMemory(address, size);
	}

	void GCHeap::ReleaseMemory(char *address, size_t size)
	{
		int result = munmap(address, size);
		GCAssert(result == 0);
	}
#else
	char* GCHeap::AllocateMemory(size_t size)
	{
		return (char *) aligned_malloc(size, 4096, m_malloc);
	}

	void GCHeap::ReleaseMemory(char *address)
	{
		aligned_free(address, m_free);
	}	
#endif


#ifdef MEMORY_INFO  
	void GetInfoFromPC(int pc, char *buff, int buffSize) 
	{
#ifdef AVMPLUS_LINUX
		Dl_info dlip;
		dladdr((void *const)pc, &dlip);
		sprintf(buff, "0x%08x:%s", pc, dlip.dli_sname);
#else
		sprintf(buff, "0x%x", pc);
#endif
	}

#ifdef MMGC_PPC
	void GetStackTrace(int *trace, int len, int skip) 
	{
	  register int stackp;
	  int pc;
	  asm("mr %0,r1" : "=r" (stackp));
	  while(skip--) {
	    stackp = *(int*)stackp;
	  }
	  int i=0;
	  // save space for 0 terminator
	  len--;
	  while(i<len && stackp) {
	    pc = *((int*)stackp+2);
	    trace[i++]=pc;
	    stackp = *(int*)stackp;
	  }
	  trace[i] = 0;
	}
#endif

#ifdef MMGC_IA32
	void GetStackTrace(int *trace, int len, int skip)
	{
		void **ebp;
		asm("mov %%ebp, %0" : "=r" (ebp));

		while(skip-- && *ebp)
		{
			ebp = (void**)(*ebp);
		}

		/* save space for 0 terminator */
		len--;

		int i = 0;

		while (i < len && *ebp)
		{
			/* store the current frame pointer */
			trace[i++] = *((int*) ebp + 1);
			/* get the next frame pointer */
			ebp = (void**)(*ebp);
		}

		trace[i] = 0;
	}
#endif

#ifdef MMGC_ARM
	void GetStackTrace(int *trace, int len, int skip) {}
#endif

#endif
}
