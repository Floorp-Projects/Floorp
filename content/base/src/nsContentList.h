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
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMNodeList.h"
#include "nsStubMutationObserver.h"
#include "nsIAtom.h"
#include "nsINameSpaceManager.h"
#include "nsCycleCollectionParticipant.h"

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


class nsBaseContentList : public nsIDOMNodeList
{
public:
  nsBaseContentList();
  virtual ~nsBaseContentList();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsIDOMNodeList
  NS_DECL_NSIDOMNODELIST
  NS_DECL_CYCLE_COLLECTION_CLASS(nsBaseContentList)

  virtual void AppendElement(nsIContent *aContent);
  virtual void RemoveElement(nsIContent *aContent);
  virtual PRInt32 IndexOf(nsIContent *aContent, PRBool aDoFlush);
  virtual void Reset();

  static void Shutdown();

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
                   nsIAtom* aMatchAtom, 
                   PRInt32 aMatchNameSpaceId)
    : mMatchAtom(aMatchAtom),
      mMatchNameSpaceId(aMatchNameSpaceId),
      mRootNode(aRootNode)
  {
  }
  
  nsContentListKey(const nsContentListKey& aContentListKey)
    : mMatchAtom(aContentListKey.mMatchAtom),
      mMatchNameSpaceId(aContentListKey.mMatchNameSpaceId),
      mRootNode(aContentListKey.mRootNode)
  {
  }

  PRBool Equals(const nsContentListKey& aContentListKey) const
  {
    return
      mMatchAtom == aContentListKey.mMatchAtom &&
      mMatchNameSpaceId == aContentListKey.mMatchNameSpaceId &&
      mRootNode == aContentListKey.mRootNode;
  }
  inline PRUint32 GetHash(void) const
  {
    return
      NS_PTR_TO_INT32(mMatchAtom.get()) ^
      (NS_PTR_TO_INT32(mRootNode) << 12) ^
      (mMatchNameSpaceId << 24);
  }
  
protected:
  nsCOMPtr<nsIAtom> mMatchAtom;
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
 * Class that implements a live NodeList that matches nodes in the
 * tree based on some criterion
 */
class nsContentList : public nsBaseContentList,
                      protected nsContentListKey,
                      public nsIDOMHTMLCollection,
                      public nsStubMutationObserver
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
                nsIAtom* aMatchAtom, 
                PRInt32 aMatchNameSpaceId,
                PRBool aDeep = PR_TRUE);

  /**
   * @param aRootNode The node under which to limit our search.
   * @param aFunc the function to be called to determine whether we match
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

  // nsContentList public methods
  NS_HIDDEN_(nsISupports*) GetParentObject();
  NS_HIDDEN_(PRUint32) Length(PRBool aDoFlush);
  NS_HIDDEN_(nsIContent*) Item(PRUint32 aIndex, PRBool aDoFlush);
  NS_HIDDEN_(nsIContent*) NamedItem(const nsAString& aName, PRBool aDoFlush);

  nsContentListKey* GetKey() {
    return NS_STATIC_CAST(nsContentListKey*, this);
  }
  

  // nsIMutationObserver
  virtual void AttributeChanged(nsIDocument *aDocument, nsIContent* aContent,
                                PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                                PRInt32 aModType);
  virtual void ContentAppended(nsIDocument *aDocument, nsIContent* aContainer,
                               PRInt32 aNewIndexInContainer);
  virtual void ContentInserted(nsIDocument *aDocument, nsIContent* aContainer,
                               nsIContent* aChild, PRInt32 aIndexInContainer);
  virtual void ContentRemoved(nsIDocument *aDocument, nsIContent* aContainer,
                              nsIContent* aChild, PRInt32 aIndexInContainer);
  virtual void NodeWillBeDestroyed(const nsINode *aNode);

  static void OnDocumentDestroy(nsIDocument *aDocument);

protected:
  /**
   * Returns whether the content element matches our criterion
   *
   * @param  aContent the content to attempt to match
   * @return whether we match
   */
  PRBool Match(nsIContent *aContent);
  /**
   * Match recursively. See if anything in the subtree rooted at
   * aContent matches our criterion.
   *
   * @param  aContent the root of the subtree to match against
   * @return whether we match something in the tree rooted at aContent
   */
  PRBool MatchSelf(nsIContent *aContent);

  /**
   * Add elements in the subtree rooted in aContent that match our
   * criterion to our list until we've picked up aElementsToAppend
   * elements.  This function enforces the invariant that
   * |aElementsToAppend + mElements.Count()| is a constant.
   *
   * @param aContent the root of the subtree we want to traverse. This node
   *                 is always included in the traversal and is thus the
   *                 first node tested.
   * @param aElementsToAppend how many elements to append to the list
   *        before stopping
   */

  void PopulateWith(nsIContent *aContent, PRUint32 & aElementsToAppend);
  /**
   * Populate our list starting at the child of aStartRoot that comes
   * after aStartChild (if such exists) and continuing in document
   * order. Stop once we've picked up aElementsToAppend elements.
   * This function enforces the invariant that |aElementsToAppend +
   * mElements.Count()| is a constant.
   *
   * @param aStartRoot the node with whose children we want to start traversal
   * @param aStartChild the child after which we want to start
   * @param aElementsToAppend how many elements to append to the list
   *        before stopping
   */
  void PopulateWithStartingAfter(nsINode *aStartRoot,
                                 nsINode *aStartChild,
                                 PRUint32 & aElementsToAppend);
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

already_AddRefed<nsContentList>
NS_GetContentList(nsINode* aRootNode, nsIAtom* aMatchAtom,
                  PRInt32 aMatchNameSpaceId);

#endif // nsContentList_h___
