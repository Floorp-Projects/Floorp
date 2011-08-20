/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
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

#include "nsContentList.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIDocument.h"
#include "nsGenericElement.h"

#include "nsContentUtils.h"

#include "nsGkAtoms.h"

#include "dombindings.h"

// Form related includes
#include "nsIDOMHTMLFormElement.h"

#include "pldhash.h"

#ifdef DEBUG_CONTENT_LIST
#include "nsIContentIterator.h"
nsresult
NS_NewPreContentIterator(nsIContentIterator** aInstancePtrResult);
#define ASSERT_IN_SYNC AssertInSync()
#else
#define ASSERT_IN_SYNC PR_BEGIN_MACRO PR_END_MACRO
#endif


using namespace mozilla::dom;

nsBaseContentList::~nsBaseContentList()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsBaseContentList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsBaseContentList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mElements)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsBaseContentList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mElements)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsBaseContentList)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

#define NS_CONTENT_LIST_INTERFACES(_class)                                    \
    NS_INTERFACE_TABLE_ENTRY(_class, nsINodeList)                             \
    NS_INTERFACE_TABLE_ENTRY(_class, nsIDOMNodeList)

DOMCI_DATA(NodeList, nsBaseContentList)

// QueryInterface implementation for nsBaseContentList
NS_INTERFACE_TABLE_HEAD(nsBaseContentList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_NODELIST_OFFSET_AND_INTERFACE_TABLE_BEGIN(nsBaseContentList)
    NS_CONTENT_LIST_INTERFACES(nsBaseContentList)
  NS_OFFSET_AND_INTERFACE_TABLE_END
  NS_OFFSET_AND_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsBaseContentList)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(NodeList)
NS_INTERFACE_MAP_END


NS_IMPL_CYCLE_COLLECTING_ADDREF(nsBaseContentList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsBaseContentList)


NS_IMETHODIMP
nsBaseContentList::GetLength(PRUint32* aLength)
{
  *aLength = mElements.Count();

  return NS_OK;
}

NS_IMETHODIMP
nsBaseContentList::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  nsISupports *tmp = GetNodeAt(aIndex);

  if (!tmp) {
    *aReturn = nsnull;

    return NS_OK;
  }

  return CallQueryInterface(tmp, aReturn);
}

nsIContent*
nsBaseContentList::GetNodeAt(PRUint32 aIndex)
{
  return mElements.SafeObjectAt(aIndex);
}


PRInt32
nsBaseContentList::IndexOf(nsIContent *aContent, bool aDoFlush)
{
  return mElements.IndexOf(aContent);
}

PRInt32
nsBaseContentList::IndexOf(nsIContent* aContent)
{
  return IndexOf(aContent, PR_TRUE);
}

void nsBaseContentList::AppendElement(nsIContent *aContent) 
{
  mElements.AppendObject(aContent);
}

void nsBaseContentList::RemoveElement(nsIContent *aContent) 
{
  mElements.RemoveObject(aContent);
}

void nsBaseContentList::InsertElementAt(nsIContent* aContent, PRInt32 aIndex)
{
  NS_ASSERTION(aContent, "Element to insert must not be null");
  mElements.InsertObjectAt(aContent, aIndex);
}


NS_IMPL_CYCLE_COLLECTION_CLASS(nsSimpleContentList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsSimpleContentList,
                                                  nsBaseContentList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRoot)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsSimpleContentList,
                                                nsBaseContentList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mRoot)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsSimpleContentList)
NS_INTERFACE_MAP_END_INHERITING(nsBaseContentList)


NS_IMPL_ADDREF_INHERITED(nsSimpleContentList, nsBaseContentList)
NS_IMPL_RELEASE_INHERITED(nsSimpleContentList, nsBaseContentList)

JSObject*
nsSimpleContentList::WrapObject(JSContext *cx, XPCWrappedNativeScope *scope,
                                bool *triedToWrap)
{
  return mozilla::dom::binding::NodeList::create(cx, scope, this, triedToWrap);
}

