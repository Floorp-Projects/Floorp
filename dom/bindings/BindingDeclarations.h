/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

#include "js/RootingAPI.h"
#include "js/Value.h"

#include "mozilla/Maybe.h"
#include "mozilla/RootedOwningNonNull.h"
#include "mozilla/RootedRefPtr.h"

#include "mozilla/dom/DOMString.h"

#include "nsCOMPtr.h"
#include "nsStringGlue.h"
#include "nsTArray.h"

class nsIPrincipal;
class nsWrapperCache;

namespace mozilla {
namespace dom {

// Struct that serves as a base class for all dictionaries.  Particularly useful
// so we can use IsBaseOf to detect dictionary template arguments.
struct DictionaryBase
{
protected:
  bool ParseJSON(JSContext* aCx, const nsAString& aJSON,
                 JS::MutableHandle<JS::Value> aVal);

  bool StringifyToJSON(JSContext* aCx,
                       JS::Handle<JSObject*> aObj,
                       nsAString& aJSON) const;

  // Struct used as a way to force a dictionary constructor to not init the
  // dictionary (via constructing from a pointer to this class).  We're putting
  // it here so that all the dictionaries will have access to it, but outside
  // code will not.
  struct FastDictionaryInitializer {
  };

  bool mIsAnyMemberPresent = false;

private:
  // aString is expected to actually be an nsAString*.  Should only be
  // called from StringifyToJSON.
  static bool AppendJSONToString(const char16_t* aJSONData,
                                 uint32_t aDataLength, void* aString);

public:
  bool IsAnyMemberPresent() const
  {
    return mIsAnyMemberPresent;
  }
};

// Struct that serves as a base class for all typed arrays and array buffers and
// array buffer views.  Particularly useful so we can use IsBaseOf to detect
// typed array/buffer/view template arguments.
struct AllTypedArraysBase {
};

// Struct that serves as a base class for all owning unions.
// Particularly useful so we can use IsBaseOf to detect owning union
// template arguments.
struct AllOwningUnionBase {
};


struct EnumEntry {
  const char* value;
  size_t length;
};

enum class CallerType : uint32_t;

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
  JSContext* Context() const
  {
    return mCx;
  }

  bool Failed() const
  {
    return !Get();
  }

  // It returns the subjectPrincipal if called on the main-thread, otherwise
  // a nullptr is returned.
  nsIPrincipal* GetSubjectPrincipal() const;

  // Get the caller type.  Note that this needs to be called before anyone has
  // had a chance to mess with the JSContext.
  dom::CallerType CallerType() const;

protected:
  JS::Rooted<JSObject*> mGlobalJSObject;
  JSContext* mCx;
  mutable nsISupports* MOZ_UNSAFE_REF("Valid because GlobalObject is a stack "
                                      "class, and mGlobalObject points to the "
                                      "global, so it won't be destroyed as long "
                                      "as GlobalObject lives on the stack") mGlobalObject;
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
    mImpl.emplace(aValue);
  }

  bool operator==(const Optional_base<T, InternalType>& aOther) const
  {
    return mImpl == aOther.mImpl;
  }

  template<typename T1, typename T2>
  explicit Optional_base(const T1& aValue1, const T2& aValue2)
  {
    mImpl.emplace(aValue1, aValue2);
  }

  bool WasPassed() const
  {
    return mImpl.isSome();
  }

  // Return InternalType here so we can work with it usefully.
  template<typename... Args>
  InternalType& Construct(Args&&... aArgs)
  {
    mImpl.emplace(Forward<Args>(aArgs)...);
    return *mImpl;
  }

  void Reset()
  {
    mImpl.reset();
  }

  const T& Value() const
  {
    return *mImpl;
  }

  // Return InternalType here so we can work with it usefully.
  InternalType& Value()
  {
    return *mImpl;
  }

  // And an explicit way to get the InternalType even if we're const.
  const InternalType& InternalValue() const
  {
    return *mImpl;
  }

