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
#include "jsapi.h"
#include "mozilla/Util.h"
#include "nsCOMPtr.h"
#include "nsDOMString.h"
#include "nsStringBuffer.h"
#include "nsTArray.h"
#include "nsAutoPtr.h" // for nsRefPtr member variables

class nsWrapperCache;

// nsGlobalWindow implements nsWrapperCache, but doesn't always use it. Don't
// try to use it without fixing that first.
class nsGlobalWindow;

namespace mozilla {
namespace dom {

struct MainThreadDictionaryBase
{
protected:
  JSContext* ParseJSON(const nsAString& aJSON,
                       Maybe<JSAutoRequest>& aAr,
                       Maybe<JSAutoCompartment>& aAc,
                       Maybe< JS::Rooted<JS::Value> >& aVal);
};

struct EnumEntry {
  const char* value;
  size_t length;
};

class MOZ_STACK_CLASS GlobalObject
{
public:
  GlobalObject(JSContext* aCx, JSObject* aObject);

  nsISupports* Get() const
  {
    return mGlobalObject;
  }

  bool Failed() const
  {
    return !Get();
  }

private:
  JS::RootedObject mGlobalJSObject;
  nsISupports* mGlobalObject;
  nsCOMPtr<nsISupports> mGlobalObjectRef;
};

class MOZ_STACK_CLASS WorkerGlobalObject
{
public:
  WorkerGlobalObject(JSContext* aCx, JSObject* aObject);

  JSObject* Get() const
  {
    return mGlobalJSObject;
  }
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

private:
  JS::RootedObject mGlobalJSObject;
  JSContext* mCx;
};

/**
 * A class for representing string return values.  This can be either passed to
 * callees that have an nsString or nsAString out param or passed to a callee
 * that actually knows about this class and can work with it.  Such a callee may
 * call SetStringBuffer on this object, but only if it plans to keep holding a
 * strong ref to the stringbuffer!
 *
 * The proper way to store a value in this class is to either to do nothing
 * (which leaves this as an empty string), to call SetStringBuffer with a
 * non-null stringbuffer, to call SetNull(), or to call AsAString() and set the
 * value in the resulting nsString.  These options are mutually exclusive!
 * Don't do more than one of them.
 *
 * The proper way to extract a value is to check IsNull().  If not null, then
 * check HasStringBuffer().  If that's true, check for a zero length, and if the
 * length is nonzero call StringBuffer().  If the length is zero this is the
 * empty string.  If HasStringBuffer() returns false, call AsAString() and get
 * the value from that.
 */
class MOZ_STACK_CLASS DOMString {
public:
  DOMString()
    : mStringBuffer(nullptr)
    , mLength(0)
    , mIsNull(false)
  {}
  ~DOMString()
  {
    MOZ_ASSERT(mString.empty() || !mStringBuffer,
               "Shouldn't have both present!");
  }

  operator nsString&()
  {
    return AsAString();
  }

  nsString& AsAString()
  {
    MOZ_ASSERT(!mStringBuffer, "We already have a stringbuffer?");
    MOZ_ASSERT(!mIsNull, "We're already set as null");
    if (mString.empty()) {
      mString.construct();
    }
    return mString.ref();
  }

  bool HasStringBuffer() const
  {
    MOZ_ASSERT(mString.empty() || !mStringBuffer,
               "Shouldn't have both present!");
    MOZ_ASSERT(!mIsNull, "Caller should have checked IsNull() first");
    return mString.empty();
  }

  // Get the stringbuffer.  This can only be called if HasStringBuffer()
  // returned true and StringBufferLength() is nonzero.  If that's true, it will
  // never return null.
  nsStringBuffer* StringBuffer() const
  {
    MOZ_ASSERT(!mIsNull, "Caller should have checked IsNull() first");
    MOZ_ASSERT(HasStringBuffer(),
               "Don't ask for the stringbuffer if we don't have it");
    MOZ_ASSERT(StringBufferLength() != 0, "Why are you asking for this?");
    MOZ_ASSERT(mStringBuffer,
               "If our length is nonzero, we better have a stringbuffer.");
    return mStringBuffer;
  }

  // Get the length of the stringbuffer.  Can only be called if
  // HasStringBuffer().
  uint32_t StringBufferLength() const
  {
    MOZ_ASSERT(HasStringBuffer(), "Don't call this if there is no stringbuffer");
    return mLength;
  }