// nsFormContentList

nsFormContentList::nsFormContentList(nsIContent *aForm,
                                     nsBaseContentList& aContentList)
  : nsSimpleContentList(aForm)
{

  // move elements that belong to mForm into this content list

  PRUint32 i, length = 0;
  aContentList.GetLength(&length);

  for (i = 0; i < length; i++) {
    nsIContent *c = aContentList.GetNodeAt(i);
    if (c && nsContentUtils::BelongsInForm(aForm, c)) {
      AppendElement(c);
    }
  }
}

// Hashtable for storing nsContentLists
static PLDHashTable gContentListHashTable;

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
                  PRInt32  aMatchNameSpaceId,
                  const nsAString& aTagname)
                  
{
  NS_ASSERTION(aRootNode, "content list has to have a root");

  nsContentList* list = nsnull;

  static PLDHashTableOps hash_table_ops =
  {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    ContentListHashtableHashKey,
    ContentListHashtableMatchEntry,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub
  };

  // Initialize the hashtable if needed.
  if (!gContentListHashTable.ops) {
    bool success = PL_DHashTableInit(&gContentListHashTable,
                                       &hash_table_ops, nsnull,
                                       sizeof(ContentListHashEntry),
                                       16);

    if (!success) {
      gContentListHashTable.ops = nsnull;
    }
  }
  
  ContentListHashEntry *entry = nsnull;
  // First we look in our hashtable.  Then we create a content list if needed
  if (gContentListHashTable.ops) {
    nsContentListKey hashKey(aRootNode, aMatchNameSpaceId, aTagname);
    
    // A PL_DHASH_ADD is equivalent to a PL_DHASH_LOOKUP for cases
    // when the entry is already in the hashtable.
    entry = static_cast<ContentListHashEntry *>
                       (PL_DHashTableOperate(&gContentListHashTable,
                                             &hashKey,
                                             PL_DHASH_ADD));
    if (entry)
      list = entry->mContentList;
  }

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
    list = new nsContentList(aRootNode, aMatchNameSpaceId,
                             htmlAtom, xmlAtom);
    if (entry) {
      entry->mContentList = list;
    }
  }

  NS_ADDREF(list);

  return list;
}

// Hashtable for storing nsCacheableFuncStringContentList
static PLDHashTable gFuncStringContentListHashTable;

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

already_AddRefed<nsContentList>
NS_GetFuncStringContentList(nsINode* aRootNode,
                            nsContentListMatchFunc aFunc,
                            nsContentListDestroyFunc aDestroyFunc,
                            nsFuncStringContentListDataAllocator aDataAllocator,
                            const nsAString& aString)
{
  NS_ASSERTION(aRootNode, "content list has to have a root");

  nsCacheableFuncStringContentList* list = nsnull;

  static PLDHashTableOps hash_table_ops =
  {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    FuncStringContentListHashtableHashKey,
    FuncStringContentListHashtableMatchEntry,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub
  };

  // Initialize the hashtable if needed.
  if (!gFuncStringContentListHashTable.ops) {
    bool success = PL_DHashTableInit(&gFuncStringContentListHashTable,
                                       &hash_table_ops, nsnull,
                                       sizeof(FuncStringContentListHashEntry),
                                       16);

    if (!success) {
      gFuncStringContentListHashTable.ops = nsnull;
    }
  }

  FuncStringContentListHashEntry *entry = nsnull;
  // First we look in our hashtable.  Then we create a content list if needed
  if (gFuncStringContentListHashTable.ops) {
    nsFuncStringCacheKey hashKey(aRootNode, aFunc, aString);

    // A PL_DHASH_ADD is equivalent to a PL_DHASH_LOOKUP for cases
    // when the entry is already in the hashtable.
    entry = static_cast<FuncStringContentListHashEntry *>
                       (PL_DHashTableOperate(&gFuncStringContentListHashTable,
                                             &hashKey,
                                             PL_DHASH_ADD));
    if (entry)
      list = entry->mContentList;
  }

  if (!list) {
    // We need to create a ContentList and add it to our new entry, if
    // we have an entry
    list = new nsCacheableFuncStringContentList(aRootNode, aFunc, aDestroyFunc,
                                                aDataAllocator, aString);
    if (list && !list->AllocatedData()) {
      // Failed to allocate the data
      delete list;
      list = nsnull;
    }

    if (entry) {
      if (list)
        entry->mContentList = list;
      else
        PL_DHashTableRawRemove(&gContentListHashTable, entry);
    }

    NS_ENSURE_TRUE(list, nsnull);
  }

  NS_ADDREF(list);

  // Don't cache these lists globally

  return list;
}

