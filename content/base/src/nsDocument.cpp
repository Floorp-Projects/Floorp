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
 *   Johnny Stenback <jst@netscape.com>
 *   L. David Baron  <dbaron@dbaron.org>
 *   Pierre Phaneuf  <pp@ludusdesign.com>
 *   Pete Collins    <petejc@collab.net>
 *   James Ross      <silver@warwickcompsocc.o.uk>
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
 * Base class for all our document implementations.
 */

#include "plstr.h"

#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsDocument.h"
#include "nsUnicharUtils.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIEventStateManager.h"
#include "nsContentList.h"
#include "nsIObserver.h"
#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsCOMArray.h"

#include "nsGUIEvent.h"

#include "nsIDOMStyleSheet.h"
#include "nsDOMAttribute.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMDocumentXBL.h"
#include "nsGenericElement.h"
#include "nsIDOMEventGroup.h"
#include "nsIDOMCDATASection.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsDOMString.h"

#include "nsRange.h"
#include "nsIDOMText.h"
#include "nsIDOMComment.h"
#include "nsDOMDocumentType.h"
#include "nsTreeWalker.h"

#include "nsIServiceManager.h"

#include "nsContentCID.h"
#include "nsDOMError.h"
#include "nsIScrollableView.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"
#include "nsNodeInfoManager.h"
#include "nsIXBLService.h"
#include "nsIXPointer.h"
#include "nsIFileChannel.h"
#include "nsIMultiPartChannel.h"
#include "nsIRefreshURI.h"
#include "nsIWebNavigation.h"

#include "nsNetUtil.h"     // for NS_MakeAbsoluteURI

#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsIPrivateDOMImplementation.h"

#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMElement.h"

#include "nsXULAtoms.h"

// for radio group stuff
#include "nsIDOMHTMLInputElement.h"
#include "nsIRadioVisitor.h"
#include "nsIFormControl.h"

#include "nsXMLEventsManager.h"

#include "nsBidiUtils.h"

static NS_DEFINE_CID(kDOMEventGroupCID, NS_DOMEVENTGROUP_CID);

#include "nsHTMLAtoms.h"
#include "nsIDOMUserDataHandler.h"
#include "nsScriptEventManager.h"
#include "nsIDOMXPathEvaluator.h"
#include "nsIXPathEvaluatorInternal.h"
#include "nsIParserService.h"
#include "nsContentCreatorFunctions.h"

#include "nsIScriptContext.h"
#include "nsBindingManager.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIRequest.h"
#include "nsILink.h"

#include "nsICharsetAlias.h"
#include "nsIParser.h"

#include "nsDateTimeFormatCID.h"
#include "nsIDateTimeFormat.h"
#include "nsEventDispatcher.h"
#include "nsIDOMXPathEvaluator.h"
#include "nsDOMCID.h"

#include "nsLayoutStatics.h"

#ifdef MOZ_LOGGING
// so we can get logging even in release builds
#define FORCE_PR_LOG 1
#endif
#include "prlog.h"

#ifdef PR_LOGGING
static PRLogModuleInfo* gDocumentLeakPRLog;
#endif

static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);

void
nsUint32ToContentHashEntry::Destroy()
{
  HashSet* set = GetHashSet();
  if (set) {
    delete set;
  } else {
    nsIContent* content = GetContent();
    NS_IF_RELEASE(content);
  }
}

nsresult
nsUint32ToContentHashEntry::PutContent(nsIContent* aVal)
{
  // Add the value to the hash if it is there
  HashSet* set = GetHashSet();
  if (set) {
    nsISupportsHashKey* entry = set->PutEntry(aVal);
    return entry ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  }

  // If an element is already there, create a hashtable and both of these to it
  nsIContent* oldVal = GetContent();
  if (oldVal) {
    nsresult rv = InitHashSet(&set);
    NS_ENSURE_SUCCESS(rv, rv);

    nsISupportsHashKey* entry = set->PutEntry(oldVal);
    if (!entry) {
      // OOM - we can't insert aVal, but we can at least put oldVal back (even
      // if we didn't, we'd still have to release oldVal so that we don't leak)
      delete set;
      SetContent(oldVal);
      // SetContent adds another reference, so release the one we had
      NS_RELEASE(oldVal);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    // The hashset adds its own reference, so release the one we had
    NS_RELEASE(oldVal);

    entry = set->PutEntry(aVal);
    return entry ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  }

  // Nothing exists in the hash right now, so just set the single pointer
  return SetContent(aVal);
}

void
nsUint32ToContentHashEntry::RemoveContent(nsIContent* aVal)
{
  // Remove from the hash if the hash is there
  HashSet* set = GetHashSet();
  if (set) {
    set->RemoveEntry(aVal);
    if (set->Count() == 0) {
      delete set;
      mValOrHash = nsnull;
    }
    return;
  }

  // Remove the ptr if there is just a ptr
  nsIContent* v = GetContent();
  if (v == aVal) {
    NS_IF_RELEASE(v);
    mValOrHash = nsnull;
  }
}

nsresult
nsUint32ToContentHashEntry::InitHashSet(HashSet** aSet)
{
  HashSet* newSet = new HashSet();
  if (!newSet) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = newSet->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  mValOrHash = newSet;
  *aSet = newSet;
  return NS_OK;
}

static PLDHashOperator PR_CALLBACK
nsUint32ToContentHashEntryVisitorCallback(nsISupportsHashKey* aEntry,
                                          void* aClosure)
{
  nsUint32ToContentHashEntry::Visitor* visitor =
    NS_STATIC_CAST(nsUint32ToContentHashEntry::Visitor*, aClosure);
  visitor->Visit(NS_STATIC_CAST(nsIContent*, aEntry->GetKey()));
  return PL_DHASH_NEXT;
}

void
nsUint32ToContentHashEntry::VisitContent(Visitor* aVisitor)
{
  HashSet* set = GetHashSet();
  if (set) {
    set->EnumerateEntries(nsUint32ToContentHashEntryVisitorCallback, aVisitor);
    if (set->Count() == 0) {
      delete set;
      mValOrHash = nsnull;
    }
    return;
  }

  nsIContent* v = GetContent();
  if (v) {
    aVisitor->Visit(v);
  }
}

// Helper structs for the content->subdoc map

class SubDocMapEntry : public PLDHashEntryHdr
{
public:
  // Both of these are strong references
  nsIContent *mKey; // must be first, to look like PLDHashEntryStub
  nsIDocument *mSubDocument;
};

struct FindContentData
{
  FindContentData(nsIDocument *aSubDoc)
    : mSubDocument(aSubDoc), mResult(nsnull)
  {
  }

  nsISupports *mSubDocument;
  nsIContent *mResult;
};


/**
 * A struct that holds all the information about a radio group.
 */
struct nsRadioGroupStruct
{
  /**
   * A strong pointer to the currently selected radio button.
   */
  nsCOMPtr<nsIDOMHTMLInputElement> mSelectedRadioButton;
  nsSmallVoidArray mRadioButtons;
};


nsDOMStyleSheetList::nsDOMStyleSheetList(nsIDocument *aDocument)
{
  mLength = -1;
  // Not reference counted to avoid circular references.
  // The document will tell us when its going away.
  mDocument = aDocument;
  mDocument->AddObserver(this);
}

nsDOMStyleSheetList::~nsDOMStyleSheetList()
{
  if (mDocument) {
    mDocument->RemoveObserver(this);
  }
}


// XXX couldn't we use the GetIIDs method from CSSStyleSheetList here?
// QueryInterface implementation for nsDOMStyleSheetList
NS_INTERFACE_MAP_BEGIN(nsDOMStyleSheetList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStyleSheetList)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMStyleSheetList)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(DocumentStyleSheetList)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsDOMStyleSheetList)
NS_IMPL_RELEASE(nsDOMStyleSheetList)


NS_IMETHODIMP
nsDOMStyleSheetList::GetLength(PRUint32* aLength)
{
  if (mDocument) {
    // XXX Find the number and then cache it. We'll use the
    // observer notification to figure out if new ones have
    // been added or removed.
    if (-1 == mLength) {
      mLength = mDocument->GetNumberOfStyleSheets();

#ifdef DEBUG
      PRInt32 i;
      for (i = 0; i < mLength; i++) {
        nsIStyleSheet *sheet = mDocument->GetStyleSheetAt(i);
        nsCOMPtr<nsIDOMStyleSheet> domss(do_QueryInterface(sheet));
        NS_ASSERTION(domss, "All \"normal\" sheets implement nsIDOMStyleSheet");
      }
#endif
    }
    *aLength = mLength;
  }
  else {
    *aLength = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMStyleSheetList::Item(PRUint32 aIndex, nsIDOMStyleSheet** aReturn)
{
  *aReturn = nsnull;
  if (mDocument) {
    PRInt32 count = mDocument->GetNumberOfStyleSheets();
    if (aIndex < (PRUint32)count) {
      nsIStyleSheet *sheet = mDocument->GetStyleSheetAt(aIndex);
      NS_ASSERTION(sheet, "Must have a sheet");
      return CallQueryInterface(sheet, aReturn);
    }
  }

  return NS_OK;
}

void
nsDOMStyleSheetList::DocumentWillBeDestroyed(nsIDocument *aDocument)
{
  if (nsnull != mDocument) {
    aDocument->RemoveObserver(this);
    mDocument = nsnull;
  }
}

void
nsDOMStyleSheetList::StyleSheetAdded(nsIDocument *aDocument,
                                     nsIStyleSheet* aStyleSheet,
                                     PRBool aDocumentSheet)
{
  if (aDocumentSheet && -1 != mLength) {
    nsCOMPtr<nsIDOMStyleSheet> domss(do_QueryInterface(aStyleSheet));
    if (domss) {
      mLength++;
    }
  }
}

void
nsDOMStyleSheetList::StyleSheetRemoved(nsIDocument *aDocument,
                                       nsIStyleSheet* aStyleSheet,
                                       PRBool aDocumentSheet)
{
  if (aDocumentSheet && -1 != mLength) {
    nsCOMPtr<nsIDOMStyleSheet> domss(do_QueryInterface(aStyleSheet));
    if (domss) {
      mLength--;
    }
  }
}

// nsOnloadBlocker implementation
NS_IMPL_ISUPPORTS1(nsOnloadBlocker, nsIRequest)

NS_IMETHODIMP
nsOnloadBlocker::GetName(nsACString &aResult)
{ 
  aResult.AssignLiteral("about:document-onload-blocker");
  return NS_OK;
}

NS_IMETHODIMP
nsOnloadBlocker::IsPending(PRBool *_retval)
{
  *_retval = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsOnloadBlocker::GetStatus(nsresult *status)
{
  *status = NS_OK;
  return NS_OK;
} 

NS_IMETHODIMP
nsOnloadBlocker::Cancel(nsresult status)
{
  return NS_OK;
}
NS_IMETHODIMP
nsOnloadBlocker::Suspend(void)
{
  return NS_OK;
}
NS_IMETHODIMP
nsOnloadBlocker::Resume(void)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOnloadBlocker::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
  *aLoadGroup = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsOnloadBlocker::SetLoadGroup(nsILoadGroup * aLoadGroup)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOnloadBlocker::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
  *aLoadFlags = nsIRequest::LOAD_NORMAL;
  return NS_OK;
}

NS_IMETHODIMP
nsOnloadBlocker::SetLoadFlags(nsLoadFlags aLoadFlags)
{
  return NS_OK;
}


class nsDOMImplementation : public nsIDOMDOMImplementation,
                            public nsIPrivateDOMImplementation
{
public:
  nsDOMImplementation(nsIURI* aDocumentURI,
                      nsIURI* aBaseURI,
                      nsIPrincipal* aPrincipal);
  virtual ~nsDOMImplementation();

  NS_DECL_ISUPPORTS

  // nsIDOMDOMImplementation
  NS_DECL_NSIDOMDOMIMPLEMENTATION

  // nsIPrivateDOMImplementation
  NS_IMETHOD Init(nsIURI* aDocumentURI, nsIURI* aBaseURI,
                  nsIPrincipal* aPrincipal);

protected:
  nsCOMPtr<nsIURI> mDocumentURI;
  nsCOMPtr<nsIURI> mBaseURI;
  nsCOMPtr<nsIPrincipal> mPrincipal;
};


nsresult
NS_NewDOMImplementation(nsIDOMDOMImplementation** aInstancePtrResult)
{
  *aInstancePtrResult = new nsDOMImplementation(nsnull, nsnull, nsnull);
  if (!*aInstancePtrResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}

nsDOMImplementation::nsDOMImplementation(nsIURI* aDocumentURI,
                                         nsIURI* aBaseURI,
                                         nsIPrincipal* aPrincipal)
  : mDocumentURI(aDocumentURI),
    mBaseURI(aBaseURI),
    mPrincipal(aPrincipal)
{
}

nsDOMImplementation::~nsDOMImplementation()
{
}

// QueryInterface implementation for nsDOMImplementation
NS_INTERFACE_MAP_BEGIN(nsDOMImplementation)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDOMImplementation)
  NS_INTERFACE_MAP_ENTRY(nsIPrivateDOMImplementation)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMDOMImplementation)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(DOMImplementation)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsDOMImplementation)
NS_IMPL_RELEASE(nsDOMImplementation)


NS_IMETHODIMP
nsDOMImplementation::HasFeature(const nsAString& aFeature,
                                const nsAString& aVersion,
                                PRBool* aReturn)
{
  return nsGenericElement::InternalIsSupported(
           NS_STATIC_CAST(nsIDOMDOMImplementation*, this),
           aFeature, aVersion, aReturn);
}

NS_IMETHODIMP
nsDOMImplementation::CreateDocumentType(const nsAString& aQualifiedName,
                                        const nsAString& aPublicId,
                                        const nsAString& aSystemId,
                                        nsIDOMDocumentType** aReturn)
{
  *aReturn = nsnull;

  nsresult rv = nsContentUtils::CheckQName(aQualifiedName);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAtom> name = do_GetAtom(aQualifiedName);
  NS_ENSURE_TRUE(name, NS_ERROR_OUT_OF_MEMORY);
    
  return NS_NewDOMDocumentType(aReturn, nsnull, mPrincipal, name, nsnull,
                               nsnull, aPublicId, aSystemId, EmptyString());
}

NS_IMETHODIMP
nsDOMImplementation::CreateDocument(const nsAString& aNamespaceURI,
                                    const nsAString& aQualifiedName,
                                    nsIDOMDocumentType* aDoctype,
                                    nsIDOMDocument** aReturn)
{
  *aReturn = nsnull;

  nsresult rv;
  if (!aQualifiedName.IsEmpty()) {
    nsIParserService *parserService = nsContentUtils::GetParserService();
    NS_ENSURE_TRUE(parserService, NS_ERROR_FAILURE);

    const nsAFlatString& qName = PromiseFlatString(aQualifiedName);
    const PRUnichar *colon;
    rv = parserService->CheckQName(qName, PR_TRUE, &colon);
    NS_ENSURE_SUCCESS(rv, rv);

    if (colon &&
        (DOMStringIsNull(aNamespaceURI) ||
         (Substring(qName.get(), colon).EqualsLiteral("xml") &&
          !aNamespaceURI.EqualsLiteral("http://www.w3.org/XML/1998/namespace")))) {
      return NS_ERROR_DOM_NAMESPACE_ERR;
    }
  }
  else if (DOMStringIsNull(aQualifiedName) &&
           !DOMStringIsNull(aNamespaceURI)) {
    return NS_ERROR_DOM_NAMESPACE_ERR;
  }

  if (aDoctype) {
    nsCOMPtr<nsIDOMDocument> owner;
    aDoctype->GetOwnerDocument(getter_AddRefs(owner));
    if (owner) {
      return NS_ERROR_DOM_WRONG_DOCUMENT_ERR;
    }
  }

  rv = NS_NewDOMDocument(aReturn, aNamespaceURI, aQualifiedName, aDoctype,
                         mDocumentURI, mBaseURI, mPrincipal);

  nsIDocShell *docShell = nsContentUtils::GetDocShellFromCaller();
  if (docShell) {
    nsCOMPtr<nsPresContext> presContext;
    docShell->GetPresContext(getter_AddRefs(presContext));
    if (presContext) {
      nsCOMPtr<nsISupports> container = presContext->GetContainer();
      nsCOMPtr<nsIDocument> document = do_QueryInterface(*aReturn);
      if (document) {
        document->SetContainer(container);
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
nsDOMImplementation::Init(nsIURI* aDocumentURI, nsIURI* aBaseURI,
                          nsIPrincipal* aPrincipal)
{
  // Note: can't require that the args be non-null, since at least one
  // caller (XMLHttpRequest) doesn't have decent args to pass in.
  mDocumentURI = aDocumentURI;
  mBaseURI = aBaseURI;
  mPrincipal = aPrincipal;
  return NS_OK;
}

// ==================================================================
// =
// ==================================================================

  // NOTE! nsDocument::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

nsDocument::nsDocument(const char* aContentType)
  : nsIDocument(),
    mVisible(PR_TRUE)
{
  nsLayoutStatics::AddRef();
  mContentType = aContentType;
  
#ifdef PR_LOGGING
  if (!gDocumentLeakPRLog)
    gDocumentLeakPRLog = PR_NewLogModule("DocumentLeak");

  if (gDocumentLeakPRLog)
    PR_LOG(gDocumentLeakPRLog, PR_LOG_DEBUG,
           ("DOCUMENT %p created", this));
#endif
}

nsDocument::~nsDocument()
{
#ifdef PR_LOGGING
  if (gDocumentLeakPRLog)
    PR_LOG(gDocumentLeakPRLog, PR_LOG_DEBUG,
           ("DOCUMENT %p destroyed", this));
#endif

  mInDestructor = PR_TRUE;

  // We can't rely on the nsINode dtor doing this for us since
  // by the time it runs GetOwnerDoc will return null.
  // This is because we call mNodeInfoManager->DropReference()
  // below, which will run before the nsINode dtor. Additionally
  // the properties hash and the document will have been destroyed,
  // so there would be no way to find the handlers.
  CallUserDataHandler(nsIDOMUserDataHandler::NODE_DELETED,
                      this, nsnull, nsnull);

  // XXX Inform any remaining observers that we are going away.
  // Note that this currently contradicts the rule that all
  // observers must hold on to live references to the document.
  // This notification will occur only after the reference has
  // been dropped.

  PRInt32 indx;
  NS_DOCUMENT_NOTIFY_OBSERVERS(DocumentWillBeDestroyed, (this));

  mParentDocument = nsnull;

  // Kill the subdocument map, doing this will release its strong
  // references, if any.
  if (mSubDocuments) {
    PL_DHashTableDestroy(mSubDocuments);

    mSubDocuments = nsnull;
  }

  if (mRootContent) {
    if (mRootContent->GetCurrentDoc()) {
      NS_ASSERTION(mRootContent->GetCurrentDoc() == this,
                   "Unexpected current doc in root content");
      // The root content still has a pointer back to the document,
      // clear the document pointer in all children.
      
      // Destroy link map now so we don't waste time removing
      // links one by one
      DestroyLinkMap();

      PRUint32 count = mChildren.ChildCount();
      for (indx = PRInt32(count) - 1; indx >= 0; --indx) {
        mChildren.ChildAt(indx)->UnbindFromTree();
        mChildren.RemoveChildAt(indx);
      }
    }
  }

  mRootContent = nsnull;

  // Let the stylesheets know we're going away
  indx = mStyleSheets.Count();
  while (--indx >= 0) {
    mStyleSheets[indx]->SetOwningDocument(nsnull);
  }
  indx = mCatalogSheets.Count();
  while (--indx >= 0) {
    mCatalogSheets[indx]->SetOwningDocument(nsnull);
  }
  if (mAttrStyleSheet)
    mAttrStyleSheet->SetOwningDocument(nsnull);
  if (mStyleAttrStyleSheet)
    mStyleAttrStyleSheet->SetOwningDocument(nsnull);

  if (mChildNodes) {
    mChildNodes->DropReference();
  }

  if (mListenerManager) {
    mListenerManager->Disconnect();
  }

  if (mScriptLoader) {
    mScriptLoader->DropDocumentReference();
  }

  if (mCSSLoader) {
    // Could be null here if Init() failed
    mCSSLoader->DropDocumentReference();
    NS_RELEASE(mCSSLoader);
  }

  // XXX Ideally we'd do this cleanup in the nsIDocument destructor.
  if (mNodeInfoManager) {
    mNodeInfoManager->DropDocumentReference();
    NS_RELEASE(mNodeInfoManager);
  }

  if (mAttrStyleSheet) {
    mAttrStyleSheet->SetOwningDocument(nsnull);
  }
  
  if (mStyleAttrStyleSheet) {
    mStyleAttrStyleSheet->SetOwningDocument(nsnull);
  }

  delete mHeaderData;
  delete mBoxObjectTable;
  nsLayoutStatics::Release();
}

NS_INTERFACE_MAP_BEGIN(nsDocument)
  NS_INTERFACE_MAP_ENTRY(nsIDocument)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocument)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSDocument)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOM3DocumentEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentStyle)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSDocumentStyle)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentView)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentRange)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentTraversal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentXBL)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventReceiver)
  NS_INTERFACE_MAP_ENTRY(nsIDOMGCParticipant)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOM3EventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOM3Node)
  NS_INTERFACE_MAP_ENTRY(nsIDOM3Document)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIRadioGroupContainer)
  NS_INTERFACE_MAP_ENTRY(nsINode)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocument)
  if (aIID.Equals(NS_GET_IID(nsIDOMXPathEvaluator)) ||
      aIID.Equals(NS_GET_IID(nsIXPathEvaluatorInternal))) {
    if (!mXPathEvaluatorTearoff) {
      nsresult rv;
      mXPathEvaluatorTearoff =
        do_CreateInstance(NS_XPATH_EVALUATOR_CONTRACTID,
                          NS_STATIC_CAST(nsIDocument *, this), &rv);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    return mXPathEvaluatorTearoff->QueryInterface(aIID, aInstancePtr);
  }
  else
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDocument)
NS_IMPL_RELEASE(nsDocument)

