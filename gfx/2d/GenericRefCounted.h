/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This header provides virtual, non-templated alternatives to MFBT's RefCounted<T>.
// It intentionally uses MFBT coding style with the intention of moving there
// should there be other use cases for it.

#ifndef MOZILLA_GENERICREFCOUNTED_H_
#define MOZILLA_GENERICREFCOUNTED_H_

#include "mozilla/RefPtr.h"
#include "mozilla/RefCounted.h"

namespace mozilla {

/**
 * Common base class for GenericRefCounted and GenericAtomicRefCounted.
 *
 * Having this shared base class, common to both the atomic and non-atomic
 * cases, allows to have RefPtr's that don't care about whether the
 * objects they're managing have atomic refcounts or not.
 */
class GenericRefCountedBase
{
  protected:
    virtual ~GenericRefCountedBase() {};

  public:
    // AddRef() and Release() method names are for compatibility with nsRefPtr.
    virtual void AddRef() = 0;

    virtual void Release() = 0;

    // ref() and deref() method names are for compatibility with wtf::RefPtr.
    // No virtual keywords here: if a subclass wants to override the refcounting
    // mechanism, it is welcome to do so by overriding AddRef() and Release().
    void ref() { AddRef(); }
    void deref() { Release(); }

#ifdef MOZ_REFCOUNTED_LEAK_CHECKING
    virtual const char* typeName() const = 0;
    virtual size_t typeSize() const = 0;
#endif
};

namespace detail {

template<RefCountAtomicity Atomicity>
class GenericRefCounted : public GenericRefCountedBase
{
  protected:
    GenericRefCounted() : refCnt(0) { }

    virtual ~GenericRefCounted() {
      MOZ_ASSERT(refCnt == detail::DEAD);
    }

  public:
    virtual void AddRef() override {
      // Note: this method must be thread safe for GenericAtomicRefCounted.
      MOZ_ASSERT(int32_t(refCnt) >= 0);
#ifndef MOZ_REFCOUNTED_LEAK_CHECKING
      ++refCnt;
#else
      const char* type = typeName();
      uint32_t size = typeSize();
      const void* ptr = this;
      MozRefCountType cnt = ++refCnt;
      detail::RefCountLogger::logAddRef(ptr, cnt, type, size);
#endif
    }

    virtual void Release() override {
      // Note: this method must be thread safe for GenericAtomicRefCounted.
      MOZ_ASSERT(int32_t(refCnt) > 0);
#ifndef MOZ_REFCOUNTED_LEAK_CHECKING
      MozRefCountType cnt = --refCnt;
#else
      const char* type = typeName();
      const void* ptr = this;
      MozRefCountType cnt = --refCnt;
      // Note: it's not safe to touch |this| after decrementing the refcount,
      // except for below.
      detail::RefCountLogger::logRelease(ptr, cnt, type);
#endif
      if (0 == cnt) {
        // Because we have atomically decremented the refcount above, only
        // one thread can get a 0 count here, so as long as we can assume that
        // everything else in the system is accessing this object through
        // RefPtrs, it's safe to access |this| here.
#ifdef DEBUG
        refCnt = detail::DEAD;
#endif
        delete this;
      }
    }

    MozRefCountType refCount() const { return refCnt; }
    bool hasOneRef() const {
      MOZ_ASSERT(refCnt > 0);
      return refCnt == 1;
    }

  private:
    typename Conditional<Atomicity == AtomicRefCount, Atomic<MozRefCountType>, MozRefCountType>::Type refCnt;
};

} // namespace detail

/**
 * This reference-counting base class is virtual instead of
 * being templated, which is useful in cases where one needs
 * genericity at binary code level, but comes at the cost
 * of a moderate performance and size overhead, like anything virtual.
 */
class GenericRefCounted : public detail::GenericRefCounted<detail::NonAtomicRefCount>
{
};

/**
 * GenericAtomicRefCounted is like GenericRefCounted, with an atomically updated
 * reference counter.
 */
class GenericAtomicRefCounted : public detail::GenericRefCounted<detail::AtomicRefCount>
{
};

} // namespace mozilla

#endif
