/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A header for declaring various things that binding implementation headers
 * might need.  The idea is to make binding implementation headers safe to
 * include anywhere without running into include hell like we do with
 * BindingUtils.h
 */
#ifndef mozilla_dom_BindingDeclarations_h__
#define mozilla_dom_BindingDeclarations_h__

#include "nsStringGlue.h"
#include "js/Value.h"
#include "js/RootingAPI.h"
#include "mozilla/Maybe.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsAutoPtr.h" // for nsRefPtr member variables
#include "mozilla/dom/DOMString.h"
#include "mozilla/dom/OwningNonNull.h"

class nsWrapperCache;

// nsGlobalWindow implements nsWrapperCache, but doesn't always use it. Don't
// try to use it without fixing that first.
class nsGlobalWindow;

namespace mozilla {
namespace dom {

// Struct that serves as a base class for all dictionaries.  Particularly useful
// so we can use IsBaseOf to detect dictionary template arguments.
struct DictionaryBase
{
};

// Struct that serves as a base class for all typed arrays and array buffers and
// array buffer views.  Particularly useful so we can use IsBaseOf to detect
// typed array/buffer/view template arguments.
struct AllTypedArraysBase {
};


struct MainThreadDictionaryBase : public DictionaryBase
{
protected:
  bool ParseJSON(JSContext *aCx, const nsAString& aJSON,
                 JS::MutableHandle<JS::Value> aVal);
};

struct EnumEntry {
  const char* value;
  size_t length;
};

class MOZ_STACK_CLASS GlobalObject
{
public:
  GlobalObject(JSContext* aCx, JSObject* aObject);

  JSObject* Get() const
  {
    return mGlobalJSObject;
  }

  nsISupports* GetAsSupports() const;

  // The context that this returns is not guaranteed to be in the compartment of
  // the object returned from Get(), in fact it's generally in the caller's
  // compartment.
  JSContext* GetContext() const
  {
    return mCx;
  }

  bool Failed() const
  {
    return !Get();
  }

protected:
  JS::Rooted<JSObject*> mGlobalJSObject;
  JSContext* mCx;
  mutable nsISupports* mGlobalObject;
  mutable nsCOMPtr<nsISupports> mGlobalObjectRef;
};

// Class for representing optional arguments.
template<typename T, typename InternalType>
class Optional_base
{
public:
  Optional_base()
  {}

  explicit Optional_base(const T& aValue)
  {
    mImpl.construct(aValue);
  }

  template<typename T1, typename T2>
  explicit Optional_base(const T1& aValue1, const T2& aValue2)
  {
    mImpl.construct(aValue1, aValue2);
  }

  bool WasPassed() const
  {
    return !mImpl.empty();
  }

  void Construct()
  {
    mImpl.construct();
  }

  template <class T1>
  void Construct(const T1 &t1)
  {
    mImpl.construct(t1);
  }

  template <class T1, class T2>
  void Construct(const T1 &t1, const T2 &t2)
  {
    mImpl.construct(t1, t2);
  }

  void Reset()
  {
    if (WasPassed()) {
      mImpl.destroy();
    }
  }

  const T& Value() const
  {
    return mImpl.ref();
  }

  // Return InternalType here so we can work with it usefully.
  InternalType& Value()
  {
    return mImpl.ref();
  }

  // And an explicit way to get the InternalType even if we're const.
  const InternalType& InternalValue() const
  {
    return mImpl.ref();
  }

  // If we ever decide to add conversion operators for optional arrays
  // like the ones Nullable has, we'll need to ensure that Maybe<> has
  // the boolean before the actual data.

private:
  // Forbid copy-construction and assignment
  Optional_base(const Optional_base& other) MOZ_DELETE;
  const Optional_base &operator=(const Optional_base &other) MOZ_DELETE;

protected:
  Maybe<InternalType> mImpl;
};

template<typename T>
class Optional : public Optional_base<T, T>
{
public:
  Optional() :
    Optional_base<T, T>()
  {}

