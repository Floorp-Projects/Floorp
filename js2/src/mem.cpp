/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <memory>

#include "mem.h"

namespace JS = JavaScript;


//
// Zones
//

// #define DEBUG_ZONE to allocate each object in its own malloc block.
// This allows tools such as Purify to do bounds checking on all blocks.


// Construct a Zone that allocates memory in chunks of the given size.
JS::Zone::Zone(size_t blockSize):
        headers(0), freeBegin(0), freeEnd(0), blockSize(blockSize)
{
    ASSERT(blockSize && !(blockSize & basicAlignment-1));
}


// Deallocate the Zone's blocks.
void JS::Zone::clear()
{
    Header *h = headers;
    while (h) {
        Header *next = h->next;
        STD::free(h);
        h = next;
    }
    headers = 0;
}


// Allocate a fully aligned block of the given size.
// Throw bad_alloc if out of memory, without corrupting any of the Zone data
// structures.
void *JS::Zone::newBlock(size_t size)
{
    Header *h = static_cast<Header *>(STD::malloc(sizeof(Header) + size));
    h->next = headers;
    headers = h;
    return h+1;
}


// Allocate a naturally-aligned object of the given size (in bytes).  Throw
// bad_alloc if out of memory, without corrupting any of the Zone data structures.
void *JS::Zone::allocate(size_t size)
{
    ASSERT(size);   // Can't allocate zero-size blocks.
#ifdef DEBUG_ZONE
    return newBlock(size);
#else
    // Round up to natural alignment if necessary
    size = size + (basicAlignment-1) & -basicAlignment;
    char *p = freeBegin;
    size_t freeBytes = toSize_t(freeEnd - p);

    if (size > freeBytes) {
        // If freeBytes is at least a quarter of blockSize, allocate a separate block.
        if (freeBytes<<2 >= blockSize || size >= blockSize)
            return newBlock(size);

        p = static_cast<char *>(newBlock(blockSize));
        freeEnd = p + blockSize;
    }
    freeBegin = p + size;
    return p;
#endif
}


// Same as allocate but does not align size up to basicAlignment.  Thus, the
// next object allocated in the zone will immediately follow this one if it
// falls in the same zone block.
// Use this when all objects in the zone have the same size.
void *JS::Zone::allocateUnaligned(size_t size)
{
    ASSERT(size);   // Can't allocate zero-size blocks.
#ifdef DEBUG_ZONE
    return newBlock(size);
#else
    char *p = freeBegin;
    size_t freeBytes = toSize_t(freeEnd - p);

    if (size > freeBytes) {
        // If freeBytes is at least a quarter of blockSize, allocate a separate block.
        if (freeBytes<<2 >= blockSize || size >= blockSize)
            return newBlock(size);

        p = static_cast<char *>(newBlock(blockSize));
        freeEnd = p + blockSize;
    }
    freeBegin = p + size;
    return p;
#endif
}


//
// Arenas
//

struct JS::Arena::DestructorEntry: JS::ArenaObject {
    DestructorEntry *next;              // Next destructor registration in linked list
    void (*destructor)(void *);         // Destructor function
    void *object;                       // Object on which to call the destructor

    DestructorEntry (void (*destructor)(void *), void *object) :
            destructor(destructor), object(object) {}
};


// Call the Arena's registered destructors.
void JS::Arena::runDestructors()
{
    DestructorEntry *e = destructorEntries;
    while (e) {
        e->destructor(e->object);
        e = e->next;
    }
    destructorEntries = 0;
}


// Ensure that object's destructor is called at the time the arena is
// deallocated or cleared. The destructors will be called in reverse order of
// being registered. registerDestructor might itself runs out of memory, in
// which case it immediately calls object's destructor before throwing
// bad_alloc.
void JS::Arena::newDestructorEntry(void (*destructor)(void *), void *object)
{
    try {
        DestructorEntry *e = new(*this) DestructorEntry(destructor, object);
        e->next = destructorEntries;
        destructorEntries = e;
    } catch (...) {
        destructor(object);
        throw;
    }
}
