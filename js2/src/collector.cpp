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
    :   mObjectSpace(kObjectSpaceSize),
        mFloatSpace(kFloatSpaceSize)
{
}

Collector::~Collector()
{
}

void
Collector::addRoot(void* root, size_type n)
{
    mRoots.push_back(RootSegment(pointer(root), n));
}

void
Collector::removeRoot(void* root)
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

Collector::pointer
Collector::allocateObject(size_type n, pointer type)
{
    size_type size = align(n + sizeof(ObjectHeader));
    pointer ptr = mObjectSpace.mAllocPtr;
    if ((ptr + size) <= mObjectSpace.mLimitPtr) {
        mObjectSpace.mAllocPtr += size;
        ObjectHeader* header = (ObjectHeader*) ptr;
        header->mSize = size;
        header->mType = type;
        return (pointer) std::memset(ptr + sizeof(ObjectHeader), 0, n);
    }
    // need to run a garbage collection to recover more space, or double space size?
    return 0;
}

float64*
Collector::allocateFloat64(float64 value)
{
    float64* fptr = mFloatSpace.mAllocPtr;
    if (fptr < mFloatSpace.mLimitPtr) {
        mFloatSpace.mAllocPtr++;
        *fptr = value;
        return (float64*) (uint32(fptr) | kFloat64Tag);
    }
    // need to run a garbage collection to recover more space, or double space size?
    return 0;
}

inline bool is_object(Collector::pointer ref)
{
    return ((uint32(ref) & kObjectAddressMask) == uint32(ref));
}

inline bool is_float64(Collector::pointer ref)
{
    return ((uint32(ref) & kFloat64TagMask) == kFloat64Tag);
}

void
Collector::collect()
{
    // 0. swap from/to space. we now start allocating in the new toSpace.
    Space<char>::pointer_type scanPtr = mObjectSpace.Swap();
    mFloatSpace.Swap();

    // 1. scan all registered root segments.
    for (RootSegments::iterator i = mRoots.begin(), e = mRoots.end(); i != e; ++i) {
        RootSegment& r = *i;
        
        pointer* refs = (pointer*) r.first;
        pointer* limit = (pointer*) (r.first + r.second);
        while (refs < limit) {
            pointer& ref = *refs++;
            if (ref) {
                if (is_object(ref))
                    ref = copy(ref);
                else
                if (is_float64(ref))
                    ref = copyFloat64(ref);
            }
        }
    }
    
    // 2. Scan through toSpace until scanPtr meets mAllocPtr.
    while (scanPtr < mObjectSpace.mAllocPtr) {
        ObjectHeader* header = (ObjectHeader*) scanPtr;
        if (header->mType)
            header->mType = copy(header->mType);
        scanPtr += header->mSize;
        pointer* refs = (pointer*) (header + 1);
        pointer* limit = (pointer*) scanPtr;
        while (refs < limit) {
            pointer& ref = *refs++;
            if (ref) {
                if (is_object(ref))
                    ref = copy(ref);
                else
                if (is_float64(ref))
                    ref = copyFloat64(ref);
            }
        }
    }
}

Collector::pointer
Collector::copy(pointer object)
{
    // forwarding pointer?
    ObjectHeader* oldHeader = ((ObjectHeader*)object) - 1;
    if (oldHeader->mSize == kIsForwardingPointer)
        return oldHeader->mType;

    // copy the old object into toSpace. copy will always succeed,
    // because we only call it from within collect. the problem
    // is when we don't recover any space... will have to be able
    // to expand the heaps.
    size_type n = oldHeader->mSize;
    ObjectHeader* newHeader = (ObjectHeader*) mObjectSpace.mAllocPtr;
    mObjectSpace.mAllocPtr += n;
    std::memcpy(newHeader, oldHeader, n);
    oldHeader->mSize = kIsForwardingPointer;
    oldHeader->mType = (pointer) (newHeader + 1);
    
    return (pointer) (newHeader + 1);
}

Collector::pointer
Collector::copyFloat64(pointer object)
{
    float64* fptr = mFloatSpace.mAllocPtr++;
    *fptr = *(float64*) (uint32(object) & kFloat64AddressMask);
    return (pointer) (uint32(fptr) | kFloat64Tag);
}

#if DEBUG

struct ConsCell {
    float64* car;
    ConsCell* cdr;

    void* operator new(std::size_t n, Collector& gc)
    {
        return gc.allocateObject(n);
    }
};

void testCollector()
{
    Collector gc;
    
    ConsCell* head = 0;
    gc.addRoot(&head, sizeof(ConsCell*));
    
    const uint32 kCellCount = 100;
    
    ConsCell* cell;
    ConsCell** link = &head;
    
    for (uint32 i = 0; i < kCellCount; ++i) {
        *link = cell = new (gc) ConsCell;
        ASSERT(cell);
        cell->car = gc.allocateFloat64(i);
        ASSERT(cell->car);
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
        ASSERT(cell->car);
        float64 value = gc.getFloat64(cell->car);
        ASSERT(value == (float64)i);
        link = &cell->cdr;
    }
    
    // make sure list is still circularly linked.
    ASSERT(*link == head);
}

#endif // DEBUG

}
