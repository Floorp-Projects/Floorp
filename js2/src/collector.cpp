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

#include "collector.h"

namespace JavaScript
{

Collector::Collector()
    :   mObjectSpace(kObjectSpaceSize)
{
}

Collector::~Collector()
{
}

void
Collector::addRoot(pointer* root, size_type n)
{
    mRoots.push_back(RootSegment(root, n));
}

void
Collector::removeRoot(pointer* root)
{
    for (RootSegments::iterator i = mRoots.begin(), e = mRoots.end(); i != e; ++i) {
        if (i->first == root) {
            mRoots.erase(i);
            return;
        }
    }
}

inline Collector::size_type align(Collector::size_type n)
{
    return (n + (kObjectAlignment - 1)) & kObjectAddressMask;
}

/**
 * Simple object header union. Contains a
 * float64 element to ensure proper object
 * alignment. On some architectures, the
 * proper alignment may be wider.
 */
union ObjectHeader {
    Collector::ObjectOwner* mOwner;
    Collector::pointer mForwardingPointer;
    float64 mAlignment;
};

Collector::pointer
Collector::allocateObject(size_type n, ObjectOwner* owner)
{
    size_type size = align(n + sizeof(ObjectHeader));
    pointer ptr = mObjectSpace.mAllocPtr;
    if ((ptr + size) <= mObjectSpace.mLimitPtr) {
        mObjectSpace.mAllocPtr += size;
        ObjectHeader* header = (ObjectHeader*) ptr;
        header->mOwner = owner;
        return (pointer) std::memset(header + 1, 0, n);
    }
    // need to run a garbage collection to recover more space, or double space size?
    return 0;
}

void
Collector::collect()
{
    // 0. swap from/to space. we now start allocating in the new toSpace.
    Space<char>::pointer_type scanPtr = mObjectSpace.Swap();

    // 1. scan all registered root segments.
    for (RootSegments::iterator i = mRoots.begin(), e = mRoots.end(); i != e; ++i) {
        RootSegment& r = *i;
        
        pointer* refs = r.first;
        pointer* limit = r.first + r.second;
        while (refs < limit) {
            pointer& ref = *refs++;
            if (ref) {
                ref = (pointer) copy(ref);
            }
        }
    }
    
    // 2. Scan through toSpace until scanPtr meets mAllocPtr.
    while (scanPtr < mObjectSpace.mAllocPtr) {
        ObjectHeader* header = (ObjectHeader*) scanPtr;
        size_type size = header->mOwner->scan(this, (header + 1));
        scanPtr += align(size + sizeof(ObjectHeader));
    }
}

inline bool isForwardingPointer(void* ptr)
{
    return (uint32(ptr) & kIsForwardingPointer);
}

void* Collector::copy(void* object, Collector::size_type size)
{
    if (object == NULL)
        return NULL;
        
    // forwarding pointer?
    ObjectHeader* oldHeader = ((ObjectHeader*)object) - 1;
    if (isForwardingPointer(oldHeader->mForwardingPointer))
        return (oldHeader->mForwardingPointer - kIsForwardingPointer);

    // copy the old object into toSpace. copy will always succeed,
    // because we only call it from within collect. the problem
    // is when we don't recover any space... will have to be able
    // to expand the heaps.
    ObjectOwner* owner = oldHeader->mOwner;
    ObjectHeader* newHeader = (ObjectHeader*) mObjectSpace.mAllocPtr;
    newHeader->mOwner = owner;
    void* newObject = (newHeader + 1);
    if (size) {
        std::memcpy(newObject, object, size);
    } else {
        size = owner->copy(newObject, object);
    }
    mObjectSpace.mAllocPtr += align(size + sizeof(ObjectHeader));
    oldHeader->mForwardingPointer = pointer(newObject) + kIsForwardingPointer;
    return newObject;
}

#if DEBUG

struct ConsCell {
    float64 car;
    ConsCell* cdr;

private:
    typedef Collector::size_type size_type;

    class Owner : public Collector::ObjectOwner {
    public:
        /**
         * Scans through the object, and copies all references.
         */
        virtual size_type scan(Collector* collector, void* object)
        {
            ConsCell* cell = (ConsCell*) object;
            cell->cdr = (ConsCell*) collector->copy(cell->cdr, sizeof(ConsCell));
            return sizeof(ConsCell);
        }
        
        /**
         * Performs a bitwise copy of the old object into the new object.
         */
        virtual size_type copy(void* newObject, void* oldObject)
        {
            ConsCell* newCell = (ConsCell*) newObject;
            ConsCell* oldCell = (ConsCell*) oldObject;
            newCell->car = oldCell->car;
            newCell->cdr = oldCell->cdr;
            return sizeof(ConsCell);
        }
    };

public:
    void* operator new(std::size_t n, Collector& gc)
    {
        static Owner owner;
        return gc.allocateObject(n, &owner);
    }
};

void testCollector()
{
    Collector gc;
    
    ConsCell* head = 0;
    gc.addRoot((Collector::pointer*)&head);
    
    const uint32 kCellCount = 100;
    
    ConsCell* cell;
    ConsCell** link = &head;
    
    for (uint32 i = 0; i < kCellCount; ++i) {
        *link = cell = new (gc) ConsCell;
        ASSERT(cell);
        cell->car = i;
        link = &cell->cdr;
    }
    
    // circularly link the list.
    *link = head;
    
    // run a garbage collection.
    gc.collect();
    
    // walk the list again to verify that it is intact.
    link = &head;
    for (uint32 i = 0; i < kCellCount; i++) {
        cell = *link;
        ASSERT(cell);
        ASSERT(cell->car == (float64)i);
        ASSERT(cell->cdr);
        link = &cell->cdr;
    }
    
    // make sure list is still circularly linked.
    ASSERT(*link == head);
}

#endif // DEBUG

}
