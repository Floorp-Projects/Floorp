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
 * by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights 
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


#include <windows.h>

#include "MMgc.h"

#ifdef MEMORY_INFO
#include <malloc.h>
#include <strsafe.h>
#include <DbgHelp.h>
#endif

#ifdef MEMORY_INFO

namespace MMgc
{
	// --------------------------------------------------------------------------
	// --------------------------------------------------------------------------
	// --------------------------------------------------------------------------

	// helper snarfed and simplified from main flash player code.
	// since we only need it here and only for debug, I didn't bother
	// migrating the whole thing.
	class DynamicLoadLibraryHelper
	{
	protected:
		DynamicLoadLibraryHelper(const char* p_dllName, bool p_required = true);
		virtual ~DynamicLoadLibraryHelper();
		
		FARPROC GetProc(const char* p_funcName, bool p_required = true);

	public:	
		// note that this is only if any of the *required* ones failed;
		// some "optional" ones may be missing and still have this return true.
		bool AllRequiredItemsPresent() const { return m_allRequiredItemsPresent; }

	private:
		HMODULE	m_lib;
		bool m_allRequiredItemsPresent;
	};

	// --------------------------------------------------------------------------
	// --------------------------------------------------------------------------
	// --------------------------------------------------------------------------

	#define GETPROC(n) do { m_##n = (n##ProcPtr)GetProc(#n); } while (0)
	#define GETPROC_OPTIONAL(n) do { m_##n = (n##ProcPtr)GetProc(#n, false); } while (0)

	// --------------------------------------------------------------------------
	DynamicLoadLibraryHelper::DynamicLoadLibraryHelper(const char* p_dllName, bool p_required) : 
		m_lib(NULL),
		m_allRequiredItemsPresent(true)	// assume the best
	{
		m_lib = ::LoadLibraryA(p_dllName);
		if (p_required && (m_lib == NULL || m_lib == INVALID_HANDLE_VALUE))
		{
			// don't assert here... it will trigger a DebugBreak(), which will crash
			// systems not running a debugger... and QE insists that they be able
			// to run Debug builds on debuggerless Win98 systems... (sigh)
			//GCAssertMsg(0, p_dllName);
			m_allRequiredItemsPresent = false;
		}
	}

	// --------------------------------------------------------------------------
	FARPROC DynamicLoadLibraryHelper::GetProc(const char* p_funcName, bool p_required)
	{
		FARPROC a_proc = NULL;
		if (m_lib != NULL && m_lib != INVALID_HANDLE_VALUE)
		{
			a_proc = ::GetProcAddress(m_lib, p_funcName); 
		}
		if (p_required && a_proc == NULL)
		{
			// don't assert here... it will trigger a DebugBreak(), which will crash
			// systems not running a debugger... and QE insists that they be able
			// to run Debug builds on debuggerless Win98 systems... (sigh)
			//GCAssertMsg(0, p_funcName);
			m_allRequiredItemsPresent = false;
		}
		return a_proc;
	}

	// --------------------------------------------------------------------------
	DynamicLoadLibraryHelper::~DynamicLoadLibraryHelper()
	{
		if (m_lib != NULL && m_lib != INVALID_HANDLE_VALUE)
		{
			::FreeLibrary(m_lib);
		}
	}

	// --------------------------------------------------------------------------
	// --------------------------------------------------------------------------
	// --------------------------------------------------------------------------

	// --------------------------------------------------------------------------
	class DbgHelpDllHelper : public DynamicLoadLibraryHelper
	{
	public:
		DbgHelpDllHelper();
		
	public:

		typedef BOOL (__stdcall *StackWalk64ProcPtr)(
			DWORD                             MachineType,
			HANDLE                            hProcess,
			HANDLE                            hThread,
			LPSTACKFRAME64                    StackFrame,
			PVOID                             ContextRecord,
			PREAD_PROCESS_MEMORY_ROUTINE64    ReadMemoryRoutine,
			PFUNCTION_TABLE_ACCESS_ROUTINE64  FunctionTableAccessRoutine,
			PGET_MODULE_BASE_ROUTINE64        GetModuleBaseRoutine,
			PTRANSLATE_ADDRESS_ROUTINE64      TranslateAddress
			);

