/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsContentList_h___
#define nsContentList_h___

#include "nsISupports.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMNodeList.h"
#include "nsStubDocumentObserver.h"
#include "nsIContentList.h"
#include "nsIAtom.h"

typedef PRBool (*nsContentListMatchFunc)(nsIContent* aContent,
                                         nsString* aData);

class nsIDocument;
class nsIDOMHTMLFormElement;


class nsBaseContentList : public nsIDOMNodeList
{
public:
  nsBaseContentList();
  virtual ~nsBaseContentList();

  NS_DECL_ISUPPORTS

  // nsIDOMNodeList
  NS_DECL_NSIDOMNODELIST

  virtual void AppendElement(nsIContent *aContent);
  virtual void RemoveElement(nsIContent *aContent);
  virtual PRInt32 IndexOf(nsIContent *aContent, PRBool aDoFlush);
  virtual void Reset();

  static void Shutdown();

protected:
  nsAutoVoidArray mElements;
};


// This class is used only by form element code and this is a static
// list of elements. NOTE! This list holds strong references to
// the elements in the list.
class nsFormContentList : public nsBaseContentList
{
public:
  nsFormContentList(nsIDOMHTMLFormElement *aForm,
                    nsBaseContentList& aContentList);
  virtual ~nsFormContentList();

  virtual void AppendElement(nsIContent *aContent);
  virtual void RemoveElement(nsIContent *aContent);

  virtual void Reset();
};

/**
 * Class that's used as the key to hash nsContentList implementations
 * for fast retrieval
 */
class nsContentListKey
{
public:
  nsContentListKey(nsIDocument *aDocument,
                   nsIAtom* aMatchAtom, 
                   PRInt32 aMatchNameSpaceId,
                   nsIContent* aRootContent)
    : mMatchAtom(aMatchAtom),
      mMatchNameSpaceId(aMatchNameSpaceId),
      mDocument(aDocument),
      mRootContent(aRootContent)
  {
  }
  
  nsContentListKey(const nsContentListKey& aContentListKey)
    : mMatchAtom(aContentListKey.mMatchAtom),
      mMatchNameSpaceId(aContentListKey.mMatchNameSpaceId),
      mDocument(aContentListKey.mDocument),
      mRootContent(aContentListKey.mRootContent)
  {
  }

  PRBool Equals(const nsContentListKey& aContentListKey) const
  {
    return
      mMatchAtom == aContentListKey.mMatchAtom &&
      mMatchNameSpaceId == aContentListKey.mMatchNameSpaceId &&
      mDocument == aContentListKey.mDocument &&
      mRootContent == aContentListKey.mRootContent;
  }
  inline PRUint32 GetHash(void) const
  {
    return
      NS_PTR_TO_INT32(mMatchAtom.get()) ^
      (NS_PTR_TO_INT32(mRootContent) << 8) ^
      (NS_PTR_TO_INT32(mDocument) << 16) ^
      (mMatchNameSpaceId << 24);
  }
  
protected:
  nsCOMPtr<nsIAtom> mMatchAtom;
  PRInt32 mMatchNameSpaceId;
  nsIDocument* mDocument;   // Weak ref
  // XXX What if the mRootContent is detached from the doc and _then_
  // goes away (so we never get notified)?
  nsIContent* mRootContent; // Weak ref
};

/**
 * Class that implements a live NodeList that matches nodes in the
 * tree based on some criterion
 */
