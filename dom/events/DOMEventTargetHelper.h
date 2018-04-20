/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DOMEventTargetHelper_h_
#define mozilla_DOMEventTargetHelper_h_

#include "nsCOMPtr.h"
#include "nsGkAtoms.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIWeakReferenceUtils.h"
#include "MainThreadUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/dom/EventTarget.h"

struct JSCompartment;
class nsIDocument;

namespace mozilla {

class ErrorResult;

namespace dom {
class Event;
} // namespace dom

#define NS_DOMEVENTTARGETHELPER_IID \
{ 0xa28385c6, 0x9451, 0x4d7e, \
  { 0xa3, 0xdd, 0xf4, 0xb6, 0x87, 0x2f, 0xa4, 0x76 } }

class DOMEventTargetHelper : public dom::EventTarget
{
public:
  DOMEventTargetHelper()
    : mParentObject(nullptr)
    , mOwnerWindow(nullptr)
    , mHasOrHasHadOwnerWindow(false)
    , mIsKeptAlive(false)
  {
  }
  explicit DOMEventTargetHelper(nsPIDOMWindowInner* aWindow)
    : mParentObject(nullptr)
    , mOwnerWindow(nullptr)
    , mHasOrHasHadOwnerWindow(false)
    , mIsKeptAlive(false)
  {
    // Be careful not to call the virtual BindToOwner() in a
    // constructor.
    nsIGlobalObject* global = aWindow ? aWindow->AsGlobal() : nullptr;
    BindToOwnerInternal(global);
  }
  explicit DOMEventTargetHelper(nsIGlobalObject* aGlobalObject)
    : mParentObject(nullptr)
    , mOwnerWindow(nullptr)
    , mHasOrHasHadOwnerWindow(false)
    , mIsKeptAlive(false)
  {
    // Be careful not to call the virtual BindToOwner() in a
    // constructor.
    BindToOwnerInternal(aGlobalObject);
  }
  explicit DOMEventTargetHelper(DOMEventTargetHelper* aOther)
    : mParentObject(nullptr)
    , mOwnerWindow(nullptr)
    , mHasOrHasHadOwnerWindow(false)
    , mIsKeptAlive(false)
  {
    // Be careful not to call the virtual BindToOwner() in a
    // constructor.
    if (!aOther) {
      BindToOwnerInternal(static_cast<nsIGlobalObject*>(nullptr));
      return;
    }
    BindToOwnerInternal(aOther->GetParentObject());
    mHasOrHasHadOwnerWindow = aOther->HasOrHasHadOwner();
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS(DOMEventTargetHelper)

  virtual EventListenerManager* GetExistingListenerManager() const override;
  virtual EventListenerManager* GetOrCreateListenerManager() override;

  bool ComputeDefaultWantsUntrusted(ErrorResult& aRv) override;

  using EventTarget::DispatchEvent;
  bool DispatchEvent(dom::Event& aEvent, dom::CallerType aCallerType,
                     ErrorResult& aRv) override;

  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;

  nsresult PostHandleEvent(EventChainPostVisitor& aVisitor) override;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DOMEVENTTARGETHELPER_IID)

  void GetParentObject(nsIScriptGlobalObject **aParentObject)
  {
    if (mParentObject) {
      CallQueryInterface(mParentObject, aParentObject);
    } else {
      *aParentObject = nullptr;
    }
  }

  static DOMEventTargetHelper* FromSupports(nsISupports* aSupports)
  {
    dom::EventTarget* target = static_cast<dom::EventTarget*>(aSupports);
#ifdef DEBUG
    {
      nsCOMPtr<dom::EventTarget> target_qi = do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the EventTarget pointer as the
      // nsISupports pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(target_qi == target, "Uh, fix QI!");
    }
#endif

    return static_cast<DOMEventTargetHelper*>(target);
  }

  bool HasListenersFor(const nsAString& aType) const
  {
    return mListenerManager && mListenerManager->HasListenersFor(aType);
  }

  bool HasListenersFor(nsAtom* aTypeWithOn) const
  {
    return mListenerManager && mListenerManager->HasListenersFor(aTypeWithOn);
  }

  virtual nsPIDOMWindowOuter* GetOwnerGlobalForBindings() override
  {
    return nsPIDOMWindowOuter::GetFromCurrentInner(GetOwner());
  }

  nsresult CheckInnerWindowCorrectness() const
  {
    NS_ENSURE_STATE(!mHasOrHasHadOwnerWindow || mOwnerWindow);
    if (mOwnerWindow && !mOwnerWindow->IsCurrentInnerWindow()) {
      return NS_ERROR_FAILURE;
    }
    return NS_OK;
  }

