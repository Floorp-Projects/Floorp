/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

#include "mozilla/Attributes.h"
#include "nsContentListDeclarations.h"
#include "nsISupports.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsIHTMLCollection.h"
#include "nsIDOMNodeList.h"
#include "nsINodeList.h"
#include "nsStubMutationObserver.h"
#include "nsIAtom.h"
#include "nsCycleCollectionParticipant.h"
#include "nsNameSpaceManager.h"
#include "nsWrapperCache.h"
#include "nsHashKeys.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/dom/NameSpaceConstants.h"

namespace mozilla {
namespace dom {
class Element;
}
}


class nsBaseContentList : public nsINodeList
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsIDOMNodeList
  NS_DECL_NSIDOMNODELIST

  // nsINodeList
  virtual int32_t IndexOf(nsIContent* aContent) override;
  virtual nsIContent* Item(uint32_t aIndex) override;

  uint32_t Length() const { 
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
  void InsertElementAt(nsIContent* aContent, int32_t aIndex)
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

  virtual int32_t IndexOf(nsIContent *aContent, bool aDoFlush);

  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
    override = 0;

  void SetCapacity(uint32_t aCapacity)
  {
    mElements.SetCapacity(aCapacity);
  }
protected:
  virtual ~nsBaseContentList();

  /**
   * To be called from non-destructor locations (e.g. unlink) that want to
   * remove from caches.  Cacheable subclasses should override.
   */
  virtual void RemoveFromCaches()
  {
  }

  nsTArray< nsCOMPtr<nsIContent> > mElements;
};


class nsSimpleContentList : public nsBaseContentList
{
public:
  explicit nsSimpleContentList(nsINode* aRoot) : nsBaseContentList(),
                                                 mRoot(aRoot)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsSimpleContentList,
                                           nsBaseContentList)

  virtual nsINode* GetParentObject() override
  {
    return mRoot;
  }
  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

protected:
  virtual ~nsSimpleContentList() {}

private:
  // This has to be a strong reference, the root might go away before the list.
  nsCOMPtr<nsINode> mRoot;
};

/**
 * Class that's used as the key to hash nsContentList implementations
 * for fast retrieval
 */
struct nsContentListKey
{
  nsContentListKey(nsINode* aRootNode,
                   int32_t aMatchNameSpaceId,
                   const nsAString& aTagname)
    : mRootNode(aRootNode),
      mMatchNameSpaceId(aMatchNameSpaceId),
      mTagname(aTagname),
      mHash(mozilla::AddToHash(mozilla::HashString(aTagname), mRootNode,
                               mMatchNameSpaceId))
  {
  }

  nsContentListKey(const nsContentListKey& aContentListKey)
    : mRootNode(aContentListKey.mRootNode),
      mMatchNameSpaceId(aContentListKey.mMatchNameSpaceId),
      mTagname(aContentListKey.mTagname),
      mHash(aContentListKey.mHash)
  {
  }

  inline uint32_t GetHash(void) const
  {
    return mHash;
  }
  
  nsINode* const mRootNode; // Weak ref
  const int32_t mMatchNameSpaceId;
  const nsAString& mTagname;
  const uint32_t mHash;
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
                int32_t aMatchNameSpaceId,
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
                nsIAtom* aMatchAtom = nullptr,
                int32_t aMatchNameSpaceId = kNameSpaceID_None,
                bool aFuncMayDependOnAttr = true);

  // nsWrapperCache
  using nsWrapperCache::GetWrapperPreserveColor;
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;
protected:
  virtual ~nsContentList();

  virtual JSObject* GetWrapperPreserveColorInternal() override
  {
    return nsWrapperCache::GetWrapperPreserveColor();
  }
