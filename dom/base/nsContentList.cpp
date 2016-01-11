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

#include "nsContentList.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIDocument.h"
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

// Form related includes
#include "nsIDOMHTMLFormElement.h"

#include "PLDHashTable.h"

#ifdef DEBUG_CONTENT_LIST
#include "nsIContentIterator.h"
#define ASSERT_IN_SYNC AssertInSync()
#else
#define ASSERT_IN_SYNC PR_BEGIN_MACRO PR_END_MACRO
#endif

using namespace mozilla;
using namespace mozilla::dom;

nsBaseContentList::~nsBaseContentList()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsBaseContentList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsBaseContentList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mElements)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->RemoveFromCaches();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsBaseContentList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mElements)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(nsBaseContentList)

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsBaseContentList)
  if (nsCCUncollectableMarker::sGeneration && tmp->IsBlack()) {
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
  return nsCCUncollectableMarker::sGeneration && tmp->IsBlack();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsBaseContentList)
  return nsCCUncollectableMarker::sGeneration && tmp->IsBlack();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

#define NS_CONTENT_LIST_INTERFACES(_class)                                    \
    NS_INTERFACE_TABLE_ENTRY(_class, nsINodeList)                             \
    NS_INTERFACE_TABLE_ENTRY(_class, nsIDOMNodeList)

// QueryInterface implementation for nsBaseContentList
NS_INTERFACE_TABLE_HEAD(nsBaseContentList)
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
  NS_INTERFACE_TABLE(nsBaseContentList, nsINodeList, nsIDOMNodeList)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsBaseContentList)
NS_INTERFACE_MAP_END


NS_IMPL_CYCLE_COLLECTING_ADDREF(nsBaseContentList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsBaseContentList)


NS_IMETHODIMP
nsBaseContentList::GetLength(uint32_t* aLength)
{
  *aLength = mElements.Length();

  return NS_OK;
}

NS_IMETHODIMP
nsBaseContentList::Item(uint32_t aIndex, nsIDOMNode** aReturn)
{
  nsISupports *tmp = Item(aIndex);

  if (!tmp) {
    *aReturn = nullptr;

    return NS_OK;
  }

  return CallQueryInterface(tmp, aReturn);
}

nsIContent*
nsBaseContentList::Item(uint32_t aIndex)
{
  return mElements.SafeElementAt(aIndex);
}


int32_t
nsBaseContentList::IndexOf(nsIContent *aContent, bool aDoFlush)
{
  return mElements.IndexOf(aContent);
}

int32_t
nsBaseContentList::IndexOf(nsIContent* aContent)
{
  return IndexOf(aContent, true);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsSimpleContentList, nsBaseContentList,
                                   mRoot)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsSimpleContentList)
NS_INTERFACE_MAP_END_INHERITING(nsBaseContentList)


NS_IMPL_ADDREF_INHERITED(nsSimpleContentList, nsBaseContentList)
NS_IMPL_RELEASE_INHERITED(nsSimpleContentList, nsBaseContentList)

JSObject*
nsSimpleContentList::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return NodeListBinding::Wrap(cx, this, aGivenProto);
}

// Hashtable for storing nsContentLists
static PLDHashTable* gContentListHashTable;

#define RECENTLY_USED_CONTENT_LIST_CACHE_SIZE 31
static nsContentList*
  sRecentlyUsedContentLists[RECENTLY_USED_CONTENT_LIST_CACHE_SIZE] = {};

static MOZ_ALWAYS_INLINE uint32_t
RecentlyUsedCacheIndex(const nsContentListKey& aKey)
{
  return aKey.GetHash() % RECENTLY_USED_CONTENT_LIST_CACHE_SIZE;
}

struct ContentListHashEntry : public PLDHashEntryHdr
{
  nsContentList* mContentList;
};

static PLDHashNumber
ContentListHashtableHashKey(PLDHashTable *table, const void *key)
{
  const nsContentListKey* list = static_cast<const nsContentListKey *>(key);
  return list->GetHash();
}

