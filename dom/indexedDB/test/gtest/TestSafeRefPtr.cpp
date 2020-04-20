/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/dom/SafeRefPtr.h"
#include "mozilla/RefCounted.h"

using namespace mozilla;

class SafeBase : public SafeRefCounted<SafeBase> {
 public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(SafeBase)

  SafeBase() : mDead(false) {}

  static inline int sNumDestroyed;

  ~SafeBase() {
    MOZ_RELEASE_ASSERT(!mDead);
    mDead = true;
    sNumDestroyed++;
  }

 private:
  bool mDead;
};
struct SafeDerived : public SafeBase {};

class Base : public RefCounted<Base> {
 public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(Base)

  Base() : mDead(false) {}

  static inline int sNumDestroyed;

  ~Base() {
    MOZ_RELEASE_ASSERT(!mDead);
    mDead = true;
    sNumDestroyed++;
  }

 private:
  bool mDead;
};

struct Derived : public Base {};

already_AddRefed<Base> NewFoo() {
  RefPtr<Base> ptr(new Base());
  return ptr.forget();
}

already_AddRefed<Base> NewBar() {
  RefPtr<Derived> bar = new Derived();
  return bar.forget();
}

TEST(DOM_IndexedDB_SafeRefPtr, Construct_Default)
{
  Base::sNumDestroyed = 0;
  {
    SafeRefPtr<Base> ptr;
    ASSERT_FALSE(ptr);
    ASSERT_EQ(0, Base::sNumDestroyed);
  }
  ASSERT_EQ(0, Base::sNumDestroyed);
}

TEST(DOM_IndexedDB_SafeRefPtr, Construct_FromNullPtr)
{
  Base::sNumDestroyed = 0;
  {
    SafeRefPtr<Base> ptr = nullptr;
    ASSERT_FALSE(ptr);
    ASSERT_EQ(0, Base::sNumDestroyed);
  }
  ASSERT_EQ(0, Base::sNumDestroyed);
}

TEST(DOM_IndexedDB_SafeRefPtr, Construct_FromMakeSafeRefPtr_SafeRefCounted)
{
  SafeBase::sNumDestroyed = 0;
  {
    SafeRefPtr<SafeBase> ptr = MakeSafeRefPtr<SafeBase>();
    ASSERT_TRUE(ptr);
    ASSERT_EQ(1u, ptr->refCount());
  }
  ASSERT_EQ(1, SafeBase::sNumDestroyed);
}

TEST(DOM_IndexedDB_SafeRefPtr,
     Construct_FromMakeSafeRefPtr_SafeRefCounted_DerivedType)
{
  SafeBase::sNumDestroyed = 0;
  {
    SafeRefPtr<SafeBase> ptr = MakeSafeRefPtr<SafeDerived>();
    ASSERT_TRUE(ptr);
    ASSERT_EQ(1u, ptr->refCount());
  }
  ASSERT_EQ(1, SafeBase::sNumDestroyed);
}

TEST(DOM_IndexedDB_SafeRefPtr, Construct_FromMakeSafeRefPtr_RefCounted)
{
  Base::sNumDestroyed = 0;
  {
    SafeRefPtr<Base> ptr = MakeSafeRefPtr<Base>();
    ASSERT_TRUE(ptr);
    ASSERT_EQ(1u, ptr->refCount());
  }
  ASSERT_EQ(1, Base::sNumDestroyed);
}

TEST(DOM_IndexedDB_SafeRefPtr, Construct_FromRefPtr)
{
  Base::sNumDestroyed = 0;
  {
    SafeRefPtr<Base> ptr = SafeRefPtr{MakeRefPtr<Base>()};
    ASSERT_TRUE(ptr);
    ASSERT_EQ(1u, ptr->refCount());
  }
  ASSERT_EQ(1, Base::sNumDestroyed);
}

TEST(DOM_IndexedDB_SafeRefPtr, Construct_FromRefPtr_DerivedType)
{
  Base::sNumDestroyed = 0;
  {
    SafeRefPtr<Base> ptr = SafeRefPtr{MakeRefPtr<Derived>()};
    ASSERT_TRUE(ptr);
    ASSERT_EQ(1u, ptr->refCount());
  }
  ASSERT_EQ(1, Base::sNumDestroyed);
}

TEST(DOM_IndexedDB_SafeRefPtr, Construct_FromAlreadyAddRefed)
{
  Base::sNumDestroyed = 0;
  {
    SafeRefPtr<Base> ptr1 = SafeRefPtr{NewFoo()};
    SafeRefPtr<Base> ptr2(NewFoo());
    ASSERT_TRUE(ptr1);
    ASSERT_TRUE(ptr2);
    ASSERT_EQ(0, Base::sNumDestroyed);
  }
  ASSERT_EQ(2, Base::sNumDestroyed);
}