  explicit Optional(const T& aValue) :
    Optional_base<T, T>(aValue)
  {}
};

template<typename T>
class Optional<JS::Handle<T> > :
  public Optional_base<JS::Handle<T>, JS::Rooted<T> >
{
public:
  Optional() :
    Optional_base<JS::Handle<T>, JS::Rooted<T> >()
  {}

  Optional(JSContext* cx) :
    Optional_base<JS::Handle<T>, JS::Rooted<T> >()
  {
    this->Construct(cx);
  }

  Optional(JSContext* cx, const T& aValue) :
    Optional_base<JS::Handle<T>, JS::Rooted<T> >(cx, aValue)
  {}

  // Override the const Value() to return the right thing so we're not
  // returning references to temporaries.
  JS::Handle<T> Value() const
  {
    return this->mImpl.ref();
  }

  // And we have to override the non-const one too, since we're
  // shadowing the one on the superclass.
  JS::Rooted<T>& Value()
  {
    return this->mImpl.ref();
  }
};

// A specialization of Optional for JSObject* to make sure that when someone
// calls Construct() on it we will pre-initialized the JSObject* to nullptr so
// it can be traced safely.
template<>
class Optional<JSObject*> : public Optional_base<JSObject*, JSObject*>
{
public:
  Optional() :
    Optional_base<JSObject*, JSObject*>()
  {}

  explicit Optional(JSObject* aValue) :
    Optional_base<JSObject*, JSObject*>(aValue)
  {}

  // Don't allow us to have an uninitialized JSObject*
  void Construct()
  {
    // The Android compiler sucks and thinks we're trying to construct
    // a JSObject* from an int if we don't cast here.  :(
    Optional_base<JSObject*, JSObject*>::Construct(
      static_cast<JSObject*>(nullptr));
  }

  template <class T1>
  void Construct(const T1& t1)
  {
    Optional_base<JSObject*, JSObject*>::Construct(t1);
  }
};

// A specialization of Optional for JS::Value to make sure that when someone
// calls Construct() on it we will pre-initialized the JS::Value to
// JS::UndefinedValue() so it can be traced safely.
template<>
class Optional<JS::Value> : public Optional_base<JS::Value, JS::Value>
{
public:
  Optional() :
    Optional_base<JS::Value, JS::Value>()
  {}

  explicit Optional(JS::Value aValue) :
    Optional_base<JS::Value, JS::Value>(aValue)
  {}

  // Don't allow us to have an uninitialized JS::Value
  void Construct()
  {
    Optional_base<JS::Value, JS::Value>::Construct(JS::UndefinedValue());
  }

  template <class T1>
  void Construct(const T1& t1)
  {
    Optional_base<JS::Value, JS::Value>::Construct(t1);
  }
};

// A specialization of Optional for NonNull that lets us get a T& from Value()
template<typename U> class NonNull;
template<typename T>
class Optional<NonNull<T> > : public Optional_base<T, NonNull<T> >
{
public:
  // We want our Value to actually return a non-const reference, even
  // if we're const.  At least for things that are normally pointer
  // types...
  T& Value() const
  {
    return *this->mImpl.ref().get();
  }

  // And we have to override the non-const one too, since we're
  // shadowing the one on the superclass.
  NonNull<T>& Value()
  {
    return this->mImpl.ref();
  }
};

// A specialization of Optional for OwningNonNull that lets us get a
// T& from Value()
template<typename T>
class Optional<OwningNonNull<T> > : public Optional_base<T, OwningNonNull<T> >
{
public:
  // We want our Value to actually return a non-const reference, even
  // if we're const.  At least for things that are normally pointer
  // types...
  T& Value() const
  {
    return *this->mImpl.ref().get();
  }

  // And we have to override the non-const one too, since we're
  // shadowing the one on the superclass.
  OwningNonNull<T>& Value()
  {
    return this->mImpl.ref();
  }
};

// Specialization for strings.
// XXXbz we can't pull in FakeDependentString here, because it depends on
// internal strings.  So we just have to forward-declare it and reimplement its
// ToAStringPtr.

namespace binding_detail {
struct FakeDependentString;
} // namespace binding_detail

template<>
class Optional<nsAString>
{
public:
  Optional() : mPassed(false) {}

