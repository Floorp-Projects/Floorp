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

#include "nsContentList.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIDOM3Node.h"
#include "nsIDocument.h"
#include "nsGenericElement.h"

#include "nsContentUtils.h"

#include "nsLayoutAtoms.h"
#include "nsHTMLAtoms.h" // XXX until atoms get factored into nsLayoutAtoms

// Form related includes
#include "nsIDOMHTMLFormElement.h"
#include "nsIContentList.h"

#include "pldhash.h"


static nsIContentList *gCachedContentList;

nsBaseContentList::nsBaseContentList()
{
}

nsBaseContentList::~nsBaseContentList()
{
  // mElements only has weak references to the content objects so we
  // don't need to do any cleanup here.
}


// QueryInterface implementation for nsBaseContentList
NS_INTERFACE_MAP_BEGIN(nsBaseContentList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNodeList)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMNodeList)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(NodeList)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsBaseContentList)
NS_IMPL_RELEASE(nsBaseContentList)


NS_IMETHODIMP
nsBaseContentList::GetLength(PRUint32* aLength)
{
  *aLength = mElements.Count();

  return NS_OK;
}

NS_IMETHODIMP
nsBaseContentList::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  nsISupports *tmp = NS_REINTERPRET_CAST(nsISupports *,
                                         mElements.SafeElementAt(aIndex));
  if (!tmp) {
    *aReturn = nsnull;

    return NS_OK;
  }

  return CallQueryInterface(tmp, aReturn);
}

void
nsBaseContentList::AppendElement(nsIContent *aContent)
{
  // Shouldn't hold a reference since we'll be told when the content
  // leaves the document or the document will be destroyed.
  mElements.AppendElement(aContent);
}

void
nsBaseContentList::RemoveElement(nsIContent *aContent)
{
  mElements.RemoveElement(aContent);
}

PRInt32
nsBaseContentList::IndexOf(nsIContent *aContent, PRBool aDoFlush)
{
  return mElements.IndexOf(aContent);
}

void
nsBaseContentList::Reset()
{
  mElements.Clear();
}

// static
void
nsBaseContentList::Shutdown()
{
  NS_IF_RELEASE(gCachedContentList);
}


// nsFormContentList

nsFormContentList::nsFormContentList(nsIDOMHTMLFormElement *aForm,
                                     nsBaseContentList& aContentList)
  : nsBaseContentList()
{

  // move elements that belong to mForm into this content list

  PRUint32 i, length = 0;
  nsCOMPtr<nsIDOMNode> item;

  aContentList.GetLength(&length);

  for (i = 0; i < length; i++) {
    aContentList.Item(i, getter_AddRefs(item));

    nsCOMPtr<nsIContent> c(do_QueryInterface(item));

    if (c && nsContentUtils::BelongsInForm(aForm, c)) {
      AppendElement(c);
    }
  }
}

nsFormContentList::~nsFormContentList()
{
  Reset();
}

void
nsFormContentList::AppendElement(nsIContent *aContent)
{
  NS_ADDREF(aContent);

  nsBaseContentList::AppendElement(aContent);
}

void
nsFormContentList::RemoveElement(nsIContent *aContent)
{
  PRInt32 i = mElements.IndexOf(aContent);

  if (i >= 0) {
    nsIContent *content = NS_STATIC_CAST(nsIContent *, mElements.ElementAt(i));

    NS_RELEASE(content);

    mElements.RemoveElementAt(i);
  }
}

void
nsFormContentList::Reset()
{
  PRInt32 i, length = mElements.Count();

  for (i = 0; i < length; i++) {
    nsIContent *content = NS_STATIC_CAST(nsIContent *, mElements.ElementAt(i));

    NS_RELEASE(content);
  }

  nsBaseContentList::Reset();
}

// Hashtable for storing nsContentLists
static PLDHashTable gContentListHashTable;

struct ContentListHashEntry : public PLDHashEntryHdr
{
  nsContentList* mContentList;
};

PR_STATIC_CALLBACK(const void *)
ContentListHashtableGetKey(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  ContentListHashEntry *e = NS_STATIC_CAST(ContentListHashEntry *, entry);
  return e->mContentList->GetKey();
}

PR_STATIC_CALLBACK(PLDHashNumber)
ContentListHashtableHashKey(PLDHashTable *table, const void *key)
{
  const nsContentListKey* list = NS_STATIC_CAST(const nsContentListKey *, key);
  return list->GetHash();
}