// nsContentList implementation

nsContentList::nsContentList(nsINode* aRootNode,
                             PRInt32 aMatchNameSpaceId,
                             nsIAtom* aHTMLMatchAtom,
                             nsIAtom* aXMLMatchAtom,
                             bool aDeep)
  : nsBaseContentList(),
    mRootNode(aRootNode),
    mMatchNameSpaceId(aMatchNameSpaceId),
    mHTMLMatchAtom(aHTMLMatchAtom),
    mXMLMatchAtom(aXMLMatchAtom),
    mFunc(nsnull),
    mDestroyFunc(nsnull),
    mData(nsnull),
    mState(LIST_DIRTY),
    mDeep(aDeep),
    mFuncMayDependOnAttr(PR_FALSE)
{
  NS_ASSERTION(mRootNode, "Must have root");
  if (nsGkAtoms::_asterix == mHTMLMatchAtom) {
    NS_ASSERTION(mXMLMatchAtom == nsGkAtoms::_asterix, "HTML atom and XML atom are not both asterix?");
    mMatchAll = PR_TRUE;
  }
  else {
    mMatchAll = PR_FALSE;
  }
  mRootNode->AddMutationObserver(this);

  // We only need to flush if we're in an non-HTML document, since the
  // HTML5 parser doesn't need flushing.  Further, if we're not in a
  // document at all right now (in the GetCurrentDoc() sense), we're
  // not parser-created and don't need to be flushing stuff under us
  // to get our kids right.
  nsIDocument* doc = mRootNode->GetCurrentDoc();
  mFlushesNeeded = doc && !doc->IsHTML();
}