  bool WasPassed() const
  {
    return mPassed;
  }

  void operator=(const nsAString* str)
  {
    MOZ_ASSERT(str);
    mStr = str;
    mPassed = true;
  }

  // If this code ever goes away, remove the comment pointing to it in the
  // FakeDependentString class in BindingUtils.h.
  void operator=(const binding_detail::FakeDependentString* str)
  {
    MOZ_ASSERT(str);
    mStr = reinterpret_cast<const nsDependentString*>(str);
    mPassed = true;
  }

  const nsAString& Value() const
  {
    MOZ_ASSERT(WasPassed());
    return *mStr;
  }

private:
  // Forbid copy-construction and assignment
  Optional(const Optional& other) MOZ_DELETE;
  const Optional &operator=(const Optional &other) MOZ_DELETE;

  bool mPassed;
  const nsAString* mStr;
};

template<class T>
class NonNull
{
public:
  NonNull()
#ifdef DEBUG
    : inited(false)
#endif
  {}

  operator T&() {
    MOZ_ASSERT(inited);
    MOZ_ASSERT(ptr, "NonNull<T> was set to null");
    return *ptr;
  }

  operator const T&() const {
    MOZ_ASSERT(inited);
    MOZ_ASSERT(ptr, "NonNull<T> was set to null");
    return *ptr;
  }

  operator T*() {
    MOZ_ASSERT(inited);
    MOZ_ASSERT(ptr, "NonNull<T> was set to null");
    return ptr;
  }

  void operator=(T* t) {
    ptr = t;
    MOZ_ASSERT(ptr);
#ifdef DEBUG
    inited = true;
#endif
  }

  template<typename U>
  void operator=(U* t) {
    ptr = t->ToAStringPtr();
    MOZ_ASSERT(ptr);
#ifdef DEBUG
    inited = true;
#endif
  }

  T** Slot() {
#ifdef DEBUG
    inited = true;
#endif
    return &ptr;
  }

  T* Ptr() {
    MOZ_ASSERT(inited);
    MOZ_ASSERT(ptr, "NonNull<T> was set to null");
    return ptr;
  }

  // Make us work with smart-ptr helpers that expect a get()
  T* get() const {
    MOZ_ASSERT(inited);
    MOZ_ASSERT(ptr);
    return ptr;
  }

protected:
  T* ptr;
#ifdef DEBUG
  bool inited;
#endif
};

// Class for representing sequences in arguments.  We use a non-auto array
// because that allows us to use sequences of sequences and the like.  This
// needs to be fallible because web content controls the length of the array,
// and can easily try to create very large lengths.
template<typename T>
class Sequence : public FallibleTArray<T>
{
public:
  Sequence() : FallibleTArray<T>()
  {}
};

inline nsWrapperCache*
GetWrapperCache(nsWrapperCache* cache)
{
  return cache;
}

inline nsWrapperCache*
GetWrapperCache(nsGlobalWindow*)
{
  return nullptr;
}

inline nsWrapperCache*
GetWrapperCache(void* p)
{
  return nullptr;
}

// Helper template for smart pointers to resolve ambiguity between
// GetWrappeCache(void*) and GetWrapperCache(const ParentObject&).
template <template <typename> class SmartPtr, typename T>
inline nsWrapperCache*
GetWrapperCache(const SmartPtr<T>& aObject)
{
  return GetWrapperCache(aObject.get());
}

struct ParentObject {
  template<class T>
  ParentObject(T* aObject) :
    mObject(aObject),
    mWrapperCache(GetWrapperCache(aObject))
  {}

  template<class T, template<typename> class SmartPtr>
  ParentObject(const SmartPtr<T>& aObject) :
    mObject(aObject.get()),
    mWrapperCache(GetWrapperCache(aObject.get()))
  {}

  ParentObject(nsISupports* aObject, nsWrapperCache* aCache) :
    mObject(aObject),
    mWrapperCache(aCache)
  {}

  nsISupports* const mObject;
  nsWrapperCache* const mWrapperCache;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_BindingDeclarations_h__