PR_STATIC_CALLBACK(PRBool)
ContentListHashtableMatchEntry(PLDHashTable *table,
                               const PLDHashEntryHdr *entry,
                               const void *key)
{
  const ContentListHashEntry *e =
    NS_STATIC_CAST(const ContentListHashEntry *, entry);
  const nsContentListKey* list1 = e->mContentList->GetKey();
  const nsContentListKey* list2 = NS_STATIC_CAST(const nsContentListKey *, key);

  return list1->Equals(*list2);
}

nsresult
NS_GetContentList(nsIDocument* aDocument, nsIAtom* aMatchAtom,
                  PRInt32 aMatchNameSpaceId, nsIContent* aRootContent,
                  nsIContentList** aInstancePtrResult)
{
  *aInstancePtrResult = nsnull;
  nsContentList* list = nsnull;

  static PLDHashTableOps hash_table_ops =
  {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    ContentListHashtableGetKey,
    ContentListHashtableHashKey,
    ContentListHashtableMatchEntry,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub
  };

  // Initialize the hashtable if needed.
  if (!gContentListHashTable.ops) {
    PRBool success = PL_DHashTableInit(&gContentListHashTable,
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
    nsContentListKey hashKey(aDocument, aMatchAtom,
                             aMatchNameSpaceId, aRootContent);
    
    // A PL_DHASH_ADD is equivalent to a PL_DHASH_LOOKUP for cases
    // when the entry is already in the hashtable.
    entry = NS_STATIC_CAST(ContentListHashEntry *,
                           PL_DHashTableOperate(&gContentListHashTable,
                                                &hashKey,
                                                PL_DHASH_ADD));
    if (entry)
      list = entry->mContentList;
  }

  if (!list) {
    // We need to create a ContentList and add it to our new entry, if
    // we have an entry
    list = new nsContentList(aDocument, aMatchAtom,
                             aMatchNameSpaceId, aRootContent);
    if (entry) {
      if (list)
        entry->mContentList = list;
      else
        PL_DHashTableRawRemove(&gContentListHashTable, entry);
    }

    NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);
  }

  *aInstancePtrResult = list;
  NS_ADDREF(*aInstancePtrResult);

  // Hold on to the last requested content list to avoid having it be
  // removed from the cache immediately when it's released. Avoid
  // bumping the refcount on the list if the requested list is the one
  // that's already cached.

  if (gCachedContentList != list) {
    NS_IF_RELEASE(gCachedContentList);

    gCachedContentList = list;
    NS_ADDREF(gCachedContentList);
  }

  return NS_OK;
}


// nsContentList implementation

nsContentList::nsContentList(nsIDocument *aDocument,
                             nsIAtom* aMatchAtom,
                             PRInt32 aMatchNameSpaceId,
                             nsIContent* aRootContent,
                             PRBool aDeep)
  : nsBaseContentList(),
    nsContentListKey(aDocument, aMatchAtom, aMatchNameSpaceId, aRootContent),
    mFunc(nsnull),
    mData(nsnull),
    mState(LIST_DIRTY),
    mDeep(aDeep)
{
  NS_ASSERTION(mDeep || mRootContent, "Must have root content for non-deep list!");
  if (nsLayoutAtoms::wildcard == mMatchAtom) {
    mMatchAll = PR_TRUE;
  }
  else {
    mMatchAll = PR_FALSE;
  }
  Init(aDocument);
}

nsContentList::nsContentList(nsIDocument *aDocument,
                             nsContentListMatchFunc aFunc,
                             const nsAString& aData,
                             nsIContent* aRootContent,
                             PRBool aDeep,
                             nsIAtom* aMatchAtom,
                             PRInt32 aMatchNameSpaceId)
  : nsBaseContentList(),
    nsContentListKey(aDocument, aMatchAtom, aMatchNameSpaceId, aRootContent),
    mFunc(aFunc),
    mData(&EmptyString()),
    mMatchAll(PR_FALSE),
    mState(LIST_DIRTY),
    mDeep(aDeep)
{
  NS_ASSERTION(mDeep || mRootContent, "Must have root content for non-deep list!");
  if (!aData.IsEmpty()) {
    mData = new nsString(aData);
    // If this fails, fail silently
  }
  Init(aDocument);
}

void nsContentList::Init(nsIDocument *aDocument)
{
  // We don't reference count the reference to the document
  // If the document goes away first, we'll be informed and we
  // can drop our reference.
  // If we go away first, we'll get rid of ourselves from the
  // document's observer list.
  mDocument = aDocument;
  if (mDocument) {
    mDocument->AddObserver(this);
  }
}

