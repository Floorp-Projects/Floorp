/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * nsBaseContentList is a basic list of content nodes; nsContentList
 * is a commonly used NodeList implementation (used for
 * getElementsByTagName, some properties on nsIDOMHTMLDocument, etc).
 */

#ifndef nsContentList_h___
#define nsContentList_h___

#include "nsISupports.h"
#include "nsCOMArray.h"
#include "nsString.h"
#include "nsIHTMLCollection.h"
#include "nsIDOMNodeList.h"
#include "nsINodeList.h"
#include "nsStubMutationObserver.h"
#include "nsIAtom.h"
#include "nsINameSpaceManager.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsCRT.h"
#include "mozilla/dom/Element.h"

// Magic namespace id that means "match all namespaces".  This is
// negative so it won't collide with actual namespace constants.
#define kNameSpaceID_Wildcard PR_INT32_MIN

// This is a callback function type that can be used to implement an
// arbitrary matching algorithm.  aContent is the content that may
// match the list, while aNamespaceID, aAtom, and aData are whatever
// was passed to the list's constructor.
typedef PRBool (*nsContentListMatchFunc)(nsIContent* aContent,
                                         PRInt32 aNamespaceID,
                                         nsIAtom* aAtom,
                                         void* aData);

typedef void (*nsContentListDestroyFunc)(void* aData);

class nsIDocument;
class nsIDOMHTMLFormElement;


class nsBaseContentList : public nsINodeList
{
public:
  virtual ~nsBaseContentList();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsIDOMNodeList
  NS_DECL_NSIDOMNODELIST

  // nsINodeList
  virtual nsIContent* GetNodeAt(PRUint32 aIndex);
  virtual PRInt32 IndexOf(nsIContent* aContent);
  
  PRUint32 Length() const { 
    return mElements.Count();
  }

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsBaseContentList, nsINodeList)

  void AppendElement(nsIContent *aContent);
  void MaybeAppendElement(nsIContent* aContent)
  {
    if (aContent)
      AppendElement(aContent);
  }

  /**
   * Insert the element at a given index, shifting the objects at
   * the given index and later to make space.
   * @param aContent Element to insert, must not be null
   * @param aIndex Index to insert the element at.
   */
  void InsertElementAt(nsIContent* aContent, PRInt32 aIndex);

  void RemoveElement(nsIContent *aContent); 

  void Reset() {
    mElements.Clear();
  }


  virtual PRInt32 IndexOf(nsIContent *aContent, PRBool aDoFlush);

protected:
  nsCOMArray<nsIContent> mElements;
};


// This class is used only by form element code and this is a static
// list of elements. NOTE! This list holds strong references to
// the elements in the list.
class nsFormContentList : public nsBaseContentList
{
public:
  nsFormContentList(nsIDOMHTMLFormElement *aForm,
                    nsBaseContentList& aContentList);
};

/**
 * Class that's used as the key to hash nsContentList implementations
 * for fast retrieval
 */
class nsContentListKey
{
public:
  nsContentListKey(nsINode* aRootNode,
                   nsIAtom* aHTMLMatchAtom,
                   nsIAtom* aXMLMatchAtom,
                   PRInt32 aMatchNameSpaceId)
    : mHTMLMatchAtom(aHTMLMatchAtom),
      mXMLMatchAtom(aXMLMatchAtom),
      mMatchNameSpaceId(aMatchNameSpaceId),
      mRootNode(aRootNode)
  {
    NS_ASSERTION(!aXMLMatchAtom == !aHTMLMatchAtom, "Either neither or both atoms should be null");
  }
  
  nsContentListKey(const nsContentListKey& aContentListKey)
    : mHTMLMatchAtom(aContentListKey.mHTMLMatchAtom),
      mXMLMatchAtom(aContentListKey.mXMLMatchAtom),
      mMatchNameSpaceId(aContentListKey.mMatchNameSpaceId),
      mRootNode(aContentListKey.mRootNode)
  {
  }

