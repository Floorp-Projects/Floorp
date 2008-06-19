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
 *                 Andreas Gal <gal@uci.edu>
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

#ifndef avm_h___
#define avm_h___

#include <assert.h>
#include <string.h>
#include "jstypes.h"

namespace avmplus
{
#define FASTCALL

#define AvmAssert(x) assert(x)

    typedef JSUint16 uint16_t;
    typedef JSUint32 uint32_t;
    
    class AvmCore 
    {
    };
    
    class OSDep
    {
    };
    
    class GC 
    {
    };
    
    class GCObject 
    {
    };
    
    /**
     * The List<T> template implements a simple List, which can
     * be templated to support different types.
     * 
     * Elements can be added to the end, modified in the middle, 
     * but no holes are allowed.  That is for set(n, v) to work
     * size() > n
     *
     * Note that [] operators are provided and you can violate the
     * set properties using these operators, if you want a real
     * list dont use the [] operators, if you want a general purpose
     * array use the [] operators.  
     */

    enum ListElementType { LIST_NonGCObjects, LIST_GCObjects };

    template <typename T, ListElementType kElementType>
    class List
    {
    public:
        enum { kInitialCapacity = 128 };        

        List(GC *_gc, uint32_t _capacity=kInitialCapacity) : data(NULL), len(0)
        {
            ensureCapacity(_capacity);
            // this is only b/c of a lot API deficiency, probably would be good to support byte/short lists
            AvmAssert(sizeof(T) >= sizeof(void*));
        }
        
        ~List()
        {
            //clear();
            destroy();
            // zero out in case we are part of an RCObject
            len = 0;
        }

        inline void destroy()
        {
            if (data)
                delete data;
        }
        
        uint32_t FASTCALL add(T value)
        {
            if (len >= capacity()) {
                grow();
            }
            wb(len++, value);
            return len-1;
        }
        
        inline bool isEmpty() const
        {
            return len == 0;
        }
        
        inline uint32_t size() const
        {
            return len;
        }
        
        inline uint32_t capacity() const
        {
            return data ? lot_size(data) / sizeof(T) : 0;
        }
        
        inline T get(uint32_t index) const
        {
            AvmAssert(index < len);
            return *(T*)lot_get(data, factor(index));
        }
        
        void FASTCALL set(uint32_t index, T value)
        {
            AvmAssert(index < capacity());
            if (index >= len)
            {
                len = index+1;
            }
            AvmAssert(len <= capacity());
            wb(index, value);
        }
        
        inline void clear()
        {
            zero_range(0, len);
            len = 0;
        }

        int FASTCALL indexOf(T value) const
        {
            for(uint32_t i=0; i<len; i++)
                if (get(i) == value)
                    return i;
            return -1;
        }
        
        int FASTCALL lastIndexOf(T value) const
        {
            for(int32_t i=len-1; i>=0; i--)
                if (get(i) == value)
                    return i;
            return -1;
        }   
        
        inline T last()
        {
            return get(len-1);
        }
        
        T FASTCALL removeLast()  
        { 
            if(isEmpty())
                return undef_list_val();
            T t = get(len-1);
            set(len-1, undef_list_val());
            len--;
            return t;
        }
    
        inline T operator[](uint32_t index) const
        {
            AvmAssert(index < capacity());
            return get(index);
        }
        
        void FASTCALL ensureCapacity(uint32_t cap)
        {           
            if (cap > capacity()) {
                if (data == NULL) {
                    data = new T[cap];
                    zero_range(0, cap);
                } else {
                    data = (T*)realloc(data, factor(cap));
                    zero_range(capacity(), cap - capacity());
                }
            }
        }
        
        void FASTCALL insert(uint32_t index, T value, uint32_t count = 1)
        {
            AvmAssert(index <= len);
            AvmAssert(count > 0);
            ensureCapacity(len+count);
            memmove(data + index + count, data + index, factor(len - index));
            wbzm(index, index+count, value);
            len += count;
        }

        T FASTCALL removeAt(uint32_t index)
        {
            T old = get(index);
            // dec the refcount on the one we're removing
            wb(index, undef_list_val());
            memmove(data + index, data + index + 1, factor(len - index - 1));
            len--;
            return old;
        }
    
    private:
        void FASTCALL grow()
        {
            // growth is fast at first, then slows at larger list sizes.
            uint32_t newMax = 0;
            const uint32_t curMax = capacity();
            if (curMax == 0)
                newMax = kInitialCapacity;
            else if(curMax > 15)
                newMax = curMax * 3/2;
            else
                newMax = curMax * 2;
        
            ensureCapacity(newMax);
        }
        