public:

  // nsIDOMHTMLCollection
  NS_DECL_NSIDOMHTMLCOLLECTION

  // nsBaseContentList overrides
  virtual int32_t IndexOf(nsIContent *aContent, bool aDoFlush) override;
  virtual int32_t IndexOf(nsIContent* aContent) override;
  virtual nsINode* GetParentObject() override
  {
    return mRootNode;
  }

  virtual nsIContent* Item(uint32_t aIndex) override;
  virtual mozilla::dom::Element* GetElementAt(uint32_t index) override;
  virtual mozilla::dom::Element*
  GetFirstNamedElement(const nsAString& aName, bool& aFound) override
  {
    mozilla::dom::Element* item = NamedItem(aName, true);
    aFound = !!item;
    return item;
  }
  virtual void GetSupportedNames(unsigned aFlags,
                                 nsTArray<nsString>& aNames) override;

  // nsContentList public methods
  uint32_t Length(bool aDoFlush);
  nsIContent* Item(uint32_t aIndex, bool aDoFlush);
  mozilla::dom::Element*
  NamedItem(const nsAString& aName, bool aDoFlush);

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

  /**
   * Sets the state to LIST_DIRTY and clears mElements array.
   * @note This is the only acceptable way to set state to LIST_DIRTY.
   */
  void SetDirty()
  {
    mState = LIST_DIRTY;
    Reset();
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
  void PopulateSelf(uint32_t aNeededLength);

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
   * To be called from non-destructor locations that want to remove from caches.
   * Needed because if subclasses want to have cache behavior they can't just
   * override RemoveFromHashtable(), since we call that in our destructor.
   */
  virtual void RemoveFromCaches() override
  {
    RemoveFromHashtable();
  }

  nsINode* mRootNode; // Weak ref
  int32_t mMatchNameSpaceId;
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
  uint8_t mState;

  // The booleans have to use uint8_t to pack with mState, because MSVC won't
  // pack different typedefs together.  Once we no longer have to worry about
  // flushes in XML documents, we can go back to using bool for the
  // booleans.
  
  /**
   * True if we are looking for elements named "*"
   */
  uint8_t mMatchAll : 1;
  /**
   * Whether to actually descend the tree.  If this is false, we won't
   * consider grandkids of mRootNode.
   */
  uint8_t mDeep : 1;
  /**
   * Whether the return value of mFunc could depend on the values of
   * attributes.
   */
  uint8_t mFuncMayDependOnAttr : 1;
  /**
   * Whether we actually need to flush to get our state correct.
   */
  uint8_t mFlushesNeeded : 1;

#ifdef DEBUG_CONTENT_LIST
  void AssertInSync();
#endif
};

/**
 * A class of cacheable content list; cached on the combination of aRootNode + aFunc + aDataString
 */
class nsCacheableFuncStringContentList;

class MOZ_STACK_CLASS nsFuncStringCacheKey {
public:
  nsFuncStringCacheKey(nsINode* aRootNode,
                       nsContentListMatchFunc aFunc,
                       const nsAString& aString) :
    mRootNode(aRootNode),
    mFunc(aFunc),
    mString(aString)
    {}

  uint32_t GetHash(void) const
  {
    uint32_t hash = mozilla::HashString(mString);
    return mozilla::AddToHash(hash, mRootNode, mFunc);
  }

private:
  friend class nsCacheableFuncStringContentList;

  nsINode* const mRootNode;
  const nsContentListMatchFunc mFunc;
  const nsAString& mString;
};

// aDestroyFunc is allowed to be null
// aDataAllocator must always return a non-null pointer
class nsCacheableFuncStringContentList : public nsContentList {
public:
  virtual ~nsCacheableFuncStringContentList();

  bool Equals(const nsFuncStringCacheKey* aKey) {
    return mRootNode == aKey->mRootNode && mFunc == aKey->mFunc &&
      mString == aKey->mString;
  }

#ifdef DEBUG
  enum ContentListType {
    eNodeList,
    eHTMLCollection
  };
  ContentListType mType;
#endif

protected:
  nsCacheableFuncStringContentList(nsINode* aRootNode,
                                   nsContentListMatchFunc aFunc,
                                   nsContentListDestroyFunc aDestroyFunc,
                                   nsFuncStringContentListDataAllocator aDataAllocator,
                                   const nsAString& aString) :
    nsContentList(aRootNode, aFunc, aDestroyFunc, nullptr),
    mString(aString)
  {
    mData = (*aDataAllocator)(aRootNode, &mString);
    MOZ_ASSERT(mData);
  }

  virtual void RemoveFromCaches() override {
    RemoveFromFuncStringHashtable();
  }
  void RemoveFromFuncStringHashtable();

  nsString mString;
};

class nsCacheableFuncStringNodeList
  : public nsCacheableFuncStringContentList
{
public:
  nsCacheableFuncStringNodeList(nsINode* aRootNode,
                                nsContentListMatchFunc aFunc,
                                nsContentListDestroyFunc aDestroyFunc,
                                nsFuncStringContentListDataAllocator aDataAllocator,
                                const nsAString& aString)
    : nsCacheableFuncStringContentList(aRootNode, aFunc, aDestroyFunc,
                                       aDataAllocator, aString)
  {
#ifdef DEBUG
    mType = eNodeList;
#endif
  }

  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

#ifdef DEBUG
  static const ContentListType sType;
#endif
};

class nsCacheableFuncStringHTMLCollection
  : public nsCacheableFuncStringContentList
{
public:
  nsCacheableFuncStringHTMLCollection(nsINode* aRootNode,
                                      nsContentListMatchFunc aFunc,
                                      nsContentListDestroyFunc aDestroyFunc,
                                      nsFuncStringContentListDataAllocator aDataAllocator,
                                      const nsAString& aString)
    : nsCacheableFuncStringContentList(aRootNode, aFunc, aDestroyFunc,
                                       aDataAllocator, aString)
  {
#ifdef DEBUG
    mType = eHTMLCollection;
#endif
  }

  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

#ifdef DEBUG
  static const ContentListType sType;
#endif
};

#endif // nsContentList_h___
