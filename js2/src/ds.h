/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

#ifndef ds_h___
#define ds_h___

#include <memory>

#include "utilities.h"

namespace JavaScript 
{

//
// Save-Restore Pattern
//

    // Use the definition
    //   SaveRestore<T> temp(var)
    // to save the current value of var at the time of the definition into a temporary temp
    // and restore var to the saved value at the end of temp's scope, regardless of whether
    // temp goes out of scope due to normal execution or due to a thrown exception.
    template<typename T> class SaveRestore {
        const T savedValue;
        T &var;

      public:
        SaveRestore(T &t): savedValue(t), var(t) {}
        ~SaveRestore() {var = savedValue;}
    };


//
// Doubly Linked Lists
//

    // A ListQueue provides insert and delete operations on a doubly-linked list of
    // objects threaded through fields named 'next' and 'prev'.  The type parameter
    // E must be a class derived from ListQueueEntry.
    // The ListQueue does not own its elements.  They must be deleted explicitly if
    // needed.
    struct ListQueueEntry {
        ListQueueEntry *next;                   // Next entry in linked list
        ListQueueEntry *prev;                   // Previous entry in linked list
        
#ifdef DEBUG
        ListQueueEntry(): next(0), prev(0) {}
#endif
    };
    
    template <class E>
    struct ListQueue: private ListQueueEntry {
        
        ListQueue() {next = this; prev = this;}
        
        // Return true if the ListQueue is nonempty.
        operator bool() const {return next != static_cast<const ListQueueEntry *>(this);}

        // Return true if the ListQueue is empty.
        bool operator !() const {return next == static_cast<const ListQueueEntry *>(this);}

        E &front() const {ASSERT(operator bool()); return *static_cast<E *>(next);}
        E &back() const {ASSERT(operator bool()); return *static_cast<E *>(prev);}

        void push_front(E &elt) {
            ASSERT(!elt.next && !elt.prev);
            elt.next = next; elt.prev = this; next->prev = &elt; next = &elt;
        }
        
        void push_back(E &elt) {
            ASSERT(!elt.next && !elt.prev);
            elt.next = this; elt.prev = prev; prev->next = &elt; prev = &elt;
        }

        E &pop_front() {
            ASSERT(operator bool());
            E *elt = static_cast<E *>(next); next = elt->next; next->prev = this;
            DEBUG_ONLY(elt->next = 0; elt->prev = 0;) return *elt;
        }
        
        E &pop_back() {
            ASSERT(operator bool());
            E *elt = static_cast<E *>(prev); prev = elt->prev; prev->next = this;
            DEBUG_ONLY(elt->next = 0; elt->prev = 0;);
            return *elt;
        }
    };


//
// Growable Arrays
//

    // A Buffer initially points to inline storage of initialSize elements of type T.
    // The Buffer can be expanded via the expand method to increase its size by
    // allocating storage from the heap.
    template <typename T, size_t initialSize> class Buffer {
      public:
        T *buffer;                      // Pointer to the current buffer
        size_t size;                    // Current size of the buffer
      private:
        T initialBuffer[initialSize];   // Initial buffer
      public:
        Buffer(): buffer(initialBuffer), size(initialSize) {}
        ~Buffer() {if (buffer != initialBuffer) delete[] buffer;}
        
        void expand(size_t newSize);
    };


    // Expand the buffer to size newSize, which must be greater than the current
    // size. The buffer's contents are not preserved.
    template <typename T, size_t initialSize>
    inline void Buffer<T, initialSize>::expand(size_t newSize) {
        ASSERT(newSize > size);
        if (buffer != initialBuffer) {
            delete[] buffer;
            buffer = 0;     // For exception safety if the allocation below fails.
        }
        buffer = new T[newSize];
        size = newSize;
    }