nsresult
nsDocument::Init()
{
  if (mBindingManager || mCSSLoader || mNodeInfoManager) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  mLinkMap.Init();

  // Force initialization.
  nsBindingManager *bindingManager = new nsBindingManager();
  NS_ENSURE_TRUE(bindingManager, NS_ERROR_OUT_OF_MEMORY);
  mBindingManager = bindingManager;

  // The binding manager must always be the first observer of the document.
  // (static cast to the correct interface pointer)
  mObservers.InsertElementAt(NS_STATIC_CAST(nsIDocumentObserver*, bindingManager), 0);

  mOnloadBlocker = new nsOnloadBlocker();
  NS_ENSURE_TRUE(mOnloadBlocker, NS_ERROR_OUT_OF_MEMORY);
  
  NS_NewCSSLoader(this, &mCSSLoader);
  NS_ENSURE_TRUE(mCSSLoader, NS_ERROR_OUT_OF_MEMORY);
  // Assume we're not HTML and not quirky, until we know otherwise
  mCSSLoader->SetCaseSensitive(PR_TRUE);
  mCSSLoader->SetCompatibilityMode(eCompatibility_FullStandards);

  mNodeInfoManager = new nsNodeInfoManager();
  NS_ENSURE_TRUE(mNodeInfoManager, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(mNodeInfoManager);

  nsresult  rv = mNodeInfoManager->Init(this);
  NS_ENSURE_SUCCESS(rv, rv);

  mNodeInfo = mNodeInfoManager->GetDocumentNodeInfo();
  NS_ENSURE_TRUE(mNodeInfo, NS_ERROR_OUT_OF_MEMORY);

  NS_ASSERTION(GetOwnerDoc() == this, "Our nodeinfo is busted!");

  return NS_OK;
}

nsresult
nsDocument::AddXMLEventsContent(nsIContent *aXMLEventsElement)
{
  if (!mXMLEventsManager) {
    mXMLEventsManager = new nsXMLEventsManager();
    NS_ENSURE_TRUE(mXMLEventsManager, NS_ERROR_OUT_OF_MEMORY);
    AddObserver(mXMLEventsManager);
  }
  mXMLEventsManager->AddXMLEventsContent(aXMLEventsElement);
  return NS_OK;
}

void
nsDocument::Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup)
{
  nsCOMPtr<nsIURI> uri;
  if (aChannel) {
    // Note: this code is duplicated in nsXULDocument::StartDocumentLoad.
    // Note: this should match nsDocShell::OnLoadingSite
    nsLoadFlags loadFlags = 0;
    nsresult rv = aChannel->GetLoadFlags(&loadFlags);
    if (NS_SUCCEEDED(rv) && (loadFlags & nsIChannel::LOAD_REPLACE)) {
      aChannel->GetURI(getter_AddRefs(uri));
    } else {
      aChannel->GetOriginalURI(getter_AddRefs(uri));
    }
  }

  ResetToURI(uri, aLoadGroup);

  if (aChannel) {
    nsCOMPtr<nsISupports> owner;
    if (NS_SUCCEEDED(aChannel->GetOwner(getter_AddRefs(owner)))) {
      nsCOMPtr<nsIPrincipal> principal = do_QueryInterface(owner);
      if (principal) {
        SetPrincipal(principal);
      }
    }
  }

  mChannel = aChannel;
}

void
nsDocument::ResetToURI(nsIURI *aURI, nsILoadGroup *aLoadGroup)
{
  NS_PRECONDITION(aURI, "Null URI passed to ResetToURI");

#ifdef PR_LOGGING
  if (gDocumentLeakPRLog && PR_LOG_TEST(gDocumentLeakPRLog, PR_LOG_DEBUG)) {
    nsCAutoString spec;
    aURI->GetSpec(spec);
    PR_LogPrint("DOCUMENT %p ResetToURI %s", this, spec.get());
  }
#endif

  mDocumentTitle.SetIsVoid(PR_TRUE);

  SetPrincipal(nsnull);
  mSecurityInfo = nsnull;

  mDocumentLoadGroup = nsnull;

  // Delete references to sub-documents and kill the subdocument map,
  // if any. It holds strong references
  if (mSubDocuments) {
    PL_DHashTableDestroy(mSubDocuments);

    mSubDocuments = nsnull;
  }

  // Destroy link map now so we don't waste time removing
  // links one by one
  DestroyLinkMap();

  mRootContent = nsnull;
  PRUint32 count = mChildren.ChildCount();
  for (PRInt32 i = PRInt32(count) - 1; i >= 0; i--) {
    nsCOMPtr<nsIContent> content = mChildren.ChildAt(i);

    ContentRemoved(nsnull, content, i);
    content->UnbindFromTree();
    mChildren.RemoveChildAt(i);
  }

  // Reset our stylesheets
  ResetStylesheetsToURI(aURI);
  
  // Release the listener manager
  mListenerManager = nsnull;

  // Release the stylesheets list.
  mDOMStyleSheets = nsnull;

  SetDocumentURI(aURI);
  mDocumentBaseURI = mDocumentURI;

  if (aLoadGroup) {
    mDocumentLoadGroup = do_GetWeakReference(aLoadGroup);
    // there was an assertion here that aLoadGroup was not null.  This
    // is no longer valid nsWebShell::SetDocument does not create a
    // load group, and it works just fine.
  }

  mLastModified.Truncate();
  // XXXbz I guess we're assuming that the caller will either pass in
  // a channel with a useful type or call SetContentType?
  mContentType.Truncate();
  mContentLanguage.Truncate();
  mBaseTarget.Truncate();
  mReferrer.Truncate();

  mXMLDeclarationBits = 0;

  // Now get our new principal
  nsIScriptSecurityManager *securityManager =
    nsContentUtils::GetSecurityManager();
  if (securityManager) {
    nsCOMPtr<nsIPrincipal> principal;
    nsresult rv =
      securityManager->GetCodebasePrincipal(mDocumentURI,
                                            getter_AddRefs(principal));
    if (NS_SUCCEEDED(rv)) {
      SetPrincipal(principal);
    }
  }
}

nsresult
nsDocument::ResetStylesheetsToURI(nsIURI* aURI)
{
  NS_PRECONDITION(aURI, "Null URI passed to ResetStylesheetsToURI");

  mozAutoDocUpdate upd(this, UPDATE_STYLE, PR_TRUE);
  
  // The stylesheets should forget us
  PRInt32 indx = mStyleSheets.Count();
  while (--indx >= 0) {
    nsIStyleSheet* sheet = mStyleSheets[indx];
    sheet->SetOwningDocument(nsnull);

    PRBool applicable;
    sheet->GetApplicable(applicable);
    if (applicable) {
      RemoveStyleSheetFromStyleSets(sheet);
    }

    // XXX Tell observers?
  }

  indx = mCatalogSheets.Count();
  while (--indx >= 0) {
    nsIStyleSheet* sheet = mCatalogSheets[indx];
    sheet->SetOwningDocument(nsnull);

    PRBool applicable;
    sheet->GetApplicable(applicable);
    if (applicable) {
      for (PRInt32 i = 0, i_end = GetNumberOfShells(); i < i_end; ++i) {
        GetShellAt(i)->StyleSet()->
          RemoveStyleSheet(nsStyleSet::eAgentSheet, sheet);
      }
    }

    // XXX Tell observers?
  }


  // Release all the sheets
  mStyleSheets.Clear();
  // NOTE:  We don't release the catalog sheets.  It doesn't really matter
  // now, but it could in the future -- in which case not releasing them
  // is probably the right thing to do.

  // Now reset our inline style and attribute sheets.
  nsresult rv;
  nsStyleSet::sheetType attrSheetType = GetAttrSheetType();
  if (mAttrStyleSheet) {
    // Remove this sheet from all style sets
    PRInt32 count = GetNumberOfShells();
    for (indx = 0; indx < count; ++indx) {
      GetShellAt(indx)->StyleSet()->
        RemoveStyleSheet(attrSheetType, mAttrStyleSheet);
    }
    rv = mAttrStyleSheet->Reset(aURI);
  } else {
    rv = NS_NewHTMLStyleSheet(getter_AddRefs(mAttrStyleSheet), aURI, this);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // Don't use AddStyleSheet, since it'll put the sheet into style
  // sets in the document level, which is not desirable here.
  mAttrStyleSheet->SetOwningDocument(this);
  
  if (mStyleAttrStyleSheet) {
    // Remove this sheet from all style sets
    PRInt32 count = GetNumberOfShells();
    for (indx = 0; indx < count; ++indx) {
      GetShellAt(indx)->StyleSet()->
        RemoveStyleSheet(nsStyleSet::eStyleAttrSheet, mStyleAttrStyleSheet);
    }
    rv = mStyleAttrStyleSheet->Reset(aURI);
  } else {
    rv = NS_NewHTMLCSSStyleSheet(getter_AddRefs(mStyleAttrStyleSheet), aURI,
                                                this);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // The loop over style sets below will handle putting this sheet
  // into style sets as needed.
  mStyleAttrStyleSheet->SetOwningDocument(this);

  // Now set up our style sets
  PRInt32 count = GetNumberOfShells();
  for (indx = 0; indx < count; ++indx) {
    FillStyleSet(GetShellAt(indx)->StyleSet());
  }

  return rv;
}

nsStyleSet::sheetType
nsDocument::GetAttrSheetType()
{
  return nsStyleSet::ePresHintSheet;
}

void
nsDocument::FillStyleSet(nsStyleSet* aStyleSet)
{
  NS_PRECONDITION(aStyleSet, "Must have a style set");
  NS_PRECONDITION(aStyleSet->SheetCount(nsStyleSet::ePresHintSheet) == 0,
                  "Style set already has a preshint sheet?");
  NS_PRECONDITION(aStyleSet->SheetCount(nsStyleSet::eHTMLPresHintSheet) == 0,
                  "Style set already has a HTML preshint sheet?");
  NS_PRECONDITION(aStyleSet->SheetCount(nsStyleSet::eDocSheet) == 0,
                  "Style set already has document sheets?");
  NS_PRECONDITION(aStyleSet->SheetCount(nsStyleSet::eStyleAttrSheet) == 0,
                  "Style set already has style attr sheets?");
  NS_PRECONDITION(mStyleAttrStyleSheet, "No style attr stylesheet?");
  NS_PRECONDITION(mAttrStyleSheet, "No attr stylesheet?");
  
  aStyleSet->AppendStyleSheet(GetAttrSheetType(), mAttrStyleSheet);

  aStyleSet->AppendStyleSheet(nsStyleSet::eStyleAttrSheet,
                              mStyleAttrStyleSheet);

  PRInt32 i;
  for (i = mStyleSheets.Count() - 1; i >= 0; --i) {
    nsIStyleSheet* sheet = mStyleSheets[i];
    PRBool sheetApplicable;
    sheet->GetApplicable(sheetApplicable);
    if (sheetApplicable) {
      aStyleSet->AddDocStyleSheet(sheet, this);
    }
  }

  for (i = mCatalogSheets.Count() - 1; i >= 0; --i) {
    nsIStyleSheet* sheet = mCatalogSheets[i];
    PRBool sheetApplicable;
    sheet->GetApplicable(sheetApplicable);
    if (sheetApplicable) {
      aStyleSet->AppendStyleSheet(nsStyleSet::eAgentSheet, sheet);
    }
  }
}

nsresult
nsDocument::StartDocumentLoad(const char* aCommand, nsIChannel* aChannel,
                              nsILoadGroup* aLoadGroup,
                              nsISupports* aContainer,
                              nsIStreamListener **aDocListener,
                              PRBool aReset, nsIContentSink* aSink)
{
#ifdef PR_LOGGING
  if (gDocumentLeakPRLog && PR_LOG_TEST(gDocumentLeakPRLog, PR_LOG_DEBUG)) {
    nsCOMPtr<nsIURI> uri;
    aChannel->GetURI(getter_AddRefs(uri));
    nsCAutoString spec;
    if (uri)
      uri->GetSpec(spec);
    PR_LogPrint("DOCUMENT %p StartDocumentLoad %s", this, spec.get());
  }
#endif

  if (aReset) {
    Reset(aChannel, aLoadGroup);
  }

  nsCAutoString contentType;
  if (NS_SUCCEEDED(aChannel->GetContentType(contentType))) {
    // XXX this is only necessary for viewsource:
    nsACString::const_iterator start, end, semicolon;
    contentType.BeginReading(start);
    contentType.EndReading(end);
    semicolon = start;
    FindCharInReadable(';', semicolon, end);
    mContentType = Substring(start, semicolon);
  }

  RetrieveRelevantHeaders(aChannel);

  mChannel = aChannel;

  return NS_OK;
}

void
nsDocument::StopDocumentLoad()
{
  if (mParser) {
    mParser->Terminate();
  }
}

void
nsDocument::SetDocumentURI(nsIURI* aURI)
{
  mDocumentURI = NS_TryToMakeImmutable(aURI);
}

NS_IMETHODIMP
nsDocument::GetLastModified(nsAString& aLastModified)
{
  if (!mLastModified.IsEmpty()) {
    aLastModified.Assign(mLastModified);
  } else {
    // If we for whatever reason failed to find the last modified time
    // (or even the current time), fall back to what NS4.x returned.
    aLastModified.Assign(NS_LITERAL_STRING("01/01/1970 00:00:00"));
  }

  return NS_OK;
}

nsIPrincipal*
nsDocument::GetPrincipal()
{
  return NodePrincipal();
}

void
nsDocument::SetPrincipal(nsIPrincipal *aNewPrincipal)
{
  mNodeInfoManager->SetDocumentPrincipal(aNewPrincipal);
}

NS_IMETHODIMP
nsDocument::GetContentType(nsAString& aContentType)
{
  CopyUTF8toUTF16(mContentType, aContentType);

  return NS_OK;
}

void
nsDocument::SetContentType(const nsAString& aContentType)
{
  NS_ASSERTION(mContentType.IsEmpty() ||
               mContentType.Equals(NS_ConvertUTF16toUTF8(aContentType)),
               "Do you really want to change the content-type?");

  CopyUTF16toUTF8(aContentType, mContentType);
}

NS_IMETHODIMP
nsDocument::GetReferrer(nsAString& aReferrer)
{
  CopyUTF8toUTF16(mReferrer, aReferrer);
  return NS_OK;
}

nsresult
nsDocument::SetBaseURI(nsIURI* aURI)
{
  nsresult rv = NS_OK;

  if (aURI) {
    rv = nsContentUtils::GetSecurityManager()->
      CheckLoadURIWithPrincipal(NodePrincipal(), aURI,
                                nsIScriptSecurityManager::STANDARD);
    if (NS_SUCCEEDED(rv)) {
      mDocumentBaseURI = NS_TryToMakeImmutable(aURI);
    }
  } else {
    mDocumentBaseURI = nsnull;
  }

  return rv;
}

void
nsDocument::GetBaseTarget(nsAString &aBaseTarget) const
{
  aBaseTarget.Assign(mBaseTarget);
}

void
nsDocument::SetBaseTarget(const nsAString &aBaseTarget)
{
  mBaseTarget.Assign(aBaseTarget);
}

void
nsDocument::SetDocumentCharacterSet(const nsACString& aCharSetID)
{
  if (!mCharacterSet.Equals(aCharSetID)) {
    mCharacterSet = aCharSetID;

#ifdef DEBUG
    nsCOMPtr<nsICharsetAlias> calias(do_GetService(NS_CHARSETALIAS_CONTRACTID));
    if (calias) {
      nsCAutoString canonicalName;
      calias->GetPreferred(aCharSetID, canonicalName);
      NS_ASSERTION(canonicalName.Equals(aCharSetID),
                   "charset name must be canonical");
    }
#endif

    PRInt32 n = mCharSetObservers.Count();

    for (PRInt32 i = 0; i < n; i++) {
      nsIObserver* observer =
        NS_STATIC_CAST(nsIObserver *, mCharSetObservers.ElementAt(i));

      observer->Observe(NS_STATIC_CAST(nsIDocument *, this), "charset",
                        NS_ConvertASCIItoUTF16(aCharSetID).get());
    }
  }
}

nsresult
nsDocument::AddCharSetObserver(nsIObserver* aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);

  NS_ENSURE_TRUE(mCharSetObservers.AppendElement(aObserver), NS_ERROR_FAILURE);

  return NS_OK;
}

void
nsDocument::RemoveCharSetObserver(nsIObserver* aObserver)
{
  mCharSetObservers.RemoveElement(aObserver);
}

void
nsDocument::GetHeaderData(nsIAtom* aHeaderField, nsAString& aData) const
{
  aData.Truncate();
  const nsDocHeaderData* data = mHeaderData;
  while (data) {
    if (data->mField == aHeaderField) {
      aData = data->mData;

      break;
    }
    data = data->mNext;
  }
}

void
nsDocument::SetHeaderData(nsIAtom* aHeaderField, const nsAString& aData)
{
  if (!aHeaderField) {
    NS_ERROR("null headerField");
    return;
  }

  if (!mHeaderData) {
    if (!aData.IsEmpty()) { // don't bother storing empty string
      mHeaderData = new nsDocHeaderData(aHeaderField, aData);
    }
  }
  else {
    nsDocHeaderData* data = mHeaderData;
    nsDocHeaderData** lastPtr = &mHeaderData;
    PRBool found = PR_FALSE;
    do {  // look for existing and replace
      if (data->mField == aHeaderField) {
        if (!aData.IsEmpty()) {
          data->mData.Assign(aData);
        }
        else {  // don't store empty string
          *lastPtr = data->mNext;
          data->mNext = nsnull;
          delete data;
        }
        found = PR_TRUE;

        break;
      }
      lastPtr = &(data->mNext);
      data = *lastPtr;
    } while (data);

    if (!aData.IsEmpty() && !found) {
      // didn't find, append
      *lastPtr = new nsDocHeaderData(aHeaderField, aData);
    }
  }

  if (aHeaderField == nsHTMLAtoms::headerContentLanguage) {
    CopyUTF16toUTF8(aData, mContentLanguage);
  }
  
  if (aHeaderField == nsHTMLAtoms::headerDefaultStyle) {
    // switch alternate style sheets based on default
    // XXXldb What if we don't have all the sheets yet?  Should this use
    // the DOM API for preferred stylesheet set that's "coming soon"?
    nsAutoString type;
    nsAutoString title;
    PRInt32 index;

    CSSLoader()->SetPreferredSheet(aData);

    PRInt32 count = mStyleSheets.Count();
    for (index = 0; index < count; index++) {
      nsIStyleSheet* sheet = mStyleSheets[index];
      sheet->GetType(type);
      if (!type.EqualsLiteral("text/html")) {
        sheet->GetTitle(title);
        if (!title.IsEmpty()) {  // if sheet has title
          PRBool enabled =
            (!aData.IsEmpty() &&
             title.Equals(aData, nsCaseInsensitiveStringComparator()));

          sheet->SetEnabled(enabled);
        }
      }
    }
  }

  if (aHeaderField == nsHTMLAtoms::refresh) {
    // We get into this code before we have a script global yet, so get to
    // our container via mDocumentContainer.
    nsCOMPtr<nsIRefreshURI> refresher = do_QueryReferent(mDocumentContainer);
    if (refresher) {
      // Note: using mDocumentURI instead of mBaseURI here, for consistency
      // (used to just use the current URI of our webnavigation, but that
      // should really be the same thing).  Note that this code can run
      // before the current URI of the webnavigation has been updated, so we
      // can't assert equality here.
      refresher->SetupRefreshURIFromHeader(mDocumentURI,
                                           NS_LossyConvertUTF16toASCII(aData));
    }
  }
}

PRBool
nsDocument::TryChannelCharset(nsIChannel *aChannel,
                              PRInt32& aCharsetSource,
                              nsACString& aCharset)
{
  if(kCharsetFromChannel <= aCharsetSource) {
    return PR_TRUE;
  }

  if (aChannel) {
    nsCAutoString charsetVal;
    nsresult rv = aChannel->GetContentCharset(charsetVal);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsICharsetAlias> calias(do_GetService(NS_CHARSETALIAS_CONTRACTID));
      if (calias) {
        nsCAutoString preferred;
        rv = calias->GetPreferred(charsetVal,
                                  preferred);
        if(NS_SUCCEEDED(rv)) {
          aCharset = preferred;
          aCharsetSource = kCharsetFromChannel;
          return PR_TRUE;
        }
      }
    }
  }
  return PR_FALSE;
}