		typedef PVOID (__stdcall *SymFunctionTableAccess64ProcPtr)(
			HANDLE  hProcess,
			DWORD64 AddrBase
			);
			
		typedef DWORD64 (__stdcall *SymGetModuleBase64ProcPtr)(
			HANDLE  hProcess,
			DWORD64 qwAddr
			);

		typedef BOOL (__stdcall *SymGetLineFromAddr64ProcPtr)(
			HANDLE                  hProcess,
			DWORD64                 qwAddr,
			PDWORD                  pdwDisplacement,
			PIMAGEHLP_LINE64        Line64
			);

		typedef BOOL (__stdcall *SymGetSymFromAddr64ProcPtr)(
			HANDLE                  hProcess,
			DWORD64                 qwAddr,
			PDWORD64                pdwDisplacement,
			PIMAGEHLP_SYMBOL64      Symbol
			);

		typedef BOOL (__stdcall *SymInitializeProcPtr)(
			HANDLE   hProcess,
			PSTR     UserSearchPath,
			BOOL     fInvadeProcess
			);

	public:
		StackWalk64ProcPtr					m_StackWalk64;
		SymFunctionTableAccess64ProcPtr		m_SymFunctionTableAccess64;
		SymGetModuleBase64ProcPtr			m_SymGetModuleBase64;
		SymGetLineFromAddr64ProcPtr			m_SymGetLineFromAddr64;
		SymGetSymFromAddr64ProcPtr			m_SymGetSymFromAddr64;
		SymInitializeProcPtr				m_SymInitialize;
	};

	// --------------------------------------------------------------------------
	DbgHelpDllHelper::DbgHelpDllHelper() : 
		DynamicLoadLibraryHelper("dbghelp.dll"),
		m_StackWalk64(NULL),
		m_SymFunctionTableAccess64(NULL),
		m_SymGetModuleBase64(NULL),
		m_SymGetLineFromAddr64(NULL),
		m_SymGetSymFromAddr64(NULL),
		m_SymInitialize(NULL)
	{
		GETPROC(StackWalk64);
		GETPROC(SymFunctionTableAccess64);
		GETPROC(SymGetModuleBase64);
		GETPROC(SymGetLineFromAddr64);
		GETPROC(SymGetSymFromAddr64);
		GETPROC(SymInitialize);
	}

	// declaring this statically will dynamically load the dll and procs
	// at startup, and never ever release them... if this ever becomes NON-debug
	// code, you might want to have a way to toss all this... but for _DEBUG
	// only, it should be fine
	static DbgHelpDllHelper g_DbgHelpDll;

}
#endif

namespace MMgc
{
#ifdef MMGC_AVMPLUS
	int GCHeap::vmPageSize()
	{
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);

