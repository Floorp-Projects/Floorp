/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SMILTIMEVALUESPEC_H_
#define NS_SMILTIMEVALUESPEC_H_

#include "mozilla/Attributes.h"
#include "nsSMILTimeValueSpecParams.h"
#include "nsReferencedElement.h"
#include "nsAutoPtr.h"
#include "nsIDOMEventListener.h"

class nsAString;
class nsSMILTimeValue;
class nsSMILTimedElement;
class nsSMILTimeContainer;
class nsSMILInstanceTime;
class nsSMILInterval;

namespace mozilla {
class EventListenerManager;
} // namespace mozilla

//----------------------------------------------------------------------
// nsSMILTimeValueSpec class
//
// An individual element of a 'begin' or 'end' attribute, e.g. '5s', 'a.end'.
// This class handles the parsing of such specifications and performs the
// necessary event handling (for event, repeat, and accesskey specifications)
// and synchronisation (for syncbase specifications).
//
// For an overview of how this class is related to other SMIL time classes see
// the documentation in nsSMILTimeValue.h

class nsSMILTimeValueSpec
{
public:
  typedef mozilla::dom::Element Element;

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
  mozilla::EventListenerManager* GetEventListenerManager(Element* aElement);
  void HandleEvent(nsIDOMEvent* aEvent);
  bool CheckEventDetail(nsIDOMEvent* aEvent);
  bool CheckRepeatEventDetail(nsIDOMEvent* aEvent);
  bool CheckAccessKeyEventDetail(nsIDOMEvent* aEvent);
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

  class TimeReferenceElement : public nsReferencedElement
  {
  public:
    explicit TimeReferenceElement(nsSMILTimeValueSpec* aOwner) : mSpec(aOwner) { }
    void ResetWithElement(Element* aTo) {
      nsRefPtr<Element> from = get();
      Unlink();
      ElementChanged(from, aTo);
    }

  protected:
    virtual void ElementChanged(Element* aFrom, Element* aTo) MOZ_OVERRIDE
    {
      nsReferencedElement::ElementChanged(aFrom, aTo);
      mSpec->UpdateReferencedElement(aFrom, aTo);
    }
    virtual bool IsPersistent() MOZ_OVERRIDE { return true; }
  private:
    nsSMILTimeValueSpec* mSpec;
  };

  TimeReferenceElement mReferencedElement;

  class EventListener MOZ_FINAL : public nsIDOMEventListener
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
  nsRefPtr<EventListener> mEventListener;
};

#endif // NS_SMILTIMEVALUESPEC_H_