nsresult
nsDocument::CreateShell(nsPresContext* aContext, nsIViewManager* aViewManager,
                        nsStyleSet* aStyleSet,
                        nsIPresShell** aInstancePtrResult)
{
  // Don't add anything here.  Add it to |doCreateShell| instead.
  // This exists so that subclasses can pass other values for the 4th
  // parameter some of the time.
  return doCreateShell(aContext, aViewManager, aStyleSet,
                       eCompatibility_FullStandards, aInstancePtrResult);
}

nsresult
nsDocument::doCreateShell(nsPresContext* aContext,
                          nsIViewManager* aViewManager, nsStyleSet* aStyleSet,
                          nsCompatibility aCompatMode,
                          nsIPresShell** aInstancePtrResult)
{
  *aInstancePtrResult = nsnull;

  NS_ENSURE_FALSE(mShellsAreHidden, NS_ERROR_FAILURE);

  FillStyleSet(aStyleSet);
  
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = NS_NewPresShell(getter_AddRefs(shell));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = shell->Init(this, aContext, aViewManager, aStyleSet, aCompatMode);
  NS_ENSURE_SUCCESS(rv, rv);

  // Note: we don't hold a ref to the shell (it holds a ref to us)
  mPresShells.AppendElement(shell);
  shell.swap(*aInstancePtrResult);

  return NS_OK;
}

PRBool
nsDocument::DeleteShell(nsIPresShell* aShell)
{
  return mPresShells.RemoveElement(aShell);
}

PRUint32
nsDocument::GetNumberOfShells() const
{
  return mShellsAreHidden ? 0 : mPresShells.Count();
}

nsIPresShell *
nsDocument::GetShellAt(PRUint32 aIndex) const
{
  return mShellsAreHidden ? nsnull :
         NS_STATIC_CAST(nsIPresShell*, mPresShells.SafeElementAt(aIndex));
}

void
nsDocument::SetShellsHidden(PRBool aHide)
{
  mShellsAreHidden = aHide;
}

PR_STATIC_CALLBACK(void)
SubDocClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  SubDocMapEntry *e = NS_STATIC_CAST(SubDocMapEntry *, entry);

  NS_RELEASE(e->mKey);
  NS_IF_RELEASE(e->mSubDocument);
}

PR_STATIC_CALLBACK(PRBool)
SubDocInitEntry(PLDHashTable *table, PLDHashEntryHdr *entry, const void *key)
{
  SubDocMapEntry *e =
    NS_CONST_CAST(SubDocMapEntry *,
                  NS_STATIC_CAST(const SubDocMapEntry *, entry));

  e->mKey = NS_CONST_CAST(nsIContent *,
                          NS_STATIC_CAST(const nsIContent *, key));
  NS_ADDREF(e->mKey);

  e->mSubDocument = nsnull;
  return PR_TRUE;
}

nsresult
nsDocument::SetSubDocumentFor(nsIContent *aContent, nsIDocument* aSubDoc)
{
  NS_ENSURE_TRUE(aContent, NS_ERROR_UNEXPECTED);

  if (!aSubDoc) {
    // aSubDoc is nsnull, remove the mapping

    if (mSubDocuments) {
      SubDocMapEntry *entry =
        NS_STATIC_CAST(SubDocMapEntry*,
                       PL_DHashTableOperate(mSubDocuments, aContent,
                                            PL_DHASH_LOOKUP));

      if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
        entry->mSubDocument->SetParentDocument(nsnull);

        PL_DHashTableRawRemove(mSubDocuments, entry);
      }
    }
  } else {
    if (!mSubDocuments) {
      // Create a new hashtable

      static PLDHashTableOps hash_table_ops =
      {
        PL_DHashAllocTable,
        PL_DHashFreeTable,
        PL_DHashGetKeyStub,
        PL_DHashVoidPtrKeyStub,
        PL_DHashMatchEntryStub,
        PL_DHashMoveEntryStub,
        SubDocClearEntry,
        PL_DHashFinalizeStub,
        SubDocInitEntry
      };

      mSubDocuments = PL_NewDHashTable(&hash_table_ops, nsnull,
                                       sizeof(SubDocMapEntry), 16);
      if (!mSubDocuments) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    // Add a mapping to the hash table
    SubDocMapEntry *entry =
      NS_STATIC_CAST(SubDocMapEntry*,
                     PL_DHashTableOperate(mSubDocuments, aContent,
                                          PL_DHASH_ADD));

    if (!entry) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (entry->mSubDocument) {
      entry->mSubDocument->SetParentDocument(nsnull);

      // Release the old sub document
      NS_RELEASE(entry->mSubDocument);
    }

    entry->mSubDocument = aSubDoc;
    NS_ADDREF(entry->mSubDocument);

    aSubDoc->SetParentDocument(this);
  }

  return NS_OK;
}

nsIDocument*
nsDocument::GetSubDocumentFor(nsIContent *aContent) const
{
  if (mSubDocuments) {
    SubDocMapEntry *entry =
      NS_STATIC_CAST(SubDocMapEntry*,
                     PL_DHashTableOperate(mSubDocuments, aContent,
                                          PL_DHASH_LOOKUP));

    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      return entry->mSubDocument;
    }
  }

  return nsnull;
}

PR_STATIC_CALLBACK(PLDHashOperator)
FindContentEnumerator(PLDHashTable *table, PLDHashEntryHdr *hdr,
                      PRUint32 number, void *arg)
{
  SubDocMapEntry *entry = NS_STATIC_CAST(SubDocMapEntry*, hdr);
  FindContentData *data = NS_STATIC_CAST(FindContentData*, arg);

  if (entry->mSubDocument == data->mSubDocument) {
    data->mResult = entry->mKey;

    return PL_DHASH_STOP;
  }

  return PL_DHASH_NEXT;
}

nsIContent*
nsDocument::FindContentForSubDocument(nsIDocument *aDocument) const
{
  NS_ENSURE_TRUE(aDocument, nsnull);

  if (!mSubDocuments) {
    return nsnull;
  }

  FindContentData data(aDocument);
  PL_DHashTableEnumerate(mSubDocuments, FindContentEnumerator, &data);

  return data.mResult;
}

PRBool
nsDocument::IsNodeOfType(PRUint32 aFlags) const
{
    return !(aFlags & ~eDOCUMENT);
}

nsIContent *
nsDocument::GetChildAt(PRUint32 aIndex) const
{
  return mChildren.GetSafeChildAt(aIndex);
}

PRInt32
nsDocument::IndexOf(nsINode* aPossibleChild) const
{
  return mChildren.IndexOfChild(aPossibleChild);
}

PRUint32
nsDocument::GetChildCount() const
{
  return mChildren.ChildCount();
}

nsresult
nsDocument::InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                          PRBool aNotify)
{
  if (aKid->IsNodeOfType(nsINode::eELEMENT)) {
    if (mRootContent) {
      NS_ERROR("Inserting element child when we already have one");
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }

    mRootContent = aKid;
  }
  
  nsresult rv = nsGenericElement::doInsertChildAt(aKid, aIndex, aNotify,
                                                  nsnull, this, mChildren);

  if (NS_FAILED(rv) && mRootContent == aKid) {
    PRInt32 kidIndex = mChildren.IndexOfChild(aKid);
    NS_ASSERTION(kidIndex == -1,
                 "Error result and still have same root content but it's in "
                 "our child list?");
    // Check to make sure that we're keeping mRootContent in sync with our
    // child list... but really, if kidIndex != -1 we have major problems
    // coming up; hence the assert above.  This check is just a feeble attempt
    // to not die due to mRootContent being bogus.
    if (kidIndex == -1) {
      mRootContent = nsnull;
    }
  }

#ifdef DEBUG
  VerifyRootContentState();
#endif

  return rv;
}

nsresult
nsDocument::AppendChildTo(nsIContent* aKid, PRBool aNotify)
{
  // Make sure to _not_ call the subclass InsertChildAt here.  If
  // subclasses wanted to hook into this stuff, they would have
  // overridden AppendChildTo.
  // XXXbz maybe this should just be a non-virtual method on nsINode?
  // Feels that way to me...
  return nsDocument::InsertChildAt(aKid, GetChildCount(), aNotify);
}

nsresult
nsDocument::RemoveChildAt(PRUint32 aIndex, PRBool aNotify)
{
  nsCOMPtr<nsIContent> oldKid = GetChildAt(aIndex);
  nsresult rv = NS_OK;
  if (oldKid) {
    if (oldKid == mRootContent) {
      NS_ASSERTION(oldKid->IsNodeOfType(nsINode::eELEMENT),
                   "Non-element root content?");
      // Destroy the link map up front and null out mRootContent before we mess
      // with the child list.  Hopefully no one in doRemoveChildAt will compare
      // the content being removed to GetRootContent()....  Need to do this
      // before calling doRemoveChildAt because DOM events might fire while
      // we're inside the doInsertChildAt call and want to set a new
      // mRootContent; if they do that, setting mRootContent after the
      // doRemoveChildAt call would clobber state.  If we ever fix the issue of
      // DOM events firing at inconvenient times, consider changing the order
      // here.  Just make sure we DestroyLinkMap() before unbinding the
      // content.
      DestroyLinkMap();
      mRootContent = nsnull;
    }
    
    rv = nsGenericElement::doRemoveChildAt(aIndex, aNotify, oldKid,
                                           nsnull, this, mChildren);
    if (NS_FAILED(rv) && mChildren.IndexOfChild(oldKid) != -1) {
      mRootContent = oldKid;
    }
  }

#ifdef DEBUG
  VerifyRootContentState();
#endif

  return rv;
}

#ifdef DEBUG
void
nsDocument::VerifyRootContentState()
{
  nsIContent* elementChild = nsnull;
  for (PRUint32 i = 0; i < GetChildCount(); ++i) {
    nsIContent* kid = GetChildAt(i);
    NS_ASSERTION(kid, "Must have kid here");

    if (kid->IsNodeOfType(nsINode::eELEMENT)) {
      NS_ASSERTION(!elementChild, "Multiple element kids?");
      elementChild = kid;
    }
  }

  NS_ASSERTION(mRootContent == elementChild, "Incorrect mRootContent");
}
#endif // DEBUG

PRInt32
nsDocument::GetNumberOfStyleSheets() const
{
  return mStyleSheets.Count();
}

nsIStyleSheet*
nsDocument::GetStyleSheetAt(PRInt32 aIndex) const
{
  NS_ENSURE_TRUE(0 <= aIndex && aIndex < mStyleSheets.Count(), nsnull);
  return mStyleSheets[aIndex];
}

PRInt32
nsDocument::GetIndexOfStyleSheet(nsIStyleSheet* aSheet) const
{
  return mStyleSheets.IndexOf(aSheet);
}

void
nsDocument::AddStyleSheetToStyleSets(nsIStyleSheet* aSheet)
{
  PRInt32 count = GetNumberOfShells();
  PRInt32 indx;
  for (indx = 0; indx < count; ++indx) {
    GetShellAt(indx)->StyleSet()->AddDocStyleSheet(aSheet, this);
  }
}

void
nsDocument::AddStyleSheet(nsIStyleSheet* aSheet)
{
  NS_PRECONDITION(aSheet, "null arg");
  mStyleSheets.AppendObject(aSheet);
  aSheet->SetOwningDocument(this);

  PRBool applicable;
  aSheet->GetApplicable(applicable);

  if (applicable) {
    AddStyleSheetToStyleSets(aSheet);
  }

  NS_DOCUMENT_NOTIFY_OBSERVERS(StyleSheetAdded, (this, aSheet, PR_TRUE));
}

void
nsDocument::RemoveStyleSheetFromStyleSets(nsIStyleSheet* aSheet)
{
  for (PRInt32 i = 0, i_end = GetNumberOfShells(); i < i_end; ++i)
    GetShellAt(i)->StyleSet()->
      RemoveStyleSheet(nsStyleSet::eDocSheet, aSheet);
}

void
nsDocument::RemoveStyleSheet(nsIStyleSheet* aSheet)
{
  NS_PRECONDITION(aSheet, "null arg");
  nsCOMPtr<nsIStyleSheet> sheet = aSheet; // hold ref so it won't die too soon

  if (!mStyleSheets.RemoveObject(aSheet)) {
    NS_NOTREACHED("stylesheet not found");
    return;
  }

  if (!mIsGoingAway) {
    PRBool applicable = PR_TRUE;
    aSheet->GetApplicable(applicable);
    if (applicable) {
      RemoveStyleSheetFromStyleSets(aSheet);
    }

    NS_DOCUMENT_NOTIFY_OBSERVERS(StyleSheetRemoved, (this, aSheet, PR_TRUE));
  }

  aSheet->SetOwningDocument(nsnull);
}

void
nsDocument::UpdateStyleSheets(nsCOMArray<nsIStyleSheet>& aOldSheets,
                              nsCOMArray<nsIStyleSheet>& aNewSheets)
{
  NS_DOCUMENT_NOTIFY_OBSERVERS(BeginUpdate, (this, UPDATE_STYLE));

  // XXX Need to set the sheet on the ownernode, if any
  NS_PRECONDITION(aOldSheets.Count() == aNewSheets.Count(),
                  "The lists must be the same length!");
  PRInt32 count = aOldSheets.Count();

  nsCOMPtr<nsIStyleSheet> oldSheet;
  PRInt32 i;
  for (i = 0; i < count; ++i) {
    oldSheet = aOldSheets[i];

    // First remove the old sheet.
    NS_ASSERTION(oldSheet, "None of the old sheets should be null");
    PRInt32 oldIndex = mStyleSheets.IndexOf(oldSheet);
    RemoveStyleSheet(oldSheet);  // This does the right notifications

    // Now put the new one in its place.  If it's null, just ignore it.
    nsIStyleSheet* newSheet = aNewSheets[i];
    if (newSheet) {
      mStyleSheets.InsertObjectAt(newSheet, oldIndex);
      newSheet->SetOwningDocument(this);
      PRBool applicable = PR_TRUE;
      newSheet->GetApplicable(applicable);
      if (applicable) {
        AddStyleSheetToStyleSets(newSheet);
      }

      NS_DOCUMENT_NOTIFY_OBSERVERS(StyleSheetAdded, (this, newSheet, PR_TRUE));
    }
  }

  NS_DOCUMENT_NOTIFY_OBSERVERS(EndUpdate, (this, UPDATE_STYLE));
}

