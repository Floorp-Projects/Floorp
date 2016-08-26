/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RestyleManagerHandle_h
#define mozilla_RestyleManagerHandle_h

#include "mozilla/Assertions.h"
#include "mozilla/EventStates.h"
#include "mozilla/HandleRefPtr.h"
#include "mozilla/RefCountType.h"
#include "mozilla/StyleBackendType.h"
#include "nsChangeHint.h"

namespace mozilla {
class RestyleManager;
class ServoRestyleManager;
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla
class nsAttrValue;
class nsIAtom;
class nsIContent;
class nsIFrame;
class nsStyleChangeList;

namespace mozilla {

#define SERVO_BIT 0x1

/**
 * Smart pointer class that can hold a pointer to either a RestyleManager
 * or a ServoRestyleManager.
 */
class RestyleManagerHandle
{
public:
  typedef HandleRefPtr<RestyleManagerHandle> RefPtr;

  // We define this Ptr class with a RestyleManager API that forwards on to the
  // wrapped pointer, rather than putting these methods on RestyleManagerHandle
  // itself, so that we can have RestyleManagerHandle behave like a smart
  // pointer and be dereferenced with operator->.
  class Ptr
  {
  public:
    friend class ::mozilla::RestyleManagerHandle;

    bool IsGecko() const { return !IsServo(); }
    bool IsServo() const
    {
      MOZ_ASSERT(mValue, "RestyleManagerHandle null pointer dereference");
#ifdef MOZ_STYLO
      return mValue & SERVO_BIT;
#else
      return false;
#endif
    }

    StyleBackendType BackendType() const
    {
      return IsGecko() ? StyleBackendType::Gecko :
                         StyleBackendType::Servo;
    }

    RestyleManager* AsGecko()
    {
      MOZ_ASSERT(IsGecko());
      return reinterpret_cast<RestyleManager*>(mValue);
    }

    ServoRestyleManager* AsServo()
    {
      MOZ_ASSERT(IsServo());
      return reinterpret_cast<ServoRestyleManager*>(mValue & ~SERVO_BIT);
    }

    RestyleManager* GetAsGecko() { return IsGecko() ? AsGecko() : nullptr; }
    ServoRestyleManager* GetAsServo() { return IsServo() ? AsServo() : nullptr; }

    const RestyleManager* AsGecko() const
    {
      return const_cast<Ptr*>(this)->AsGecko();
    }

    const ServoRestyleManager* AsServo() const
    {
      MOZ_ASSERT(IsServo());
      return const_cast<Ptr*>(this)->AsServo();
    }

    const RestyleManager* GetAsGecko() const { return IsGecko() ? AsGecko() : nullptr; }
    const ServoRestyleManager* GetAsServo() const { return IsServo() ? AsServo() : nullptr; }

    // These inline methods are defined in RestyleManagerHandleInlines.h.
    inline MozExternalRefCountType AddRef();
    inline MozExternalRefCountType Release();

    // Restyle manager interface.  These inline methods are defined in
    // RestyleManagerHandleInlines.h and just forward to the underlying
    // RestyleManager or ServoRestyleManager.  See corresponding comments in
    // RestyleManager.h for descriptions of these methods.

    inline void Disconnect();
    inline void PostRestyleEvent(dom::Element* aElement,
                                 nsRestyleHint aRestyleHint,
                                 nsChangeHint aMinChangeHint);
    inline void PostRestyleEventForLazyConstruction();
    inline void RebuildAllStyleData(nsChangeHint aExtraHint,
                                    nsRestyleHint aRestyleHint);
    inline void PostRebuildAllStyleDataEvent(nsChangeHint aExtraHint,
                                             nsRestyleHint aRestyleHint);
    inline void ProcessPendingRestyles();
    inline void ContentInserted(nsINode* aContainer,
                                nsIContent* aChild);
    inline void ContentAppended(nsIContent* aContainer,
                                nsIContent* aFirstNewContent);
    inline void ContentRemoved(nsINode* aContainer,
                               nsIContent* aOldChild,
                               nsIContent* aFollowingSibling);
    inline void RestyleForInsertOrChange(nsINode* aContainer,
                                         nsIContent* aChild);
    inline void RestyleForAppend(nsIContent* aContainer,
                                 nsIContent* aFirstNewContent);
    inline nsresult ContentStateChanged(nsIContent* aContent,
                                        EventStates aStateMask);
    inline void AttributeWillChange(dom::Element* aElement,
                                    int32_t aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t aModType,
                                    const nsAttrValue* aNewValue);
    inline void AttributeChanged(dom::Element* aElement,
                                 int32_t aNameSpaceID,
                                 nsIAtom* aAttribute,
                                 int32_t aModType,
                                 const nsAttrValue* aOldValue);
    inline nsresult ReparentStyleContext(nsIFrame* aFrame);
    inline bool HasPendingRestyles();
    inline uint64_t GetRestyleGeneration() const;
    inline uint32_t GetHoverGeneration() const;
    inline void SetObservingRefreshDriver(bool aObserving);
    inline nsresult ProcessRestyledFrames(nsStyleChangeList& aChangeList);
    inline void FlushOverflowChangedTracker();
    inline void NotifyDestroyingFrame(nsIFrame* aFrame);

  private:
    // Stores a pointer to an RestyleManager or a ServoRestyleManager.  The least
    // significant bit is 0 for the former, 1 for the latter.  This is
    // valid as the least significant bit will never be used for a pointer
    // value on platforms we care about.
    uintptr_t mValue;
  };

  MOZ_IMPLICIT RestyleManagerHandle(decltype(nullptr) = nullptr)
  {
    mPtr.mValue = 0;
  }
  RestyleManagerHandle(const RestyleManagerHandle& aOth)
  {
    mPtr.mValue = aOth.mPtr.mValue;
  }
  MOZ_IMPLICIT RestyleManagerHandle(RestyleManager* aManager)
  {
    *this = aManager;
  }
  MOZ_IMPLICIT RestyleManagerHandle(ServoRestyleManager* aManager)
  {
    *this = aManager;
  }

  RestyleManagerHandle& operator=(decltype(nullptr))
  {
    mPtr.mValue = 0;
    return *this;
  }

  RestyleManagerHandle& operator=(RestyleManager* aManager)
  {
    MOZ_ASSERT(!(reinterpret_cast<uintptr_t>(aManager) & SERVO_BIT),
               "least significant bit shouldn't be set; we use it for state");
    mPtr.mValue = reinterpret_cast<uintptr_t>(aManager);
    return *this;
  }

  RestyleManagerHandle& operator=(ServoRestyleManager* aManager)
  {
#ifdef MOZ_STYLO
    MOZ_ASSERT(!(reinterpret_cast<uintptr_t>(aManager) & SERVO_BIT),
               "least significant bit shouldn't be set; we use it for state");
    mPtr.mValue =
      aManager ? (reinterpret_cast<uintptr_t>(aManager) | SERVO_BIT) : 0;
    return *this;
#else
    MOZ_CRASH("should not have a ServoRestyleManager object when MOZ_STYLO is "
              "disabled");
#endif
  }

  // Make RestyleManagerHandle usable in boolean contexts.
  explicit operator bool() const { return !!mPtr.mValue; }
  bool operator!() const { return !mPtr.mValue; }

  // Make RestyleManagerHandle behave like a smart pointer.
  Ptr* operator->() { return &mPtr; }
  const Ptr* operator->() const { return &mPtr; }

private:
  Ptr mPtr;
};

#undef SERVO_BIT

} // namespace mozilla

#endif // mozilla_RestyleManagerHandle_h
