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
#include "nsIAtom.h"
#include "mozilla/dom/EventHandlerBinding.h"

#define NS_IJSEVENTLISTENER_IID \
{ 0x92f9212b, 0xa6aa, 0x4867, \
  { 0x93, 0x8a, 0x56, 0xbe, 0x17, 0x67, 0x4f, 0xd4 } }

class nsEventHandler
{
public:
  typedef mozilla::dom::EventHandlerNonNull EventHandlerNonNull;
  typedef mozilla::dom::BeforeUnloadEventHandlerNonNull
    BeforeUnloadEventHandlerNonNull;
  typedef mozilla::dom::OnErrorEventHandlerNonNull OnErrorEventHandlerNonNull;
  typedef mozilla::dom::CallbackFunction CallbackFunction;

  enum HandlerType {
    eUnset = 0,
    eNormal = 0x1,
    eOnError = 0x2,
    eOnBeforeUnload = 0x3,
    eTypeBits = 0x3
  };

  nsEventHandler() :
    mBits(0)
  {}

  nsEventHandler(EventHandlerNonNull* aHandler)
  {
    Assign(aHandler, eNormal);
  }

  nsEventHandler(OnErrorEventHandlerNonNull* aHandler)
  {
    Assign(aHandler, eOnError);
  }

  nsEventHandler(BeforeUnloadEventHandlerNonNull* aHandler)
  {
    Assign(aHandler, eOnBeforeUnload);
  }

  nsEventHandler(const nsEventHandler& aOther)
  {
    if (aOther.HasEventHandler()) {
      // Have to make sure we take our own ref
      Assign(aOther.Ptr(), aOther.Type());
    } else {
      mBits = 0;
    }
  }

  ~nsEventHandler()
  {
    ReleaseHandler();
  }

  HandlerType Type() const {
    return HandlerType(mBits & eTypeBits);
  }

  bool HasEventHandler() const
  {
    return !!Ptr();
  }

  void SetHandler(const nsEventHandler& aHandler)
  {
    if (aHandler.HasEventHandler()) {
      ReleaseHandler();
      Assign(aHandler.Ptr(), aHandler.Type());
    } else {
      ForgetHandler();
    }
  }

  EventHandlerNonNull* EventHandler() const
  {
    MOZ_ASSERT(Type() == eNormal && Ptr());
    return reinterpret_cast<EventHandlerNonNull*>(Ptr());
  }

  void SetHandler(EventHandlerNonNull* aHandler)
  {
    ReleaseHandler();
    Assign(aHandler, eNormal);
  }

  BeforeUnloadEventHandlerNonNull* BeforeUnloadEventHandler() const
  {
    MOZ_ASSERT(Type() == eOnBeforeUnload);
    return reinterpret_cast<BeforeUnloadEventHandlerNonNull*>(Ptr());
  }

  void SetHandler(BeforeUnloadEventHandlerNonNull* aHandler)
  {
    ReleaseHandler();
    Assign(aHandler, eOnBeforeUnload);
  }

  OnErrorEventHandlerNonNull* OnErrorEventHandler() const
  {
    MOZ_ASSERT(Type() == eOnError);
    return reinterpret_cast<OnErrorEventHandlerNonNull*>(Ptr());
  }

  void SetHandler(OnErrorEventHandlerNonNull* aHandler)
  {
    ReleaseHandler();
    Assign(aHandler, eOnError);
  }

  CallbackFunction* Ptr() const
  {
    // Have to cast eTypeBits so we don't have to worry about
    // promotion issues after the bitflip.
    return reinterpret_cast<CallbackFunction*>(mBits & ~uintptr_t(eTypeBits));
  }

  void ForgetHandler()
  {
    ReleaseHandler();
    mBits = 0;
  }

  bool operator==(const nsEventHandler& aOther) const
  {
    return
      Ptr() && aOther.Ptr() &&
      Ptr()->CallbackPreserveColor() == aOther.Ptr()->CallbackPreserveColor();
  }
private:
  void operator=(const nsEventHandler&) MOZ_DELETE;

  void ReleaseHandler()
  {
    nsISupports* ptr = Ptr();
    NS_IF_RELEASE(ptr);
  }

  void Assign(nsISupports* aHandler, HandlerType aType) {
    MOZ_ASSERT(aHandler, "Must have handler");
    NS_ADDREF(aHandler);
    mBits = uintptr_t(aHandler) | uintptr_t(aType);
  }

  uintptr_t mBits;
};

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
                     nsISupports *aTarget, nsIAtom* aType,
                     const nsEventHandler& aHandler)
  : mContext(aContext), mScopeObject(aScopeObject), mEventName(aType),
    mHandler(aHandler)
  {
    nsCOMPtr<nsISupports> base = do_QueryInterface(aTarget);
    mTarget = base.get();
  }

  // Can return null if we already have a handler.
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

  // Can return null if we already have a handler.
  JSObject* GetEventScope() const
  {
    return xpc_UnmarkGrayObject(mScopeObject);
  }

  const nsEventHandler& GetHandler() const
  {
    return mHandler;
  }

  nsIAtom* EventName() const
  {
    return mEventName;
  }

  // Set a handler for this event listener.  The handler must already
  // be bound to the right target.
  void SetHandler(const nsEventHandler& aHandler, nsIScriptContext* aContext,
                  JS::Handle<JSObject*> aScopeObject)
  {
    mHandler.SetHandler(aHandler);
    mContext = aContext;
    UpdateScopeObject(aScopeObject);
  }
  void SetHandler(mozilla::dom::EventHandlerNonNull* aHandler)
  {
    mHandler.SetHandler(aHandler);
  }
  void SetHandler(mozilla::dom::BeforeUnloadEventHandlerNonNull* aHandler)
  {
    mHandler.SetHandler(aHandler);
  }
  void SetHandler(mozilla::dom::OnErrorEventHandlerNonNull* aHandler)
  {
    mHandler.SetHandler(aHandler);
  }

  virtual size_t SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const
  {
    return 0;

    // Measurement of the following members may be added later if DMD finds it
    // is worthwhile:
    // - mContext
    // - mTarget
    //
    // The following members are not measured:
    // - mScopeObject: because they're measured by the JS memory
    //   reporters
    // - mHandler: may be shared with others
    // - mEventName: shared with others
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

  // Update our mScopeObject; we have to make sure we properly handle
  // the hold/drop stuff, so have to do it in nsJSEventListener.
  virtual void UpdateScopeObject(JS::Handle<JSObject*> aScopeObject) = 0;

  nsCOMPtr<nsIScriptContext> mContext;
  JS::Heap<JSObject*> mScopeObject;
  nsISupports* mTarget;
  nsCOMPtr<nsIAtom> mEventName;
  nsEventHandler mHandler;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIJSEventListener, NS_IJSEVENTLISTENER_IID)

/* factory function.  aHandler must already be bound to aTarget.
   aContext is allowed to be null if aHandler is already set up.
 */
nsresult NS_NewJSEventListener(nsIScriptContext *aContext,
                               JSObject* aScopeObject, nsISupports* aTarget,
                               nsIAtom* aType, const nsEventHandler& aHandler,
                               nsIJSEventListener **aReturn);

#endif // nsIJSEventListener_h__