nsContentList::nsContentList(nsINode* aRootNode,
                             nsContentListMatchFunc aFunc,
                             nsContentListDestroyFunc aDestroyFunc,
                             void* aData,
                             bool aDeep,
                             nsIAtom* aMatchAtom,
                             PRInt32 aMatchNameSpaceId,
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
    mMatchAll(PR_FALSE),
    mDeep(aDeep),
    mFuncMayDependOnAttr(aFuncMayDependOnAttr)
{
  NS_ASSERTION(mRootNode, "Must have root");
  mRootNode->AddMutationObserver(this);

  // We only need to flush if we're in an non-HTML document, since the
  // HTML5 parser doesn't need flushing.  Further, if we're not in a
  // document at all right now (in the GetCurrentDoc() sense), we're
  // not parser-created and don't need to be flushing stuff under us
  // to get our kids right.
  nsIDocument* doc = mRootNode->GetCurrentDoc();
  mFlushesNeeded = doc && !doc->IsHTML();
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
nsContentList::WrapObject(JSContext *cx, XPCWrappedNativeScope *scope,
                          bool *triedToWrap)
{
  return mozilla::dom::binding::HTMLCollection::create(cx, scope, this, this,
                                                       triedToWrap);
}

DOMCI_DATA(ContentList, nsContentList)

// QueryInterface implementation for nsContentList
NS_INTERFACE_TABLE_HEAD(nsContentList)
  NS_NODELIST_OFFSET_AND_INTERFACE_TABLE_BEGIN(nsContentList)
    NS_CONTENT_LIST_INTERFACES(nsContentList)
    NS_INTERFACE_TABLE_ENTRY(nsContentList, nsIHTMLCollection)
    NS_INTERFACE_TABLE_ENTRY(nsContentList, nsIDOMHTMLCollection)
    NS_INTERFACE_TABLE_ENTRY(nsContentList, nsIMutationObserver)
  NS_OFFSET_AND_INTERFACE_TABLE_END
  NS_OFFSET_AND_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ContentList)
NS_INTERFACE_MAP_END_INHERITING(nsBaseContentList)


NS_IMPL_ADDREF_INHERITED(nsContentList, nsBaseContentList)
NS_IMPL_RELEASE_INHERITED(nsContentList, nsBaseContentList)

PRUint32
nsContentList::Length(bool aDoFlush)
{
  BringSelfUpToDate(aDoFlush);
    
  return mElements.Count();
}

nsIContent *
nsContentList::Item(PRUint32 aIndex, bool aDoFlush)
{
  if (mRootNode && aDoFlush && mFlushesNeeded) {
    // XXX sXBL/XBL2 issue
    nsIDocument* doc = mRootNode->GetCurrentDoc();
    if (doc) {
      // Flush pending content changes Bug 4891.
      doc->FlushPendingNotifications(Flush_ContentAndNotify);
    }
  }

  if (mState != LIST_UP_TO_DATE)
    PopulateSelf(NS_MIN(aIndex, PR_UINT32_MAX - 1) + 1);

  ASSERT_IN_SYNC;
  NS_ASSERTION(!mRootNode || mState != LIST_DIRTY,
               "PopulateSelf left the list in a dirty (useless) state!");

  return mElements.SafeObjectAt(aIndex);
}

nsIContent *
nsContentList::NamedItem(const nsAString& aName, bool aDoFlush)
{
  BringSelfUpToDate(aDoFlush);
    
  PRInt32 i, count = mElements.Count();

  // Typically IDs and names are atomized
  nsCOMPtr<nsIAtom> name = do_GetAtom(aName);
  NS_ENSURE_TRUE(name, nsnull);

  for (i = 0; i < count; i++) {
    nsIContent *content = mElements[i];
    // XXX Should this pass eIgnoreCase?
    if (content &&
        (content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                              name, eCaseMatters) ||
         content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::id,
                              name, eCaseMatters))) {
      return content;
    }
  }

  return nsnull;
}

PRInt32
nsContentList::IndexOf(nsIContent *aContent, bool aDoFlush)
{
  BringSelfUpToDate(aDoFlush);
    
  return mElements.IndexOf(aContent);
}

PRInt32
nsContentList::IndexOf(nsIContent* aContent)
{
  return IndexOf(aContent, PR_TRUE);
}

void
nsContentList::NodeWillBeDestroyed(const nsINode* aNode)
{
  // We shouldn't do anything useful from now on

  RemoveFromCaches();
  mRootNode = nsnull;

  // We will get no more updates, so we can never know we're up to
  // date
  SetDirty();
}

