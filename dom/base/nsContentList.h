/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsBaseContentList is a basic list of content nodes; nsContentList
 * is a commonly used NodeList implementation (used for
 * getElementsByTagName, some properties on HTMLDocument/Document, etc).
 */

#ifndef nsContentList_h___
#define nsContentList_h___

#include "mozilla/Attributes.h"
#include "nsContentListDeclarations.h"
#include "nsISupports.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsIHTMLCollection.h"
#include "nsINodeList.h"
#include "nsStubMutationObserver.h"
#include "nsAtomHashKeys.h"
#include "nsCycleCollectionParticipant.h"
#include "nsNameSpaceManager.h"
#include "nsWrapperCache.h"
#include "nsHashKeys.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/NameSpaceConstants.h"

// XXX Avoid including this here by moving function bodies to the cpp file.
#include "nsIContent.h"

namespace mozilla::dom {
class Element;
}  // namespace mozilla::dom

class nsBaseContentList : public nsINodeList {
 protected:
  using Element = mozilla::dom::Element;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsINodeList
  int32_t IndexOf(nsIContent* aContent) override;
  nsIContent* Item(uint32_t aIndex) override;

  uint32_t Length() override { return mElements.Length(); }

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_WRAPPERCACHE_CLASS(nsBaseContentList)

  void AppendElement(nsIContent* aContent) {
    MOZ_ASSERT(aContent);
    mElements.AppendElement(aContent);
  }
  void MaybeAppendElement(nsIContent* aContent) {
    if (aContent) {
      AppendElement(aContent);
    }
  }

  /**
   * Insert the element at a given index, shifting the objects at
   * the given index and later to make space.
   * @param aContent Element to insert, must not be null
   * @param aIndex Index to insert the element at.
   */
  void InsertElementAt(nsIContent* aContent, int32_t aIndex) {
    NS_ASSERTION(aContent, "Element to insert must not be null");
    mElements.InsertElementAt(aIndex, aContent);
  }

  void RemoveElement(nsIContent* aContent) {
    mElements.RemoveElement(aContent);
  }

  void Reset() { mElements.Clear(); }

  virtual int32_t IndexOf(nsIContent* aContent, bool aDoFlush);

  JSObject* WrapObject(JSContext* cx,
                       JS::Handle<JSObject*> aGivenProto) override = 0;

  void SetCapacity(uint32_t aCapacity) { mElements.SetCapacity(aCapacity); }

  virtual void LastRelease() {}

  // Memory reporting.  For now, subclasses of nsBaseContentList don't really
  // need to report any members that are not part of the object itself, so we
  // don't need to make this virtual.
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 protected:
  virtual ~nsBaseContentList();

  /**
   * To be called from non-destructor locations (e.g. unlink) that want to
   * remove from caches.  Cacheable subclasses should override.
   */
  virtual void RemoveFromCaches() {}

  AutoTArray<nsCOMPtr<nsIContent>, 10> mElements;
};

class nsSimpleContentList : public nsBaseContentList {
 public:
  explicit nsSimpleContentList(nsINode* aRoot)
      : nsBaseContentList(), mRoot(aRoot) {}

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsSimpleContentList,
                                           nsBaseContentList)

  nsINode* GetParentObject() override { return mRoot; }
  JSObject* WrapObject(JSContext* cx,
                       JS::Handle<JSObject*> aGivenProto) override;

 protected:
  virtual ~nsSimpleContentList() = default;

 private:
  // This has to be a strong reference, the root might go away before the list.
  nsCOMPtr<nsINode> mRoot;
};

// Used for returning lists that will always be empty, such as the applets list
// in HTML Documents
class nsEmptyContentList final : public nsBaseContentList,
                                 public nsIHTMLCollection {
 public:
  explicit nsEmptyContentList(nsINode* aRoot)
      : nsBaseContentList(), mRoot(aRoot) {}

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsEmptyContentList,
                                           nsBaseContentList)

  nsINode* GetParentObject() override { return mRoot; }

  JSObject* WrapObject(JSContext* cx,
                       JS::Handle<JSObject*> aGivenProto) override;

  JSObject* GetWrapperPreserveColorInternal() override {
    return nsWrapperCache::GetWrapperPreserveColor();
  }
  void PreserveWrapperInternal(nsISupports* aScriptObjectHolder) override {
    nsWrapperCache::PreserveWrapper(aScriptObjectHolder);
  }

  uint32_t Length() final { return 0; }
  nsIContent* Item(uint32_t aIndex) override;
  Element* GetElementAt(uint32_t index) override;
  Element* GetFirstNamedElement(const nsAString& aName, bool& aFound) override;
  void GetSupportedNames(nsTArray<nsString>& aNames) override;

 protected:
  virtual ~nsEmptyContentList() = default;

 private:
  // This has to be a strong reference, the root might go away before the list.
  nsCOMPtr<nsINode> mRoot;
};