  // If we ever decide to add conversion operators for optional arrays
  // like the ones Nullable has, we'll need to ensure that Maybe<> has
  // the boolean before the actual data.

private:
  // Forbid copy-construction and assignment
  Optional_base(const Optional_base& other) = delete;
  const Optional_base &operator=(const Optional_base &other) = delete;

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

  explicit Optional(JSContext* cx) :
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
    return *this->mImpl;
  }

  // And we have to override the non-const one too, since we're
  // shadowing the one on the superclass.
  JS::Rooted<T>& Value()
  {
    return *this->mImpl;
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
  JSObject*& Construct()
  {
    // The Android compiler sucks and thinks we're trying to construct
    // a JSObject* from an int if we don't cast here.  :(
    return Optional_base<JSObject*, JSObject*>::Construct(
      static_cast<JSObject*>(nullptr));
  }

  template <class T1>
  JSObject*& Construct(const T1& t1)
  {
    return Optional_base<JSObject*, JSObject*>::Construct(t1);
  }
};

// A specialization of Optional for JS::Value to make sure no one ever uses it.
template<>
class Optional<JS::Value>
{
private:
  Optional() = delete;

  explicit Optional(const JS::Value& aValue) = delete;
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
    return *this->mImpl->get();
  }

  // And we have to override the non-const one too, since we're
  // shadowing the one on the superclass.
  NonNull<T>& Value()
  {
    return *this->mImpl;
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
    return *this->mImpl->get();
  }

  // And we have to override the non-const one too, since we're
  // shadowing the one on the superclass.
  OwningNonNull<T>& Value()
  {
    return *this->mImpl;
  }
};

// Specialization for strings.
// XXXbz we can't pull in FakeString here, because it depends on internal
// strings.  So we just have to forward-declare it and reimplement its
// ToAStringPtr.

namespace binding_detail {
struct FakeString;
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
  // FakeString class in BindingUtils.h.
  void operator=(const binding_detail::FakeString* str)
  {
    MOZ_ASSERT(str);
    mStr = reinterpret_cast<const nsString*>(str);
    mPassed = true;
  }

  const nsAString& Value() const
  {
    MOZ_ASSERT(WasPassed());
    return *mStr;
  }

private:
  // Forbid copy-construction and assignment
  Optional(const Optional& other) = delete;
  const Optional &operator=(const Optional &other) = delete;

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

  // This is no worse than get() in terms of const handling.
  operator T&() const {
    MOZ_ASSERT(inited);
    MOZ_ASSERT(ptr, "NonNull<T> was set to null");
    return *ptr;
  }

  operator T*() const {
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

struct MOZ_STACK_CLASS ParentObject {
  template<class T>
  MOZ_IMPLICIT ParentObject(T* aObject) :
    mObject(aObject),
    mWrapperCache(GetWrapperCache(aObject)),
    mUseXBLScope(false)
  {}

  template<class T, template<typename> class SmartPtr>
  MOZ_IMPLICIT ParentObject(const SmartPtr<T>& aObject) :
    mObject(aObject.get()),
    mWrapperCache(GetWrapperCache(aObject.get())),
    mUseXBLScope(false)
  {}

  ParentObject(nsISupports* aObject, nsWrapperCache* aCache) :
    mObject(aObject),
    mWrapperCache(aCache),
    mUseXBLScope(false)
  {}

  // We don't want to make this an nsCOMPtr because of performance reasons, but
  // it's safe because ParentObject is a stack class.
  nsISupports* const MOZ_NON_OWNING_REF mObject;
  nsWrapperCache* const mWrapperCache;
  bool mUseXBLScope;
};

namespace binding_detail {

// Class for simple sequence arguments, only used internally by codegen.
template<typename T>
class AutoSequence : public AutoTArray<T, 16>
{
public:
  AutoSequence() : AutoTArray<T, 16>()
  {}

  // Allow converting to const sequences as needed
  operator const Sequence<T>&() const {
    return *reinterpret_cast<const Sequence<T>*>(this);
  }
};

} // namespace binding_detail

// Enum to represent a system or non-system caller type.
enum class CallerType : uint32_t {
  System,
  NonSystem
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_BindingDeclarations_h__