  PRBool Equals(const nsContentListKey& aContentListKey) const
  {
    NS_ASSERTION(mHTMLMatchAtom == aContentListKey.mHTMLMatchAtom 
                 || mXMLMatchAtom != aContentListKey.mXMLMatchAtom, "HTML atoms should match if XML atoms match");

    return
      mXMLMatchAtom == aContentListKey.mXMLMatchAtom &&
      mMatchNameSpaceId == aContentListKey.mMatchNameSpaceId &&
      mRootNode == aContentListKey.mRootNode;
  }

  inline PRUint32 GetHash(void) const
  {
    return
      NS_PTR_TO_INT32(mXMLMatchAtom.get()) ^
      (NS_PTR_TO_INT32(mRootNode) << 12) ^
      (mMatchNameSpaceId << 24);
  }
  
protected:
  nsCOMPtr<nsIAtom> mHTMLMatchAtom;
  nsCOMPtr<nsIAtom> mXMLMatchAtom;
  PRInt32 mMatchNameSpaceId;
  nsINode* mRootNode; // Weak ref
};

/**
 * LIST_UP_TO_DATE means that the list is up to date and need not do
 * any walking to be able to answer any questions anyone may have.
 */
#define LIST_UP_TO_DATE 0
/**
 * LIST_DIRTY means that the list contains no useful information and
 * if anyone asks it anything it will have to populate itself before
 * answering.
 */
#define LIST_DIRTY 1
/**
 * LIST_LAZY means that the list has populated itself to a certain
 * extent and that that part of the list is still valid.  Requests for
 * things outside that part of the list will require walking the tree
 * some more.  When a list is in this state, the last thing in
 * mElements is the last node in the tree that the list looked at.
 */
#define LIST_LAZY 2

/**
 * Class that implements a live NodeList that matches Elements in the
 * tree based on some criterion.
 */
class nsContentList : public nsBaseContentList,
                      protected nsContentListKey,
                      public nsIHTMLCollection,
                      public nsStubMutationObserver,
                      public nsWrapperCache
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  /**
   * @param aRootNode The node under which to limit our search.
   * @param aMatchAtom An atom whose meaning depends on aMatchNameSpaceId.
   *                   The special value "*" always matches whatever aMatchAtom
   *                   is matched against.
   * @param aMatchNameSpaceId If kNameSpaceID_Unknown, then aMatchAtom is the
   *                          tagName to match.
   *                          If kNameSpaceID_Wildcard, then aMatchAtom is the
   *                          localName to match.
   *                          Otherwise we match nodes whose namespace is
   *                          aMatchNameSpaceId and localName matches
   *                          aMatchAtom.
   * @param aDeep If false, then look only at children of the root, nothing
   *              deeper.  If true, then look at the whole subtree rooted at
   *              our root.
   */  
  nsContentList(nsINode* aRootNode,
                PRInt32 aMatchNameSpaceId,
                nsIAtom* aHTMLMatchAtom,
                nsIAtom* aXMLMatchAtom,
                PRBool aDeep = PR_TRUE);

  /**
   * @param aRootNode The node under which to limit our search.
   * @param aFunc the function to be called to determine whether we match.
   *              This function MUST NOT ever cause mutation of the DOM.
   *              The nsContentList implementation guarantees that everything
   *              passed to the function will be IsElement().
   * @param aDestroyFunc the function that will be called to destroy aData
   * @param aData closure data that will need to be passed back to aFunc
   * @param aDeep If false, then look only at children of the root, nothing
   *              deeper.  If true, then look at the whole subtree rooted at
   *              our root.
   * @param aMatchAtom an atom to be passed back to aFunc
   * @param aMatchNameSpaceId a namespace id to be passed back to aFunc
   * @param aFuncMayDependOnAttr a boolean that indicates whether this list is
   *                             sensitive to attribute changes.
   */  
  nsContentList(nsINode* aRootNode,
                nsContentListMatchFunc aFunc,
                nsContentListDestroyFunc aDestroyFunc,
                void* aData,
                PRBool aDeep = PR_TRUE,
                nsIAtom* aMatchAtom = nsnull,
                PRInt32 aMatchNameSpaceId = kNameSpaceID_None,
                PRBool aFuncMayDependOnAttr = PR_TRUE);
  virtual ~nsContentList();

  // nsIDOMHTMLCollection
  NS_DECL_NSIDOMHTMLCOLLECTION

  // nsBaseContentList overrides
  virtual PRInt32 IndexOf(nsIContent *aContent, PRBool aDoFlush);
  virtual nsIContent* GetNodeAt(PRUint32 aIndex);
  virtual PRInt32 IndexOf(nsIContent* aContent);

  // nsIHTMLCollection
  virtual nsIContent* GetNodeAt(PRUint32 aIndex, nsresult* aResult);
  virtual nsISupports* GetNamedItem(const nsAString& aName,
                                    nsWrapperCache** aCache,
                                    nsresult* aResult);

  // nsContentList public methods
  NS_HIDDEN_(nsINode*) GetParentObject() { return mRootNode; }
  NS_HIDDEN_(PRUint32) Length(PRBool aDoFlush);
  NS_HIDDEN_(nsIContent*) Item(PRUint32 aIndex, PRBool aDoFlush);
  NS_HIDDEN_(nsIContent*) NamedItem(const nsAString& aName, PRBool aDoFlush);

  nsContentListKey* GetKey() {
    return static_cast<nsContentListKey*>(this);
  }
  

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED
  
  static nsContentList* FromSupports(nsISupports* aSupports)
  {
    nsINodeList* list = static_cast<nsINodeList*>(aSupports);
#ifdef DEBUG
    {
      nsCOMPtr<nsINodeList> list_qi = do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsINodeList pointer as the nsISupports
      // pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(list_qi == list, "Uh, fix QI!");
    }
#endif
    return static_cast<nsContentList*>(list);
  }