/**
 * Class that's used as the key to hash nsContentList implementations
 * for fast retrieval
 */
struct nsContentListKey {
  // We have to take an aIsHTMLDocument arg for two reasons:
  // 1) We don't want to include Document.h in this header.
  // 2) We need to do that to make nsContentList::RemoveFromHashtable
  //    work, because by the time it's called the document of the
  //    list's root node might have changed.
  nsContentListKey(nsINode* aRootNode, int32_t aMatchNameSpaceId,
                   const nsAString& aTagname, bool aIsHTMLDocument)
      : mRootNode(aRootNode),
        mMatchNameSpaceId(aMatchNameSpaceId),
        mTagname(aTagname),
        mIsHTMLDocument(aIsHTMLDocument),
        mHash(mozilla::AddToHash(mozilla::HashString(aTagname), mRootNode,
                                 mMatchNameSpaceId, mIsHTMLDocument)) {}

  nsContentListKey(const nsContentListKey& aContentListKey) = default;

  inline uint32_t GetHash(void) const { return mHash; }

  nsINode* const mRootNode;  // Weak ref
  const int32_t mMatchNameSpaceId;
  const nsAString& mTagname;
  bool mIsHTMLDocument;
  const uint32_t mHash;
};

/**
 * Class that implements a possibly live NodeList that matches Elements
 * in the tree based on some criterion.
 */