static bool
ContentListHashtableMatchEntry(PLDHashTable *table,
                               const PLDHashEntryHdr *entry,
                               const void *key)
{
  const ContentListHashEntry *e =
    static_cast<const ContentListHashEntry *>(entry);
  const nsContentList* list = e->mContentList;
  const nsContentListKey* ourKey = static_cast<const nsContentListKey *>(key);

  return list->MatchesKey(*ourKey);
}

already_AddRefed<nsContentList>
NS_GetContentList(nsINode* aRootNode, 
                  int32_t  aMatchNameSpaceId,
                  const nsAString& aTagname)
{
  NS_ASSERTION(aRootNode, "content list has to have a root");

  RefPtr<nsContentList> list;
  nsContentListKey hashKey(aRootNode, aMatchNameSpaceId, aTagname,
                           aRootNode->OwnerDoc()->IsHTMLDocument());
  uint32_t recentlyUsedCacheIndex = RecentlyUsedCacheIndex(hashKey);
  nsContentList* cachedList = sRecentlyUsedContentLists[recentlyUsedCacheIndex];
  if (cachedList && cachedList->MatchesKey(hashKey)) {
    list = cachedList;
    return list.forget();
  }

  static const PLDHashTableOps hash_table_ops =
  {
    ContentListHashtableHashKey,
    ContentListHashtableMatchEntry,
    PLDHashTable::MoveEntryStub,
    PLDHashTable::ClearEntryStub
  };

  // Initialize the hashtable if needed.
  if (!gContentListHashTable) {
    gContentListHashTable =
      new PLDHashTable(&hash_table_ops, sizeof(ContentListHashEntry));
  }

  // First we look in our hashtable.  Then we create a content list if needed
  auto entry = static_cast<ContentListHashEntry*>
                          (gContentListHashTable->Add(&hashKey, fallible));
  if (entry)
    list = entry->mContentList;

  if (!list) {
    // We need to create a ContentList and add it to our new entry, if
    // we have an entry
    nsCOMPtr<nsIAtom> xmlAtom = do_GetAtom(aTagname);
    nsCOMPtr<nsIAtom> htmlAtom;
    if (aMatchNameSpaceId == kNameSpaceID_Unknown) {
      nsAutoString lowercaseName;
      nsContentUtils::ASCIIToLower(aTagname, lowercaseName);
      htmlAtom = do_GetAtom(lowercaseName);
    } else {
      htmlAtom = xmlAtom;
    }
    list = new nsContentList(aRootNode, aMatchNameSpaceId, htmlAtom, xmlAtom);
    if (entry) {
      entry->mContentList = list;
    }
  }

  sRecentlyUsedContentLists[recentlyUsedCacheIndex] = list;
  return list.forget();
}

#ifdef DEBUG
const nsCacheableFuncStringContentList::ContentListType
  nsCacheableFuncStringNodeList::sType = nsCacheableFuncStringContentList::eNodeList;
const nsCacheableFuncStringContentList::ContentListType
  nsCacheableFuncStringHTMLCollection::sType = nsCacheableFuncStringContentList::eHTMLCollection;
#endif

JSObject*
nsCacheableFuncStringNodeList::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return NodeListBinding::Wrap(cx, this, aGivenProto);
}


JSObject*
nsCacheableFuncStringHTMLCollection::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLCollectionBinding::Wrap(cx, this, aGivenProto);
}

// Hashtable for storing nsCacheableFuncStringContentList
static PLDHashTable* gFuncStringContentListHashTable;

struct FuncStringContentListHashEntry : public PLDHashEntryHdr
{
  nsCacheableFuncStringContentList* mContentList;
};

static PLDHashNumber
FuncStringContentListHashtableHashKey(PLDHashTable *table, const void *key)
{
  const nsFuncStringCacheKey* funcStringKey =
    static_cast<const nsFuncStringCacheKey *>(key);
  return funcStringKey->GetHash();
}

