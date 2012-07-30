/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIJSEventListener_h__
#define nsIJSEventListener_h__

#include "nsIScriptContext.h"
#include "jsapi.h"
#include "xpcpublic.h"
#include "nsIDOMEventListener.h"

class nsIAtom;

#define NS_IJSEVENTLISTENER_IID \
{ 0x92f9212b, 0xa6aa, 0x4867, \
  { 0x93, 0x8a, 0x56, 0xbe, 0x17, 0x67, 0x4f, 0xd4 } }

// Implemented by script event listeners. Used to retrieve the
// script object corresponding to the event target and the handler itself.
// (Note this interface is now used to store script objects for all
// script languages, so is no longer JS specific)
//
// Note, mTarget is a raw pointer and the owner of the nsIJSEventListener object
// is expected to call Disconnect()!
class nsIJSEventListener : public nsIDOMEventListener
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IJSEVENTLISTENER_IID)

  nsIJSEventListener(nsIScriptContext* aContext, JSObject* aScopeObject,
                     nsISupports *aTarget, JSObject *aHandler)
    : mContext(aContext), mScopeObject(aScopeObject), mHandler(aHandler)
  {
    nsCOMPtr<nsISupports> base = do_QueryInterface(aTarget);
    mTarget = base.get();
  }

  nsIScriptContext *GetEventContext() const
  {
    return mContext;
  }

  nsISupports *GetEventTarget() const
  {
    return mTarget;
  }

  void Disconnect()
  {
    mTarget = nullptr;
  }

  JSObject* GetEventScope() const
  {
    return xpc_UnmarkGrayObject(mScopeObject);
  }

  JSObject *GetHandler() const
  {
    return xpc_UnmarkGrayObject(mHandler);
  }

  // Set a handler for this event listener.  Must not be called if
  // there is already a handler!  The handler must already be bound to
  // the right target.
  virtual void SetHandler(JSObject *aHandler) = 0;

  // Among the sub-classes that inherit (directly or indirectly) from nsINode,
  // measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - nsIJSEventListener: mEventName
  //
  virtual size_t SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const
  {
    return 0;

    // Measurement of the following members may be added later if DMD finds it
    // is worthwhile:
    // - mContext
    // - mTarget
    //
    // The following members are not measured:
    // - mScopeObject, mHandler: because they're measured by the JS memory
    //   reporters
  }

  virtual size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

protected:
  virtual ~nsIJSEventListener()
  {
    NS_ASSERTION(!mTarget, "Should have called Disconnect()!");
  }
  nsCOMPtr<nsIScriptContext> mContext;
  JSObject* mScopeObject;
  nsISupports* mTarget;
  JSObject *mHandler;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIJSEventListener, NS_IJSEVENTLISTENER_IID)

/* factory function.  aHandler must already be bound to aTarget */
nsresult NS_NewJSEventListener(nsIScriptContext *aContext,
                               JSObject* aScopeObject, nsISupports* aTarget,
                               nsIAtom* aType, JSObject* aHandler,
                               nsIJSEventListener **aReturn);

#endif // nsIJSEventListener_h__
