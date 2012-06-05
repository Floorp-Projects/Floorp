/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HTMLPropertiesCollection_h_
#define HTMLPropertiesCollection_h_

#include "nsDOMLists.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"
#include "nsIDOMHTMLPropertiesCollection.h"
#include "nsIDOMPropertyNodeList.h"
#include "nsCOMArray.h"
#include "nsIMutationObserver.h"
#include "nsStubMutationObserver.h"
#include "nsBaseHashtable.h"
#include "nsINodeList.h"
#include "nsIHTMLCollection.h"
#include "nsHashKeys.h"
#include "nsGenericHTMLElement.h"

class nsXPCClassInfo;
class nsIDocument;
class nsINode;

namespace mozilla {
namespace dom {

class HTMLPropertiesCollection;
class PropertyNodeList;

class PropertyStringList : public nsDOMStringList
{
public:
  PropertyStringList(HTMLPropertiesCollection* aCollection);
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(PropertyStringList)
  NS_DECL_NSIDOMDOMSTRINGLIST

  bool ContainsInternal(const nsAString& aString);

protected:
  nsRefPtr<HTMLPropertiesCollection> mCollection;
};

class HTMLPropertiesCollection : public nsIDOMHTMLPropertiesCollection,
                                 public nsStubMutationObserver,
                                 public nsWrapperCache,
                                 public nsIHTMLCollection
{
  friend class PropertyNodeList;
  friend class PropertyStringList;
public:
  HTMLPropertiesCollection(nsGenericHTMLElement* aRoot);
  virtual ~HTMLPropertiesCollection();

  virtual JSObject* WrapObject(JSContext *cx, JSObject *scope,
                               bool *triedToWrap);

  NS_IMETHOD NamedItem(const nsAString& aName, nsIDOMNode** aResult);
  void SetDocument(nsIDocument* aDocument);
  nsINode* GetParentObject();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIDOMHTMLPROPERTIESCOLLECTION

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(HTMLPropertiesCollection,
                                                         nsIHTMLCollection)

  nsXPCClassInfo* GetClassInfo();

protected:
  // Make sure this collection is up to date, in case the DOM has been mutated.
  void EnsureFresh();
  
  // Crawl the properties of mRoot, following any itemRefs it may have
  void CrawlProperties();

  // Crawl startNode and its descendants, looking for items
  void CrawlSubtree(Element* startNode);

  // the items that make up this collection
  nsTArray<nsRefPtr<nsGenericHTMLElement> > mProperties; 
  
  // the itemprop attribute of the properties
  nsRefPtr<PropertyStringList> mNames; 
 
  // The cached PropertyNodeLists that are NamedItems of this collection 
  nsRefPtrHashtable<nsStringHashKey, PropertyNodeList> mNamedItemEntries;
  
  // The element this collection is rooted at
  nsCOMPtr<nsGenericHTMLElement> mRoot;
  
  // The document mRoot is in, if any
  nsCOMPtr<nsIDocument> mDoc;
  
  // True if there have been DOM modifications since the last EnsureFresh call.
  bool mIsDirty;
};

class PropertyNodeList : public nsINodeList,
                         public nsIDOMPropertyNodeList,
                         public nsStubMutationObserver
{
public:
  PropertyNodeList(HTMLPropertiesCollection* aCollection,
                   nsIContent* aRoot, const nsAString& aName);
  virtual ~PropertyNodeList();

  virtual JSObject* WrapObject(JSContext *cx, JSObject *scope,
                               bool *triedToWrap);

  void SetDocument(nsIDocument* aDocument);

  NS_DECL_NSIDOMPROPERTYNODELIST

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(PropertyNodeList,
                                                         nsINodeList)

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  // nsINodeList interface
  virtual PRInt32 IndexOf(nsIContent* aContent);
  virtual nsINode* GetParentObject();
  
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