class nsContentList : public nsBaseContentList,
                      public nsIHTMLCollection,
                      public nsStubMutationObserver {
 protected:
  enum class State : uint8_t {
    // The list is up to date and need not do any walking to be able to answer
    // any questions anyone may have.
    UpToDate = 0,
    // The list contains no useful information and if anyone asks it anything it
    // will have to populate itself before answering.
    Dirty,
    // The list has populated itself to a certain extent and that that part of
    // the list is still valid.  Requests for things outside that part of the
    // list will require walking the tree some more.  When a list is in this
    // state, the last thing in mElements is the last node in the tree that the
    // list looked at.
    Lazy,
  };

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
   * @param aLiveList Whether the created list should be a live list observing
   *                  mutations to the DOM tree.
   */
  nsContentList(nsINode* aRootNode, int32_t aMatchNameSpaceId,
                nsAtom* aHTMLMatchAtom, nsAtom* aXMLMatchAtom,
                bool aDeep = true, bool aLiveList = true);

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
   * @param aLiveList Whether the created list should be a live list observing
   *                  mutations to the DOM tree.
   */
  nsContentList(nsINode* aRootNode, nsContentListMatchFunc aFunc,
                nsContentListDestroyFunc aDestroyFunc, void* aData,
                bool aDeep = true, nsAtom* aMatchAtom = nullptr,
                int32_t aMatchNameSpaceId = kNameSpaceID_None,
                bool aFuncMayDependOnAttr = true, bool aLiveList = true);

  // nsWrapperCache
  using nsWrapperCache::GetWrapperPreserveColor;
  using nsWrapperCache::PreserveWrapper;
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

 protected:
  virtual ~nsContentList();

  JSObject* GetWrapperPreserveColorInternal() override {
    return nsWrapperCache::GetWrapperPreserveColor();
  }
  void PreserveWrapperInternal(nsISupports* aScriptObjectHolder) override {
    nsWrapperCache::PreserveWrapper(aScriptObjectHolder);
  }

 public:
  // nsBaseContentList overrides
  int32_t IndexOf(nsIContent* aContent, bool aDoFlush) override;
  int32_t IndexOf(nsIContent* aContent) override;
  nsINode* GetParentObject() override { return mRootNode; }

  uint32_t Length() final { return Length(true); }
  nsIContent* Item(uint32_t aIndex) final;
  Element* GetElementAt(uint32_t index) override;
  Element* GetFirstNamedElement(const nsAString& aName, bool& aFound) override {
    Element* item = NamedItem(aName, true);
    aFound = !!item;
    return item;
  }
  void GetSupportedNames(nsTArray<nsString>& aNames) override;

  // nsContentList public methods
  uint32_t Length(bool aDoFlush);
  nsIContent* Item(uint32_t aIndex, bool aDoFlush);
  Element* NamedItem(const nsAString& aName, bool aDoFlush);

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

  static nsContentList* FromSupports(nsISupports* aSupports) {
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

  bool MatchesKey(const nsContentListKey& aKey) const {
    // The root node is most commonly the same: the document.  And the
    // most common namespace id is kNameSpaceID_Unknown.  So check the
    // string first.  Cases in which whether our root's ownerDocument
    // is HTML changes are extremely rare, so check those last.
    MOZ_ASSERT(mXMLMatchAtom,
               "How did we get here with a null match atom on our list?");
    return mXMLMatchAtom->Equals(aKey.mTagname) &&
           mRootNode == aKey.mRootNode &&
           mMatchNameSpaceId == aKey.mMatchNameSpaceId &&
           mIsHTMLDocument == aKey.mIsHTMLDocument;
  }

  /**
   * Sets the state to LIST_DIRTY and clears mElements array.
   * @note This is the only acceptable way to set state to LIST_DIRTY.
   */
  void SetDirty() {
    mState = State::Dirty;
    InvalidateNamedItemsCache();
    Reset();
    SetEnabledCallbacks(nsIMutationObserver::kNodeWillBeDestroyed);
  }

  void LastRelease() override;

  class HashEntry;

 protected:
  // A cache from name to the first named item in mElements. Only possibly
  // non-null when mState is State::UpToDate. Elements are kept alive by our
  // mElements array.
  using NamedItemsCache = nsTHashMap<nsAtomHashKey, Element*>;

  void InvalidateNamedItemsCache() {
    mNamedItemsCache = nullptr;
    mNamedItemsCacheValid = false;
  }

  inline void InsertElementInNamedItemsCache(nsIContent&);
  inline void InvalidateNamedItemsCacheForAttributeChange(int32_t aNameSpaceID,
                                                          nsAtom* aAttribute);
  inline void InvalidateNamedItemsCacheForInsertion(Element&);
  inline void InvalidateNamedItemsCacheForDeletion(Element&);

  void EnsureNamedItemsCacheValid(bool aDoFlush);

  /**
   * Returns whether the element matches our criterion
   *
   * @param  aElement the element to attempt to match
   * @return whether we match
   */
  bool Match(Element* aElement);
  /**
   * See if anything in the subtree rooted at aContent, including
   * aContent itself, matches our criterion.
   *
   * @param  aContent the root of the subtree to match against
   * @return whether we match something in the tree rooted at aContent
   */
  bool MatchSelf(nsIContent* aContent);

  /**
   * Populate our list.  Stop once we have at least aNeededLength
   * elements.  At the end of PopulateSelf running, either the last
   * node we examined is the last node in our array or we have
   * traversed the whole document (or both).
   *
   * @param aNeededLength the length the list should have when we are
   *        done (unless it exhausts the document)
   * @param aExpectedElementsIfDirty is for debugging only to
   *        assert that mElements has expected number of entries.
   */
  virtual void PopulateSelf(uint32_t aNeededLength,
                            uint32_t aExpectedElementsIfDirty = 0);

  /**
   * @param  aContainer a content node which must be a descendant of
   *         mRootNode
   * @return true if children or descendants of aContainer could match our
   *                 criterion.
   *         false otherwise.
   */
  bool MayContainRelevantNodes(nsINode* aContainer) {
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
  void RemoveFromCaches() override { RemoveFromHashtable(); }

  void MaybeMarkDirty() {
    if (mState != State::Dirty && ++mMissedUpdates > 128) {
      mMissedUpdates = 0;
      SetDirty();
    }
  }

  nsINode* mRootNode;  // Weak ref
  int32_t mMatchNameSpaceId;
  RefPtr<nsAtom> mHTMLMatchAtom;
  RefPtr<nsAtom> mXMLMatchAtom;

  /**
   * Function to use to determine whether a piece of content matches
   * our criterion
   */
  nsContentListMatchFunc mFunc = nullptr;
  /**
   * Cleanup closure data with this.
   */
  nsContentListDestroyFunc mDestroyFunc = nullptr;
  /**
   * Closure data to pass to mFunc when we call it
   */
  void* mData = nullptr;

  mozilla::UniquePtr<NamedItemsCache> mNamedItemsCache;

  uint8_t mMissedUpdates = 0;

  // The current state of the list.
  State mState;

  /**
   * True if we are looking for elements named "*"
   */
  bool mMatchAll : 1;
  /**
   * Whether to actually descend the tree.  If this is false, we won't
   * consider grandkids of mRootNode.
   */
  bool mDeep : 1;
  /**
   * Whether the return value of mFunc could depend on the values of
   * attributes.
   */
  bool mFuncMayDependOnAttr : 1;
  /**
   * Whether we actually need to flush to get our state correct.
   */
  bool mFlushesNeeded : 1;
  /**
   * Whether the ownerDocument of our root node at list creation time was an
   * HTML document.  Only needed when we're doing a namespace/atom match, not
   * when doing function matching, always false otherwise.
   */
  bool mIsHTMLDocument : 1;
  /**
   * True mNamedItemsCache is valid. Note mNamedItemsCache might still be null
   * if there's no named items at all.
   */
  bool mNamedItemsCacheValid : 1;
  /**
   * Whether the list observes mutations to the DOM tree.
   */
  const bool mIsLiveList : 1;
  /*
   * True if this content list is cached in a hash table.
   * For nsContentList (but not its subclasses), the hash table is
   * gContentListHashTable.
   * For nsCacheableFuncStringContentList, the hash table is
   * gFuncStringContentListHashTable.
   * Other subclasses of nsContentList can't be in hash tables.
   */
  bool mInHashtable : 1;

#ifdef DEBUG_CONTENT_LIST
  void AssertInSync();
#endif
};

/**
 * A class of cacheable content list; cached on the combination of aRootNode +
 * aFunc + aDataString
 */
class nsCacheableFuncStringContentList;

class MOZ_STACK_CLASS nsFuncStringCacheKey {
 public:
  nsFuncStringCacheKey(nsINode* aRootNode, nsContentListMatchFunc aFunc,
                       const nsAString& aString)
      : mRootNode(aRootNode), mFunc(aFunc), mString(aString) {}

  uint32_t GetHash(void) const {
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

  enum ContentListType { eNodeList, eHTMLCollection };
#ifdef DEBUG
  ContentListType mType;
#endif

  class HashEntry;

 protected:
  nsCacheableFuncStringContentList(
      nsINode* aRootNode, nsContentListMatchFunc aFunc,
      nsContentListDestroyFunc aDestroyFunc,
      nsFuncStringContentListDataAllocator aDataAllocator,
      const nsAString& aString, mozilla::DebugOnly<ContentListType> aType)
      : nsContentList(aRootNode, aFunc, aDestroyFunc, nullptr),
#ifdef DEBUG
        mType(aType),
#endif
        mString(aString) {
    mData = (*aDataAllocator)(aRootNode, &mString);
    MOZ_ASSERT(mData);
  }

  void RemoveFromCaches() override { RemoveFromFuncStringHashtable(); }
  void RemoveFromFuncStringHashtable();

  nsString mString;
};

class nsCachableElementsByNameNodeList
    : public nsCacheableFuncStringContentList {
 public:
  nsCachableElementsByNameNodeList(
      nsINode* aRootNode, nsContentListMatchFunc aFunc,
      nsContentListDestroyFunc aDestroyFunc,
      nsFuncStringContentListDataAllocator aDataAllocator,
      const nsAString& aString)
      : nsCacheableFuncStringContentList(aRootNode, aFunc, aDestroyFunc,
                                         aDataAllocator, aString, eNodeList) {}

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED

  JSObject* WrapObject(JSContext* cx,
                       JS::Handle<JSObject*> aGivenProto) override;

#ifdef DEBUG
  static const ContentListType sType;
#endif
};

class nsCacheableFuncStringHTMLCollection
    : public nsCacheableFuncStringContentList {
 public:
  nsCacheableFuncStringHTMLCollection(
      nsINode* aRootNode, nsContentListMatchFunc aFunc,
      nsContentListDestroyFunc aDestroyFunc,
      nsFuncStringContentListDataAllocator aDataAllocator,
      const nsAString& aString)
      : nsCacheableFuncStringContentList(aRootNode, aFunc, aDestroyFunc,
                                         aDataAllocator, aString,
                                         eHTMLCollection) {}

  JSObject* WrapObject(JSContext* cx,
                       JS::Handle<JSObject*> aGivenProto) override;

#ifdef DEBUG
  static const ContentListType sType;
#endif
};

class nsLabelsNodeList final : public nsContentList {
 public:
  nsLabelsNodeList(nsINode* aRootNode, nsContentListMatchFunc aFunc,
                   nsContentListDestroyFunc aDestroyFunc, void* aData)
      : nsContentList(aRootNode, aFunc, aDestroyFunc, aData) {}

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  JSObject* WrapObject(JSContext* cx,
                       JS::Handle<JSObject*> aGivenProto) override;

  /**
   * Reset root, mutation observer, and clear content list
   * if the root has been changed.
   *
   * @param aRootNode The node under which to limit our search.
   */
  void MaybeResetRoot(nsINode* aRootNode);

 private:
  /**
   * Start searching at the last one if we already have nodes, otherwise
   * start searching at the root.
   *
   * @param aNeededLength The list of length should have when we are
   *                      done (unless it exhausts the document).
   * @param aExpectedElementsIfDirty is for debugging only to
   *        assert that mElements has expected number of entries.
   */
  void PopulateSelf(uint32_t aNeededLength,
                    uint32_t aExpectedElementsIfDirty = 0) override;
};
#endif  // nsContentList_h___