    // See ArrayBuffer below.
    template <typename T> class RawArrayBuffer {
        T *const cache;                 // Pointer to a fixed-size cache for holding the buffer if it's small enough
      protected:
        T *buffer;                      // Pointer to the current buffer
        size_t length;                  // Logical size of the buffer
        size_t bufferSize;              // Physical size of the buffer
#ifdef DEBUG
        size_t maxReservedSize;         // Maximum size reserved so far
#endif

      public:
        RawArrayBuffer(T *cache, size_t cacheSize) :
            cache(cache), buffer(cache), length(0), bufferSize(cacheSize) {
            DEBUG_ONLY(maxReservedSize = 0);
        }
      private:
        RawArrayBuffer(const RawArrayBuffer&);      // No copy constructor
        void operator=(const RawArrayBuffer&);      // No assignment operator
      public:
        ~RawArrayBuffer() {if (buffer != cache) delete[] buffer;}
        
      private:
        void enlarge(size_t newLength);
      public:
        // Methods that do not expand the buffer cannot throw exceptions.
        size_t size() const {return length;}
        operator bool() const {return length != 0;}
        bool operator !() const {return length == 0;}
        
        T &front() {ASSERT(length); return *buffer;}
        const T &front() const {ASSERT(length); return *buffer;}
        T &back() {ASSERT(length); return buffer[length-1];}
        const T &back() const {ASSERT(length); return buffer[length-1];}
        T *contents() const {return buffer;}
        
        void reserve(size_t nElts);
        T *reserve_back(size_t nElts = 1);
        T *advance_back(size_t nElts = 1);
        T *reserve_advance_back(size_t nElts = 1);
        
        void fast_push_back(const T &elt);
        void push_back(const T &elt);
        void append(const T *elts, size_t nElts);
        void append(const T *begin, const T *end) {ASSERT(end >= begin); append(begin, toSize_t(end - begin));}
        
        T &pop_back() {ASSERT(length); return buffer[--length];}
    };


    // Enlarge the buffer so that it can hold at least newLength elements.
    // May throw an exception, in which case the buffer is left unchanged.
    template <typename T>
    void RawArrayBuffer<T>::enlarge(size_t newLength) {
        size_t newBufferSize = bufferSize * 2;
        if (newBufferSize < newLength)
            newBufferSize = newLength;
        
        auto_ptr<T> newBuffer(new T[newBufferSize]);
        T *oldBuffer = buffer;
        std::copy(oldBuffer, oldBuffer + length, newBuffer.get());
        buffer = newBuffer.release();
        if (oldBuffer != cache)
            delete[] oldBuffer;
        bufferSize = newBufferSize;
    }

    // Ensure that there is room to hold nElts elements in the buffer, without
    // expanding the buffer's logical length.
    // May throw an exception, in which case the buffer is left unchanged.
    template <typename T>
    inline void RawArrayBuffer<T>::reserve(size_t nElts) {
        if (bufferSize < nElts)
            enlarge(nElts);
#ifdef DEBUG
        if (maxReservedSize < nElts)
            maxReservedSize = nElts;
#endif
    }

    // Ensure that there is room to hold nElts more elements in the buffer, without
    // expanding the buffer's logical length.  Return a pointer to the first element
    // just past the logical length.
    // May throw an exception, in which case the buffer is left unchanged.
    template <typename T>
    inline T *RawArrayBuffer<T>::reserve_back(size_t nElts) {
        reserve(length + nElts);
        return buffer[length];
    }

    // Advance the logical length by nElts, assuming that the memory has previously
    // been reserved.
    // Return a pointer to the first new element.
    template <typename T>
    inline T *RawArrayBuffer<T>::advance_back(size_t nElts) {
        ASSERT(length + nElts <= maxReservedSize);
        T *p = buffer + length;
        length += nElts;
        return p;
    }

    // Combine the effects of reserve_back and advance_back.
    template <typename T>
    inline T *RawArrayBuffer<T>::reserve_advance_back(size_t nElts) {
        reserve(length + nElts);
        T *p = buffer + length;
        length += nElts;
        return p;
    }

    // Same as push_back but assumes that the memory has previously been reserved.
    // May throw an exception if copying elt throws one, in which case the buffer is
    // left unchanged.
    template <typename T>
    inline void RawArrayBuffer<T>::fast_push_back(const T &elt) {
        ASSERT(length < maxReservedSize);
        buffer[length] = elt;
        ++length;
    }