static bool
FuncStringContentListHashtableMatchEntry(PLDHashTable *table,
                               const PLDHashEntryHdr *entry,
                               const void *key)
{
  const FuncStringContentListHashEntry *e =
    static_cast<const FuncStringContentListHashEntry *>(entry);
  const nsFuncStringCacheKey* ourKey =
    static_cast<const nsFuncStringCacheKey *>(key);

  return e->mContentList->Equals(ourKey);
}

template<class ListType>
already_AddRefed<nsContentList>
GetFuncStringContentList(nsINode* aRootNode,
                         nsContentListMatchFunc aFunc,
                         nsContentListDestroyFunc aDestroyFunc,
                         nsFuncStringContentListDataAllocator aDataAllocator,
                         const nsAString& aString)
{
  NS_ASSERTION(aRootNode, "content list has to have a root");

  RefPtr<nsCacheableFuncStringContentList> list;

  static const PLDHashTableOps hash_table_ops =
  {
    FuncStringContentListHashtableHashKey,
    FuncStringContentListHashtableMatchEntry,
    PLDHashTable::MoveEntryStub,
    PLDHashTable::ClearEntryStub
  };

  // Initialize the hashtable if needed.
  if (!gFuncStringContentListHashTable) {
    gFuncStringContentListHashTable =
      new PLDHashTable(&hash_table_ops, sizeof(FuncStringContentListHashEntry));
  }

  FuncStringContentListHashEntry *entry = nullptr;
  // First we look in our hashtable.  Then we create a content list if needed
  if (gFuncStringContentListHashTable) {
    nsFuncStringCacheKey hashKey(aRootNode, aFunc, aString);

    entry = static_cast<FuncStringContentListHashEntry*>
      (gFuncStringContentListHashTable->Add(&hashKey, fallible));
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
    list = new ListType(aRootNode, aFunc, aDestroyFunc, aDataAllocator,
                        aString);
    if (entry) {
      entry->mContentList = list;
    }
  }

  // Don't cache these lists globally

  return list.forget();
}

already_AddRefed<nsContentList>
NS_GetFuncStringNodeList(nsINode* aRootNode,
                         nsContentListMatchFunc aFunc,
                         nsContentListDestroyFunc aDestroyFunc,
                         nsFuncStringContentListDataAllocator aDataAllocator,
                         const nsAString& aString)
{
  return GetFuncStringContentList<nsCacheableFuncStringNodeList>(aRootNode,
                                                                 aFunc,
                                                                 aDestroyFunc,
                                                                 aDataAllocator,
                                                                 aString);
}

already_AddRefed<nsContentList>
NS_GetFuncStringHTMLCollection(nsINode* aRootNode,
                               nsContentListMatchFunc aFunc,
                               nsContentListDestroyFunc aDestroyFunc,
                               nsFuncStringContentListDataAllocator aDataAllocator,
                               const nsAString& aString)
{
  return GetFuncStringContentList<nsCacheableFuncStringHTMLCollection>(aRootNode,
                                                                       aFunc,
                                                                       aDestroyFunc,
                                                                       aDataAllocator,
                                                                       aString);
}

// nsContentList implementation

nsContentList::nsContentList(nsINode* aRootNode,
                             int32_t aMatchNameSpaceId,
                             nsIAtom* aHTMLMatchAtom,
                             nsIAtom* aXMLMatchAtom,
                             bool aDeep)
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
    mIsHTMLDocument(aRootNode->OwnerDoc()->IsHTMLDocument())
{
  NS_ASSERTION(mRootNode, "Must have root");
  if (nsGkAtoms::_asterisk == mHTMLMatchAtom) {
    NS_ASSERTION(mXMLMatchAtom == nsGkAtoms::_asterisk, "HTML atom and XML atom are not both asterisk?");
    mMatchAll = true;
  }
  else {
    mMatchAll = false;
  }
  mRootNode->AddMutationObserver(this);

  // We only need to flush if we're in an non-HTML document, since the
  // HTML5 parser doesn't need flushing.  Further, if we're not in a
  // document at all right now (in the GetUncomposedDoc() sense), we're
  // not parser-created and don't need to be flushing stuff under us
  // to get our kids right.
  nsIDocument* doc = mRootNode->GetUncomposedDoc();
  mFlushesNeeded = doc && !doc->IsHTMLDocument();
}

