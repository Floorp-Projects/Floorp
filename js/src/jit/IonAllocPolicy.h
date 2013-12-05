/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_IonAllocPolicy_h
#define jit_IonAllocPolicy_h

#include "mozilla/GuardObjects.h"
#include "mozilla/TypeTraits.h"

#include "jscntxt.h"

#include "ds/LifoAlloc.h"
#include "jit/InlineList.h"
#include "jit/Ion.h"

namespace js {
namespace jit {

class TempAllocator
{
    LifoAllocScope lifoScope_;

    // Linked list of GCThings rooted by this allocator.
    CompilerRootNode *rootList_;

  public:
    TempAllocator(LifoAlloc *lifoAlloc)
      : lifoScope_(lifoAlloc),
        rootList_(nullptr)
    { }

    void *allocateInfallible(size_t bytes)
    {
        void *p = lifoScope_.alloc().allocInfallible(bytes);
        JS_ASSERT(p);
        return p;
    }

    void *allocate(size_t bytes)
    {
        void *p = lifoScope_.alloc().alloc(bytes);
        if (!ensureBallast())
            return nullptr;
        return p;
    }

    LifoAlloc *lifoAlloc()
    {
        return &lifoScope_.alloc();
    }

    CompilerRootNode *&rootList()
    {
        return rootList_;
    }

    bool ensureBallast() {
        // Most infallible Ion allocations are small, so we use a ballast of
        // ~16K for now.
        return lifoScope_.alloc().ensureUnusedApproximate(16 * 1024);
    }
};

// Stack allocated rooter for all roots associated with a TempAllocator
class AutoTempAllocatorRooter : private AutoGCRooter
{
  public:
    explicit AutoTempAllocatorRooter(JSContext *cx, TempAllocator *temp
                                     MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : AutoGCRooter(cx, IONALLOC), temp(temp)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    friend void AutoGCRooter::trace(JSTracer *trc);
    void trace(JSTracer *trc);

  private:
    TempAllocator *temp;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class IonAllocPolicy
{
    TempAllocator &alloc_;

  public:
    IonAllocPolicy(TempAllocator &alloc)
      : alloc_(alloc)
    {}
    void *malloc_(size_t bytes) {
        return alloc_.allocate(bytes);
    }
    void *calloc_(size_t bytes) {
        void *p = alloc_.allocate(bytes);
        if (p)
            memset(p, 0, bytes);
        return p;
    }
    void *realloc_(void *p, size_t oldBytes, size_t bytes) {
        void *n = malloc_(bytes);
        if (!n)
            return n;
        memcpy(n, p, Min(oldBytes, bytes));
        return n;
    }
    void free_(void *p) {
    }
    void reportAllocOverflow() const {
    }
};

// Deprecated. Don't use this. Will be removed after everything has been
// converted to IonAllocPolicy.
class OldIonAllocPolicy
{
  public:
    OldIonAllocPolicy()
    {}
    void *malloc_(size_t bytes) {
        return GetIonContext()->temp->allocate(bytes);
    }
    void *calloc_(size_t bytes) {
        void *p = GetIonContext()->temp->allocate(bytes);
        if (p)
            memset(p, 0, bytes);
        return p;
    }
    void *realloc_(void *p, size_t oldBytes, size_t bytes) {
        void *n = malloc_(bytes);
        if (!n)
            return n;
        memcpy(n, p, Min(oldBytes, bytes));
        return n;
    }
    void free_(void *p) {
    }
    void reportAllocOverflow() const {
    }
};

class AutoIonContextAlloc
{
    TempAllocator tempAlloc_;
    IonContext *icx_;
    TempAllocator *prevAlloc_;

  public:
    explicit AutoIonContextAlloc(JSContext *cx)
      : tempAlloc_(&cx->tempLifoAlloc()),
        icx_(GetIonContext()),
        prevAlloc_(icx_->temp)
    {
        icx_->temp = &tempAlloc_;
    }

    ~AutoIonContextAlloc() {
        JS_ASSERT(icx_->temp == &tempAlloc_);
        icx_->temp = prevAlloc_;
    }
};

struct TempObject
{
    inline void *operator new(size_t nbytes, TempAllocator &alloc) {
        return alloc.allocateInfallible(nbytes);
    }
    template <class T>
    inline void *operator new(size_t nbytes, T *pos) {
        static_assert(mozilla::IsConvertible<T*, TempObject*>::value,
                      "Placement new argument type must inherit from TempObject");
        return pos;
    }
};

// Deprecated, don't use for (new) classes. Will be removed when all classes have
// been converted to placement new/TempObject (bug 937540).
struct OldTempObject
  : public TempObject
{
    using TempObject::operator new;

    inline void *operator new(size_t nbytes) {
        return GetIonContext()->temp->allocateInfallible(nbytes);
    }
};

template <typename T>
class TempObjectPool
{
    InlineForwardList<T> freed_;

  public:
    T *allocate() {
        if (freed_.empty())
            return new T();
        return freed_.popFront();
    }
    void free(T *obj) {
        freed_.pushFront(obj);
    }
    void clear() {
        freed_.clear();
    }
};

} // namespace jit
} // namespace js

#endif /* jit_IonAllocPolicy_h */
