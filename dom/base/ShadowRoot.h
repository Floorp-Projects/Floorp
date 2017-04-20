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
#include "nsTHashtable.h"
#include "nsDocument.h"

class nsIAtom;
class nsIContent;
class nsXBLPrototypeBinding;

namespace mozilla {
namespace dom {

class Element;
class HTMLContentElement;
class HTMLShadowElement;
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

  ShadowRoot(nsIContent* aContent, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
             nsXBLPrototypeBinding* aProtoBinding);

  void AddToIdTable(Element* aElement, nsIAtom* aId);
  void RemoveFromIdTable(Element* aElement, nsIAtom* aId);
  void InsertSheet(StyleSheet* aSheet, nsIContent* aLinkingContent);
  void RemoveSheet(StyleSheet* aSheet);
  bool ApplyAuthorStyles();
  void SetApplyAuthorStyles(bool aApplyAuthorStyles);
  StyleSheetList* StyleSheets();
  HTMLShadowElement* GetShadowElement() { return mShadowElement; }

  /**
   * Sets the current shadow insertion point where the older
   * ShadowRoot will be projected.
   */
  void SetShadowElement(HTMLShadowElement* aShadowElement);

  /**
   * Change the node that populates the distribution pool with
   * its children. This is distinct from the ShadowRoot host described
   * in the specifications. The ShadowRoot host is the element
   * which created this ShadowRoot and does not change. The pool host
   * is the same as the ShadowRoot host if this is the youngest
   * ShadowRoot. If this is an older ShadowRoot, the pool host is
   * the <shadow> element in the younger ShadowRoot (if it exists).
   */
  void ChangePoolHost(nsIContent* aNewHost);

  /**
   * Distributes a single explicit child of the pool host to the content
   * insertion points in this ShadowRoot.
   */
  void DistributeSingleNode(nsIContent* aContent);

  /**
   * Removes a single explicit child of the pool host from the content
   * insertion points in this ShadowRoot.
   */
  void RemoveDistributedNode(nsIContent* aContent);

  /**
   * Distributes all the explicit children of the pool host to the content
   * insertion points in this ShadowRoot.
   */
  void DistributeAllNodes();

  void AddInsertionPoint(HTMLContentElement* aInsertionPoint);
  void RemoveInsertionPoint(HTMLContentElement* aInsertionPoint);

  void SetYoungerShadow(ShadowRoot* aYoungerShadow);
  ShadowRoot* GetYoungerShadowRoot() { return mYoungerShadow; }
  void SetInsertionPointChanged() { mInsertionPointChanged = true; }

  void SetAssociatedBinding(nsXBLBinding* aBinding) { mAssociatedBinding = aBinding; }

  nsISupports* GetParentObject() const { return mPoolHost; }

  nsIContent* GetPoolHost() { return mPoolHost; }
  nsTArray<HTMLShadowElement*>& ShadowDescendants() { return mShadowDescendants; }

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static bool IsPooledNode(nsIContent* aChild, nsIContent* aContainer,
                           nsIContent* aHost);
  static ShadowRoot* FromNode(nsINode* aNode);
  static bool IsShadowInsertionPoint(nsIContent* aContent);

  static void RemoveDestInsertionPoint(nsIContent* aInsertionPoint,
                                       nsTArray<nsIContent*>& aDestInsertionPoints);

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
  Element* Host();
  ShadowRoot* GetOlderShadowRoot() { return mOlderShadow; }
  void StyleSheetChanged();

  bool IsComposedDocParticipant() { return mIsComposedDocParticipant; }
  void SetIsComposedDocParticipant(bool aIsComposedDocParticipant)
  {
    mIsComposedDocParticipant = aIsComposedDocParticipant;
  }

  virtual void DestroyContent() override;
protected:
  virtual ~ShadowRoot();

  // The pool host is the parent of the nodes that will be distributed
  // into the insertion points in this ShadowRoot. See |ChangeShadowRoot|.
  nsCOMPtr<nsIContent> mPoolHost;

  // An array of content insertion points that are a descendant of the ShadowRoot
  // sorted in tree order. Insertion points are responsible for notifying
  // the ShadowRoot when they are removed or added as a descendant. The insertion
  // points are kept alive by the parent node, thus weak references are held
  // by the array.
  nsTArray<HTMLContentElement*> mInsertionPoints;

  // An array of the <shadow> elements that are descendant of the ShadowRoot
  // sorted in tree order. Only the first may be a shadow insertion point.
  nsTArray<HTMLShadowElement*> mShadowDescendants;

  nsTHashtable<nsIdentifierMapEntry> mIdentifierMap;
  nsXBLPrototypeBinding* mProtoBinding;

  // It is necessary to hold a reference to the associated nsXBLBinding
  // because the binding holds a reference on the nsXBLDocumentInfo that
  // owns |mProtoBinding|.
  RefPtr<nsXBLBinding> mAssociatedBinding;

  RefPtr<ShadowRootStyleSheetList> mStyleSheetList;

  // The current shadow insertion point of this ShadowRoot.
  HTMLShadowElement* mShadowElement;

  // The ShadowRoot that was created by the host element before
  // this ShadowRoot was created.
  RefPtr<ShadowRoot> mOlderShadow;

  // The ShadowRoot that was created by the host element after
  // this ShadowRoot was created.
  RefPtr<ShadowRoot> mYoungerShadow;

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

