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

//
// GCWriteBarrier
//

#ifndef _WRITE_BARRIER_H_
#define _WRITE_BARRIER_H_

#ifdef WRITE_BARRIERS

// inline write barrier
#define WB(gc, container, addr, value) gc->writeBarrier(container, addr, (const void *) (value))

// fast manual RC write barrier
#define WBRC(gc, container, addr, value) gc->writeBarrierRC(container, addr, (const void *) (value))

// declare write barrier
#define DWB(type) MMgc::WriteBarrier<type>

// declare an optimized RCObject write barrier
#define DRCWB(type) MMgc::WriteBarrierRC<type>

#else

#define WB(gc, container, addr, value) *addr = value

#define WBRC(gc, container, addr, value) *addr = value

// declare write barrier
#define DWB(type) type

#define DVWB(type) type

#endif

namespace MMgc
{
	/**
	 * WB is a smart pointer write barrier meant to be used on any field of a GC object that
	 * may point to another GC object.  A write barrier may only be avoided if if the field is
	 * const and no allocations occur between the construction of the object holding the field 
	 * and the assignment. 
	 */
	template<class T> class WriteBarrier
	{
	public:
		WriteBarrier() {}
		WriteBarrier(T _t)
		{ 
			set(_t);
		}

		~WriteBarrier() 
		{ 
			t = 0;
		}

		T operator=(const WriteBarrier<T>& wb)
		{
			return set(wb.t);	
		}

		T operator=(T tNew)
		{
			return set(tNew);
		}

		// BEHOLD ... The weird power of C++ operator overloading
		operator T() const { return t; }

#ifdef MMGC_DRC
		operator ZeroPtr<T>() const { return t; }
#endif

		bool operator!=(T other) const { return other != t; }

		T operator->() const
		{
			return t;
		}
	private:

		// private to prevent its use and someone adding it, GCC creates
		// WriteBarrier's on the stack with it
		WriteBarrier(const WriteBarrier<T>& toCopy) { GCAssert(false); }
		
		T set(const T tNew)
		{
			if(t != tNew || tNew != 0)
				GC::WriteBarrier(this, (const void*)tNew);
			else
				t = tNew;
			return tNew;
		}
		T t;
	};

	/**
	 * WriteBarrierRC is a write barrier for naked (not pointer swizzled) RC objects.
	 * the only thing going in and out of the slot is NULL or a valid RCObject
	 */
	template<class T> class WriteBarrierRC
	{
	public:
		WriteBarrierRC() {}
		WriteBarrierRC(T _t)
		{ 
			set(_t);
		}

		~WriteBarrierRC() 
		{
			if(t != 0) {
				((RCObject*)t)->DecrementRef();
				t=0;
			}
		}

		T operator=(const WriteBarrierRC<T>& wb)
		{
			return set(wb.t);	
		}

		T operator=(T tNew)
		{
			return set(tNew);
		}

		operator T() const { return t; }

#ifdef MMGC_DRC
		operator ZeroPtr<T>() const { return t; }
#endif

		bool operator!=(T other) const { return other != t; }

		T operator->() const
		{
			return t;
		}

		void Clear() { t = 0; }
	private:

		// see note for WriteBarrier
		WriteBarrierRC(const WriteBarrierRC<T>& toCopy);
		
		T set(const T tNew)
		{
			GC *gc = GC::GetGC(this);
			gc->writeBarrierRC(gc->FindBeginning(this), this, (const void*)tNew);
			return tNew;
		}
		T t;
	};
}

#endif // _WRITE_BARRIER_H_
