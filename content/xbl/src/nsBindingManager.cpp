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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
 *   Alec Flett <alecf@netscape.com>
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

#include "nsCOMPtr.h"
#include "nsIXBLService.h"
#include "nsIInputStream.h"
#include "nsDoubleHashtable.h"
#include "nsInterfaceHashtable.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsURIHashKey.h"
#include "nsIChannel.h"
#include "nsXPIDLString.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsNetUtil.h"
#include "plstr.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIXMLContentSink.h"
#include "nsContentCID.h"
#include "nsXMLDocument.h"
#include "nsHTMLAtoms.h"
#include "nsSupportsArray.h"
#include "nsITextContent.h"
#include "nsIStreamListener.h"
#include "nsIStyleRuleSupplier.h"
#include "nsStubDocumentObserver.h"

#include "nsIXBLBinding.h"
#include "nsIXBLDocumentInfo.h"
#include "nsXBLInsertionPoint.h"

#include "nsIStyleSheet.h"
#include "nsHTMLStyleSheet.h"
#include "nsIHTMLCSSStyleSheet.h"

#include "nsIStyleRuleProcessor.h"
#include "nsIWeakReference.h"

#include "jsapi.h"
#include "nsIXPConnect.h"
#include "nsDOMCID.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIPrincipal.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"

#include "nsIScriptContext.h"

// ==================================================================
// = nsAnonymousContentList 
// ==================================================================

class nsAnonymousContentList : public nsGenericDOMNodeList
{
public:
  nsAnonymousContentList(nsVoidArray* aElements);
  virtual ~nsAnonymousContentList();

  // nsIDOMNodeList interface
  NS_DECL_NSIDOMNODELIST

  PRInt32 GetInsertionPointCount() { return mElements->Count(); }

  nsXBLInsertionPoint* GetInsertionPointAt(PRInt32 i) { return NS_STATIC_CAST(nsXBLInsertionPoint*, mElements->ElementAt(i)); }

private:
  nsVoidArray* mElements;
};

MOZ_DECL_CTOR_COUNTER(nsAnonymousContentList)

nsAnonymousContentList::nsAnonymousContentList(nsVoidArray* aElements)
  : mElements(aElements)
{
  MOZ_COUNT_CTOR(nsAnonymousContentList);

  // We don't reference count our Anonymous reference (to avoid circular
  // references). We'll be told when the Anonymous goes away.
}

static PRBool PR_CALLBACK DeleteInsertionPoint(void* aElement, void* aData)
{
  delete NS_STATIC_CAST(nsXBLInsertionPoint*, aElement);
  return PR_TRUE;
}

nsAnonymousContentList::~nsAnonymousContentList()
{
  MOZ_COUNT_DTOR(nsAnonymousContentList);
  mElements->EnumerateForwards(DeleteInsertionPoint, nsnull);
  delete mElements;
}

NS_IMETHODIMP
nsAnonymousContentList::GetLength(PRUint32* aLength)
{
  NS_ASSERTION(aLength != nsnull, "null ptr");
  if (! aLength)
      return NS_ERROR_NULL_POINTER;

  PRInt32 cnt = mElements->Count();

  *aLength = 0;
  for (PRInt32 i = 0; i < cnt; i++)
    *aLength += NS_STATIC_CAST(nsXBLInsertionPoint*, mElements->ElementAt(i))->ChildCount();

  return NS_OK;
}

NS_IMETHODIMP    
nsAnonymousContentList::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  PRInt32 cnt = mElements->Count();
  PRUint32 pointCount = 0;

  for (PRInt32 i = 0; i < cnt; i++) {
    aIndex -= pointCount;
    
    nsXBLInsertionPoint* point = NS_STATIC_CAST(nsXBLInsertionPoint*, mElements->ElementAt(i));
    pointCount = point->ChildCount();

    if (aIndex < pointCount) {
      nsCOMPtr<nsIContent> result = point->ChildAt(aIndex);
      if (result)
        return CallQueryInterface(result, aReturn);
      return NS_ERROR_FAILURE;
    }
  }

  return NS_ERROR_FAILURE;
}

//
// Generic pldhash table stuff for mapping one nsISupports to another
//
// These values are never null - a null value implies that this
// whole key should be removed (See SetOrRemoveObject)
class ObjectEntry : public PLDHashEntryHdr
{
public:

  // note that these are allocated within the PLDHashTable, but we
  // want to keep track of them anyway
  ObjectEntry() { MOZ_COUNT_CTOR(ObjectEntry); }
  ~ObjectEntry() { MOZ_COUNT_DTOR(ObjectEntry); }
  
  nsISupports* GetValue() { return mValue; }
  nsISupports* GetKey() { return mKey; }
  void SetValue(nsISupports* aValue) { mValue = aValue; }
  void SetKey(nsISupports* aKey) { mKey = aKey; }
  
private:
  nsCOMPtr<nsISupports> mKey;
  nsCOMPtr<nsISupports> mValue;
};

PR_STATIC_CALLBACK(void)
ClearObjectEntry(PLDHashTable* table, PLDHashEntryHdr *entry)
{
  ObjectEntry* objEntry = NS_STATIC_CAST(ObjectEntry*, entry);
  objEntry->~ObjectEntry();
}

PR_STATIC_CALLBACK(PRBool)
InitObjectEntry(PLDHashTable* table, PLDHashEntryHdr* entry, const void* key)
{
  new (entry) ObjectEntry;
  return PR_TRUE;
}
  


static PLDHashTableOps ObjectTableOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  PL_DHashGetKeyStub,
  PL_DHashVoidPtrKeyStub,
  PL_DHashMatchEntryStub,
  PL_DHashMoveEntryStub,
  ClearObjectEntry,
  PL_DHashFinalizeStub,
  InitObjectEntry
};

// helper routine for adding a new entry
static nsresult
AddObjectEntry(PLDHashTable& table, nsISupports* aKey, nsISupports* aValue)
{
  NS_ASSERTION(aKey, "key must be non-null");
  if (!aKey) return NS_ERROR_INVALID_ARG;
  
  ObjectEntry *entry =
    NS_STATIC_CAST(ObjectEntry*,
                   PL_DHashTableOperate(&table, aKey, PL_DHASH_ADD));

  if (!entry)
    return NS_ERROR_OUT_OF_MEMORY;

  // only add the key if the entry is new
  if (!entry->GetKey())
    entry->SetKey(aKey);

  // now attach the new entry - note that entry->mValue could possibly
  // have a value already, this will release that.
  entry->SetValue(aValue);
  
  return NS_OK;
}