protected:
  /**
   * Returns whether the element matches our criterion
   *
   * @param  aElement the element to attempt to match
   * @return whether we match
   */
  PRBool Match(mozilla::dom::Element *aElement);
  /**
   * See if anything in the subtree rooted at aContent, including
   * aContent itself, matches our criterion.
   *
   * @param  aContent the root of the subtree to match against
   * @return whether we match something in the tree rooted at aContent
   */
  PRBool MatchSelf(nsIContent *aContent);

  /**
   * Populate our list.  Stop once we have at least aNeededLength
   * elements.  At the end of PopulateSelf running, either the last
   * node we examined is the last node in our array or we have
   * traversed the whole document (or both).
   *
   * @param aNeededLength the length the list should have when we are
   *        done (unless it exhausts the document)   
   */
  void PopulateSelf(PRUint32 aNeededLength);

  /**
   * @param  aContainer a content node which must be a descendant of
   *         mRootNode
   * @return PR_TRUE if children or descendants of aContainer could match our
   *                 criterion.
   *         PR_FALSE otherwise.
   */
  PRBool MayContainRelevantNodes(nsINode* aContainer)
  {
    return mDeep || aContainer == mRootNode;
  }

  /**
   * Remove ourselves from the hashtable that caches commonly accessed
   * content lists.  Generally done on destruction.
   */
  void RemoveFromHashtable();
  /**
   * If state is not LIST_UP_TO_DATE, fully populate ourselves with
   * all the nodes we can find.
   */
  inline void BringSelfUpToDate(PRBool aDoFlush);

  /**
   * Sets the state to LIST_DIRTY and clears mElements array.
   * @note This is the only acceptable way to set state to LIST_DIRTY.
   */
  void SetDirty()
  {
    mState = LIST_DIRTY;
    Reset();
  }

  /**
   * To be called from non-destructor locations that want to remove from caches.
   * Needed because if subclasses want to have cache behavior they can't just
   * override RemoveFromHashtable(), since we call that in our destructor.
   */
  virtual void RemoveFromCaches() {
    RemoveFromHashtable();
  }

  /**
   * Function to use to determine whether a piece of content matches
   * our criterion
   */
  nsContentListMatchFunc mFunc;
  /**
   * Cleanup closure data with this.
   */
  nsContentListDestroyFunc mDestroyFunc;
  /**
   * Closure data to pass to mFunc when we call it
   */
  void* mData;
  /**
   * True if we are looking for elements named "*"
   */
  PRPackedBool mMatchAll;
  /**
   * The current state of the list (possible values are:
   * LIST_UP_TO_DATE, LIST_LAZY, LIST_DIRTY
   */
  PRUint8 mState;
  /**
   * Whether to actually descend the tree.  If this is false, we won't
   * consider grandkids of mRootNode.
   */
  PRPackedBool mDeep;
  /**
   * Whether the return value of mFunc could depend on the values of
   * attributes.
   */
  PRPackedBool mFuncMayDependOnAttr;