    // Append elt to the back of the buffer.
    // May throw an exception, in which case the buffer is left unchanged.
    template <typename T>
    inline void RawArrayBuffer<T>::push_back(const T &elt) {
        *reserve_back() = elt;
        ++length;
    }

    // Append nElts elements elts to the back of the array buffer.
    // May throw an exception, in which case the buffer is left unchanged.
    template <typename T>
    void RawArrayBuffer<T>::append(const T *elts, size_t nElts) {
        size_t newLength = length + nElts;
        if (newLength > bufferSize)
            enlarge(newLength);
        std::copy(elts, elts + nElts, buffer + length);
        length = newLength;
    }


    // An ArrayBuffer represents an array of elements of type T.  The ArrayBuffer
    // contains storage for a fixed size array of cacheSize elements; if this size
    // is exceeded, the ArrayBuffer allocates the array from the heap.  Elements can
    // be appended to the back of the array using append.  An ArrayBuffer can also
    // act as a stack: elements can be pushed and popped from the back.
    //
    // All ArrayBuffer operations are atomic with respect to exceptions -- either
    // they succeed or they do not affect the ArrayBuffer's existing elements and
    // length. If T has a constructor, it must have a constructor with no arguments;
    // that constructor is called at the time memory for the ArrayBuffer is
    // allocated, just like when allocating a regular C++ array.
    template <typename T, size_t cacheSize>
    class ArrayBuffer: public RawArrayBuffer<T> {
        T cacheArray[cacheSize];
      public:
        ArrayBuffer(): RawArrayBuffer<T>(cacheArray, cacheSize) {}
    };


//
// Bit Sets
//

    template<size_t size> class BitSet {
        STATIC_CONST(size_t, nWords = (size+31)>>5);
        STATIC_CONST(uint32, lastWordMask = (2u<<((size-1)&31)) - 1);

        uint32 words[nWords];   // Bitmap; the first word contains bits 0(LSB)...31(MSB), the second contains bits 32...63, etc.

      public:
        void clear() {zero(words, words+nWords);}
        BitSet() {clear();}

        // Construct a BitSet out of an array of alternating low (inclusive)
        // and high (exclusive) ends of ranges of set bits.
        // The array is terminated by a 0,0 range.
        template<typename In> explicit BitSet(const In *a) {
            clear();
            size_t low, high;
            while (low = *a++, (high = *a++) != 0) setRange(low, high);
        }
                
        bool operator[](size_t i) const {ASSERT(i < size); return static_cast<bool>(words[i>>5]>>(i&31) & 1);}
        bool none() const;
        bool operator==(const BitSet &s) const;
        bool operator!=(const BitSet &s) const;
        
        void set(size_t i) {ASSERT(i < size); words[i>>5] |= 1u<<(i&31);}
        void reset(size_t i) {ASSERT(i < size); words[i>>5] &= ~(1u<<(i&31));}
        void flip(size_t i) {ASSERT(i < size); words[i>>5] ^= 1u<<(i&31);}
        void setRange(size_t low, size_t high);
        void resetRange(size_t low, size_t high);
        void flipRange(size_t low, size_t high);
    };


    // Return true if all bits are clear.
    template<size_t size>
    inline bool BitSet<size>::none() const {
        if (nWords == 1)
            return !words[0];
        else {
            const uint32 *w = words;
            while (w != words + nWords)
                if (*w++)
                    return false;
            return true;
        }
    }

    // Return true if the BitSets are equal.
    template<size_t size>
    inline bool BitSet<size>::operator==(const BitSet &s) const {
        if (nWords == 1)
            return words[0] == s.words[0];
        else
            return std::equal(words, s.words);
    }

    // Return true if the BitSets are not equal.
    template<size_t size>
    inline bool BitSet<size>::operator!=(const BitSet &s) const {
        return !operator==(s);
    }

