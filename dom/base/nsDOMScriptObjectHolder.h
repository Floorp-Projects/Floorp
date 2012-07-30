/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMScriptObjectHolder_h__
#define nsDOMScriptObjectHolder_h__

#include "nsIScriptContext.h"
#include "nsIDOMScriptObjectFactory.h"

#include "jspubtd.h"

// A thin class used to help with script object memory management.  No virtual
// functions and a fully inline implementation should keep the cost down.
// [Note that a fully inline implementation is necessary for use by other
// languages, which do not link against the layout component module]
template<class T>
class NS_STACK_CLASS nsScriptObjectHolder {
public:
  // A constructor that will cause a reference to |ctx| to be stored in
  // the object.  Only use for short-lived object holders.
  nsScriptObjectHolder<T>(nsIScriptContext *ctx, T* aObject = nullptr) :
      mObject(aObject), mContext(ctx) {
    NS_ASSERTION(ctx, "Must provide a valid context");
  }

  // copy constructor
  nsScriptObjectHolder<T>(const nsScriptObjectHolder<T>& other) :
      mObject(other.mObject),
      mContext(other.mContext)
  {
    // New hold the script object and new reference on the script context.
    if (mObject)
      mContext->HoldScriptObject(mObject);
  }

  ~nsScriptObjectHolder<T>() {
    if (mObject)
      mContext->DropScriptObject(mObject);
  }

  // misc operators
  nsScriptObjectHolder<T> &operator=(const nsScriptObjectHolder<T> &other) {
    set(other);
    return *this;
  }
  bool operator!() const {
    return !mObject;
  }
  operator bool() const {
    return !!mObject;
  }
  T* get() const {
    return mObject;
  }

  // Drop the script object - but *not* the nsIScriptContext.
  nsresult drop() {
    nsresult rv = NS_OK;
    if (mObject) {
      rv = mContext->DropScriptObject(mObject);
      mObject = nullptr;
    }
    return rv;
  }

  nsresult set(T* object) {
    nsresult rv = drop();
    if (NS_FAILED(rv))
      return rv;
    if (object) {
      rv = mContext->HoldScriptObject(object);
      // don't store the pointer if we failed to lock it.
      if (NS_SUCCEEDED(rv)) {
        mObject = object;
      }
    }
    return rv;
  }
  nsresult set(const nsScriptObjectHolder<T> &other) {
    nsresult rv = drop();
    if (NS_FAILED(rv))
      return rv;
    return set(other.mObject);
  }

protected:
  T* mObject;
  nsCOMPtr<nsIScriptContext> mContext;
};

#endif // nsDOMScriptObjectHolder_h__