nsContentList::~nsContentList()
{
  RemoveFromHashtable();
  if (mDocument) {
    mDocument->RemoveObserver(this);
  }
  
  if (mData && mData != &EmptyString()) {
    // We actually allocated mData ourselves
    delete mData;
  }
}


// QueryInterface implementation for nsContentList
NS_INTERFACE_MAP_BEGIN(nsContentList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLCollection)
  NS_INTERFACE_MAP_ENTRY(nsIContentList)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(ContentList)
NS_INTERFACE_MAP_END_INHERITING(nsBaseContentList)


NS_IMPL_ADDREF_INHERITED(nsContentList, nsBaseContentList)
NS_IMPL_RELEASE_INHERITED(nsContentList, nsBaseContentList)


nsISupports *
nsContentList::GetParentObject()
{
  if (mRootContent) {
    return mRootContent;
  }

  return mDocument;
}
  
PRUint32
nsContentList::Length(PRBool aDoFlush)
{
  CheckDocumentExistence();
  BringSelfUpToDate(aDoFlush);
    
  return mElements.Count();
}

nsIContent *
nsContentList::Item(PRUint32 aIndex, PRBool aDoFlush)
{
  CheckDocumentExistence();

  if (mDocument && aDoFlush) {
    // Flush pending content changes Bug 4891.
    mDocument->FlushPendingNotifications(Flush_ContentAndNotify);
  }

  if (mState != LIST_UP_TO_DATE)
    PopulateSelf(aIndex+1);

  NS_ASSERTION(!mDocument || mState != LIST_DIRTY,
               "PopulateSelf left the list in a dirty (useless) state!");

  return NS_STATIC_CAST(nsIContent *, mElements.SafeElementAt(aIndex));
}

nsIContent *
nsContentList::NamedItem(const nsAString& aName, PRBool aDoFlush)
{
  CheckDocumentExistence();

  BringSelfUpToDate(aDoFlush);
    
  PRInt32 i, count = mElements.Count();

  for (i = 0; i < count; i++) {
    nsIContent *content = NS_STATIC_CAST(nsIContent *,
                                         mElements.ElementAt(i));
    if (content) {
      nsAutoString name;
      // XXX Should it be an EqualsIgnoreCase?
      if (((content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::name,
                             name) == NS_CONTENT_ATTR_HAS_VALUE) &&
           aName.Equals(name)) ||
          ((content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::id,
                             name) == NS_CONTENT_ATTR_HAS_VALUE) &&
           aName.Equals(name))) {
        return content;
      }
    }
  }

  return nsnull;
}

PRInt32
nsContentList::IndexOf(nsIContent *aContent, PRBool aDoFlush)
{
  CheckDocumentExistence();
  BringSelfUpToDate(aDoFlush);
    
  return mElements.IndexOf(aContent);
}