    // Set all bits between low inclusive and high exclusive.
    template<size_t size>
    void BitSet<size>::setRange(size_t low, size_t high) {
        ASSERT(low <= high && high <= size);
        if (low != high)
            if (nWords == 1)
                words[0] |= (2u<<(high-1)) - (1u<<low);
            else {
                --high;
                uint32 *w = words + (low>>5);
                uint32 *wHigh = words + (high>>5);
                uint32 l = 1u << (low&31);
                uint32 h = 2u << (high&31);
                if (w == wHigh)
                    *w |= h - l;
                else {
                    *w++ |= -l;
                    while (w != wHigh)
                        *w++ = static_cast<uint32>(-1);
                    *w |= h - 1;
                }
            }
    }

    // Clear all bits between low inclusive and high exclusive.
    template<size_t size>
    void BitSet<size>::resetRange(size_t low, size_t high) {
        ASSERT(low <= high && high <= size);
        if (low != high)
            if (nWords == 1)
                words[0] &= (1u<<low) - 1 - (2u<<(high-1));
            else {
                --high;
                uint32 *w = words + (low>>5);
                uint32 *wHigh = words + (high>>5);
                uint32 l = 1u << (low&31);
                uint32 h = 2u << (high&31);
                if (w == wHigh)
                    *w &= l - 1 - h;
                else {
                    *w++ &= l - 1;
                    while (w != wHigh)
                        *w++ = 0;
                    *w &= -h;
                }
            }
    }

    // Invert all bits between low inclusive and high exclusive.
    template<size_t size>
    void BitSet<size>::flipRange(size_t low, size_t high) {
        ASSERT(low <= high && high <= size);
        if (low != high)
            if (nWords == 1)
                words[0] ^= (2u<<(high-1)) - (1u<<low);
            else {
                --high;
                uint32 *w = words + (low>>5);
                uint32 *wHigh = words + (high>>5);
                uint32 l = 1u << (low&31);
                uint32 h = 2u << (high&31);
                if (w == wHigh)
                    *w ^= h - l;
                else {
                    *w++ ^= -l;
                    while (w != wHigh)
                        *w++ ^= static_cast<uint32>(-1);
                    *w ^= h - 1;
                }
            }
    }


//
// Array Queues
//

    // See ArrayQueue below.
    template <typename T> class RawArrayQueue {
        T *const cache;                 // Pointer to a fixed-size cache for holding the buffer if it's small enough
      protected:
        T *buffer;                      // Pointer to the current buffer
        T *bufferEnd;                   // Pointer to the end of the buffer
        T *f;                           // Front end of the circular buffer, used for reading elements; buffer <= f < bufferEnd
        T *b;                           // Back end of the circular buffer, used for writing elements; buffer < b <= bufferEnd
        size_t length;                  // Number of elements used in the circular buffer
        size_t bufferSize;              // Physical size of the buffer
#ifdef DEBUG
        size_t maxReservedSize;         // Maximum size reserved so far
#endif

      public:
        RawArrayQueue(T *cache, size_t cacheSize):
                cache(cache), buffer(cache), bufferEnd(cache + cacheSize),
                f(cache), b(cache), length(0), bufferSize(cacheSize)
            {DEBUG_ONLY(maxReservedSize = 0);}
      private:
        RawArrayQueue(const RawArrayQueue&);        // No copy constructor
        void operator=(const RawArrayQueue&);       // No assignment operator
      public:
        ~RawArrayQueue() {if (buffer != cache) delete[] buffer;}

      private:
        void enlarge(size_t newLength);
      public:

        // Methods that do not expand the buffer cannot throw exceptions.
        size_t size() const {return length;}
        operator bool() const {return length != 0;}
        bool operator !() const {return length == 0;}
        
        T &front() {ASSERT(length); return *f;}
        const T &front() const {ASSERT(length); return *f;}
        T &back() {ASSERT(length); return b[-1];}
        const T &back() const {ASSERT(length); return b[-1];}
        
        T &pop_front() {
            ASSERT(length);
            --length;
            T &elt = *f++;
            if (f == bufferEnd)
                f = buffer;
            return elt;
        }
        
        size_t pop_front(size_t nElts, T *&begin, T *&end);

        T &pop_back() {
            ASSERT(length);
            --length;
            T &elt = *--b;
            if (b == buffer)
                b = bufferEnd;
            return elt;
        }

        void reserve_back();
        void reserve_back(size_t nElts);
        T *advance_back();
        T *advance_back(size_t nElts, size_t &nEltsAdvanced);

        void fast_push_back(const T &elt);
        void push_back(const T &elt);

        // Same as append but assumes that memory has previously been reserved.
        // Does not throw exceptions.  T::operator= must not throw exceptions.
        template <class InputIter>
        void fast_append(InputIter begin, InputIter end) {
            size_t nElts = toSize_t(std::distance(begin, end));
            ASSERT(length + nElts <= maxReservedSize);
            while (nElts) {
                size_t nEltsAdvanced;
                T *dst = advance_back(nElts, nEltsAdvanced);
                nElts -= nEltsAdvanced;
                while (nEltsAdvanced--) {
                    *dst = *begin; ++dst; ++begin;
                }
            }
        }

        // Append elements from begin to end to the back of the queue.
        // T::operator= must not throw exceptions.
        // reserve_back may throw an exception, in which case the queue is left
        // unchanged.
        template <class InputIter> void append(InputIter begin, InputIter end) {
            size_t nElts = toSize_t(std::distance(begin, end));
            reserve_back(nElts);
            while (nElts) {
                size_t nEltsAdvanced;
                T *dst = advance_back(nElts, nEltsAdvanced);
                nElts -= nEltsAdvanced;
                while (nEltsAdvanced--) {
                    *dst = *begin; ++dst; ++begin;
                }
            }
        }
    };


