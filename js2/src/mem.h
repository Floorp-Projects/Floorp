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

#ifndef mem_h___
#define mem_h___

#include "systemtypes.h"
#include "stlcfg.h"
#include "utilities.h"

namespace JavaScript
{

//
// Zones
//

    // A zone is a region of memory from which objects can be allocated
    // individually.
    // The memory in a zone is deallocated when the zone is deallocated or its
    // clear method called.
    class Zone {
        union Header {
            Header *next;               // Next block header in linked list
            char padding[basicAlignment];// Padding to ensure following block is fully aligned
        };// Block data follows header

        Header *headers;                // Linked list of allocated blocks
        char *freeBegin;                // Pointer to free bytes left in current block
        char *freeEnd;                  // Pointer to end of free bytes left in current block
        size_t blockSize;               // Size of individual arena blocks

      public:
        explicit Zone(size_t blockSize = 1024);
      private:
        Zone(const Zone&);           // No copy constructor
        void operator=(const Zone&); // No assignment operator
      public:
        void clear();
        ~Zone() {clear();}

      private:
        void *newBlock(size_t size);
      public:
        void *allocate(size_t size);
        void *allocateUnaligned(size_t size);
    };


//
// Arenas
//

#ifndef _WIN32
    // Pretend that obj points to a value of class T and call obj's destructor.
    template<class T>
    void classDestructor(void *obj)
    {
        static_cast<T *>(obj)->~T();
    }
#else  // Microsoft Visual C++ 6.0 bug workaround
    template<class T>
    struct DestructorHolder
    {
        static void destroy(void *obj) {static_cast<T *>(obj)->~T();}
    };
#endif

    // An arena is a region of memory from which objects either derived from
    // ArenaObject or allocated using a ArenaAllocator can be allocated.  Deleting
    // these objects individually runs the destructors, if any, but does not
    // deallocate the memory.  On the other hand, the entire arena can be
    // deallocated as a whole.
    //
    // One may also allocate other objects in an arena by using the Arena
    // specialization of the global operator new.  However, be careful not to
    // delete any such objects explicitly!
    //
    // Destructors can be registered for objects (or parts of objects) allocated
    // in the arena.  These destructors are called, in reverse order of being
    // registered, at the time the arena is deallocated or cleared.  When
    // registering destructors for an object O be careful not to delete O manually
    // because that would run its destructor twice.
    class Arena: public Zone {
        struct DestructorEntry;

        DestructorEntry *destructorEntries;// Linked list of destructor registrations, ordered from most to least recently registered

      public:
        explicit Arena(size_t blockSize = 1024): Zone(blockSize), destructorEntries(0) {}
      private:
        void runDestructors();
      public:
        void clear() {runDestructors(); Zone::clear();}
        ~Arena() {runDestructors();}

      private:
        void newDestructorEntry(void (*destructor)(void *), void *object);
      public:

        // Ensure that object's destructor is called at the time the arena is deallocated or cleared.
        // The destructors will be called in reverse order of being registered.
        // registerDestructor might itself runs out of memory, in which case it immediately
        // calls object's destructor before throwing bad_alloc.
#ifndef _WIN32
        template<class T> void registerDestructor(T *object) {newDestructorEntry(&classDestructor<T>, object);}
#else
        template<class T> void registerDestructor(T *object) {newDestructorEntry(&DestructorHolder<T>::destroy, object);}
#endif
    };


    // Objects derived from this class will be contained in the Arena
    // passed to the new operator.
    struct ArenaObject {
        void *operator new(size_t size, Arena &arena) {return arena.allocate(size);}
        void *operator new[](size_t size, Arena &arena) {return arena.allocate(size);}
        void operator delete(void *, Arena &) {}
        void operator delete[](void *, Arena &) {}
      private:
        void operator delete(void *, size_t) {}
        void operator delete[](void *) {}
    };


    // Objects allocated by passing this class to standard containers will
    // be contained in the Arena passed to the ArenaAllocator's constructor.
    template<class T> class ArenaAllocator {
        Arena &arena;

      public:
        typedef T value_type;
        typedef size_t size_type;
        typedef ptrdiff_t difference_type;
        typedef T *pointer;
        typedef const T *const_pointer;
        typedef T &reference;
        typedef const T &const_reference;

        static pointer address(reference r) {return &r;}
        static const_pointer address(const_reference r) {return &r;}

        ArenaAllocator(Arena &arena): arena(arena) {}
        template<class U> ArenaAllocator(const ArenaAllocator<U> &u): arena(u.arena) {}

        pointer allocate(size_type n, const void *hint = 0) {return static_cast<pointer>(arena.allocate(n*sizeof(T)));}
        static void deallocate(pointer, size_type) {}

        static void construct(pointer p, const T &val) {new(p) T(val);}
        static void destroy(pointer p) {p->~T();}

#ifdef __GNUC__ // why doesn't g++ support numeric_limits<T>?
        static size_type max_size() {return size_type(-1) / sizeof(T);}
#else
        static size_type max_size() {return std::numeric_limits<size_type>::v_max() / sizeof(T);}
#endif

        template<class U> struct rebind {typedef ArenaAllocator<U> other;};
    };


//
// Pools
//

    // A Pool holds a collection of objects of the same type.  These
    // objects can be allocated and deallocated inexpensively.
    // To allocate a T, use new(pool) T(...), where pool has type Pool<T>.
    // To deallocate a T, use pool.destroy(t), where t has type T*.
    template <typename T> class Pool: public Zone {
        struct FreeList {
            FreeList *next;             // Next item in linked list of freed objects
        };

        STATIC_CONST(size_t, entrySize = sizeof(T) >= sizeof(FreeList *) ? sizeof(T) : sizeof(FreeList *));

        FreeList *freeList;             // Head of linked list of freed objects

      public:
        // clumpSize is the number of T's that are allocated at a time.
        explicit Pool(size_t clumpSize): Zone(clumpSize * entrySize), freeList(0) {}

        // Allocate memory for a single T.  Use this with a placement new operator to create a new T.
        void *allocate() {
            if (freeList) {
                FreeList *p = freeList;
                freeList = p->next;
                return p;
            }
            return allocateUnaligned(entrySize);
        }

        void deallocate(void *t) {
            FreeList *p = static_cast<FreeList *>(t);
            p->next = freeList;
            freeList = p;
        }

        void destroy(T *t) {ASSERT(t); t->~T(); deallocate(t);}
    };
}


inline void *operator new(size_t size, JavaScript::Arena &arena) {
    return arena.allocate(size);
}
#ifndef _WIN32
// Microsoft Visual C++ 6.0 bug: new and new[] aren't distinguished
inline void *operator new[](size_t size, JavaScript::Arena &arena) {
    return arena.allocate(size);
}
#endif


// Global delete operators.  These are only called in the rare cases that a
// constructor throws an exception and has to undo an operator new.
// An explicit delete statement will never invoke these.
inline void operator delete(void *, JavaScript::Arena &) {}
// Microsoft Visual C++ 6.0 bug: new and new[] aren't distinguished
#ifndef _WIN32
 inline void operator delete[](void *, JavaScript::Arena &) {}
#endif

template <typename T>
inline void *operator new(size_t DEBUG_ONLY(size), JavaScript::Pool<T> &pool) {
    ASSERT(size == sizeof(T)); return pool.allocate();
}
template <typename T>
inline void operator delete(void *t, JavaScript::Pool<T> &pool) {
    pool.deallocate(t);
}
#endif /* mem_h___ */