void
nsDocument::InsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex)
{
  NS_PRECONDITION(aSheet, "null ptr");
  mStyleSheets.InsertObjectAt(aSheet, aIndex);

  aSheet->SetOwningDocument(this);

  PRBool applicable;
  aSheet->GetApplicable(applicable);

  if (applicable) {
    AddStyleSheetToStyleSets(aSheet);
  }

  NS_DOCUMENT_NOTIFY_OBSERVERS(StyleSheetAdded, (this, aSheet, PR_TRUE));
}


void
nsDocument::SetStyleSheetApplicableState(nsIStyleSheet* aSheet,
                                         PRBool aApplicable)
{
  NS_PRECONDITION(aSheet, "null arg");

  // If we're actually in the document style sheet list
  if (-1 != mStyleSheets.IndexOf(aSheet)) {
    if (aApplicable) {
      AddStyleSheetToStyleSets(aSheet);
    } else {
      RemoveStyleSheetFromStyleSets(aSheet);
    }
  }

  // We have to always notify, since this will be called for sheets
  // that are children of sheets in our style set, as well as some
  // sheets for nsHTMLEditor.

  NS_DOCUMENT_NOTIFY_OBSERVERS(StyleSheetApplicableStateChanged,
                               (this, aSheet, aApplicable));
}

// These three functions are a lot like the implementation of the
// corresponding API for regular stylesheets.

PRInt32
nsDocument::GetNumberOfCatalogStyleSheets() const
{
  return mCatalogSheets.Count();
}

nsIStyleSheet*
nsDocument::GetCatalogStyleSheetAt(PRInt32 aIndex) const
{
  NS_ENSURE_TRUE(0 <= aIndex && aIndex < mCatalogSheets.Count(), nsnull);
  return mCatalogSheets[aIndex];
}

void
nsDocument::AddCatalogStyleSheet(nsIStyleSheet* aSheet)
{
  mCatalogSheets.AppendObject(aSheet);
  aSheet->SetOwningDocument(this);

  PRBool applicable;
  aSheet->GetApplicable(applicable);
                                                                                
  if (applicable) {
    // This is like |AddStyleSheetToStyleSets|, but for an agent sheet.
    for (PRInt32 i = 0, i_end = GetNumberOfShells(); i < i_end; ++i)
      GetShellAt(i)->StyleSet()->
        AppendStyleSheet(nsStyleSet::eAgentSheet, aSheet);
  }
                                                                                
  NS_DOCUMENT_NOTIFY_OBSERVERS(StyleSheetAdded, (this, aSheet, PR_FALSE));
}

void
nsDocument::EnsureCatalogStyleSheet(const char *aStyleSheetURI)
{
  nsICSSLoader* cssLoader = CSSLoader();
  PRBool enabled;
  if (NS_SUCCEEDED(cssLoader->GetEnabled(&enabled)) && enabled) {
    PRInt32 sheetCount = GetNumberOfCatalogStyleSheets();
    for (PRInt32 i = 0; i < sheetCount; i++) {
      nsIStyleSheet* sheet = GetCatalogStyleSheetAt(i);
      NS_ASSERTION(sheet, "unexpected null stylesheet in the document");
      if (sheet) {
        nsCOMPtr<nsIURI> uri;
        sheet->GetSheetURI(getter_AddRefs(uri));
        nsCAutoString uriStr;
        uri->GetSpec(uriStr);
        if (uriStr.Equals(aStyleSheetURI))
          return;
      }
    }

    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), aStyleSheetURI);
    if (uri) {
      nsCOMPtr<nsICSSStyleSheet> sheet;
      cssLoader->LoadSheetSync(uri, getter_AddRefs(sheet));
      if (sheet) {
        BeginUpdate(UPDATE_STYLE);
        AddCatalogStyleSheet(sheet);
        EndUpdate(UPDATE_STYLE);
      }
    }
  }
}

nsIScriptGlobalObject*
nsDocument::GetScriptGlobalObject() const
{
   // If we're going away, we've already released the reference to our
   // ScriptGlobalObject.  We can, however, try to obtain it for the
   // caller through our docshell.

   if (mIsGoingAway) {
     nsCOMPtr<nsIInterfaceRequestor> requestor =
       do_QueryReferent(mDocumentContainer);
     if (requestor) {
       nsCOMPtr<nsIScriptGlobalObject> globalObject = do_GetInterface(requestor);
       return globalObject;
     }
   }

   return mScriptGlobalObject;
}

nsIScriptGlobalObject*
nsDocument::GetScopeObject()
{
  nsCOMPtr<nsIScriptGlobalObject> scope(do_QueryReferent(mScopeObject));
  return scope;
}

void
nsDocument::SetScriptGlobalObject(nsIScriptGlobalObject *aScriptGlobalObject)
{
#ifdef DEBUG
  {
    nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(aScriptGlobalObject));

    NS_ASSERTION(!win || win->IsInnerWindow(),
                 "Script global object must be an inner window!");
  }
#endif

  if (mScriptGlobalObject && !aScriptGlobalObject) {
    // We're detaching from the window.  We need to grab a pointer to
    // our layout history state now.
    mLayoutHistoryState = GetLayoutHistoryState();

    // Also make sure to remove our onload blocker now if we haven't done it yet
    if (mOnloadBlockCount != 0) {
      nsCOMPtr<nsILoadGroup> loadGroup = GetDocumentLoadGroup();
      if (loadGroup) {
        loadGroup->RemoveRequest(mOnloadBlocker, nsnull, NS_OK);
      }
    }
  }

  mScriptGlobalObject = aScriptGlobalObject;

  if (aScriptGlobalObject) {
    // Go back to using the docshell for the layout history state
    mLayoutHistoryState = nsnull;
    mScopeObject = do_GetWeakReference(aScriptGlobalObject);
  }
}

nsPIDOMWindow *
nsDocument::GetWindow()
{
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(GetScriptGlobalObject()));

  if (!win) {
    return nsnull;
  }

  return win->GetOuterWindow();
}

nsPIDOMWindow *
nsDocument::GetInnerWindow()
{
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(GetScriptGlobalObject()));

  return win;
}

nsIScriptLoader *
nsDocument::GetScriptLoader()
{
  if (!mScriptLoader) {
    mScriptLoader = new nsScriptLoader();
    if (!mScriptLoader) {
      return nsnull;
    }
    mScriptLoader->Init(this);
  }

  return mScriptLoader;
}

// Note: We don't hold a reference to the document observer; we assume
// that it has a live reference to the document.
void
nsDocument::AddObserver(nsIDocumentObserver* aObserver)
{
  // XXX Make sure the observer isn't already in the list
  if (mObservers.IndexOf(aObserver) == -1) {
    mObservers.AppendElement(aObserver);
  }
}

PRBool
nsDocument::RemoveObserver(nsIDocumentObserver* aObserver)
{
  // If we're in the process of destroying the document (and we're
  // informing the observers of the destruction), don't remove the
  // observers from the list. This is not a big deal, since we
  // don't hold a live reference to the observers.
  if (!mInDestructor) {
    return mObservers.RemoveElement(aObserver);
  }

  return (mObservers.IndexOf(aObserver) != -1);
}

void
nsDocument::CopyObserversTo(nsCOMArray<nsIDocumentObserver>& aDestination)
{
  PRInt32 count = mObservers.Count();
  aDestination.SetCapacity(count);
  // If we run out of memory, we just won't notify some of the observers.
  for (PRInt32 i = 0; i < count; ++i) {
    aDestination.AppendObject(
      NS_STATIC_CAST(nsIDocumentObserver*,mObservers[i]));
  }
}


void
nsDocument::BeginUpdate(nsUpdateType aUpdateType)
{
  NS_DOCUMENT_NOTIFY_OBSERVERS(BeginUpdate, (this, aUpdateType));
}

void
nsDocument::EndUpdate(nsUpdateType aUpdateType)
{
  NS_DOCUMENT_NOTIFY_OBSERVERS(EndUpdate, (this, aUpdateType));
}

void
nsDocument::BeginLoad()
{
  // Block onload here to prevent having to deal with blocking and
  // unblocking it while we know the document is loading.
  BlockOnload();

  NS_DOCUMENT_NOTIFY_OBSERVERS(BeginLoad, (this));
}

static void
GetDocumentFromDocShellTreeItem(nsIDocShellTreeItem *aDocShell,
                                nsIDocument **aDocument)
{
  *aDocument = nsnull;

  nsCOMPtr<nsIDOMWindow> window(do_GetInterface(aDocShell));

  if (window) {
    nsCOMPtr<nsIDOMDocument> dom_doc;
    window->GetDocument(getter_AddRefs(dom_doc));

    if (dom_doc) {
      CallQueryInterface(dom_doc, aDocument);
    }
  }
}

void
nsDocument::DispatchContentLoadedEvents()
{
  // Fire a DOM event notifying listeners that this document has been
  // loaded (excluding images and other loads initiated by this
  // document).
  nsContentUtils::DispatchTrustedEvent(this, NS_STATIC_CAST(nsIDocument*, this),
                                       NS_LITERAL_STRING("DOMContentLoaded"),
                                       PR_TRUE, PR_TRUE);

  // If this document is a [i]frame, fire a DOMFrameContentLoaded
  // event on all parent documents notifying that the HTML (excluding
  // other external files such as images and stylesheets) in a frame
  // has finished loading.

  nsCOMPtr<nsIDocShellTreeItem> docShellParent;

  // target_frame is the [i]frame element that will be used as the
  // target for the event. It's the [i]frame whose content is done
  // loading.
  nsCOMPtr<nsIDOMEventTarget> target_frame;

  nsPIDOMWindow *win = GetWindow();
  if (win) {
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem =
      do_QueryInterface(win->GetDocShell());

    if (docShellAsItem) {
      docShellAsItem->GetSameTypeParent(getter_AddRefs(docShellParent));

      nsCOMPtr<nsIDocument> parent_doc;

      GetDocumentFromDocShellTreeItem(docShellParent,
                                      getter_AddRefs(parent_doc));

      if (parent_doc) {
        target_frame = do_QueryInterface(parent_doc->FindContentForSubDocument(this));
      }
    }
  }

  if (target_frame) {
    while (docShellParent) {
      nsCOMPtr<nsIDocument> ancestor_doc;

      GetDocumentFromDocShellTreeItem(docShellParent,
                                      getter_AddRefs(ancestor_doc));

      if (!ancestor_doc) {
        break;
      }

      nsCOMPtr<nsIDOMDocumentEvent> document_event =
        do_QueryInterface(ancestor_doc);

      nsCOMPtr<nsIDOMEvent> event;
      nsCOMPtr<nsIPrivateDOMEvent> privateEvent;
      if (document_event) {
        document_event->CreateEvent(NS_LITERAL_STRING("Events"),
                                    getter_AddRefs(event));

        privateEvent = do_QueryInterface(event);
      }

      if (event && privateEvent) {
        event->InitEvent(NS_LITERAL_STRING("DOMFrameContentLoaded"), PR_TRUE,
                         PR_TRUE);

        privateEvent->SetTarget(target_frame);
        privateEvent->SetTrusted(PR_TRUE);

        // To dispatch this event we must manually call
        // nsEventDispatcher::Dispatch() on the ancestor document since the
        // target is not in the same document, so the event would never reach
        // the ancestor document if we used the normal event
        // dispatching code.

        nsEvent* innerEvent;
        privateEvent->GetInternalNSEvent(&innerEvent);
        if (innerEvent) {
          nsEventStatus status = nsEventStatus_eIgnore;

          nsIPresShell *shell = ancestor_doc->GetShellAt(0);
          if (shell) {
            nsCOMPtr<nsPresContext> context = shell->GetPresContext();

            if (context) {
              nsEventDispatcher::Dispatch(ancestor_doc, context, innerEvent,
                                          event, &status);
            }
          }
        }
      }

      nsCOMPtr<nsIDocShellTreeItem> tmp(docShellParent);
      tmp->GetSameTypeParent(getter_AddRefs(docShellParent));
    }
  }
}

void
nsDocument::EndLoad()
{
  // Drop the ref to our parser, if any
  mParser = nsnull;
  
  NS_DOCUMENT_NOTIFY_OBSERVERS(EndLoad, (this));

  DispatchContentLoadedEvents();
  UnblockOnload(PR_TRUE);
}

void
nsDocument::CharacterDataChanged(nsIContent* aContent, PRBool aAppend)
{
  NS_ABORT_IF_FALSE(aContent, "Null content!");

  NS_DOCUMENT_NOTIFY_OBSERVERS(CharacterDataChanged, (this, aContent, aAppend));
}

void
nsDocument::ContentStatesChanged(nsIContent* aContent1, nsIContent* aContent2,
                                 PRInt32 aStateMask)
{
  NS_DOCUMENT_NOTIFY_OBSERVERS(ContentStatesChanged,
                               (this, aContent1, aContent2, aStateMask));
}


void
nsDocument::ContentAppended(nsIContent* aContainer,
                            PRInt32 aNewIndexInContainer)
{
  NS_ABORT_IF_FALSE(aContainer, "Null container!");

  // XXXdwh There is a hacky ordering dependency between the binding
  // manager and the frame constructor that forces us to walk the
  // observer list in a forward order
  // XXXldb So one should notify the other rather than both being
  // registered.
  nsCOMArray<nsIDocumentObserver> observers;
  CopyObserversTo(observers);
  for (PRInt32 i = 0, i_end = observers.Count(); i < i_end; ++i) {
    observers[i]->ContentAppended(this, aContainer, aNewIndexInContainer);
  }
}

void
nsDocument::ContentInserted(nsIContent* aContainer, nsIContent* aChild,
                            PRInt32 aIndexInContainer)
{
  NS_ABORT_IF_FALSE(aChild, "Null child!");

  // XXXdwh There is a hacky ordering dependency between the binding manager
  // and the frame constructor that forces us to walk the observer list
  // in a forward order
  // XXXldb So one should notify the other rather than both being
  // registered.
  nsCOMArray<nsIDocumentObserver> observers;
  CopyObserversTo(observers);
  for (PRInt32 i = 0, i_end = observers.Count(); i < i_end; ++i) {
    observers[i]->ContentInserted(this, aContainer, aChild, aIndexInContainer);
  }
}

void
nsDocument::ContentRemoved(nsIContent* aContainer, nsIContent* aChild,
                           PRInt32 aIndexInContainer)
{
  NS_ABORT_IF_FALSE(aChild, "Null child!");

  // XXXdwh There is a hacky ordering dependency between the binding
  // manager and the frame constructor that forces us to walk the
  // observer list in a reverse order
  // XXXldb So one should notify the other rather than both being
  // registered.
  nsCOMArray<nsIDocumentObserver> observers;
  CopyObserversTo(observers);
  for (PRInt32 i = observers.Count() - 1; i >= 0; --i) {
    observers[i]->ContentRemoved(this, aContainer, aChild, aIndexInContainer);
  }
}

void
nsDocument::AttributeWillChange(nsIContent* aChild, PRInt32 aNameSpaceID,
                                nsIAtom* aAttribute)
{
  NS_ASSERTION(aChild, "Null child!");
}

void
nsDocument::AttributeChanged(nsIContent* aChild, PRInt32 aNameSpaceID,
                             nsIAtom* aAttribute, PRInt32 aModType)
{
  NS_ABORT_IF_FALSE(aChild, "Null child!");
  
  NS_DOCUMENT_NOTIFY_OBSERVERS(AttributeChanged, (this, aChild, aNameSpaceID,
                                                  aAttribute, aModType));
}


void
nsDocument::StyleRuleChanged(nsIStyleSheet* aStyleSheet,
                             nsIStyleRule* aOldStyleRule,
                             nsIStyleRule* aNewStyleRule)
{
  NS_DOCUMENT_NOTIFY_OBSERVERS(StyleRuleChanged,
                               (this, aStyleSheet,
                                aOldStyleRule, aNewStyleRule));
}

void
nsDocument::StyleRuleAdded(nsIStyleSheet* aStyleSheet,
                           nsIStyleRule* aStyleRule)
{
  NS_DOCUMENT_NOTIFY_OBSERVERS(StyleRuleAdded,
                               (this, aStyleSheet, aStyleRule));
}

void
nsDocument::StyleRuleRemoved(nsIStyleSheet* aStyleSheet,
                             nsIStyleRule* aStyleRule)
{
  NS_DOCUMENT_NOTIFY_OBSERVERS(StyleRuleRemoved,
                               (this, aStyleSheet, aStyleRule));
}


