/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XULBroadcastManager_h
#define mozilla_dom_XULBroadcastManager_h

#include "mozilla/dom/Element.h"
#include "nsAtom.h"

class nsXULElement;

namespace mozilla {
namespace dom {

class XULBroadcastManager final {

public:
  typedef mozilla::dom::Element Element;

  explicit XULBroadcastManager(nsIDocument* aDocument);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(XULBroadcastManager)

  /**
   * Checks whether an element uses any of the special broadcaster attributes
   * or is an observes element. This mimics the logic in FindBroadcaster, but
   * is intended to be a lighter weight check and doesn't actually guarantee
   * that the element will need a listener.
   */
  static bool MayNeedListener(const Element& aElement);

  nsresult AddListener(Element* aElement);
  nsresult RemoveListener(Element* aElement);
  void AttributeChanged(Element* aElement, int32_t aNameSpaceID,
                        nsAtom* aAttribute);
  void MaybeBroadcast();
  void DropDocumentReference(); // notification that doc is going away
protected:

   enum HookupAction {
    eHookupAdd = 0,
    eHookupRemove
  };

  nsresult UpdateListenerHookup(Element* aElement, HookupAction aAction);

  void RemoveListenerFor(Element& aBroadcaster, Element& aListener,
                         const nsAString& aAttr);
  void AddListenerFor(Element& aBroadcaster, Element& aListener,
                      const nsAString& aAttr, ErrorResult& aRv);

  nsresult
  ExecuteOnBroadcastHandlerFor(Element* aBroadcaster,
                               Element* aListener,
                               nsAtom* aAttr);
  // The out params of FindBroadcaster only have values that make sense when
  // the method returns NS_FINDBROADCASTER_FOUND.  In all other cases, the
  // values of the out params should not be relied on (though *aListener and
  // *aBroadcaster do need to be released if non-null, of course).
  nsresult
  FindBroadcaster(Element* aElement,
                  Element** aListener,
                  nsString& aBroadcasterID,
                  nsString& aAttribute,
                  Element** aBroadcaster);

  void
  SynchronizeBroadcastListener(Element *aBroadcaster,
                               Element *aListener,
                               const nsAString &aAttr);


  // This reference is nulled by the Document in it's destructor through
  // DropDocumentReference().
  nsIDocument* MOZ_NON_OWNING_REF mDocument;

  /**
   * A map from a broadcaster element to a list of listener elements.
   */
  PLDHashTable* mBroadcasterMap;

  class nsDelayedBroadcastUpdate
  {
  public:
    nsDelayedBroadcastUpdate(Element* aBroadcaster,
                             Element* aListener,
                             const nsAString &aAttr)
    : mBroadcaster(aBroadcaster), mListener(aListener), mAttr(aAttr),
      mSetAttr(false), mNeedsAttrChange(false) {}

    nsDelayedBroadcastUpdate(Element* aBroadcaster,
                             Element* aListener,
                             nsAtom* aAttrName,
                             const nsAString &aAttr,
                             bool aSetAttr,
                             bool aNeedsAttrChange)
    : mBroadcaster(aBroadcaster), mListener(aListener), mAttr(aAttr),
      mAttrName(aAttrName), mSetAttr(aSetAttr),
      mNeedsAttrChange(aNeedsAttrChange) {}

    nsDelayedBroadcastUpdate(const nsDelayedBroadcastUpdate& aOther)
    : mBroadcaster(aOther.mBroadcaster), mListener(aOther.mListener),
      mAttr(aOther.mAttr), mAttrName(aOther.mAttrName),
      mSetAttr(aOther.mSetAttr), mNeedsAttrChange(aOther.mNeedsAttrChange) {}

    nsCOMPtr<Element>       mBroadcaster;
    nsCOMPtr<Element>       mListener;
    // Note if mAttrName isn't used, this is the name of the attr, otherwise
    // this is the value of the attribute.
    nsString                mAttr;
    RefPtr<nsAtom>       mAttrName;
    bool                    mSetAttr;
    bool                    mNeedsAttrChange;

    class Comparator {
      public:
        static bool Equals(const nsDelayedBroadcastUpdate& a, const nsDelayedBroadcastUpdate& b) {
          return a.mBroadcaster == b.mBroadcaster && a.mListener == b.mListener && a.mAttrName == b.mAttrName;
        }
    };
  };
  nsTArray<nsDelayedBroadcastUpdate> mDelayedBroadcasters;
  nsTArray<nsDelayedBroadcastUpdate> mDelayedAttrChangeBroadcasts;
  bool                               mHandlingDelayedAttrChange;
  bool                               mHandlingDelayedBroadcasters;

private:
  ~XULBroadcastManager();


};

} // namespace dom
} // namespace mozilla


#endif // mozilla_dom_XULBroadcastManager_h
