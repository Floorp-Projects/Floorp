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

#include "nsContentList.h"
#include "nsIContent.h"
#include "mozilla/dom/Document.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/dom/Element.h"
#include "nsWrapperCacheInlines.h"
#include "nsContentUtils.h"
#include "nsCCUncollectableMarker.h"
#include "nsGkAtoms.h"
#include "mozilla/dom/HTMLCollectionBinding.h"
#include "mozilla/dom/NodeListBinding.h"
#include "mozilla/Likely.h"
#include "nsGenericHTMLElement.h"
#include "jsfriendapi.h"
#include <algorithm>
#include "mozilla/dom/NodeInfoInlines.h"
#include "mozilla/MruCache.h"

#include "PLDHashTable.h"

#ifdef DEBUG_CONTENT_LIST
#  define ASSERT_IN_SYNC AssertInSync()
#else
#  define ASSERT_IN_SYNC PR_BEGIN_MACRO PR_END_MACRO
#endif

using namespace mozilla;
using namespace mozilla::dom;

nsBaseContentList::~nsBaseContentList() = default;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(nsBaseContentList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsBaseContentList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mElements)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->RemoveFromCaches();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsBaseContentList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mElements)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsBaseContentList)
  if (nsCCUncollectableMarker::sGeneration && tmp->HasKnownLiveWrapper()) {
    for (uint32_t i = 0; i < tmp->mElements.Length(); ++i) {
      nsIContent* c = tmp->mElements[i];
      if (c->IsPurple()) {
        c->RemovePurple();
      }
      Element::MarkNodeChildren(c);
    }
    return true;
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsBaseContentList)
  return nsCCUncollectableMarker::sGeneration && tmp->HasKnownLiveWrapper();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsBaseContentList)
  return nsCCUncollectableMarker::sGeneration && tmp->HasKnownLiveWrapper();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

// QueryInterface implementation for nsBaseContentList
NS_INTERFACE_TABLE_HEAD(nsBaseContentList)
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
  NS_INTERFACE_TABLE(nsBaseContentList, nsINodeList)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsBaseContentList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsBaseContentList)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(nsBaseContentList,
                                                   LastRelease())

nsIContent* nsBaseContentList::Item(uint32_t aIndex) {
  return mElements.SafeElementAt(aIndex);
}

int32_t nsBaseContentList::IndexOf(nsIContent* aContent, bool aDoFlush) {
  return mElements.IndexOf(aContent);
}

int32_t nsBaseContentList::IndexOf(nsIContent* aContent) {
  return IndexOf(aContent, true);
}

size_t nsBaseContentList::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);
  n += mElements.ShallowSizeOfExcludingThis(aMallocSizeOf);
  return n;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsSimpleContentList, nsBaseContentList,
                                   mRoot)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSimpleContentList)
NS_INTERFACE_MAP_END_INHERITING(nsBaseContentList)

NS_IMPL_ADDREF_INHERITED(nsSimpleContentList, nsBaseContentList)
NS_IMPL_RELEASE_INHERITED(nsSimpleContentList, nsBaseContentList)

JSObject* nsSimpleContentList::WrapObject(JSContext* cx,
                                          JS::Handle<JSObject*> aGivenProto) {
  return NodeList_Binding::Wrap(cx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsEmptyContentList, nsBaseContentList, mRoot)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsEmptyContentList)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLCollection)
NS_INTERFACE_MAP_END_INHERITING(nsBaseContentList)

NS_IMPL_ADDREF_INHERITED(nsEmptyContentList, nsBaseContentList)
NS_IMPL_RELEASE_INHERITED(nsEmptyContentList, nsBaseContentList)

JSObject* nsEmptyContentList::WrapObject(JSContext* cx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return HTMLCollection_Binding::Wrap(cx, this, aGivenProto);
}

mozilla::dom::Element* nsEmptyContentList::GetElementAt(uint32_t index) {
  return nullptr;
}

mozilla::dom::Element* nsEmptyContentList::GetFirstNamedElement(
    const nsAString& aName, bool& aFound) {
  aFound = false;
  return nullptr;
}

void nsEmptyContentList::GetSupportedNames(nsTArray<nsString>& aNames) {}

nsIContent* nsEmptyContentList::Item(uint32_t aIndex) { return nullptr; }

// Hashtable for storing nsContentLists
static PLDHashTable* gContentListHashTable;