nsContentList::nsContentList(nsINode* aRootNode,
                             nsContentListMatchFunc aFunc,
                             nsContentListDestroyFunc aDestroyFunc,
                             void* aData,
                             bool aDeep,
                             nsIAtom* aMatchAtom,
                             int32_t aMatchNameSpaceId,
                             bool aFuncMayDependOnAttr)
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
    mIsHTMLDocument(false)
{
  NS_ASSERTION(mRootNode, "Must have root");
  mRootNode->AddMutationObserver(this);

  // We only need to flush if we're in an non-HTML document, since the
  // HTML5 parser doesn't need flushing.  Further, if we're not in a
  // document at all right now (in the GetUncomposedDoc() sense), we're
  // not parser-created and don't need to be flushing stuff under us
  // to get our kids right.
  nsIDocument* doc = mRootNode->GetUncomposedDoc();
  mFlushesNeeded = doc && !doc->IsHTMLDocument();
}

nsContentList::~nsContentList()
{
  RemoveFromHashtable();
  if (mRootNode) {
    mRootNode->RemoveMutationObserver(this);
  }

  if (mDestroyFunc) {
    // Clean up mData
    (*mDestroyFunc)(mData);
  }
}

JSObject*
nsContentList::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLCollectionBinding::Wrap(cx, this, aGivenProto);
}

NS_IMPL_ISUPPORTS_INHERITED(nsContentList, nsBaseContentList,
                            nsIHTMLCollection, nsIDOMHTMLCollection,
                            nsIMutationObserver)

uint32_t
nsContentList::Length(bool aDoFlush)
{
  BringSelfUpToDate(aDoFlush);
    
  return mElements.Length();
}

nsIContent *
nsContentList::Item(uint32_t aIndex, bool aDoFlush)
{
  if (mRootNode && aDoFlush && mFlushesNeeded) {
    // XXX sXBL/XBL2 issue
    nsIDocument* doc = mRootNode->GetUncomposedDoc();
    if (doc) {
      // Flush pending content changes Bug 4891.
      doc->FlushPendingNotifications(Flush_ContentAndNotify);
    }
  }

  if (mState != LIST_UP_TO_DATE)
    PopulateSelf(std::min(aIndex, UINT32_MAX - 1) + 1);

  ASSERT_IN_SYNC;
  NS_ASSERTION(!mRootNode || mState != LIST_DIRTY,
               "PopulateSelf left the list in a dirty (useless) state!");

  return mElements.SafeElementAt(aIndex);
}

Element*
nsContentList::NamedItem(const nsAString& aName, bool aDoFlush)
{
  if (aName.IsEmpty()) {
    return nullptr;
  }

  BringSelfUpToDate(aDoFlush);

  uint32_t i, count = mElements.Length();

  // Typically IDs and names are atomized
  nsCOMPtr<nsIAtom> name = do_GetAtom(aName);
  NS_ENSURE_TRUE(name, nullptr);

  for (i = 0; i < count; i++) {
    nsIContent *content = mElements[i];
    // XXX Should this pass eIgnoreCase?
    if (content &&
        ((content->IsHTMLElement() &&
          content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                               name, eCaseMatters)) ||
         content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::id,
                              name, eCaseMatters))) {
      return content->AsElement();
    }
  }

  return nullptr;
}

