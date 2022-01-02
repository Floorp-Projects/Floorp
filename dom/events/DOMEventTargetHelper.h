/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DOMEventTargetHelper_h_
#define mozilla_DOMEventTargetHelper_h_

#include "mozilla/Attributes.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/LinkedList.h"
#include "mozilla/RefPtr.h"
#include "nsAtom.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"
#include "nsGkAtoms.h"
#include "nsID.h"
#include "nsIGlobalObject.h"
#include "nsIScriptGlobalObject.h"
#include "nsISupports.h"
#include "nsISupportsUtils.h"
#include "nsPIDOMWindow.h"
#include "nsStringFwd.h"
#include "nsTArray.h"

class nsCycleCollectionTraversalCallback;

namespace mozilla {

class ErrorResult;
class EventChainPostVisitor;
class EventChainPreVisitor;
class EventListenerManager;

namespace dom {
class Document;
class Event;
enum class CallerType : uint32_t;
}  // namespace dom

#define NS_DOMEVENTTARGETHELPER_IID                  \
  {                                                  \
    0xa28385c6, 0x9451, 0x4d7e, {                    \
      0xa3, 0xdd, 0xf4, 0xb6, 0x87, 0x2f, 0xa4, 0x76 \
    }                                                \
  }

class DOMEventTargetHelper : public dom::EventTarget,
                             public LinkedListElement<DOMEventTargetHelper> {
 public:
  DOMEventTargetHelper();
  explicit DOMEventTargetHelper(nsPIDOMWindowInner* aWindow);
  explicit DOMEventTargetHelper(nsIGlobalObject* aGlobalObject);
  explicit DOMEventTargetHelper(DOMEventTargetHelper* aOther);

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

  void GetParentObject(nsIScriptGlobalObject** aParentObject) {
    if (mParentObject) {
      CallQueryInterface(mParentObject, aParentObject);
    } else {
      *aParentObject = nullptr;
    }
  }

  static DOMEventTargetHelper* FromSupports(nsISupports* aSupports) {
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

  bool HasListenersFor(const nsAString& aType) const;

  bool HasListenersFor(nsAtom* aTypeWithOn) const;

  virtual nsPIDOMWindowOuter* GetOwnerGlobalForBindingsInternal() override {
    return nsPIDOMWindowOuter::GetFromCurrentInner(GetOwner());
  }

  // A global permanently becomes invalid when DisconnectEventTargetObjects() is
  // called.  Normally this means:
  // - For the main thread, when nsGlobalWindowInner::FreeInnerObjects is
  //   called.
  // - For a worker thread, when clearing the main event queue.  (Which we do
  //   slightly later than when the spec notionally calls for it to be done.)
  //
  // A global may also become temporarily invalid when:
  // - For the main thread, if the window is no longer the WindowProxy's current
  //   inner window due to being placed in the bfcache.
  nsresult CheckCurrentGlobalCorrectness() const;

  nsPIDOMWindowInner* GetOwner() const { return mOwnerWindow; }
  // Like GetOwner, but only returns non-null if the window being returned is
  // current (in the "current document" sense of the HTML spec).
  nsPIDOMWindowInner* GetWindowIfCurrent() const;
  // Returns the document associated with this event target, if that document is
  // the current document of its browsing context.  Will return null otherwise.
  mozilla::dom::Document* GetDocumentIfCurrent() const;

  virtual void DisconnectFromOwner();
  using EventTarget::GetParentObject;
  nsIGlobalObject* GetOwnerGlobal() const final { return mParentObject; }
  bool HasOrHasHadOwner() { return mHasOrHasHadOwnerWindow; }

  virtual void EventListenerAdded(nsAtom* aType) override;

  virtual void EventListenerRemoved(nsAtom* aType) override;

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
  virtual bool IsCertainlyAliveForCC() const { return mIsKeptAlive; }

  RefPtr<EventListenerManager> mListenerManager;
  // Make |event| trusted and dispatch |aEvent| to |this|.
  nsresult DispatchTrustedEvent(dom::Event* aEvent);

  virtual void LastRelease() {}

  void KeepAliveIfHasListenersFor(const nsAString& aType);
  void KeepAliveIfHasListenersFor(nsAtom* aType);

  void IgnoreKeepAliveIfHasListenersFor(const nsAString& aType);
  void IgnoreKeepAliveIfHasListenersFor(nsAtom* aType);

  void BindToOwner(nsIGlobalObject* aOwner);

 private:
  // The parent global object.  The global will clear this when
  // it is destroyed by calling DisconnectFromOwner().
  nsIGlobalObject* MOZ_NON_OWNING_REF mParentObject;
  // mParentObject pre QI-ed and cached (inner window)
  // (it is needed for off main thread access)
  // It is obtained in BindToOwner and reset in DisconnectFromOwner.
  nsPIDOMWindowInner* MOZ_NON_OWNING_REF mOwnerWindow;
  bool mHasOrHasHadOwnerWindow;

  struct {
    nsTArray<nsString> mStrings;
    nsTArray<RefPtr<nsAtom>> mAtoms;
  } mKeepingAliveTypes;

  bool mIsKeptAlive;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DOMEventTargetHelper, NS_DOMEVENTTARGETHELPER_IID)

}  // namespace mozilla

// WebIDL event handlers
#define IMPL_EVENT_HANDLER(_event)                                          \
  inline mozilla::dom::EventHandlerNonNull* GetOn##_event() {               \
    return GetEventHandler(nsGkAtoms::on##_event);                          \
  }                                                                         \
  inline void SetOn##_event(mozilla::dom::EventHandlerNonNull* aCallback) { \
    SetEventHandler(nsGkAtoms::on##_event, aCallback);                      \
  }

#endif  // mozilla_DOMEventTargetHelper_h_
