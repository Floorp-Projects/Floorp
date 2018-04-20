/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SMILTIMEVALUESPEC_H_
#define NS_SMILTIMEVALUESPEC_H_

#include "mozilla/Attributes.h"
#include "mozilla/dom/IDTracker.h"
#include "nsSMILTimeValueSpecParams.h"
#include "nsStringFwd.h"
#include "nsIDOMEventListener.h"

class nsSMILTimeValue;
class nsSMILTimedElement;
class nsSMILTimeContainer;
class nsSMILInstanceTime;
class nsSMILInterval;

namespace mozilla {
namespace dom {
class Event;
} // namespace dom

class EventListenerManager;
} // namespace mozilla

//----------------------------------------------------------------------
// nsSMILTimeValueSpec class
//
// An individual element of a 'begin' or 'end' attribute, e.g. '5s', 'a.end'.
// This class handles the parsing of such specifications and performs the
// necessary event handling (for event and repeat specifications)
// and synchronisation (for syncbase specifications).
//
// For an overview of how this class is related to other SMIL time classes see
// the documentation in nsSMILTimeValue.h

class nsSMILTimeValueSpec
{
public:
  typedef mozilla::dom::Element Element;
  typedef mozilla::dom::Event Event;
  typedef mozilla::dom::IDTracker IDTracker;

  nsSMILTimeValueSpec(nsSMILTimedElement& aOwner, bool aIsBegin);
  ~nsSMILTimeValueSpec();

  nsresult SetSpec(const nsAString& aStringSpec, Element* aContextNode);
  void     ResolveReferences(nsIContent* aContextNode);
  bool     IsEventBased() const;

  void     HandleNewInterval(nsSMILInterval& aInterval,
                             const nsSMILTimeContainer* aSrcContainer);
  void     HandleTargetElementChange(Element* aNewTarget);

  // For created nsSMILInstanceTime objects
  bool     DependsOnBegin() const;
  void     HandleChangedInstanceTime(const nsSMILInstanceTime& aBaseTime,
                                     const nsSMILTimeContainer* aSrcContainer,
                                     nsSMILInstanceTime& aInstanceTimeToUpdate,
                                     bool aObjectChanged);
  void     HandleDeletedInstanceTime(nsSMILInstanceTime& aInstanceTime);

  // Cycle-collection support
  void Traverse(nsCycleCollectionTraversalCallback* aCallback);
  void Unlink();

protected:
  void UpdateReferencedElement(Element* aFrom, Element* aTo);
  void UnregisterFromReferencedElement(Element* aElement);
  nsSMILTimedElement* GetTimedElement(Element* aElement);
  bool IsWhitelistedEvent();
  void RegisterEventListener(Element* aElement);
  void UnregisterEventListener(Element* aElement);
  void HandleEvent(Event* aEvent);
  bool CheckRepeatEventDetail(Event* aEvent);
  nsSMILTimeValue ConvertBetweenTimeContainers(const nsSMILTimeValue& aSrcTime,
                                      const nsSMILTimeContainer* aSrcContainer);
  bool ApplyOffset(nsSMILTimeValue& aTime) const;

  nsSMILTimedElement*           mOwner;
  bool                          mIsBegin; // Indicates if *we* are a begin spec,
                                          // not to be confused with
                                          // mParams.mSyncBegin which indicates
                                          // if we're synced with the begin of
                                          // the target.
  nsSMILTimeValueSpecParams     mParams;

  /**
   * If our nsSMILTimeValueSpec exists for a 'begin' or 'end' attribute with a
   * value that specifies a time that is relative to the animation of some
   * other element, it will create an instance of this class to reference and
   * track that other element.  For example, if the nsSMILTimeValueSpec is for
   * end='a.end+2s', an instance of this class will be created to track the
   * element associated with the element ID "a".  This class will notify the
   * nsSMILTimeValueSpec if the element that that ID identifies changes to a
   * different element (or none).
   */
  class TimeReferenceTracker final : public IDTracker
  {
  public:
    explicit TimeReferenceTracker(nsSMILTimeValueSpec* aOwner)
      : mSpec(aOwner)
    {}
    void ResetWithElement(Element* aTo) {
      RefPtr<Element> from = get();
      Unlink();
      ElementChanged(from, aTo);
    }

  protected:
    virtual void ElementChanged(Element* aFrom, Element* aTo) override
    {
      IDTracker::ElementChanged(aFrom, aTo);
      mSpec->UpdateReferencedElement(aFrom, aTo);
    }
    virtual bool IsPersistent() override { return true; }
  private:
    nsSMILTimeValueSpec* mSpec;
  };

  TimeReferenceTracker mReferencedElement;

  class EventListener final : public nsIDOMEventListener
  {
    ~EventListener() {}
  public:
    explicit EventListener(nsSMILTimeValueSpec* aOwner) : mSpec(aOwner) { }
    void Disconnect()
    {
      mSpec = nullptr;
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOMEVENTLISTENER

  private:
    nsSMILTimeValueSpec* mSpec;
  };
  RefPtr<EventListener> mEventListener;
};

#endif // NS_SMILTIMEVALUESPEC_H_
