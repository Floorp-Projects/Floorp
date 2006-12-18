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

#ifndef __avmplus_SortedIntMap__
#define __avmplus_SortedIntMap__


namespace avmplus
{
	/**
	 * The SortedIntMap<T> template implements an object that
	 * maps integer keys to values.	  The keys are sorted
	 * from smallest to largest in the map. Time of operations 
	 * is as follows: 
 	 *   put() is O(1) if the key is higher than any existing 
	 *         key; O(logN) if the key already exists,
     *         and O(N) otherwise. 
	 *   get() is an O(logN) binary search.
	 * 
	 * no duplicates are allowed.
	 */
	template <class T>
	class SortedIntMap : public MMgc::GCObject
	{
	public:
		enum { kInitialCapacity= 64 };
		
		SortedIntMap(int _capacity=kInitialCapacity)
		{
			init(0, _capacity, false, 0);
		}
		SortedIntMap(MMgc::GC* _gc, uint32 _capacity=kInitialCapacity, bool _livesInGCContainer=true, int _flags=MMgc::GC::kContainsPointers)
		{
			init(_gc, _capacity, _livesInGCContainer, _flags);
		}
		void init(MMgc::GC* _gc, uint32 _capacity, bool _livesInGCContainer, int _flags)
		{
			len = 0;
			max = 0;
			keys = NULL;
			values = NULL;
			this->gc = _gc;
			this->gcFlags = _flags;
			this->livesInGCContainer = _livesInGCContainer;
			ensureCapacity(_capacity);
		}
		~SortedIntMap()
		{
			if(gc)
			{
				gc->Free(keys);
				gc->Free(values);
			}
			else
			{
				delete [] keys;
				delete [] values;
			}
			max = 0;
			len = 0;
		}
		bool isEmpty() const
		{
			return len == 0;
		}
		int size() const
		{
			return len;
		}
		int capacity() const
		{
			return max;
		}
		void clear()
		{
			len = 0;
		}
		T put(sintptr k, T v)
		{
			if (len == 0 || k > keys[len-1])
			{
				if (len == max)
					grow();
				set(len, k, v);
				len++;
				return (T)v;
			}
			else
			{
				int i = find(k);		
				if (i >= 0)
				{
					T old = values[i];
					set(i, k, v);
					return old;
				}
				else
				{
					i = -i - 1; // recover the insertion point
					if (len == max)
						grow();
					arraycopy(keys,i,keys,i+1,len-i);
					arraycopy(values,i,values,i+1,len-i);
					set(i, k, v);
					len++;
					return (T)v;
				}
			}
		}
		T get(sintptr k) const
		{
			int i = find(k);
	        return i >= 0 ? values[i] : 0;
		}
		bool containsKey(sintptr k) const
		{
			int i = find(k);
			return (i >= 0) ? true : false;
		}
		T remove(sintptr k)
		{
			int i = find(k);
			return removeAt(i);
		}
		T removeAt(int i)
		{
			T old = (T)0;
			if (i >= 0)
			{
				old = values[i];
				arraycopy(keys, i+1, keys, i, len-i-1);
				arraycopy(values, i+1, values, i, len-i-1);
				len--;
			}
			return old;
		}

		T removeFirst() { return isEmpty() ? (T)0 : removeAt(0); }
		T removeLast()  { return isEmpty() ? (T)0 : removeAt(len-1); }
		T first() const { return isEmpty() ? (T)0 : values[0]; }
		T last()  const { return isEmpty() ? (T)0 : values[len-1]; }

		sintptr firstKey() const	{ return isEmpty() ? 0 : keys[0]; }
		sintptr lastKey() const		{ return isEmpty() ? 0 : keys[len-1]; }

		// you need to allocate the space for these
		int keyArray(sintptr* arr) { arraycopy(keys, 0, arr, 0, len); return len; }
		int valueArray(T* arr) { arraycopy(values, 0, arr, 0, len); return len; }

		// iterator 
		T	at(int i) const { return values[i]; }
		sintptr keyAt(int i) const { return keys[i]; }

	protected:
		T *values;
		sintptr *keys;
		int len;
		int max;

		MMgc::GC* gc;
		int reserved:28;
		int gcFlags:3;
		int livesInGCContainer:1;

		void set(int index, sintptr k, T v)
		{
			keys[index] = k;
			if (gc)
			{
				AvmAssert(sizeof(T) <= sizeof(int*)); // WB requries this...
				WB(gc, values, &values[index], v);
			}
			else
			{
				values[index] = v;
			}
		}
		int find(sintptr k) const
		{
			int lo = 0;
			int hi = len-1;

			while (lo <= hi)
			{
				int i = (lo + hi)/2;
				sintptr m = keys[i];
				if (k > m)
					lo = i + 1;
				else if (k < m)
					hi = i - 1;
				else
					return i; // key found
			}
			return -(lo + 1);  // key not found, low is the insertion point
		}
		void grow()
		{
			ensureCapacity(2*max);
		}
		void ensureCapacity(int32 cap)
		{			
			if(cap > max) 
			{
				T* newvalues = (gc) ? (T*) gc->Calloc(cap, sizeof(T), gcFlags) : new T[cap];
				sintptr* newkeys = (gc) ? (sintptr*) gc->Calloc(cap, sizeof(sintptr), 0) : new sintptr[cap];
				arraycopy(keys, 0, newkeys, 0 , len);
				arraycopy(values, 0, newvalues, 0 , len);
				if (gc)
				{
					gc->Free(keys);
					gc->Free(values);
				}
				else
				{
					delete [] keys;
					delete [] values;
				}

				if (gc && livesInGCContainer) 
				{
					WB(gc, gc->FindBeginning(this), &keys, newkeys);
					WB(gc, gc->FindBeginning(this), &values, newvalues);
				} 
				else 
				{
					keys = newkeys;
					values = newvalues;
				}
				max = cap;
			}
		}
		void arraycopy(sintptr* src, int srcStart, sintptr* dst, int dstStart, int nbr)
		{
			// we have 2 cases, either closing a gap or opening it.
			if ((src == dst) && (srcStart > dstStart) )
			{
				for(int i=0; i<nbr; i++)
					dst[i+dstStart] = src[i+srcStart];	
			}
			else
			{
				for(int i=nbr-1; i>=0; i--)
					dst[i+dstStart] = src[i+srcStart];
			}
		}
		void arraycopy(T* src, int srcStart, T* dst, int dstStart, int nbr)
		{
			// we have 2 cases, either closing a gap or opening it.
			if ((src == dst) && (srcStart > dstStart) )
			{
				for(int i=0; i<nbr; i++)
					dst[i+dstStart] = src[i+srcStart];	
			}
			else
			{
				for(int i=nbr-1; i>=0; i--)
					dst[i+dstStart] = src[i+srcStart];
			}
		}
	};
}

#endif /* __avmplus_SortedIntMap__ */