    // Pop between one and nElts elements from the front of the queue.  Set begin
    // and end to an array of the first n elements, where n is the return value.
    // The popped elements may be accessed until the next non-const operation.
    // Does not throw exceptions.
    template <typename T>
    size_t RawArrayQueue<T>::pop_front(size_t nElts, T *&begin, T *&end) {
        ASSERT(nElts <= length);
        begin = f;
        size_t eltsToEnd = toSize_t(bufferEnd - f);
        if (nElts < eltsToEnd) {
            length -= nElts;
            f += nElts;
            end = f;
            return nElts;
        } else {
            length -= eltsToEnd;
            end = bufferEnd;
            f = buffer;
            return eltsToEnd;
        }
    }

    // Enlarge the buffer so that it can hold at least newLength elements.
    // May throw an exception, in which case the queue is left unchanged.
    template <typename T>
    void RawArrayQueue<T>::enlarge(size_t newLength) {
        size_t newBufferSize = bufferSize * 2;
        if (newBufferSize < newLength)
            newBufferSize = newLength;
        
        auto_ptr<T> newBuffer(new T[newBufferSize]);
        T *oldBuffer = buffer;
        size_t eltsToEnd = toSize_t(bufferEnd - f);
        if (eltsToEnd <= length)
            std::copy(f, f + eltsToEnd, newBuffer.get());
        else {
            std::copy(f, bufferEnd, newBuffer.get());
            std::copy(oldBuffer, b, newBuffer.get() + eltsToEnd);
        }
        buffer = newBuffer.release();
        f = buffer;
        b = buffer + length;
        if (oldBuffer != cache)
            delete[] oldBuffer;
        bufferSize = newBufferSize;
        bufferEnd = buffer + newBufferSize;
    }

    // Ensure that there is room to hold one more element at the back of the queue,
    // without expanding the queue's logical length.
    // May throw an exception, in which case the queue is left unchanged.
    template <typename T>
    inline void RawArrayQueue<T>::reserve_back() {
        if (length == bufferSize)
            enlarge(length + 1);
#ifdef DEBUG
        if (maxReservedSize <= length)
            maxReservedSize = length + 1;
#endif
    }