struct ContentListCache
    : public MruCache<nsContentListKey, nsContentList*, ContentListCache> {
  static HashNumber Hash(const nsContentListKey& aKey) {
    return aKey.GetHash();
  }
  static bool Match(const nsContentListKey& aKey, const nsContentList* aVal) {
    return aVal->MatchesKey(aKey);
  }
};

static ContentListCache sRecentlyUsedContentLists;

struct ContentListHashEntry : public PLDHashEntryHdr {
  nsContentList* mContentList;
};

static PLDHashNumber ContentListHashtableHashKey(const void* key) {
  const nsContentListKey* list = static_cast<const nsContentListKey*>(key);
  return list->GetHash();
}

static bool ContentListHashtableMatchEntry(const PLDHashEntryHdr* entry,
                                           const void* key) {
  const ContentListHashEntry* e =
      static_cast<const ContentListHashEntry*>(entry);
  const nsContentList* list = e->mContentList;
  const nsContentListKey* ourKey = static_cast<const nsContentListKey*>(key);

  return list->MatchesKey(*ourKey);
}

already_AddRefed<nsContentList> NS_GetContentList(nsINode* aRootNode,
                                                  int32_t aMatchNameSpaceId,
                                                  const nsAString& aTagname) {
  NS_ASSERTION(aRootNode, "content list has to have a root");

  RefPtr<nsContentList> list;
  nsContentListKey hashKey(aRootNode, aMatchNameSpaceId, aTagname,
                           aRootNode->OwnerDoc()->IsHTMLDocument());
  auto p = sRecentlyUsedContentLists.Lookup(hashKey);
  if (p) {
    list = p.Data();
    return list.forget();
  }

  static const PLDHashTableOps hash_table_ops = {
      ContentListHashtableHashKey, ContentListHashtableMatchEntry,
      PLDHashTable::MoveEntryStub, PLDHashTable::ClearEntryStub};

  // Initialize the hashtable if needed.
  if (!gContentListHashTable) {
    gContentListHashTable =
        new PLDHashTable(&hash_table_ops, sizeof(ContentListHashEntry));
  }

  // First we look in our hashtable.  Then we create a content list if needed
  auto entry = static_cast<ContentListHashEntry*>(
      gContentListHashTable->Add(&hashKey, fallible));
  if (entry) list = entry->mContentList;

  if (!list) {
    // We need to create a ContentList and add it to our new entry, if
    // we have an entry
    RefPtr<nsAtom> xmlAtom = NS_Atomize(aTagname);
    RefPtr<nsAtom> htmlAtom;
    if (aMatchNameSpaceId == kNameSpaceID_Unknown) {
      nsAutoString lowercaseName;
      nsContentUtils::ASCIIToLower(aTagname, lowercaseName);
      htmlAtom = NS_Atomize(lowercaseName);
    } else {
      htmlAtom = xmlAtom;
    }
    list = new nsContentList(aRootNode, aMatchNameSpaceId, htmlAtom, xmlAtom);
    if (entry) {
      entry->mContentList = list;
    }
  }

  p.Set(list);
  return list.forget();
}

#ifdef DEBUG
const nsCacheableFuncStringContentList::ContentListType
    nsCachableElementsByNameNodeList::sType =
        nsCacheableFuncStringContentList::eNodeList;
const nsCacheableFuncStringContentList::ContentListType
    nsCacheableFuncStringHTMLCollection::sType =
        nsCacheableFuncStringContentList::eHTMLCollection;
#endif

// Hashtable for storing nsCacheableFuncStringContentList
static PLDHashTable* gFuncStringContentListHashTable;

struct FuncStringContentListHashEntry : public PLDHashEntryHdr {
  nsCacheableFuncStringContentList* mContentList;
};

static PLDHashNumber FuncStringContentListHashtableHashKey(const void* key) {
  const nsFuncStringCacheKey* funcStringKey =
      static_cast<const nsFuncStringCacheKey*>(key);
  return funcStringKey->GetHash();
}

static bool FuncStringContentListHashtableMatchEntry(
    const PLDHashEntryHdr* entry, const void* key) {
  const FuncStringContentListHashEntry* e =
      static_cast<const FuncStringContentListHashEntry*>(entry);
  const nsFuncStringCacheKey* ourKey =
      static_cast<const nsFuncStringCacheKey*>(key);

  return e->mContentList->Equals(ourKey);
}

