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
#include "nsIPresContext.h"
#include "nsContentUtils.h"
#include "nsNodeInfoManager.h"
#include "nsIXBLService.h"
#include "nsIXPointer.h"
#include "nsIFileChannel.h"

#include "nsNetUtil.h"     // for NS_MakeAbsoluteURI

#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsIPrivateDOMImplementation.h"

#include "nsIDOMWindowInternal.h"
#include "nsIDOMElement.h"

#include "nsIBoxObject.h"
#include "nsPIBoxObject.h"
#include "nsXULAtoms.h"

// for radio group stuff
#include "nsIDOMHTMLInputElement.h"
#include "nsIRadioVisitor.h"
#include "nsIFormControl.h"

#include "nsXMLEventsManager.h"

#include "nsBidiUtils.h"

static NS_DEFINE_CID(kDOMEventGroupCID, NS_DOMEVENTGROUP_CID);

#include "nsILineBreakerFactory.h"
#include "nsIWordBreakerFactory.h"
#include "nsLWBrkCIID.h"

#include "nsHTMLAtoms.h"

#include "nsScriptEventManager.h"
#include "nsIXPathEvaluatorInternal.h"
#include "nsIParserService.h"
#include "nsContentCreatorFunctions.h"

#include "nsIScriptContext.h"

#include "nsICharsetAlias.h"
static NS_DEFINE_CID(kCharsetAliasCID, NS_CHARSETALIAS_CID);

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
      mLength = mDocument->GetNumberOfStyleSheets(PR_FALSE);

#ifdef DEBUG
      PRInt32 i;
      for (i = 0; i < mLength; i++) {
        nsIStyleSheet *sheet = mDocument->GetStyleSheetAt(i, PR_FALSE);
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
    PRInt32 count = mDocument->GetNumberOfStyleSheets(PR_FALSE);
    if (aIndex < (PRUint32)count) {
      nsIStyleSheet *sheet = mDocument->GetStyleSheetAt(aIndex, PR_FALSE);
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
                                     nsIStyleSheet* aStyleSheet)
{
  if (-1 != mLength) {
    nsCOMPtr<nsIDOMStyleSheet> domss(do_QueryInterface(aStyleSheet));
    if (domss) {
      mLength++;
    }
  }
}

void
nsDOMStyleSheetList::StyleSheetRemoved(nsIDocument *aDocument,
                                       nsIStyleSheet* aStyleSheet)
{
  if (-1 != mLength) {
    nsCOMPtr<nsIDOMStyleSheet> domss(do_QueryInterface(aStyleSheet));
    if (domss) {
      mLength--;
    }
  }
}


class nsDOMImplementation : public nsIDOMDOMImplementation,
                            public nsIPrivateDOMImplementation
{
public:
  nsDOMImplementation(nsIURI* aBaseURI = nsnull);
  virtual ~nsDOMImplementation();

  NS_DECL_ISUPPORTS

  // nsIDOMDOMImplementation
  NS_DECL_NSIDOMDOMIMPLEMENTATION

  // nsIPrivateDOMImplementation
  NS_IMETHOD Init(nsIURI* aBaseURI);

protected:
  nsCOMPtr<nsIURI> mBaseURI;
};


nsresult
NS_NewDOMImplementation(nsIDOMDOMImplementation** aInstancePtrResult)
{
  *aInstancePtrResult = new nsDOMImplementation();
  if (!*aInstancePtrResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}

nsDOMImplementation::nsDOMImplementation(nsIURI* aBaseURI)
{
  mBaseURI = aBaseURI;
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
  return nsGenericElement::InternalIsSupported(aFeature, aVersion, aReturn);
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

  return NS_NewDOMDocumentType(aReturn, name, nsnull, nsnull,
                               aPublicId, aSystemId, EmptyString());
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
    nsIParserService *parserService =
      nsContentUtils::GetParserServiceWeakRef();
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
                         mBaseURI);

  nsIDocShell *docShell = nsContentUtils::GetDocShellFromCaller();
  if (docShell) {
    nsCOMPtr<nsIPresContext> presContext;
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
nsDOMImplementation::Init(nsIURI* aBaseURI)
{
  mBaseURI = aBaseURI;
  return NS_OK;
}

// ==================================================================
// =
// ==================================================================

nsDocumentChildNodes::nsDocumentChildNodes(nsIDocument* aDocument)
{
  MOZ_COUNT_CTOR(nsDocumentChildNodes);

  // We don't reference count our document reference (to avoid circular
  // references). We'll be told when the document goes away.
  mDocument = aDocument;
}

nsDocumentChildNodes::~nsDocumentChildNodes()
{
  MOZ_COUNT_DTOR(nsDocumentChildNodes);
}

NS_IMETHODIMP
nsDocumentChildNodes::GetLength(PRUint32* aLength)
{
  if (mDocument) {
    *aLength = mDocument->GetChildCount();
  } else {
    *aLength = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentChildNodes::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  *aReturn = nsnull;

  if (mDocument) {
    nsIContent *content = mDocument->GetChildAt(aIndex);

    if (content) {
      return CallQueryInterface(content, aReturn);
    }
  }

  return NS_OK;
}

void
nsDocumentChildNodes::DropReference()
{
  mDocument = nsnull;
}

class nsXPathDocumentTearoff : public nsIDOMXPathEvaluator
{
public:
  nsXPathDocumentTearoff(nsIDOMXPathEvaluator* aEvaluator,
                         nsIDocument* aDocument)
    : mEvaluator(aEvaluator), mDocument(aDocument)
  {
  }
  virtual ~nsXPathDocumentTearoff()
  {
  }

  NS_DECL_ISUPPORTS_INHERITED

  NS_FORWARD_NSIDOMXPATHEVALUATOR(mEvaluator->)

private:
  nsCOMPtr<nsIDOMXPathEvaluator> mEvaluator;
  nsIDocument* mDocument;
};

NS_INTERFACE_MAP_BEGIN(nsXPathDocumentTearoff)
  NS_INTERFACE_MAP_ENTRY(nsIDOMXPathEvaluator)
NS_INTERFACE_MAP_END_AGGREGATED(mDocument)

NS_IMPL_ADDREF_USING_AGGREGATOR(nsXPathDocumentTearoff, mDocument)
NS_IMPL_RELEASE_USING_AGGREGATOR(nsXPathDocumentTearoff, mDocument)


// ==================================================================
// =
// ==================================================================

  // NOTE! nsDocument::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

nsDocument::nsDocument()
{

  // NOTE! nsIDocument::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

  // Force initialization.
  mBindingManager = do_CreateInstance("@mozilla.org/xbl/binding-manager;1");

  nsCOMPtr<nsIDocumentObserver> observer(do_QueryInterface(mBindingManager));
  if (observer) {
    // The binding manager must always be the first observer of the
    // document.

    mObservers.InsertElementAt(observer, 0);
  }
}

nsDocument::~nsDocument()
{
  mInDestructor = PR_TRUE;

  // XXX Inform any remaining observers that we are going away.
  // Note that this currently contradicts the rule that all
  // observers must hold on to live references to the document.
  // This notification will occur only after the reference has
  // been dropped.

  // if an observer removes itself, we're ok (not if it removes others though)
  PRInt32 indx;
  for (indx = mObservers.Count() - 1; indx >= 0; --indx) {
    // XXX Should this be a kungfudeathgrip?!!!!
    nsIDocumentObserver* observer =
      NS_STATIC_CAST(nsIDocumentObserver *, mObservers.ElementAt(indx));

    observer->DocumentWillBeDestroyed(this);
  }

  mParentDocument = nsnull;

  // Kill the subdocument map, doing this will release its strong
  // references, if any.
  if (mSubDocuments) {
    PL_DHashTableDestroy(mSubDocuments);

    mSubDocuments = nsnull;
  }

  if (mRootContent) {
    if (mRootContent->GetDocument()) {
      // The root content still has a pointer back to the document,
      // clear the document pointer in all children.

      PRInt32 count = mChildren.Count();
      for (indx = 0; indx < count; ++indx) {
        mChildren[indx]->SetDocument(nsnull, PR_TRUE, PR_FALSE);
      }
    }
  }

  mRootContent = nsnull;
  mChildren.Clear();

  // Let the stylesheets know we're going away
  indx = mStyleSheets.Count();
  while (--indx >= 0) {
    mStyleSheets[indx]->SetOwningDocument(nsnull);
  }

  if (mChildNodes) {
    mChildNodes->DropReference();
  }

  if (mListenerManager) {
    mListenerManager->SetListenerTarget(nsnull);
  }

  if (mScriptLoader) {
    mScriptLoader->DropDocumentReference();
  }

  if (mCSSLoader) {
    mCSSLoader->DropDocumentReference();
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
  delete mXPathDocument;
}

PRBool gCheckedForXPathDOM = PR_FALSE;
PRBool gHaveXPathDOM = PR_FALSE;

NS_INTERFACE_MAP_BEGIN(nsDocument)
  NS_INTERFACE_MAP_ENTRY(nsIDocument)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocument)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSDocument)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOM3DocumentEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentStyle)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentView)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentRange)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentTraversal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentXBL)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventReceiver)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOM3EventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOM3Node)
  NS_INTERFACE_MAP_ENTRY(nsIDOM3Document)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIRadioGroupContainer)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocument)
  if (aIID.Equals(NS_GET_IID(nsIDOMXPathEvaluator)) &&
      (!gCheckedForXPathDOM || gHaveXPathDOM)) {
    if (!mXPathDocument) {
      nsCOMPtr<nsIDOMXPathEvaluator> evaluator;
      nsresult rv;
      evaluator = do_CreateInstance(NS_XPATH_EVALUATOR_CONTRACTID, &rv);
      gCheckedForXPathDOM = PR_TRUE;
      gHaveXPathDOM = (evaluator != nsnull);
      if (rv == NS_ERROR_FACTORY_NOT_REGISTERED) {
          return NS_ERROR_NO_INTERFACE;
      }
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIXPathEvaluatorInternal> internal =
          do_QueryInterface(evaluator);
      if (internal) {
          internal->SetDocument(this);
      }

      mXPathDocument = new nsXPathDocumentTearoff(evaluator, this);
      NS_ENSURE_TRUE(mXPathDocument, NS_ERROR_OUT_OF_MEMORY);
    }
    foundInterface = mXPathDocument;
  }
  else
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDocument)
NS_IMPL_RELEASE(nsDocument)

