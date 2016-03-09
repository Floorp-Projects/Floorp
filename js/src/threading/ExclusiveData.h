/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef threading_ExclusiveData_h
#define threading_ExclusiveData_h

#include "mozilla/Maybe.h"
#include "mozilla/Move.h"

#include "jslock.h"

namespace js {

namespace detail {

class ExclusiveDataBase
{
  private:
    mutable PRLock* lock_;

    ExclusiveDataBase(const ExclusiveDataBase&) = delete;
    ExclusiveDataBase& operator=(const ExclusiveDataBase&) = delete;

  public:
    // This move constructor is only public for `mozilla::Forward`.
    ExclusiveDataBase(ExclusiveDataBase&& rhs)
      : lock_(rhs.lock_)
    {
        MOZ_ASSERT(&rhs != this, "self-move disallowed!");
        rhs.lock_ = nullptr;
    }

    ~ExclusiveDataBase();

  protected:
    explicit ExclusiveDataBase(PRLock* lock)
      : lock_(lock)
    {
        MOZ_ASSERT(lock_);
    }

    static mozilla::Maybe<ExclusiveDataBase> Create();

    void acquire() const;
    void release() const;
};

} // namespace detail

/**
 * A mutual exclusion lock class.
 *
 * `ExclusiveData` provides an RAII guard to automatically lock and unlock when
 * accessing the protected inner value.
 *
 * Unlike the STL's `std::mutex`, the protected value is internal to this
 * class. This is a huge win: one no longer has to rely on documentation to
 * explain the relationship between a lock and its protected data, and the type
 * system can enforce[0] it.
 *
 * For example, suppose we have a counter class:
 *
 *     class Counter
 *     {
 *         int32_t i;
 *
 *       public:
 *         void inc(int32_t n) { i += n; }
 *     };
 *
 * If we share a counter across threads with `std::mutex`, we rely solely on
 * comments to document the relationship between the lock and its data, like
 * this:
 *
 *     class SharedCounter
 *     {
 *         // Remember to acquire `counter_lock` when accessing `counter`,
 *         // pretty please!
 *         Counter counter;
 *         std::mutex counter_lock;
 *
 *       public:
 *         void inc(size_t n) {
 *             // Whoops, forgot to acquire the lock! Off to the races!
 *             counter.inc(n);
 *         }
 *     };
 *
 * In contrast, `ExclusiveData` wraps the protected value, enabling the type
 * system to enforce that we acquire the lock before accessing the value:
 *
 *     class SharedCounter
 *     {
 *         ExclusiveData<Counter> counter;
 *
 *       public:
 *         void inc(size_t n) {
 *             auto guard = counter.lock();
 *             guard->inc(n);
 *         }
 *     };
 *
 * The API design is based on Rust's `std::sync::ExclusiveData<T>` type.
 *
 * [0]: Of course, we don't have a borrow checker in C++, so the type system
 *      cannot guarantee that you don't stash references received from
 *      `ExclusiveData<T>::Guard` somewhere such that the reference outlives the
 *      guard's lifetime and therefore becomes invalid. To help avoid this last
 *      foot-gun, prefer using the guard directly! Do not store raw references
 *      to the protected value in other structures!
 */
template <typename T>
class ExclusiveData : private detail::ExclusiveDataBase
{
    mutable T value_;

    ExclusiveData(const ExclusiveData&) = delete;
    ExclusiveData& operator=(const ExclusiveData&) = delete;

    template <typename U>
    explicit ExclusiveData(U&& u, ExclusiveDataBase&& base)
      : ExclusiveDataBase(mozilla::Move(base))
      , value_(mozilla::Forward<U>(u))
    { }

  public:
    /**
     * Create a new `ExclusiveData`, with perfect forwarding of the protected
     * value.
     *
     * On success, `mozilla::Some` is returned. On failure, `mozilla::Nothing`
     * is returned.
     */
    template <typename U>
    static mozilla::Maybe<ExclusiveData<T>> Create(U&& u) {
        auto base = detail::ExclusiveDataBase::Create();
        if (base.isNothing())
            return mozilla::Nothing();
        return mozilla::Some(ExclusiveData(mozilla::Forward<U>(u), mozilla::Move(*base)));
    }

    ExclusiveData(ExclusiveData&& rhs)
      : ExclusiveDataBase(mozilla::Move(static_cast<ExclusiveDataBase&&>(rhs)))
      , value_(mozilla::Move(rhs.value_))
    {
        MOZ_ASSERT(&rhs != this, "self-move disallowed!");
    }

    ExclusiveData& operator=(ExclusiveData&& rhs) {
        this->~ExclusiveData();
        new (this) ExclusiveData(mozilla::Move(rhs));
        return *this;
    }

    /**
     * An RAII class that provides exclusive access to a `ExclusiveData<T>`'s
     * protected inner `T` value.
     *
     * Note that this is intentionally marked MOZ_STACK_CLASS instead of
     * MOZ_RAII_CLASS, as the latter disallows moves and returning by value, but
     * Guard utilizes both.
     */
    class MOZ_STACK_CLASS Guard
    {
        const ExclusiveData* parent_;

        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

      public:
        explicit Guard(const ExclusiveData& parent)
          : parent_(&parent)
        {
            parent_->acquire();
        }

        Guard(Guard&& rhs)
          : parent_(rhs.parent_)
        {
            MOZ_ASSERT(&rhs != this, "self-move disallowed!");
            rhs.parent_ = nullptr;
        }

        Guard& operator=(Guard&& rhs) {
            this->~Guard();
            new (this) Guard(mozilla::Move(rhs));
            return *this;
        }

        T& get() const {
            MOZ_ASSERT(parent_);
            return parent_->value_;
        }

        operator T& () const { return get(); }
        T* operator->() const { return &get(); }

        ~Guard() {
            if (parent_)
                parent_->release();
        }
    };

    /**
     * Access the protected inner `T` value for exclusive reading and writing.
     */
    Guard lock() const {
        return Guard(*this);
    }
};

} // namespace js

#endif // threading_ExclusiveData_h