  nsPIDOMWindowInner* GetOwner() const { return mOwnerWindow; }
  // Like GetOwner, but only returns non-null if the window being returned is
  // current (in the "current document" sense of the HTML spec).
  nsPIDOMWindowInner* GetWindowIfCurrent() const;
  // Returns the document associated with this event target, if that document is
  // the current document of its browsing context.  Will return null otherwise.
  nsIDocument* GetDocumentIfCurrent() const;

  // DETH subclasses may override the BindToOwner(nsIGlobalObject*) method
  // to take action when dynamically binding to a new global.  This is only
  // called on rebind since virtual methods cannot be called from the
  // constructor.  The other BindToOwner() methods will call into this
  // method.
  //
  // NOTE: Any overrides of BindToOwner() *must* invoke
  //       DOMEventTargetHelper::BindToOwner(aOwner).
  virtual void BindToOwner(nsIGlobalObject* aOwner);

  void BindToOwner(nsPIDOMWindowInner* aOwner);
  void BindToOwner(DOMEventTargetHelper* aOther);

  virtual void DisconnectFromOwner();
  using EventTarget::GetParentObject;
  virtual nsIGlobalObject* GetOwnerGlobal() const override
  {
    return mParentObject;
  }
  bool HasOrHasHadOwner() { return mHasOrHasHadOwnerWindow; }

  virtual void EventListenerAdded(nsAtom* aType) override;
  virtual void EventListenerAdded(const nsAString& aType) override;

  virtual void EventListenerRemoved(nsAtom* aType) override;
  virtual void EventListenerRemoved(const nsAString& aType) override;

  // Dispatch a trusted, non-cancellable and non-bubbling event to |this|.
  nsresult DispatchTrustedEvent(const nsAString& aEventName);
protected:
  virtual ~DOMEventTargetHelper();

  nsresult WantsUntrusted(bool* aRetVal);

  void MaybeUpdateKeepAlive();
  void MaybeDontKeepAlive();

  // If this method returns true your object is kept alive until it returns
  // false. You can use this method instead using
  // NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN macro.
  virtual bool IsCertainlyAliveForCC() const
  {
    return mIsKeptAlive;
  }

  RefPtr<EventListenerManager> mListenerManager;
  // Make |event| trusted and dispatch |aEvent| to |this|.
  nsresult DispatchTrustedEvent(dom::Event* aEvent);

  virtual void LastRelease() {}

  void KeepAliveIfHasListenersFor(const nsAString& aType);
  void KeepAliveIfHasListenersFor(nsAtom* aType);

  void IgnoreKeepAliveIfHasListenersFor(const nsAString& aType);
  void IgnoreKeepAliveIfHasListenersFor(nsAtom* aType);

  void BindToOwnerInternal(nsIGlobalObject* aOwner);

private:
  // The parent global object.  The global will clear this when
  // it is destroyed by calling DisconnectFromOwner().
  nsIGlobalObject* MOZ_NON_OWNING_REF mParentObject;
  // mParentObject pre QI-ed and cached (inner window)
  // (it is needed for off main thread access)
  // It is obtained in BindToOwner and reset in DisconnectFromOwner.
  nsPIDOMWindowInner* MOZ_NON_OWNING_REF mOwnerWindow;
  bool                       mHasOrHasHadOwnerWindow;

  struct {
    nsTArray<nsString> mStrings;
    nsTArray<RefPtr<nsAtom>> mAtoms;
  } mKeepingAliveTypes;

  bool mIsKeptAlive;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DOMEventTargetHelper,
                              NS_DOMEVENTTARGETHELPER_IID)

} // namespace mozilla

// WebIDL event handlers
#define IMPL_EVENT_HANDLER(_event)                                        \
  inline mozilla::dom::EventHandlerNonNull* GetOn##_event()               \
  {                                                                       \
    if (NS_IsMainThread()) {                                              \
      return GetEventHandler(nsGkAtoms::on##_event, EmptyString());       \
    }                                                                     \
    return GetEventHandler(nullptr, NS_LITERAL_STRING(#_event));          \
  }                                                                       \
  inline void SetOn##_event(mozilla::dom::EventHandlerNonNull* aCallback) \
  {                                                                       \
    if (NS_IsMainThread()) {                                              \
      SetEventHandler(nsGkAtoms::on##_event, EmptyString(), aCallback);   \
    } else {                                                              \
      SetEventHandler(nullptr, NS_LITERAL_STRING(#_event), aCallback);    \
    }                                                                     \
  }

#endif // mozilla_DOMEventTargetHelper_h_