template <class ListType>
already_AddRefed<nsContentList> GetFuncStringContentList(
    nsINode* aRootNode, nsContentListMatchFunc aFunc,
    nsContentListDestroyFunc aDestroyFunc,
    nsFuncStringContentListDataAllocator aDataAllocator,
    const nsAString& aString) {
  NS_ASSERTION(aRootNode, "content list has to have a root");

  RefPtr<nsCacheableFuncStringContentList> list;

  static const PLDHashTableOps hash_table_ops = {
      FuncStringContentListHashtableHashKey,
      FuncStringContentListHashtableMatchEntry, PLDHashTable::MoveEntryStub,
      PLDHashTable::ClearEntryStub};

  // Initialize the hashtable if needed.
  if (!gFuncStringContentListHashTable) {
    gFuncStringContentListHashTable = new PLDHashTable(
        &hash_table_ops, sizeof(FuncStringContentListHashEntry));
  }

  FuncStringContentListHashEntry* entry = nullptr;
  // First we look in our hashtable.  Then we create a content list if needed
  if (gFuncStringContentListHashTable) {
    nsFuncStringCacheKey hashKey(aRootNode, aFunc, aString);

    entry = static_cast<FuncStringContentListHashEntry*>(
        gFuncStringContentListHashTable->Add(&hashKey, fallible));
    if (entry) {
      list = entry->mContentList;
#ifdef DEBUG
      MOZ_ASSERT_IF(list, list->mType == ListType::sType);
#endif
    }
  }

  if (!list) {
    // We need to create a ContentList and add it to our new entry, if
    // we have an entry
    list =
        new ListType(aRootNode, aFunc, aDestroyFunc, aDataAllocator, aString);
    if (entry) {
      entry->mContentList = list;
    }
  }

  // Don't cache these lists globally

  return list.forget();
}

// Explicit instantiations to avoid link errors
template already_AddRefed<nsContentList>
GetFuncStringContentList<nsCachableElementsByNameNodeList>(
    nsINode* aRootNode, nsContentListMatchFunc aFunc,
    nsContentListDestroyFunc aDestroyFunc,
    nsFuncStringContentListDataAllocator aDataAllocator,
    const nsAString& aString);
template already_AddRefed<nsContentList>
GetFuncStringContentList<nsCacheableFuncStringHTMLCollection>(
    nsINode* aRootNode, nsContentListMatchFunc aFunc,
    nsContentListDestroyFunc aDestroyFunc,
    nsFuncStringContentListDataAllocator aDataAllocator,
    const nsAString& aString);

//-----------------------------------------------------
// nsContentList implementation

nsContentList::nsContentList(nsINode* aRootNode, int32_t aMatchNameSpaceId,
                             nsAtom* aHTMLMatchAtom, nsAtom* aXMLMatchAtom,
                             bool aDeep, bool aLiveList)
    : nsBaseContentList(),
      mRootNode(aRootNode),
      mMatchNameSpaceId(aMatchNameSpaceId),
      mHTMLMatchAtom(aHTMLMatchAtom),
      mXMLMatchAtom(aXMLMatchAtom),
      mFunc(nullptr),
      mDestroyFunc(nullptr),
      mData(nullptr),
      mState(LIST_DIRTY),
      mDeep(aDeep),
      mFuncMayDependOnAttr(false),
      mIsHTMLDocument(aRootNode->OwnerDoc()->IsHTMLDocument()),
      mIsLiveList(aLiveList) {
  NS_ASSERTION(mRootNode, "Must have root");
  if (nsGkAtoms::_asterisk == mHTMLMatchAtom) {
    NS_ASSERTION(mXMLMatchAtom == nsGkAtoms::_asterisk,
                 "HTML atom and XML atom are not both asterisk?");
    mMatchAll = true;
  } else {
    mMatchAll = false;
  }
  if (mIsLiveList) {
    mRootNode->AddMutationObserver(this);
  }

  // We only need to flush if we're in an non-HTML document, since the
  // HTML5 parser doesn't need flushing.  Further, if we're not in a
  // document at all right now (in the GetUncomposedDoc() sense), we're
  // not parser-created and don't need to be flushing stuff under us
  // to get our kids right.
  Document* doc = mRootNode->GetUncomposedDoc();
  mFlushesNeeded = doc && !doc->IsHTMLDocument();
}

