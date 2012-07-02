/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsBaseContentList is a basic list of content nodes; nsContentList
 * is a commonly used NodeList implementation (used for
 * getElementsByTagName, some properties on nsIDOMHTMLDocument, etc).
 */

#ifndef nsContentList_h___
#define nsContentList_h___

#include "nsISupports.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsIHTMLCollection.h"
#include "nsIDOMNodeList.h"
#include "nsINodeList.h"
#include "nsStubMutationObserver.h"
#include "nsIAtom.h"
#include "nsINameSpaceManager.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsHashKeys.h"
#include "mozilla/HashFunctions.h"

// Magic namespace id that means "match all namespaces".  This is
// negative so it won't collide with actual namespace constants.
#define kNameSpaceID_Wildcard PR_INT32_MIN

// This is a callback function type that can be used to implement an
// arbitrary matching algorithm.  aContent is the content that may
// match the list, while aNamespaceID, aAtom, and aData are whatever
// was passed to the list's constructor.
typedef bool (*nsContentListMatchFunc)(nsIContent* aContent,
                                         PRInt32 aNamespaceID,
                                         nsIAtom* aAtom,
                                         void* aData);

typedef void (*nsContentListDestroyFunc)(void* aData);

namespace mozilla {
namespace dom {
class Element;
}
}


class nsBaseContentList : public nsINodeList
{
public:
  nsBaseContentList()
  {
    SetIsDOMBinding();
  }
  virtual ~nsBaseContentList();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsIDOMNodeList
  NS_DECL_NSIDOMNODELIST

  // nsINodeList
  virtual PRInt32 IndexOf(nsIContent* aContent);
  
  PRUint32 Length() const { 
    return mElements.Length();
  }

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS(nsBaseContentList)

  void AppendElement(nsIContent *aContent)
  {
    mElements.AppendElement(aContent);
  }
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
  void InsertElementAt(nsIContent* aContent, PRInt32 aIndex)
  {
    NS_ASSERTION(aContent, "Element to insert must not be null");
    mElements.InsertElementAt(aIndex, aContent);
  }

  void RemoveElement(nsIContent *aContent)
  {
    mElements.RemoveElement(aContent);
  }

  void Reset() {
    mElements.Clear();
  }

  virtual PRInt32 IndexOf(nsIContent *aContent, bool aDoFlush);

  virtual JSObject* WrapObject(JSContext *cx, JSObject *scope,
                               bool *triedToWrap) = 0;

protected:
  nsTArray< nsCOMPtr<nsIContent> > mElements;
};


class nsSimpleContentList : public nsBaseContentList
{
public:
  nsSimpleContentList(nsINode *aRoot) : nsBaseContentList(),
                                        mRoot(aRoot)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsSimpleContentList,
                                           nsBaseContentList)

  virtual nsINode* GetParentObject()
  {
    return mRoot;
  }
  virtual JSObject* WrapObject(JSContext *cx, JSObject *scope,
                               bool *triedToWrap);

private:
  // This has to be a strong reference, the root might go away before the list.
  nsCOMPtr<nsINode> mRoot;
};

// This class is used only by form element code and this is a static
// list of elements. NOTE! This list holds strong references to
// the elements in the list.
class nsFormContentList : public nsSimpleContentList
{
public:
  nsFormContentList(nsIContent *aForm,
                    nsBaseContentList& aContentList);
};

/**
 * Class that's used as the key to hash nsContentList implementations
 * for fast retrieval
 */
struct nsContentListKey
{
  nsContentListKey(nsINode* aRootNode,
                   PRInt32 aMatchNameSpaceId,
                   const nsAString& aTagname)
    : mRootNode(aRootNode),
      mMatchNameSpaceId(aMatchNameSpaceId),
      mTagname(aTagname)
  {
  }

  nsContentListKey(const nsContentListKey& aContentListKey)
    : mRootNode(aContentListKey.mRootNode),
      mMatchNameSpaceId(aContentListKey.mMatchNameSpaceId),
      mTagname(aContentListKey.mTagname)
  {
  }

  inline PRUint32 GetHash(void) const
  {
    PRUint32 hash = mozilla::HashString(mTagname);
    return mozilla::AddToHash(hash, mRootNode, mMatchNameSpaceId);
  }
  
