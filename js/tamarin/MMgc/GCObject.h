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

#ifndef __GCObject__
#define __GCObject__


// VC++ wants these declared
//void *operator new(size_t size);
//void *operator new[] (size_t size);

inline void *operator new(size_t size, MMgc::GC *gc, int flags=MMgc::GC::kContainsPointers|MMgc::GC::kZero)
{
	return gc->Alloc(size, flags, 4);
}

namespace MMgc
{
	/**
	 *
	 */
	class GCCustomSizer
	{
	public:
		virtual ~GCCustomSizer() {} // here since gcc complains otherwise
		virtual size_t Size() = 0;
	};

	/**
	 * Baseclass for GC managed objects that aren't finalized
	 */
	class GCObject
	{
	public:
		static void *operator new(size_t size, GC *gc, size_t extra = 0)
#ifdef __GNUC__
			// add this to avoid GCC warning: 'operator new' must not return NULL unless it is declared 'throw()' (or -fcheck-new is in effect)
			throw()
#endif
		{
			// TODO throw exception and shutdown player?
			if (size + extra < size)
			{
				GCAssert(0);
				return 0;
			}
			return gc->Alloc(size + extra, GC::kContainsPointers|GC::kZero, 4);
		}

		static void operator delete (void *gcObject)
		{
			GC::GetGC(gcObject)->Free(gcObject);
		}
		
		GCWeakRef *GetWeakRef() const;
#ifdef MEMORY_INFO
		// give everyone a vtable for rtti type info purposes
		virtual ~GCObject() {}
#endif
	};

	/**
	 *	Baseclass for GC managed objects that are finalized 
	 */
	class GCFinalizedObject
	//: public GCObject can't do this, get weird compile errors in AVM plus, I think it has to do with
	// the most base class (GCObject) not having any virtual methods)
	{
	public:
		virtual ~GCFinalizedObject() {}
		GCWeakRef *GetWeakRef() const;
		static void *operator new(size_t size, GC *gc, size_t extra = 0);
		static void operator delete (void *gcObject);
	};

	/**
	 *	Baseclass for GC managed objects that are finalized 
	 */
	class GCFinalizedObjectOptIn : public GCFinalizedObject
	//: public GCObject can't do this, get weird compile errors in AVM plus, I think it has to do with
	// the most base class (GCObject) not having any virtual methods)
	{
	public:
		static void *operator new(size_t size, GC *gc, size_t extra = 0);
		static void operator delete (void *gcObject);
	};

#ifdef MMGC_DRC
	class RCObject : public GCFinalizedObject
	{
		friend class GC;
	public:
		RCObject()
#ifdef _DEBUG
			: history(0)
#endif
		{
			// composite == 0 is special, it means a deleted object in Release builds
			// b/c RCObjects have a vtable we know composite isn't the first 4 byte and thus
			// won't be trampled by the freelist
			composite = 1;
			GC::GetGC(this)->AddToZCT(this);
		}

		~RCObject()
		{
			// for explicit deletion
			if (InZCT())
				GC::GetGC(this)->RemoveFromZCT(this);
			composite = 0;
		}

		bool IsPinned()
		{
			return (composite & STACK_PIN) != 0;
		}

		void Pin()
		{
#ifdef _DEBUG
			// this is a deleted object but 0xca indicates the InZCT flag so we
			// might erroneously get here for a deleted RCObject
			if(composite == (int32)0xcacacaca || composite == (int32)0xbabababa)
				return;
#endif

			// In Release builds, a deleted object is indicated by
			// composite == 0.  We must not set the STACK_PIN bit
			// on a deleted object, because if we do, it transforms
			// from a deleted object into a zero-ref count live object,
			// causing nasty crashes down the line.
			if (composite == 0)
				return;
			
			composite |= STACK_PIN;
		}

		void Unpin()
		{
			composite &= ~STACK_PIN;
		}

		int InZCT() const { return composite & ZCTFLAG; }
		int RefCount() const { return (composite & RCBITS) - 1; }
		int Sticky() const { return composite & STICKYFLAG; }		
		void Stick() { composite = STICKYFLAG; }
		
		// called by EnqueZCT
		void ClearZCTFlag() 
		{ 
			composite &= ~(ZCTFLAG|ZCT_INDEX);
		}

		void IncrementRef() 
		{
			if(Sticky() || composite == 0)
				return;

#ifdef _DEBUG
			GC* gc = GC::GetGC(this);
			GCAssert(gc->IsRCObject(this));
			GCAssert(this == gc->FindBeginning(this));
			// don't touch swept objects
			if(composite == (int32)0xcacacaca || composite == (int32)0xbabababa)
				return;
#endif

			composite++;
			if((composite&RCBITS) == RCBITS) {
				composite |= STICKYFLAG;
			} else if(InZCT()) {
				GCAssert(RefCount() == 1);
				GC::GetGC(this)->RemoveFromZCT(this);
			}
#ifdef _DEBUG
			if(gc->keepDRCHistory)
				history.Push(GetStackTraceIndex(2));
#endif
		}

