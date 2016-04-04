/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HardwareKeyHandler_h_
#define mozilla_HardwareKeyHandler_h_

#include "mozilla/EventForwards.h"          // for nsEventStatus
#include "mozilla/StaticPtr.h"
#include "mozilla/TextEvents.h"
#include "nsCOMPtr.h"
#include "nsDeque.h"
#include "nsIHardwareKeyHandler.h"
#include "nsIWeakReferenceUtils.h"          // for nsWeakPtr

class nsIContent;
class nsINode;
class nsIPresShell;
class nsPIDOMWindowOuter;
class nsPresContext;

namespace mozilla {

// This module will copy the events' data into its event queue for reuse
// after receiving input-method-app's reply, so we use the following struct
// for storing these information.
// RefCounted<T> is a helper class for adding reference counting mechanism.
struct KeyboardInfo : public RefCounted<KeyboardInfo>
{
  MOZ_DECLARE_REFCOUNTED_TYPENAME(KeyboardInfo)

  nsINode* mTarget;
  WidgetKeyboardEvent mEvent;
  nsEventStatus mStatus;

  KeyboardInfo(nsINode* aTarget,
               WidgetKeyboardEvent& aEvent,
               nsEventStatus aStatus)
    : mTarget(aTarget)
    , mEvent(aEvent)
    , mStatus(aStatus)
  {
  }
};

// The following is the type-safe wrapper around nsDeque
// for storing events' data.
// The T must be one class that supports reference counting mechanism.
// The EventQueueDeallocator will be called in nsDeque::~nsDeque() or
// nsDeque::Erase() to deallocate the objects. nsDeque::Erase() will remove
// and delete all items in the queue. See more from nsDeque.h.
template <class T>
class EventQueueDeallocator : public nsDequeFunctor
{
  virtual void* operator() (void* aObject)
  {
    RefPtr<T> releaseMe = dont_AddRef(static_cast<T*>(aObject));
    return nullptr;
  }
};

// The type-safe queue to be used to store the KeyboardInfo data
template <class T>
class EventQueue : private nsDeque
{
public:
  EventQueue()
    : nsDeque(new EventQueueDeallocator<T>())
  {
  };

  ~EventQueue()
  {
    Clear();
  }

  inline size_t GetSize()
  {
    return nsDeque::GetSize();
  }

  bool IsEmpty()
  {
    return !nsDeque::GetSize();
  }

  inline bool Push(T* aItem)
  {
    MOZ_ASSERT(aItem);
    NS_ADDREF(aItem);
    size_t sizeBefore = GetSize();
    nsDeque::Push(aItem);
    if (GetSize() != sizeBefore + 1) {
      NS_RELEASE(aItem);
      return false;
    }
    return true;
  }

  inline already_AddRefed<T> PopFront()
  {
    RefPtr<T> rv = dont_AddRef(static_cast<T*>(nsDeque::PopFront()));
    return rv.forget();
  }

  inline void RemoveFront()
  {
    RefPtr<T> releaseMe = PopFront();
  }

  inline T* PeekFront()
  {
    return static_cast<T*>(nsDeque::PeekFront());
  }

  void Clear()
  {
    while (GetSize() > 0) {
      RemoveFront();
    }
  }
};

class HardwareKeyHandler : public nsIHardwareKeyHandler
{
public:
  HardwareKeyHandler();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIHARDWAREKEYHANDLER

  static already_AddRefed<HardwareKeyHandler> GetInstance();

  virtual bool ForwardKeyToInputMethodApp(nsINode* aTarget,
                                          WidgetKeyboardEvent* aEvent,
                                          nsEventStatus* aEventStatus) override;

private:
  virtual ~HardwareKeyHandler();

  // Return true if the keypress is successfully dispatched.
  // Otherwise, return false.
  bool DispatchKeyPress(nsINode* aTarget,
                        WidgetKeyboardEvent& aEvent,
                        nsEventStatus& aStatus);

  void DispatchAfterKeyEvent(nsINode* aTarget, WidgetKeyboardEvent& aEvent);

  void DispatchToCurrentProcess(nsIPresShell* aPresShell,
                                nsIContent* aTarget,
                                WidgetKeyboardEvent& aEvent,
                                nsEventStatus& aStatus);

  bool DispatchToCrossProcess(nsINode* aTarget, WidgetKeyboardEvent& aEvent);

  // This method will dispatch not only key* event to its event target,
  // no mather it's in the current process or in its child process,
  // but also mozbrowserafterkey* to the corresponding target if it needs.
  // Return true if the key is successfully dispatched.
  // Otherwise, return false.
  bool DispatchToTargetApp(nsINode* aTarget,
                           WidgetKeyboardEvent& aEvent,
                           nsEventStatus& aStatus);

  // This method will be called after dispatching keypress to its target,
  // if the input-method-app doesn't handle the key.
  // In normal dispatching path, EventStateManager::PostHandleKeyboardEvent
  // will be called when event is keypress.
  // However, the ::PostHandleKeyboardEvent mentioned above will be aborted
  // when we try to forward key event to the input-method-app.
  // If the input-method-app consumes the key, then we don't need to do anything
  // because the input-method-app will generate a new key event by itself.
  // On the other hand, if the input-method-app doesn't consume the key,
  // then we need to dispatch the key event by ourselves
  // and call ::PostHandleKeyboardEvent again after the event is forwarded.
  // Note that the EventStateManager::PreHandleEvent is already called before
  // forwarding, so we don't need to call it in this module.
  void PostHandleKeyboardEvent(nsINode* aTarget,
                               WidgetKeyboardEvent& aEvent,
                               nsEventStatus& aStatus);

  void SetDefaultPrevented(WidgetKeyboardEvent& aEvent,
                           uint16_t aDefaultPrevented);

  // Check whether the event is valid to be fired.
  // This method should be called every time before dispatching next event.
  bool CanDispatchEvent(nsINode* aTarget,
                        WidgetKeyboardEvent& aEvent);

  already_AddRefed<nsPIDOMWindowOuter> GetRootWindow(nsINode* aNode);

  already_AddRefed<nsIContent> GetCurrentTarget();

  nsPresContext* GetPresContext(nsINode* aNode);

  already_AddRefed<nsIPresShell> GetPresShell(nsINode* aNode);

  static StaticRefPtr<HardwareKeyHandler> sInstance;

  // The event queue is used to store the forwarded keyboard events.
  // Those stored events will be dispatched if input-method-app doesn't
  // consume them.
  EventQueue<KeyboardInfo> mEventQueue;

  // Hold the pointer to the latest keydown's data
  RefPtr<KeyboardInfo> mLatestKeyDownInfo;

  // input-method-app needs to register a listener by
  // |nsIHardwareKeyHandler.registerListener| to receive
  // the hardware keyboard event, and |nsIHardwareKeyHandler.registerListener|
  // will set an nsIHardwareKeyEventListener to mHardwareKeyEventListener.
  // Then, mHardwareKeyEventListener is used to forward the event
  // to the input-method-app.
  nsWeakPtr mHardwareKeyEventListener;

  // To keep tracking the input-method-app is active or disabled.
  bool mInputMethodAppConnected;
};

} // namespace mozilla

#endif // #ifndef mozilla_HardwareKeyHandler_h_
