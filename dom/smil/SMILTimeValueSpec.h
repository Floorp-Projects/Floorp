/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SMILTimeValueSpec_h
#define mozilla_SMILTimeValueSpec_h

#include "mozilla/Attributes.h"
#include "mozilla/SMILTimeValueSpecParams.h"
#include "mozilla/dom/IDTracker.h"
#include "nsStringFwd.h"
#include "nsIDOMEventListener.h"

namespace mozilla {

class EventListenerManager;
class SMILInstanceTime;
class SMILInterval;
class SMILTimeContainer;
class SMILTimedElement;
class SMILTimeValue;

namespace dom {
class Event;
}  // namespace dom

//----------------------------------------------------------------------
// SMILTimeValueSpec class
//
// An individual element of a 'begin' or 'end' attribute, e.g. '5s', 'a.end'.
// This class handles the parsing of such specifications and performs the
// necessary event handling (for event and repeat specifications)
// and synchronisation (for syncbase specifications).
//
// For an overview of how this class is related to other SMIL time classes see
// the documentation in SMILTimeValue.h

class SMILTimeValueSpec {
 public:
  typedef mozilla::dom::Element Element;
  typedef mozilla::dom::Event Event;
  typedef mozilla::dom::IDTracker IDTracker;

  SMILTimeValueSpec(SMILTimedElement& aOwner, bool aIsBegin);
  ~SMILTimeValueSpec();

  nsresult SetSpec(const nsAString& aStringSpec, Element& aContextElement);
  void ResolveReferences(Element& aContextElement);
  bool IsEventBased() const;

  void HandleNewInterval(SMILInterval& aInterval,
                         const SMILTimeContainer* aSrcContainer);
  void HandleTargetElementChange(Element* aNewTarget);

  // For created SMILInstanceTime objects
  bool DependsOnBegin() const;
  void HandleChangedInstanceTime(const SMILInstanceTime& aBaseTime,
                                 const SMILTimeContainer* aSrcContainer,
                                 SMILInstanceTime& aInstanceTimeToUpdate,
                                 bool aObjectChanged);
  void HandleDeletedInstanceTime(SMILInstanceTime& aInstanceTime);

  // Cycle-collection support
  void Traverse(nsCycleCollectionTraversalCallback* aCallback);
  void Unlink();

 protected:
  void UpdateReferencedElement(Element* aFrom, Element* aTo);
  void UnregisterFromReferencedElement(Element* aElement);
  SMILTimedElement* GetTimedElement(Element* aElement);
  bool IsWhitelistedEvent();
  void RegisterEventListener(Element* aTarget);
  void UnregisterEventListener(Element* aTarget);
  void HandleEvent(Event* aEvent);
  bool CheckRepeatEventDetail(Event* aEvent);
  SMILTimeValue ConvertBetweenTimeContainers(
      const SMILTimeValue& aSrcTime, const SMILTimeContainer* aSrcContainer);
  bool ApplyOffset(SMILTimeValue& aTime) const;

  SMILTimedElement* mOwner;
  bool mIsBegin;  // Indicates if *we* are a begin spec,
                  // not to be confused with
                  // mParams.mSyncBegin which indicates
                  // if we're synced with the begin of
                  // the target.
  SMILTimeValueSpecParams mParams;

  /**
   * If our SMILTimeValueSpec exists for a 'begin' or 'end' attribute with a
   * value that specifies a time that is relative to the animation of some
   * other element, it will create an instance of this class to reference and
   * track that other element.  For example, if the SMILTimeValueSpec is for
   * end='a.end+2s', an instance of this class will be created to track the
   * element associated with the element ID "a".  This class will notify the
   * SMILTimeValueSpec if the element that that ID identifies changes to a
   * different element (or none).
   */
  class TimeReferenceTracker final : public IDTracker {
   public:
    explicit TimeReferenceTracker(SMILTimeValueSpec* aOwner) : mSpec(aOwner) {}
    void ResetWithElement(Element* aTo) {
      RefPtr<Element> from = get();
      Unlink();
      ElementChanged(from, aTo);
    }

   protected:
    virtual void ElementChanged(Element* aFrom, Element* aTo) override {
      IDTracker::ElementChanged(aFrom, aTo);
      mSpec->UpdateReferencedElement(aFrom, aTo);
    }
    virtual bool IsPersistent() override { return true; }

   private:
    SMILTimeValueSpec* mSpec;
  };

  TimeReferenceTracker mReferencedElement;

  class EventListener final : public nsIDOMEventListener {
    ~EventListener() {}

   public:
    explicit EventListener(SMILTimeValueSpec* aOwner) : mSpec(aOwner) {}
    void Disconnect() { mSpec = nullptr; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOMEVENTLISTENER

   private:
    SMILTimeValueSpec* mSpec;
  };
  RefPtr<EventListener> mEventListener;
};

}  // namespace mozilla

#endif  // mozilla_SMILTimeValueSpec_h