nsContentList::nsContentList(nsINode* aRootNode, nsContentListMatchFunc aFunc,
                             nsContentListDestroyFunc aDestroyFunc, void* aData,
                             bool aDeep, nsAtom* aMatchAtom,
                             int32_t aMatchNameSpaceId,
                             bool aFuncMayDependOnAttr, bool aLiveList)
    : nsBaseContentList(),
      mRootNode(aRootNode),
      mMatchNameSpaceId(aMatchNameSpaceId),
      mHTMLMatchAtom(aMatchAtom),
      mXMLMatchAtom(aMatchAtom),
      mFunc(aFunc),
      mDestroyFunc(aDestroyFunc),
      mData(aData),
      mState(LIST_DIRTY),
      mMatchAll(false),
      mDeep(aDeep),
      mFuncMayDependOnAttr(aFuncMayDependOnAttr),
      mIsHTMLDocument(false),
      mIsLiveList(aLiveList) {
  NS_ASSERTION(mRootNode, "Must have root");
  if (mIsLiveList) {
    mRootNode->AddMutationObserver(this);
  }

  // We only need to flush if we're in an non-HTML document, since the
  // HTML5 parser doesn't need flushing.  Further, if we're not in a
  // document at all right now (in the GetUncomposedDoc() sense), we're
  // not parser-created and don't need to be flushing stuff under us
  // to get our kids right.
  Document* doc = mRootNode->GetUncomposedDoc();
  mFlushesNeeded = doc && !doc->IsHTMLDocument();
}

nsContentList::~nsContentList() {
  RemoveFromHashtable();
  if (mIsLiveList && mRootNode) {
    mRootNode->RemoveMutationObserver(this);
  }

  if (mDestroyFunc) {
    // Clean up mData
    (*mDestroyFunc)(mData);
  }
}

JSObject* nsContentList::WrapObject(JSContext* cx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return HTMLCollection_Binding::Wrap(cx, this, aGivenProto);
}

NS_IMPL_ISUPPORTS_INHERITED(nsContentList, nsBaseContentList, nsIHTMLCollection,
                            nsIMutationObserver)

uint32_t nsContentList::Length(bool aDoFlush) {
  BringSelfUpToDate(aDoFlush);

  return mElements.Length();
}

nsIContent* nsContentList::Item(uint32_t aIndex, bool aDoFlush) {
  if (mRootNode && aDoFlush && mFlushesNeeded) {
    // XXX sXBL/XBL2 issue
    Document* doc = mRootNode->GetUncomposedDoc();
    if (doc) {
      // Flush pending content changes Bug 4891.
      doc->FlushPendingNotifications(FlushType::ContentAndNotify);
    }
  }

  if (mState != LIST_UP_TO_DATE)
    PopulateSelf(std::min(aIndex, UINT32_MAX - 1) + 1);

  ASSERT_IN_SYNC;
  NS_ASSERTION(!mRootNode || mState != LIST_DIRTY,
               "PopulateSelf left the list in a dirty (useless) state!");

  return mElements.SafeElementAt(aIndex);
}

Element* nsContentList::NamedItem(const nsAString& aName, bool aDoFlush) {
  if (aName.IsEmpty()) {
    return nullptr;
  }

  BringSelfUpToDate(aDoFlush);

  uint32_t i, count = mElements.Length();

  // Typically IDs and names are atomized
  RefPtr<nsAtom> name = NS_Atomize(aName);
  NS_ENSURE_TRUE(name, nullptr);

  for (i = 0; i < count; i++) {
    nsIContent* content = mElements[i];
    // XXX Should this pass eIgnoreCase?
    if (content &&
        ((content->IsHTMLElement() &&
          content->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                                            name, eCaseMatters)) ||
         content->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::id,
                                           name, eCaseMatters))) {
      return content->AsElement();
    }
  }

  return nullptr;
}

void nsContentList::GetSupportedNames(nsTArray<nsString>& aNames) {
  BringSelfUpToDate(true);

  AutoTArray<nsAtom*, 8> atoms;
  for (uint32_t i = 0; i < mElements.Length(); ++i) {
    nsIContent* content = mElements.ElementAt(i);
    if (content->HasID()) {
      nsAtom* id = content->GetID();
      MOZ_ASSERT(id != nsGkAtoms::_empty, "Empty ids don't get atomized");
      if (!atoms.Contains(id)) {
        atoms.AppendElement(id);
      }
    }

    nsGenericHTMLElement* el = nsGenericHTMLElement::FromNode(content);
    if (el) {
      // XXXbz should we be checking for particular tags here?  How
      // stable is this part of the spec?
      // Note: nsINode::HasName means the name is exposed on the document,
      // which is false for options, so we don't check it here.
      const nsAttrValue* val = el->GetParsedAttr(nsGkAtoms::name);
      if (val && val->Type() == nsAttrValue::eAtom) {
        nsAtom* name = val->GetAtomValue();
        MOZ_ASSERT(name != nsGkAtoms::_empty, "Empty names don't get atomized");
        if (!atoms.Contains(name)) {
          atoms.AppendElement(name);
        }
      }
    }
  }

  uint32_t atomsLen = atoms.Length();
  nsString* names = aNames.AppendElements(atomsLen);
  for (uint32_t i = 0; i < atomsLen; ++i) {
    atoms[i]->ToString(names[i]);
  }
}