		__forceinline void DecrementRef() 
		{ 
			if(Sticky() || composite == 0)
				return;
#ifdef _DEBUG
			GC* gc = GC::GetGC(this);
			GCAssert(gc->IsRCObject(this));
			GCAssert(this == gc->FindBeginning(this));
			// don't touch swept objects
			if(composite == (int32)0xcacacaca || composite == (int32)0xbabababa)
				return;
		
			if(gc->Destroying())
				return;

			if(RefCount() == 0) {
				DumpHistory();
				GCAssert(false);
			}
#endif

			if (RefCount() == 0)
			{
				// This is a defensive measure.  If DecrementRef is
				// ever called on a zero ref-count object, composite--
				// will cause an underflow, flipping all kinds of bits
				// in bad ways and resulting in a crash later.  Often,
				// such a DecrementRef bug would be caught by the
				// _DEBUG asserts above, but sometimes we have
				// release-only crashers like this.  Better to fail
				// gracefully at the point of failure, rather than
				// push the failure to some later point.
				return;
			}
			
			composite--; 

#ifdef _DEBUG
			// the delete flag works around the fact that DecrementRef
			// may be called after ~RCObject since all dtors are called
			// in one pass.  For example a FunctionScriptObject may be
			// the sole reference to a ScopeChain and dec its ref in 
			// ~FunctionScriptObject during a sweep, but since ScopeChain's
			// are smaller the ScopeChain was already finalized, thus the
			// push crashes b/c the history object has been destructed.
			if(gc->keepDRCHistory)
				history.Push(GetStackTraceIndex(1));
#endif
			// composite == 1 is the same as (rc == 1 && !notSticky && !notInZCT)
			if(RefCount() == 0) {
				GC::GetGC(this)->AddToZCT(this);
			}
		}
#ifdef _DEBUG
		void DumpHistory();
#endif

		void setZCTIndex(int index) 
		{
			GCAssert(index >= 0 && index < (ZCT_INDEX>>8));
			GCAssert(index < ZCT_INDEX>>8);
			composite = (composite&~ZCT_INDEX) | ((index<<8)|ZCTFLAG);
		}

		int getZCTIndex() const
		{
			return (composite & ZCT_INDEX) >> 8;
		}
		
		static void *operator new(size_t size, GC *gc, size_t extra = 0)
		{
			return gc->Alloc(size + extra, GC::kContainsPointers|GC::kZero|GC::kRCObject|GC::kFinalize, 4);
		}

	private:
		// 1 bit for inZCT flag (0x80000000)
		// 1 bit for sticky flag (0x40000000)
		// 20 bits for ZCT index
		// 8 bits for RC count (0x000000FF)
		static const int ZCTFLAG	         = 0x80000000;
		static const int STICKYFLAG          = 0x40000000;
		static const int STACK_PIN           = 0x20000000;
		static const int RCBITS		         = 0x000000FF;
		static const int ZCT_INDEX           = 0x0FFFFF00;
#ifdef MMGC_DRC
		int32 composite;
#ifdef _DEBUG
		// addref/decref stack traces
		GCStack<int,4> history;
		int padto32bytes;
#endif
#endif
	};

	class RCFinalizedObject : public RCObject{};
	
	template<class T> 
	class ZeroPtr
	{
	public:
		ZeroPtr() { t = NULL; }
		ZeroPtr(T _t) : t(_t) { }
		~ZeroPtr() 
		{
			t = NULL;
		}
		
		operator T() { return t; }
		bool operator!=(T other) { return other != t; }
		T operator->() const { return t; }
	private:
		T t;
	};

	template<class T> 
	class RCPtr
	{
	public:
		RCPtr() { t = NULL; }
		RCPtr(T _t) : t(_t) { if(t && (uintptr)t != 1) t->IncrementRef(); }
		~RCPtr() 
		{
			if(t && t != (T)1)
				t->DecrementRef();

			// 02may06 grandma : I want to enable 
			//	class DataIOBase { DRC(PlayerToplevel *) const m_toplevel; }
			//
			// DataIOBase is a virtual base class, so we don't know if the
			// subclass is GCObject or not. We need it to be const, or
			// a GCObject would require a DWB(), and if it's const, we
			// cannot zero it out during ~DataIOBase. The simplest solution
			// seemed to be zeroing out the member here.

			t = NULL; 
		}

		T operator=(T tNew)
		{
			if(t && (uintptr)t != 1)
				t->DecrementRef();
			t = tNew;
			if(t && (uintptr)t != 1)
				t->IncrementRef();
			// this cast is safe b/c other wise compilation would fail
			return (T) t;
		}

		operator T() const
		{
			return (T) t;
		}

		operator ZeroPtr<T>() const { return t; }

		bool operator!=(T other) { return other != t; }

		T operator->() const
		{
			return (T) t;
		}

		void Clear() { t = NULL; }

	private:
		T t;
	};


#define DRC(_type) MMgc::RCPtr<_type>

#else // !MMGC_DRC

#define DRC(_type) _type

class RCObject : public GCObject {};
class RCFinalizedObject : public GCFinalizedObject {};

#endif
}


#endif /* __GCObject__ */