    // Ensure that there is room to hold nElts more elements at the back of the
    // queue, without expanding the queue's logical length.
    // May throw an exception, in which case the queue is left unchanged.
    template <typename T>
    inline void RawArrayQueue<T>::reserve_back(size_t nElts) {
        nElts += length;
        if (bufferSize < nElts)
            enlarge(nElts);
#ifdef DEBUG
        if (maxReservedSize < nElts)
            maxReservedSize = nElts;
#endif
    }

    // Advance the back of the queue by one element, assuming that the memory has
    // previously been reserved.
    // Return a pointer to that new element.
    // Does not throw exceptions.
    template <typename T>
    inline T *RawArrayQueue<T>::advance_back() {
        ASSERT(length < maxReservedSize);
        ++length;
        if (b == bufferEnd)
            b = buffer;
        return b++;
    }

    // Advance the back of the queue by between one and nElts elements and return a
    // pointer to them, assuming that the memory has previously been reserved.
    // nEltsAdvanced gets the actual number of elements advanced.
    // Does not throw exceptions.
    template <typename T>
    T *RawArrayQueue<T>::advance_back(size_t nElts, size_t &nEltsAdvanced) {
        size_t newLength = length + nElts;
        ASSERT(newLength <= maxReservedSize);
        if (nElts) {
            T *b2 = b;
            if (b2 == bufferEnd)
                b2 = buffer;
            
            size_t room = toSize_t(bufferEnd - b2);
            if (nElts > room) {
                nElts = room;
                newLength = length + nElts;
            }
            length = newLength;
            nEltsAdvanced = nElts;
            b = b2 + nElts;
            return b2;
        } else {
            nEltsAdvanced = 0;
            return 0;
        }
    }

    // Same as push_back but assumes that the memory has previously been reserved.
    // May throw an exception if copying elt throws one, in which case the queue is
    // left unchanged.
    template <typename T>
    inline void RawArrayQueue<T>::fast_push_back(const T &elt) {
        ASSERT(length < maxReservedSize);
        T *b2 = b;
        if (b2 == bufferEnd)
            b2 = buffer;
        *b2 = elt;
        b = b2 + 1;
        ++length;
    }

    // Append elt to the back of the queue.
    // May throw an exception, in which case the queue is left unchanged.
    template <typename T>
    inline void RawArrayQueue<T>::push_back(const T &elt) {
        reserve_back();
        T *b2 = b == bufferEnd ? buffer : b;
        *b2 = elt;
        b = b2 + 1;
        ++length;
    }


    // An ArrayQueue represents an array of elements of type T that can be written
    // at its back end and read at its front or back end.  In addition, arrays of
    // multiple elements may be written at the back end or read at the front end.
    // The ArrayQueue contains storage for a fixed size array of cacheSize elements;
    // if this size is exceeded, the ArrayQueue allocates the array from the heap.
    template <typename T, size_t cacheSize>
    class ArrayQueue: public RawArrayQueue<T> {
        T cacheArray[cacheSize];
          public:
        ArrayQueue(): RawArrayQueue<T>(cacheArray, cacheSize) {}
    };


//
// Array auto_ptr's
//

    // An ArrayAutoPtr holds a pointer to an array initialized by new T[x].
    // A regular auto_ptr cannot be used here because it deletes its pointer using
    // delete rather than delete[].
    // An appropriate operator[] is also provided.
    template <typename T> class ArrayAutoPtr {
        T *ptr;
        
      public:
        explicit ArrayAutoPtr(T *p = 0): ptr(p) {}
        ArrayAutoPtr(ArrayAutoPtr &a): ptr(a.ptr) {a.ptr = 0;}
        ArrayAutoPtr &operator=(ArrayAutoPtr &a) {reset(a.release());}
        ~ArrayAutoPtr() {delete[] ptr;}
        
        T &operator*() const {return *ptr;}
        T &operator->() const {return *ptr;}
        template<class N> T &operator[](N i) const {return ptr[i];}
        T *get() const {return ptr;}
        T *release() {T *p = ptr; ptr = 0; return p;}
        void reset(T *p = 0) {delete[] ptr; ptr = p;}
    };
    
    typedef ArrayAutoPtr<char> CharAutoPtr;
}
#endif /* ds_h___ */