int32_t nsContentList::IndexOf(nsIContent* aContent, bool aDoFlush) {
  BringSelfUpToDate(aDoFlush);

  return mElements.IndexOf(aContent);
}

int32_t nsContentList::IndexOf(nsIContent* aContent) {
  return IndexOf(aContent, true);
}

void nsContentList::NodeWillBeDestroyed(const nsINode* aNode) {
  // We shouldn't do anything useful from now on

  RemoveFromCaches();
  mRootNode = nullptr;

  // We will get no more updates, so we can never know we're up to
  // date
  SetDirty();
}

void nsContentList::LastRelease() {
  RemoveFromCaches();
  if (mIsLiveList && mRootNode) {
    mRootNode->RemoveMutationObserver(this);
    mRootNode = nullptr;
  }
  SetDirty();
}

Element* nsContentList::GetElementAt(uint32_t aIndex) {
  return static_cast<Element*>(Item(aIndex, true));
}

nsIContent* nsContentList::Item(uint32_t aIndex) {
  return GetElementAt(aIndex);
}

void nsContentList::AttributeChanged(Element* aElement, int32_t aNameSpaceID,
                                     nsAtom* aAttribute, int32_t aModType,
                                     const nsAttrValue* aOldValue) {
  MOZ_ASSERT(aElement, "Must have a content node to work with");

  if (!mFunc || !mFuncMayDependOnAttr || mState == LIST_DIRTY ||
      !MayContainRelevantNodes(aElement->GetParentNode()) ||
      !nsContentUtils::IsInSameAnonymousTree(mRootNode, aElement)) {
    // Either we're already dirty or this notification doesn't affect
    // whether we might match aElement.
    return;
  }

  if (Match(aElement)) {
    if (mElements.IndexOf(aElement) == mElements.NoIndex) {
      // We match aElement now, and it's not in our list already.  Just dirty
      // ourselves; this is simpler than trying to figure out where to insert
      // aElement.
      SetDirty();
    }
  } else {
    // We no longer match aElement.  Remove it from our list.  If it's
    // already not there, this is a no-op (though a potentially
    // expensive one).  Either way, no change of mState is required
    // here.
    mElements.RemoveElement(aElement);
  }
}

void nsContentList::ContentAppended(nsIContent* aFirstNewContent) {
  nsIContent* container = aFirstNewContent->GetParent();
  MOZ_ASSERT(container, "Can't get at the new content if no container!");

  /*
   * If the state is LIST_DIRTY then we have no useful information in our list
   * and we want to put off doing work as much as possible.
   *
   * Also, if container is anonymous from our point of view, we know that we
   * can't possibly be matching any of the kids.
   *
   * Optimize out also the common case when just one new node is appended and
   * it doesn't match us.
   */
  if (mState == LIST_DIRTY ||
      !nsContentUtils::IsInSameAnonymousTree(mRootNode, container) ||
      !MayContainRelevantNodes(container) ||
      (!aFirstNewContent->HasChildren() &&
       !aFirstNewContent->GetNextSibling() && !MatchSelf(aFirstNewContent))) {
    return;
  }

  /*
   * We want to handle the case of ContentAppended by sometimes
   * appending the content to our list, not just setting state to
   * LIST_DIRTY, since most of our ContentAppended notifications
   * should come during pageload and be at the end of the document.
   * Do a bit of work to see whether we could just append to what we
   * already have.
   */

  uint32_t ourCount = mElements.Length();
  const bool appendingToList = [&] {
    if (ourCount == 0) {
      return true;
    }
    if (mRootNode == container) {
      return true;
    }
    return nsContentUtils::PositionIsBefore(mElements.LastElement(),
                                            aFirstNewContent);
  }();

  if (!appendingToList) {
    // The new stuff is somewhere in the middle of our list; check
    // whether we need to invalidate
    for (nsIContent* cur = aFirstNewContent; cur; cur = cur->GetNextSibling()) {
      if (MatchSelf(cur)) {
        // Uh-oh.  We're gonna have to add elements into the middle
        // of our list. That's not worth the effort.
        SetDirty();
        break;
      }
    }

    ASSERT_IN_SYNC;
    return;
  }

  /*
   * At this point we know we could append.  If we're not up to
   * date, however, that would be a bad idea -- it could miss some
   * content that we never picked up due to being lazy.  Further, we
   * may never get asked for this content... so don't grab it yet.
   */
  if (mState == LIST_LAZY) {
    return;
  }

  /*
   * We're up to date.  That means someone's actively using us; we
   * may as well grab this content....
   */
  if (mDeep) {
    for (nsIContent* cur = aFirstNewContent; cur;
         cur = cur->GetNextNode(container)) {
      if (cur->IsElement() && Match(cur->AsElement())) {
        mElements.AppendElement(cur);
      }
    }
  } else {
    for (nsIContent* cur = aFirstNewContent; cur; cur = cur->GetNextSibling()) {
      if (cur->IsElement() && Match(cur->AsElement())) {
        mElements.AppendElement(cur);
      }
    }
  }

  ASSERT_IN_SYNC;
}

