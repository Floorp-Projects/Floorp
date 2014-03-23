/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIJSEventListener_h__
#define nsIJSEventListener_h__

#include "nsIScriptContext.h"
#include "xpcpublic.h"
#include "nsIDOMEventListener.h"
#include "nsIAtom.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/EventHandlerBinding.h"

#define NS_IJSEVENTLISTENER_IID \
{ 0x8f06b4af, 0xbd0d, 0x486b, \
  { 0x81, 0xc8, 0x20, 0x42, 0x40, 0x2b, 0xf1, 0xef } }

class nsEventHandler
{
public:
  typedef mozilla::dom::EventHandlerNonNull EventHandlerNonNull;
  typedef mozilla::dom::OnBeforeUnloadEventHandlerNonNull
    OnBeforeUnloadEventHandlerNonNull;
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

  nsEventHandler(OnBeforeUnloadEventHandlerNonNull* aHandler)
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

  OnBeforeUnloadEventHandlerNonNull* OnBeforeUnloadEventHandler() const
  {
    MOZ_ASSERT(Type() == eOnBeforeUnload);
    return reinterpret_cast<OnBeforeUnloadEventHandlerNonNull*>(Ptr());
  }

  void SetHandler(OnBeforeUnloadEventHandlerNonNull* aHandler)
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

  nsIJSEventListener(nsISupports *aTarget, nsIAtom* aType,
                     const nsEventHandler& aHandler)
  : mEventName(aType), mHandler(aHandler)
  {
    nsCOMPtr<nsISupports> base = do_QueryInterface(aTarget);
    mTarget = base.get();
  }

  nsISupports *GetEventTarget() const
  {
    return mTarget;
  }

  void Disconnect()
  {
    mTarget = nullptr;
  }

  const nsEventHandler& GetHandler() const
  {
    return mHandler;
  }

  void ForgetHandler()
  {
    mHandler.ForgetHandler();
  }

  nsIAtom* EventName() const
  {
    return mEventName;
  }

  // Set a handler for this event listener.  The handler must already
  // be bound to the right target.
  void SetHandler(const nsEventHandler& aHandler)
  {
    mHandler.SetHandler(aHandler);
  }
  void SetHandler(mozilla::dom::EventHandlerNonNull* aHandler)
  {
    mHandler.SetHandler(aHandler);
  }
  void SetHandler(mozilla::dom::OnBeforeUnloadEventHandlerNonNull* aHandler)
  {
    mHandler.SetHandler(aHandler);
  }
  void SetHandler(mozilla::dom::OnErrorEventHandlerNonNull* aHandler)
  {
    mHandler.SetHandler(aHandler);
  }

  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return 0;

    // Measurement of the following members may be added later if DMD finds it
    // is worthwhile:
    // - mTarget
    //
    // The following members are not measured:
    // - mHandler: may be shared with others
    // - mEventName: shared with others
  }

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

protected:
  virtual ~nsIJSEventListener()
  {
    NS_ASSERTION(!mTarget, "Should have called Disconnect()!");
  }

  nsISupports* mTarget;
  nsCOMPtr<nsIAtom> mEventName;
  nsEventHandler mHandler;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIJSEventListener, NS_IJSEVENTLISTENER_IID)

/* factory function.  aHandler must already be bound to aTarget.
   aContext is allowed to be null if aHandler is already set up.
 */
nsresult NS_NewJSEventListener(nsISupports* aTarget, nsIAtom* aType,
                               const nsEventHandler& aHandler,
                               nsIJSEventListener **aReturn);

#endif // nsIJSEventListener_h__