#ifdef DEBUG_CONTENT_LIST
  void AssertInSync();
#endif
};

/**
 * A class of cacheable content list; cached on the combination of aRootNode + aFunc + aDataString
 */
class nsCacheableFuncStringContentList;

class NS_STACK_CLASS nsFuncStringCacheKey {
public:
  nsFuncStringCacheKey(nsINode* aRootNode,
                       nsContentListMatchFunc aFunc,
                       const nsAString& aString) :
    mRootNode(aRootNode),
    mFunc(aFunc),
    mString(aString)
    {}

  PRUint32 GetHash(void) const
  {
    return NS_PTR_TO_INT32(mRootNode) ^ (NS_PTR_TO_INT32(mFunc) << 12) ^
      nsCRT::HashCode(mString.BeginReading(), mString.Length());
  }

private:
  friend class nsCacheableFuncStringContentList;

  nsINode* const mRootNode;
  const nsContentListMatchFunc mFunc;
  const nsAString& mString;
};

/**
 * A function that allocates the matching data for this
 * FuncStringContentList.  Returning aString is perfectly fine; in
 * that case the destructor function should be a no-op.
 */
typedef void* (*nsFuncStringContentListDataAllocator)(nsINode* aRootNode,
                                                      const nsString* aString);

// aDestroyFunc is allowed to be null
class nsCacheableFuncStringContentList : public nsContentList {
public:
  nsCacheableFuncStringContentList(nsINode* aRootNode,
                                   nsContentListMatchFunc aFunc,
                                   nsContentListDestroyFunc aDestroyFunc,
                                   nsFuncStringContentListDataAllocator aDataAllocator,
                                   const nsAString& aString) :
    nsContentList(aRootNode, aFunc, aDestroyFunc, nsnull),
    mString(aString)
  {
    mData = (*aDataAllocator)(aRootNode, &mString);
  }

  virtual ~nsCacheableFuncStringContentList();

  PRBool Equals(const nsFuncStringCacheKey* aKey) {
    return mRootNode == aKey->mRootNode && mFunc == aKey->mFunc &&
      mString == aKey->mString;
  }

  PRBool AllocatedData() const { return !!mData; }
protected:
  virtual void RemoveFromCaches() {
    RemoveFromFuncStringHashtable();
  }
  void RemoveFromFuncStringHashtable();

  nsString mString;
};

already_AddRefed<nsContentList>
NS_GetContentList(nsINode* aRootNode,
                  PRInt32 aMatchNameSpaceId,
                  nsIAtom* aHTMLMatchAtom,
                  nsIAtom* aXMLMatchAtom = nsnull);

already_AddRefed<nsContentList>
NS_GetFuncStringContentList(nsINode* aRootNode,
                            nsContentListMatchFunc aFunc,
                            nsContentListDestroyFunc aDestroyFunc,
                            nsFuncStringContentListDataAllocator aDataAllocator,
                            const nsAString& aString);
#endif // nsContentList_h___