void nsContentList::ContentInserted(nsIContent* aChild) {
  // Note that aChild->GetParentNode() can be null here if we are inserting into
  // the document itself; any attempted optimizations to this method should deal
  // with that.
  if (mState != LIST_DIRTY &&
      MayContainRelevantNodes(aChild->GetParentNode()) &&
      nsContentUtils::IsInSameAnonymousTree(mRootNode, aChild) &&
      MatchSelf(aChild)) {
    SetDirty();
  }

  ASSERT_IN_SYNC;
}

void nsContentList::ContentRemoved(nsIContent* aChild,
                                   nsIContent* aPreviousSibling) {
  if (mState != LIST_DIRTY &&
      MayContainRelevantNodes(aChild->GetParentNode()) &&
      nsContentUtils::IsInSameAnonymousTree(mRootNode, aChild) &&
      MatchSelf(aChild)) {
    SetDirty();
  }

  ASSERT_IN_SYNC;
}

bool nsContentList::Match(Element* aElement) {
  if (mFunc) {
    return (*mFunc)(aElement, mMatchNameSpaceId, mXMLMatchAtom, mData);
  }

  if (!mXMLMatchAtom) return false;

  NodeInfo* ni = aElement->NodeInfo();

  bool unknown = mMatchNameSpaceId == kNameSpaceID_Unknown;
  bool wildcard = mMatchNameSpaceId == kNameSpaceID_Wildcard;
  bool toReturn = mMatchAll;
  if (!unknown && !wildcard) toReturn &= ni->NamespaceEquals(mMatchNameSpaceId);

  if (toReturn) return toReturn;

  bool matchHTML =
      mIsHTMLDocument && aElement->GetNameSpaceID() == kNameSpaceID_XHTML;

  if (unknown) {
    return matchHTML ? ni->QualifiedNameEquals(mHTMLMatchAtom)
                     : ni->QualifiedNameEquals(mXMLMatchAtom);
  }

  if (wildcard) {
    return matchHTML ? ni->Equals(mHTMLMatchAtom) : ni->Equals(mXMLMatchAtom);
  }

  return matchHTML ? ni->Equals(mHTMLMatchAtom, mMatchNameSpaceId)
                   : ni->Equals(mXMLMatchAtom, mMatchNameSpaceId);
}

bool nsContentList::MatchSelf(nsIContent* aContent) {
  MOZ_ASSERT(aContent, "Can't match null stuff, you know");
  MOZ_ASSERT(mDeep || aContent->GetParentNode() == mRootNode,
             "MatchSelf called on a node that we can't possibly match");

  if (!aContent->IsElement()) {
    return false;
  }

  if (Match(aContent->AsElement())) return true;

  if (!mDeep) return false;

  for (nsIContent* cur = aContent->GetFirstChild(); cur;
       cur = cur->GetNextNode(aContent)) {
    if (cur->IsElement() && Match(cur->AsElement())) {
      return true;
    }
  }

  return false;
}