void
nsContentList::GetSupportedNames(unsigned aFlags, nsTArray<nsString>& aNames)
{
  if (!(aFlags & JSITER_HIDDEN)) {
    return;
  }

  BringSelfUpToDate(true);

  nsAutoTArray<nsIAtom*, 8> atoms;
  for (uint32_t i = 0; i < mElements.Length(); ++i) {
    nsIContent *content = mElements.ElementAt(i);
    if (content->HasID()) {
      nsIAtom* id = content->GetID();
      MOZ_ASSERT(id != nsGkAtoms::_empty,
                 "Empty ids don't get atomized");
      if (!atoms.Contains(id)) {
        atoms.AppendElement(id);
      }
    }

    nsGenericHTMLElement* el = nsGenericHTMLElement::FromContent(content);
    if (el) {
      // XXXbz should we be checking for particular tags here?  How
      // stable is this part of the spec?
      // Note: nsINode::HasName means the name is exposed on the document,
      // which is false for options, so we don't check it here.
      const nsAttrValue* val = el->GetParsedAttr(nsGkAtoms::name);
      if (val && val->Type() == nsAttrValue::eAtom) {
        nsIAtom* name = val->GetAtomValue();
        MOZ_ASSERT(name != nsGkAtoms::_empty,
                   "Empty names don't get atomized");
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

int32_t
nsContentList::IndexOf(nsIContent *aContent, bool aDoFlush)
{
  BringSelfUpToDate(aDoFlush);
    
  return mElements.IndexOf(aContent);
}

int32_t
nsContentList::IndexOf(nsIContent* aContent)
{
  return IndexOf(aContent, true);
}

void
nsContentList::NodeWillBeDestroyed(const nsINode* aNode)
{
  // We shouldn't do anything useful from now on

  RemoveFromCaches();
  mRootNode = nullptr;

  // We will get no more updates, so we can never know we're up to
  // date
  SetDirty();
}

NS_IMETHODIMP
nsContentList::GetLength(uint32_t* aLength)
{
  *aLength = Length(true);

  return NS_OK;
}

NS_IMETHODIMP
nsContentList::Item(uint32_t aIndex, nsIDOMNode** aReturn)
{
  nsINode* node = Item(aIndex);

  if (node) {
    return CallQueryInterface(node, aReturn);
  }

  *aReturn = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsContentList::NamedItem(const nsAString& aName, nsIDOMNode** aReturn)
{
  nsIContent *content = NamedItem(aName, true);

  if (content) {
    return CallQueryInterface(content, aReturn);
  }

  *aReturn = nullptr;

  return NS_OK;
}

Element*
nsContentList::GetElementAt(uint32_t aIndex)
{
  return static_cast<Element*>(Item(aIndex, true));
}

nsIContent*
nsContentList::Item(uint32_t aIndex)
{
  return GetElementAt(aIndex);
}

void
nsContentList::AttributeChanged(nsIDocument *aDocument, Element* aElement,
                                int32_t aNameSpaceID, nsIAtom* aAttribute,
                                int32_t aModType,
                                const nsAttrValue* aOldValue)
{
  NS_PRECONDITION(aElement, "Must have a content node to work with");
  
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

void
nsContentList::ContentAppended(nsIDocument* aDocument, nsIContent* aContainer,
                               nsIContent* aFirstNewContent,
                               int32_t aNewIndexInContainer)
{
  NS_PRECONDITION(aContainer, "Can't get at the new content if no container!");
  
  /*
   * If the state is LIST_DIRTY then we have no useful information in our list
   * and we want to put off doing work as much as possible.
   *
   * Also, if aContainer is anonymous from our point of view, we know that we
   * can't possibly be matching any of the kids.
   *
   * Optimize out also the common case when just one new node is appended and
   * it doesn't match us.
   */
  if (mState == LIST_DIRTY ||
      !nsContentUtils::IsInSameAnonymousTree(mRootNode, aContainer) ||
      !MayContainRelevantNodes(aContainer) ||
      (!aFirstNewContent->HasChildren() &&
       !aFirstNewContent->GetNextSibling() &&
       !MatchSelf(aFirstNewContent))) {
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
  
  int32_t count = aContainer->GetChildCount();

  if (count > 0) {
    uint32_t ourCount = mElements.Length();
    bool appendToList = false;
    if (ourCount == 0) {
      appendToList = true;
    } else {
      nsIContent* ourLastContent = mElements[ourCount - 1];
      /*
       * We want to append instead of invalidating if the first thing
       * that got appended comes after ourLastContent.
       */
      if (nsContentUtils::PositionIsBefore(ourLastContent, aFirstNewContent)) {
        appendToList = true;
      }
    }
    

    if (!appendToList) {
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
    if (mState == LIST_LAZY) // be lazy
      return;

    /*
     * We're up to date.  That means someone's actively using us; we
     * may as well grab this content....
     */
    if (mDeep) {
      for (nsIContent* cur = aFirstNewContent;
           cur;
           cur = cur->GetNextNode(aContainer)) {
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
}

void
nsContentList::ContentInserted(nsIDocument *aDocument,
                               nsIContent* aContainer,
                               nsIContent* aChild,
                               int32_t aIndexInContainer)
{
  // Note that aContainer can be null here if we are inserting into
  // the document itself; any attempted optimizations to this method
  // should deal with that.
  if (mState != LIST_DIRTY &&
      MayContainRelevantNodes(NODE_FROM(aContainer, aDocument)) &&
      nsContentUtils::IsInSameAnonymousTree(mRootNode, aChild) &&
      MatchSelf(aChild)) {
    SetDirty();
  }

  ASSERT_IN_SYNC;
}
 
void
nsContentList::ContentRemoved(nsIDocument *aDocument,
                              nsIContent* aContainer,
                              nsIContent* aChild,
                              int32_t aIndexInContainer,
                              nsIContent* aPreviousSibling)
{
  // Note that aContainer can be null here if we are removing from
  // the document itself; any attempted optimizations to this method
  // should deal with that.
  if (mState != LIST_DIRTY &&
      MayContainRelevantNodes(NODE_FROM(aContainer, aDocument)) &&
      nsContentUtils::IsInSameAnonymousTree(mRootNode, aChild) &&
      MatchSelf(aChild)) {
    SetDirty();
  }

  ASSERT_IN_SYNC;
}

bool
nsContentList::Match(Element *aElement)
{
  if (mFunc) {
    return (*mFunc)(aElement, mMatchNameSpaceId, mXMLMatchAtom, mData);
  }

  if (!mXMLMatchAtom)
    return false;

  NodeInfo *ni = aElement->NodeInfo();

  bool wildcard = mMatchNameSpaceId == kNameSpaceID_Wildcard ||
                  mMatchNameSpaceId == kNameSpaceID_Unknown;
  bool toReturn = mMatchAll;
  if (!wildcard)
    toReturn &= ni->NamespaceEquals(mMatchNameSpaceId);

  if (toReturn)
    return toReturn;

  bool matchHTML =
    mIsHTMLDocument && aElement->GetNameSpaceID() == kNameSpaceID_XHTML;

  if (wildcard) {
    return matchHTML ? ni->Equals(mHTMLMatchAtom) :
                       ni->Equals(mXMLMatchAtom);
  }
  
  return matchHTML ? ni->Equals(mHTMLMatchAtom, mMatchNameSpaceId) :
                     ni->Equals(mXMLMatchAtom, mMatchNameSpaceId);
}

bool 
nsContentList::MatchSelf(nsIContent *aContent)
{
  NS_PRECONDITION(aContent, "Can't match null stuff, you know");
  NS_PRECONDITION(mDeep || aContent->GetParentNode() == mRootNode,
                  "MatchSelf called on a node that we can't possibly match");

  if (!aContent->IsElement()) {
    return false;
  }
  
  if (Match(aContent->AsElement()))
    return true;

  if (!mDeep)
    return false;

  for (nsIContent* cur = aContent->GetFirstChild();
       cur;
       cur = cur->GetNextNode(aContent)) {
    if (cur->IsElement() && Match(cur->AsElement())) {
      return true;
    }
  }
  
  return false;
}

void 
nsContentList::PopulateSelf(uint32_t aNeededLength)
{
  if (!mRootNode) {
    return;
  }

  ASSERT_IN_SYNC;

  uint32_t count = mElements.Length();
  NS_ASSERTION(mState != LIST_DIRTY || count == 0,
               "Reset() not called when setting state to LIST_DIRTY?");

  if (count >= aNeededLength) // We're all set
    return;

  uint32_t elementsToAppend = aNeededLength - count;
#ifdef DEBUG
  uint32_t invariant = elementsToAppend + mElements.Length();
#endif

  if (mDeep) {
    // If we already have nodes start searching at the last one, otherwise
    // start searching at the root.
    nsINode* cur = count ? mElements[count - 1] : mRootNode;
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
    nsIContent* cur =
      count ? mElements[count-1]->GetNextSibling() : mRootNode->GetFirstChild();
    for ( ; cur && elementsToAppend; cur = cur->GetNextSibling()) {
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

void
nsContentList::RemoveFromHashtable()
{
  if (mFunc) {
    // This can't be in the table anyway
    return;
  }
  
  nsDependentAtomString str(mXMLMatchAtom);
  nsContentListKey key(mRootNode, mMatchNameSpaceId, str, mIsHTMLDocument);
  uint32_t recentlyUsedCacheIndex = RecentlyUsedCacheIndex(key);
  if (sRecentlyUsedContentLists[recentlyUsedCacheIndex] == this) {
    sRecentlyUsedContentLists[recentlyUsedCacheIndex] = nullptr;
  }

  if (!gContentListHashTable)
    return;

  gContentListHashTable->Remove(&key);

  if (gContentListHashTable->EntryCount() == 0) {
    delete gContentListHashTable;
    gContentListHashTable = nullptr;
  }
}

void
nsContentList::BringSelfUpToDate(bool aDoFlush)
{
  if (mRootNode && aDoFlush && mFlushesNeeded) {
    // XXX sXBL/XBL2 issue
    nsIDocument* doc = mRootNode->GetUncomposedDoc();
    if (doc) {
      // Flush pending content changes Bug 4891.
      doc->FlushPendingNotifications(Flush_ContentAndNotify);
    }
  }

  if (mState != LIST_UP_TO_DATE)
    PopulateSelf(uint32_t(-1));
    
  ASSERT_IN_SYNC;
  NS_ASSERTION(!mRootNode || mState == LIST_UP_TO_DATE,
               "PopulateSelf dod not bring content list up to date!");
}

nsCacheableFuncStringContentList::~nsCacheableFuncStringContentList()
{
  RemoveFromFuncStringHashtable();
}

void
nsCacheableFuncStringContentList::RemoveFromFuncStringHashtable()
{
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
void
nsContentList::AssertInSync()
{
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
  nsIContent *root;
  if (mRootNode->IsNodeOfType(nsINode::eDOCUMENT)) {
    root = static_cast<nsIDocument*>(mRootNode)->GetRootElement();
  }
  else {
    root = static_cast<nsIContent*>(mRootNode);
  }

  nsCOMPtr<nsIContentIterator> iter;
  if (mDeep) {
    iter = NS_NewPreContentIterator();
    iter->Init(root);
    iter->First();
  }

  uint32_t cnt = 0, index = 0;
  while (true) {
    if (cnt == mElements.Length() && mState == LIST_LAZY) {
      break;
    }

    nsIContent *cur = mDeep ? iter->GetCurrentNode() :
                              mRootNode->GetChildAt(index++);
    if (!cur) {
      break;
    }

    if (cur->IsElement() && Match(cur->AsElement())) {
      NS_ASSERTION(cnt < mElements.Length() && mElements[cnt] == cur,
                   "Elements is out of sync");
      ++cnt;
    }

    if (mDeep) {
      iter->Next();
    }
  }

  NS_ASSERTION(cnt == mElements.Length(), "Too few elements");
}
#endif