  nsINode* const mRootNode; // Weak ref
  const PRInt32 mMatchNameSpaceId;
  const nsAString& mTagname;
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
                      public nsIHTMLCollection,
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
                PRInt32 aMatchNameSpaceId,
                nsIAtom* aHTMLMatchAtom,
                nsIAtom* aXMLMatchAtom,
                bool aDeep = true);

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
                bool aDeep = true,
                nsIAtom* aMatchAtom = nsnull,
                PRInt32 aMatchNameSpaceId = kNameSpaceID_None,
                bool aFuncMayDependOnAttr = true);
  virtual ~nsContentList();

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext *cx, JSObject *scope,
                               bool *triedToWrap);

  // nsIDOMHTMLCollection
  NS_DECL_NSIDOMHTMLCOLLECTION

  // nsBaseContentList overrides
  virtual PRInt32 IndexOf(nsIContent *aContent, bool aDoFlush);
  virtual PRInt32 IndexOf(nsIContent* aContent);
  virtual nsINode* GetParentObject()
  {
    return mRootNode;
  }

  // nsContentList public methods
  NS_HIDDEN_(PRUint32) Length(bool aDoFlush);
  NS_HIDDEN_(nsIContent*) Item(PRUint32 aIndex, bool aDoFlush);
  NS_HIDDEN_(nsIContent*) NamedItem(const nsAString& aName, bool aDoFlush);

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

  bool MatchesKey(const nsContentListKey& aKey) const
  {
    // The root node is most commonly the same: the document.  And the
    // most common namespace id is kNameSpaceID_Unknown.  So check the
    // string first.
    NS_PRECONDITION(mXMLMatchAtom,
                    "How did we get here with a null match atom on our list?");
    return
      mXMLMatchAtom->Equals(aKey.mTagname) &&
      mRootNode == aKey.mRootNode &&
      mMatchNameSpaceId == aKey.mMatchNameSpaceId;
  }

protected:
  /**
   * Returns whether the element matches our criterion
   *
   * @param  aElement the element to attempt to match
   * @return whether we match
   */
  bool Match(mozilla::dom::Element *aElement);
  /**
   * See if anything in the subtree rooted at aContent, including
   * aContent itself, matches our criterion.
   *
   * @param  aContent the root of the subtree to match against
   * @return whether we match something in the tree rooted at aContent
   */
  bool MatchSelf(nsIContent *aContent);

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
   * @return true if children or descendants of aContainer could match our
   *                 criterion.
   *         false otherwise.
   */
  bool MayContainRelevantNodes(nsINode* aContainer)
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
  inline void BringSelfUpToDate(bool aDoFlush);

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

  nsINode* mRootNode; // Weak ref
  PRInt32 mMatchNameSpaceId;
  nsCOMPtr<nsIAtom> mHTMLMatchAtom;
  nsCOMPtr<nsIAtom> mXMLMatchAtom;

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
   * The current state of the list (possible values are:
   * LIST_UP_TO_DATE, LIST_LAZY, LIST_DIRTY
   */
  PRUint8 mState;

  // The booleans have to use PRUint8 to pack with mState, because MSVC won't
  // pack different typedefs together.  Once we no longer have to worry about
  // flushes in XML documents, we can go back to using bool for the
  // booleans.
  
  /**
   * True if we are looking for elements named "*"
   */
  PRUint8 mMatchAll : 1;
  /**
   * Whether to actually descend the tree.  If this is false, we won't
   * consider grandkids of mRootNode.
   */
  PRUint8 mDeep : 1;
  /**
   * Whether the return value of mFunc could depend on the values of
   * attributes.
   */
  PRUint8 mFuncMayDependOnAttr : 1;
  /**
   * Whether we actually need to flush to get our state correct.
   */
  PRUint8 mFlushesNeeded : 1;

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
    PRUint32 hash = mozilla::HashString(mString);
    return mozilla::AddToHash(hash, mRootNode, mFunc);
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

  bool Equals(const nsFuncStringCacheKey* aKey) {
    return mRootNode == aKey->mRootNode && mFunc == aKey->mFunc &&
      mString == aKey->mString;
  }

  bool AllocatedData() const { return !!mData; }
protected:
  virtual void RemoveFromCaches() {
    RemoveFromFuncStringHashtable();
  }
  void RemoveFromFuncStringHashtable();

  nsString mString;
};

// If aMatchNameSpaceId is kNameSpaceID_Unknown, this will return a
// content list which matches ASCIIToLower(aTagname) against HTML
// elements in HTML documents and aTagname against everything else.
// For any other value of aMatchNameSpaceId, the list will match
// aTagname against all elements.
already_AddRefed<nsContentList>
NS_GetContentList(nsINode* aRootNode,
                  PRInt32 aMatchNameSpaceId,
                  const nsAString& aTagname);

already_AddRefed<nsContentList>
NS_GetFuncStringContentList(nsINode* aRootNode,
                            nsContentListMatchFunc aFunc,
                            nsContentListDestroyFunc aDestroyFunc,
                            nsFuncStringContentListDataAllocator aDataAllocator,
                            const nsAString& aString);
#endif // nsContentList_h___
