/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_dombindingbase_h__
#define mozilla_dom_workers_dombindingbase_h__

#include "mozilla/dom/workers/Workers.h"

#include "nsISupportsImpl.h"
#include "nsWrapperCache.h"
#include "nsWrapperCacheInlines.h"

BEGIN_WORKERS_NAMESPACE

#define BINDING_ENSURE_TRUE(_cond, _result, _retval) \
  PR_BEGIN_MACRO \
    if (!(_cond)) { \
      NS_WARNING("BINDING_ENSURE failed!"); \
      aRv = _result; \
      return _retval; \
    } \
  PR_END_MACRO

#define BINDING_ENSURE_SUCCESS(_cond, _result, _retval) \
  BINDING_ENSURE_TRUE(NS_SUCCEEDED(_cond), _result, _retval)

class DOMBindingBase : public nsWrapperCache,
                       public nsISupports
{
  JSContext* mJSContext;

protected:
  DOMBindingBase(JSContext* aCx);
  virtual ~DOMBindingBase();

  virtual void
  _trace(JSTracer* aTrc);

  virtual void
  _finalize(JSFreeOp* aFop);

public:
  NS_DECL_ISUPPORTS

  JSContext*
  GetJSContext() const;

  void
  TraceJSObject(JSTracer* aTrc, const char* aName)
  {
    if (GetJSObject()) {
      TraceWrapperJSObject(aTrc, aName);
    }
  }

#ifdef DEBUG
  JSObject*
  GetJSObject() const;

  void
  SetJSObject(JSObject* aObject);
#else
  JSObject*
  GetJSObject() const
  {
    return GetWrapperJSObject();
  }

  void
  SetJSObject(JSObject* aObject)
  {
    SetWrapperJSObject(aObject);
  }
#endif
};

template <class T>
class DOMBindingAnchor
{
  T* mBinding;
  JS::Anchor<JSObject*> mAnchor;

public:
  DOMBindingAnchor()
  : mBinding(nullptr)
  { }

  DOMBindingAnchor(T* aBinding)
  {
    *this = aBinding;
  }

  DOMBindingAnchor&
  operator=(T* aBinding)
  {
    mBinding = aBinding;

    if (aBinding) {
      JSObject* obj = aBinding->GetJSObject();
      MOZ_ASSERT(obj);

      mAnchor.set(obj);
    }
    else {
      mAnchor.clear();
    }
  }

  T*
  get() const
  {
    return const_cast<T*>(mBinding);
  }

  T*
  operator->() const
  {
    return get();
  }

  operator T*() const
  {
    return get();
  }

private:
  DOMBindingAnchor(const DOMBindingAnchor& aOther) MOZ_DELETE;

  DOMBindingAnchor&
  operator=(const DOMBindingAnchor& aOther) MOZ_DELETE;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_dombindingbase_h__
