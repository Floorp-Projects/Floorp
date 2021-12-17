/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XULBroadcastManager_h
#define mozilla_dom_XULBroadcastManager_h

#include "nsAtom.h"
#include "nsTArray.h"

class PLDHashTable;
class nsXULElement;

namespace mozilla {

class ErrorResult;

namespace dom {

class Document;
class Element;

class XULBroadcastManager final {
 public:
  explicit XULBroadcastManager(Document* aDocument);

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
  // TODO: Convert this to MOZ_CAN_RUN_SCRIPT (bug 1415230)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void MaybeBroadcast();
  void DropDocumentReference();  // notification that doc is going away
 protected:
  enum HookupAction { eHookupAdd = 0, eHookupRemove };

  nsresult UpdateListenerHookup(Element* aElement, HookupAction aAction);

  void RemoveListenerFor(Element& aBroadcaster, Element& aListener,
                         const nsAString& aAttr);
  void AddListenerFor(Element& aBroadcaster, Element& aListener,
                      const nsAString& aAttr, ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT nsresult ExecuteOnBroadcastHandlerFor(
      Element* aBroadcaster, Element* aListener, nsAtom* aAttr);
  // The out params of FindBroadcaster only have values that make sense when
  // the method returns NS_FINDBROADCASTER_FOUND.  In all other cases, the
  // values of the out params should not be relied on (though *aListener and
  // *aBroadcaster do need to be released if non-null, of course).
  nsresult FindBroadcaster(Element* aElement, Element** aListener,
                           nsString& aBroadcasterID, nsString& aAttribute,
                           Element** aBroadcaster);

  void SynchronizeBroadcastListener(Element* aBroadcaster, Element* aListener,
                                    const nsAString& aAttr);

  // This reference is nulled by the Document in it's destructor through
  // DropDocumentReference().
  Document* MOZ_NON_OWNING_REF mDocument;

  /**
   * A map from a broadcaster element to a list of listener elements.
   */
  PLDHashTable* mBroadcasterMap;

  class nsDelayedBroadcastUpdate;
  nsTArray<nsDelayedBroadcastUpdate> mDelayedBroadcasters;
  nsTArray<nsDelayedBroadcastUpdate> mDelayedAttrChangeBroadcasts;
  bool mHandlingDelayedAttrChange;
  bool mHandlingDelayedBroadcasters;

 private:
  ~XULBroadcastManager();
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_XULBroadcastManager_h