  void SetStringBuffer(nsStringBuffer* aStringBuffer, uint32_t aLength)
  {
    MOZ_ASSERT(mString.empty(), "We already have a string?");
    MOZ_ASSERT(!mIsNull, "We're already set as null");
    MOZ_ASSERT(!mStringBuffer, "Setting stringbuffer twice?");
    MOZ_ASSERT(aStringBuffer, "Why are we getting null?");
    mStringBuffer = aStringBuffer;
    mLength = aLength;
  }

  void SetNull()
  {
    MOZ_ASSERT(!mStringBuffer, "Should have no stringbuffer if null");
    MOZ_ASSERT(mString.empty(), "Should have no string if null");
    mIsNull = true;
  }

  bool IsNull() const
  {
    MOZ_ASSERT(!mStringBuffer || mString.empty(),
               "How could we have a stringbuffer and a nonempty string?");
    return mIsNull || (!mString.empty() && mString.ref().IsVoid());
  }

  void ToString(nsAString& aString)
  {
    if (IsNull()) {
      SetDOMStringToNull(aString);
    } else if (HasStringBuffer()) {
      if (StringBufferLength() == 0) {
        aString.Truncate();
      } else {
        StringBuffer()->ToString(StringBufferLength(), aString);
      }
    } else {
      aString = AsAString();
    }
  }

private:
  // We need to be able to act like a string as needed
  Maybe<nsString> mString;

  // For callees that know we exist, we can be a stringbuffer/length/null-flag
  // triple.
  nsStringBuffer* mStringBuffer;
  uint32_t mLength;
  bool mIsNull;
};

// Class for representing optional arguments.
template<typename T>
class Optional
{
public:
  Optional()
  {}

  explicit Optional(const T& aValue)
  {
    mImpl.construct(aValue);
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

  const T& Value() const
  {
    return mImpl.ref();
  }

  T& Value()
  {
    return mImpl.ref();
  }

  // If we ever decide to add conversion operators for optional arrays
  // like the ones Nullable has, we'll need to ensure that Maybe<> has
  // the boolean before the actual data.

private:
  // Forbid copy-construction and assignment
  Optional(const Optional& other) MOZ_DELETE;
  const Optional &operator=(const Optional &other) MOZ_DELETE;

  Maybe<T> mImpl;
};

// Specialization for strings.
// XXXbz we can't pull in FakeDependentString here, because it depends on
// internal strings.  So we just have to forward-declare it and reimplement its
// ToAStringPtr.

struct FakeDependentString;

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
  void operator=(const FakeDependentString* str)
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

class RootedJSValue
{
public:
  RootedJSValue()
    : mCx(nullptr)
  {}

  ~RootedJSValue()
  {
    if (mCx) {
      JS_RemoveValueRoot(mCx, &mValue);
    }
  }

  bool SetValue(JSContext* aCx, JS::Value aValue)
  {
    // We don't go ahead and root if v is null, because we want to allow
    // null-initialization even when there is no cx.
    MOZ_ASSERT_IF(!aValue.isNull(), aCx);

    // Be careful to not clobber mCx if it's already set, just in case we're
    // being null-initialized (with a null cx for some reason) after we have
    // already been initialized properly with a non-null value.
    if (!aValue.isNull() && !mCx) {
      if (!JS_AddNamedValueRoot(aCx, &mValue, "RootedJSValue::mValue")) {
        return false;
      }
      mCx = aCx;
    }

    mValue = aValue;
    return true;
  }

  operator JS::Value()
  {
    return mValue;
  }

  operator const JS::Value() const
  {
    return mValue;
  }

private:
  // Don't allow copy-construction of these objects, because it'll do the wrong
  // thing with our flag mCx.
  RootedJSValue(const RootedJSValue&) MOZ_DELETE;

  JS::Value mValue;
  JSContext* mCx;
};

inline nsWrapperCache*
GetWrapperCache(nsWrapperCache* cache)
{
  return cache;
}

inline nsWrapperCache*
GetWrapperCache(nsGlobalWindow* not_allowed);

inline nsWrapperCache*
GetWrapperCache(void* p)
{
  return NULL;
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