void nsContentList::PopulateSelf(uint32_t aNeededLength,
                                 uint32_t aExpectedElementsIfDirty) {
  if (!mRootNode) {
    return;
  }

  ASSERT_IN_SYNC;

  uint32_t count = mElements.Length();
  NS_ASSERTION(mState != LIST_DIRTY || count == aExpectedElementsIfDirty,
               "Reset() not called when setting state to LIST_DIRTY?");

  if (count >= aNeededLength)  // We're all set
    return;

  uint32_t elementsToAppend = aNeededLength - count;
#ifdef DEBUG
  uint32_t invariant = elementsToAppend + mElements.Length();
#endif

  if (mDeep) {
    // If we already have nodes start searching at the last one, otherwise
    // start searching at the root.
    nsINode* cur = count ? mElements[count - 1].get() : mRootNode;
    do {
      cur = cur->GetNextNode(mRootNode);
      if (!cur) {
        break;
      }
      if (cur->IsElement() && Match(cur->AsElement())) {
        // Append AsElement() to get nsIContent instead of nsINode
        mElements.AppendElement(cur->AsElement());
        --elementsToAppend;
      }
    } while (elementsToAppend);
  } else {
    nsIContent* cur = count ? mElements[count - 1]->GetNextSibling()
                            : mRootNode->GetFirstChild();
    for (; cur && elementsToAppend; cur = cur->GetNextSibling()) {
      if (cur->IsElement() && Match(cur->AsElement())) {
        mElements.AppendElement(cur);
        --elementsToAppend;
      }
    }
  }

  NS_ASSERTION(elementsToAppend + mElements.Length() == invariant,
               "Something is awry!");

  if (elementsToAppend != 0)
    mState = LIST_UP_TO_DATE;
  else
    mState = LIST_LAZY;

  ASSERT_IN_SYNC;
}

void nsContentList::RemoveFromHashtable() {
  if (mFunc) {
    // This can't be in the table anyway
    return;
  }

  nsDependentAtomString str(mXMLMatchAtom);
  nsContentListKey key(mRootNode, mMatchNameSpaceId, str, mIsHTMLDocument);
  sRecentlyUsedContentLists.Remove(key);

  if (!gContentListHashTable) return;

  gContentListHashTable->Remove(&key);

  if (gContentListHashTable->EntryCount() == 0) {
    delete gContentListHashTable;
    gContentListHashTable = nullptr;
  }
}

void nsContentList::BringSelfUpToDate(bool aDoFlush) {
  if (mRootNode && aDoFlush && mFlushesNeeded) {
    // XXX sXBL/XBL2 issue
    Document* doc = mRootNode->GetUncomposedDoc();
    if (doc) {
      // Flush pending content changes Bug 4891.
      doc->FlushPendingNotifications(FlushType::ContentAndNotify);
    }
  }

  if (mState != LIST_UP_TO_DATE) PopulateSelf(uint32_t(-1));

  ASSERT_IN_SYNC;
  NS_ASSERTION(!mRootNode || mState == LIST_UP_TO_DATE,
               "PopulateSelf dod not bring content list up to date!");
}

nsCacheableFuncStringContentList::~nsCacheableFuncStringContentList() {
  RemoveFromFuncStringHashtable();
}

void nsCacheableFuncStringContentList::RemoveFromFuncStringHashtable() {
  if (!gFuncStringContentListHashTable) {
    return;
  }

  nsFuncStringCacheKey key(mRootNode, mFunc, mString);
  gFuncStringContentListHashTable->Remove(&key);

  if (gFuncStringContentListHashTable->EntryCount() == 0) {
    delete gFuncStringContentListHashTable;
    gFuncStringContentListHashTable = nullptr;
  }
}

#ifdef DEBUG_CONTENT_LIST
void nsContentList::AssertInSync() {
  if (mState == LIST_DIRTY) {
    return;
  }

  if (!mRootNode) {
    NS_ASSERTION(mElements.Length() == 0 && mState == LIST_DIRTY,
                 "Empty iterator isn't quite empty?");
    return;
  }

  // XXX This code will need to change if nsContentLists can ever match
  // elements that are outside of the document element.
  nsIContent* root = mRootNode->IsDocument()
                         ? mRootNode->AsDocument()->GetRootElement()
                         : mRootNode->AsContent();

  PreContentIterator preOrderIter;
  if (mDeep) {
    preOrderIter.Init(root);
    preOrderIter.First();
  }

  uint32_t cnt = 0, index = 0;
  while (true) {
    if (cnt == mElements.Length() && mState == LIST_LAZY) {
      break;
    }

    nsIContent* cur =
        mDeep ? preOrderIter.GetCurrentNode() : mRootNode->GetChildAt(index++);
    if (!cur) {
      break;
    }

    if (cur->IsElement() && Match(cur->AsElement())) {
      NS_ASSERTION(cnt < mElements.Length() && mElements[cnt] == cur,
                   "Elements is out of sync");
      ++cnt;
    }

    if (mDeep) {
      preOrderIter.Next();
    }
  }

  NS_ASSERTION(cnt == mElements.Length(), "Too few elements");
}
#endif