nsresult
nsDocument::Init()
{
  if (mNodeInfoManager) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  mNodeInfoManager = new nsNodeInfoManager();
  NS_ENSURE_TRUE(mNodeInfoManager, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(mNodeInfoManager);

  return mNodeInfoManager->Init(this);
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
    aChannel->GetOriginalURI(getter_AddRefs(uri));
    nsresult rv;
    PRBool isAbout = PR_FALSE;
    PRBool isChrome = PR_FALSE;
    PRBool isRes = PR_FALSE;
    rv = uri->SchemeIs("chrome", &isChrome);
    rv |= uri->SchemeIs("resource", &isRes);
    rv |= uri->SchemeIs("about", &isAbout);

    if (NS_SUCCEEDED(rv) && !isChrome && !isRes && !isAbout) {
      aChannel->GetURI(getter_AddRefs(uri));
    }
  }

  ResetToURI(uri, aLoadGroup);

  if (aChannel) {
    nsCOMPtr<nsISupports> owner;
    aChannel->GetOwner(getter_AddRefs(owner));

    mPrincipal = do_QueryInterface(owner);
  }
}

void
nsDocument::ResetToURI(nsIURI *aURI, nsILoadGroup *aLoadGroup)
{
  NS_PRECONDITION(aURI, "Null URI passed to ResetToURI");
  mDocumentTitle.Truncate();

  mPrincipal = nsnull;
  mDocumentLoadGroup = nsnull;

  // Delete references to sub-documents and kill the subdocument map,
  // if any. It holds strong references
  if (mSubDocuments) {
    PL_DHashTableDestroy(mSubDocuments);

    mSubDocuments = nsnull;
  }

  mRootContent = nsnull;
  PRInt32 count, i;
  count = mChildren.Count();
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsIContent> content = mChildren[i];

    content->SetDocument(nsnull, PR_TRUE, PR_TRUE);
    ContentRemoved(nsnull, content, i);
  }
  mChildren.Clear();

  // Reset our stylesheets
  ResetStylesheetsToURI(aURI);
  
  // Release the listener manager
  mListenerManager = nsnull;

  // Release the stylesheets list.
  mDOMStyleSheets = nsnull;

  mDocumentURI = aURI;
  mDocumentBaseURI = mDocumentURI;

  if (aLoadGroup) {
    mDocumentLoadGroup = do_GetWeakReference(aLoadGroup);
    // there was an assertion here that aLoadGroup was not null.  This
    // is no longer valid nsWebShell::SetDocument does not create a
    // load group, and it works just fine.
  }

  mLastModified.Truncate();
  mContentType.Truncate();
  mContentLanguage.Truncate();
  mBaseTarget.Truncate();
  mReferrer.Truncate();

  mXMLDeclarationBits = 0;
}