//
// nsIDOMDocument interface
//
NS_IMETHODIMP
nsDocument::GetDoctype(nsIDOMDocumentType** aDoctype)
{
  NS_ENSURE_ARG_POINTER(aDoctype);

  *aDoctype = nsnull;
  PRInt32 i, count;
  count = mChildren.ChildCount();
  nsCOMPtr<nsIDOMNode> rootContentNode(do_QueryInterface(mRootContent) );
  nsCOMPtr<nsIDOMNode> node;

  for (i = 0; i < count; i++) {
    node = do_QueryInterface(mChildren.ChildAt(i));

    NS_ASSERTION(node, "null element of mChildren");

    // doctype can't be after the root
    // XXX Do we really want to enforce this when we don't enforce
    // anything else?
    if (node == rootContentNode)
      return NS_OK;

    if (node) {
      PRUint16 nodeType;

      node->GetNodeType(&nodeType);

      if (nodeType == nsIDOMNode::DOCUMENT_TYPE_NODE) {
        return CallQueryInterface(node, aDoctype);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetImplementation(nsIDOMDOMImplementation** aImplementation)
{
  // For now, create a new implementation every time. This shouldn't
  // be a high bandwidth operation
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "about:blank");
  NS_ENSURE_TRUE(uri, NS_ERROR_OUT_OF_MEMORY);
  
  *aImplementation = new nsDOMImplementation(uri, uri, NodePrincipal());
  if (!*aImplementation) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aImplementation);

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetDocumentElement(nsIDOMElement** aDocumentElement)
{
  NS_ENSURE_ARG_POINTER(aDocumentElement);

  nsresult rv = NS_OK;

  if (mRootContent) {
    rv = CallQueryInterface(mRootContent, aDocumentElement);
    NS_ASSERTION(NS_OK == rv, "Must be a DOM Element");
  } else {
    *aDocumentElement = nsnull;
  }

  return rv;
}

NS_IMETHODIMP
nsDocument::CreateElement(const nsAString& aTagName,
                          nsIDOMElement** aReturn)
{
  *aReturn = nsnull;

  nsresult rv = nsContentUtils::CheckQName(aTagName, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(IsCaseSensitive(),
               "nsDocument::CreateElement() called on document that is not "
               "case sensitive. Fix caller, or fix "
               "nsDocument::CreateElement()!");

  nsCOMPtr<nsIAtom> name = do_GetAtom(aTagName);

  nsCOMPtr<nsIContent> content;
  rv = CreateElem(name, nsnull, GetDefaultNamespaceID(), PR_TRUE,
                  getter_AddRefs(content));
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(content, aReturn);
}

NS_IMETHODIMP
nsDocument::CreateElementNS(const nsAString& aNamespaceURI,
                            const nsAString& aQualifiedName,
                            nsIDOMElement** aReturn)
{
  *aReturn = nsnull;

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsresult rv = nsContentUtils::GetNodeInfoFromQName(aNamespaceURI,
                                                     aQualifiedName,
                                                     mNodeInfoManager,
                                                     getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> content;
  rv = CreateElement(nodeInfo, nodeInfo->NamespaceID(),
                     getter_AddRefs(content));
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(content, aReturn);
}

NS_IMETHODIMP
nsDocument::CreateTextNode(const nsAString& aData, nsIDOMText** aReturn)
{
  *aReturn = nsnull;

  nsCOMPtr<nsITextContent> text;
  nsresult rv = NS_NewTextNode(getter_AddRefs(text), mNodeInfoManager);

  if (NS_SUCCEEDED(rv)) {
    rv = CallQueryInterface(text, aReturn);
    (*aReturn)->AppendData(aData);
  }

  return rv;
}

NS_IMETHODIMP
nsDocument::CreateDocumentFragment(nsIDOMDocumentFragment** aReturn)
{
  return NS_NewDocumentFragment(aReturn, mNodeInfoManager);
}

NS_IMETHODIMP
nsDocument::CreateComment(const nsAString& aData, nsIDOMComment** aReturn)
{
  *aReturn = nsnull;

  nsCOMPtr<nsIContent> comment;
  nsresult rv = NS_NewCommentNode(getter_AddRefs(comment), mNodeInfoManager);

  if (NS_SUCCEEDED(rv)) {
    rv = CallQueryInterface(comment, aReturn);
    (*aReturn)->AppendData(aData);
  }

  return rv;
}

NS_IMETHODIMP
nsDocument::CreateCDATASection(const nsAString& aData,
                               nsIDOMCDATASection** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsReadingIterator<PRUnichar> begin;
  nsReadingIterator<PRUnichar> end;
  aData.BeginReading(begin);
  aData.EndReading(end);
  if (FindInReadable(NS_LITERAL_STRING("]]>"),begin,end))
    return NS_ERROR_DOM_INVALID_CHARACTER_ERR;

  nsCOMPtr<nsIContent> content;
  nsresult rv = NS_NewXMLCDATASection(getter_AddRefs(content),
                                      mNodeInfoManager);

  if (NS_SUCCEEDED(rv)) {
    rv = CallQueryInterface(content, aReturn);
    (*aReturn)->AppendData(aData);
  }

  return rv;
}

NS_IMETHODIMP
nsDocument::CreateProcessingInstruction(const nsAString& aTarget,
                                        const nsAString& aData,
                                        nsIDOMProcessingInstruction** aReturn)
{
  *aReturn = nsnull;

  nsresult rv = nsContentUtils::CheckQName(aTarget, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> content;
  rv = NS_NewXMLProcessingInstruction(getter_AddRefs(content),
                                      mNodeInfoManager, aTarget, aData);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return CallQueryInterface(content, aReturn);
}

NS_IMETHODIMP
nsDocument::CreateAttribute(const nsAString& aName,
                            nsIDOMAttr** aReturn)
{
  *aReturn = nsnull;
  NS_ENSURE_TRUE(mNodeInfoManager, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = nsContentUtils::CheckQName(aName, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString value;
  nsDOMAttribute* attribute;

  nsCOMPtr<nsINodeInfo> nodeInfo;
  rv = mNodeInfoManager->GetNodeInfo(aName, nsnull, kNameSpaceID_None,
                                     getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  attribute = new nsDOMAttribute(nsnull, nodeInfo, value);
  NS_ENSURE_TRUE(attribute, NS_ERROR_OUT_OF_MEMORY);

  return CallQueryInterface(attribute, aReturn);
}

NS_IMETHODIMP
nsDocument::CreateAttributeNS(const nsAString & aNamespaceURI,
                              const nsAString & aQualifiedName,
                              nsIDOMAttr **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsnull;

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsresult rv = nsContentUtils::GetNodeInfoFromQName(aNamespaceURI,
                                                     aQualifiedName,
                                                     mNodeInfoManager,
                                                     getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString value;
  nsDOMAttribute* attribute = new nsDOMAttribute(nsnull, nodeInfo, value);
  NS_ENSURE_TRUE(attribute, NS_ERROR_OUT_OF_MEMORY);

  return CallQueryInterface(attribute, aResult);
}

NS_IMETHODIMP
nsDocument::CreateEntityReference(const nsAString& aName,
                                  nsIDOMEntityReference** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  *aReturn = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetElementsByTagName(const nsAString& aTagname,
                                 nsIDOMNodeList** aReturn)
{
  nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(aTagname);
  NS_ENSURE_TRUE(nameAtom, NS_ERROR_OUT_OF_MEMORY);

  nsContentList *list = NS_GetContentList(this, nameAtom, kNameSpaceID_Unknown,
                                          nsnull).get();
  NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);

  // transfer ref to aReturn
  *aReturn = list;
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                                   const nsAString& aLocalName,
                                   nsIDOMNodeList** aReturn)
{
  PRInt32 nameSpaceId = kNameSpaceID_Unknown;

  nsContentList *list = nsnull;

  if (!aNamespaceURI.EqualsLiteral("*")) {
    nameSpaceId =
      nsContentUtils::NameSpaceManager()->GetNameSpaceID(aNamespaceURI);

    if (nameSpaceId == kNameSpaceID_Unknown) {
      // Unknown namespace means no matches, we create an empty list...
      list = NS_GetContentList(this, nsnull, kNameSpaceID_None, nsnull).get();
      NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);
    }
  }

  if (!list) {
    nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(aLocalName);
    NS_ENSURE_TRUE(nameAtom, NS_ERROR_OUT_OF_MEMORY);

    list = NS_GetContentList(this, nameAtom, nameSpaceId, nsnull).get();
    NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);
  }

  // transfer ref to aReturn
  *aReturn = list;
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetElementById(const nsAString & elementId,
                           nsIDOMElement **_retval)
{
  // Should be implemented by subclass
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::GetAsync(PRBool *aAsync)
{
  NS_ERROR("nsDocument::GetAsync() should be overriden by subclass!");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::SetAsync(PRBool aAsync)
{
  NS_ERROR("nsDocument::SetAsync() should be overriden by subclass!");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::Load(const nsAString& aUrl, PRBool *aReturn)
{
  NS_ERROR("nsDocument::Load() should be overriden by subclass!");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::EvaluateFIXptr(const nsAString& aExpression, nsIDOMRange **aRange)
{
  NS_ERROR("nsDocument::EvaluateFIXptr() should be overriden by subclass!");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::EvaluateXPointer(const nsAString& aExpression,
                             nsIXPointerResult **aResult)
{
  NS_ERROR("nsDocument::EvaluateXPointer() should be overriden by subclass!");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::GetStyleSheets(nsIDOMStyleSheetList** aStyleSheets)
{
  if (!mDOMStyleSheets) {
    mDOMStyleSheets = new nsDOMStyleSheetList(this);
    if (!mDOMStyleSheets) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  *aStyleSheets = mDOMStyleSheets;
  NS_ADDREF(*aStyleSheets);

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetPreferredStylesheetSet(nsAString& aStyleTitle)
{
  CSSLoader()->GetPreferredSheet(aStyleTitle);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetCharacterSet(nsAString& aCharacterSet)
{
  CopyASCIItoUTF16(GetDocumentCharacterSet(), aCharacterSet);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::ImportNode(nsIDOMNode* aImportedNode,
                       PRBool aDeep,
                       nsIDOMNode** aResult)
{
  NS_ENSURE_ARG(aImportedNode);

  *aResult = nsnull;

  nsresult rv = nsContentUtils::CheckSameOrigin(this, aImportedNode);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsIDocument *document;
  PRUint16 nodeType;
  aImportedNode->GetNodeType(&nodeType);
  switch (nodeType) {
    case nsIDOMNode::ATTRIBUTE_NODE:
    {
      nsCOMPtr<nsIAttribute> attr = do_QueryInterface(aImportedNode);
      NS_ENSURE_TRUE(attr, NS_ERROR_FAILURE);

      document = attr->GetOwnerDoc();

      nsCOMPtr<nsIDOMAttr> domAttr = do_QueryInterface(aImportedNode);
      nsAutoString value;
      rv = domAttr->GetValue(value);
      NS_ENSURE_SUCCESS(rv, rv);

      nsINodeInfo *nodeInfo = attr->NodeInfo();
      nsCOMPtr<nsINodeInfo> newNodeInfo;
      if (document != this) {
        rv = mNodeInfoManager->GetNodeInfo(nodeInfo->NameAtom(),
                                           nodeInfo->GetPrefixAtom(),
                                           nodeInfo->NamespaceID(),
                                           getter_AddRefs(newNodeInfo));
        NS_ENSURE_SUCCESS(rv, rv);

        nodeInfo = newNodeInfo;
      }

      nsCOMPtr<nsIDOMNode> clone  = new nsDOMAttribute(nsnull, nodeInfo,
                                                       value);
      if (!clone) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      if (document) {
        // XXX For now, nsDOMAttribute has only one child. We need to notify
        //     about importing it, so we force creation here.
        nsCOMPtr<nsIDOMNode> child, newChild;
        aImportedNode->GetFirstChild(getter_AddRefs(child));
        clone->GetFirstChild(getter_AddRefs(newChild));

        nsCOMPtr<nsINode> childNode = do_QueryInterface(child);
        if (childNode && newChild) {
          document->CallUserDataHandler(nsIDOMUserDataHandler::NODE_IMPORTED,
                                        childNode, child, newChild);
        }
      }

      clone.swap(*aResult);
      break;
    }
    case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
    case nsIDOMNode::ELEMENT_NODE:
    case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
    case nsIDOMNode::TEXT_NODE:
    case nsIDOMNode::CDATA_SECTION_NODE:
    case nsIDOMNode::COMMENT_NODE:
    {
      nsCOMPtr<nsIContent> imported = do_QueryInterface(aImportedNode);
      NS_ENSURE_TRUE(imported, NS_ERROR_FAILURE);

      document = imported->GetOwnerDoc();
      nsCOMPtr<nsIContent> clone;
      rv = imported->CloneContent(mNodeInfoManager, aDeep,
                                  getter_AddRefs(clone));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = CallQueryInterface(clone, aResult);
      NS_ENSURE_SUCCESS(rv, rv);

      break;
    }
    case nsIDOMNode::ENTITY_NODE:
    case nsIDOMNode::ENTITY_REFERENCE_NODE:
    case nsIDOMNode::NOTATION_NODE:
    {
      return NS_ERROR_NOT_IMPLEMENTED;
    }
    default:
    {
      NS_WARNING("Don't know how to clone this nodetype for importNode.");

      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }
  }

  if (document) {
    nsCOMPtr<nsINode> importedNode = do_QueryInterface(aImportedNode);
    document->CallUserDataHandler(nsIDOMUserDataHandler::NODE_IMPORTED,
                                  importedNode, aImportedNode, *aResult);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::AddBinding(nsIDOMElement* aContent, const nsAString& aURI)
{
  NS_ENSURE_ARG(aContent);
  
  nsresult rv = nsContentUtils::CheckSameOrigin(this, aContent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIContent> content(do_QueryInterface(aContent));

  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), aURI);
  return mBindingManager->AddLayeredBinding(content, uri);
}

NS_IMETHODIMP
nsDocument::RemoveBinding(nsIDOMElement* aContent, const nsAString& aURI)
{
  NS_ENSURE_ARG(aContent);

  nsresult rv = nsContentUtils::CheckSameOrigin(this, aContent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), aURI);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIContent> content(do_QueryInterface(aContent));
  return mBindingManager->RemoveLayeredBinding(content, uri);
}

NS_IMETHODIMP
nsDocument::LoadBindingDocument(const nsAString& aURI,
                                nsIDOMDocument** aResult)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURI,
                          mCharacterSet.get(),
                          NS_STATIC_CAST(nsIDocument *, this)->GetBaseURI());

  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIDocument> doc;
  mBindingManager->LoadBindingDocument(this, uri, getter_AddRefs(doc));

  if (doc) {
    CallQueryInterface(doc, aResult);
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetBindingParent(nsIDOMNode* aNode, nsIDOMElement** aResult)
{
  *aResult = nsnull;
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  if (!content)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(content->GetBindingParent()));
  NS_IF_ADDREF(*aResult = elt);
  return NS_OK;
}

static nsresult
GetElementByAttribute(nsIContent* aContent, nsIAtom* aAttrName,
                      const nsAString& aAttrValue, PRBool aUniversalMatch,
                      nsIDOMElement** aResult)
{
  if (aUniversalMatch ? aContent->HasAttr(kNameSpaceID_None, aAttrName) :
                        aContent->AttrValueIs(kNameSpaceID_None, aAttrName,
                                              aAttrValue, eCaseMatters)) {
    return CallQueryInterface(aContent, aResult);
  }

  PRUint32 childCount = aContent->GetChildCount();

  for (PRUint32 i = 0; i < childCount; ++i) {
    nsIContent *current = aContent->GetChildAt(i);

    GetElementByAttribute(current, aAttrName, aAttrValue, aUniversalMatch,
                          aResult);

    if (*aResult)
      return NS_OK;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetAnonymousElementByAttribute(nsIDOMElement* aElement,
                                           const nsAString& aAttrName,
                                           const nsAString& aAttrValue,
                                           nsIDOMElement** aResult)
{
  *aResult = nsnull;

  nsCOMPtr<nsIDOMNodeList> nodeList;
  GetAnonymousNodes(aElement, getter_AddRefs(nodeList));

  if (!nodeList)
    return NS_OK;

  nsCOMPtr<nsIAtom> attribute = do_GetAtom(aAttrName);

  PRUint32 length;
  nodeList->GetLength(&length);

  PRBool universalMatch = aAttrValue.EqualsLiteral("*");

  for (PRUint32 i = 0; i < length; ++i) {
    nsCOMPtr<nsIDOMNode> current;
    nodeList->Item(i, getter_AddRefs(current));

    nsCOMPtr<nsIContent> content(do_QueryInterface(current));

    GetElementByAttribute(content, attribute, aAttrValue, universalMatch,
                          aResult);
    if (*aResult)
      return NS_OK;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsDocument::GetAnonymousNodes(nsIDOMElement* aElement,
                              nsIDOMNodeList** aResult)
{
  *aResult = nsnull;

  nsCOMPtr<nsIContent> content(do_QueryInterface(aElement));
  return mBindingManager->GetAnonymousNodesFor(content, aResult);
}

NS_IMETHODIMP
nsDocument::CreateRange(nsIDOMRange** aReturn)
{
  nsresult rv = NS_NewRange(aReturn);

  if (NS_SUCCEEDED(rv)) {
    (*aReturn)->SetStart(this, 0);
    (*aReturn)->SetEnd(this, 0);
  }

  return rv;
}

NS_IMETHODIMP
nsDocument::CreateNodeIterator(nsIDOMNode *aRoot,
                               PRUint32 aWhatToShow,
                               nsIDOMNodeFilter *aFilter,
                               PRBool aEntityReferenceExpansion,
                               nsIDOMNodeIterator **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::CreateTreeWalker(nsIDOMNode *aRoot,
                             PRUint32 aWhatToShow,
                             nsIDOMNodeFilter *aFilter,
                             PRBool aEntityReferenceExpansion,
                             nsIDOMTreeWalker **_retval)
{
  *_retval = nsnull;

  if (!aRoot) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  nsresult rv = nsContentUtils::CheckSameOrigin(this, aRoot);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_NewTreeWalker(aRoot, aWhatToShow, aFilter,
                          aEntityReferenceExpansion, _retval);
}


NS_IMETHODIMP
nsDocument::GetDefaultView(nsIDOMAbstractView** aDefaultView)
{
  nsPIDOMWindow* win = GetWindow();
  if (win) {
    return CallQueryInterface(win, aDefaultView);
  }

  *aDefaultView = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetLocation(nsIDOMLocation **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  nsCOMPtr<nsIDOMWindowInternal> w(do_QueryInterface(mScriptGlobalObject));

  if (!w) {
    return NS_OK;
  }

  return w->GetLocation(_retval);
}

NS_IMETHODIMP
nsDocument::GetTitle(nsAString& aTitle)
{
  aTitle.Assign(mDocumentTitle);
  // Make sure not to return null from this method even if
  // mDocumentTitle is void.
  aTitle.SetIsVoid(PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::SetTitle(const nsAString& aTitle)
{
  for (PRInt32 i = GetNumberOfShells() - 1; i >= 0; --i) {
    nsCOMPtr<nsIPresShell> shell = GetShellAt(i);

    nsCOMPtr<nsISupports> container = shell->GetPresContext()->GetContainer();
    if (!container)
      continue;

    nsCOMPtr<nsIBaseWindow> docShellWin = do_QueryInterface(container);
    if (!docShellWin)
      continue;

    nsresult rv = docShellWin->SetTitle(PromiseFlatString(aTitle).get());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mDocumentTitle.Assign(aTitle);

  // Fire a DOM event for the title change.
  nsContentUtils::DispatchTrustedEvent(this, NS_STATIC_CAST(nsIDocument*, this),
                                       NS_LITERAL_STRING("DOMTitleChanged"),
                                       PR_TRUE, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetBoxObjectFor(nsIDOMElement* aElement, nsIBoxObject** aResult)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(aElement));
  NS_ENSURE_TRUE(content, NS_ERROR_UNEXPECTED);
  NS_ENSURE_TRUE(content->GetCurrentDoc() == this,
                 NS_ERROR_DOM_WRONG_DOCUMENT_ERR);
  
  *aResult = nsnull;

  if (!mBoxObjectTable) {
    mBoxObjectTable = new nsInterfaceHashtable<nsISupportsHashKey, nsPIBoxObject>;
    if (mBoxObjectTable) {
      mBoxObjectTable->Init(12);
    }
  } else {
    // Want to use Get(content, aResult); but it's the wrong type
    *aResult = mBoxObjectTable->GetWeak(content);
    if (*aResult) {
      NS_ADDREF(*aResult);
      return NS_OK;
    }
  }

  PRInt32 namespaceID;
  nsCOMPtr<nsIAtom> tag;
  nsCOMPtr<nsIXBLService> xblService = do_GetService("@mozilla.org/xbl;1");
  NS_ENSURE_TRUE(xblService, NS_ERROR_FAILURE);
  xblService->ResolveTag(content, &namespaceID, getter_AddRefs(tag));

  nsCAutoString contractID("@mozilla.org/layout/xul-boxobject");
  if (namespaceID == kNameSpaceID_XUL) {
    if (tag == nsXULAtoms::browser)
      contractID += "-browser";
    else if (tag == nsXULAtoms::editor)
      contractID += "-editor";
    else if (tag == nsXULAtoms::iframe)
      contractID += "-iframe";
    else if (tag == nsXULAtoms::menu)
      contractID += "-menu";
    else if (tag == nsXULAtoms::popup ||
             tag == nsXULAtoms::menupopup ||
             tag == nsXULAtoms::tooltip)
      contractID += "-popup";
    else if (tag == nsXULAtoms::tree)
      contractID += "-tree";
    else if (tag == nsXULAtoms::listbox)
      contractID += "-listbox";
    else if (tag == nsXULAtoms::scrollbox)
      contractID += "-scrollbox";
  }
  contractID += ";1";

  nsCOMPtr<nsPIBoxObject> boxObject(do_CreateInstance(contractID.get()));
  if (!boxObject)
    return NS_ERROR_FAILURE;

  boxObject->Init(content);

  if (mBoxObjectTable) {
    mBoxObjectTable->Put(content, boxObject.get());
  }

  *aResult = boxObject;
  NS_ADDREF(*aResult);

  return NS_OK;
}

void
nsDocument::ClearBoxObjectFor(nsIContent* aContent)
{
  if (mBoxObjectTable) {
    nsPIBoxObject *boxObject = mBoxObjectTable->GetWeak(aContent);
    if (boxObject) {
      boxObject->Clear();
      mBoxObjectTable->Remove(aContent);
    }
  }
}

struct DirTable {
  const char* mName;
  PRUint8     mValue;
};

static const DirTable dirAttributes[] = {
  {"ltr", IBMBIDI_TEXTDIRECTION_LTR},
  {"rtl", IBMBIDI_TEXTDIRECTION_RTL},
  {0}
};

/**
 * Retrieve the "direction" property of the document.
 *
 * @lina 01/09/2001
 */
NS_IMETHODIMP
nsDocument::GetDir(nsAString& aDirection)
{
  PRUint32 options = GetBidiOptions();
  for (const DirTable* elt = dirAttributes; elt->mName; elt++) {
    if (GET_BIDI_OPTION_DIRECTION(options) == elt->mValue) {
      CopyASCIItoUTF16(elt->mName, aDirection);
      break;
    }
  }

  return NS_OK;
}

/**
 * Set the "direction" property of the document.
 *
 * @lina 01/09/2001
 */
NS_IMETHODIMP
nsDocument::SetDir(const nsAString& aDirection)
{
  PRUint32 options = GetBidiOptions();

  for (const DirTable* elt = dirAttributes; elt->mName; elt++) {
    if (aDirection == NS_ConvertASCIItoUTF16(elt->mName)) {
      if (GET_BIDI_OPTION_DIRECTION(options) != elt->mValue) {
        SET_BIDI_OPTION_DIRECTION(options, elt->mValue);
        nsIPresShell *shell = GetShellAt(0);
        if (shell) {
          nsPresContext *context = shell->GetPresContext();
          NS_ENSURE_TRUE(context, NS_ERROR_UNEXPECTED);
          context->SetBidi(options, PR_TRUE);
        } else {
          // No presentation; just set it on ourselves
          SetBidiOptions(options);
        }
      }

      break;
    }
  }

  return NS_OK;
}

//
// nsIDOMGCParticipant methods
//
nsIDOMGCParticipant*
nsDocument::GetSCCIndex()
{
  return this;
}

void
nsDocument::AppendReachableList(nsCOMArray<nsIDOMGCParticipant>& aArray)
{
  nsCOMPtr<nsIDOMGCParticipant> gcp = do_QueryInterface(mScriptGlobalObject);
  if (gcp)
    aArray.AppendObject(gcp);
}


//
// nsIDOMNode methods
//
NS_IMETHODIMP
nsDocument::GetNodeName(nsAString& aNodeName)
{
  aNodeName.AssignLiteral("#document");

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetNodeValue(nsAString& aNodeValue)
{
  SetDOMStringToNull(aNodeValue);

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::SetNodeValue(const nsAString& aNodeValue)
{
  // The DOM spec says that when nodeValue is defined to be null "setting it
  // has no effect", so we don't throw an exception.
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = nsIDOMNode::DOCUMENT_NODE;

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetParentNode(nsIDOMNode** aParentNode)
{
  *aParentNode = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  if (!mChildNodes) {
    mChildNodes = new nsChildContentList(this);
    if (!mChildNodes) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return CallQueryInterface(mChildNodes.get(), aChildNodes);
}

NS_IMETHODIMP
nsDocument::HasChildNodes(PRBool* aHasChildNodes)
{
  NS_ENSURE_ARG(aHasChildNodes);

  *aHasChildNodes = (mChildren.ChildCount() != 0);

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::HasAttributes(PRBool* aHasAttributes)
{
  NS_ENSURE_ARG(aHasAttributes);

  *aHasAttributes = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetFirstChild(nsIDOMNode** aFirstChild)
{
  if (mChildren.ChildCount()) {
    return CallQueryInterface(mChildren.ChildAt(0), aFirstChild);
  }

  *aFirstChild = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetLastChild(nsIDOMNode** aLastChild)
{
  PRInt32 count = mChildren.ChildCount();
  if (count) {
    return CallQueryInterface(mChildren.ChildAt(count-1), aLastChild);
  }

  *aLastChild = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
  *aPreviousSibling = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetNextSibling(nsIDOMNode** aNextSibling)
{
  *aNextSibling = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
  *aAttributes = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetNamespaceURI(nsAString& aNamespaceURI)
{
  SetDOMStringToNull(aNamespaceURI);

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetPrefix(nsAString& aPrefix)
{
  SetDOMStringToNull(aPrefix);

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::SetPrefix(const nsAString& aPrefix)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsDocument::GetLocalName(nsAString& aLocalName)
{
  SetDOMStringToNull(aLocalName);

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                         nsIDOMNode** aReturn)
{
  return nsGenericElement::doReplaceOrInsertBefore(PR_FALSE, aNewChild, aRefChild, nsnull, this,
                                          aReturn);
}

NS_IMETHODIMP
nsDocument::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                         nsIDOMNode** aReturn)
{
  return nsGenericElement::doReplaceOrInsertBefore(PR_TRUE, aNewChild, aOldChild, nsnull, this,
                                          aReturn);
}

NS_IMETHODIMP
nsDocument::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return nsGenericElement::doRemoveChild(aOldChild, nsnull, this, aReturn);
}

NS_IMETHODIMP
nsDocument::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
  return nsDocument::InsertBefore(aNewChild, nsnull, aReturn);
}

NS_IMETHODIMP
nsDocument::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  // XXX should be implemented by subclass
  *aReturn = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::Normalize()
{
  PRInt32 count = mChildren.ChildCount();
  for (PRInt32 i = 0; i < count; ++i) {
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mChildren.ChildAt(i)));

    if (node) {
      node->Normalize();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::IsSupported(const nsAString& aFeature, const nsAString& aVersion,
                        PRBool* aReturn)
{
  return nsGenericElement::InternalIsSupported(NS_STATIC_CAST(nsIDOMDocument*, this),
                                               aFeature, aVersion, aReturn);
}

NS_IMETHODIMP
nsDocument::GetBaseURI(nsAString &aURI)
{
  nsCAutoString spec;
  if (mDocumentBaseURI) {
    mDocumentBaseURI->GetSpec(spec);
  }

  CopyUTF8toUTF16(spec, aURI);

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetTextContent(nsAString &aTextContent)
{
  SetDOMStringToNull(aTextContent);

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::SetTextContent(const nsAString& aTextContent)
{
  return NS_OK;
}


NS_IMETHODIMP
nsDocument::CompareDocumentPosition(nsIDOMNode* aOther, PRUint16* aReturn)
{
  NS_ENSURE_ARG_POINTER(aOther);

  // We could optimize this by getting the other nodes current document
  // and comparing with ourself. But then we'd have to deal with the
  // current document being null and such so it's easier this way.
  // It's hardly a case to optimize anyway.

  nsCOMPtr<nsINode> other = do_QueryInterface(aOther);
  NS_ENSURE_TRUE(other, NS_ERROR_DOM_NOT_SUPPORTED_ERR);

  *aReturn = nsContentUtils::ComparePosition(other, this);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::IsSameNode(nsIDOMNode* aOther, PRBool* aReturn)
{
  PRBool sameNode = PR_FALSE;

  if (this == aOther) {
    sameNode = PR_TRUE;
  }

  *aReturn = sameNode;

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::IsEqualNode(nsIDOMNode* aOther, PRBool* aReturn)
{
  NS_NOTYETIMPLEMENTED("nsDocument::IsEqualNode()");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::IsDefaultNamespace(const nsAString& aNamespaceURI,
                               PRBool* aReturn)
{
  nsAutoString defaultNamespace;
  LookupNamespaceURI(EmptyString(), defaultNamespace);
  *aReturn = aNamespaceURI.Equals(defaultNamespace);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetFeature(const nsAString& aFeature,
                       const nsAString& aVersion,
                       nsISupports** aReturn)
{
  return nsGenericElement::InternalGetFeature(NS_STATIC_CAST(nsIDOMDocument*, this),
                                              aFeature, aVersion, aReturn);
}

NS_IMETHODIMP
nsDocument::SetUserData(const nsAString &aKey,
                        nsIVariant *aData,
                        nsIDOMUserDataHandler *aHandler,
                        nsIVariant **aResult)
{
  nsCOMPtr<nsIAtom> key = do_GetAtom(aKey);
  if (!key) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return SetUserData(this, key, aData, aHandler,
                     aResult);
}

NS_IMETHODIMP
nsDocument::GetUserData(const nsAString &aKey,
                        nsIVariant **aResult)
{
  nsCOMPtr<nsIAtom> key = do_GetAtom(aKey);
  if (!key) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return GetUserData(this, key, aResult);
}

nsresult
nsDocument::SetUserData(const nsINode *aObject,
                        nsIAtom *aKey,
                        nsIVariant *aData,
                        nsIDOMUserDataHandler *aHandler,
                        nsIVariant **aResult)
{
#ifdef DEBUG
  nsCOMPtr<nsINode> object =
    do_QueryInterface(NS_CONST_CAST(nsINode*, aObject));
  NS_ASSERTION(object == aObject, "Use canonical nsINode pointer!");
#endif

  *aResult = nsnull;

  nsresult rv = NS_OK;
  void *data;
  if (aData) {
    rv = mPropertyTable.SetProperty(aObject, DOM_USER_DATA, aKey, aData,
                                    nsPropertyTable::SupportsDtorFunc, &data);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ADDREF(aData);
  }
  else {
    data = mPropertyTable.UnsetProperty(aObject, DOM_USER_DATA, aKey);
  }

  // Take over ownership of the old data from the property table.
  nsCOMPtr<nsIVariant> oldData = dont_AddRef(NS_STATIC_CAST(nsIVariant*,
                                                            data));

  if (aData && aHandler) {
    rv = mPropertyTable.SetProperty(aObject, DOM_USER_DATA_HANDLER, aKey,
                                    aHandler,
                                    nsPropertyTable::SupportsDtorFunc);
    if (NS_SUCCEEDED(rv)) {
      NS_ADDREF(aHandler);
    }
  }
  else {
    nsIDOMUserDataHandler *oldHandler =
      NS_STATIC_CAST(nsIDOMUserDataHandler*,
                     mPropertyTable.UnsetProperty(aObject,
                                                  DOM_USER_DATA_HANDLER,
                                                  aKey));
    NS_IF_RELEASE(oldHandler);
  }

  if (NS_SUCCEEDED(rv)) {
    oldData.swap(*aResult);
  }
  else {
    // We failed to set the handler, remove the data.
    nsIVariant *variant =
      NS_STATIC_CAST(nsIVariant*,
                     mPropertyTable.UnsetProperty(aObject, DOM_USER_DATA,
                                                  aKey));
    NS_IF_RELEASE(variant);
  }

  return rv;
}

nsresult
nsDocument::GetUserData(const nsINode *aObject, nsIAtom *aKey,
                        nsIVariant **aResult)
{
#ifdef DEBUG
  nsCOMPtr<nsINode> object =
    do_QueryInterface(NS_CONST_CAST(nsINode*, aObject));
  NS_ASSERTION(object == aObject, "Use canonical nsINode pointer!");
#endif

  *aResult = NS_STATIC_CAST(nsIVariant*,
                            mPropertyTable.GetProperty(aObject, DOM_USER_DATA,
                                                       aKey));

  NS_IF_ADDREF(*aResult);

  return NS_OK;
}

struct nsHandlerData
{
  PRUint16 mOperation;
  nsCOMPtr<nsIDOMNode> mSource;
  nsCOMPtr<nsIDOMNode> mDest;
  nsDocument *mDocument;
};

static void
CallHandler(void *aObject, nsIAtom *aKey, void *aHandler, void *aData)
{
  nsHandlerData *handlerData = NS_STATIC_CAST(nsHandlerData*, aData);
  nsCOMPtr<nsIDOMUserDataHandler> handler =
    NS_STATIC_CAST(nsIDOMUserDataHandler*, aHandler);
  nsCOMPtr<nsIVariant> data;
  nsINode *object = NS_STATIC_CAST(nsINode*, aObject);
  nsresult rv = handlerData->mDocument->GetUserData(object, aKey,
                                                    getter_AddRefs(data));
  if (NS_SUCCEEDED(rv)) {
    NS_ASSERTION(data, "Handler without data?");

    nsAutoString key;
    aKey->ToString(key);
    handler->Handle(handlerData->mOperation, key, data, handlerData->mSource,
                    handlerData->mDest);
  }
}

void
nsDocument::CallUserDataHandler(PRUint16 aOperation,
                                const nsINode *aObject,
                                nsIDOMNode *aSource, nsIDOMNode *aDest)
{
#ifdef DEBUG
  // XXX Should we guard from QI'ing nodes that are being destroyed?
  nsCOMPtr<nsINode> object =
    do_QueryInterface(NS_CONST_CAST(nsINode*, aObject));
  NS_ASSERTION(object == aObject, "Use canonical nsINode pointer!");
#endif

  nsHandlerData handlerData;
  handlerData.mOperation = aOperation;
  handlerData.mSource = aSource;
  handlerData.mDest = aDest;
  handlerData.mDocument = this;

  mPropertyTable.Enumerate(aObject, DOM_USER_DATA_HANDLER, CallHandler,
                           &handlerData);
}

struct nsCopyData
{
  nsDocument *mSourceDocument;
  nsIDocument *mDestDocument;
};

static void
Copy(void *aObject, nsIAtom *aKey, void *aUserData, void *aData)
{
  nsCopyData *data = NS_STATIC_CAST(nsCopyData*, aData);
  nsPropertyTable *propertyTable = data->mSourceDocument->PropertyTable();
  nsINode *object = NS_STATIC_CAST(nsINode*, aObject);
  nsIDOMUserDataHandler *handler =
    NS_STATIC_CAST(nsIDOMUserDataHandler*,
                   propertyTable->GetProperty(object, DOM_USER_DATA_HANDLER,
                                              aKey));

  nsCOMPtr<nsIVariant> result;
  data->mDestDocument->SetUserData(object, aKey,
                                   NS_STATIC_CAST(nsIVariant*, aUserData),
                                   handler, getter_AddRefs(result));
}

void
nsDocument::CopyUserData(const nsINode *aObject, nsIDocument *aDestination)
{
#ifdef DEBUG
  nsCOMPtr<nsINode> object =
    do_QueryInterface(NS_CONST_CAST(nsINode*, aObject));
  NS_ASSERTION(object == aObject, "Use canonical nsINode pointer!");
#endif

  nsCopyData copyData = { this, aDestination };

  mPropertyTable.Enumerate(aObject, DOM_USER_DATA, Copy, &copyData);
}

NS_IMETHODIMP
nsDocument::LookupPrefix(const nsAString& aNamespaceURI,
                         nsAString& aPrefix)
{
  nsCOMPtr<nsIDOM3Node> root(do_QueryInterface(mRootContent));
  if (root) {
    return root->LookupPrefix(aNamespaceURI, aPrefix);
  }

  SetDOMStringToNull(aPrefix);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::LookupNamespaceURI(const nsAString& aNamespacePrefix,
                               nsAString& aNamespaceURI)
{
  if (NS_FAILED(nsContentUtils::LookupNamespaceURI(mRootContent,
                                                   aNamespacePrefix,
                                                   aNamespaceURI))) {
    SetDOMStringToNull(aNamespaceURI);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetInputEncoding(nsAString& aInputEncoding)
{
  return GetCharacterSet(aInputEncoding);
}

NS_IMETHODIMP
nsDocument::GetXmlEncoding(nsAString& aXmlEncoding)
{
  if (mXMLDeclarationBits & XML_DECLARATION_BITS_DECLARATION_EXISTS &&
      mXMLDeclarationBits & XML_DECLARATION_BITS_ENCODING_EXISTS) {
    // XXX We don't store the encoding given in the xml declaration.
    // For now, just output the inputEncoding which we do store.
    GetInputEncoding(aXmlEncoding);
  } else {
    SetDOMStringToNull(aXmlEncoding);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetXmlStandalone(PRBool *aXmlStandalone)
{
  *aXmlStandalone = 
    mXMLDeclarationBits & XML_DECLARATION_BITS_DECLARATION_EXISTS &&
    mXMLDeclarationBits & XML_DECLARATION_BITS_STANDALONE_EXISTS &&
    mXMLDeclarationBits & XML_DECLARATION_BITS_STANDALONE_YES;

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::SetXmlStandalone(PRBool aXmlStandalone)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::GetXmlVersion(nsAString& aXmlVersion)
{
  // If there is no declaration, the value is "1.0".

  // XXX We only support "1.0", so always output "1.0" until that changes.
  aXmlVersion.AssignLiteral("1.0");

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::SetXmlVersion(const nsAString& aXmlVersion)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::GetStrictErrorChecking(PRBool *aStrictErrorChecking)
{
  // This attribute is true by default, and we don't really support it being false.
  *aStrictErrorChecking = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::SetStrictErrorChecking(PRBool aStrictErrorChecking)
{
  // We don't really support non-strict error checking, so just no-op for now.
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetDocumentURI(nsAString& aDocumentURI)
{
  if (mDocumentURI) {
    nsCAutoString uri;
    mDocumentURI->GetSpec(uri);
    CopyUTF8toUTF16(uri, aDocumentURI);
  } else {
    SetDOMStringToNull(aDocumentURI);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::SetDocumentURI(const nsAString& aDocumentURI)
{
  // Not allowing this yet, need to think about security ramifications first.
  // We use mDocumentURI to get principals for this document.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::AdoptNode(nsIDOMNode *source, nsIDOMNode **aReturn)
{
  // Not allowing this yet, need to think about the security ramifications
  // of giving a node a brand new node info.

//  if (NS_SUCCEEDED(rv)) {
//    nsCOMPtr<nsISupports> object = do_QueryInterface(source);
//    CallUserDataHandler(nsIDOMUserDataHandler::NODE_ADOPTED, object, source,
//                        *aReturn);
//  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::GetDomConfig(nsIDOMDOMConfiguration **aConfig)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::NormalizeDocument()
{
  // We don't support DOMConfigurations yet, so this just
  // does a straight shot of normalization.
  return Normalize();
}

NS_IMETHODIMP
nsDocument::RenameNode(nsIDOMNode *aNode,
                       const nsAString& namespaceURI,
                       const nsAString& qualifiedName,
                       nsIDOMNode **aReturn)
{
  if (!aNode) {
    // not an element or attribute
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }
  
  PRUint16 nodeType;
  aNode->GetNodeType(&nodeType);
  if (nodeType == nsIDOMNode::ELEMENT_NODE ||
      nodeType == nsIDOMNode::ATTRIBUTE_NODE) {
    // XXXcaa Write me - Coming soon to a document near you!
    return NS_ERROR_NOT_IMPLEMENTED;
  }

//  if (NS_SUCCEEDED(rv)) {
//    nsCOMPtr<nsISupports> object = do_QueryInterface(aNode);
//    CallUserDataHandler(nsIDOMUserDataHandler::NODE_RENAMED, object, aNode,
//                        *aReturn);
//  }

  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}


NS_IMETHODIMP
nsDocument::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  *aOwnerDocument = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetListenerManager(PRBool aCreateIfNotFound,
                               nsIEventListenerManager** aInstancePtrResult)
{
  if (mListenerManager) {
    *aInstancePtrResult = mListenerManager;
    NS_ADDREF(*aInstancePtrResult);

    return NS_OK;
  }
  if (!aCreateIfNotFound) {
    *aInstancePtrResult = nsnull;
    return NS_OK;
  }

  nsresult rv = NS_NewEventListenerManager(getter_AddRefs(mListenerManager));
  NS_ENSURE_SUCCESS(rv, rv);

  mListenerManager->SetListenerTarget(NS_STATIC_CAST(nsIDocument *,this));

  *aInstancePtrResult = mListenerManager;
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}

nsresult
nsDocument::HandleEvent(nsIDOMEvent *aEvent)
{
  PRBool defaultActionEnabled;
  return DispatchEvent(aEvent, &defaultActionEnabled);
}

NS_IMETHODIMP
nsDocument::GetSystemEventGroup(nsIDOMEventGroup **aGroup)
{
  nsCOMPtr<nsIEventListenerManager> manager;
  if (NS_SUCCEEDED(GetListenerManager(PR_TRUE, getter_AddRefs(manager))) &&
      manager) {
    return manager->GetSystemEventGroupLM(aGroup);
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsDocument::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = PR_TRUE;
   // FIXME! This is a hack to make middle mouse paste working also in Editor.
   // Bug 329119
  aVisitor.mForceContentDispatch = PR_TRUE;
  aVisitor.mParentTarget = GetWindow();
  return NS_OK;
}

nsresult
nsDocument::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  return NS_OK;
}

nsresult
nsDocument::DispatchDOMEvent(nsEvent* aEvent,
                             nsIDOMEvent* aDOMEvent,
                             nsPresContext* aPresContext,
                             nsEventStatus* aEventStatus)
{
  return nsEventDispatcher::DispatchDOMEvent(NS_STATIC_CAST(nsINode*, this),
                                             aEvent, aDOMEvent,
                                             aPresContext, aEventStatus);
}

nsresult
nsDocument::AddEventListenerByIID(nsIDOMEventListener *aListener,
                                  const nsIID& aIID)
{
  nsCOMPtr<nsIEventListenerManager> manager;

  GetListenerManager(PR_TRUE, getter_AddRefs(manager));
  if (manager) {
    manager->AddEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsDocument::RemoveEventListenerByIID(nsIDOMEventListener *aListener,
                                     const nsIID& aIID)
{
  if (!mListenerManager) {
    return NS_ERROR_FAILURE;
  }

  mListenerManager->RemoveEventListenerByIID(aListener, aIID,
                                             NS_EVENT_FLAG_BUBBLE);
  return NS_OK;
}

nsresult
nsDocument::AddEventListener(const nsAString& aType,
                             nsIDOMEventListener* aListener,
                             PRBool aUseCapture)
{
  return AddEventListener(aType, aListener, aUseCapture,
                          !nsContentUtils::IsChromeDoc(this));
}

nsresult
nsDocument::RemoveEventListener(const nsAString& aType,
                                nsIDOMEventListener* aListener,
                                PRBool aUseCapture)
{
  return RemoveGroupedEventListener(aType, aListener, aUseCapture, nsnull);
}

NS_IMETHODIMP
nsDocument::DispatchEvent(nsIDOMEvent* aEvent, PRBool *_retval)
{
  // Obtain a presentation context
  nsIPresShell *shell = GetShellAt(0);
  nsCOMPtr<nsPresContext> context;
  if (shell) {
     context = shell->GetPresContext();
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv =
    nsEventDispatcher::DispatchDOMEvent(NS_STATIC_CAST(nsINode*, this),
                                        nsnull, aEvent, context, &status);

  *_retval = (status != nsEventStatus_eConsumeNoDefault);
  return rv;
}

NS_IMETHODIMP
nsDocument::AddGroupedEventListener(const nsAString& aType,
                                    nsIDOMEventListener *aListener,
                                    PRBool aUseCapture,
                                    nsIDOMEventGroup *aEvtGrp)
{
  nsCOMPtr<nsIEventListenerManager> manager;

  nsresult rv = GetListenerManager(PR_TRUE, getter_AddRefs(manager));
  if (NS_SUCCEEDED(rv) && manager) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

    manager->AddEventListenerByType(aListener, aType, flags, aEvtGrp);
    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP
nsDocument::RemoveGroupedEventListener(const nsAString& aType,
                                       nsIDOMEventListener *aListener,
                                       PRBool aUseCapture,
                                       nsIDOMEventGroup *aEvtGrp)
{
  if (!mListenerManager) {
    return NS_ERROR_FAILURE;
  }

  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

  mListenerManager->RemoveEventListenerByType(aListener, aType, flags,
                                              aEvtGrp);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::CanTrigger(const nsAString & type, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::IsRegisteredHere(const nsAString & type, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::AddEventListener(const nsAString& aType,
                             nsIDOMEventListener *aListener,
                             PRBool aUseCapture, PRBool aWantsUntrusted)
{
  nsCOMPtr<nsIEventListenerManager> manager;
  nsresult rv = GetListenerManager(PR_TRUE, getter_AddRefs(manager));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

  if (aWantsUntrusted) {
    flags |= NS_PRIV_EVENT_UNTRUSTED_PERMITTED;
  }

  return manager->AddEventListenerByType(aListener, aType, flags, nsnull);
}

NS_IMETHODIMP
nsDocument::CreateEvent(const nsAString& aEventType, nsIDOMEvent** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  // Obtain a presentation shell

  nsIPresShell *shell = GetShellAt(0);

  nsPresContext *presContext = nsnull;

  if (shell) {
    // Retrieve the context
    presContext = shell->GetPresContext();
  }

  // Create event even without presContext.
  return nsEventDispatcher::CreateEvent(presContext, nsnull,
                                        aEventType, aReturn);
}

NS_IMETHODIMP
nsDocument::CreateEventGroup(nsIDOMEventGroup **aInstancePtrResult)
{
  nsresult rv;
  nsCOMPtr<nsIDOMEventGroup> group(do_CreateInstance(kDOMEventGroupCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  *aInstancePtrResult = group;
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}

void
nsDocument::FlushPendingNotifications(mozFlushType aType)
{
  nsPIDOMWindow *window = GetWindow();

  if (aType == (aType & (Flush_Content | Flush_SinkNotifications)) ||
      !window) {
    // Nothing to do here
    return;
  }

  // We should be able to replace all this nsIDocShell* code with code
  // that uses mParentDocument, but mParentDocument is never set in
  // the current code!

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem =
    do_QueryInterface(window->GetDocShell());

  if (docShellAsItem) {
    nsCOMPtr<nsIDocShellTreeItem> docShellParent;
    docShellAsItem->GetSameTypeParent(getter_AddRefs(docShellParent));

    nsCOMPtr<nsIDOMWindow> parentWin(do_GetInterface(docShellParent));

    if (parentWin) {
      nsCOMPtr<nsIDOMDocument> dom_doc;
      parentWin->GetDocument(getter_AddRefs(dom_doc));

      nsCOMPtr<nsIDocument> doc(do_QueryInterface(dom_doc));

      // If we have a parent we must flush the parent too to ensure that our
      // container is reflown if its size was changed.  But if it's not safe to
      // flush ourselves, then don't flush the parent, since that can cause
      // things like resizes of our frame's widget, which we can't handle while
      // flushing is unsafe.
      if (doc && IsSafeToFlush()) {
        doc->FlushPendingNotifications(aType);
      }
    }
  }

  PRInt32 i, count = GetNumberOfShells();

  for (i = 0; i < count; i++) {
    nsCOMPtr<nsIPresShell> shell = GetShellAt(i);

    if (shell) {
      shell->FlushPendingNotifications(aType);
    }
  }
}

nsIScriptEventManager*
nsDocument::GetScriptEventManager()
{
  if (!mScriptEventManager) {
    mScriptEventManager = new nsScriptEventManager(this);
    // automatically AddRefs
  }

  return mScriptEventManager;
}

void
nsDocument::SetXMLDeclaration(const PRUnichar *aVersion,
                              const PRUnichar *aEncoding,
                              const PRInt32 aStandalone)
{
  if (!aVersion || *aVersion == '\0') {
    mXMLDeclarationBits = 0;
    return;
  }

  mXMLDeclarationBits = XML_DECLARATION_BITS_DECLARATION_EXISTS;

  if (aEncoding && *aEncoding != '\0') {
    mXMLDeclarationBits |= XML_DECLARATION_BITS_ENCODING_EXISTS;
  }

  if (aStandalone == 1) {
    mXMLDeclarationBits |= XML_DECLARATION_BITS_STANDALONE_EXISTS |
                           XML_DECLARATION_BITS_STANDALONE_YES;
  }
  else if (aStandalone == 0) {
    mXMLDeclarationBits |= XML_DECLARATION_BITS_STANDALONE_EXISTS;
  }
}

void
nsDocument::GetXMLDeclaration(nsAString& aVersion, nsAString& aEncoding,
                              nsAString& aStandalone)
{
  aVersion.Truncate();
  aEncoding.Truncate();
  aStandalone.Truncate();

  if (!(mXMLDeclarationBits & XML_DECLARATION_BITS_DECLARATION_EXISTS)) {
    return;
  }

  // always until we start supporting 1.1 etc.
  aVersion.AssignLiteral("1.0");

  if (mXMLDeclarationBits & XML_DECLARATION_BITS_ENCODING_EXISTS) {
    // This is what we have stored, not necessarily what was written
    // in the original
    GetCharacterSet(aEncoding);
  }

  if (mXMLDeclarationBits & XML_DECLARATION_BITS_STANDALONE_EXISTS) {
    if (mXMLDeclarationBits & XML_DECLARATION_BITS_STANDALONE_YES) {
      aStandalone.AssignLiteral("yes");
    } else {
      aStandalone.AssignLiteral("no");
    }
  }
}

PRBool
nsDocument::IsScriptEnabled()
{
  nsCOMPtr<nsIScriptSecurityManager> sm(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID));
  NS_ENSURE_TRUE(sm, PR_TRUE);

  nsIScriptGlobalObject* globalObject = GetScriptGlobalObject();
  NS_ENSURE_TRUE(globalObject, PR_TRUE);

  nsIScriptContext *scriptContext = globalObject->GetContext();
  NS_ENSURE_TRUE(scriptContext, PR_TRUE);

  JSContext* cx = (JSContext *) scriptContext->GetNativeContext();
  NS_ENSURE_TRUE(cx, PR_TRUE);

  PRBool enabled;
  nsresult rv = sm->CanExecuteScripts(cx, NodePrincipal(), &enabled);
  NS_ENSURE_SUCCESS(rv, PR_TRUE);
  return enabled;
}

nsresult
nsDocument::GetRadioGroup(const nsAString& aName,
                          nsRadioGroupStruct **aRadioGroup)
{
  nsAutoString tmKey(aName);
  if(!IsCaseSensitive())
     ToLowerCase(tmKey); //should case-insensitive.
  nsStringKey key(tmKey);
  nsRadioGroupStruct *radioGroup =
    NS_STATIC_CAST(nsRadioGroupStruct *, mRadioGroups.Get(&key));

  if (!radioGroup) {
    radioGroup = new nsRadioGroupStruct();
    NS_ENSURE_TRUE(radioGroup, NS_ERROR_OUT_OF_MEMORY);
    mRadioGroups.Put(&key, radioGroup);
  }

  *aRadioGroup = radioGroup;

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::SetCurrentRadioButton(const nsAString& aName,
                                  nsIDOMHTMLInputElement* aRadio)
{
  nsRadioGroupStruct* radioGroup = nsnull;
  GetRadioGroup(aName, &radioGroup);
  if (radioGroup) {
    radioGroup->mSelectedRadioButton = aRadio;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetCurrentRadioButton(const nsAString& aName,
                                  nsIDOMHTMLInputElement** aRadio)
{
  nsRadioGroupStruct* radioGroup = nsnull;
  GetRadioGroup(aName, &radioGroup);
  if (radioGroup) {
    *aRadio = radioGroup->mSelectedRadioButton;
    NS_IF_ADDREF(*aRadio);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetPositionInGroup(nsIDOMHTMLInputElement *aRadio,
                               PRInt32 *aPositionIndex,
                               PRInt32 *aItemsInGroup)
{
  *aPositionIndex = 0;
  *aItemsInGroup = 1;
  nsAutoString name;
  aRadio->GetName(name);
  if (name.IsEmpty()) {
    return NS_OK;
  }

  nsRadioGroupStruct* radioGroup = nsnull;
  nsresult rv = GetRadioGroup(name, &radioGroup);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFormControl> radioControl(do_QueryInterface(aRadio));
  NS_ASSERTION(radioControl, "Radio button should implement nsIFormControl");
  *aPositionIndex = radioGroup->mRadioButtons.IndexOf(radioControl);
  NS_ASSERTION(*aPositionIndex >= 0, "Radio button not found in its own group");
  *aItemsInGroup = radioGroup->mRadioButtons.Count();

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetNextRadioButton(const nsAString& aName,
                               const PRBool aPrevious,
                               nsIDOMHTMLInputElement*  aFocusedRadio,
                               nsIDOMHTMLInputElement** aRadioOut)
{
  // XXX Can we combine the HTML radio button method impls of 
  //     nsDocument and nsHTMLFormControl?
  // XXX Why is HTML radio button stuff in nsDocument, as 
  //     opposed to nsHTMLDocument?
  *aRadioOut = nsnull;

  nsRadioGroupStruct* radioGroup = nsnull;
  GetRadioGroup(aName, &radioGroup);
  if (!radioGroup) {
    return NS_ERROR_FAILURE;
  }

  // Return the radio button relative to the focused radio button.
  // If no radio is focused, get the radio relative to the selected one.
  nsCOMPtr<nsIDOMHTMLInputElement> currentRadio;
  if (aFocusedRadio) {
    currentRadio = aFocusedRadio;
  }
  else {
    currentRadio = radioGroup->mSelectedRadioButton;
    if (!currentRadio) {
      return NS_ERROR_FAILURE;
    }
  }
  nsCOMPtr<nsIFormControl> radioControl(do_QueryInterface(currentRadio));
  PRInt32 index = radioGroup->mRadioButtons.IndexOf(radioControl);
  if (index < 0) {
    return NS_ERROR_FAILURE;
  }

  PRInt32 numRadios = radioGroup->mRadioButtons.Count();
  PRBool disabled;
  nsCOMPtr<nsIDOMHTMLInputElement> radio;
  do {
    if (aPrevious) {
      if (--index < 0) {
        index = numRadios -1;
      }
    }
    else if (++index >= numRadios) {
      index = 0;
    }
    radio = do_QueryInterface(NS_STATIC_CAST(nsIFormControl*, 
                              radioGroup->mRadioButtons.ElementAt(index)));
    NS_ASSERTION(radio, "mRadioButtons holding a non-radio button");
    radio->GetDisabled(&disabled);
  } while (disabled && radio != currentRadio);

  NS_IF_ADDREF(*aRadioOut = radio);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::AddToRadioGroup(const nsAString& aName,
                            nsIFormControl* aRadio)
{
  nsRadioGroupStruct* radioGroup = nsnull;
  GetRadioGroup(aName, &radioGroup);
  if (radioGroup) {
    radioGroup->mRadioButtons.AppendElement(aRadio);
    NS_IF_ADDREF(aRadio);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::RemoveFromRadioGroup(const nsAString& aName,
                                 nsIFormControl* aRadio)
{
  nsRadioGroupStruct* radioGroup = nsnull;
  GetRadioGroup(aName, &radioGroup);
  if (radioGroup) {
    if (radioGroup->mRadioButtons.RemoveElement(aRadio)) {
      NS_IF_RELEASE(aRadio);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::WalkRadioGroup(const nsAString& aName,
                           nsIRadioVisitor* aVisitor,
                           PRBool aFlushContent)
{
  nsRadioGroupStruct* radioGroup = nsnull;
  GetRadioGroup(aName, &radioGroup);
  if (!radioGroup) {
    return NS_OK;
  }

  PRBool stop = PR_FALSE;
  for (int i = 0; i < radioGroup->mRadioButtons.Count(); i++) {
    aVisitor->Visit(NS_STATIC_CAST(nsIFormControl *,
                                   radioGroup->mRadioButtons.ElementAt(i)),
                    &stop);
    if (stop) {
      return NS_OK;
    }
  }

  return NS_OK;
}

void
nsDocument::RetrieveRelevantHeaders(nsIChannel *aChannel)
{
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  PRTime modDate = LL_ZERO;
  nsresult rv;

  if (httpChannel) {
    nsCAutoString tmp;
    rv = httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("last-modified"),
                                        tmp);

    if (NS_SUCCEEDED(rv)) {
      PRTime time;
      PRStatus st = PR_ParseTimeString(tmp.get(), PR_TRUE, &time);
      if (st == PR_SUCCESS) {
        modDate = time;
      }
    }

    // The misspelled key 'referer' is as per the HTTP spec
    rv = httpChannel->GetRequestHeader(NS_LITERAL_CSTRING("referer"),
                                       mReferrer);
    if (NS_FAILED(rv)) {
      mReferrer.Truncate();
    }

    static const char *const headers[] = {
      "default-style",
      "content-style-type",
      "content-language",
      "content-disposition",
      "refresh",
      // add more http headers if you need
      // XXXbz don't add content-location support without reading bug
      // 238654 and its dependencies/dups first.
      0
    };
    
    nsCAutoString headerVal;
    const char *const *name = headers;
    while (*name) {
      rv =
        httpChannel->GetResponseHeader(nsDependentCString(*name), headerVal);
      if (NS_SUCCEEDED(rv) && !headerVal.IsEmpty()) {
        nsCOMPtr<nsIAtom> key = do_GetAtom(*name);
        SetHeaderData(key, NS_ConvertASCIItoUTF16(headerVal));
      }
      ++name;
    }
  } else {
    nsCOMPtr<nsIFileChannel> fileChannel = do_QueryInterface(aChannel);
    if (fileChannel) {
      nsCOMPtr<nsIFile> file;
      fileChannel->GetFile(getter_AddRefs(file));
      if (file) {
        PRTime msecs;
        rv = file->GetLastModifiedTime(&msecs);

        if (NS_SUCCEEDED(rv)) {
          PRInt64 intermediateValue;
          LL_I2L(intermediateValue, PR_USEC_PER_MSEC);
          LL_MUL(modDate, msecs, intermediateValue);
        }
      }
    } else {
      nsCOMPtr<nsIMultiPartChannel> partChannel = do_QueryInterface(aChannel);
      if (partChannel) {
        nsCAutoString contentDisp;
        rv = partChannel->GetContentDisposition(contentDisp);
        if (NS_SUCCEEDED(rv) && !contentDisp.IsEmpty()) {
          SetHeaderData(nsHTMLAtoms::headerContentDisposition,
                        NS_ConvertASCIItoUTF16(contentDisp));
        }
      }
    }
  }

  if (LL_IS_ZERO(modDate)) {
    // We got nothing from our attempt to ask nsIFileChannel and
    // nsIHttpChannel for the last modified time. Return the current
    // time.
    modDate = PR_Now();
  }

  mLastModified.Truncate();
  if (LL_NE(modDate, LL_ZERO)) {
    PRExplodedTime prtime;
    PR_ExplodeTime(modDate, PR_LocalTimeParameters, &prtime);
    // "MM/DD/YYYY hh:mm:ss"
    char formatedTime[20];
    if (sprintf(formatedTime, "%02d/%02d/%04d %02d:%02d:%02d",
                prtime.tm_month + 1, prtime.tm_mday, prtime.tm_year,
                prtime.tm_hour     ,  prtime.tm_min,  prtime.tm_sec)) {
      CopyASCIItoUTF16(nsDependentCString(formatedTime), mLastModified);
    }
  }
}

nsresult
nsDocument::CreateElem(nsIAtom *aName, nsIAtom *aPrefix, PRInt32 aNamespaceID,
                       PRBool aDocumentDefaultType, nsIContent **aResult)
{
  nsresult rv;
#ifdef DEBUG
  nsAutoString qName;
  if (aPrefix) {
    aPrefix->ToString(qName);
    qName.Append(':');
  }
  const char *name;
  aName->GetUTF8String(&name);
  AppendUTF8toUTF16(name, qName);

  rv = nsContentUtils::CheckQName(qName, PR_TRUE);
  NS_ASSERTION(NS_SUCCEEDED(rv),
               "Don't pass invalid names to nsDocument::CreateElem, "
               "check caller.");
#endif

  *aResult = nsnull;
  
  PRInt32 elementType = aDocumentDefaultType ? mDefaultElementType :
                                               aNamespaceID;

  nsCOMPtr<nsINodeInfo> nodeInfo;
  rv = mNodeInfoManager->GetNodeInfo(aName, aPrefix, aNamespaceID,
                                     getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  return CreateElement(nodeInfo, elementType, aResult);
}

nsresult
nsDocument::CreateElement(nsINodeInfo *aNodeInfo, PRInt32 aElementType,
                          nsIContent** aResult)
{
  nsCOMPtr<nsIContent> content;
  nsresult rv = NS_NewElement(getter_AddRefs(content), aElementType,
                              aNodeInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  content.swap(*aResult);

  return NS_OK;
}

PRBool
nsDocument::IsSafeToFlush() const
{
  PRBool isSafeToFlush = PR_TRUE;
  PRInt32 i = 0, n = GetNumberOfShells();
  while (i < n && isSafeToFlush) {
    nsIPresShell* shell = GetShellAt(i);

    if (shell) {
      shell->IsSafeToFlush(isSafeToFlush);
    }
    ++i;
  }
  return isSafeToFlush;
}

nsresult
nsDocument::Sanitize()
{
  // Sanitize the document by resetting all password fields and any form
  // fields with autocomplete=off to their default values.  We do this now,
  // instead of when the presentation is restored, to offer some protection
  // in case there is ever an exploit that allows a cached document to be
  // accessed from a different document.

  // First locate all input elements, regardless of whether they are
  // in a form, and reset the password and autocomplete=off elements.

  nsCOMPtr<nsIDOMNodeList> nodes;
  nsresult rv = GetElementsByTagName(NS_LITERAL_STRING("input"),
                                     getter_AddRefs(nodes));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 length = 0;
  if (nodes)
    nodes->GetLength(&length);

  nsCOMPtr<nsIDOMNode> item;
  nsAutoString value;
  PRUint32 i;

  for (i = 0; i < length; ++i) {
    nodes->Item(i, getter_AddRefs(item));
    NS_ASSERTION(item, "null item in node list!");

    nsCOMPtr<nsIDOMHTMLInputElement> input = do_QueryInterface(item);
    if (!input)
      continue;

    PRBool resetValue = PR_FALSE;

    input->GetAttribute(NS_LITERAL_STRING("autocomplete"), value);
    if (value.LowerCaseEqualsLiteral("off")) {
      resetValue = PR_TRUE;
    } else {
      input->GetType(value);
      if (value.LowerCaseEqualsLiteral("password"))
        resetValue = PR_TRUE;
    }

    if (resetValue) {
      nsCOMPtr<nsIFormControl> fc = do_QueryInterface(input);
      fc->Reset();
    }
  }

  // Now locate all _form_ elements that have autocomplete=off and reset them
  rv = GetElementsByTagName(NS_LITERAL_STRING("form"), getter_AddRefs(nodes));
  NS_ENSURE_SUCCESS(rv, rv);

  length = 0;
  if (nodes)
    nodes->GetLength(&length);

  for (i = 0; i < length; ++i) {
    nodes->Item(i, getter_AddRefs(item));
    NS_ASSERTION(item, "null item in nodelist");

    nsCOMPtr<nsIDOMHTMLFormElement> form = do_QueryInterface(item);
    if (!form)
      continue;

    form->GetAttribute(NS_LITERAL_STRING("autocomplete"), value);
    if (value.LowerCaseEqualsLiteral("off"))
      form->Reset();
  }

  return NS_OK;
}

struct SubDocEnumArgs
{
  nsIDocument::nsSubDocEnumFunc callback;
  void *data;
};

PR_STATIC_CALLBACK(PLDHashOperator)
SubDocHashEnum(PLDHashTable *table, PLDHashEntryHdr *hdr,
               PRUint32 number, void *arg)
{
  SubDocMapEntry *entry = NS_STATIC_CAST(SubDocMapEntry*, hdr);
  SubDocEnumArgs *args = NS_STATIC_CAST(SubDocEnumArgs*, arg);

  nsIDocument *subdoc = entry->mSubDocument;
  PRBool next = subdoc ? args->callback(subdoc, args->data) : PR_TRUE;

  return next ? PL_DHASH_NEXT : PL_DHASH_STOP;
}

void
nsDocument::EnumerateSubDocuments(nsSubDocEnumFunc aCallback, void *aData)
{
  if (mSubDocuments) {
    SubDocEnumArgs args = { aCallback, aData };
    PL_DHashTableEnumerate(mSubDocuments, SubDocHashEnum, &args);
  }
}

PR_STATIC_CALLBACK(PLDHashOperator)
CanCacheSubDocument(PLDHashTable *table, PLDHashEntryHdr *hdr,
                    PRUint32 number, void *arg)
{
  SubDocMapEntry *entry = NS_STATIC_CAST(SubDocMapEntry*, hdr);
  PRBool *canCacheArg = NS_STATIC_CAST(PRBool*, arg);

  nsIDocument *subdoc = entry->mSubDocument;

  // The aIgnoreRequest we were passed is only for us, so don't pass it on.
  PRBool canCache = subdoc ? subdoc->CanSavePresentation(nsnull) : PR_FALSE;
  if (!canCache) {
    *canCacheArg = PR_FALSE;
    return PL_DHASH_STOP;
  }

  return PL_DHASH_NEXT;
}

#ifdef DEBUG_bryner
#define DEBUG_PAGE_CACHE
#endif

PRBool
nsDocument::CanSavePresentation(nsIRequest *aNewRequest)
{
  // Check our event listener manager for unload/beforeunload listeners.
  nsCOMPtr<nsIDOMEventReceiver> er = do_QueryInterface(mScriptGlobalObject);
  if (er) {
    nsCOMPtr<nsIEventListenerManager> manager;
    er->GetListenerManager(PR_FALSE, getter_AddRefs(manager));
    if (manager && manager->HasUnloadListeners()) {
      return PR_FALSE;
    }
  }

  nsCOMPtr<nsILoadGroup> loadGroup = GetDocumentLoadGroup();
  if (loadGroup) {
    nsCOMPtr<nsISimpleEnumerator> requests;
    loadGroup->GetRequests(getter_AddRefs(requests));

    PRBool hasMore = PR_FALSE;

    while (NS_SUCCEEDED(requests->HasMoreElements(&hasMore)) && hasMore) {
      nsCOMPtr<nsISupports> elem;
      requests->GetNext(getter_AddRefs(elem));

      nsCOMPtr<nsIRequest> request = do_QueryInterface(elem);
      if (request && request != aNewRequest) {
#ifdef DEBUG_PAGE_CACHE
        nsCAutoString requestName, docSpec;
        request->GetName(requestName);
        if (mDocumentURI)
          mDocumentURI->GetSpec(docSpec);

        printf("document %s has request %s\n",
               docSpec.get(), requestName.get());
#endif
        return PR_FALSE;
      }
    }
  }

  PRBool canCache = PR_TRUE;
  if (mSubDocuments)
    PL_DHashTableEnumerate(mSubDocuments, CanCacheSubDocument, &canCache);

  return canCache;
}

void
nsDocument::Destroy()
{
  // The ContentViewer wants to release the document now.  So, tell our content
  // to drop any references to the document so that it can be destroyed.
  if (mIsGoingAway)
    return;

  PRInt32 count = mChildren.ChildCount();

  mIsGoingAway = PR_TRUE;
  DestroyLinkMap();
  for (PRInt32 indx = 0; indx < count; ++indx) {
    mChildren.ChildAt(indx)->UnbindFromTree();
  }

  // Propagate the out-of-band notification to each PresShell's anonymous
  // content as well. This ensures that there aren't any accidental references
  // left in anonymous content keeping the document alive. (While not strictly
  // necessary -- the PresShell owns us -- it's tidy.)
  for (count = GetNumberOfShells() - 1; count >= 0; --count) {
    nsCOMPtr<nsIPresShell> shell = GetShellAt(count);
    if (!shell)
      continue;

    shell->ReleaseAnonymousContent();
  }

  mLayoutHistoryState = nsnull;
}

already_AddRefed<nsILayoutHistoryState>
nsDocument::GetLayoutHistoryState() const
{
  nsILayoutHistoryState* state = nsnull;
  if (!mScriptGlobalObject) {
    NS_IF_ADDREF(state = mLayoutHistoryState);
  } else {
    nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mDocumentContainer));
    if (docShell) {
      docShell->GetLayoutHistoryState(&state);
    }
  }

  return state;
}

void
nsDocument::BlockOnload()
{
  // If mScriptGlobalObject is null, we shouldn't be messing with the loadgroup
  // -- it's not ours.
  if (mOnloadBlockCount == 0 && mScriptGlobalObject) {
    nsCOMPtr<nsILoadGroup> loadGroup = GetDocumentLoadGroup();
    if (loadGroup) {
      loadGroup->AddRequest(mOnloadBlocker, nsnull);
    }
  }
  ++mOnloadBlockCount;      
}

void
nsDocument::UnblockOnload(PRBool aFireSync)
{
  if (mOnloadBlockCount == 0) {
    NS_NOTREACHED("More UnblockOnload() calls than BlockOnload() calls; dropping call");
    return;
  }

  --mOnloadBlockCount;

  // If mScriptGlobalObject is null, we shouldn't be messing with the loadgroup
  // -- it's not ours.
  if (mOnloadBlockCount == 0 && mScriptGlobalObject) {
    if (aFireSync) {
      // Increment mOnloadBlockCount, since DoUnblockOnload will decrement it
      ++mOnloadBlockCount;
      DoUnblockOnload();
    } else {
      PostUnblockOnloadEvent();
    }
  }
}

class nsUnblockOnloadEvent : public nsRunnable {
public:
  nsUnblockOnloadEvent(nsDocument *doc) : mDoc(doc) {}
  NS_IMETHOD Run() {
    mDoc->DoUnblockOnload();
    return NS_OK;
  }
private:  
  nsRefPtr<nsDocument> mDoc;
};

void
nsDocument::PostUnblockOnloadEvent()
{
  nsCOMPtr<nsIRunnable> evt = new nsUnblockOnloadEvent(this);
  nsresult rv = NS_DispatchToCurrentThread(evt);
  if (NS_SUCCEEDED(rv)) {
    // Stabilize block count so we don't post more events while this one is up
    ++mOnloadBlockCount;
  } else {
    NS_WARNING("failed to dispatch nsUnblockOnloadEvent");
  }
}

void
nsDocument::DoUnblockOnload()
{
  NS_ASSERTION(mOnloadBlockCount != 0,
               "Shouldn't have a count of zero here, since we stabilized in "
               "PostUnblockOnloadEvent");
  
  --mOnloadBlockCount;
  
  if (mOnloadBlockCount != 0) {
    // We blocked again after the last unblock.  Nothing to do here.  We'll
    // post a new event when we unblock again.
    return;
  }

  // If mScriptGlobalObject is null, we shouldn't be messing with the loadgroup
  // -- it's not ours.
  if (mScriptGlobalObject) {
    nsCOMPtr<nsILoadGroup> loadGroup = GetDocumentLoadGroup();
    if (loadGroup) {
      loadGroup->RemoveRequest(mOnloadBlocker, nsnull, NS_OK);
    }
  }
}

void
nsDocument::DispatchEventToWindow(nsEvent *aEvent)
{
  nsPIDOMWindow *window = GetWindow();
  if (!window)
    return;

  aEvent->target = NS_STATIC_CAST(nsIDocument*, this);
  nsEventDispatcher::Dispatch(window, nsnull, aEvent);
}

void
nsDocument::OnPageShow(PRBool aPersisted)
{
  mVisible = PR_TRUE;
  UpdateLinkMap();
  
  if (aPersisted) {
    // Send out notifications that our <link> elements are attached.
    nsRefPtr<nsContentList> links = NS_GetContentList(this,
                                                      nsHTMLAtoms::link,
                                                      kNameSpaceID_Unknown,
                                                      mRootContent);

    if (links) {
      PRUint32 linkCount = links->Length(PR_TRUE);
      for (PRUint32 i = 0; i < linkCount; ++i) {
        nsCOMPtr<nsILink> link = do_QueryInterface(links->Item(i, PR_FALSE));
        if (link) {
          link->LinkAdded();
        }
      }
    }
  }

  nsPageTransitionEvent event(PR_TRUE, NS_PAGE_SHOW, aPersisted);
  DispatchEventToWindow(&event);
}

void
nsDocument::OnPageHide(PRBool aPersisted)
{
  // Send out notifications that our <link> elements are detached,
  // but only if this is not a full unload.
  if (aPersisted) {
    nsRefPtr<nsContentList> links = NS_GetContentList(this,
                                                      nsHTMLAtoms::link,
                                                      kNameSpaceID_Unknown,
                                                      mRootContent);

    if (links) {
      PRUint32 linkCount = links->Length(PR_TRUE);
      for (PRUint32 i = 0; i < linkCount; ++i) {
        nsCOMPtr<nsILink> link = do_QueryInterface(links->Item(i, PR_FALSE));
        if (link) {
          link->LinkRemoved();
        }
      }
    }
  }

  // Now send out a PageHide event.
  nsPageTransitionEvent event(PR_TRUE, NS_PAGE_HIDE, aPersisted);
  DispatchEventToWindow(&event);

  mVisible = PR_FALSE;
}

static PRUint32 GetURIHash(nsIURI* aURI)
{
  nsCAutoString str;
  aURI->GetSpec(str);
  return HashString(str);
}

void
nsDocument::AddStyleRelevantLink(nsIContent* aContent, nsIURI* aURI)
{
  nsUint32ToContentHashEntry* entry = mLinkMap.PutEntry(GetURIHash(aURI));
  if (!entry) // out of memory?
    return;
  entry->PutContent(aContent);
}

void
nsDocument::ForgetLink(nsIContent* aContent)
{
  // Important optimization! If the link map is empty (as it will be
  // during teardown because we destroy the map early), then stop
  // now before we waste time constructing a URI object.
  if (mLinkMap.Count() == 0)
    return;
    
  nsCOMPtr<nsIURI> uri = nsContentUtils::GetLinkURI(aContent);
  if (!uri)
    return;
  PRUint32 hash = GetURIHash(uri);
  nsUint32ToContentHashEntry* entry = mLinkMap.GetEntry(hash);
  if (!entry)
    return;

  entry->RemoveContent(aContent);
  if (entry->IsEmpty()) {
    // Remove the entry and allow the table to resize, in case
    // a lot of links are being removed from the document or modified
    mLinkMap.RemoveEntry(hash);
  }
}

class URIVisitNotifier : public nsUint32ToContentHashEntry::Visitor
{
public:
  nsCAutoString matchURISpec;
  nsCOMArray<nsIContent> contentVisited;
  
  virtual void Visit(nsIContent* aContent) {
    // Ensure that the URIs really match before we try to do anything
    nsCOMPtr<nsIURI> uri = nsContentUtils::GetLinkURI(aContent);
    if (!uri) {
      NS_ERROR("Should have found a URI for content in the link map");
      return;
    }
    nsCAutoString spec;
    uri->GetSpec(spec);
    // We use nsCString::Equals here instead of nsIURI::Equals because
    // history matching is all based on spec equality
    if (!spec.Equals(matchURISpec))
      return;

    // Throw away the cached link state so it gets refetched by the style
    // system      
    nsCOMPtr<nsILink> link = do_QueryInterface(aContent);
    if (link) {
      link->SetLinkState(eLinkState_Unknown);
    }
    contentVisited.AppendObject(aContent);
  }
};

void
nsDocument::NotifyURIVisitednessChanged(nsIURI* aURI)
{
  if (!mVisible) {
    mVisitednessChangedURIs.AppendObject(aURI);
    return;
  }

  nsUint32ToContentHashEntry* entry = mLinkMap.GetEntry(GetURIHash(aURI));
  if (!entry)
    return;
  
  URIVisitNotifier visitor;
  aURI->GetSpec(visitor.matchURISpec);
  entry->VisitContent(&visitor);
  for (PRUint32 count = visitor.contentVisited.Count(), i = 0; i < count; ++i) {
    ContentStatesChanged(visitor.contentVisited[i],
                         nsnull, NS_EVENT_STATE_VISITED);
  }
}

void
nsDocument::DestroyLinkMap()
{
  mVisitednessChangedURIs.Clear();
  mLinkMap.Clear();
}

void
nsDocument::UpdateLinkMap()
{
  NS_ASSERTION(mVisible,
               "Should only be updating the link map in visible documents");
  if (!mVisible)
    return;
    
  PRInt32 count = mVisitednessChangedURIs.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    NotifyURIVisitednessChanged(mVisitednessChangedURIs[i]);
  }
  mVisitednessChangedURIs.Clear();
}