//-----------------------------------------------------
// nsCachableElementsByNameNodeList

JSObject* nsCachableElementsByNameNodeList::WrapObject(
    JSContext* cx, JS::Handle<JSObject*> aGivenProto) {
  return NodeList_Binding::Wrap(cx, this, aGivenProto);
}

void nsCachableElementsByNameNodeList::AttributeChanged(
    Element* aElement, int32_t aNameSpaceID, nsAtom* aAttribute,
    int32_t aModType, const nsAttrValue* aOldValue) {
  // No need to rebuild the list if the changed attribute is not the name
  // attribute.
  if (aAttribute != nsGkAtoms::name) {
    return;
  }

  nsCacheableFuncStringContentList::AttributeChanged(
      aElement, aNameSpaceID, aAttribute, aModType, aOldValue);
}

//-----------------------------------------------------
// nsCacheableFuncStringHTMLCollection

JSObject* nsCacheableFuncStringHTMLCollection::WrapObject(
    JSContext* cx, JS::Handle<JSObject*> aGivenProto) {
  return HTMLCollection_Binding::Wrap(cx, this, aGivenProto);
}

//-----------------------------------------------------
// nsLabelsNodeList

JSObject* nsLabelsNodeList::WrapObject(JSContext* cx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return NodeList_Binding::Wrap(cx, this, aGivenProto);
}

void nsLabelsNodeList::AttributeChanged(Element* aElement, int32_t aNameSpaceID,
                                        nsAtom* aAttribute, int32_t aModType,
                                        const nsAttrValue* aOldValue) {
  MOZ_ASSERT(aElement, "Must have a content node to work with");
  if (mState == LIST_DIRTY ||
      !nsContentUtils::IsInSameAnonymousTree(mRootNode, aElement)) {
    return;
  }

  // We need to handle input type changes to or from "hidden".
  if (aElement->IsHTMLElement(nsGkAtoms::input) &&
      aAttribute == nsGkAtoms::type && aNameSpaceID == kNameSpaceID_None) {
    SetDirty();
    return;
  }
}

void nsLabelsNodeList::ContentAppended(nsIContent* aFirstNewContent) {
  nsIContent* container = aFirstNewContent->GetParent();
  // If a labelable element is moved to outside or inside of
  // nested associated labels, we're gonna have to modify
  // the content list.
  if (mState != LIST_DIRTY ||
      nsContentUtils::IsInSameAnonymousTree(mRootNode, container)) {
    SetDirty();
    return;
  }
}

void nsLabelsNodeList::ContentInserted(nsIContent* aChild) {
  // If a labelable element is moved to outside or inside of
  // nested associated labels, we're gonna have to modify
  // the content list.
  if (mState != LIST_DIRTY ||
      nsContentUtils::IsInSameAnonymousTree(mRootNode, aChild)) {
    SetDirty();
    return;
  }
}

void nsLabelsNodeList::ContentRemoved(nsIContent* aChild,
                                      nsIContent* aPreviousSibling) {
  // If a labelable element is removed, we're gonna have to clean
  // the content list.
  if (mState != LIST_DIRTY ||
      nsContentUtils::IsInSameAnonymousTree(mRootNode, aChild)) {
    SetDirty();
    return;
  }
}

void nsLabelsNodeList::MaybeResetRoot(nsINode* aRootNode) {
  MOZ_ASSERT(aRootNode, "Must have root");
  if (mRootNode == aRootNode) {
    return;
  }

  MOZ_ASSERT(mIsLiveList, "nsLabelsNodeList is always a live list");
  if (mRootNode) {
    mRootNode->RemoveMutationObserver(this);
  }
  mRootNode = aRootNode;
  mRootNode->AddMutationObserver(this);
  SetDirty();
}

void nsLabelsNodeList::PopulateSelf(uint32_t aNeededLength,
                                    uint32_t aExpectedElementsIfDirty) {
  if (!mRootNode) {
    return;
  }

  // Start searching at the root.
  nsINode* cur = mRootNode;
  if (mElements.IsEmpty() && cur->IsElement() && Match(cur->AsElement())) {
    mElements.AppendElement(cur->AsElement());
    ++aExpectedElementsIfDirty;
  }

  nsContentList::PopulateSelf(aNeededLength, aExpectedElementsIfDirty);
}
