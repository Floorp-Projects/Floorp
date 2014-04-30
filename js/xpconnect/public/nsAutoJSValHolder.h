/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NSAUTOJSVALHOLDER_H__
#define __NSAUTOJSVALHOLDER_H__

#include "nsDebug.h"
#include "jsapi.h"

/**
 * Simple class that looks and acts like a JS::Value except that it unroots
 * itself automatically if Root() is ever called. Designed to be rooted on the
 * context or runtime (but not both!).
 */
class nsAutoJSValHolder
{
public:
  nsAutoJSValHolder()
    : mVal(JSVAL_NULL), mRt(nullptr)
  {
    // nothing to do
  }

  /**
   * Always release on destruction.
   */
  virtual ~nsAutoJSValHolder() {
    Release();
  }

  nsAutoJSValHolder(const nsAutoJSValHolder& aOther)
    : mVal(JSVAL_NULL), mRt(nullptr)
  {
    *this = aOther;
  }

  nsAutoJSValHolder& operator=(const nsAutoJSValHolder& aOther) {
    if (this != &aOther) {
      if (aOther.IsHeld()) {
        // XXX No error handling here...
        this->Hold(aOther.mRt);
      }
      else {
        this->Release();
      }
      *this = static_cast<JS::Value>(aOther);
    }
    return *this;
  }

  /**
   * Hold by rooting on the context's runtime.
   */
  bool Hold(JSContext* aCx) {
    return Hold(JS_GetRuntime(aCx));
  }

  /**
   * Hold by rooting on the runtime.
   * Note that mVal may be JSVAL_NULL, which is not a problem.
   */
  bool Hold(JSRuntime* aRt) {
    MOZ_ASSERT_IF(mRt, mRt == aRt);

    if (!mRt && JS::AddNamedValueRootRT(aRt, &mVal, "nsAutoJSValHolder")) {
      mRt = aRt;
    }

    return !!mRt;
  }

  /**
   * Manually release, nullifying mVal, and mRt, but returning
   * the original JS::Value.
   */
  JS::Value Release() {
    JS::Value oldval = mVal;

    if (mRt) {
      JS::RemoveValueRootRT(mRt, &mVal); // infallible
      mRt = nullptr;
    }

    mVal = JSVAL_NULL;

    return oldval;
  }

  /**
   * Determine if Hold has been called.
   */
  bool IsHeld() const {
    return !!mRt;
  }

  /**
   * Explicit JSObject* conversion.
   */
  JSObject* ToJSObject() const {
    return mVal.isObject()
         ? &mVal.toObject()
         : nullptr;
  }

  /**
   * Pretend to be a JS::Value.
   */
  operator JS::Value() const { return mVal; }
  JS::Value get() const { return mVal; }

  nsAutoJSValHolder &operator=(JSObject* aOther) {
    return *this = OBJECT_TO_JSVAL(aOther);
  }

  nsAutoJSValHolder &operator=(JS::Value aOther) {
#ifdef DEBUG
    if (aOther.isGCThing() && !aOther.isNull()) {
      MOZ_ASSERT(IsHeld(), "Not rooted!");
    }
#endif
    mVal = aOther;
    return *this;
  }

private:
  JS::Heap<JS::Value> mVal;
  JSRuntime* mRt;
};

#endif /* __NSAUTOJSVALHOLDER_H__ */
