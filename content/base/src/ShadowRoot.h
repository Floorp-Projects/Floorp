/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_shadowroot_h__
#define mozilla_dom_shadowroot_h__

#include "mozilla/dom/DocumentFragment.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTHashtable.h"
#include "nsDocument.h"

class nsIAtom;
class nsIContent;
class nsIDocument;
class nsINodeInfo;
class nsPIDOMWindow;
class nsXBLPrototypeBinding;
class nsTagNameMapEntry;

namespace mozilla {
namespace dom {

class Element;
class HTMLContentElement;
class ShadowRootStyleSheetList;

class ShadowRoot : public DocumentFragment,
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

  ShadowRoot(nsIContent* aContent, already_AddRefed<nsINodeInfo> aNodeInfo,
             nsXBLPrototypeBinding* aProtoBinding);
  virtual ~ShadowRoot();

  void AddToIdTable(Element* aElement, nsIAtom* aId);
  void RemoveFromIdTable(Element* aElement, nsIAtom* aId);
  static bool PrefEnabled();
  void InsertSheet(nsCSSStyleSheet* aSheet, nsIContent* aLinkingContent);
  void RemoveSheet(nsCSSStyleSheet* aSheet);
  bool ApplyAuthorStyles();
  void SetApplyAuthorStyles(bool aApplyAuthorStyles);
  nsIDOMStyleSheetList* StyleSheets();

  /**
   * Distributes a single explicit child of the host to the content
   * insertion points in this ShadowRoot.
   */
  void DistributeSingleNode(nsIContent* aContent);

  /**
   * Removes a single explicit child of the host from the content
   * insertion points in this ShadowRoot.
   */
  void RemoveDistributedNode(nsIContent* aContent);

  /**
   * Distributes all the explicit children of the host to the content
   * insertion points in this ShadowRoot.
   */
  void DistributeAllNodes();

  void AddInsertionPoint(HTMLContentElement* aInsertionPoint);
  void RemoveInsertionPoint(HTMLContentElement* aInsertionPoint);

  void SetInsertionPointChanged() { mInsertionPointChanged = true; }

  void SetAssociatedBinding(nsXBLBinding* aBinding)
  {
    mAssociatedBinding = aBinding;
  }

  nsISupports* GetParentObject() const
  {
    return mHost;
  }

  nsIContent* GetHost() { return mHost; }

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

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
protected:
  void Restyle();

  nsCOMPtr<nsIContent> mHost;

  // An array of content insertion points that are a descendant of the ShadowRoot
  // sorted in tree order. Insertion points are responsible for notifying
  // the ShadowRoot when they are removed or added as a descendant. The insertion
  // points are kept alive by the parent node, thus weak references are held
  // by the array.
  nsTArray<HTMLContentElement*> mInsertionPoints;

  nsTHashtable<nsIdentifierMapEntry> mIdentifierMap;
  nsXBLPrototypeBinding* mProtoBinding;

  // It is necessary to hold a reference to the associated nsXBLBinding
  // because the binding holds a reference on the nsXBLDocumentInfo that
  // owns |mProtoBinding|.
  nsRefPtr<nsXBLBinding> mAssociatedBinding;

  nsRefPtr<ShadowRootStyleSheetList> mStyleSheetList;

  // A boolean that indicates that an insertion point was added or removed
  // from this ShadowRoot and that the nodes need to be redistributed into
  // the insertion points. After this flag is set, nodes will be distributed
  // on the next mutation event.
  bool mInsertionPointChanged;
};

class ShadowRootStyleSheetList : public nsIDOMStyleSheetList
{
public:
  ShadowRootStyleSheetList(ShadowRoot* aShadowRoot);
  virtual ~ShadowRootStyleSheetList();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ShadowRootStyleSheetList)

  // nsIDOMStyleSheetList
  NS_DECL_NSIDOMSTYLESHEETLIST

protected:
  nsRefPtr<ShadowRoot> mShadowRoot;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_shadowroot_h__

