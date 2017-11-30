/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_shadowroot_h__
#define mozilla_dom_shadowroot_h__

#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/StyleSheetList.h"
#include "mozilla/StyleSheet.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIContentInlines.h"
#include "nsIdentifierMapEntry.h"
#include "nsTHashtable.h"

class nsAtom;
class nsIContent;
class nsXBLPrototypeBinding;

namespace mozilla {
namespace dom {

class Element;
class ShadowRootStyleSheetList;

class ShadowRoot final : public DocumentFragment,
                         public nsStubMutationObserver
{
  friend class ShadowRootStyleSheetList;
public:
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
  ShadowRootMode Mode()
  {
    return mMode;
  }
  bool IsClosed()
  {
    return mMode == ShadowRootMode::Closed;
  }

  // [deprecated] Shadow DOM v0
  void AddToIdTable(Element* aElement, nsAtom* aId);
  void RemoveFromIdTable(Element* aElement, nsAtom* aId);
  void InsertSheet(StyleSheet* aSheet, nsIContent* aLinkingContent);
  void RemoveSheet(StyleSheet* aSheet);
  bool ApplyAuthorStyles();
  void SetApplyAuthorStyles(bool aApplyAuthorStyles);
  StyleSheetList* StyleSheets();

  /**
   * Distributes all the explicit children of the pool host to the content
   * insertion points in this ShadowRoot.
   */
  void DistributeAllNodes();

private:

  /**
   * Redistributes a node of the pool, and returns whether the distribution
   * changed.
   */
  bool RedistributeElement(Element*);

  /**
   * Called when we redistribute content after insertion points have changed.
   */
  void DistributionChanged();

  bool IsPooledNode(nsIContent* aChild) const;

public:
  void SetInsertionPointChanged() { mInsertionPointChanged = true; }

  void SetAssociatedBinding(nsXBLBinding* aBinding) { mAssociatedBinding = aBinding; }

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static ShadowRoot* FromNode(nsINode* aNode);

  // WebIDL methods.
  Element* GetElementById(const nsAString& aElementId);
  already_AddRefed<nsContentList>
    GetElementsByTagName(const nsAString& aNamespaceURI);
  already_AddRefed<nsContentList>
    GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                           const nsAString& aLocalName);
  already_AddRefed<nsContentList>
    GetElementsByClassName(const nsAString& aClasses);
  void GetInnerHTML(nsAString& aInnerHTML);
  void SetInnerHTML(const nsAString& aInnerHTML, ErrorResult& aError);
  void StyleSheetChanged();

  bool IsComposedDocParticipant() { return mIsComposedDocParticipant; }
  void SetIsComposedDocParticipant(bool aIsComposedDocParticipant)
  {
    mIsComposedDocParticipant = aIsComposedDocParticipant;
  }

protected:
  virtual ~ShadowRoot();

  ShadowRootMode mMode;

  nsTHashtable<nsIdentifierMapEntry> mIdentifierMap;
  nsXBLPrototypeBinding* mProtoBinding;

  // It is necessary to hold a reference to the associated nsXBLBinding
  // because the binding holds a reference on the nsXBLDocumentInfo that
  // owns |mProtoBinding|.
  RefPtr<nsXBLBinding> mAssociatedBinding;

  RefPtr<ShadowRootStyleSheetList> mStyleSheetList;

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

class ShadowRootStyleSheetList : public StyleSheetList
{
public:
  explicit ShadowRootStyleSheetList(ShadowRoot* aShadowRoot);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ShadowRootStyleSheetList, StyleSheetList)

  virtual nsINode* GetParentObject() const override
  {
    return mShadowRoot;
  }

  uint32_t Length() override;
  StyleSheet* IndexedGetter(uint32_t aIndex, bool& aFound) override;

protected:
  virtual ~ShadowRootStyleSheetList();

  RefPtr<ShadowRoot> mShadowRoot;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_shadowroot_h__

