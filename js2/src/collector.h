/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
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
 * Contributor(s):  Patrick Beard <beard@netscape.com>
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

#ifndef collector_h___
#define collector_h___

#include "mem.h"
#include <deque>
#include <utility>

namespace JavaScript
{
    using std::deque;
    using std::pair;

    // tuneable parameters of the collector.
    enum {
        kLogObjectAlignment = 3,
        kObjectAlignment = (1 << kLogObjectAlignment),
        kObjectAddressMask = (-1 << kLogObjectAlignment),

        kIsForwardingPointer = 0x1,

        kObjectSpaceSize = 1024 * 1024
    };
    
    // collector entry points.
    class Collector {
    public:
        typedef size_t size_type;
        typedef ptrdiff_t difference_type;
        typedef char *pointer;
        typedef const char *const_pointer;

        Collector();
        ~Collector();

        /**
         * Adds a root, which is required to be a densely packed array of pointers.
         * Each element of a root array must contain either a valid pointer or NULL.
         */
        void addRoot(pointer* root, size_type count = 1);
        void removeRoot(pointer* root);

        class ObjectOwner;

        /**
         * Allocates an object, which will be managed by the specified ObjectOwner.
         */
        pointer allocateObject(size_type n, ObjectOwner* owner);
        
        /**
         * Runs a garbage collection cycle.
         */
        void collect();
        
        /**
         * Abstract class used by clients of the collector to manage objects.
         */
        class ObjectOwner {
        public:
            /**
             * Scans the owned object, calling Collector::copy() on each
             * of the object's references.
             */
            virtual size_type scan(Collector* collector, void* object) = 0;

            /**
             * Performs a bitwise copy of oldObject into newObject.
             * This is only called when Collector::copy() is passed
             * 0 as its size parameter.
             */
            virtual size_type copy(void* newObject, void* oldObject) = 0;
        };
        
        template<class T>
        class Owner : public ObjectOwner {
        public:
            virtual size_type scan(Collector* collector, void* object)
            {
                T* t = reinterpret_cast<T*>(object);
                return t->scan(collector);
            }
            
            virtual size_type copy(void* newObject, void* oldObject)
            {
                memcpy(newObject, oldObject, sizeof(T));
                return sizeof(T);
            }
        };
        
 /* private: */
        void* copy(void* object, size_type size = 0);
        
    private:
        template <typename T> struct Space {
            typedef T value_type;
            typedef T *pointer_type;
            size_type mSize;
            pointer_type mFromPtr;
            pointer_type mToPtr;
            pointer_type mAllocPtr;
            pointer_type mLimitPtr;
            
            Space(size_type n)
                :   mSize(n),
                    mFromPtr(0), mToPtr(0),
                    mAllocPtr(0), mLimitPtr(0)
            {
                mFromPtr = new value_type[n];
                mToPtr = new value_type[n];
                mAllocPtr = mToPtr;
                mLimitPtr = mToPtr + n;
            }
            
            ~Space()
            {
                delete[] mFromPtr;
                delete[] mToPtr;
            }
            
            pointer_type Swap()
            {
                pointer_type newToPtr = mFromPtr;
                mFromPtr = mToPtr;
                mToPtr = newToPtr;
                mAllocPtr = newToPtr;
                mLimitPtr = newToPtr + mSize;
                return newToPtr;
            }
            
            bool Grow()
            {
                mSize *= 2;
                delete[] mFromPtr;
                mFromPtr = new value_type[mSize];
                collect();
                delete[] mFromPtr;
                mFromPtr = new value_type[mSize];
            }
            
            bool Contains(pointer ptr)
            {
                return (ptr >= mToPtr && ptr < mLimitPtr);
            }
        };
        Space<char> mObjectSpace;

        typedef pair<pointer*, size_type> RootSegment;
        typedef deque<RootSegment> RootSegments;
        RootSegments mRoots;

        Collector(const Collector&);        // No copy constructor
        void operator=(const Collector&);   // No assignment operator
    };

    void testCollector();
}

#endif // collector_h___
