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
        return (pointer) STD::memset(header + 1, 0, n);
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
    if (size == 0) size = owner->size(object);
    STD::memcpy(newObject, object, size);
    mObjectSpace.mAllocPtr += align(size + sizeof(ObjectHeader));
    oldHeader->mForwardingPointer = pointer(newObject) + kIsForwardingPointer;
    return newObject;
}

void* Collector::arrayCopy(void* array)
{
    size_t* header = (size_t*) (reinterpret_cast<char*>(array) - ARRAY_HEADER_SIZE);
    void* newArray = reinterpret_cast<char*>(copy(header, ARRAY_HEADER_SIZE + (header[0] * header[1]))) + ARRAY_HEADER_SIZE;
    return newArray;
}

#if DEBUG

struct ConsCell {
    float32 car;
    ConsCell* cdr;

    ConsCell() : car(0.0), cdr(NULL) {}
    ~ConsCell() {}

private:
    typedef Collector::InstanceOwner<ConsCell> ConsCellOwner;
    friend class Collector::InstanceOwner<ConsCell>;
    typedef Collector::ArrayOwner<ConsCell> ConsCellArrayOwner;
    friend class Collector::ArrayOwner<ConsCell>;

    /**
    * Scans through the object, and copies all references.
    */
    Collector::size_type scan(Collector* collector)
    {
        this->cdr = (ConsCell*) collector->copy(this->cdr, sizeof(ConsCell));
        return sizeof(ConsCell);
    }

public:
    void* operator new(size_t n, Collector& gc)
    {
        static ConsCellOwner owner;
        return gc.allocateObject(n, &owner);
    }

    void operator delete(void* ptr, Collector& gc) {}
 
    void* operator new[] (size_t n, Collector& gc)
    {
        static ConsCellArrayOwner owner;
        return gc.allocateObject(n, &owner);
    }
};

#if 0

struct ConsCellArray {
    ConsCell* cells;
    
    ConsCellArray(size_t count, Collector& gc) : cells(NULL) {
        cells = new (gc) ConsCell[count];
    }

private:
    typedef Collector::InstanceOwner<ConsCellArray> ConsCellArrayOwner;
    friend class Collector::InstanceOwner<ConsCellArray>;

    /**
    * Scans through the object, and copies all references.
    */
    Collector::size_type scan(Collector* collector)
    {
        this->cells = (ConsCell*) collector->arrayCopy(this->cells);
        return sizeof(ConsCellArray);
    }

public:
    void* operator new(size_t n, Collector& gc)
    {
        static ConsCellArrayOwner owner;
        return gc.allocateObject(n, &owner);
    }

    void operator delete(void* ptr, Collector& gc) {}
};

#endif

void testCollector()
{
    Collector gc;
    
    ConsCell* head = 0;
    gc.addRoot((Collector::pointer*)&head);
    
    const size_t kCellCount = 10;
    
    ConsCell* cell;
    ConsCell** link = &head;
    
    size_t i;
    for (i = 0; i < kCellCount; ++i) {
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
    for (i = 0; i < kCellCount; i++) {
        cell = *link;
        ASSERT(cell);
        ASSERT(cell->car == (float32)i);
        ASSERT(cell->cdr);
        link = &cell->cdr;
    }
    
    // make sure list is still circularly linked.
    ASSERT(*link == head);
    gc.removeRoot((Collector::pointer*)&head);

#if 0    
    // create an array of ConsCells.
    ConsCellArray* array = 0;
    gc.addRoot((Collector::pointer*)&array);
    
    array = new (gc) ConsCellArray(kCellCount, gc);
    for (i = 0; i < kCellCount; i++) {
        cell = &array->cells[i];
        cell->car = i;
    }

    // run a garbage collection.
    gc.collect();

    for (i = 0; i < kCellCount; i++) {
        cell = &array->cells[i];
        ASSERT(cell);
        ASSERT(cell->car == (float32)i);
        ASSERT(cell->cdr == NULL);
    }
#endif
}

#endif // DEBUG

}
