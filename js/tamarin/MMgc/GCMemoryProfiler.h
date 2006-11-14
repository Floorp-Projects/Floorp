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

#ifndef __GCMemoryProfiler__
#define __GCMemoryProfiler__


#ifndef MEMORY_INFO

#define MMGC_MEM_TAG(_x)
#define MMGC_MEM_TYPE(_x)
#define GetRealPointer(_x) _x
#define GetUserPointer(_x) _x
#define DebugSize() 0
#else

#define MMGC_MEM_TAG(_x) MMgc::SetMemTag(_x)
#define MMGC_MEM_TYPE(_x) MMgc::SetMemType(_x)

namespace MMgc
{
#ifdef WIN32
	/**
	 * GCCriticalSection is a simple Critical Section class used by GCMemoryProfiler to
	 * ensure mutually exclusive access.  GCSpinLock doesn't suffice since its not
	 * re-entrant and we need that
	 */
	class GCCriticalSection
	{
	public:
		GCCriticalSection()
		{
			InitializeCriticalSection(&cs);
		}

		inline void Acquire()
		{
			EnterCriticalSection(&cs);
		}
		
		inline void Release()
		{
			LeaveCriticalSection(&cs);
		}

	private:
		CRITICAL_SECTION cs;
	};
	
	template<typename T>
	class GCThreadLocal
	{
	public:
		GCThreadLocal()
		{
			GCAssert(sizeof(T) == sizeof(LPVOID));
			tlsId = TlsAlloc();
		}
		T operator=(T tNew)
		{
			TlsSetValue(tlsId, (LPVOID) tNew);
			return tNew;
		}
		operator T() const
		{
			return (T) TlsGetValue(tlsId);
		}
	private:
		DWORD tlsId;
	};
#else

	// FIXME: implement
	template<typename T>
	class GCThreadLocal
	{
	public:
		GCThreadLocal()
		{			
		}
		T operator=(T tNew)
		{
			tlsId = tNew;
			return tNew;
		}
		operator T() const
		{
			return tlsId;
		}
	private:
		T tlsId;
	};

	class GCCriticalSection
	{
	public:
		GCCriticalSection()
		{
		}

		inline void Acquire()
		{
		}
		
		inline void Release()
		{
		}
	};
#endif

	class GCEnterCriticalSection
	{
	public:
		GCEnterCriticalSection(GCCriticalSection& cs) : m_cs(cs)
		{
			m_cs.Acquire();
		}
		~GCEnterCriticalSection()
		{
			m_cs.Release();
		}

	private:
		GCCriticalSection& m_cs;
	};

	void SetMemTag(const char *memtag);
	void SetMemType(void *memtype);

	/**
	 * calculate a stack trace skipping skip frames and return index into
	 * trace table of stored trace
	 */
	unsigned int GetStackTraceIndex(int skip);
	unsigned int LookupTrace(int *trace);
	void ChangeSize(int traceIndex, int delta);
	void DumpFatties();

	/**
	* Manually set me, for special memory not new/deleted, like the code memory region
	*/
	void ChangeSizeForObject(void *object, int size);

	/**
	* How much extra size does DebugDecorate need?
	*/
	size_t DebugSize();

	/**
	* decorate memory with debug information, return pointer to memory to return to caller
	*/
	void *DebugDecorate(void *item, size_t size, int skip);

	/** 
	* Given a pointer to user memory do debug checks and return pointer to real memory
	*/
	void *DebugFree(void *item, int poison, int skip);		

	/**
	* Given a pointer to real memory do debug checks and return pointer to user memory
	*/
	void *DebugFreeReverse(void *item, int poison, int skip);

	/**
	* Given a user pointer back up to real beginning
	*/
	inline void *GetRealPointer(const void *item) { return (void*)((uintptr) item -  2 * sizeof(int)); }

	/**
	* Given a user pointer back up to real beginning
	*/
	inline void *GetUserPointer(const void *item) { return (void*)((uintptr) item +  2 * sizeof(int)); }

	const char* GetTypeName(int index, void *obj);

	void GetInfoFromPC(int pc, char *buff, int buffSize);
	void GetStackTrace(int *trace, int len, int skip);
	// print stack trace of index into trace table
	void PrintStackTraceByIndex(unsigned int index);
	void PrintStackTrace(const void *item);
	// print stack trace of caller
	void DumpStackTrace();
}

#endif //MEMORY_INFO
#endif //!__GCMemoryProfiler__