// helper routine for looking up an existing entry. Note that the
// return result is NOT addreffed
static nsISupports*
LookupObject(PLDHashTable& table, nsISupports* aKey)
{
  ObjectEntry *entry =
    NS_STATIC_CAST(ObjectEntry*,
                   PL_DHashTableOperate(&table, aKey, PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_BUSY(entry))
    return entry->GetValue();

  return nsnull;
}

inline void
RemoveObjectEntry(PLDHashTable& table, nsISupports* aKey)
{
  PL_DHashTableOperate(&table, aKey, PL_DHASH_REMOVE);
}

static nsresult
SetOrRemoveObject(PLDHashTable& table, nsISupports* aKey, nsISupports* aValue)
{
  if (aValue) {
    // lazily create the table, but only when adding elements
    if (!table.ops &&
        !PL_DHashTableInit(&table, &ObjectTableOps, nsnull,
                           sizeof(ObjectEntry), 16)) {
      table.ops = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    return AddObjectEntry(table, aKey, aValue);
  }

  // no value, so remove the key from the table
  if (table.ops)
    RemoveObjectEntry(table, aKey);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////

class nsBindingManager : public nsIBindingManager,
                         public nsIStyleRuleSupplier,
                         public nsStubDocumentObserver
{
  NS_DECL_ISUPPORTS

public:
  nsBindingManager();
  virtual ~nsBindingManager();

  NS_IMETHOD GetBinding(nsIContent* aContent, nsIXBLBinding** aResult);
  NS_IMETHOD SetBinding(nsIContent* aContent, nsIXBLBinding* aBinding);

  NS_IMETHOD GetInsertionParent(nsIContent* aContent, nsIContent** aResult);
  NS_IMETHOD SetInsertionParent(nsIContent* aContent, nsIContent* aResult);

  NS_IMETHOD GetWrappedJS(nsIContent* aContent, nsIXPConnectWrappedJS** aResult);
  NS_IMETHOD SetWrappedJS(nsIContent* aContent, nsIXPConnectWrappedJS* aResult);

  NS_IMETHOD ChangeDocumentFor(nsIContent* aContent, nsIDocument* aOldDocument,
                               nsIDocument* aNewDocument);

  NS_IMETHOD ResolveTag(nsIContent* aContent, PRInt32* aNameSpaceID, nsIAtom** aResult);

  NS_IMETHOD GetContentListFor(nsIContent* aContent, nsIDOMNodeList** aResult);
  NS_IMETHOD SetContentListFor(nsIContent* aContent, nsVoidArray* aList);
  NS_IMETHOD HasContentListFor(nsIContent* aContent, PRBool* aResult);

  NS_IMETHOD GetAnonymousNodesFor(nsIContent* aContent, nsIDOMNodeList** aResult);
  NS_IMETHOD SetAnonymousNodesFor(nsIContent* aContent, nsVoidArray* aList);

  NS_IMETHOD GetXBLChildNodesFor(nsIContent* aContent, nsIDOMNodeList** aResult);

  NS_IMETHOD GetInsertionPoint(nsIContent* aParent, nsIContent* aChild, nsIContent** aResult, PRUint32* aIndex);
  NS_IMETHOD GetSingleInsertionPoint(nsIContent* aParent, nsIContent** aResult, PRUint32* aIndex,  
                                     PRBool* aMultipleInsertionPoints);

  NS_IMETHOD AddLayeredBinding(nsIContent* aContent, nsIURI* aURL);
  NS_IMETHOD RemoveLayeredBinding(nsIContent* aContent, nsIURI* aURL);
  NS_IMETHOD LoadBindingDocument(nsIDocument* aBoundDoc, nsIURI* aURL,
                                 nsIDocument** aResult);

  NS_IMETHOD AddToAttachedQueue(nsIXBLBinding* aBinding);
  NS_IMETHOD ClearAttachedQueue();
  NS_IMETHOD ProcessAttachedQueue();

  NS_IMETHOD ExecuteDetachedHandlers();

  NS_IMETHOD PutXBLDocumentInfo(nsIXBLDocumentInfo* aDocumentInfo);
  NS_IMETHOD GetXBLDocumentInfo(nsIURI* aURI, nsIXBLDocumentInfo** aResult);
  NS_IMETHOD RemoveXBLDocumentInfo(nsIXBLDocumentInfo* aDocumentInfo);

  NS_IMETHOD PutLoadingDocListener(nsIURI* aURL, nsIStreamListener* aListener);
  NS_IMETHOD GetLoadingDocListener(nsIURI* aURL, nsIStreamListener** aResult);
  NS_IMETHOD RemoveLoadingDocListener(nsIURI* aURL);

  NS_IMETHOD InheritsStyle(nsIContent* aContent, PRBool* aResult);
  NS_IMETHOD FlushSkinBindings();

  NS_IMETHOD GetBindingImplementation(nsIContent* aContent, REFNSIID aIID, void** aResult);

  NS_IMETHOD ShouldBuildChildFrames(nsIContent* aContent, PRBool* aResult);

  // nsIStyleRuleSupplier
  NS_IMETHOD UseDocumentRules(nsIContent* aContent, PRBool* aResult);
  NS_IMETHOD WalkRules(nsStyleSet* aStyleSet, 
                       nsIStyleRuleProcessor::EnumFunc aFunc,
                       RuleProcessorData* aData);

  // nsIDocumentObserver
  virtual void ContentAppended(nsIDocument* aDocument,
                               nsIContent* aContainer,
                               PRInt32     aNewIndexInContainer);
  virtual void ContentInserted(nsIDocument* aDocument,
                               nsIContent* aContainer,
                               nsIContent* aChild,
                               PRInt32 aIndexInContainer);
  virtual void ContentRemoved(nsIDocument* aDocument,
                              nsIContent* aContainer,
                              nsIContent* aChild,
                              PRInt32 aIndexInContainer);

protected:
  nsresult GetXBLChildNodesInternal(nsIContent* aContent,
                                    nsIDOMNodeList** aResult,
                                    PRBool* aIsAnonymousContentList);
  nsresult GetAnonymousNodesInternal(nsIContent* aContent,
                                     nsIDOMNodeList** aResult,
                                     PRBool* aIsAnonymousContentList);

  nsIContent* GetEnclosingScope(nsIContent* aContent) {
    return aContent->GetBindingParent();
  }
  nsIContent* GetOutermostStyleScope(nsIContent* aContent);

  void WalkRules(nsIStyleRuleProcessor::EnumFunc aFunc,
                 RuleProcessorData* aData,
                 nsIContent* aParent, nsIContent* aCurrContent);

  nsresult GetNestedInsertionPoint(nsIContent* aParent, nsIContent* aChild, nsIContent** aResult);

// MEMBER VARIABLES
protected: 
  // A mapping from nsIContent* to the nsIXBLBinding* that is
  // installed on that element.
  PLDHashTable mBindingTable;

  // A mapping from nsIContent* to an nsIDOMNodeList*
  // (nsAnonymousContentList*).  This list contains an accurate
  // reflection of our *explicit* children (once intermingled with
  // insertion points) in the altered DOM.
  PLDHashTable mContentListTable;

  // A mapping from nsIContent* to an nsIDOMNodeList*
  // (nsAnonymousContentList*).  This list contains an accurate
  // reflection of our *anonymous* children (if and only if they are
  // intermingled with insertion points) in the altered DOM.  This
  // table is not used if no insertion points were defined directly
  // underneath a <content> tag in a binding.  The NodeList from the
  // <content> is used instead as a performance optimization.
  PLDHashTable mAnonymousNodesTable;

  // A mapping from nsIContent* to nsIContent*.  The insertion parent
  // is our one true parent in the transformed DOM.  This gives us a
  // more-or-less O(1) way of obtaining our transformed parent.
  PLDHashTable mInsertionParentTable;

  // A mapping from nsIContent* to nsIXPWrappedJS* (an XPConnect
  // wrapper for JS objects).  For XBL bindings that implement XPIDL
  // interfaces, and that get referred to from C++, this table caches
  // the XPConnect wrapper for the binding.  By caching it, I control
  // its lifetime, and I prevent a re-wrap of the same script object
  // (in the case where multiple bindings in an XBL inheritance chain
  // both implement an XPIDL interface).
  PLDHashTable mWrapperTable;

  // A mapping from a URL (a string) to nsIXBLDocumentInfo*.  This table
  // is the cache of all binding documents that have been loaded by a
  // given bound document.
  nsInterfaceHashtable<nsURIHashKey,nsIXBLDocumentInfo> mDocumentTable;

  // A mapping from a URL (a string) to a nsIStreamListener. This
  // table is the currently loading binding docs.  If they're in this
  // table, they have not yet finished loading.
  nsInterfaceHashtable<nsURIHashKey,nsIStreamListener> mLoadingDocTable;

  // A queue of binding attached event handlers that are awaiting
  // execution.
  nsCOMPtr<nsISupportsArray> mAttachedStack;
  PRBool mProcessingAttachedStack;
};

// Implementation /////////////////////////////////////////////////////////////////

// Static member variable initialization

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS3(nsBindingManager, nsIBindingManager, nsIStyleRuleSupplier, nsIDocumentObserver)

// Constructors/Destructors
nsBindingManager::nsBindingManager(void)
: mProcessingAttachedStack(PR_FALSE)
{
  mBindingTable.ops = nsnull;
  mContentListTable.ops = nsnull;
  mAnonymousNodesTable.ops = nsnull;
  mInsertionParentTable.ops = nsnull;
  mWrapperTable.ops = nsnull;
}

nsBindingManager::~nsBindingManager(void)
{
  if (mBindingTable.ops)
    PL_DHashTableFinish(&mBindingTable);
  if (mContentListTable.ops)
    PL_DHashTableFinish(&mContentListTable);
  if (mAnonymousNodesTable.ops)
    PL_DHashTableFinish(&mAnonymousNodesTable);
  if (mInsertionParentTable.ops)
    PL_DHashTableFinish(&mInsertionParentTable);
  if (mWrapperTable.ops)
    PL_DHashTableFinish(&mWrapperTable);
}

NS_IMETHODIMP
nsBindingManager::GetBinding(nsIContent* aContent, nsIXBLBinding** aResult) 
{
  if (mBindingTable.ops) {
    *aResult = NS_STATIC_CAST(nsIXBLBinding*,
                              LookupObject(mBindingTable, aContent));
    NS_IF_ADDREF(*aResult);
  }
  else {
    *aResult = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::SetBinding(nsIContent* aContent, nsIXBLBinding* aBinding)
{
  nsresult rv =
    SetOrRemoveObject(mBindingTable, aContent, aBinding);
  if (!aBinding) {
    // The death of the bindings means the death of the JS wrapper,
    // and the flushing of our explicit and anonymous insertion point
    // lists.
    SetWrappedJS(aContent, nsnull);
    SetContentListFor(aContent, nsnull);
    SetAnonymousNodesFor(aContent, nsnull);
  }

  return rv;
}

NS_IMETHODIMP
nsBindingManager::GetInsertionParent(nsIContent* aContent, nsIContent** aResult) 
{ 
  if (mInsertionParentTable.ops) {
    *aResult = NS_STATIC_CAST(nsIContent*,
                              LookupObject(mInsertionParentTable, aContent));
    NS_IF_ADDREF(*aResult);
  }
  else {
    *aResult = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::SetInsertionParent(nsIContent* aContent, nsIContent* aParent)
{
  return SetOrRemoveObject(mInsertionParentTable, aContent, aParent);
}

NS_IMETHODIMP
nsBindingManager::GetWrappedJS(nsIContent* aContent, nsIXPConnectWrappedJS** aResult) 
{ 
  if (mWrapperTable.ops) {
    *aResult = NS_STATIC_CAST(nsIXPConnectWrappedJS*, LookupObject(mWrapperTable, aContent));
    NS_IF_ADDREF(*aResult);
  }
  else {
    *aResult = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::SetWrappedJS(nsIContent* aContent, nsIXPConnectWrappedJS* aWrappedJS)
{
  return SetOrRemoveObject(mWrapperTable, aContent, aWrappedJS);
}

NS_IMETHODIMP
nsBindingManager::ChangeDocumentFor(nsIContent* aContent, nsIDocument* aOldDocument,
                                    nsIDocument* aNewDocument)
{
  NS_PRECONDITION(aOldDocument != nsnull, "no old document");
  if (! aOldDocument)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIXBLBinding> binding;
  GetBinding(aContent, getter_AddRefs(binding));
  if (binding) {
    binding->ChangeDocument(aOldDocument, aNewDocument);
    SetBinding(aContent, nsnull);
    if (aNewDocument)
      aNewDocument->GetBindingManager()->SetBinding(aContent, binding);
  }

  // Clear out insertion parents and content lists.
  SetInsertionParent(aContent, nsnull);
  SetContentListFor(aContent, nsnull);
  SetAnonymousNodesFor(aContent, nsnull);

  PRUint32 count = aOldDocument->GetNumberOfShells();

  for (PRUint32 i = 0; i < count; ++i) {
    nsIPresShell *shell = aOldDocument->GetShellAt(i);
    NS_ASSERTION(shell != nsnull, "Zoiks! nsIDocument::GetShellAt() broke");

    // See if the element has nsIAnonymousContentCreator-created
    // anonymous content...
    nsCOMPtr<nsISupportsArray> anonymousElements;
    shell->GetAnonymousContentFor(aContent, getter_AddRefs(anonymousElements));

    if (anonymousElements) {
      // ...yep, so be sure to update the doc pointer in those
      // elements, too.
      PRUint32 count;
      anonymousElements->Count(&count);

      while (PRInt32(--count) >= 0) {
        nsCOMPtr<nsIContent> content( do_QueryElementAt(anonymousElements, count));
        NS_ASSERTION(content != nsnull, "not an nsIContent");
        if (! content)
          continue;

        content->SetDocument(aNewDocument, PR_TRUE, PR_TRUE);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::ResolveTag(nsIContent* aContent, PRInt32* aNameSpaceID,
                             nsIAtom** aResult)
{
  nsCOMPtr<nsIXBLBinding> binding;
  GetBinding(aContent, getter_AddRefs(binding));
  
  if (binding) {
    binding->GetBaseTag(aNameSpaceID, aResult);

    if (*aResult) {
      return NS_OK;
    }
  }

  aContent->GetNameSpaceID(aNameSpaceID);
  NS_ADDREF(*aResult = aContent->Tag());

  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::GetContentListFor(nsIContent* aContent, nsIDOMNodeList** aResult)
{ 
  // Locate the primary binding and get its node list of anonymous children.
  *aResult = nsnull;
  
  if (mContentListTable.ops) {
    *aResult = NS_STATIC_CAST(nsIDOMNodeList*,
                              LookupObject(mContentListTable, aContent));
    NS_IF_ADDREF(*aResult);
  }
  
  if (!*aResult) {
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(aContent));
    return node->GetChildNodes(aResult);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::SetContentListFor(nsIContent* aContent, nsVoidArray* aList)
{
  nsIDOMNodeList* contentList = nsnull;
  if (aList) {
    contentList = new nsAnonymousContentList(aList);
    if (!contentList) return NS_ERROR_OUT_OF_MEMORY;
  }

  return SetOrRemoveObject(mContentListTable, aContent, contentList);
}

NS_IMETHODIMP
nsBindingManager::HasContentListFor(nsIContent* aContent, PRBool* aResult)
{
  *aResult = PR_FALSE;
  if (mContentListTable.ops) {
    nsISupports* list = LookupObject(mContentListTable, aContent);
    *aResult = (list != nsnull);
  }

  return NS_OK;
}

nsresult
nsBindingManager::GetAnonymousNodesInternal(nsIContent* aContent,
                                            nsIDOMNodeList** aResult,
                                            PRBool* aIsAnonymousContentList)
{ 
  // Locate the primary binding and get its node list of anonymous children.
  *aResult = nsnull;
  if (mAnonymousNodesTable.ops) {
    *aResult = NS_STATIC_CAST(nsIDOMNodeList*,
                              LookupObject(mAnonymousNodesTable, aContent));
    NS_IF_ADDREF(*aResult);
  }

  if (!*aResult) {
    *aIsAnonymousContentList = PR_FALSE;
    nsCOMPtr<nsIXBLBinding> binding;
    GetBinding(aContent, getter_AddRefs(binding));
    if (binding)
      return binding->GetAnonymousNodes(aResult);
  } else
    *aIsAnonymousContentList = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::GetAnonymousNodesFor(nsIContent* aContent,
                                       nsIDOMNodeList** aResult)
{
  PRBool dummy;
  return GetAnonymousNodesInternal(aContent, aResult, &dummy);
}

NS_IMETHODIMP
nsBindingManager::SetAnonymousNodesFor(nsIContent* aContent, nsVoidArray* aList)
{
  nsIDOMNodeList* contentList = nsnull;
  if (aList) {
    contentList = new nsAnonymousContentList(aList);
    if (!contentList) return NS_ERROR_OUT_OF_MEMORY;
  }
  
  return SetOrRemoveObject(mAnonymousNodesTable, aContent, contentList);
}

nsresult
nsBindingManager::GetXBLChildNodesInternal(nsIContent* aContent,
                                           nsIDOMNodeList** aResult,
                                           PRBool* aIsAnonymousContentList)
{
  *aResult = nsnull;

  PRUint32 length;

  // Retrieve the anonymous content that we should build.
  GetAnonymousNodesInternal(aContent, aResult, aIsAnonymousContentList);
  if (*aResult) {
    (*aResult)->GetLength(&length);
    if (length == 0)
      *aResult = nsnull;
  }
    
  // We may have an altered list of children from XBL insertion points.
  // If we don't have any anonymous kids, we next check to see if we have 
  // insertion points.
  if (! *aResult) {
    if (mContentListTable.ops) {
      *aResult = NS_STATIC_CAST(nsIDOMNodeList*,
                                LookupObject(mContentListTable, aContent));
      NS_IF_ADDREF(*aResult);
      *aIsAnonymousContentList = PR_TRUE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::GetXBLChildNodesFor(nsIContent* aContent, nsIDOMNodeList** aResult)
{
  PRBool dummy;
  return GetXBLChildNodesInternal(aContent, aResult, &dummy);
}

NS_IMETHODIMP
nsBindingManager::GetInsertionPoint(nsIContent* aParent, nsIContent* aChild, nsIContent** aResult, PRUint32* aIndex)
{
  nsCOMPtr<nsIXBLBinding> binding;
  GetBinding(aParent, getter_AddRefs(binding));

  if (!binding) {
    *aResult = nsnull;
    return NS_OK;
  }
  
  nsCOMPtr<nsIContent> defContent;
  return binding->GetInsertionPoint(aChild, aResult, aIndex,
                                    getter_AddRefs(defContent));
}

NS_IMETHODIMP
nsBindingManager::GetSingleInsertionPoint(nsIContent* aParent, nsIContent** aResult, PRUint32* aIndex,
                                          PRBool* aMultipleInsertionPoints)
{
  nsCOMPtr<nsIXBLBinding> binding;
  GetBinding(aParent, getter_AddRefs(binding));
  
  if (!binding) {
    *aMultipleInsertionPoints = PR_FALSE;
    *aResult = nsnull;
    return NS_OK;
  }

  nsCOMPtr<nsIContent> defContent;
  return binding->GetSingleInsertionPoint(aResult, aIndex,
                                          aMultipleInsertionPoints,
                                          getter_AddRefs(defContent));
}

NS_IMETHODIMP
nsBindingManager::AddLayeredBinding(nsIContent* aContent, nsIURI* aURL)
{
  // First we need to load our binding.
  nsresult rv;
  nsCOMPtr<nsIXBLService> xblService = 
           do_GetService("@mozilla.org/xbl;1", &rv);
  if (!xblService)
    return rv;

  // Load the bindings.
  nsCOMPtr<nsIXBLBinding> binding;
  PRBool dummy;
  xblService->LoadBindings(aContent, aURL, PR_TRUE, getter_AddRefs(binding), &dummy);
  if (binding) {
    AddToAttachedQueue(binding);
    ProcessAttachedQueue();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::RemoveLayeredBinding(nsIContent* aContent, nsIURI* aURL)
{
  nsCOMPtr<nsIXBLBinding> binding;
  GetBinding(aContent, getter_AddRefs(binding));
  
  if (!binding) {
    return NS_OK;
  }

  // For now we can only handle removing a binding if it's the only one
  nsCOMPtr<nsIXBLBinding> nextBinding;
  binding->GetBaseBinding(getter_AddRefs(nextBinding));
  NS_ENSURE_FALSE(nextBinding, NS_ERROR_FAILURE);

  // Make sure that the binding has the URI that is requested to be removed
  nsIURI* bindingUri = binding->BindingURI();
  
  PRBool equalUri;
  nsresult rv = aURL->Equals(bindingUri, &equalUri);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!equalUri) {
    return NS_OK;
  }

  // Make sure it isn't a style binding
  PRBool style;
  binding->IsStyleBinding(&style);
  if (style) {
    return NS_OK;
  }
  
  // Get to the document, this way is safer then using nsIContent::GetDocument
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aContent);
  NS_ASSERTION(node, "uh? RemoveLayeredBinding called on non-node");
  nsCOMPtr<nsIDOMDocument> domDoc;
  node->GetOwnerDocument(getter_AddRefs(domDoc));
  NS_ENSURE_TRUE(domDoc, NS_ERROR_UNEXPECTED);
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  NS_ASSERTION(doc, "document doesn't implement nsIDocument");
  
  // Finally remove the binding...
  binding->UnhookEventHandlers();
  binding->ChangeDocument(doc, nsnull);
  SetBinding(aContent, nsnull);
  binding->MarkForDeath();
  
  // ...and recreate it's frames. We need to do this since the frames may have
  // been removed and style may have changed due to the removal of the
  // anonymous children.
  nsIPresShell *presShell = doc->GetShellAt(0);
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  return presShell->RecreateFramesFor(aContent);;
}

NS_IMETHODIMP
nsBindingManager::LoadBindingDocument(nsIDocument* aBoundDoc,
                                      nsIURI* aURL,
                                      nsIDocument** aResult)
{
  NS_PRECONDITION(aURL, "Must have a URI to load!");
  
  nsCAutoString otherScheme;
  aURL->GetScheme(otherScheme);
  
  nsCAutoString scheme;
  aBoundDoc->GetDocumentURI()->GetScheme(scheme);

  // First we need to load our binding.
  *aResult = nsnull;
  nsresult rv;
  nsCOMPtr<nsIXBLService> xblService = 
           do_GetService("@mozilla.org/xbl;1", &rv);
  if (!xblService)
    return rv;

  // Load the binding doc.
  nsCOMPtr<nsIXBLDocumentInfo> info;
  xblService->LoadBindingDocumentInfo(nsnull, aBoundDoc, aURL,
                                      PR_TRUE, getter_AddRefs(info));
  if (!info)
    return NS_ERROR_FAILURE;

  // XXXbz Why is this based on a scheme comparison?  Shouldn't this
  // be a real security check???
    if (!strcmp(scheme.get(), otherScheme.get()))
    info->GetDocument(aResult); // Addref happens here.
    
  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::AddToAttachedQueue(nsIXBLBinding* aBinding)
{
  if (!mAttachedStack)
    NS_NewISupportsArray(getter_AddRefs(mAttachedStack)); // This call addrefs the array.

  mAttachedStack->AppendElement(aBinding);

  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::ClearAttachedQueue()
{
  if (mAttachedStack)
    mAttachedStack->Clear();
  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::ProcessAttachedQueue()
{
  if (!mAttachedStack || mProcessingAttachedStack)
    return NS_OK;

  mProcessingAttachedStack = PR_TRUE;

  PRUint32 count;
  while (NS_SUCCEEDED(mAttachedStack->Count(&count)) && count--) {
    nsCOMPtr<nsIXBLBinding> binding = do_QueryElementAt(mAttachedStack, count);
    mAttachedStack->RemoveElementAt(count);

    if (binding)
      binding->ExecuteAttachedHandler();
  }

  mProcessingAttachedStack = PR_FALSE;
  ClearAttachedQueue();
  return NS_OK;
}

PR_STATIC_CALLBACK(PLDHashOperator)
ExecuteDetachedHandler(PLDHashTable* aTable, PLDHashEntryHdr* aHdr, PRUint32 aNumber, void* aClosure)
{
  ObjectEntry* entry = NS_STATIC_CAST(ObjectEntry*, aHdr);
  nsIXBLBinding* binding = NS_STATIC_CAST(nsIXBLBinding*, entry->GetValue());
  binding->ExecuteDetachedHandler();
  return PL_DHASH_NEXT;
}


NS_IMETHODIMP
nsBindingManager::ExecuteDetachedHandlers()
{
  // Walk our hashtable of bindings.
  if (mBindingTable.ops)
    PL_DHashTableEnumerate(&mBindingTable, ExecuteDetachedHandler, nsnull);
  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::PutXBLDocumentInfo(nsIXBLDocumentInfo* aDocumentInfo)
{
  NS_PRECONDITION(aDocumentInfo, "Must have a non-null documentinfo!");
  
  NS_ENSURE_TRUE(mDocumentTable.IsInitialized() || mDocumentTable.Init(16),
                 NS_ERROR_OUT_OF_MEMORY);

  NS_ENSURE_TRUE(mDocumentTable.Put(aDocumentInfo->DocumentURI(),
                                    aDocumentInfo),
                 NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::RemoveXBLDocumentInfo(nsIXBLDocumentInfo* aDocumentInfo)
{
  if (!mDocumentTable.IsInitialized())
    return NS_OK;

  mDocumentTable.Remove(aDocumentInfo->DocumentURI());
  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::GetXBLDocumentInfo(nsIURI* aURL, nsIXBLDocumentInfo** aResult)
{
  *aResult = nsnull;
  if (!mDocumentTable.IsInitialized())
    return NS_OK;

  mDocumentTable.Get(aURL, aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::PutLoadingDocListener(nsIURI* aURL, nsIStreamListener* aListener)
{
  NS_PRECONDITION(aListener, "Must have a non-null listener!");
  
  NS_ENSURE_TRUE(mLoadingDocTable.IsInitialized() || mLoadingDocTable.Init(16),
                 NS_ERROR_OUT_OF_MEMORY);
  
  NS_ENSURE_TRUE(mLoadingDocTable.Put(aURL, aListener),
                 NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::GetLoadingDocListener(nsIURI* aURL, nsIStreamListener** aResult)
{
  *aResult = nsnull;
  if (!mLoadingDocTable.IsInitialized())
    return NS_OK;

  mLoadingDocTable.Get(aURL, aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::RemoveLoadingDocListener(nsIURI* aURL)
{
  if (!mLoadingDocTable.IsInitialized())
    return NS_OK;

  mLoadingDocTable.Remove(aURL);

  return NS_OK;
}

PR_STATIC_CALLBACK(PLDHashOperator)
MarkForDeath(PLDHashTable* aTable, PLDHashEntryHdr* aHdr, PRUint32 aNumber, void* aClosure)
{
  ObjectEntry* entry = NS_STATIC_CAST(ObjectEntry*, aHdr);
  nsIXBLBinding* binding = NS_STATIC_CAST(nsIXBLBinding*, entry->GetValue());
  
  PRBool marked = PR_FALSE;
  binding->MarkedForDeath(&marked);
  if (marked)
    return PL_DHASH_NEXT; // Already marked for death.

  nsCAutoString path;
  binding->DocURI()->GetPath(path);

  if (!strncmp(path.get(), "/skin", 5))
    binding->MarkForDeath();
  
  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsBindingManager::FlushSkinBindings()
{
  if (mBindingTable.ops)
    PL_DHashTableEnumerate(&mBindingTable, MarkForDeath, nsnull);
  return NS_OK;
}

// Used below to protect from recurring in QI calls through XPConnect.
struct AntiRecursionData {
  nsIContent* element; 
  REFNSIID iid; 
  AntiRecursionData* next;

  AntiRecursionData(nsIContent* aElement, 
                    REFNSIID aIID, 
                    AntiRecursionData* aNext)
    : element(aElement), iid(aIID), next(aNext) {}
};

NS_IMETHODIMP
nsBindingManager::GetBindingImplementation(nsIContent* aContent, REFNSIID aIID,
                                           void** aResult)
{
  *aResult = nsnull;
  nsCOMPtr<nsIXBLBinding> binding;
  GetBinding(aContent, getter_AddRefs(binding));
  if (binding) {
    PRBool supportsInterface;
    // The binding should not be asked for nsISupports
    NS_ASSERTION(!aIID.Equals(NS_GET_IID(nsISupports)), "Asking a binding for nsISupports");
    binding->ImplementsInterface(aIID, &supportsInterface);
    if (supportsInterface) {
      nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS;
      GetWrappedJS(aContent, getter_AddRefs(wrappedJS));

      if (wrappedJS) {
        // Protect from recurring in QI calls through XPConnect. 
        // This can happen when a second binding is being resolved.
        // At that point a wrappedJS exists, but it doesn't yet know about
        // the iid we are asking for. So, without this protection, 
        // AggregatedQueryInterface would end up recurring back into itself
        // through this code. 
        //
        // With this protection, when we detect the recursion we return 
        // NS_NOINTERFACE in the inner call. The outer call will then fall 
        // through (see below) and build a new chained wrappedJS for the iid.
        //
        // We're careful to not assume that only one direct nesting can occur
        // because there is a call into JS in the middle and we can't assume 
        // that this code won't be reached by some more complex nesting path.
        //
        // NOTE: We *assume* this is single threaded, so we can use a
        // static linked list to do the check.

        static AntiRecursionData* list = nsnull;

        for (AntiRecursionData* p = list; p; p = p->next) {
          if (p->element == aContent && p->iid.Equals(aIID)) {
            *aResult = nsnull;
            return NS_NOINTERFACE;
          }
        }

        AntiRecursionData item(aContent, aIID, list);
        list = &item;

        nsresult rv = wrappedJS->AggregatedQueryInterface(aIID, aResult);
        
        list = item.next;
        
        if (*aResult)
          return rv;
        
        // No result was found, so this must be another XBL interface.
        // Fall through to create a new wrapper.
      }

      // We have never made a wrapper for this implementation.
      // Create an XPC wrapper for the script object and hand it back.

      nsIDocument* doc = aContent->GetDocument();
      if (!doc)
        return NS_NOINTERFACE;

      nsIScriptGlobalObject *global = doc->GetScriptGlobalObject();
      if (!global)
        return NS_NOINTERFACE;

      nsIScriptContext *context = global->GetContext();
      if (!context)
        return NS_NOINTERFACE;

      JSContext* jscontext = (JSContext*)context->GetNativeContext();
      if (!jscontext)
        return NS_NOINTERFACE;

      nsCOMPtr<nsIXPConnect> xpConnect = do_GetService("@mozilla.org/js/xpc/XPConnect;1");
      if (!xpConnect)
        return NS_NOINTERFACE;

      nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
      xpConnect->GetWrappedNativeOfNativeObject(jscontext,
                                                JS_GetGlobalObject(jscontext),
                                                aContent,
                                                NS_GET_IID(nsISupports),
                                                getter_AddRefs(wrapper));
      NS_ENSURE_TRUE(wrapper, NS_NOINTERFACE);

      JSObject* jsobj = nsnull;

      wrapper->GetJSObject(&jsobj);
      NS_ENSURE_TRUE(jsobj, NS_NOINTERFACE);

      nsresult rv = xpConnect->WrapJSAggregatedToNative(aContent, jscontext,
                                                        jsobj, aIID, aResult);
      if (NS_FAILED(rv))
        return rv;

      // We successfully created a wrapper.  We will own this wrapper for as long as the binding remains
      // alive.  At the time the binding is cleared out of the bindingManager, we will remove the wrapper
      // from the bindingManager as well.
      nsISupports* supp = NS_STATIC_CAST(nsISupports*, *aResult);
      wrappedJS = do_QueryInterface(supp);
      SetWrappedJS(aContent, wrappedJS);

      return rv;
    }
  }
  
  *aResult = nsnull;
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsBindingManager::InheritsStyle(nsIContent* aContent, PRBool* aResult)
{
  // Get our enclosing parent.
  *aResult = PR_TRUE;
  nsCOMPtr<nsIContent> parent = GetEnclosingScope(aContent);
  if (parent) {
    // See if the parent is our parent.
    if (aContent->GetParent() == parent) {
      // Yes. Check the binding and see if it wants to allow us
      // to inherit styles.
      nsCOMPtr<nsIXBLBinding> binding;
      GetBinding(parent, getter_AddRefs(binding));
      if (binding)
        binding->InheritsStyle(aResult);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::UseDocumentRules(nsIContent* aContent, PRBool* aResult)
{
  if (!aContent)
    return NS_OK;

  *aResult = !GetOutermostStyleScope(aContent);
  return NS_OK;
}

nsIContent*
nsBindingManager::GetOutermostStyleScope(nsIContent* aContent)
{
  nsIContent* parent = GetEnclosingScope(aContent);
  while (parent) {
    PRBool inheritsStyle = PR_TRUE;
    nsCOMPtr<nsIXBLBinding> binding;
    GetBinding(parent, getter_AddRefs(binding));
    if (binding) {
      binding->InheritsStyle(&inheritsStyle);
    }
    if (!inheritsStyle)
      break;
    nsIContent* child = parent;
    parent = GetEnclosingScope(child);
    if (parent == child)
      break; // The scrollbar case only is deliberately hacked to return itself
             // (see GetBindingParent in nsXULElement.cpp).
  }
  return parent;
}

void
nsBindingManager::WalkRules(nsIStyleRuleProcessor::EnumFunc aFunc,
                            RuleProcessorData* aData,
                            nsIContent* aParent, nsIContent* aCurrContent)
{
  nsCOMPtr<nsIXBLBinding> binding;
  GetBinding(aCurrContent, getter_AddRefs(binding));
  if (binding) {
    aData->mScopedRoot = aCurrContent;
    binding->WalkRules(aFunc, aData);
  }
  if (aParent != aCurrContent) {
    nsCOMPtr<nsIContent> par = GetEnclosingScope(aCurrContent);
    if (par)
      WalkRules(aFunc, aData, aParent, par);
  }
}

NS_IMETHODIMP
nsBindingManager::WalkRules(nsStyleSet* aStyleSet,
                            nsIStyleRuleProcessor::EnumFunc aFunc,
                            RuleProcessorData* aData)
{
  nsIContent *content = aData->mContent;
  if (!content)
    return NS_OK;

  nsCOMPtr<nsIContent> parent = GetOutermostStyleScope(content);

  WalkRules(aFunc, aData, parent, content);

  // Null out the scoped root that we set repeatedly in the other |WalkRules|.
  aData->mScopedRoot = nsnull;

  if (parent) {
    // We cut ourselves off, but we still need to walk the document's attribute sheet
    // so that inline style continues to work on anonymous content.
    nsIDocument* doc = content->GetDocument();
    if (doc) {
      nsCOMPtr<nsIStyleRuleProcessor> inlineCSS(
              do_QueryInterface(doc->GetInlineStyleSheet()));
      if (inlineCSS)
        (*aFunc)(inlineCSS, aData);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::ShouldBuildChildFrames(nsIContent* aContent, PRBool* aResult)
{
  *aResult = PR_TRUE;

  nsCOMPtr<nsIXBLBinding> binding;
  GetBinding(aContent, getter_AddRefs(binding));

  if (binding)
    return binding->ShouldBuildChildFrames(aResult);

  return NS_OK;
}

nsresult
nsBindingManager::GetNestedInsertionPoint(nsIContent* aParent, nsIContent* aChild, nsIContent** aResult)
{
  *aResult = nsnull;

  // Check to see if the content is anonymous.
  if (aChild->GetBindingParent() == aParent)
    return NS_OK; // It is anonymous. Don't use the insertion point, since that's only
                  // for the explicit kids.

  nsCOMPtr<nsIContent> insertionElement;
  PRUint32 index;
  GetInsertionPoint(aParent, aChild, getter_AddRefs(insertionElement), &index);
  if (insertionElement != aParent) {
    // See if we nest even further in.
    nsCOMPtr<nsIContent> nestedPoint;
    GetNestedInsertionPoint(insertionElement, aChild, getter_AddRefs(nestedPoint));
    if (nestedPoint)
      insertionElement = nestedPoint;
  }

  *aResult = insertionElement;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

void
nsBindingManager::ContentAppended(nsIDocument* aDocument,
                                  nsIContent* aContainer,
                                  PRInt32     aNewIndexInContainer)
{
  // XXX This is hacked and not quite correct. See below.
  if (aNewIndexInContainer == -1 || !mContentListTable.ops)
    // It's anonymous.
    return;

  PRInt32 childCount = aContainer->GetChildCount();

  nsIContent *child = aContainer->GetChildAt(aNewIndexInContainer);

  nsCOMPtr<nsIContent> ins;
  GetNestedInsertionPoint(aContainer, child, getter_AddRefs(ins));

  if (ins) {
    nsCOMPtr<nsIDOMNodeList> nodeList;
    PRBool isAnonymousContentList;
    GetXBLChildNodesInternal(ins, getter_AddRefs(nodeList),
                             &isAnonymousContentList);

    if (nodeList && isAnonymousContentList) {
      // Find a non-pseudo-insertion point and just jam ourselves in.
      // This is not 100% correct.  Hack city, baby.
      nsAnonymousContentList* contentList = NS_STATIC_CAST(nsAnonymousContentList*, NS_STATIC_CAST(nsIDOMNodeList*, nodeList.get()));

      PRInt32 count = contentList->GetInsertionPointCount();
      for (PRInt32 i = 0; i < count; i++) {
        nsXBLInsertionPoint* point = contentList->GetInsertionPointAt(i);
        PRInt32 index = point->GetInsertionIndex();
        if (index != -1) {
          // We're real. Jam all the kids in.
          // XXX Check the filters to find the correct points.
          for (PRInt32 j = aNewIndexInContainer; j < childCount; j++) {
            child = aContainer->GetChildAt(j);
            point->AddChild(child);
            SetInsertionParent(child, ins);
          }
          break;
        }
      }
    }
  }
}

void
nsBindingManager::ContentInserted(nsIDocument* aDocument,
                                  nsIContent* aContainer,
                                  nsIContent* aChild,
                                  PRInt32 aIndexInContainer)
{
// XXX This is hacked just to make menus work again.
  if (aIndexInContainer == -1 || !mContentListTable.ops)
    // It's anonymous.
    return;

  nsCOMPtr<nsIContent> ins;
  GetNestedInsertionPoint(aContainer, aChild, getter_AddRefs(ins));

  if (ins) {
    nsCOMPtr<nsIDOMNodeList> nodeList;
    PRBool isAnonymousContentList;
    GetXBLChildNodesInternal(ins, getter_AddRefs(nodeList),
                             &isAnonymousContentList);

    if (nodeList && isAnonymousContentList) {
      // Find a non-pseudo-insertion point and just jam ourselves in.
      // This is not 100% correct.  Hack city, baby.
      nsAnonymousContentList* contentList = NS_STATIC_CAST(nsAnonymousContentList*, NS_STATIC_CAST(nsIDOMNodeList*, nodeList.get()));

      PRInt32 count = contentList->GetInsertionPointCount();
      for (PRInt32 i = 0; i < count; i++) {
        nsXBLInsertionPoint* point = contentList->GetInsertionPointAt(i);
        if (point->GetInsertionIndex() != -1) {
          // We're real. Jam the kid in.
          // XXX Check the filters to find the correct points.

          // Find the right insertion spot.  Can't just insert in the insertion
          // point at aIndexInContainer since the point may contain anonymous
          // content, not all of aContainer's kids, etc.  So find the last
          // child of aContainer that comes before aIndexInContainer and is in
          // the insertion point and insert right after it.
          PRInt32 pointSize = point->ChildCount();
          PRBool inserted = PR_FALSE;
          for (PRInt32 parentIndex = aIndexInContainer - 1;
               parentIndex >= 0 && !inserted; --parentIndex) {
            nsIContent* currentSibling = aContainer->GetChildAt(parentIndex);
            for (PRInt32 pointIndex = pointSize - 1; pointIndex >= 0;
                 --pointIndex) {
              nsCOMPtr<nsIContent> currContent = point->ChildAt(pointIndex);
              if (currContent == currentSibling) {
                point->InsertChildAt(pointIndex + 1, aChild);
                inserted = PR_TRUE;
                break;
              }
            }
          }
          if (!inserted) {
            // None of our previous siblings are in here... just stick
            // ourselves in at the beginning of the insertion point.
            // XXXbz if we ever start doing the filter thing right, this may be
            // no good, since we may _still_ have anonymous kids in there and
            // may need to get the ordering with those right.
            point->InsertChildAt(0, aChild);
          }
          SetInsertionParent(aChild, ins);
          break;
        }
      }
    }
  }
}

void
nsBindingManager::ContentRemoved(nsIDocument* aDocument,
                                 nsIContent* aContainer,
                                 nsIContent* aChild,
                                 PRInt32 aIndexInContainer)
{
  if (aIndexInContainer == -1 || !mContentListTable.ops)
    // It's anonymous.
    return;

  nsCOMPtr<nsIContent> point;
  GetNestedInsertionPoint(aContainer, aChild, getter_AddRefs(point));

  if (point) {
    nsCOMPtr<nsIDOMNodeList> nodeList;
    PRBool isAnonymousContentList;
    GetXBLChildNodesInternal(point, getter_AddRefs(nodeList),
                             &isAnonymousContentList);

    if (nodeList && isAnonymousContentList) {
      // Find a non-pseudo-insertion point and remove ourselves.
      nsAnonymousContentList* contentList = NS_STATIC_CAST(nsAnonymousContentList*, NS_STATIC_CAST(nsIDOMNodeList*, nodeList));
      PRInt32 count = contentList->GetInsertionPointCount();
      for (PRInt32 i =0; i < count; i++) {
        nsXBLInsertionPoint* point = contentList->GetInsertionPointAt(i);
        if (point->GetInsertionIndex() != -1) {
          point->RemoveChild(aChild);
        }
      }
    }
  }
}


// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewBindingManager(nsIBindingManager** aResult)
{
  *aResult = new nsBindingManager;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

