/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HTMLPropertiesCollection_h_
#define HTMLPropertiesCollection_h_

#include "mozilla/Attributes.h"
#include "mozilla/dom/DOMStringList.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsIMutationObserver.h"
#include "nsStubMutationObserver.h"
#include "nsBaseHashtable.h"
#include "nsINodeList.h"
#include "nsIHTMLCollection.h"
#include "nsHashKeys.h"
#include "nsRefPtrHashtable.h"
#include "nsGenericHTMLElement.h"

class nsIDocument;
class nsINode;

namespace mozilla {
namespace dom {

class HTMLPropertiesCollection;
class PropertyNodeList;
class Element;

class PropertyStringList : public DOMStringList
{
public:
  PropertyStringList(HTMLPropertiesCollection* aCollection);
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PropertyStringList, DOMStringList)

  bool ContainsInternal(const nsAString& aString);

protected:
  virtual ~PropertyStringList();

  virtual void EnsureFresh() MOZ_OVERRIDE;

  nsRefPtr<HTMLPropertiesCollection> mCollection;
};

class HTMLPropertiesCollection : public nsIHTMLCollection,
                                 public nsStubMutationObserver,
                                 public nsWrapperCache
{
  friend class PropertyNodeList;
  friend class PropertyStringList;
public:
  HTMLPropertiesCollection(nsGenericHTMLElement* aRoot);

  // nsWrapperCache
  using nsWrapperCache::GetWrapperPreserveColor;
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;
protected:
  virtual ~HTMLPropertiesCollection();

  virtual JSObject* GetWrapperPreserveColorInternal() MOZ_OVERRIDE
  {
    return nsWrapperCache::GetWrapperPreserveColor();
  }
public:

  virtual Element* GetElementAt(uint32_t aIndex);

  void SetDocument(nsIDocument* aDocument);
  nsINode* GetParentObject() MOZ_OVERRIDE;

  virtual Element*
  GetFirstNamedElement(const nsAString& aName, bool& aFound) MOZ_OVERRIDE
  {
    // HTMLPropertiesCollection.namedItem and the named getter call the
    // NamedItem that returns a PropertyNodeList, calling
    // HTMLCollection.namedItem doesn't make sense so this returns null.
    aFound = false;
    return nullptr;
  }
  PropertyNodeList* NamedItem(const nsAString& aName);
  PropertyNodeList* NamedGetter(const nsAString& aName, bool& aFound)
  {
    aFound = IsSupportedNamedProperty(aName);
    return aFound ? NamedItem(aName) : nullptr;
  }
  bool NameIsEnumerable(const nsAString& aName)
  {
    return true;
  }
  DOMStringList* Names()
  {
    EnsureFresh();
    return mNames;
  }
  virtual void GetSupportedNames(unsigned,
                                 nsTArray<nsString>& aNames) MOZ_OVERRIDE;

  NS_DECL_NSIDOMHTMLCOLLECTION

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(HTMLPropertiesCollection,
                                                         nsIHTMLCollection)

protected:
  // Make sure this collection is up to date, in case the DOM has been mutated.
  void EnsureFresh();

  // Crawl the properties of mRoot, following any itemRefs it may have
  void CrawlProperties();

  // Crawl startNode and its descendants, looking for items
  void CrawlSubtree(Element* startNode);

  bool IsSupportedNamedProperty(const nsAString& aName)
  {
    EnsureFresh();
    return mNames->ContainsInternal(aName);
  }

  // the items that make up this collection
  nsTArray<nsRefPtr<nsGenericHTMLElement> > mProperties;

  // the itemprop attribute of the properties
  nsRefPtr<PropertyStringList> mNames;

  // The cached PropertyNodeLists that are NamedItems of this collection
  nsRefPtrHashtable<nsStringHashKey, PropertyNodeList> mNamedItemEntries;

  // The element this collection is rooted at
  nsRefPtr<nsGenericHTMLElement> mRoot;

  // The document mRoot is in, if any
  nsCOMPtr<nsIDocument> mDoc;

  // True if there have been DOM modifications since the last EnsureFresh call.
  bool mIsDirty;
};

class PropertyNodeList : public nsINodeList,
                         public nsStubMutationObserver
{
public:
  PropertyNodeList(HTMLPropertiesCollection* aCollection,
                   nsIContent* aRoot, const nsAString& aName);

  virtual JSObject* WrapObject(JSContext *cx) MOZ_OVERRIDE;

  void SetDocument(nsIDocument* aDocument);

  void GetValues(JSContext* aCx, nsTArray<JS::Value >& aResult,
                 ErrorResult& aError);

  virtual nsIContent* Item(uint32_t aIndex) MOZ_OVERRIDE;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(PropertyNodeList,
                                                         nsINodeList)
  NS_DECL_NSIDOMNODELIST

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  // nsINodeList interface
  virtual int32_t IndexOf(nsIContent* aContent) MOZ_OVERRIDE;
  virtual nsINode* GetParentObject() MOZ_OVERRIDE;

  void AppendElement(nsGenericHTMLElement* aElement)
  {
    mElements.AppendElement(aElement);
  }

  void Clear()
  {
    mElements.Clear();
  }

  void SetDirty() { mIsDirty = true; }

protected:
  virtual ~PropertyNodeList();

  // Make sure this list is up to date, in case the DOM has been mutated.
  void EnsureFresh();

  // the the name that this list corresponds to
  nsString mName;

  // the document mParent is in, if any
  nsCOMPtr<nsIDocument> mDoc;

  // the collection that this list is a named item of
  nsRefPtr<HTMLPropertiesCollection> mCollection;

  // the node this list is rooted at
  nsCOMPtr<nsINode> mParent;

  // the properties that make up this list
  nsTArray<nsRefPtr<nsGenericHTMLElement> > mElements;

  // True if there have been DOM modifications since the last EnsureFresh call. 
  bool mIsDirty;
};

} // namespace dom
} // namespace mozilla
#endif // HTMLPropertiesCollection_h_