class nsContentList : public nsBaseContentList,
                      protected nsContentListKey,
                      public nsIDOMHTMLCollection,
                      public nsStubDocumentObserver,
                      public nsIContentList
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  nsContentList(nsIDocument *aDocument, 
                nsIAtom* aMatchAtom, 
                PRInt32 aMatchNameSpaceId,
                nsIContent* aRootContent=nsnull);
  nsContentList(nsIDocument *aDocument, 
                nsContentListMatchFunc aFunc,
                const nsAString& aData,
                nsIContent* aRootContent=nsnull);
  virtual ~nsContentList();

  // nsIDOMHTMLCollection
  NS_DECL_NSIDOMHTMLCOLLECTION

  /// nsIContentList
  virtual nsISupports *GetParentObject();
  virtual PRUint32 Length(PRBool aDoFlush);
  virtual nsIContent *Item(PRUint32 aIndex, PRBool aDoFlush);
  virtual nsIContent *NamedItem(const nsAString& aName, PRBool aDoFlush);
  virtual PRInt32 IndexOf(nsIContent *aContent, PRBool aDoFlush);

  // nsIDocumentObserver
  virtual void ContentAppended(nsIDocument *aDocument, nsIContent* aContainer,
                               PRInt32 aNewIndexInContainer);
  virtual void ContentInserted(nsIDocument *aDocument, nsIContent* aContainer,
                               nsIContent* aChild, PRInt32 aIndexInContainer);
  virtual void ContentReplaced(nsIDocument *aDocument, nsIContent* aContainer,
                               nsIContent* aOldChild, nsIContent* aNewChild,
                               PRInt32 aIndexInContainer);
  virtual void ContentRemoved(nsIDocument *aDocument, nsIContent* aContainer,
                              nsIContent* aChild, PRInt32 aIndexInContainer);
  virtual void DocumentWillBeDestroyed(nsIDocument *aDocument);

  // Other public methods
  nsContentListKey* GetKey() {
    return NS_STATIC_CAST(nsContentListKey*, this);
  }
  
protected:
  void Init(nsIDocument *aDocument);
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
   * @param aContent the root of the subtree we want to traverse
   * @param aIncludeRoot whether to include the root in the traversal
   * @param aElementsToAppend how many elements to append to the list
   *        before stopping
   */
  void PopulateWith(nsIContent *aContent, PRBool aIncludeRoot,
                    PRUint32 & aElementsToAppend);
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
  void PopulateWithStartingAfter(nsIContent *aStartRoot,
                                 nsIContent *aStartChild,
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
   * Our root content has been disconnected from the document, so stop
   * observing. From this point on, if someone asks us something we
   * walk the tree rooted at mRootContent starting at the beginning
   * and going as far as we need to to answer the question.
   */
  void DisconnectFromDocument();

  /**
   * @param  aContainer a content node which could be a descendant of
   *         mRootContent
   * @return PR_TRUE if mRootContent is null, PR_FALSE if aContainer
   *         is null, PR_TRUE if aContainer is a descendant of mRootContent,
   *         PR_FALSE otherwise
   */
  PRBool IsDescendantOfRoot(nsIContent* aContainer);
  /**
   * Does this subtree contain our mRootContent?
   *
   * @param  aContainer the root of the subtree
   * @return PR_FALSE if mRootContent is null, otherwise whether
   *         mRootContent is a descendant of aContainer
   */
  PRBool ContainsRoot(nsIContent* aContent);
  /**
   * If we have no document and we have a root content, then check if
   * our content has been added to a document. If so, we'll become an
   * observer of the document.
   */
  void CheckDocumentExistence();
  void RemoveFromHashtable();
  inline void BringSelfUpToDate(PRBool aDoFlush);

  /**
   * Function to use to determine whether a piece of content matches
   * our criterion
   */
  nsContentListMatchFunc mFunc;
  /**
   * Closure data to pass to mFunc when we call it
   */
  nsString* mData;
  /**
   * True if we are looking for elements named "*"
   */
  PRPackedBool mMatchAll;
  /**
   * The current state of the list (possible values are:
   * LIST_UP_TO_DATE, LIST_LAZY, LIST_DIRTY
   */
  PRUint8 mState;
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

nsresult
NS_GetContentList(nsIDocument* aDocument, nsIAtom* aMatchAtom,
                  PRInt32 aMatchNameSpaceId, nsIContent* aRootContent,
                  nsIContentList** aInstancePtrResult);

#endif // nsContentList_h___
