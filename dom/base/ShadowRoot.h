/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_shadowroot_h__
#define mozilla_dom_shadowroot_h__

#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/DocumentOrShadowRoot.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIdentifierMapEntry.h"
#include "nsTHashtable.h"

class nsAtom;
class nsIContent;
class nsXBLPrototypeBinding;

namespace mozilla {

class EventChainPreVisitor;

namespace dom {

class Element;

class ShadowRoot final : public DocumentFragment,
                         public DocumentOrShadowRoot,
                         public nsStubMutationObserver
{
public:
  static ShadowRoot* FromNode(nsINode* aNode)
  {
    return aNode->IsShadowRoot() ? static_cast<ShadowRoot*>(aNode) : nullptr;
  }

  static const ShadowRoot* FromNode(const nsINode* aNode)
  {
    return aNode->IsShadowRoot() ? static_cast<const ShadowRoot*>(aNode) : nullptr;
  }

  static ShadowRoot* FromNodeOrNull(nsINode* aNode)
  {
    return aNode ? FromNode(aNode) : nullptr;
  }

  static const ShadowRoot* FromNodeOrNull(const nsINode* aNode)
  {
    return aNode ? FromNode(aNode) : nullptr;
  }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ShadowRoot,
                                           DocumentFragment)
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  ShadowRoot(Element* aElement, bool aClosed,
             already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
             nsXBLPrototypeBinding* aProtoBinding);

  // Shadow DOM v1
  Element* Host();
  ShadowRootMode Mode() const
  {
    return mMode;
  }
  bool IsClosed() const
  {
    return mMode == ShadowRootMode::Closed;
  }

  // [deprecated] Shadow DOM v0
  void InsertSheet(StyleSheet* aSheet, nsIContent* aLinkingContent);
  void RemoveSheet(StyleSheet* aSheet);
  bool ApplyAuthorStyles();
  void SetApplyAuthorStyles(bool aApplyAuthorStyles);
  StyleSheetList* StyleSheets()
  {
    return &DocumentOrShadowRoot::EnsureDOMStyleSheets();
  }

  /**
   * Distributes all the explicit children of the pool host to the content
   * insertion points in this ShadowRoot.
   */
  void DistributeAllNodes();

private:

  /**
   * Try to reassign an element to a slot and returns whether the assignment
   * changed.
   */
  bool MaybeReassignElement(Element* aElement, const nsAttrValue* aOldValue);

  /**
   * Try to assign aContent to a slot in the shadow tree, returns the assigned
   * slot if found.
   */
  const HTMLSlotElement* AssignSlotFor(nsIContent* aContent);

  /**
   * Unassign aContent from the assigned slot in the shadow tree, returns the
   * assigned slot if found.
   *
   * Note: slot attribute of aContent may have changed already, so pass slot
   *       name explicity here.
   */
  const HTMLSlotElement* UnassignSlotFor(nsIContent* aContent,
                                         const nsAString& aSlotName);

  /**
   * Called when we redistribute content after insertion points have changed.
   */
  void DistributionChanged();

  bool IsPooledNode(nsIContent* aChild) const;

public:
  void AddSlot(HTMLSlotElement* aSlot);
  void RemoveSlot(HTMLSlotElement* aSlot);

  void SetInsertionPointChanged() { mInsertionPointChanged = true; }

  void SetAssociatedBinding(nsXBLBinding* aBinding) { mAssociatedBinding = aBinding; }

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void AddToIdTable(Element* aElement, nsAtom* aId);
  void RemoveFromIdTable(Element* aElement, nsAtom* aId);

  // WebIDL methods.
  using mozilla::dom::DocumentOrShadowRoot::GetElementById;
  void GetInnerHTML(nsAString& aInnerHTML);
  void SetInnerHTML(const nsAString& aInnerHTML, ErrorResult& aError);
  void StyleSheetChanged();

  bool IsComposedDocParticipant() { return mIsComposedDocParticipant; }
  void SetIsComposedDocParticipant(bool aIsComposedDocParticipant)
  {
    mIsComposedDocParticipant = aIsComposedDocParticipant;
  }

  nsresult GetEventTargetParent(EventChainPreVisitor& aVisitor) override;

protected:
  virtual ~ShadowRoot();

  ShadowRootMode mMode;

  // Map from name of slot to an array of all slots in the shadow DOM with with
  // the given name. The slots are stored as a weak pointer because the elements
  // are in the shadow tree and should be kept alive by its parent.
  nsClassHashtable<nsStringHashKey, nsTArray<mozilla::dom::HTMLSlotElement*>> mSlotMap;
  nsXBLPrototypeBinding* mProtoBinding;

  // It is necessary to hold a reference to the associated nsXBLBinding
  // because the binding holds a reference on the nsXBLDocumentInfo that
  // owns |mProtoBinding|.
  RefPtr<nsXBLBinding> mAssociatedBinding;

  // A boolean that indicates that an insertion point was added or removed
  // from this ShadowRoot and that the nodes need to be redistributed into
  // the insertion points. After this flag is set, nodes will be distributed
  // on the next mutation event.
  bool mInsertionPointChanged;

  // Flag to indicate whether the descendants of this shadow root are part of the
  // composed document. Ideally, we would use a node flag on nodes to
  // mark whether it is in the composed document, but we have run out of flags
  // so instead we track it here.
  bool mIsComposedDocParticipant;

  nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                 bool aPreallocateChildren) const override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_shadowroot_h__