nsresult
nsDocument::ResetStylesheetsToURI(nsIURI* aURI)
{
  NS_PRECONDITION(aURI, "Null URI passed to ResetStylesheetsToURI");

  mozAutoDocUpdate(this, UPDATE_STYLE, PR_TRUE);
  
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

  // Release all the sheets
  mStyleSheets.Clear();

  // Now reset our inline style and attribute sheets.  Note that we
  // already set their owning document to null in the loop above, but
  // we'll reset it when we call AddStyleSheet on them.
  nsresult rv;
  nsStyleSet::sheetType attrSheetType = GetAttrSheetType();
  if (mAttrStyleSheet) {
    // Remove this sheet from all style sets
    PRInt32 count = mPresShells.Count();
    for (indx = 0; indx < count; ++indx) {
      NS_STATIC_CAST(nsIPresShell*, mPresShells.ElementAt(indx))->StyleSet()->
        RemoveStyleSheet(attrSheetType, mAttrStyleSheet);
    }
    rv = mAttrStyleSheet->Reset(aURI);
  } else {
    rv = NS_NewHTMLStyleSheet(getter_AddRefs(mAttrStyleSheet), aURI, this);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // Don't use AddStyleSheet, since it'll put the sheet into style
  // sets in the document level, which is not desirable here.
  InternalAddStyleSheet(mAttrStyleSheet, 0);
  mAttrStyleSheet->SetOwningDocument(this);
  
  if (mStyleAttrStyleSheet) {
    rv = mStyleAttrStyleSheet->Reset(aURI);
  } else {
    rv = NS_NewHTMLCSSStyleSheet(getter_AddRefs(mStyleAttrStyleSheet), aURI,
                                                this);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // The loop over style sets below will handle putting this sheet
  // into style sets as needed.
  InternalAddStyleSheet(mStyleAttrStyleSheet, 0);
  mStyleAttrStyleSheet->SetOwningDocument(this);

  // Now set up our style sets
  PRInt32 count = mPresShells.Count();
  for (indx = 0; indx < count; ++indx) {
    FillStyleSet(NS_STATIC_CAST(nsIPresShell*,
                                mPresShells.ElementAt(indx))->StyleSet());
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
  NS_PRECONDITION(mStyleAttrStyleSheet, "No style attr stylesheet?");
  NS_PRECONDITION(mAttrStyleSheet, "No attr stylesheet?");
  NS_PRECONDITION(mAttrStyleSheet == GetStyleSheetAt(0, PR_TRUE),
                  "Unexpected first sheet");
  
  // First, place the attribute style sheet in the style set.  Prepend it,
  // since it should be the most significant sheet in its level.
  aStyleSet->PrependStyleSheet(GetAttrSheetType(), mAttrStyleSheet);

  // Now that we've handled the attribute sheet, do the other sheets.  The
  // attribute sheet should be at position 0, hence the loop termination
  // condition.
  PRInt32 index;
  PRBool sheetApplicable;
  for (index = GetNumberOfStyleSheets(PR_TRUE) - 1; index > 0; --index) {
    nsIStyleSheet* sheet = GetStyleSheetAt(index, PR_TRUE);
    sheet->GetApplicable(sheetApplicable);
    if (sheetApplicable) {
      aStyleSet->AddDocStyleSheet(sheet, this);
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

  return NS_OK;
}

void
nsDocument::StopDocumentLoad()
{
}

NS_IMETHODIMP
nsDocument::GetLastModified(nsAString& aLastModified)
{
  if (!mLastModified.IsEmpty()) {
    CopyASCIItoUCS2(mLastModified, aLastModified);
  } else {
    // If we for whatever reason failed to find the last modified time
    // (or even the current time), fall back to what NS4.x returned.
    CopyASCIItoUCS2(NS_LITERAL_CSTRING("January 1, 1970 GMT"), aLastModified);
  }

  return NS_OK;
}

nsIPrincipal*
nsDocument::GetPrincipal()
{
  if (!mPrincipal) {
    nsresult rv;
    nsCOMPtr<nsIScriptSecurityManager> securityManager =
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv))
      return nsnull;
    NS_WARN_IF_FALSE(mDocumentURI, "no URI!");
    rv = securityManager->GetCodebasePrincipal(mDocumentURI,
                                               getter_AddRefs(mPrincipal));

    if (NS_FAILED(rv)) {
      return nsnull;
    }
  }

  return mPrincipal;
}

void
nsDocument::SetPrincipal(nsIPrincipal *aNewPrincipal)
{
  mPrincipal = aNewPrincipal;
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
               mContentType.Equals(NS_ConvertUCS2toUTF8(aContentType)),
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
    nsIPrincipal* principal = GetPrincipal();
    NS_ENSURE_TRUE(principal, NS_ERROR_FAILURE);
    
    nsIScriptSecurityManager* securityManager =
      nsContentUtils::GetSecurityManager();
    rv = securityManager->
      CheckLoadURIWithPrincipal(principal, aURI,
                                nsIScriptSecurityManager::STANDARD);
    if (NS_SUCCEEDED(rv)) {
      mDocumentBaseURI = aURI;
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
    nsCOMPtr<nsICharsetAlias> calias(do_GetService(kCharsetAliasCID));
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
                        NS_ConvertASCIItoUCS2(aCharSetID).get());
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

nsILineBreaker*
nsDocument::GetLineBreaker()
{
  if (!mLineBreaker) {
    // no line breaker, find a default one
    nsresult rv;
    nsCOMPtr<nsILineBreakerFactory> lbf =
      do_GetService(NS_LWBRK_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, nsnull);

    lbf->GetBreaker(EmptyString(), getter_AddRefs(mLineBreaker));
    NS_ENSURE_TRUE(mLineBreaker, nsnull);
  }

  return mLineBreaker;
}

void
nsDocument::SetLineBreaker(nsILineBreaker* aLineBreaker)
{
  mLineBreaker = aLineBreaker;
}

nsIWordBreaker*
nsDocument::GetWordBreaker()
{
  if (!mWordBreaker) {
    // no word breaker, find a default one
    nsresult rv;
    nsCOMPtr<nsIWordBreakerFactory> wbf =
      do_GetService(NS_LWBRK_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, nsnull);

    wbf->GetBreaker(EmptyString(), getter_AddRefs(mWordBreaker));
    NS_ENSURE_TRUE(wbf, nsnull);
  }

  return mWordBreaker;
}

void
nsDocument::SetWordBreaker(nsIWordBreaker* aWordBreaker)
{
  mWordBreaker = aWordBreaker;
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
    nsAutoString type;
    nsAutoString title;
    PRInt32 index;

    mCSSLoader->SetPreferredSheet(aData);

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
      nsCOMPtr<nsICharsetAlias> calias(do_GetService(kCharsetAliasCID));
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
nsDocument::CreateShell(nsIPresContext* aContext, nsIViewManager* aViewManager,
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
nsDocument::doCreateShell(nsIPresContext* aContext,
                          nsIViewManager* aViewManager, nsStyleSet* aStyleSet,
                          nsCompatibility aCompatMode,
                          nsIPresShell** aInstancePtrResult)
{
  *aInstancePtrResult = nsnull;
  
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
  return mPresShells.Count();
}

nsIPresShell *
nsDocument::GetShellAt(PRUint32 aIndex) const
{
  return (nsIPresShell*)mPresShells.SafeElementAt(aIndex);
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

void
nsDocument::SetRootContent(nsIContent* aRoot)
{
  if (mRootContent) {
    PRInt32 indx = mChildren.IndexOf(mRootContent);
    if (aRoot) {
      mChildren.ReplaceObjectAt(aRoot, indx);
    } else {
      mChildren.RemoveObjectAt(indx);
    }
  } else if (aRoot) {
    mChildren.AppendObject(aRoot);
  }

  mRootContent = aRoot;
}

nsIContent *
nsDocument::GetChildAt(PRUint32 aIndex) const
{
  if (aIndex >= (PRUint32)mChildren.Count()) {
    return nsnull;
  }

  return mChildren[aIndex];
}

PRInt32
nsDocument::IndexOf(nsIContent* aPossibleChild) const
{
  return mChildren.IndexOf(aPossibleChild);
}

PRUint32
nsDocument::GetChildCount() const
{
  return mChildren.Count();
}

PRInt32
nsDocument::InternalGetNumberOfStyleSheets() const
{
  return mStyleSheets.Count();
}

PRInt32
nsDocument::GetNumberOfStyleSheets(PRBool aIncludeSpecialSheets) const
{
  if (aIncludeSpecialSheets) {
    return mStyleSheets.Count();
  }

  return InternalGetNumberOfStyleSheets();
}

nsIStyleSheet*
nsDocument::InternalGetStyleSheetAt(PRInt32 aIndex) const
{
  NS_ASSERTION(aIndex >= 0 && aIndex < mStyleSheets.Count(), "Invalid index");
  return mStyleSheets[aIndex];
}

nsIStyleSheet*
nsDocument::GetStyleSheetAt(PRInt32 aIndex, PRBool aIncludeSpecialSheets) const
{
  if (aIncludeSpecialSheets) {
    if (aIndex < 0 || aIndex >= mStyleSheets.Count()) {
      NS_ERROR("Index out of range");
      return nsnull;
    }

    return mStyleSheets[aIndex];
  }

  return InternalGetStyleSheetAt(aIndex);
}

PRInt32
nsDocument::GetIndexOfStyleSheet(nsIStyleSheet* aSheet) const
{
  return mStyleSheets.IndexOf(aSheet);
}

// subclass hooks for sheet ordering
void
nsDocument::InternalAddStyleSheet(nsIStyleSheet* aSheet, PRUint32 aFlags)
{
  mStyleSheets.AppendObject(aSheet);
}

void
nsDocument::AddStyleSheetToStyleSets(nsIStyleSheet* aSheet)
{
  PRInt32 count = mPresShells.Count();
  PRInt32 indx;
  for (indx = 0; indx < count; ++indx) {
    NS_STATIC_CAST(nsIPresShell*, mPresShells.ElementAt(indx))->StyleSet()->
      AddDocStyleSheet(aSheet, this);
  }
}

void
nsDocument::AddStyleSheet(nsIStyleSheet* aSheet, PRUint32 aFlags)
{
  NS_PRECONDITION(aSheet, "null arg");
  InternalAddStyleSheet(aSheet, aFlags);
  aSheet->SetOwningDocument(this);

  PRBool applicable;
  aSheet->GetApplicable(applicable);

  if (applicable) {
    AddStyleSheetToStyleSets(aSheet);
  }

  // if an observer removes itself, we're ok (not if it removes others though)
  PRInt32 i;
  for (i = mObservers.Count() - 1; i >= 0; --i) {
    nsIDocumentObserver* observer =
      NS_STATIC_CAST(nsIDocumentObserver *, mObservers.ElementAt(i));

    observer->StyleSheetAdded(this, aSheet);
  }
}

void
nsDocument::RemoveStyleSheetFromStyleSets(nsIStyleSheet* aSheet)
{
  PRInt32 count = mPresShells.Count();
  PRInt32 indx;
  for (indx = 0; indx < count; ++indx) {
    NS_STATIC_CAST(nsIPresShell*, mPresShells.ElementAt(indx))->StyleSet()->
      RemoveStyleSheet(nsStyleSet::eDocSheet, aSheet);
  }
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

    // if an observer removes itself, we're ok (not if it removes
    // others though)

    for (PRInt32 indx = mObservers.Count() - 1; indx >= 0; --indx) {
      nsIDocumentObserver *observer =
        NS_STATIC_CAST(nsIDocumentObserver *, mObservers.ElementAt(indx));

      observer->StyleSheetRemoved(this, aSheet);
    }
  }

  aSheet->SetOwningDocument(nsnull);
}

void
nsDocument::UpdateStyleSheets(nsCOMArray<nsIStyleSheet>& aOldSheets,
                              nsCOMArray<nsIStyleSheet>& aNewSheets)
{
  // if an observer removes itself, we're ok (not if it removes
  // others though)
  PRInt32 obsIndx;
  for (obsIndx = mObservers.Count() - 1; obsIndx >= 0; --obsIndx) {
    nsIDocumentObserver *observer =
      NS_STATIC_CAST(nsIDocumentObserver *, mObservers.ElementAt(obsIndx));

    observer->BeginUpdate(this, UPDATE_STYLE);
  }

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

      for (obsIndx = mObservers.Count() - 1; obsIndx >= 0; --obsIndx) {
        nsIDocumentObserver *observer =
          NS_STATIC_CAST(nsIDocumentObserver *, mObservers.ElementAt(obsIndx));
        
        observer->StyleSheetAdded(this, newSheet);
      }
    }
  }

  for (obsIndx = mObservers.Count() - 1; obsIndx >= 0; --obsIndx) {
    nsIDocumentObserver *observer =
      NS_STATIC_CAST(nsIDocumentObserver *, mObservers.ElementAt(obsIndx));

    observer->EndUpdate(this, UPDATE_STYLE);
  }
}


void
nsDocument::InternalInsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex)
{
  // subclass hook for sheet ordering
  mStyleSheets.InsertObjectAt(aSheet, aIndex);
}

void
nsDocument::InsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex)
{
  NS_PRECONDITION(aSheet, "null ptr");
  InternalInsertStyleSheetAt(aSheet, aIndex);

  aSheet->SetOwningDocument(this);

  PRBool applicable;
  aSheet->GetApplicable(applicable);

  if (applicable) {
    AddStyleSheetToStyleSets(aSheet);
  }

  // if an observer removes itself, we're ok (not if it removes others
  // though)
  PRInt32 i;
  for (i = mObservers.Count() - 1; i >= 0; --i) {
    nsIDocumentObserver *observer =
      NS_STATIC_CAST(nsIDocumentObserver *, mObservers.ElementAt(i));

    observer->StyleSheetAdded(this, aSheet);
  }
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

  PRInt32 indx;
  // if an observer removes itself, we're ok (not if it removes others though)
  for (indx = mObservers.Count() - 1; indx >= 0; --indx) {
    nsIDocumentObserver* observer =
      (nsIDocumentObserver*)mObservers.ElementAt(indx);
    observer->StyleSheetApplicableStateChanged(this, aSheet, aApplicable);
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

void
nsDocument::SetScriptGlobalObject(nsIScriptGlobalObject *aScriptGlobalObject)
{
  // XXX HACK ALERT! If the script context owner is null, the document
  // will soon be going away. So tell our content that to lose its
  // reference to the document. This has to be done before we actually
  // set the script context owner to null so that the content elements
  // can remove references to their script objects.
  if (!aScriptGlobalObject) {
    PRInt32 count, indx;

    count = mChildren.Count();

    mIsGoingAway = PR_TRUE;

    for (indx = 0; indx < count; ++indx) {
      mChildren[indx]->SetDocument(nsnull, PR_TRUE, PR_TRUE);
    }

    // Propagate the out-of-band notification to each PresShell's
    // anonymous content as well. This ensures that there aren't any
    // accidental script references left in anonymous content keeping
    // the document alive. (While not strictly necessary -- the
    // PresShell owns us -- it's tidy.)
    for (count = mPresShells.Count() - 1; count >= 0; --count) {
      nsCOMPtr<nsIPresShell> shell =
        NS_STATIC_CAST(nsIPresShell*, mPresShells[count]);
      if (!shell)
        continue;

      shell->ReleaseAnonymousContent();
    }

#ifdef DEBUG_jst
    printf ("Content wrapper hash had %d entries.\n",
            mContentWrapperHash.Count());
#endif

    mContentWrapperHash.Reset();
  }

  mScriptGlobalObject = aScriptGlobalObject;
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
nsDocument::BeginUpdate(nsUpdateType aUpdateType)
{
  PRInt32 i;
  for (i = mObservers.Count() - 1; i >= 0; --i) {
    nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
    observer->BeginUpdate(this, aUpdateType);
  }
}

void
nsDocument::EndUpdate(nsUpdateType aUpdateType)
{
  PRInt32 i;
  for (i = mObservers.Count() - 1; i >= 0; --i) {
    nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
    observer->EndUpdate(this, aUpdateType);
  }
}

void
nsDocument::BeginLoad()
{
  PRInt32 i;
  for (i = mObservers.Count() - 1; i >= 0; --i) {
    nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
    observer->BeginLoad(this);
  }
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
nsDocument::EndLoad()
{
  PRInt32 i;
  for (i = mObservers.Count() - 1; i >= 0; --i) {
    nsIDocumentObserver* observer = (nsIDocumentObserver*)mObservers[i];
    observer->EndLoad(this);
  }

  // Fire a DOM event notifying listeners that this document has been
  // loaded (excluding images and other loads initiated by this
  // document).
  nsCOMPtr<nsIDOMEvent> event;
  CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));

  if (event) {
    event->InitEvent(NS_LITERAL_STRING("DOMContentLoaded"), PR_TRUE, PR_TRUE);
    PRBool noDefault;
    DispatchEvent(event, &noDefault);
  }

  // If this document is a [i]frame, fire a DOMFrameContentLoaded
  // event on all parent documents notifying that the HTML (excluding
  // other external files such as images and stylesheets) in a frame
  // has finished loading.

  nsCOMPtr<nsIDocShellTreeItem> docShellParent;

  // target_frame is the [i]frame element that will be used as the
  // target for the event. It's the [i]frame whose content is done
  // loading.
  nsCOMPtr<nsIDOMEventTarget> target_frame;

  if (mScriptGlobalObject) {
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem =
      do_QueryInterface(mScriptGlobalObject->GetDocShell());

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

      nsCOMPtr<nsIPrivateDOMEvent> private_event;

      nsCOMPtr<nsIDOMDocumentEvent> document_event =
        do_QueryInterface(ancestor_doc);

      if (document_event) {
        document_event->CreateEvent(NS_LITERAL_STRING("Events"),
                                    getter_AddRefs(event));

        private_event = do_QueryInterface(event);
      }

      if (event && private_event) {
        event->InitEvent(NS_LITERAL_STRING("DOMFrameContentLoaded"), PR_TRUE,
                         PR_TRUE);

        private_event->SetTarget(target_frame);

        // To dispatch this event we must manually call
        // HandleDOMEvent() on the ancestor document since the target
        // is not in the same document, so the event would never reach
        // the ancestor document if we used the normal event
        // dispatching code.

        nsEvent* innerEvent;
        private_event->GetInternalNSEvent(&innerEvent);
        if (innerEvent) {
          nsEventStatus status = nsEventStatus_eIgnore;

          nsIPresShell *shell = ancestor_doc->GetShellAt(0);
          if (shell) {
            nsCOMPtr<nsIPresContext> context;
            shell->GetPresContext(getter_AddRefs(context));

            if (context) {
              // The event argument to HandleDOMEvent() is inout, and
              // that doesn't mix well with nsCOMPtr's. We'll need to
              // perform some refcounting magic here.
              nsIDOMEvent *tmp_event = event;
              NS_ADDREF(tmp_event);

              ancestor_doc->HandleDOMEvent(context, innerEvent, &tmp_event,
                                           NS_EVENT_FLAG_INIT, &status);

              NS_IF_RELEASE(tmp_event);
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
nsDocument::CharacterDataChanged(nsIContent* aContent, PRBool aAppend)
{
  NS_ABORT_IF_FALSE(aContent, "Null content!");

  PRInt32 i;
  for (i = mObservers.Count() - 1; i >= 0; --i) {
    nsIDocumentObserver *observer =
      NS_STATIC_CAST(nsIDocumentObserver *, mObservers.ElementAt(i));

    observer->CharacterDataChanged(this, aContent, aAppend);
  }
}

void
nsDocument::ContentStatesChanged(nsIContent* aContent1, nsIContent* aContent2,
                                 PRInt32 aStateMask)
{
  PRInt32 i;
  for (i = mObservers.Count() - 1; i >= 0; --i) {
    nsIDocumentObserver *observer =
      NS_STATIC_CAST(nsIDocumentObserver *, mObservers.ElementAt(i));

    observer->ContentStatesChanged(this, aContent1, aContent2, aStateMask);
  }
}


void
nsDocument::ContentAppended(nsIContent* aContainer,
                            PRInt32 aNewIndexInContainer)
{
  NS_ABORT_IF_FALSE(aContainer, "Null container!");

  PRInt32 i;

  // XXXdwh There is a hacky ordering dependency between the binding
  // manager and the frame constructor that forces us to walk the
  // observer list in a forward order
  for (i = 0; i < mObservers.Count(); i++) {
    nsIDocumentObserver *observer =
      NS_STATIC_CAST(nsIDocumentObserver *, mObservers.ElementAt(i));

    observer->ContentAppended(this, aContainer, aNewIndexInContainer);
    // Make sure that the observer didn't remove itself during the
    // notification. If it did, update our index
    if (i < mObservers.Count() &&
        observer != (nsIDocumentObserver*)mObservers[i]) {
      i--;
    }
  }
}

void
nsDocument::ContentInserted(nsIContent* aContainer, nsIContent* aChild,
                            PRInt32 aIndexInContainer)
{
  NS_ABORT_IF_FALSE(aChild, "Null child!");

  PRInt32 i;
  // XXXdwh There is a hacky ordering dependency between the binding manager
  // and the frame constructor that forces us to walk the observer list
  // in a forward order
  for (i = 0; i < mObservers.Count(); i++) {
    nsIDocumentObserver *observer =
      NS_STATIC_CAST(nsIDocumentObserver *, mObservers.ElementAt(i));

    observer->ContentInserted(this, aContainer, aChild, aIndexInContainer);
    // Make sure that the observer didn't remove itself during the
    // notification. If it did, update our index.
    if (i < mObservers.Count() &&
        observer != (nsIDocumentObserver*)mObservers[i]) {
      i--;
    }
  }
}

void
nsDocument::ContentRemoved(nsIContent* aContainer, nsIContent* aChild,
                           PRInt32 aIndexInContainer)
{
  NS_ABORT_IF_FALSE(aChild, "Null child!");

  PRInt32 i;
  // XXXdwh There is a hacky ordering dependency between the binding
  // manager and the frame constructor that forces us to walk the
  // observer list in a reverse order
  for (i = mObservers.Count() - 1; i >= 0; --i) {
    nsIDocumentObserver* observer = (nsIDocumentObserver*)mObservers[i];
    observer->ContentRemoved(this, aContainer,
                             aChild, aIndexInContainer);
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

  PRInt32 i;
  for (i = mObservers.Count() - 1; i >= 0; --i) {
    nsIDocumentObserver *observer =
      NS_STATIC_CAST(nsIDocumentObserver *, mObservers.ElementAt(i));

    observer->AttributeChanged(this, aChild, aNameSpaceID,
                               aAttribute, aModType);
  }
}


void
nsDocument::StyleRuleChanged(nsIStyleSheet* aStyleSheet,
                             nsIStyleRule* aOldStyleRule,
                             nsIStyleRule* aNewStyleRule)
{
  PRInt32 i;

  // Loop backwards to handle observers removing themselves
  for (i = mObservers.Count() - 1; i >= 0; --i) {
    nsIDocumentObserver *observer =
      NS_STATIC_CAST(nsIDocumentObserver *, mObservers.ElementAt(i));

    observer->StyleRuleChanged(this, aStyleSheet, aOldStyleRule, aNewStyleRule);
  }
}

void
nsDocument::StyleRuleAdded(nsIStyleSheet* aStyleSheet,
                           nsIStyleRule* aStyleRule)
{
  PRInt32 i;

  // Loop backwards to handle observers removing themselves
  for (i = mObservers.Count() - 1; i >= 0; --i) {
    nsIDocumentObserver *observer =
      NS_STATIC_CAST(nsIDocumentObserver *, mObservers.ElementAt(i));

    observer->StyleRuleAdded(this, aStyleSheet, aStyleRule);
  }
}

void
nsDocument::StyleRuleRemoved(nsIStyleSheet* aStyleSheet,
                             nsIStyleRule* aStyleRule)
{
  PRInt32 i;

  // Loop backwards to handle observers removing themselves
  for (i = mObservers.Count() - 1; i >= 0; --i) {
    nsIDocumentObserver *observer =
      NS_STATIC_CAST(nsIDocumentObserver *, mObservers.ElementAt(i));

    observer->StyleRuleRemoved(this, aStyleSheet, aStyleRule);
  }
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
  count = mChildren.Count();
  nsCOMPtr<nsIDOMNode> rootContentNode(do_QueryInterface(mRootContent) );
  nsCOMPtr<nsIDOMNode> node;

  for (i = 0; i < count; i++) {
    node = do_QueryInterface(mChildren[i]);

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
  *aImplementation = new nsDOMImplementation(mDocumentURI);
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
  nsresult rv = NS_NewTextNode(getter_AddRefs(text));

  if (NS_SUCCEEDED(rv)) {
    rv = CallQueryInterface(text, aReturn);
    (*aReturn)->AppendData(aData);
  }

  return rv;
}

NS_IMETHODIMP
nsDocument::CreateDocumentFragment(nsIDOMDocumentFragment** aReturn)
{
  return NS_NewDocumentFragment(aReturn, this);
}

NS_IMETHODIMP
nsDocument::CreateComment(const nsAString& aData, nsIDOMComment** aReturn)
{
  *aReturn = nsnull;

  nsCOMPtr<nsIContent> comment;
  nsresult rv = NS_NewCommentNode(getter_AddRefs(comment));

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
  nsresult rv = NS_NewXMLCDATASection(getter_AddRefs(content));

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
  rv = NS_NewXMLProcessingInstruction(getter_AddRefs(content), aTarget, aData);
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

  nsCOMPtr<nsIContentList> list;
  NS_GetContentList(this, nameAtom, kNameSpaceID_Unknown, nsnull,
                    getter_AddRefs(list));
  NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);

  return CallQueryInterface(list, aReturn);
}

NS_IMETHODIMP
nsDocument::GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                                   const nsAString& aLocalName,
                                   nsIDOMNodeList** aReturn)
{
  PRInt32 nameSpaceId = kNameSpaceID_Unknown;

  nsCOMPtr<nsIContentList> list;

  if (!aNamespaceURI.EqualsLiteral("*")) {
    nsContentUtils::GetNSManagerWeakRef()->GetNameSpaceID(aNamespaceURI,
                                                          &nameSpaceId);

    if (nameSpaceId == kNameSpaceID_Unknown) {
      // Unknown namespace means no matches, we create an empty list...
      NS_GetContentList(this, nsnull, kNameSpaceID_None, nsnull,
                        getter_AddRefs(list));
      NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);
    }
  }

  if (!list) {
    nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(aLocalName);
    NS_ENSURE_TRUE(nameAtom, NS_ERROR_OUT_OF_MEMORY);

    NS_GetContentList(this, nameAtom, nameSpaceId, nsnull,
                      getter_AddRefs(list));
    NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);
  }

  return CallQueryInterface(list, aReturn);
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
nsDocument::GetCharacterSet(nsAString& aCharacterSet)
{
  CopyASCIItoUCS2(GetDocumentCharacterSet(), aCharacterSet);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::ImportNode(nsIDOMNode* aImportedNode,
                       PRBool aDeep,
                       nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG(aImportedNode);
  NS_PRECONDITION(aReturn, "Null out param!");

  nsresult rv = nsContentUtils::CheckSameOrigin(this, aImportedNode);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return aImportedNode->CloneNode(aDeep, aReturn);
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

  if (mBindingManager) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aContent));
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), aURI);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return mBindingManager->RemoveLayeredBinding(content, uri);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocument::LoadBindingDocument(const nsAString& aURI,
                                nsIDOMDocument** aResult)
{
  if (!mBindingManager) {
    return NS_ERROR_FAILURE;
  }

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
  nsAutoString value;
  nsresult rv = aContent->GetAttr(kNameSpaceID_None, aAttrName, value);
  if (rv == NS_CONTENT_ATTR_HAS_VALUE &&
      (aUniversalMatch || value.Equals(aAttrValue))) {
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
  if (!mBindingManager) {
    return NS_OK;
  }

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
  NS_ENSURE_ARG_POINTER(aDefaultView);
  *aDefaultView = nsnull;

  NS_ENSURE_TRUE(mPresShells.Count() != 0, NS_OK);
  nsCOMPtr<nsIPresShell> shell = NS_STATIC_CAST(nsIPresShell *,
                                                mPresShells.ElementAt(0));
  NS_ENSURE_TRUE(shell, NS_OK);

  nsCOMPtr<nsIPresContext> ctx;
  nsresult rv = shell->GetPresContext(getter_AddRefs(ctx));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && ctx, rv);

  nsCOMPtr<nsISupports> container = ctx->GetContainer();
  NS_ENSURE_TRUE(container, NS_OK);

  nsCOMPtr<nsIDOMWindowInternal> window = do_GetInterface(container);
  NS_ENSURE_TRUE(window, NS_OK);

  CallQueryInterface(window, aDefaultView);

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

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::SetTitle(const nsAString& aTitle)
{
  for (PRInt32 i = mPresShells.Count() - 1; i >= 0; --i) {
    nsCOMPtr<nsIPresShell> shell =
      NS_STATIC_CAST(nsIPresShell*, mPresShells[i]);

    nsCOMPtr<nsIPresContext> context;
    nsresult rv = shell->GetPresContext(getter_AddRefs(context));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISupports> container = context->GetContainer();
    if (!container)
      continue;

    nsCOMPtr<nsIBaseWindow> docShellWin = do_QueryInterface(container);
    if (!docShellWin)
      continue;

    rv = docShellWin->SetTitle(PromiseFlatString(aTitle).get());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mDocumentTitle.Assign(aTitle);

  // Fire a DOM event for the title change.
  nsCOMPtr<nsIDOMEvent> event;
  CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
  if (event) {
    event->InitEvent(NS_LITERAL_STRING("DOMTitleChanged"), PR_TRUE, PR_TRUE);
    PRBool noDefault;
    DispatchEvent(event, &noDefault);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetBoxObjectFor(nsIDOMElement* aElement, nsIBoxObject** aResult)
{
  NS_ENSURE_ARG(aElement);
  
  nsresult rv;

  *aResult = nsnull;

  if (!mBoxObjectTable) {
    mBoxObjectTable = new nsSupportsHashtable;
  } else {
    nsISupportsKey key(aElement);
    nsCOMPtr<nsISupports> supports = dont_AddRef(mBoxObjectTable->Get(&key));

    nsCOMPtr<nsIBoxObject> boxObject(do_QueryInterface(supports));
    if (boxObject) {
      *aResult = boxObject;
      NS_ADDREF(*aResult);

      return NS_OK;
    }
  }

  nsIPresShell *shell = GetShellAt(0);
  if (!shell)
    return NS_ERROR_FAILURE;

  PRInt32 namespaceID;
  nsCOMPtr<nsIAtom> tag;
  nsCOMPtr<nsIXBLService> xblService =
           do_GetService("@mozilla.org/xbl;1", &rv);
  nsCOMPtr<nsIContent> content(do_QueryInterface(aElement));
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

  nsCOMPtr<nsIBoxObject> boxObject(do_CreateInstance(contractID.get()));
  if (!boxObject)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsPIBoxObject> privateBox(do_QueryInterface(boxObject));
  rv = privateBox->Init(content, shell);

  if (NS_FAILED(rv)) {
    return rv;
  }

  SetBoxObjectFor(aElement, boxObject);

  *aResult = boxObject;
  NS_ADDREF(*aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::SetBoxObjectFor(nsIDOMElement* aElement, nsIBoxObject* aBoxObject)
{
  if (!mBoxObjectTable) {
    if (!aBoxObject)
      return NS_OK;
    mBoxObjectTable = new nsSupportsHashtable(12);
  }

  nsISupportsKey key(aElement);

  if (aBoxObject) {
    mBoxObjectTable->Put(&key, aBoxObject);
  } else {
    nsCOMPtr<nsISupports> supp;
    mBoxObjectTable->Remove(&key, getter_AddRefs(supp));
    nsCOMPtr<nsPIBoxObject> boxObject(do_QueryInterface(supp));
    if (boxObject) {
      boxObject->SetDocument(nsnull);
    }
  }

  return NS_OK;
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
  nsCOMPtr<nsIPresShell> shell = (nsIPresShell*)mPresShells.SafeElementAt(0);
  if (shell) {
    nsCOMPtr<nsIPresContext> context;
    shell->GetPresContext(getter_AddRefs(context));
    if (context) {
      PRUint32 options;
      context->GetBidi(&options);
      for (const DirTable* elt = dirAttributes; elt->mName; elt++) {
        if (GET_BIDI_OPTION_DIRECTION(options) == elt->mValue) {
          CopyASCIItoUTF16(elt->mName, aDirection);
          break;
        }
      }
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
  nsIPresShell *shell =
    NS_STATIC_CAST(nsIPresShell *, mPresShells.SafeElementAt(0));

  if (!shell) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresContext> context;
  shell->GetPresContext(getter_AddRefs(context));
  NS_ENSURE_TRUE(context, NS_ERROR_UNEXPECTED);

  PRUint32 options;
  context->GetBidi(&options);

  for (const DirTable* elt = dirAttributes; elt->mName; elt++) {
    if (aDirection == NS_ConvertASCIItoUCS2(elt->mName)) {
      if (GET_BIDI_OPTION_DIRECTION(options) != elt->mValue) {
        SET_BIDI_OPTION_DIRECTION(options, elt->mValue);
        context->SetBidi(options, PR_TRUE);
      }

      break;
    }
  }

  return NS_OK;
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
    mChildNodes = new nsDocumentChildNodes(this);
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

  *aHasChildNodes = (mChildren.Count() != 0);

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
  if (mChildren.Count()) {
    return CallQueryInterface(mChildren[0], aFirstChild);
  }

  *aFirstChild = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetLastChild(nsIDOMNode** aLastChild)
{
  PRInt32 count = mChildren.Count();
  if (count) {
    return CallQueryInterface(mChildren[count-1], aLastChild);
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

nsresult
nsDocument::IsAllowedAsChild(PRUint16 aNodeType, nsIContent* aRefContent)
{
  if (aNodeType != nsIDOMNode::COMMENT_NODE &&
      aNodeType != nsIDOMNode::ELEMENT_NODE &&
      aNodeType != nsIDOMNode::PROCESSING_INSTRUCTION_NODE &&
      aNodeType != nsIDOMNode::DOCUMENT_TYPE_NODE) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  if (aNodeType == nsIDOMNode::ELEMENT_NODE && mRootContent &&
      mRootContent != aRefContent) {
    // We already have a child Element, and we're not trying to
    // replace it, so throw an error.
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  if (aNodeType == nsIDOMNode::DOCUMENT_TYPE_NODE) {
    nsCOMPtr<nsIDOMDocumentType> docType;
    GetDoctype(getter_AddRefs(docType));

    nsCOMPtr<nsIContent> docTypeContent = do_QueryInterface(docType);
    if (docTypeContent && docTypeContent != aRefContent) {
      // We already have a doctype, and we're not trying to
      // replace it, so throw an error.
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                         nsIDOMNode** aReturn)
{
  *aReturn = nsnull;

  NS_ENSURE_ARG(aNewChild);

  nsresult rv = nsContentUtils::CheckSameOrigin(this, aNewChild);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRUint16 nodeType;
  aNewChild->GetNodeType(&nodeType);

  rv = IsAllowedAsChild(nodeType, nsnull);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(aNewChild);
  if (!content) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  PRInt32 indx;
  if (!aRefChild) {
    if (nodeType == nsIDOMNode::DOCUMENT_TYPE_NODE && mRootContent) {
      // docType needs to be before documentElement
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }

    indx = mChildren.Count();
    mChildren.AppendObject(content);
  }
  else {
    nsCOMPtr<nsIContent> refContent(do_QueryInterface(aRefChild));
    if (!refContent) {
      return NS_ERROR_DOM_NOT_FOUND_ERR;
    }

    indx = mChildren.IndexOf(refContent);
    if (indx == -1) {
      // couldn't find refChild
      return NS_ERROR_DOM_NOT_FOUND_ERR;
    }

    if (nodeType == nsIDOMNode::DOCUMENT_TYPE_NODE && mRootContent &&
        indx > mChildren.IndexOf(mRootContent)) {
      // docType needs to be before documentElement
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }

    mChildren.InsertObjectAt(content, indx);
  }

  // If we get here, we've succesfully inserted content into the
  // index-th spot in mChildren.
  if (nodeType == nsIDOMNode::ELEMENT_NODE) {
    mRootContent = content;
  }

  content->SetDocument(this, PR_TRUE, PR_TRUE);
  ContentInserted(nsnull, content, indx);

  NS_ADDREF(*aReturn = aNewChild);

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                         nsIDOMNode** aReturn)
{
  *aReturn = nsnull;

  NS_ENSURE_ARG(aNewChild && aOldChild);

  nsCOMPtr<nsIContent> refContent(do_QueryInterface(aOldChild));
  if (!refContent) {
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(aNewChild);
  if (!content) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  nsresult rv = nsContentUtils::CheckSameOrigin(this, aNewChild);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRUint16 nodeType;
  aNewChild->GetNodeType(&nodeType);

  rv = IsAllowedAsChild(nodeType, refContent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRInt32 indx = mChildren.IndexOf(refContent);
  if (indx == -1) {
    // The reference child is not a child of the document.
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  if (nodeType == nsIDOMNode::DOCUMENT_TYPE_NODE && mRootContent &&
      indx > mChildren.IndexOf(mRootContent)) {
    // docType needs to be before documentElement
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  ContentRemoved(nsnull, refContent, indx);
  refContent->SetDocument(nsnull, PR_TRUE, PR_TRUE);

  mChildren.ReplaceObjectAt(content, indx);
  // This is OK because we checked above.
  if (nodeType == nsIDOMNode::ELEMENT_NODE) {
    mRootContent = content;
  }

  content->SetDocument(this, PR_TRUE, PR_TRUE);
  ContentInserted(nsnull, content, indx);

  NS_ADDREF(*aReturn = aNewChild);

  return rv;
}

NS_IMETHODIMP
nsDocument::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  *aReturn = nsnull; // do we need to do this?

  NS_ENSURE_TRUE(aOldChild, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIContent> content(do_QueryInterface(aOldChild));
  if (!content) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  PRInt32 indx = mChildren.IndexOf(content);
  if (indx == -1) {
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  ContentRemoved(nsnull, content, indx);

  mChildren.RemoveObjectAt(indx);
  if (content == mRootContent)
    mRootContent = nsnull;

  content->SetDocument(nsnull, PR_TRUE, PR_TRUE);

  *aReturn = aOldChild;
  NS_ADDREF(aOldChild);

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
  return InsertBefore(aNewChild, nsnull, aReturn);
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
  PRInt32 count = mChildren.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mChildren[i]));

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
  return nsGenericElement::InternalIsSupported(aFeature, aVersion, aReturn);
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
  NS_PRECONDITION(aReturn, "Must have an out parameter");

  if (this == aOther) {
    // If the two nodes being compared are the same node,
    // then no flags are set on the return.
    *aReturn = 0;

    return NS_OK;
  }

  PRUint16 mask = 0;

  nsCOMPtr<nsIContent> otherContent(do_QueryInterface(aOther));
  if (!otherContent) {
    PRUint16 otherNodeType = 0;
    aOther->GetNodeType(&otherNodeType);
    NS_ASSERTION(otherNodeType == nsIDOMNode::DOCUMENT_NODE ||
                 otherNodeType == nsIDOMNode::ATTRIBUTE_NODE,
                 "Hmm, this really _should_ support nsIContent...");
    if (otherNodeType == nsIDOMNode::ATTRIBUTE_NODE) {
      nsCOMPtr<nsIDOMAttr> otherAttr(do_QueryInterface(aOther));
      NS_ASSERTION(otherAttr, "Attributes really should be supporting "
                              "nsIDOMAttr you know...");

      nsCOMPtr<nsIDOMElement> otherOwnerEl;
      otherAttr->GetOwnerElement(getter_AddRefs(otherOwnerEl));
      if (otherOwnerEl) {
        // Documents have no direct relationship to attribute
        // nodes.  So we'll look at our relationship in relation
        // to its owner element, since that is also our relation
        // to the attribute.
        return CompareDocumentPosition(otherOwnerEl, aReturn);
      }
    }

    // If there is no common container node, then the order
    // is based upon order between the root container of each
    // node that is in no container. In this case, the result
    // is disconnected and implementation-dependent.
    mask |= (nsIDOM3Node::DOCUMENT_POSITION_DISCONNECTED |
             nsIDOM3Node::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC);

    *aReturn = mask;
    return NS_OK;
  }

  if (this == otherContent->GetDocument()) {
    // If the node being compared is contained by our node,
    // then it follows it.
    mask |= (nsIDOM3Node::DOCUMENT_POSITION_CONTAINED_BY |
             nsIDOM3Node::DOCUMENT_POSITION_FOLLOWING);
  }
  else {
    // If there is no common container node, then the order
    // is based upon order between the root container of each
    // node that is in no container. In this case, the result
    // is disconnected and implementation-dependent.
    mask |= (nsIDOM3Node::DOCUMENT_POSITION_DISCONNECTED |
             nsIDOM3Node::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC);
  }

  *aReturn = mask;

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
  NS_NOTYETIMPLEMENTED("nsDocument::IsDefaultNamespace()");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::GetFeature(const nsAString& aFeature,
                       const nsAString& aVersion,
                       nsISupports** aReturn)
{
  NS_NOTYETIMPLEMENTED("nsDocument::GetFeature()");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::SetUserData(const nsAString& aKey,
                        nsIVariant* aData,
                        nsIDOMUserDataHandler* aHandler,
                        nsIVariant** aReturn)
{
  NS_NOTYETIMPLEMENTED("nsDocument::SetUserData()");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::GetUserData(const nsAString& aKey,
                        nsIVariant** aReturn)
{
  NS_NOTYETIMPLEMENTED("nsDocument::GetUserData()");

  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsDocument::LookupPrefix(const nsAString& aNamespaceURI,
                         nsAString& aPrefix)
{
  aPrefix.Truncate();

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::LookupNamespaceURI(const nsAString& aNamespacePrefix,
                               nsAString& aNamespaceURI)
{
  aNamespaceURI.Truncate();

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetActualEncoding(nsAString& aActualEncoding)
{
  return GetCharacterSet(aActualEncoding);
}

NS_IMETHODIMP
nsDocument::GetXmlEncoding(nsAString& aXmlEncoding)
{
  if (mXMLDeclarationBits & XML_DECLARATION_BITS_DECLARATION_EXISTS &&
      mXMLDeclarationBits & XML_DECLARATION_BITS_ENCODING_EXISTS) {
    // XXX We don't store the encoding given in the xml declaration.
    // For now, just output the actualEncoding which we do store.
    GetActualEncoding(aXmlEncoding);
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

  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}


NS_IMETHODIMP
nsDocument::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  *aOwnerDocument = nsnull;

  return NS_OK;
}

nsresult
nsDocument::GetListenerManager(nsIEventListenerManager **aInstancePtrResult)
{
  if (mListenerManager) {
    *aInstancePtrResult = mListenerManager;
    NS_ADDREF(*aInstancePtrResult);

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
  PRBool noDefault;

  return DispatchEvent(aEvent, &noDefault);
}

NS_IMETHODIMP
nsDocument::GetSystemEventGroup(nsIDOMEventGroup **aGroup)
{
  nsCOMPtr<nsIEventListenerManager> manager;
  if (NS_SUCCEEDED(GetListenerManager(getter_AddRefs(manager))) && manager) {
    return manager->GetSystemEventGroupLM(aGroup);
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsDocument::HandleDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent,
                           nsIDOMEvent** aDOMEvent, PRUint32 aFlags,
                           nsEventStatus* aEventStatus)
{
  nsresult mRet = NS_OK;
  PRBool externalDOMEvent = PR_FALSE;

  nsIDOMEvent* domEvent = nsnull;

  if (NS_EVENT_FLAG_INIT & aFlags) {
    if (aDOMEvent) {
      if (*aDOMEvent) {
        externalDOMEvent = PR_TRUE;
      }
    }
    else {
      aDOMEvent = &domEvent;
    }
    aEvent->flags |= aFlags;
    aFlags &= ~(NS_EVENT_FLAG_CANT_BUBBLE | NS_EVENT_FLAG_CANT_CANCEL);
    aFlags |= NS_EVENT_FLAG_BUBBLE | NS_EVENT_FLAG_CAPTURE;
  }

  // Capturing stage
  if (NS_EVENT_FLAG_CAPTURE & aFlags && nsnull != mScriptGlobalObject) {
    mScriptGlobalObject->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                        aFlags & NS_EVENT_CAPTURE_MASK,
                                        aEventStatus);
  }

  // Local handling stage
  // Check for null mDOMSlots or ELM, check if we're a non-bubbling
  // event in the bubbling state (bubbling state is indicated by the
  // presence of the NS_EVENT_FLAG_BUBBLE flag and not the
  // NS_EVENT_FLAG_INIT).
  if (mListenerManager &&
      !(NS_EVENT_FLAG_CANT_BUBBLE & aEvent->flags &&
        NS_EVENT_FLAG_BUBBLE & aFlags && !(NS_EVENT_FLAG_INIT & aFlags))) {
    aEvent->flags |= aFlags;
    mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, this,
                                  aFlags, aEventStatus);
    aEvent->flags &= ~aFlags;
  }

  // Bubbling stage
  if (NS_EVENT_FLAG_BUBBLE & aFlags && nsnull != mScriptGlobalObject) {
    mScriptGlobalObject->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                        aFlags & NS_EVENT_BUBBLE_MASK,
                                        aEventStatus);
  }

  if (NS_EVENT_FLAG_INIT & aFlags) {
    // We're leaving the DOM event loop so if we created a DOM event,
    // release here.
    if (*aDOMEvent && !externalDOMEvent) {
      nsrefcnt rc;
      NS_RELEASE2(*aDOMEvent, rc);
      if (0 != rc) {
        // Okay, so someone in the DOM loop (a listener, JS object)
        // still has a ref to the DOM Event but the internal data
        // hasn't been malloc'd.  Force a copy of the data here so the
        // DOM Event is still valid.
        nsCOMPtr<nsIPrivateDOMEvent> privateEvent =
          do_QueryInterface(*aDOMEvent);
        if (privateEvent) {
          privateEvent->DuplicatePrivateData();
        }
      }
      aDOMEvent = nsnull;
    }
  }

  return mRet;
}

nsresult
nsDocument::AddEventListenerByIID(nsIDOMEventListener *aListener,
                                  const nsIID& aIID)
{
  nsCOMPtr<nsIEventListenerManager> manager;

  GetListenerManager(getter_AddRefs(manager));
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
  return AddGroupedEventListener(aType, aListener, aUseCapture, nsnull);
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
  PRInt32 count = GetNumberOfShells();
  if (count == 0)
    return NS_OK;

  nsIPresShell *shell = GetShellAt(0);
  if (!shell)
    return NS_ERROR_FAILURE;

  // Retrieve the context
  nsCOMPtr<nsIPresContext> presContext;
  shell->GetPresContext(getter_AddRefs(presContext));

  return presContext->EventStateManager()->
    DispatchNewEvent(NS_STATIC_CAST(nsIDOMDocument*, this), aEvent, _retval);
}

NS_IMETHODIMP
nsDocument::AddGroupedEventListener(const nsAString& aType,
                                    nsIDOMEventListener *aListener,
                                    PRBool aUseCapture,
                                    nsIDOMEventGroup *aEvtGrp)
{
  nsCOMPtr<nsIEventListenerManager> manager;

  nsresult rv = GetListenerManager(getter_AddRefs(manager));
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
nsDocument::CreateEvent(const nsAString& aEventType, nsIDOMEvent** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  // Obtain a presentation context

  nsIPresShell *shell = GetShellAt(0);
  nsCOMPtr<nsIPresContext> presContext;

  if (shell) {
    // Retrieve the context
    shell->GetPresContext(getter_AddRefs(presContext));
  }

  nsCOMPtr<nsIEventListenerManager> manager;
  GetListenerManager(getter_AddRefs(manager));
  if (manager) {
    return manager->CreateEvent(presContext, nsnull, aEventType, aReturn);
  }

  return NS_ERROR_FAILURE;
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
  if (aType == (aType & (Flush_Content | Flush_SinkNotifications)) ||
      !mScriptGlobalObject) {
    // Nothing to do here
    return;
  }

  // We should be able to replace all this nsIDocShell* code with code
  // that uses mParentDocument, but mParentDocument is never set in
  // the current code!

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem =
    do_QueryInterface(mScriptGlobalObject->GetDocShell());

  if (docShellAsItem) {
    nsCOMPtr<nsIDocShellTreeItem> docShellParent;
    docShellAsItem->GetSameTypeParent(getter_AddRefs(docShellParent));

    nsCOMPtr<nsIDOMWindow> win(do_GetInterface(docShellParent));

    if (win) {
      nsCOMPtr<nsIDOMDocument> dom_doc;
      win->GetDocument(getter_AddRefs(dom_doc));

      nsCOMPtr<nsIDocument> doc(do_QueryInterface(dom_doc));

      if (doc) {
        // If we have a parent we must flush the parent too to ensure
        // that our container is reflown if its size was changed.
        doc->FlushPendingNotifications(aType);
      }
    }
  }

  PRInt32 i, count = mPresShells.Count();

  for (i = 0; i < count; i++) {
    nsCOMPtr<nsIPresShell> shell =
      NS_STATIC_CAST(nsIPresShell*, mPresShells[i]);

    if (shell) {
      shell->FlushPendingNotifications(aType);
    }
  }
}

void
nsDocument::AddReference(void *aKey, nsISupports *aReference)
{
  nsVoidKey key(aKey);

  if (mScriptGlobalObject) {
    mContentWrapperHash.Put(&key, aReference);
  }
}

already_AddRefed<nsISupports>
nsDocument::RemoveReference(void *aKey)
{
  nsVoidKey key(aKey);

  nsISupports* oldReference;
  mContentWrapperHash.Remove(&key, &oldReference);
  return oldReference;
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
nsDocument::SetXMLDeclaration(const nsAString& aVersion,
                              const nsAString& aEncoding,
                              const nsAString& aStandalone)
{
  if (aVersion.IsEmpty()) {
    mXMLDeclarationBits = 0;
    return;
  }

  mXMLDeclarationBits = XML_DECLARATION_BITS_DECLARATION_EXISTS;

  if (!aEncoding.IsEmpty()) {
    mXMLDeclarationBits |= XML_DECLARATION_BITS_ENCODING_EXISTS;
  }

  if (aStandalone.EqualsLiteral("yes")) {
    mXMLDeclarationBits |= XML_DECLARATION_BITS_STANDALONE_EXISTS |
                           XML_DECLARATION_BITS_STANDALONE_YES;
  } else if (aStandalone.EqualsLiteral("no")) {
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

  nsIPrincipal* principal = GetPrincipal();
  NS_ENSURE_TRUE(principal, PR_TRUE);

  nsIScriptGlobalObject* globalObject = GetScriptGlobalObject();
  NS_ENSURE_TRUE(globalObject, PR_TRUE);

  nsIScriptContext *scriptContext = globalObject->GetContext();
  NS_ENSURE_TRUE(scriptContext, PR_TRUE);

  JSContext* cx = (JSContext *) scriptContext->GetNativeContext();
  NS_ENSURE_TRUE(cx, PR_TRUE);

  PRBool enabled;
  nsresult rv = sm->CanExecuteScripts(cx, principal, &enabled);
  NS_ENSURE_SUCCESS(rv, PR_TRUE);
  return enabled;
}

nsresult
nsDocument::GetRadioGroup(const nsAString& aName,
                          nsRadioGroupStruct **aRadioGroup)
{
  nsStringKey key(aName);
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
                           nsIRadioVisitor* aVisitor)
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
    rv = httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("last-modified"),
                                        mLastModified);

    if (NS_FAILED(rv)) {
      mLastModified.Truncate();
    }

    // The misspelled key 'referer' is as per the HTTP spec
    rv = httpChannel->GetRequestHeader(NS_LITERAL_CSTRING("referer"),
                                       mReferrer);
    if (NS_FAILED(rv)) {
      mReferrer.Truncate();
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
    }
  }

  if (mLastModified.IsEmpty() && LL_IS_ZERO(modDate)) {
    // We got nothing from our attempt to ask nsIFileChannel and
    // nsIHttpChannel for the last modified time. Return the current
    // time.
    modDate = PR_Now();
  }

  if (LL_NE(modDate, LL_ZERO)) {
    PRExplodedTime prtime;
    char buf[100];

    PR_ExplodeTime(modDate, PR_LocalTimeParameters, &prtime);

    // Use '%#c' for windows, because '%c' is backward-compatible and
    // non-y2k with msvc; '%#c' requests that a full year be used in the
    // result string.  Other OSes just use "%c".
    PR_FormatTime(buf, sizeof buf,
#ifdef XP_WIN
                  "%#c",
#else
                  "%c",
#endif
                  &prtime);
    mLastModified.Assign(buf);
  }
}

nsresult
nsDocument::CreateElem(nsIAtom *aName, nsIAtom *aPrefix, PRInt32 aNamespaceID,
                       PRBool aDocumentDefaultType, nsIContent **aResult)
{
  *aResult = nsnull;
  
  PRInt32 elementType = aDocumentDefaultType ? mDefaultElementType :
                                               aNamespaceID;

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsresult rv = mNodeInfoManager->GetNodeInfo(aName, aPrefix, aNamespaceID,
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

  content->SetContentID(mNextContentID++);

  content.swap(*aResult);

  return NS_OK;
}
