/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Hierarchy of SpiderMonkey system memory allocators:
 *
 *   - System {m,c,re}alloc/new/free: Overridden by jemalloc in most
 *     environments. Do not use these functions directly.
 *
 *   - js_{m,c,re}alloc/new/free: Wraps the system allocators and adds a
 *     failure injection framework for use by the fuzzers as well as templated,
 *     typesafe variants. See js/public/Utility.h.
 *
 *   - AllocPolicy: An interface for the js allocators, for use with templates.
 *     These allocators are for system memory whose lifetime is not associated
 *     with a GC thing. See js/src/jsalloc.h.
 *
 *       - SystemAllocPolicy: No extra functionality over bare allocators.
 *
 *       - TempAllocPolicy: Adds automatic error reporting to the provided
 *         Context when allocations fail.
 *
 *   - MallocProvider. A mixin base class that handles automatically updating
 *     the GC's state in response to allocations that are tied to a GC lifetime
 *     or are for a particular GC purpose. These allocators must only be used
 *     for memory that will be freed when a GC thing is swept.
 *
 *       - gc::Zone:  Automatically triggers zone GC.
 *       - JSRuntime: Automatically triggers full GC.
 *       - ThreadsafeContext > ExclusiveContext > JSContext:
 *                    Dispatches directly to the runtime.
 */

#ifndef vm_MallocProvider_h
#define vm_MallocProvider_h

#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"
#include "mozilla/UniquePtr.h"

#include "js/Utility.h"

namespace js {

template<class Client>
struct MallocProvider
{
    void *malloc_(size_t bytes) {
        Client *client = static_cast<Client *>(this);
        client->updateMallocCounter(bytes);
        void *p = js_malloc(bytes);
        return MOZ_LIKELY(!!p) ? p : client->onOutOfMemory(nullptr, bytes);
    }

    void *calloc_(size_t bytes) {
        Client *client = static_cast<Client *>(this);
        client->updateMallocCounter(bytes);
        void *p = js_calloc(bytes);
        return MOZ_LIKELY(!!p) ? p : client->onOutOfMemory(reinterpret_cast<void *>(1), bytes);
    }

    void *realloc_(void *p, size_t oldBytes, size_t newBytes) {
        Client *client = static_cast<Client *>(this);
        /*
         * For compatibility we do not account for realloc that decreases
         * previously allocated memory.
         */
        if (newBytes > oldBytes)
            client->updateMallocCounter(newBytes - oldBytes);
        void *p2 = js_realloc(p, newBytes);
        return MOZ_LIKELY(!!p2) ? p2 : client->onOutOfMemory(p, newBytes);
    }

    void *realloc_(void *p, size_t bytes) {
        Client *client = static_cast<Client *>(this);
        /*
         * For compatibility we do not account for realloc that increases
         * previously allocated memory.
         */
        if (!p)
            client->updateMallocCounter(bytes);
        void *p2 = js_realloc(p, bytes);
        return MOZ_LIKELY(!!p2) ? p2 : client->onOutOfMemory(p, bytes);
    }

    template <class T>
    T *pod_malloc() {
        return (T *)malloc_(sizeof(T));
    }

    template <class T>
    T *pod_calloc() {
        return (T *)calloc_(sizeof(T));
    }

    template <class T>
    T *pod_malloc(size_t numElems) {
        if (numElems & mozilla::tl::MulOverflowMask<sizeof(T)>::value) {
            Client *client = static_cast<Client *>(this);
            client->reportAllocationOverflow();
            return nullptr;
        }
        return (T *)malloc_(numElems * sizeof(T));
    }

    template <class T>
    mozilla::UniquePtr<T[], JS::FreePolicy>
    make_pod_array(size_t numElems) {
        return mozilla::UniquePtr<T[], JS::FreePolicy>(pod_malloc<T>(numElems));
    }

    template <class T>
    T *
    pod_calloc(size_t numElems, JSCompartment *comp = nullptr, JSContext *cx = nullptr) {
        if (numElems & mozilla::tl::MulOverflowMask<sizeof(T)>::value) {
            Client *client = static_cast<Client *>(this);
            client->reportAllocationOverflow();
            return nullptr;
        }
        return (T *)calloc_(numElems * sizeof(T));
    }

    template <class T>
    mozilla::UniquePtr<T[], JS::FreePolicy>
    make_zeroed_pod_array(size_t numElems,
                          JSCompartment *comp = nullptr,
                          JSContext *cx = nullptr)
    {
        return mozilla::UniquePtr<T[], JS::FreePolicy>(pod_calloc<T>(numElems, comp, cx));
    }

    JS_DECLARE_NEW_METHODS(new_, malloc_, MOZ_ALWAYS_INLINE)
    JS_DECLARE_MAKE_METHODS(make_unique, new_, MOZ_ALWAYS_INLINE)
};

} /* namespace js */

#endif /* vm_MallocProvider_h */