		return sysinfo.dwPageSize;
	}

	void* GCHeap::ReserveCodeMemory(void* address,
									size_t size)
	{
		return VirtualAlloc(address,
							size,
							MEM_RESERVE,
							PAGE_NOACCESS);
	}

	void GCHeap::ReleaseCodeMemory(void* address,
								   size_t /*size*/)
	{
		VirtualFree(address, 0, MEM_RELEASE);
	}

	void* GCHeap::CommitCodeMemory(void* address,
								  size_t size)
	{
		if (size == 0)
			size = GCHeap::kNativePageSize;  // default of one page

#ifdef AVMPLUS_JIT_READONLY
		void* addr = VirtualAlloc(address,
								  size,
								  MEM_COMMIT,
								  PAGE_READWRITE);
#else
		void* addr = VirtualAlloc(address,
								  size,
								  MEM_COMMIT,
								  PAGE_EXECUTE_READWRITE);
#endif /* AVMPLUS_JIT_READONLY */
		if (addr == NULL)
			address = 0;
		else {
			address = (void*)( (int)address + size );
			committedCodeMemory += size;
		}
		return address;
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
	 *
	 * [rickr] bug #182323  The codegen can bail in the middle of generating 
	 * code for any number of reasons.  When this occurs we need to ensure 
	 * that any code that was previously on the page still executes, so we 
	 * leave the page as PAGE_EXECUTE_READWRITE rather than PAGE_READWRITE.  
	 * Ideally we'd use PAGE_READWRITE and then on failure revert it back to 
	 * read/execute but this is a little tricker and doesn't add too much 
	 * protection since only a single page is 'exposed' with this technique.
	 */
	void GCHeap::SetExecuteBit(void *address,
							   size_t size,
							   bool executableFlag)
	{
		DWORD oldProtectFlags = 0;
		BOOL retval = VirtualProtect(address,
									 size,
									 executableFlag ? PAGE_EXECUTE_READ : PAGE_EXECUTE_READWRITE,
									 &oldProtectFlags);

		(void)retval;
		GCAssert(retval);

		// We should not be clobbering PAGE_GUARD protections
		GCAssert((oldProtectFlags & PAGE_GUARD) == 0);
	}
#endif /* AVMPLUS_JIT_READONLY */
	
	bool GCHeap::SetGuardPage(void *address)
	{
		if (!useGuardPages)
		{
			return false;
		}
		void *res = VirtualAlloc(address,
								 GCHeap::kNativePageSize,
								 MEM_COMMIT,
								 PAGE_GUARD | PAGE_READWRITE);
		return res != 0;
	}
	
	void* GCHeap::DecommitCodeMemory(void* address,
									 size_t size)
	{
		if (size == 0)
			size = GCHeap::kNativePageSize;  // default of one page

		if (VirtualFree(address, size, MEM_DECOMMIT) == false)
			address = 0;	
		else
			committedCodeMemory -= size;

		return address;
	}
#endif

	#ifdef USE_MMAP
	char* GCHeap::ReserveMemory(char *address,
								size_t size)
	{
		return (char*) VirtualAlloc(address,
									size,
									MEM_RESERVE,
									PAGE_NOACCESS);
	}

	bool GCHeap::CommitMemory(char *address,
							  size_t size)
	{
		void *addr = VirtualAlloc(address,
							size,
							MEM_COMMIT,
							PAGE_READWRITE);
#ifdef _DEBUG
		if(addr == NULL) {
			MEMORY_BASIC_INFORMATION mbi;
			VirtualQuery(address, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
			LPVOID lpMsgBuf;
			FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
									NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
									(LPTSTR) &lpMsgBuf, 0, NULL );
			GCAssertMsg(false, (const char*)lpMsgBuf);
		}
#endif
		return addr != NULL;
	}

	bool GCHeap::DecommitMemory(char *address, size_t size)
	{
		return VirtualFree(address, size, MEM_DECOMMIT) != 0;
	}

	void GCHeap::ReleaseMemory(char *address,
							   size_t /*size*/)
	{
		VirtualFree(address, 0, MEM_RELEASE);
	}

	bool GCHeap::CommitMemoryThatMaySpanRegions(char *address, size_t size)
	{
		bool success = false;
		MEMORY_BASIC_INFORMATION mbi;	
		do {
			VirtualQuery(address, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
			size_t commitSize = size > mbi.RegionSize ? mbi.RegionSize : size;
			success = CommitMemory(address, commitSize);
			address += commitSize;
			size -= commitSize;
		} while(size > 0 && success);
		return success;
	}

	bool GCHeap::DecommitMemoryThatMaySpanRegions(char *address, size_t size)
	{
		bool success = false;
		MEMORY_BASIC_INFORMATION mbi;	
		do {
			VirtualQuery(address, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
			size_t commitSize = size > mbi.RegionSize ? mbi.RegionSize : size;
			success = DecommitMemory(address, commitSize);
			address += commitSize;
			size -= commitSize;
		} while(size > 0 && success);
		return success;
	}

	#else
	char* GCHeap::AllocateMemory(size_t size)
	{
		return (char*)_aligned_malloc(size, kBlockSize);
	}

	void GCHeap::ReleaseMemory(char *address)
	{
		_aligned_free(address);
	}	
	#endif

#ifdef MEMORY_INFO
#ifndef MEMORY_INFO

	// empty when MEMORY_INFO not defined
	void GetInfoFromPC(int pc, char *buff, int buffSize) { }
	void GetStackTrace(int *trace, int len, int skip) {}

#else

	void GetStackTrace(int *trace, int len, int skip)
	{
		HANDLE ht = GetCurrentThread();
		HANDLE hp = GetCurrentProcess();

		CONTEXT c;		
		memset( &c, '\0', sizeof c );
		c.ContextFlags = CONTEXT_FULL;

#if 0
		// broken with SP2
		if ( !GetThreadContext( ht, &c ) )
			return;
#else		
      __asm
      {
        call x
        x: pop eax
        mov c.Eip, eax
        mov c.Ebp, ebp
      }
#endif

		// skip an extra frame
		skip++;
		
		STACKFRAME64 frame;
		memset(&frame, 0, sizeof frame);
		
		frame.AddrPC.Offset = c.Eip;
		frame.AddrPC.Mode = AddrModeFlat;
		frame.AddrFrame.Offset = c.Ebp;
		frame.AddrFrame.Mode = AddrModeFlat;

		int i=0;
		// save space for 0 pc terminator
		len--;
		while(i < len && 
			g_DbgHelpDll.m_StackWalk64 != NULL &&
			g_DbgHelpDll.m_SymFunctionTableAccess64 != NULL &&
			g_DbgHelpDll.m_SymGetModuleBase64 != NULL &&
			(*g_DbgHelpDll.m_StackWalk64)(IMAGE_FILE_MACHINE_I386, hp, ht, &frame, 
						NULL, NULL, g_DbgHelpDll.m_SymFunctionTableAccess64, g_DbgHelpDll.m_SymGetModuleBase64, NULL)) {
			if(skip-- > 0)
				continue;
			// FIXME: not 64 bit safe
			trace[i++] = (int) frame.AddrPC.Offset;
		}
		trace[i] = 0;
	}

	static bool inited = false;	
	static const int MaxNameLength = 256;
	void GetInfoFromPC(int pc, char *buff, int buffSize)
	{
		if(!inited) {
			if(!g_DbgHelpDll.m_SymInitialize ||
				!(*g_DbgHelpDll.m_SymInitialize)(GetCurrentProcess(), NULL, true)) {
				LPVOID lpMsgBuf;
				if(FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
								NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
								(LPTSTR) &lpMsgBuf, 0, NULL )) 
				{
					GCDebugMsg("See lpMsgBuf", true);
					LocalFree(lpMsgBuf);
				}			
				goto nosym;
			}
			inited = true;
		}
		
		// gleaned from IMAGEHLP_SYMBOL64 docs
		IMAGEHLP_SYMBOL64 *pSym = (IMAGEHLP_SYMBOL64 *) alloca(sizeof(IMAGEHLP_SYMBOL64) + MaxNameLength);
		pSym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
		pSym->MaxNameLength = MaxNameLength;

		DWORD64 offsetFromSymbol;
		if(!g_DbgHelpDll.m_SymGetSymFromAddr64 ||
			!(*g_DbgHelpDll.m_SymGetSymFromAddr64)(GetCurrentProcess(), pc, &offsetFromSymbol, pSym)) {
			goto nosym;
		}

		// get line
		IMAGEHLP_LINE64 line;
		memset(&line, 0, sizeof(IMAGEHLP_LINE64));
		line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
		DWORD offsetFromLine;
		if(!g_DbgHelpDll.m_SymGetLineFromAddr64 ||
			!(*g_DbgHelpDll.m_SymGetLineFromAddr64)(GetCurrentProcess(), pc, &offsetFromLine, &line)) {
			goto nosym;
		}
		
		/* 
		 this isn't working, I think i need to call SymLoadModule64 or something
		IMAGEHLP_MODULE64 module;
		memset(&module, 0, sizeof module);
		module.SizeOfStruct = sizeof module;
		if(!SymGetModuleInfo64(GetCurrentProcess(), pSym->Address, &module)) 
		{			
			LPVOID lpMsgBuf;
			if(FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
							NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
							(LPTSTR) &lpMsgBuf, 0, NULL )) 
			{
				GCDebugMsg((wchar*)lpMsgBuf, true);
				LocalFree(lpMsgBuf);
			}			
		}
		*/

		// success!
		char *fileName = line.FileName + strlen(line.FileName);

		// skip everything up to last slash
		while(fileName > line.FileName && *fileName != '\\')
			fileName--;
		fileName++;
		StringCchPrintfA(buff, buffSize, "%s:%d", fileName, line.LineNumber);
		return;

nosym:	
		StringCchPrintfA(buff, buffSize, "0x%x", pc);
	}
#endif
#endif // MEMORY_INFO

}