TEST(DOM_IndexedDB_SafeRefPtr, Construct_FromAlreadyAddRefed_DerivedType)
{
  Base::sNumDestroyed = 0;
  {
    SafeRefPtr<Base> ptr = SafeRefPtr{NewBar()};
    ASSERT_TRUE(ptr);
    ASSERT_EQ(1u, ptr->refCount());
    ASSERT_EQ(0, Base::sNumDestroyed);
  }
  ASSERT_EQ(1, Base::sNumDestroyed);
}

TEST(DOM_IndexedDB_SafeRefPtr, Construct_FromRawPtr)
{
  Base::sNumDestroyed = 0;
  {
    SafeRefPtr<Base> ptr = SafeRefPtr{new Base(), AcquireStrongRefFromRawPtr{}};
    ASSERT_TRUE(ptr);
    ASSERT_EQ(1u, ptr->refCount());
    ASSERT_EQ(0, Base::sNumDestroyed);
  }
  ASSERT_EQ(1, Base::sNumDestroyed);
}

TEST(DOM_IndexedDB_SafeRefPtr, Construct_FromRawPtr_Dervied)
{
  Base::sNumDestroyed = 0;
  {
    SafeRefPtr<Base> ptr =
        SafeRefPtr{new Derived(), AcquireStrongRefFromRawPtr{}};
    ASSERT_TRUE(ptr);
    ASSERT_EQ(1u, ptr->refCount());
    ASSERT_EQ(0, Base::sNumDestroyed);
  }
  ASSERT_EQ(1, Base::sNumDestroyed);
}

TEST(DOM_IndexedDB_SafeRefPtr, ClonePtr)
{
  Base::sNumDestroyed = 0;
  {
    SafeRefPtr<Base> ptr1;
    {
      ptr1 = MakeSafeRefPtr<Base>();
      const SafeRefPtr<Base> ptr2 = ptr1.clonePtr();
      SafeRefPtr<Base> f3 = ptr2.clonePtr();

      ASSERT_EQ(3u, ptr1->refCount());
      ASSERT_EQ(0, Base::sNumDestroyed);
    }
    ASSERT_EQ(1u, ptr1->refCount());
    ASSERT_EQ(0, Base::sNumDestroyed);
  }
  ASSERT_EQ(1, Base::sNumDestroyed);
}

TEST(DOM_IndexedDB_SafeRefPtr, Forget)
{
  Base::sNumDestroyed = 0;
  {
    SafeRefPtr<Base> ptr1 = MakeSafeRefPtr<Base>();
    SafeRefPtr<Base> ptr2 = SafeRefPtr{ptr1.forget()};

    ASSERT_FALSE(ptr1);
    ASSERT_TRUE(ptr2);
    ASSERT_EQ(1u, ptr2->refCount());
  }
  ASSERT_EQ(1, Base::sNumDestroyed);
}

TEST(DOM_IndexedDB_SafeRefPtr, Downcast)
{
  Base::sNumDestroyed = 0;
  {
    SafeRefPtr<Base> ptr1 = MakeSafeRefPtr<Derived>();
    SafeRefPtr<Derived> ptr2 = std::move(ptr1).downcast<Derived>();

    ASSERT_FALSE(ptr1);
    ASSERT_TRUE(ptr2);
    ASSERT_EQ(1u, ptr2->refCount());
  }
  ASSERT_EQ(1, Base::sNumDestroyed);
}

struct SafeTest final : SafeBase {
  template <typename Func>
  explicit SafeTest(Func aCallback) {
    aCallback(SafeRefPtrFromThis());
  }
};

TEST(DOM_IndexedDB_SafeRefPtr, SafeRefPtrFromThis_StoreFromCtor)
{
  SafeBase::sNumDestroyed = 0;
  {
    SafeRefPtr<SafeBase> ptr1;
    {
      SafeRefPtr<SafeTest> ptr2 =
          MakeSafeRefPtr<SafeTest>([&ptr1](SafeRefPtr<SafeBase> ptr) {
            ptr1 = std::move(ptr);
            EXPECT_EQ(2u, ptr1->refCount());
          });
      ASSERT_EQ(2u, ptr2->refCount());
    }
    ASSERT_EQ(0, SafeBase::sNumDestroyed);
    ASSERT_EQ(1u, ptr1->refCount());
  }
  ASSERT_EQ(1, SafeBase::sNumDestroyed);
}

TEST(DOM_IndexedDB_SafeRefPtr, SafeRefPtrFromThis_DiscardInCtor)
{
  SafeBase::sNumDestroyed = 0;
  {
    SafeRefPtr<SafeTest> ptr = MakeSafeRefPtr<SafeTest>(
        [](SafeRefPtr<SafeBase> ptr) { EXPECT_EQ(2u, ptr->refCount()); });
    ASSERT_EQ(1u, ptr->refCount());
    ASSERT_EQ(0, SafeBase::sNumDestroyed);
  }
  ASSERT_EQ(1, SafeBase::sNumDestroyed);
}

static_assert(
    std::is_same_v<SafeRefPtr<Base>,
                   decltype(std::declval<bool>() ? MakeSafeRefPtr<Derived>()
                                                 : MakeSafeRefPtr<Base>())>);