NS_IMETHODIMP
nsContentList::GetLength(PRUint32* aLength)
{
  *aLength = Length(PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsContentList::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  nsINode* node = GetNodeAt(aIndex);

  if (node) {
    return CallQueryInterface(node, aReturn);
  }

  *aReturn = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsContentList::NamedItem(const nsAString& aName, nsIDOMNode** aReturn)
{
  nsIContent *content = NamedItem(aName, PR_TRUE);

  if (content) {
    return CallQueryInterface(content, aReturn);
  }

  *aReturn = nsnull;

  return NS_OK;
}

nsIContent*
nsContentList::GetNodeAt(PRUint32 aIndex)
{
  return Item(aIndex, PR_TRUE);
}

nsISupports*
nsContentList::GetNamedItem(const nsAString& aName, nsWrapperCache **aCache)
{
  nsIContent *item;
  *aCache = item = NamedItem(aName, PR_TRUE);
  return item;
}

void
nsContentList::AttributeChanged(nsIDocument *aDocument, Element* aElement,
                                PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                                PRInt32 aModType)
{
  NS_PRECONDITION(aElement, "Must have a content node to work with");
  
  if (!mFunc || !mFuncMayDependOnAttr || mState == LIST_DIRTY ||
      !MayContainRelevantNodes(aElement->GetNodeParent()) ||
      !nsContentUtils::IsInSameAnonymousTree(mRootNode, aElement)) {
    // Either we're already dirty or this notification doesn't affect
    // whether we might match aElement.
    return;
  }
  
  if (Match(aElement)) {
    if (mElements.IndexOf(aElement) == -1) {
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
    mElements.RemoveObject(aElement);
  }
}

void
nsContentList::ContentAppended(nsIDocument* aDocument, nsIContent* aContainer,
                               nsIContent* aFirstNewContent,
                               PRInt32 aNewIndexInContainer)
{
  NS_PRECONDITION(aContainer, "Can't get at the new content if no container!");
  
  /*
   * If the state is LIST_DIRTY then we have no useful information in our list
   * and we want to put off doing work as much as possible.  Also, if
   * aContainer is anonymous from our point of view, we know that we can't
   * possibly be matching any of the kids.
   */
  if (mState == LIST_DIRTY ||
      !nsContentUtils::IsInSameAnonymousTree(mRootNode, aContainer) ||
      !MayContainRelevantNodes(aContainer))
    return;

  /*
   * We want to handle the case of ContentAppended by sometimes
   * appending the content to our list, not just setting state to
   * LIST_DIRTY, since most of our ContentAppended notifications
   * should come during pageload and be at the end of the document.
   * Do a bit of work to see whether we could just append to what we
   * already have.
   */
  
  PRInt32 count = aContainer->GetChildCount();

  if (count > 0) {
    PRInt32 ourCount = mElements.Count();
    bool appendToList = false;
    if (ourCount == 0) {
      appendToList = PR_TRUE;
    } else {
      nsIContent* ourLastContent = mElements[ourCount - 1];
      /*
       * We want to append instead of invalidating if the first thing
       * that got appended comes after ourLastContent.
       */
      if (nsContentUtils::PositionIsBefore(ourLastContent, aFirstNewContent)) {
        appendToList = PR_TRUE;
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
          mElements.AppendObject(cur);
        }
      }
    } else {
      for (nsIContent* cur = aFirstNewContent; cur; cur = cur->GetNextSibling()) {
        if (cur->IsElement() && Match(cur->AsElement())) {
          mElements.AppendObject(cur);
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
                               PRInt32 aIndexInContainer)
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
                              PRInt32 aIndexInContainer,
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
    return PR_FALSE;

  nsINodeInfo *ni = aElement->NodeInfo();
 
  bool unknown = mMatchNameSpaceId == kNameSpaceID_Unknown;
  bool wildcard = mMatchNameSpaceId == kNameSpaceID_Wildcard;
  bool toReturn = mMatchAll;
  if (!unknown && !wildcard)
    toReturn &= ni->NamespaceEquals(mMatchNameSpaceId);

  if (toReturn)
    return toReturn;

  nsIDocument* doc = aElement->GetOwnerDoc();
  bool matchHTML = aElement->GetNameSpaceID() == kNameSpaceID_XHTML &&
    doc && doc->IsHTML();
 
  if (unknown) {
    return matchHTML ? ni->QualifiedNameEquals(mHTMLMatchAtom) :
                       ni->QualifiedNameEquals(mXMLMatchAtom);
  }
  
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
  NS_PRECONDITION(mDeep || aContent->GetNodeParent() == mRootNode,
                  "MatchSelf called on a node that we can't possibly match");

  if (!aContent->IsElement()) {
    return PR_FALSE;
  }
  
  if (Match(aContent->AsElement()))
    return PR_TRUE;

  if (!mDeep)
    return PR_FALSE;

  for (nsIContent* cur = aContent->GetFirstChild();
       cur;
       cur = cur->GetNextNode(aContent)) {
    if (cur->IsElement() && Match(cur->AsElement())) {
      return PR_TRUE;
    }
  }
  
  return PR_FALSE;
}

void 
nsContentList::PopulateSelf(PRUint32 aNeededLength)
{
  if (!mRootNode) {
    return;
  }

  ASSERT_IN_SYNC;

  PRUint32 count = mElements.Count();
  NS_ASSERTION(mState != LIST_DIRTY || count == 0,
               "Reset() not called when setting state to LIST_DIRTY?");

  if (count >= aNeededLength) // We're all set
    return;

  PRUint32 elementsToAppend = aNeededLength - count;
#ifdef DEBUG
  PRUint32 invariant = elementsToAppend + mElements.Count();
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
        mElements.AppendObject(cur->AsElement());
        --elementsToAppend;
      }
    } while (elementsToAppend);
  } else {
    nsIContent* cur =
      count ? mElements[count-1]->GetNextSibling() : mRootNode->GetFirstChild();
    for ( ; cur && elementsToAppend; cur = cur->GetNextSibling()) {
      if (cur->IsElement() && Match(cur->AsElement())) {
        mElements.AppendObject(cur);
        --elementsToAppend;
      }
    }
  }

  NS_ASSERTION(elementsToAppend + mElements.Count() == invariant,
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
  
  if (!gContentListHashTable.ops)
    return;

  nsDependentAtomString str(mXMLMatchAtom);
  nsContentListKey key(mRootNode, mMatchNameSpaceId, str);
  PL_DHashTableOperate(&gContentListHashTable,
                       &key,
                       PL_DHASH_REMOVE);

  if (gContentListHashTable.entryCount == 0) {
    PL_DHashTableFinish(&gContentListHashTable);
    gContentListHashTable.ops = nsnull;
  }
}

void
nsContentList::BringSelfUpToDate(bool aDoFlush)
{
  if (mRootNode && aDoFlush && mFlushesNeeded) {
    // XXX sXBL/XBL2 issue
    nsIDocument* doc = mRootNode->GetCurrentDoc();
    if (doc) {
      // Flush pending content changes Bug 4891.
      doc->FlushPendingNotifications(Flush_ContentAndNotify);
    }
  }

  if (mState != LIST_UP_TO_DATE)
    PopulateSelf(PRUint32(-1));
    
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
  if (!gFuncStringContentListHashTable.ops) {
    return;
  }

  nsFuncStringCacheKey key(mRootNode, mFunc, mString);
  PL_DHashTableOperate(&gFuncStringContentListHashTable,
                       &key,
                       PL_DHASH_REMOVE);

  if (gFuncStringContentListHashTable.entryCount == 0) {
    PL_DHashTableFinish(&gFuncStringContentListHashTable);
    gFuncStringContentListHashTable.ops = nsnull;
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
    NS_ASSERTION(mElements.Count() == 0 && mState == LIST_DIRTY,
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
    NS_NewPreContentIterator(getter_AddRefs(iter));
    iter->Init(root);
    iter->First();
  }

  PRInt32 cnt = 0, index = 0;
  while (PR_TRUE) {
    if (cnt == mElements.Count() && mState == LIST_LAZY) {
      break;
    }

    nsIContent *cur = mDeep ? iter->GetCurrentNode() :
                              mRootNode->GetChildAt(index++);
    if (!cur) {
      break;
    }

    if (cur->IsElement() && Match(cur->AsElement())) {
      NS_ASSERTION(cnt < mElements.Count() && mElements[cnt] == cur,
                   "Elements is out of sync");
      ++cnt;
    }

    if (mDeep) {
      iter->Next();
    }
  }

  NS_ASSERTION(cnt == mElements.Count(), "Too few elements");
}
#endif
