/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_Mutex_h
#define js_Mutex_h

#include "mozilla/Maybe.h"
#include "mozilla/Move.h"

#include "jslock.h"

namespace js {

namespace detail {

class MutexBase
{
  private:
    mutable PRLock* lock_;

    MutexBase(const MutexBase&) = delete;
    MutexBase& operator=(const MutexBase&) = delete;

  public:
    // This move constructor is only public for `mozilla::Forward`.
    MutexBase(MutexBase&& rhs)
      : lock_(rhs.lock_)
    {
        MOZ_ASSERT(&rhs != this, "self-move disallowed!");
        rhs.lock_ = nullptr;
    }

    ~MutexBase();

  protected:
    explicit MutexBase(PRLock* lock)
      : lock_(lock)
    {
        MOZ_ASSERT(lock_);
    }

    static mozilla::Maybe<MutexBase> Create();

    void acquire() const;
    void release() const;
};

} // namespace detail

/**
 * A mutual exclusion lock class.
 *
 * `Mutex` provides an RAII guard to automatically lock and unlock when
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
 *         // Remember to acquire `counter_lock` when accessing `counter`, pretty please!
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
 * In contrast, `Mutex` wraps the protected value, enabling the type system to
 * enforce that we acquire the lock before accessing the value:
 *
 *     class SharedCounter
 *     {
 *         Mutex<Counter> counter;
 *
 *       public:
 *         void inc(size_t n) {
 *             auto guard = counter.lock();
 *             guard->inc(n);
 *         }
 *     };
 *
 * The API design is based on Rust's `std::sync::Mutex<T>` type.
 *
 * [0]: Of course, we don't have a borrow checker in C++, so the type system
 *      cannot guarantee that you don't stash references received from
 *      `Mutex<T>::Guard` somewhere such that the reference outlives the guard's
 *      lifetime and therefore becomes invalid. To help avoid this last
 *      foot-gun, prefer using the guard directly! Do not store raw references
 *      to the protected value in other structures!
 */
template <typename T>
class Mutex : private detail::MutexBase
{
    mutable T value_;

    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    template <typename U>
    explicit Mutex(U&& u, MutexBase&& base)
      : MutexBase(mozilla::Move(base))
      , value_(mozilla::Forward<U>(u))
    { }

  public:
    /**
     * Create a new `Mutex`, with perfect forwarding of the protected value.
     *
     * On success, `mozilla::Some` is returned. On failure, `mozilla::Nothing`
     * is returned.
     */
    template <typename U>
    static mozilla::Maybe<Mutex<T>> Create(U&& u) {
        auto base = detail::MutexBase::Create();
        if (base.isNothing())
            return mozilla::Nothing();
        return mozilla::Some(Mutex(mozilla::Forward<U>(u), mozilla::Move(*base)));
    }

    Mutex(Mutex&& rhs)
      : MutexBase(mozilla::Move(static_cast<MutexBase&&>(rhs)))
      , value_(mozilla::Move(rhs.value_))
    {
        MOZ_ASSERT(&rhs != this, "self-move disallowed!");
    }

    Mutex& operator=(Mutex&& rhs) {
        this->~Mutex();
        new (this) Mutex(mozilla::Move(rhs));
        return *this;
    }

    /**
     * An RAII class that provides exclusive access to a `Mutex<T>`'s protected
     * inner `T` value.
     *
     * Note that this is intentionally marked MOZ_STACK_CLASS instead of
     * MOZ_RAII_CLASS, as the latter disallows moves and returning by value, but
     * Guard utilizes both.
     */
    class MOZ_STACK_CLASS Guard
    {
        const Mutex* parent_;

        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

      public:
        explicit Guard(const Mutex& parent)
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

#endif // js_Mutex_h