        inline void do_wb_nongc(void* /*container*/, T* slot, T value)
        {   
            *slot = value;
        }

        inline void do_wb_gc(void* container, GCObject** slot, const GCObject** value)
        {   
            *slot = *value;
        }

        void FASTCALL wb(uint32_t index, T value)
        {   
            AvmAssert(index < capacity());
            AvmAssert(data != NULL);
            T* slot = data[index];
            switch(kElementType)
            {
                case LIST_NonGCObjects:
                    do_wb_nongc(0, slot, value);
                    break;
                case LIST_GCObjects:
                    do_wb_gc(0, (GCObject**)slot, (const GCObject**)&value);
                    break;
            }
        }

        // multiple wb call with the same value, and assumption that existing value is all zero bits,
        // like
        //  for (uint32_t u = index; u < index_end; ++u)
        //      wb(u, value);
        void FASTCALL wbzm(uint32_t index, uint32_t index_end, T value)
        {   
            AvmAssert(index < capacity());
            AvmAssert(index_end <= capacity());
            AvmAssert(index < index_end);
            AvmAssert(data != NULL);
            void *container;
            T* slot = data + index;
            switch(kElementType)
            {
                case LIST_NonGCObjects:
                    for (  ; index < index_end; ++index, ++slot)
                        do_wb_nongc(container, slot, value);
                    break;
                case LIST_GCObjects:
                    for (  ; index < index_end; ++index, ++slot)
                        do_wb_gc(container, (GCObject**)slot, (const GCObject**)&value);
                    break;
            }
        }
        
        inline uint32_t factor(uint32_t index) const
        {
            return index * sizeof(T);
        }

        void FASTCALL zero_range(uint32_t _first, uint32_t _count)
        {
            memset(data + _first, 0, factor(_count));
        }
        
        // stuff that needs specialization based on the type
        static inline T undef_list_val();

    private:
        List(const List& toCopy);           // unimplemented
        void operator=(const List& that);   // unimplemented

    // ------------------------ DATA SECTION BEGIN
    private:
        T* data;
        uint32_t len;
    // ------------------------ DATA SECTION END

    };

    // stuff that needs specialization based on the type
    template<typename T, ListElementType kElementType> 
    /* static */ inline T List<T, kElementType>::undef_list_val() { return T(0); }

    /**
     * The SortedMap<K,T> template implements an object that
     * maps keys to values.   The keys are sorted
     * from smallest to largest in the map. Time of operations 
     * is as follows: 
     *   put() is O(1) if the key is higher than any existing 
     *         key; O(logN) if the key already exists,
     *         and O(N) otherwise. 
     *   get() is an O(logN) binary search.
     * 
     * no duplicates are allowed.
     */
    template <class K, class T, ListElementType valType>
    class SortedMap 
    {
    public:
        enum { kInitialCapacity= 64 };
        
        SortedMap(GC* gc, int _capacity=kInitialCapacity)
          : keys(gc, _capacity), values(gc, _capacity)
        {
        }

        bool isEmpty() const
        {
            return keys.size() == 0;
        }
        
        int size() const
        {
            return keys.size();
        }
        
        void clear()
        {
            keys.clear();
            values.clear();
        }
        
        void destroy()
        {
            keys.destroy();
            values.destroy();
        }
        
        T put(K k, T v)
        {
            if (keys.size() == 0 || k > keys.last()) 
            {
                keys.add(k);
                values.add(v);
                return (T)v;
            }
            else
            {
                int i = find(k);        
                if (i >= 0)
                {
                    T old = values[i];
                    keys.set(i, k);
                    values.set(i, v);
                    return old;
                }
                else
                {
                    i = -i - 1; // recover the insertion point
                    AvmAssert(keys.size() != (uint32_t)i);
                    keys.insert(i, k);
                    values.insert(i, v);
                    return v;
                }
            }
        }
        
        T get(K k) const
        {
            int i = find(k);
            return i >= 0 ? values[i] : 0;
        }
        
        bool get(K k, T& v) const
        {
            int i = find(k);
            if (i >= 0)
            {
                v = values[i];
                return true;
            }
            return false;
        }
        
        bool containsKey(K k) const
        {
            int i = find(k);
            return (i >= 0) ? true : false;
        }
        
        T remove(K k)
        {
            int i = find(k);
            return removeAt(i);
        }
        
        T removeAt(int i)
        {
            T old = values.removeAt(i);
            keys.removeAt(i);
            return old;
        }