void
nsContentList::RootDestroyed()
{
  // We shouldn't do anything useful from now on
  DisconnectFromDocument();  // This dirties us so we lose all state
  mRootContent = nsnull;
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
  nsIContent *content = Item(aIndex, PR_TRUE);

  if (content) {
    return CallQueryInterface(content, aReturn);
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

void
nsContentList::AttributeChanged(nsIDocument *aDocument, nsIContent* aContent,
                                PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                                PRInt32 aModType)
{
  NS_PRECONDITION(aContent, "Must have a content node to work with");
  
  if (!mFunc || mState == LIST_DIRTY || IsContentAnonymous(aContent)) {
    // Either we're already dirty or this notification doesn't affect
    // whether we might match aContent.
    return;
  }
  
  if (MayContainRelevantNodes(aContent->GetParent())) {
    if (Match(aContent)) {
      // We may now match a new node.  Just dirty ourselves.
      mState = LIST_DIRTY;
    } else {
      // We no longer match aContent.  Remove it from our list.  If
      // it's already not there, this is a no-op, which is fine.
      // Either way, no change of mState is required here.
      mElements.RemoveElement(aContent);
    }
  }
}

void
nsContentList::ContentAppended(nsIDocument *aDocument, nsIContent* aContainer,
                               PRInt32 aNewIndexInContainer)
{
  NS_PRECONDITION(aContainer, "Can't get at the new content if no container!");
  
  /*
   * If the state is LIST_DIRTY then we have no useful information in our list
   * and we want to put off doing work as much as possible.  Also, if
   * aContainer is anonymous from our point of view, we know that we can't
   * possibly be matching any of the kids.
   */
  if (mState == LIST_DIRTY || IsContentAnonymous(aContainer)) 
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

  if (count > 0 && MayContainRelevantNodes(aContainer)) {
    PRInt32 ourCount = mElements.Count();
    PRBool appendToList = PR_FALSE;
    if (ourCount == 0) {
      appendToList = PR_TRUE;
    } else {
      nsIContent* ourLastContent =
        NS_STATIC_CAST(nsIContent*, mElements.ElementAt(ourCount - 1));
      /*
       * We want to append instead of invalidating if the first thing
       * that got appended comes after ourLastContent.
       */
      nsCOMPtr<nsIDOM3Node> ourLastDOM3Node(do_QueryInterface(ourLastContent));
      if (ourLastDOM3Node) {
        nsCOMPtr<nsIDOMNode> newNode =
          do_QueryInterface(aContainer->GetChildAt(aNewIndexInContainer));
        NS_ASSERTION(newNode, "Content being inserted is not a node.... why?");

        PRUint16 comparisonFlags;
        nsresult rv =
          ourLastDOM3Node->CompareDocumentPosition(newNode, &comparisonFlags);
        if (NS_SUCCEEDED(rv) && 
            (comparisonFlags & nsIDOM3Node::DOCUMENT_POSITION_FOLLOWING)) {
          appendToList = PR_TRUE;
        }
      }
    }
    
    PRInt32 i;
    
    if (!appendToList) {
      // The new stuff is somewhere in the middle of our list; check
      // whether we need to invalidate
      for (i = aNewIndexInContainer; i <= count-1; ++i) {
        if (MatchSelf(aContainer->GetChildAt(i))) {
          // Uh-oh.  We're gonna have to add elements into the middle
          // of our list. That's not worth the effort.
          mState = LIST_DIRTY;
          break;
        }
      }
 
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
    for (i = aNewIndexInContainer; i <= count-1; ++i) {
      PRUint32 limit = PRUint32(-1);
      PopulateWith(aContainer->GetChildAt(i), PR_TRUE, limit);
    }
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
  if (mState == LIST_DIRTY || IsContentAnonymous(aChild))
    return;

  if (MayContainRelevantNodes(aContainer) && MatchSelf(aChild))
    mState = LIST_DIRTY;
}
 
void
nsContentList::ContentRemoved(nsIDocument *aDocument,
                              nsIContent* aContainer,
                              nsIContent* aChild,
                              PRInt32 aIndexInContainer)
{
  // Note that aContainer can be null here if we are removing from
  // the document itself; any attempted optimizations to this method
  // should deal with that.
  if (mState != LIST_DIRTY) {
    if (MayContainRelevantNodes(aContainer)) {
      if (!IsContentAnonymous(aChild) && MatchSelf(aChild)) {
        mState = LIST_DIRTY;
      }
      return;
    }
  }

  // Even if aChild is anonymous from our point of view, it could
  // contain our root (eg say our root is an anonymous child of
  // aChild).
  if (ContainsRoot(aChild)) {
    DisconnectFromDocument();
  }
}

void
nsContentList::DocumentWillBeDestroyed(nsIDocument *aDocument)
{
  DisconnectFromDocument();
  Reset();
}

PRBool
nsContentList::Match(nsIContent *aContent)
{
  if (!aContent)
    return PR_FALSE;

  if (mFunc) {
    return (*mFunc)(aContent, mMatchNameSpaceId, mMatchAtom, *mData);
  }

  if (mMatchAtom) {
    if (!aContent->IsContentOfType(nsIContent::eELEMENT)) {
      return PR_FALSE;
    }

    nsINodeInfo *ni = aContent->GetNodeInfo();
    NS_ASSERTION(ni, "Element without nodeinfo!");

    if (mMatchNameSpaceId == kNameSpaceID_Unknown) {
      return (mMatchAll || ni->Equals(mMatchAtom));
    }

    return ((mMatchAll && ni->NamespaceEquals(mMatchNameSpaceId)) ||
            ni->Equals(mMatchAtom, mMatchNameSpaceId));
  }

  return PR_FALSE;
}

void
nsContentList::CheckDocumentExistence()
{
  if (!mDocument && mRootContent) {
    
    mDocument = mRootContent->GetDocument();
    if (mDocument) {
      mDocument->AddObserver(this);
      mState = LIST_DIRTY;
    }
  }
}

PRBool 
nsContentList::MatchSelf(nsIContent *aContent)
{
  NS_PRECONDITION(aContent, "Can't match null stuff, you know");
  NS_PRECONDITION(mDeep || aContent->GetParent() == mRootContent,
                  "MatchSelf called on a node that we can't possibly match");
  
  if (Match(aContent))
    return PR_TRUE;

  if (!mDeep)
    return PR_FALSE;

  PRUint32 i, count = aContent->GetChildCount();

  for (i = 0; i < count; i++) {
    if (MatchSelf(aContent->GetChildAt(i))) {
      return PR_TRUE;
    }
  }
  
  return PR_FALSE;
}

void 
nsContentList::PopulateWith(nsIContent *aContent, PRBool aIncludeRoot,
                            PRUint32 & aElementsToAppend)
{
  NS_PRECONDITION(mDeep || aContent == mRootContent ||
                  aContent->GetParent() == mRootContent,
                  "PopulateWith called on nodes we can't possibly match");
  NS_PRECONDITION(mDeep || aIncludeRoot || aContent == mRootContent,
                  "Bogus root passed to PopulateWith in non-deep list");
  NS_PRECONDITION(!aIncludeRoot || aContent != mRootContent,
                  "We should never be trying to match mRootContent");
  
  if (aIncludeRoot) {
    if (Match(aContent)) {
      mElements.AppendElement(aContent);
      --aElementsToAppend;
      if (aElementsToAppend == 0)
        return;
    }
  }

  // Don't recurse down if we're not doing a deep match and we're
  // already looking at kids of the root.
  if (!mDeep && aIncludeRoot)
    return;
  
  PRUint32 i, count = aContent->GetChildCount();

  for (i = 0; i < count; i++) {
    PopulateWith(aContent->GetChildAt(i), PR_TRUE, aElementsToAppend);
    if (aElementsToAppend == 0)
      return;
  }
}

void 
nsContentList::PopulateWithStartingAfter(nsIContent *aStartRoot,
                                         nsIContent *aStartChild,
                                         PRUint32 & aElementsToAppend)
{
  NS_PRECONDITION(mDeep || aStartRoot == mRootContent ||
                  (aStartRoot->GetParent() == mRootContent &&
                   aStartChild == nsnull),
                  "Bogus aStartRoot or aStartChild");

  if (mDeep || aStartRoot == mRootContent) {
#ifdef DEBUG
    PRUint32 invariant = aElementsToAppend + mElements.Count();
#endif
    PRInt32 i = 0;
    if (aStartChild) {
      i = aStartRoot->IndexOf(aStartChild);
      NS_ASSERTION(i >= 0, "The start child must be a child of the start root!");
      ++i;  // move to one past
    }

    PRUint32 childCount = aStartRoot->GetChildCount();
    for ( ; ((PRUint32)i) < childCount; ++i) {
      PopulateWith(aStartRoot->GetChildAt(i), PR_TRUE, aElementsToAppend);
    
      NS_ASSERTION(aElementsToAppend + mElements.Count() == invariant,
                   "Something is awry in PopulateWith!");
      if (aElementsToAppend == 0)
        return;
    }
  }

  // We want to make sure we don't move up past our root node. So if
  // we're there, don't move to the parent.
  if (aStartRoot == mRootContent)
    return;
  
  nsIContent* parent = aStartRoot->GetParent();
  
  if (parent)
    PopulateWithStartingAfter(parent, aStartRoot, aElementsToAppend);
}

void 
nsContentList::PopulateSelf(PRUint32 aNeededLength)
{
  if (mState == LIST_DIRTY) {
    Reset();
  }
  PRUint32 count = mElements.Count();

  if (count >= aNeededLength) // We're all set
    return;

  PRUint32 elementsToAppend = aNeededLength - count;
#ifdef DEBUG
  PRUint32 invariant = elementsToAppend + mElements.Count();
#endif
  if (count != 0) {
    PopulateWithStartingAfter(NS_STATIC_CAST(nsIContent*,
                                             mElements.ElementAt(count - 1)),
                              nsnull,
                              elementsToAppend);
    NS_ASSERTION(elementsToAppend + mElements.Count() == invariant,
                 "Something is awry in PopulateWithStartingAfter!");
  } else if (mRootContent) {
    PopulateWith(mRootContent, PR_FALSE, elementsToAppend);
    NS_ASSERTION(elementsToAppend + mElements.Count() == invariant,
                 "Something is awry in PopulateWith!");
  }
  else if (mDocument) {
    nsIContent *root = mDocument->GetRootContent();
    if (root) {
      PopulateWith(root, PR_TRUE, elementsToAppend);
      NS_ASSERTION(elementsToAppend + mElements.Count() == invariant,
                   "Something is awry in PopulateWith!");
    }
  }

  if (mDocument) {
    if (elementsToAppend != 0)
      mState = LIST_UP_TO_DATE;
    else
      mState = LIST_LAZY;
  } else {
    // No document means we have to stay on our toes since we don't
    // get content notifications.
    mState = LIST_DIRTY;
  }
}

PRBool
nsContentList::MayContainRelevantNodes(nsIContent* aContainer) 
{
  if (!mRootContent) {
#ifdef DEBUG
    // aContainer can be null when ContentInserted/ContentRemoved are
    // called, but we still want to return PR_TRUE in such cases if
    // mRootContent is null.  We could pass the document into this
    // method instead of trying to get it from aContainer, but that
    // seems a little pointless just to run this debug-only integrity
    // check.
    if (aContainer) { 
      NS_ASSERTION(aContainer->GetDocument() == mDocument,
                   "We should not get in here if aContainer is in some _other_ document!");
    }
#endif
    return PR_TRUE;
  }

  if (!aContainer) {
    return PR_FALSE;
  }

  if (!mDeep) {
    // We only care about cases when aContainer is our root content node.
    return aContainer == mRootContent;
  }
  
  return nsContentUtils::ContentIsDescendantOf(aContainer, mRootContent);
}

PRBool
nsContentList::ContainsRoot(nsIContent* aContent)
{
  if (!mRootContent || !aContent) {
    return PR_FALSE;
  }

  return nsContentUtils::ContentIsDescendantOf(mRootContent, aContent);
}

void 
nsContentList::DisconnectFromDocument()
{
  if (mDocument) {
    // Our key will change... Best remove ourselves before that happens.
    RemoveFromHashtable();
    mDocument->RemoveObserver(this);
    mDocument = nsnull;
  }

  // We will get no more updates, so we can never know we're up to
  // date
  mState = LIST_DIRTY;
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

  PL_DHashTableOperate(&gContentListHashTable,
                       GetKey(),
                       PL_DHASH_REMOVE);

  if (gContentListHashTable.entryCount == 0) {
    PL_DHashTableFinish(&gContentListHashTable);
    gContentListHashTable.ops = nsnull;
  }
}

void
nsContentList::BringSelfUpToDate(PRBool aDoFlush)
{
  if (mDocument && aDoFlush) {
    // Flush pending content changes Bug 4891.
    mDocument->FlushPendingNotifications(Flush_ContentAndNotify);
  }

  if (mState != LIST_UP_TO_DATE)
    PopulateSelf(PRUint32(-1));
    
  NS_ASSERTION(!mDocument || mState == LIST_UP_TO_DATE,
               "PopulateSelf dod not bring content list up to date!");
}

PRBool
nsContentList::IsContentAnonymous(nsIContent* aContent)
{
  NS_PRECONDITION(aContent, "Must have a content node to work with");
  
  /**
   * The goal of this function is to filter out content that is "anonymous"
   * from our point of view, so that we don't end up with it in the list.  Note
   * that in some cases content can be anonymous but NOT from our point of
   * view.  For example, say we're the result of calling getElementsByTagName
   * on an anonymous node in a XBL binding.  In that case, all descendants of
   * that node that it can see are not anonymous from our point of view.
   */

  if (!mRootContent) {
    /**
     * If we have no root, that means we correspond to a list gotten off the
     * "document" object, so will always contain only nodes that the document
     * can see, which means only nodes with a null bindingParent.
     */
    return aContent->GetBindingParent() != nsnull;
  }

  /**
   * If mRootContent and aContent have _different_ bindingParents, then there
   * are two possibilities:
   *
   * 1) aContent or one of its ancestors is an anonymous child of a descendant
   *    of mRootContent (or of mRootContent itself).
   * 2) mRootContent is not an ancestor of aContent.
   *
   * In either case, we don't want to be matching aContent or any of its
   * descendants.
   *
   * On the other hand, if mRootContent and aContent have the same
   * bindingParent then they are part of the same binding (or native anonymous
   * content chunk) and then aContent may be a descendant of mRootContent and
   * we may want to match it.
   *
   * So we return true if the binding parents don't match; if the binding
   * parents are the same, the checks we normally do to determine whether we
   * match a node should be done.
   */
  return mRootContent->GetBindingParent() != aContent->GetBindingParent();
}