        T removeFirst() { return isEmpty() ? (T)0 : removeAt(0); }
        T removeLast()  { return isEmpty() ? (T)0 : removeAt(keys.size()-1); }
        T first() const { return isEmpty() ? (T)0 : values[0]; }
        T last()  const { return isEmpty() ? (T)0 : values[keys.size()-1]; }

        K firstKey() const  { return isEmpty() ? 0 : keys[0]; }
        K lastKey() const   { return isEmpty() ? 0 : keys[keys.size()-1]; }

        // iterator 
        T   at(int i) const { return values[i]; }
        K   keyAt(int i) const { return keys[i]; }

        int findNear(K k) const {
            int i = find(k);
            return i >= 0 ? i : -i-2;
        }
    protected:
        List<K, LIST_NonGCObjects> keys;
        List<T, valType> values;
        
        int find(K k) const
        {
            int lo = 0;
            int hi = keys.size()-1;

            while (lo <= hi)
            {
                int i = (lo + hi)/2;
                K m = keys[i];
                if (k > m)
                    lo = i + 1;
                else if (k < m)
                    hi = i - 1;
                else
                    return i; // key found
            }
            return -(lo + 1);  // key not found, low is the insertion point
        }
    };
    
    /**
     * Bit vectors are an efficent method of keeping True/False information 
     * on a set of items or conditions. Class BitSet provides functions 
     * to manipulate individual bits in the vector.
     *
     * Since most vectors are rather small an array of longs is used by
     * default to house the value of the bits.  If more bits are needed
     * then an array is allocated dynamically outside of this object. 
     * 
     * This object is not optimized for a fixed sized bit vector
     * it instead allows for dynamically growing the bit vector.
     */ 
    class BitSet
    {
        public:
            enum {  kUnit = 8*sizeof(long),
                    kDefaultCapacity = 4   };

            BitSet()
            {
                capacity = kDefaultCapacity;
                reset();
            }
            
            ~BitSet()
            {
                if (capacity > kDefaultCapacity)
                    delete bits.ptr;
            }

            void reset()
            {
                if (capacity > kDefaultCapacity)
                    for(int i=0; i<capacity; i++)
                        bits.ptr[i] = 0;
                else
                    for(int i=0; i<capacity; i++)
                        bits.ar[i] = 0;
            }

            void set(GC *gc, int bitNbr)
            {
                int index = bitNbr / kUnit;
                int bit = bitNbr % kUnit;
                if (index >= capacity)
                    grow(gc, index+1);

                if (capacity > kDefaultCapacity)
                    bits.ptr[index] |= (1<<bit);
                else
                    bits.ar[index] |= (1<<bit);
            }

            void clear(int bitNbr)
            {
                int index = bitNbr / kUnit;
                int bit = bitNbr % kUnit;
                if (index < capacity)
                {
                    if (capacity > kDefaultCapacity)
                        bits.ptr[index] &= ~(1<<bit);
                    else
                        bits.ar[index] &= ~(1<<bit);
                }
            }

            bool get(int bitNbr) const
            {
                int index = bitNbr / kUnit;
                int bit = bitNbr % kUnit;
                bool value = false;
                if (index < capacity)
                {
                    if (capacity > kDefaultCapacity)
                        value = ( bits.ptr[index] & (1<<bit) ) ? true : false;
                    else
                        value = ( bits.ar[index] & (1<<bit) ) ? true : false;
                }
                return value;
            }

        private:
            // Grow the array until at least newCapacity big
            void grow(GC *gc, int newCapacity)
            {
                // create vector that is 2x bigger than requested 
                newCapacity *= 2;
                //MEMTAG("BitVector::Grow - long[]");
                long* newBits = new long[newCapacity * sizeof(long)];
                memset(newBits, 0, newCapacity * sizeof(long));

                // copy the old one 
                if (capacity > kDefaultCapacity)
                    for(int i=0; i<capacity; i++)
                        newBits[i] = bits.ptr[i];
                else
                    for(int i=0; i<capacity; i++)
                        newBits[i] = bits.ar[i];

                // in with the new out with the old
                if (capacity > kDefaultCapacity)
                    delete bits.ptr;

                bits.ptr = newBits;
                capacity = newCapacity;
            }

            // by default we use the array, but if the vector 
            // size grows beyond kDefaultCapacity we allocate
            // space dynamically.
            int capacity;
            union
            {
                long ar[kDefaultCapacity];
                long*  ptr;
            }
            bits;
    };
}

#endif
