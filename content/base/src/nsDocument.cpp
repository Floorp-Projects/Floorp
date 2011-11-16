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
 *   James Ross      <silver@warwickcompsoc.co.uk>
 *   Ryan Jones      <sciguyryan@gmail.com>
 *   Ms2ger <ms2ger@gmail.com>
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

#include "mozilla/Util.h"

#ifdef MOZ_LOGGING
// so we can get logging even in release builds
#define FORCE_PR_LOG 1
#endif
#include "prlog.h"
#include "plstr.h"
#include "prprf.h"

#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsDocument.h"
#include "nsUnicharUtils.h"
#include "nsIPrivateDOMEvent.h"
#include "nsContentList.h"
#include "nsIObserver.h"
#include "nsIBaseWindow.h"
#include "mozilla/css/Loader.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIScriptRuntime.h"
#include "nsCOMArray.h"

#include "nsGUIEvent.h"
#include "nsPLDOMEvent.h"

#include "nsIDOMStyleSheet.h"
#include "nsDOMAttribute.h"
#include "nsIDOMDOMStringList.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMDocumentXBL.h"
#include "mozilla/FunctionTimer.h"
#include "nsGenericElement.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMCDATASection.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsDOMString.h"
#include "nsNodeUtils.h"
#include "nsLayoutUtils.h" // for GetFrameForPoint
#include "nsIFrame.h"
#include "nsITabChild.h"

#include "nsRange.h"
#include "nsIDOMText.h"
#include "nsIDOMComment.h"
#include "nsDOMDocumentType.h"
#include "nsNodeIterator.h"
#include "nsTreeWalker.h"

#include "nsIServiceManager.h"

#include "nsContentCID.h"
#include "nsDOMError.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIJSON.h"
#include "nsThreadUtils.h"
#include "nsNodeInfoManager.h"
#include "nsIXBLService.h"
#include "nsIFileChannel.h"
#include "nsIMultiPartChannel.h"
#include "nsIRefreshURI.h"
#include "nsIWebNavigation.h"
#include "nsIScriptError.h"

#include "nsNetUtil.h"     // for NS_MakeAbsoluteURI

#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"

#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMElement.h"
#include "nsFocusManager.h"

// for radio group stuff
#include "nsIDOMHTMLInputElement.h"
#include "nsIRadioVisitor.h"
#include "nsIFormControl.h"

#include "nsXMLEventsManager.h"

#include "nsBidiUtils.h"

#include "nsIDOMUserDataHandler.h"
#include "nsIDOMXPathEvaluator.h"
#include "nsIXPathEvaluatorInternal.h"
#include "nsIParserService.h"
#include "nsContentCreatorFunctions.h"

#include "nsIScriptContext.h"
#include "nsBindingManager.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIRequest.h"
#include "nsILink.h"
#include "nsFileDataProtocolHandler.h"

#include "nsICharsetAlias.h"
#include "nsIParser.h"
#include "nsIContentSink.h"

#include "nsDateTimeFormatCID.h"
#include "nsIDateTimeFormat.h"
#include "nsEventDispatcher.h"
#include "nsMutationEvent.h"
#include "nsIDOMXPathEvaluator.h"
#include "nsDOMCID.h"

#include "jsapi.h"
#include "nsIJSContextStack.h"
#include "nsIXPConnect.h"
#include "nsCycleCollector.h"
#include "nsCCUncollectableMarker.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsICategoryManager.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIContentViewer.h"
#include "nsIXMLContentSink.h"
#include "nsContentErrors.h"
#include "nsIXULDocument.h"
#include "nsIPrompt.h"
#include "nsIPropertyBag2.h"
#include "nsIDOMPageTransitionEvent.h"
#include "nsFrameLoader.h"
#include "nsEscape.h"
#ifdef MOZ_MEDIA
#include "nsHTMLMediaElement.h"
#endif // MOZ_MEDIA

#include "mozAutoDocUpdate.h"
#include "nsGlobalWindow.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseManager.h"
#include "nsDOMNavigationTiming.h"
#include "nsEventStateManager.h"

#include "nsSMILAnimationController.h"
#include "imgIContainer.h"
#include "nsSVGUtils.h"

#include "nsRefreshDriver.h"

// FOR CSP (autogenerated by xpidl)
#include "nsIContentSecurityPolicy.h"
#include "nsCSPService.h"
#include "nsHTMLStyleSheet.h"
#include "nsHTMLCSSStyleSheet.h"

#include "mozilla/dom/Link.h"
#include "nsIHTMLDocument.h"
#include "nsXULAppAPI.h"
#include "nsDOMTouchEvent.h"

#include "mozilla/Preferences.h"

#include "imgILoader.h"
#include "nsWrapperCacheInlines.h"

using namespace mozilla;
using namespace mozilla::dom;

typedef nsTArray<Link*> LinkArray;

// Reference to the document which requested DOM full-screen mode.
nsWeakPtr nsDocument::sFullScreenDoc = nsnull;

// Reference to the root document of the branch containing the document
// which requested DOM full-screen mode.
nsWeakPtr nsDocument::sFullScreenRootDoc = nsnull;

#ifdef PR_LOGGING
static PRLogModuleInfo* gDocumentLeakPRLog;
static PRLogModuleInfo* gCspPRLog;
#endif

#define NAME_NOT_VALID ((nsSimpleContentList*)1)

nsIdentifierMapEntry::~nsIdentifierMapEntry()
{
}

void
nsIdentifierMapEntry::Traverse(nsCycleCollectionTraversalCallback* aCallback)
{
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*aCallback,
                                     "mIdentifierMap mNameContentList");
  aCallback->NoteXPCOMChild(static_cast<nsIDOMNodeList*>(mNameContentList));

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*aCallback, "mIdentifierMap mDocAllList");
  aCallback->NoteXPCOMChild(static_cast<nsIDOMNodeList*>(mDocAllList));

  if (mImageElement) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*aCallback,
                                       "mIdentifierMap mImageElement element");
    nsIContent* imageElement = mImageElement;
    aCallback->NoteXPCOMChild(imageElement);
  }
}

bool
nsIdentifierMapEntry::IsEmpty()
{
  return mIdContentList.Count() == 0 && !mNameContentList &&
         !mChangeCallbacks && !mImageElement;
}

Element*
nsIdentifierMapEntry::GetIdElement()
{
  return static_cast<Element*>(mIdContentList.SafeElementAt(0));
}

Element*
nsIdentifierMapEntry::GetImageIdElement()
{
  return mImageElement ? mImageElement.get() : GetIdElement();
}

void
nsIdentifierMapEntry::AppendAllIdContent(nsCOMArray<nsIContent>* aElements)
{
  for (PRInt32 i = 0; i < mIdContentList.Count(); ++i) {
    aElements->AppendObject(static_cast<Element*>(mIdContentList[i]));
  }
}

void
nsIdentifierMapEntry::AddContentChangeCallback(nsIDocument::IDTargetObserver aCallback,
                                               void* aData, bool aForImage)
{
  if (!mChangeCallbacks) {
    mChangeCallbacks = new nsTHashtable<ChangeCallbackEntry>;
    if (!mChangeCallbacks)
      return;
    mChangeCallbacks->Init();
  }

  ChangeCallback cc = { aCallback, aData, aForImage };
  mChangeCallbacks->PutEntry(cc);
}

void
nsIdentifierMapEntry::RemoveContentChangeCallback(nsIDocument::IDTargetObserver aCallback,
                                                  void* aData, bool aForImage)
{
  if (!mChangeCallbacks)
    return;
  ChangeCallback cc = { aCallback, aData, aForImage };
  mChangeCallbacks->RemoveEntry(cc);
  if (mChangeCallbacks->Count() == 0) {
    mChangeCallbacks = nsnull;
  }
}

struct FireChangeArgs {
  Element* mFrom;
  Element* mTo;
  bool mImageOnly;
  bool mHaveImageOverride;
};

static PLDHashOperator
FireChangeEnumerator(nsIdentifierMapEntry::ChangeCallbackEntry *aEntry, void *aArg)
{
  FireChangeArgs* args = static_cast<FireChangeArgs*>(aArg);
  // Don't fire image changes for non-image observers, and don't fire element
  // changes for image observers when an image override is active.
  if (aEntry->mKey.mForImage ? (args->mHaveImageOverride && !args->mImageOnly) :
                               args->mImageOnly)
    return PL_DHASH_NEXT;
  return aEntry->mKey.mCallback(args->mFrom, args->mTo, aEntry->mKey.mData)
      ? PL_DHASH_NEXT : PL_DHASH_REMOVE;
}

void
nsIdentifierMapEntry::FireChangeCallbacks(Element* aOldElement,
                                          Element* aNewElement,
                                          bool aImageOnly)
{
  if (!mChangeCallbacks)
    return;

  FireChangeArgs args = { aOldElement, aNewElement, aImageOnly, !!mImageElement };
  mChangeCallbacks->EnumerateEntries(FireChangeEnumerator, &args);
}

bool
nsIdentifierMapEntry::AddIdElement(Element* aElement)
{
  NS_PRECONDITION(aElement, "Must have element");
  NS_PRECONDITION(mIdContentList.IndexOf(nsnull) < 0,
                  "Why is null in our list?");

#ifdef DEBUG
  Element* currentElement =
    static_cast<Element*>(mIdContentList.SafeElementAt(0));
#endif

  // Common case
  if (mIdContentList.Count() == 0) {
    if (!mIdContentList.AppendElement(aElement))
      return false;
    NS_ASSERTION(currentElement == nsnull, "How did that happen?");
    FireChangeCallbacks(nsnull, aElement);
    return true;
  }

  // We seem to have multiple content nodes for the same id, or XUL is messing
  // with us.  Search for the right place to insert the content.
  PRInt32 start = 0;
  PRInt32 end = mIdContentList.Count();
  do {
    NS_ASSERTION(start < end, "Bogus start/end");
    
    PRInt32 cur = (start + end) / 2;
    NS_ASSERTION(cur >= start && cur < end, "What happened here?");

    Element* curElement = static_cast<Element*>(mIdContentList[cur]);
    if (curElement == aElement) {
      // Already in the list, so already in the right spot.  Get out of here.
      // XXXbz this only happens because XUL does all sorts of random
      // UpdateIdTableEntry calls.  Hate, hate, hate!
      return true;
    }

    if (nsContentUtils::PositionIsBefore(aElement, curElement)) {
      end = cur;
    } else {
      start = cur + 1;
    }
  } while (start != end);

  if (!mIdContentList.InsertElementAt(aElement, start))
    return false;

  if (start == 0) {
    Element* oldElement =
      static_cast<Element*>(mIdContentList.SafeElementAt(1));
    NS_ASSERTION(currentElement == oldElement, "How did that happen?");
    FireChangeCallbacks(oldElement, aElement);
  }
  return true;
}

void
nsIdentifierMapEntry::RemoveIdElement(Element* aElement)
{
  NS_PRECONDITION(aElement, "Missing element");

  // This should only be called while the document is in an update.
  // Assertions near the call to this method guarantee this.

  // This could fire in OOM situations
  // Only assert this in HTML documents for now as XUL does all sorts of weird
  // crap.
  NS_ASSERTION(!aElement->OwnerDoc()->IsHTML() ||
               mIdContentList.IndexOf(aElement) >= 0,
               "Removing id entry that doesn't exist");

  // XXXbz should this ever Compact() I guess when all the content is gone
  // we'll just get cleaned up in the natural order of things...
  Element* currentElement =
    static_cast<Element*>(mIdContentList.SafeElementAt(0));
  mIdContentList.RemoveElement(aElement);
  if (currentElement == aElement) {
    FireChangeCallbacks(currentElement,
                        static_cast<Element*>(mIdContentList.SafeElementAt(0)));
  }
}

void
nsIdentifierMapEntry::SetImageElement(Element* aElement)
{
  Element* oldElement = GetImageIdElement();
  mImageElement = aElement;
  Element* newElement = GetImageIdElement();
  if (oldElement != newElement) {
    FireChangeCallbacks(oldElement, newElement, true);
  }
}

void
nsIdentifierMapEntry::AddNameElement(nsIDocument* aDocument, Element* aElement)
{
  if (!mNameContentList) {
    mNameContentList = new nsSimpleContentList(aDocument);
  }

  mNameContentList->AppendElement(aElement);
}

void
nsIdentifierMapEntry::RemoveNameElement(Element* aElement)
{
  if (mNameContentList) {
    mNameContentList->RemoveElement(aElement);
  }
}

// Helper structs for the content->subdoc map

class SubDocMapEntry : public PLDHashEntryHdr
{
public:
  // Both of these are strong references
  Element *mKey; // must be first, to look like PLDHashEntryStub
  nsIDocument *mSubDocument;
};

struct FindContentData
{
  FindContentData(nsIDocument *aSubDoc)
    : mSubDocument(aSubDoc), mResult(nsnull)
  {
  }

  nsISupports *mSubDocument;
  Element *mResult;
};


/**
 * A struct that holds all the information about a radio group.
 */
struct nsRadioGroupStruct
{
  nsRadioGroupStruct()
    : mRequiredRadioCount(0)
    , mGroupSuffersFromValueMissing(false)
  {}

  /**
   * A strong pointer to the currently selected radio button.
   */
  nsCOMPtr<nsIDOMHTMLInputElement> mSelectedRadioButton;
  nsCOMArray<nsIFormControl> mRadioButtons;
  PRUint32 mRequiredRadioCount;
  bool mGroupSuffersFromValueMissing;
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

DOMCI_DATA(StyleSheetList, nsDOMStyleSheetList)

// XXX couldn't we use the GetIIDs method from CSSStyleSheetList here?
// QueryInterface implementation for nsDOMStyleSheetList
NS_INTERFACE_TABLE_HEAD(nsDOMStyleSheetList)
  NS_INTERFACE_TABLE3(nsDOMStyleSheetList,
                      nsIDOMStyleSheetList,
                      nsIDocumentObserver,
                      nsIMutationObserver)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(StyleSheetList)
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

nsIStyleSheet*
nsDOMStyleSheetList::GetItemAt(PRUint32 aIndex)
{
  if (!mDocument || aIndex >= (PRUint32)mDocument->GetNumberOfStyleSheets()) {
    return nsnull;
  }

  nsIStyleSheet *sheet = mDocument->GetStyleSheetAt(aIndex);
  NS_ASSERTION(sheet, "Must have a sheet");

  return sheet;
}

NS_IMETHODIMP
nsDOMStyleSheetList::Item(PRUint32 aIndex, nsIDOMStyleSheet** aReturn)
{
  nsIStyleSheet *sheet = GetItemAt(aIndex);
  if (!sheet) {
      *aReturn = nsnull;

      return NS_OK;
  }

  return CallQueryInterface(sheet, aReturn);
}

void
nsDOMStyleSheetList::NodeWillBeDestroyed(const nsINode *aNode)
{
  mDocument = nsnull;
}

void
nsDOMStyleSheetList::StyleSheetAdded(nsIDocument *aDocument,
                                     nsIStyleSheet* aStyleSheet,
                                     bool aDocumentSheet)
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
                                       bool aDocumentSheet)
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
nsOnloadBlocker::IsPending(bool *_retval)
{
  *_retval = true;
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

// ==================================================================

nsExternalResourceMap::nsExternalResourceMap()
  : mHaveShutDown(false)
{
  mMap.Init();
  mPendingLoads.Init();
}

nsIDocument*
nsExternalResourceMap::RequestResource(nsIURI* aURI,
                                       nsINode* aRequestingNode,
                                       nsDocument* aDisplayDocument,
                                       ExternalResourceLoad** aPendingLoad)
{
  // If we ever start allowing non-same-origin loads here, we might need to do
  // something interesting with aRequestingPrincipal even for the hashtable
  // gets.
  NS_PRECONDITION(aURI, "Must have a URI");
  NS_PRECONDITION(aRequestingNode, "Must have a node");
  *aPendingLoad = nsnull;
  if (mHaveShutDown) {
    return nsnull;
  }
  
  // First, make sure we strip the ref from aURI.
  nsCOMPtr<nsIURI> clone;
  nsresult rv = aURI->CloneIgnoringRef(getter_AddRefs(clone));
  if (NS_FAILED(rv) || !clone) {
    return nsnull;
  }
  
  ExternalResource* resource;
  mMap.Get(clone, &resource);
  if (resource) {
    return resource->mDocument;
  }

  nsRefPtr<PendingLoad> load;
  mPendingLoads.Get(clone, getter_AddRefs(load));
  if (load) {
    load.forget(aPendingLoad);
    return nsnull;
  }

  load = new PendingLoad(aDisplayDocument);

  if (!mPendingLoads.Put(clone, load)) {
    return nsnull;
  }

  if (NS_FAILED(load->StartLoad(clone, aRequestingNode))) {
    // Make sure we don't thrash things by trying this load again, since
    // chances are it failed for good reasons (security check, etc).
    AddExternalResource(clone, nsnull, nsnull, aDisplayDocument);
  } else {
    load.forget(aPendingLoad);
  }

  return nsnull;
}

struct
nsExternalResourceEnumArgs
{
  nsIDocument::nsSubDocEnumFunc callback;
  void *data;
};

static PLDHashOperator
ExternalResourceEnumerator(nsIURI* aKey,
                           nsExternalResourceMap::ExternalResource* aData,
                           void* aClosure)
{
  nsExternalResourceEnumArgs* args =
    static_cast<nsExternalResourceEnumArgs*>(aClosure);
  bool next =
    aData->mDocument ? args->callback(aData->mDocument, args->data) : true;
  return next ? PL_DHASH_NEXT : PL_DHASH_STOP;
}

void
nsExternalResourceMap::EnumerateResources(nsIDocument::nsSubDocEnumFunc aCallback,
                                          void* aData)
{
  nsExternalResourceEnumArgs args = { aCallback, aData };
  mMap.EnumerateRead(ExternalResourceEnumerator, &args);
}

static PLDHashOperator
ExternalResourceTraverser(nsIURI* aKey,
                          nsExternalResourceMap::ExternalResource* aData,
                          void* aClosure)
{
  nsCycleCollectionTraversalCallback *cb = 
    static_cast<nsCycleCollectionTraversalCallback*>(aClosure);

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb,
                                     "mExternalResourceMap.mMap entry"
                                     "->mDocument");
  cb->NoteXPCOMChild(aData->mDocument);

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb,
                                     "mExternalResourceMap.mMap entry"
                                     "->mViewer");
  cb->NoteXPCOMChild(aData->mViewer);

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb,
                                     "mExternalResourceMap.mMap entry"
                                     "->mLoadGroup");
  cb->NoteXPCOMChild(aData->mLoadGroup);

  return PL_DHASH_NEXT;
}

void
nsExternalResourceMap::Traverse(nsCycleCollectionTraversalCallback* aCallback) const
{
  // mPendingLoads will get cleared out as the requests complete, so
  // no need to worry about those here.
  mMap.EnumerateRead(ExternalResourceTraverser, aCallback);
}

static PLDHashOperator
ExternalResourceHider(nsIURI* aKey,
                      nsExternalResourceMap::ExternalResource* aData,
                      void* aClosure)
{
  if (aData->mViewer) {
    aData->mViewer->Hide();
  }
  return PL_DHASH_NEXT;
}

void
nsExternalResourceMap::HideViewers()
{
  mMap.EnumerateRead(ExternalResourceHider, nsnull);
}

static PLDHashOperator
ExternalResourceShower(nsIURI* aKey,
                       nsExternalResourceMap::ExternalResource* aData,
                       void* aClosure)
{
  if (aData->mViewer) {
    aData->mViewer->Show();
  }
  return PL_DHASH_NEXT;
}

void
nsExternalResourceMap::ShowViewers()
{
  mMap.EnumerateRead(ExternalResourceShower, nsnull);
}

void
TransferZoomLevels(nsIDocument* aFromDoc,
                   nsIDocument* aToDoc)
{
  NS_ABORT_IF_FALSE(aFromDoc && aToDoc,
                    "transferring zoom levels from/to null doc");

  nsIPresShell* fromShell = aFromDoc->GetShell();
  if (!fromShell)
    return;

  nsPresContext* fromCtxt = fromShell->GetPresContext();
  if (!fromCtxt)
    return;

  nsIPresShell* toShell = aToDoc->GetShell();
  if (!toShell)
    return;

  nsPresContext* toCtxt = toShell->GetPresContext();
  if (!toCtxt)
    return;

  toCtxt->SetFullZoom(fromCtxt->GetFullZoom());
  toCtxt->SetMinFontSize(fromCtxt->MinFontSize());
  toCtxt->SetTextZoom(fromCtxt->TextZoom());
}

void
TransferShowingState(nsIDocument* aFromDoc, nsIDocument* aToDoc)
{
  NS_ABORT_IF_FALSE(aFromDoc && aToDoc,
                    "transferring showing state from/to null doc");

  if (aFromDoc->IsShowing()) {
    aToDoc->OnPageShow(true, nsnull);
  }
}

nsresult
nsExternalResourceMap::AddExternalResource(nsIURI* aURI,
                                           nsIContentViewer* aViewer,
                                           nsILoadGroup* aLoadGroup,
                                           nsIDocument* aDisplayDocument)
{
  NS_PRECONDITION(aURI, "Unexpected call");
  NS_PRECONDITION((aViewer && aLoadGroup) || (!aViewer && !aLoadGroup),
                  "Must have both or neither");
  
  nsRefPtr<PendingLoad> load;
  mPendingLoads.Get(aURI, getter_AddRefs(load));
  mPendingLoads.Remove(aURI);

  nsresult rv = NS_OK;
  
  nsCOMPtr<nsIDocument> doc;
  if (aViewer) {
    doc = aViewer->GetDocument();
    NS_ASSERTION(doc, "Must have a document");

    nsCOMPtr<nsIXULDocument> xulDoc = do_QueryInterface(doc);
    if (xulDoc) {
      // We don't handle XUL stuff here yet.
      rv = NS_ERROR_NOT_AVAILABLE;
    } else {
      doc->SetDisplayDocument(aDisplayDocument);

      // Make sure that hiding our viewer will tear down its presentation.
      aViewer->SetSticky(false);

      rv = aViewer->Init(nsnull, nsIntRect(0, 0, 0, 0));
      if (NS_SUCCEEDED(rv)) {
        rv = aViewer->Open(nsnull, nsnull);
      }
    }
    
    if (NS_FAILED(rv)) {
      doc = nsnull;
      aViewer = nsnull;
      aLoadGroup = nsnull;
    }
  }

  ExternalResource* newResource = new ExternalResource();
  if (newResource && !mMap.Put(aURI, newResource)) {
    delete newResource;
    newResource = nsnull;
    if (NS_SUCCEEDED(rv)) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }

  if (newResource) {
    newResource->mDocument = doc;
    newResource->mViewer = aViewer;
    newResource->mLoadGroup = aLoadGroup;
    if (doc) {
      TransferZoomLevels(aDisplayDocument, doc);
      TransferShowingState(aDisplayDocument, doc);
    }
  }

  const nsTArray< nsCOMPtr<nsIObserver> > & obs = load->Observers();
  for (PRUint32 i = 0; i < obs.Length(); ++i) {
    obs[i]->Observe(doc, "external-resource-document-created", nsnull);
  }

  return rv;
}

NS_IMPL_ISUPPORTS2(nsExternalResourceMap::PendingLoad,
                   nsIStreamListener,
                   nsIRequestObserver)

NS_IMETHODIMP
nsExternalResourceMap::PendingLoad::OnStartRequest(nsIRequest *aRequest,
                                                   nsISupports *aContext)
{
  nsExternalResourceMap& map = mDisplayDocument->ExternalResourceMap();
  if (map.HaveShutDown()) {
    return NS_BINDING_ABORTED;
  }

  nsCOMPtr<nsIContentViewer> viewer;
  nsCOMPtr<nsILoadGroup> loadGroup;
  nsresult rv = SetupViewer(aRequest, getter_AddRefs(viewer),
                            getter_AddRefs(loadGroup));

  // Make sure to do this no matter what
  nsresult rv2 = map.AddExternalResource(mURI, viewer, loadGroup,
                                         mDisplayDocument);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (NS_FAILED(rv2)) {
    mTargetListener = nsnull;
    return rv2;
  }
  
  return mTargetListener->OnStartRequest(aRequest, aContext);
}

nsresult
nsExternalResourceMap::PendingLoad::SetupViewer(nsIRequest* aRequest,
                                                nsIContentViewer** aViewer,
                                                nsILoadGroup** aLoadGroup)
{
  NS_PRECONDITION(!mTargetListener, "Unexpected call to OnStartRequest");
  *aViewer = nsnull;
  *aLoadGroup = nsnull;
  
  nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));
  NS_ENSURE_TRUE(chan, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aRequest));
  if (httpChannel) {
    bool requestSucceeded;
    if (NS_FAILED(httpChannel->GetRequestSucceeded(&requestSucceeded)) ||
        !requestSucceeded) {
      // Bail out on this load, since it looks like we have an HTTP error page
      return NS_BINDING_ABORTED;
    }
  }
 
  nsCAutoString type;
  chan->GetContentType(type);

  nsCOMPtr<nsILoadGroup> loadGroup;
  chan->GetLoadGroup(getter_AddRefs(loadGroup));

  // Give this document its own loadgroup
  nsCOMPtr<nsILoadGroup> newLoadGroup =
        do_CreateInstance(NS_LOADGROUP_CONTRACTID);
  NS_ENSURE_TRUE(newLoadGroup, NS_ERROR_OUT_OF_MEMORY);
  newLoadGroup->SetLoadGroup(loadGroup);

  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  loadGroup->GetNotificationCallbacks(getter_AddRefs(callbacks));

  nsCOMPtr<nsIInterfaceRequestor> newCallbacks =
    new LoadgroupCallbacks(callbacks);
  newLoadGroup->SetNotificationCallbacks(newCallbacks);

  // This is some serious hackery cribbed from docshell
  nsCOMPtr<nsICategoryManager> catMan =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(catMan, NS_ERROR_NOT_AVAILABLE);
  nsXPIDLCString contractId;
  nsresult rv = catMan->GetCategoryEntry("Gecko-Content-Viewers", type.get(),
                                         getter_Copies(contractId));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory =
    do_GetService(contractId);
  NS_ENSURE_TRUE(docLoaderFactory, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIContentViewer> viewer;
  nsCOMPtr<nsIStreamListener> listener;
  rv = docLoaderFactory->CreateInstance("external-resource", chan, newLoadGroup,
                                        type.get(), nsnull, nsnull,
                                        getter_AddRefs(listener),
                                        getter_AddRefs(viewer));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(viewer, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIParser> parser = do_QueryInterface(listener);
  if (!parser) {
    /// We don't want to deal with the various fake documents yet
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // We can't handle HTML and other weird things here yet.
  nsIContentSink* sink = parser->GetContentSink();
  nsCOMPtr<nsIXMLContentSink> xmlSink = do_QueryInterface(sink);
  if (!xmlSink) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  listener.swap(mTargetListener);
  viewer.forget(aViewer);
  newLoadGroup.forget(aLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP
nsExternalResourceMap::PendingLoad::OnDataAvailable(nsIRequest* aRequest,
                                                    nsISupports* aContext,
                                                    nsIInputStream* aStream,
                                                    PRUint32 aOffset,
                                                    PRUint32 aCount)
{
  NS_PRECONDITION(mTargetListener, "Shouldn't be getting called!");
  if (mDisplayDocument->ExternalResourceMap().HaveShutDown()) {
    return NS_BINDING_ABORTED;
  }
  return mTargetListener->OnDataAvailable(aRequest, aContext, aStream, aOffset,
                                          aCount);
}

NS_IMETHODIMP
nsExternalResourceMap::PendingLoad::OnStopRequest(nsIRequest* aRequest,
                                                  nsISupports* aContext,
                                                  nsresult aStatus)
{
  // mTargetListener might be null if SetupViewer or AddExternalResource failed
  if (mTargetListener) {
    nsCOMPtr<nsIStreamListener> listener;
    mTargetListener.swap(listener);
    return listener->OnStopRequest(aRequest, aContext, aStatus);
  }

  return NS_OK;
}

nsresult
nsExternalResourceMap::PendingLoad::StartLoad(nsIURI* aURI,
                                              nsINode* aRequestingNode)
{
  NS_PRECONDITION(aURI, "Must have a URI");
  NS_PRECONDITION(aRequestingNode, "Must have a node");

  // Time to start a load.  First, the security checks.

  nsIPrincipal* requestingPrincipal = aRequestingNode->NodePrincipal();

  nsresult rv = nsContentUtils::GetSecurityManager()->
    CheckLoadURIWithPrincipal(requestingPrincipal, aURI,
                              nsIScriptSecurityManager::STANDARD);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // Allow data URIs (let them skip the CheckMayLoad call), since we want
  // to allow external resources from data URIs regardless of the difference
  // in URI scheme.
  bool doesInheritSecurityContext;
  rv =
    NS_URIChainHasFlags(aURI,
                        nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT,
                        &doesInheritSecurityContext);
  if (NS_FAILED(rv) || !doesInheritSecurityContext) {
    rv = requestingPrincipal->CheckMayLoad(aURI, true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_OTHER,
                                 aURI,
                                 requestingPrincipal,
                                 aRequestingNode,
                                 EmptyCString(), //mime guess
                                 nsnull,         //extra
                                 &shouldLoad,
                                 nsContentUtils::GetContentPolicy(),
                                 nsContentUtils::GetSecurityManager());
  if (NS_FAILED(rv)) return rv;
  if (NS_CP_REJECTED(shouldLoad)) {
    // Disallowed by content policy
    return NS_ERROR_CONTENT_BLOCKED;
  }

  nsIDocument* doc = aRequestingNode->OwnerDoc();

  nsCOMPtr<nsIInterfaceRequestor> req = nsContentUtils::GetSameOriginChecker();
  NS_ENSURE_TRUE(req, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsILoadGroup> loadGroup = doc->GetDocumentLoadGroup();
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), aURI, nsnull, loadGroup, req);
  NS_ENSURE_SUCCESS(rv, rv);

  mURI = aURI;

  return channel->AsyncOpen(this, nsnull);
}

NS_IMPL_ISUPPORTS1(nsExternalResourceMap::LoadgroupCallbacks,
                   nsIInterfaceRequestor)

#define IMPL_SHIM(_i) \
  NS_IMPL_ISUPPORTS1(nsExternalResourceMap::LoadgroupCallbacks::_i##Shim, _i)

IMPL_SHIM(nsILoadContext)
IMPL_SHIM(nsIProgressEventSink)
IMPL_SHIM(nsIChannelEventSink)
IMPL_SHIM(nsISecurityEventSink)
IMPL_SHIM(nsIApplicationCacheContainer)

#undef IMPL_SHIM

#define IID_IS(_i) aIID.Equals(NS_GET_IID(_i))

#define TRY_SHIM(_i)                                                       \
  PR_BEGIN_MACRO                                                           \
    if (IID_IS(_i)) {                                                      \
      nsCOMPtr<_i> real = do_GetInterface(mCallbacks);                     \
      if (!real) {                                                         \
        return NS_NOINTERFACE;                                             \
      }                                                                    \
      nsCOMPtr<_i> shim = new _i##Shim(this, real);                        \
      if (!shim) {                                                         \
        return NS_ERROR_OUT_OF_MEMORY;                                     \
      }                                                                    \
      *aSink = shim.forget().get();                                        \
      return NS_OK;                                                        \
    }                                                                      \
  PR_END_MACRO

NS_IMETHODIMP
nsExternalResourceMap::LoadgroupCallbacks::GetInterface(const nsIID & aIID,
                                                        void **aSink)
{
  if (mCallbacks &&
      (IID_IS(nsIPrompt) || IID_IS(nsIAuthPrompt) || IID_IS(nsIAuthPrompt2) ||
       IID_IS(nsITabChild))) {
    return mCallbacks->GetInterface(aIID, aSink);
  }

  *aSink = nsnull;

  TRY_SHIM(nsILoadContext);
  TRY_SHIM(nsIProgressEventSink);
  TRY_SHIM(nsIChannelEventSink);
  TRY_SHIM(nsISecurityEventSink);
  TRY_SHIM(nsIApplicationCacheContainer);
    
  return NS_NOINTERFACE;
}

#undef TRY_SHIM
#undef IID_IS

nsExternalResourceMap::ExternalResource::~ExternalResource()
{
  if (mViewer) {
    mViewer->Close(nsnull);
    mViewer->Destroy();
  }
}

// ==================================================================
// =
// ==================================================================

// If we ever have an nsIDocumentObserver notification for stylesheet title
// changes, we could make this inherit from nsDOMStringList instead of
// reimplementing nsIDOMDOMStringList.
class nsDOMStyleSheetSetList : public nsIDOMDOMStringList
                          
{
public:
  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMDOMSTRINGLIST

  nsDOMStyleSheetSetList(nsIDocument* aDocument);

  void Disconnect()
  {
    mDocument = nsnull;
  }

protected:
  // Rebuild our list of style sets
  nsresult GetSets(nsTArray<nsString>& aStyleSets);
  
  nsIDocument* mDocument;  // Our document; weak ref.  It'll let us know if it
                           // dies.
};

NS_IMPL_ADDREF(nsDOMStyleSheetSetList)
NS_IMPL_RELEASE(nsDOMStyleSheetSetList)
NS_INTERFACE_TABLE_HEAD(nsDOMStyleSheetSetList)
  NS_OFFSET_AND_INTERFACE_TABLE_BEGIN(nsDOMStyleSheetSetList)
    NS_INTERFACE_TABLE_ENTRY(nsDOMStyleSheetSetList, nsIDOMDOMStringList)
  NS_OFFSET_AND_INTERFACE_TABLE_END
  NS_OFFSET_AND_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DOMStringList)
NS_INTERFACE_MAP_END

nsDOMStyleSheetSetList::nsDOMStyleSheetSetList(nsIDocument* aDocument)
  : mDocument(aDocument)
{
  NS_ASSERTION(mDocument, "Must have document!");
}

NS_IMETHODIMP
nsDOMStyleSheetSetList::Item(PRUint32 aIndex, nsAString& aResult)
{
  nsTArray<nsString> styleSets;
  nsresult rv = GetSets(styleSets);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (aIndex >= styleSets.Length()) {
    SetDOMStringToNull(aResult);
  } else {
    aResult = styleSets[aIndex];
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMStyleSheetSetList::GetLength(PRUint32 *aLength)
{
  nsTArray<nsString> styleSets;
  nsresult rv = GetSets(styleSets);
  NS_ENSURE_SUCCESS(rv, rv);
  
  *aLength = styleSets.Length();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMStyleSheetSetList::Contains(const nsAString& aString, bool *aResult)
{
  nsTArray<nsString> styleSets;
  nsresult rv = GetSets(styleSets);
  NS_ENSURE_SUCCESS(rv, rv);
  
  *aResult = styleSets.Contains(aString);

  return NS_OK;
}

nsresult
nsDOMStyleSheetSetList::GetSets(nsTArray<nsString>& aStyleSets)
{
  if (!mDocument) {
    return NS_OK; // Spec says "no exceptions", and we have no style sets if we
                  // have no document, for sure
  }
  
  PRInt32 count = mDocument->GetNumberOfStyleSheets();
  nsAutoString title;
  nsAutoString temp;
  for (PRInt32 index = 0; index < count; index++) {
    nsIStyleSheet* sheet = mDocument->GetStyleSheetAt(index);
    NS_ASSERTION(sheet, "Null sheet in sheet list!");
    sheet->GetTitle(title);
    if (!title.IsEmpty() && !aStyleSets.Contains(title) &&
        !aStyleSets.AppendElement(title)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_OK;
}

// ==================================================================
// =
// ==================================================================

class nsDOMImplementation : public nsIDOMDOMImplementation
{
public:
  nsDOMImplementation(nsIDocument* aOwner,
                      nsIScriptGlobalObject* aScriptObject,
                      nsIURI* aDocumentURI,
                      nsIURI* aBaseURI);
  virtual ~nsDOMImplementation();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsDOMImplementation)

  // nsIDOMDOMImplementation
  NS_DECL_NSIDOMDOMIMPLEMENTATION

protected:
  nsCOMPtr<nsIDocument> mOwner;
  nsWeakPtr mScriptObject;
  nsCOMPtr<nsIURI> mDocumentURI;
  nsCOMPtr<nsIURI> mBaseURI;
};

nsDOMImplementation::nsDOMImplementation(nsIDocument* aOwner,
                                         nsIScriptGlobalObject* aScriptObject,
                                         nsIURI* aDocumentURI,
                                         nsIURI* aBaseURI)
  : mOwner(aOwner),
    mScriptObject(do_GetWeakReference(aScriptObject)),
    mDocumentURI(aDocumentURI),
    mBaseURI(aBaseURI)
{
}

nsDOMImplementation::~nsDOMImplementation()
{
}

DOMCI_DATA(DOMImplementation, nsDOMImplementation)

// QueryInterface implementation for nsDOMImplementation
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMImplementation)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDOMImplementation)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMDOMImplementation)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DOMImplementation)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_1(nsDOMImplementation, mOwner)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMImplementation)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMImplementation)


NS_IMETHODIMP
nsDOMImplementation::HasFeature(const nsAString& aFeature,
                                const nsAString& aVersion,
                                bool* aReturn)
{
  return nsGenericElement::InternalIsSupported(
           static_cast<nsIDOMDOMImplementation*>(this),
           aFeature, aVersion, aReturn);
}

NS_IMETHODIMP
nsDOMImplementation::CreateDocumentType(const nsAString& aQualifiedName,
                                        const nsAString& aPublicId,
                                        const nsAString& aSystemId,
                                        nsIDOMDocumentType** aReturn)
{
  *aReturn = nsnull;
  NS_ENSURE_STATE(mOwner);

  nsresult rv = nsContentUtils::CheckQName(aQualifiedName);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAtom> name = do_GetAtom(aQualifiedName);
  NS_ENSURE_TRUE(name, NS_ERROR_OUT_OF_MEMORY);

  // Indicate that there is no internal subset (not just an empty one)
  nsAutoString voidString;
  voidString.SetIsVoid(true);
  return NS_NewDOMDocumentType(aReturn, mOwner->NodeInfoManager(),
                               name, aPublicId,
                               aSystemId, voidString);
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
    rv = parserService->CheckQName(qName, true, &colon);
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

  nsCOMPtr<nsIScriptGlobalObject> scriptHandlingObject =
    do_QueryReferent(mScriptObject);
  
  NS_ENSURE_STATE(!mScriptObject || scriptHandlingObject);

  return nsContentUtils::CreateDocument(aNamespaceURI, aQualifiedName, aDoctype,
                                        mDocumentURI, mBaseURI,
                                        mOwner->NodePrincipal(),
                                        scriptHandlingObject, false, aReturn);
}

NS_IMETHODIMP
nsDOMImplementation::CreateHTMLDocument(const nsAString& aTitle,
                                        nsIDOMDocument** aReturn)
{
  *aReturn = nsnull;
  NS_ENSURE_STATE(mOwner);

  nsCOMPtr<nsIDOMDocumentType> doctype;
  // Indicate that there is no internal subset (not just an empty one)
  nsAutoString voidString;
  voidString.SetIsVoid(true);
  nsresult rv = NS_NewDOMDocumentType(getter_AddRefs(doctype),
                                      mOwner->NodeInfoManager(),
                                      nsGkAtoms::html, // aName
                                      EmptyString(), // aPublicId
                                      EmptyString(), // aSystemId
                                      voidString); // aInternalSubset
  NS_ENSURE_SUCCESS(rv, rv);


  nsCOMPtr<nsIScriptGlobalObject> scriptHandlingObject =
    do_QueryReferent(mScriptObject);

  NS_ENSURE_STATE(!mScriptObject || scriptHandlingObject);
                                                       
  nsCOMPtr<nsIDOMDocument> document;
  rv = nsContentUtils::CreateDocument(EmptyString(), EmptyString(),
                                      doctype, mDocumentURI, mBaseURI,
                                      mOwner->NodePrincipal(),
                                      scriptHandlingObject, false,
                                      getter_AddRefs(document));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(document);

  nsCOMPtr<nsIContent> root;
  rv = doc->CreateElem(NS_LITERAL_STRING("html"), NULL, kNameSpaceID_XHTML,
                       getter_AddRefs(root));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = doc->AppendChildTo(root, false);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> head;
  rv = doc->CreateElem(NS_LITERAL_STRING("head"), NULL, kNameSpaceID_XHTML,
                       getter_AddRefs(head));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = root->AppendChildTo(head, false);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> title;
  rv = doc->CreateElem(NS_LITERAL_STRING("title"), NULL, kNameSpaceID_XHTML,
                       getter_AddRefs(title));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = head->AppendChildTo(title, false);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> titleText;
  rv = NS_NewTextNode(getter_AddRefs(titleText), doc->NodeInfoManager());
  NS_ENSURE_SUCCESS(rv, rv);
  rv = titleText->SetText(aTitle, false);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = title->AppendChildTo(titleText, false);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> body;
  rv = doc->CreateElem(NS_LITERAL_STRING("body"), NULL, kNameSpaceID_XHTML,
                       getter_AddRefs(body));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = root->AppendChildTo(body, false);
  NS_ENSURE_SUCCESS(rv, rv);

  document.forget(aReturn);

  return NS_OK;
}

// ==================================================================
// =
// ==================================================================

  // NOTE! nsDocument::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

nsDocument::nsDocument(const char* aContentType)
  : nsIDocument()
  , mAnimatingImages(true)
  , mIsFullScreen(false)
  , mVisibilityState(eHidden)
{
  SetContentTypeInternal(nsDependentCString(aContentType));
  
#ifdef PR_LOGGING
  if (!gDocumentLeakPRLog)
    gDocumentLeakPRLog = PR_NewLogModule("DocumentLeak");

  if (gDocumentLeakPRLog)
    PR_LOG(gDocumentLeakPRLog, PR_LOG_DEBUG,
           ("DOCUMENT %p created", this));

  if (!gCspPRLog)
    gCspPRLog = PR_NewLogModule("CSP");
#endif

  // Start out mLastStyleSheetSet as null, per spec
  SetDOMStringToNull(mLastStyleSheetSet);
  
  mLinksToUpdate.Init();
}

static PLDHashOperator
ClearAllBoxObjects(const void* aKey, nsPIBoxObject* aBoxObject, void* aUserArg)
{
  if (aBoxObject) {
    aBoxObject->Clear();
  }
  return PL_DHASH_NEXT;
}

nsDocument::~nsDocument()
{
#ifdef PR_LOGGING
  if (gDocumentLeakPRLog)
    PR_LOG(gDocumentLeakPRLog, PR_LOG_DEBUG,
           ("DOCUMENT %p destroyed", this));
#endif

#ifdef DEBUG
  nsCycleCollector_DEBUG_wasFreed(static_cast<nsIDocument*>(this));
#endif

  NS_ASSERTION(!mIsShowing, "Destroying a currently-showing document");

  mInDestructor = true;
  mInUnlinkOrDeletion = true;

  // Clear mObservers to keep it in sync with the mutationobserver list
  mObservers.Clear();

  if (mStyleSheetSetList) {
    mStyleSheetSetList->Disconnect();
  }

  if (mAnimationController) {
    mAnimationController->Disconnect();
  }

  mParentDocument = nsnull;

  // Kill the subdocument map, doing this will release its strong
  // references, if any.
  if (mSubDocuments) {
    PL_DHashTableDestroy(mSubDocuments);

    mSubDocuments = nsnull;
  }

  // Destroy link map now so we don't waste time removing
  // links one by one
  DestroyElementMaps();

  nsAutoScriptBlocker scriptBlocker;

  PRInt32 indx; // must be signed
  PRUint32 count = mChildren.ChildCount();
  for (indx = PRInt32(count) - 1; indx >= 0; --indx) {
    mChildren.ChildAt(indx)->UnbindFromTree();
    mChildren.RemoveChildAt(indx);
  }
  mFirstChild = nsnull;
  mCachedRootElement = nsnull;

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
  }

  if (mAttrStyleSheet) {
    mAttrStyleSheet->SetOwningDocument(nsnull);
  }
  
  if (mStyleAttrStyleSheet) {
    mStyleAttrStyleSheet->SetOwningDocument(nsnull);
  }

  delete mHeaderData;

  if (mBoxObjectTable) {
    mBoxObjectTable->EnumerateRead(ClearAllBoxObjects, nsnull);
    delete mBoxObjectTable;
  }

  mPendingTitleChangeEvent.Revoke();

  for (PRUint32 i = 0; i < mFileDataUris.Length(); ++i) {
    nsFileDataProtocolHandler::RemoveFileDataEntry(mFileDataUris[i]);
  }

  // We don't want to leave residual locks on images. Make sure we're in an
  // unlocked state, and then clear the table.
  SetImageLockingState(false);
  mImageTracker.Clear();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDocument)

NS_INTERFACE_TABLE_HEAD(nsDocument)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_DOCUMENT_INTERFACE_TABLE_BEGIN(nsDocument)
    NS_INTERFACE_TABLE_ENTRY(nsDocument, nsIDocument)
    NS_INTERFACE_TABLE_ENTRY(nsDocument, nsIDOMDocumentXBL)
    NS_INTERFACE_TABLE_ENTRY(nsDocument, nsIScriptObjectPrincipal)
    NS_INTERFACE_TABLE_ENTRY(nsDocument, nsIDOMEventTarget)
    NS_INTERFACE_TABLE_ENTRY(nsDocument, nsISupportsWeakReference)
    NS_INTERFACE_TABLE_ENTRY(nsDocument, nsIRadioGroupContainer)
    NS_INTERFACE_TABLE_ENTRY(nsDocument, nsIMutationObserver)
    NS_INTERFACE_TABLE_ENTRY(nsDocument, nsIApplicationCacheContainer)
    NS_INTERFACE_TABLE_ENTRY(nsDocument, nsIDOMDocumentTouch)
    NS_INTERFACE_TABLE_ENTRY(nsDocument, nsITouchEventReceiver)
    NS_INTERFACE_TABLE_ENTRY(nsDocument, nsIInlineEventHandlers)
  NS_OFFSET_AND_INTERFACE_TABLE_END
  NS_OFFSET_AND_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsDocument)
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMXPathNSResolver,
                                 new nsNode3Tearoff(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMNodeSelector,
                                 new nsNodeSelectorTearoff(this))
  if (aIID.Equals(NS_GET_IID(nsIDOMXPathEvaluator)) ||
      aIID.Equals(NS_GET_IID(nsIXPathEvaluatorInternal))) {
    if (!mXPathEvaluatorTearoff) {
      nsresult rv;
      mXPathEvaluatorTearoff =
        do_CreateInstance(NS_XPATH_EVALUATOR_CONTRACTID,
                          static_cast<nsIDocument *>(this), &rv);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    return mXPathEvaluatorTearoff->QueryInterface(aIID, aInstancePtr);
  }
  else
NS_INTERFACE_MAP_END


NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDocument)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_DESTROY(nsDocument, 
                                              nsNodeUtils::LastRelease(this))

static PLDHashOperator
SubDocTraverser(PLDHashTable *table, PLDHashEntryHdr *hdr, PRUint32 number,
                void *arg)
{
  SubDocMapEntry *entry = static_cast<SubDocMapEntry*>(hdr);
  nsCycleCollectionTraversalCallback *cb = 
    static_cast<nsCycleCollectionTraversalCallback*>(arg);

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb, "mSubDocuments entry->mKey");
  cb->NoteXPCOMChild(entry->mKey);
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb, "mSubDocuments entry->mSubDocument");
  cb->NoteXPCOMChild(entry->mSubDocument);

  return PL_DHASH_NEXT;
}

static PLDHashOperator
RadioGroupsTraverser(const nsAString& aKey, nsRadioGroupStruct* aData,
                     void* aClosure)
{
  nsCycleCollectionTraversalCallback *cb = 
    static_cast<nsCycleCollectionTraversalCallback*>(aClosure);

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb,
                                   "mRadioGroups entry->mSelectedRadioButton");
  cb->NoteXPCOMChild(aData->mSelectedRadioButton);

  PRUint32 i, count = aData->mRadioButtons.Count();
  for (i = 0; i < count; ++i) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb,
                                       "mRadioGroups entry->mRadioButtons[i]");
    cb->NoteXPCOMChild(aData->mRadioButtons[i]);
  }

  return PL_DHASH_NEXT;
}

static PLDHashOperator
BoxObjectTraverser(const void* key, nsPIBoxObject* boxObject, void* userArg)
{
  nsCycleCollectionTraversalCallback *cb = 
    static_cast<nsCycleCollectionTraversalCallback*>(userArg);
 
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb, "mBoxObjectTable entry");
  cb->NoteXPCOMChild(boxObject);

  return PL_DHASH_NEXT;
}

static PLDHashOperator
IdentifierMapEntryTraverse(nsIdentifierMapEntry *aEntry, void *aArg)
{
  nsCycleCollectionTraversalCallback *cb =
    static_cast<nsCycleCollectionTraversalCallback*>(aArg);
  aEntry->Traverse(cb);
  return PL_DHASH_NEXT;
}

static const char* kNSURIs[] = {
  " ([none])",
  " (xmlns)",
  " (xml)",
  " (xhtml)",
  " (XLink)",
  " (XSLT)",
  " (XBL)",
  " (MathML)",
  " (RDF)",
  " (XUL)"
};

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(nsDocument)
  if (NS_UNLIKELY(cb.WantDebugInfo())) {
    char name[512];
    PRUint32 nsid = tmp->GetDefaultNamespaceID();
    nsCAutoString uri;
    if (tmp->mDocumentURI)
      tmp->mDocumentURI->GetSpec(uri);
    if (nsid < ArrayLength(kNSURIs)) {
      PR_snprintf(name, sizeof(name), "nsDocument%s %s", kNSURIs[nsid],
                  uri.get());
    }
    else {
      PR_snprintf(name, sizeof(name), "nsDocument %s", uri.get());
    }
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), sizeof(nsDocument), name);
  }
  else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsDocument, tmp->mRefCnt.get())
  }

  // Always need to traverse script objects, so do that before we check
  // if we're uncollectable.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS

  if (!nsINode::Traverse(tmp, cb)) {
    return NS_SUCCESS_INTERRUPTED_TRAVERSE;
  }

  tmp->mIdentifierMap.EnumerateEntries(IdentifierMapEntryTraverse, &cb);

  tmp->mExternalResourceMap.Traverse(&cb);

  // Traverse the mChildren nsAttrAndChildArray.
  for (PRInt32 indx = PRInt32(tmp->mChildren.ChildCount()); indx > 0; --indx) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mChildren[i]");
    cb.NoteXPCOMChild(tmp->mChildren.ChildAt(indx - 1));
  }

  // Traverse all nsIDocument pointer members.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mSecurityInfo)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDisplayDocument)

  // Traverse all nsDocument nsCOMPtrs.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mParser)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mScriptGlobalObject)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_MEMBER(mListenerManager,
                                                  nsEventListenerManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDOMStyleSheets)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mScriptLoader)

  tmp->mRadioGroups.EnumerateRead(RadioGroupsTraverser, &cb);

  // The boxobject for an element will only exist as long as it's in the
  // document, so we'll traverse the table here instead of from the element.
  if (tmp->mBoxObjectTable) {
    tmp->mBoxObjectTable->EnumerateRead(BoxObjectTraverser, &cb);
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mChannel)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mStyleAttrStyleSheet, nsIStyleSheet)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mXPathEvaluatorTearoff)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mLayoutHistoryState)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnloadBlocker)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFirstBaseNodeWithHref)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDOMImplementation)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mImageMaps,
                                                       nsIDOMNodeList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOriginalDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mCachedEncoder)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFullScreenElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mStateObjectCached)

  // Traverse all our nsCOMArrays.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mStyleSheets)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mCatalogSheets)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mPreloadingImages)

  for (PRUint32 i = 0; i < tmp->mAnimationFrameListeners.Length(); ++i) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mAnimationFrameListeners[i]");
    cb.NoteXPCOMChild(tmp->mAnimationFrameListeners[i]);
  }

  // Traverse animation components
  if (tmp->mAnimationController) {
    tmp->mAnimationController->Traverse(&cb);
  }

  if (tmp->mSubDocuments && tmp->mSubDocuments->ops) {
    PL_DHashTableEnumerate(tmp->mSubDocuments, SubDocTraverser, &cb);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END


NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsDocument)
  nsINode::Trace(tmp, aCallback, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END


NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDocument)
  tmp->mInUnlinkOrDeletion = true;

  // Clear out our external resources
  tmp->mExternalResourceMap.Shutdown();

  nsAutoScriptBlocker scriptBlocker;

  nsINode::Unlink(tmp);

  // Unlink the mChildren nsAttrAndChildArray.
  for (PRInt32 indx = PRInt32(tmp->mChildren.ChildCount()) - 1; 
       indx >= 0; --indx) {
    tmp->mChildren.ChildAt(indx)->UnbindFromTree();
    tmp->mChildren.RemoveChildAt(indx);
  }
  tmp->mFirstChild = nsnull;

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mXPathEvaluatorTearoff)
  tmp->mCachedRootElement = nsnull; // Avoid a dangling pointer
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDisplayDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFirstBaseNodeWithHref)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDOMImplementation)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mImageMaps)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOriginalDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mCachedEncoder)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFullScreenElement)

  tmp->mParentDocument = nsnull;

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mPreloadingImages)

  
  if (tmp->mBoxObjectTable) {
   tmp->mBoxObjectTable->EnumerateRead(ClearAllBoxObjects, nsnull);
   delete tmp->mBoxObjectTable;
   tmp->mBoxObjectTable = nsnull;
 }

  if (tmp->mListenerManager) {
    tmp->mListenerManager->Disconnect();
    tmp->mListenerManager = nsnull;
  }

  if (tmp->mSubDocuments) {
    PL_DHashTableDestroy(tmp->mSubDocuments);
    tmp->mSubDocuments = nsnull;
  }

  tmp->mAnimationFrameListeners.Clear();

  tmp->mRadioGroups.Clear();
  
  // nsDocument has a pretty complex destructor, so we're going to
  // assume that *most* cycles you actually want to break somewhere
  // else, and not unlink an awful lot here.

  tmp->mIdentifierMap.Clear();

  if (tmp->mAnimationController) {
    tmp->mAnimationController->Unlink();
  }

  tmp->mPendingTitleChangeEvent.Revoke();
  
  tmp->mInUnlinkOrDeletion = false;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


nsresult
nsDocument::Init()
{
  if (mCSSLoader || mNodeInfoManager || mScriptLoader) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  mIdentifierMap.Init();
  (void)mStyledLinks.Init();
  mRadioGroups.Init();

  // Force initialization.
  nsINode::nsSlots* slots = GetSlots();
  NS_ENSURE_TRUE(slots,NS_ERROR_OUT_OF_MEMORY);

  // Prepend self as mutation-observer whether we need it or not (some
  // subclasses currently do, other don't). This is because the code in
  // nsNodeUtils always notifies the first observer first, expecting the
  // first observer to be the document.
  NS_ENSURE_TRUE(slots->mMutationObservers.PrependElementUnlessExists(static_cast<nsIMutationObserver*>(this)),
                 NS_ERROR_OUT_OF_MEMORY);


  mOnloadBlocker = new nsOnloadBlocker();
  NS_ENSURE_TRUE(mOnloadBlocker, NS_ERROR_OUT_OF_MEMORY);

  mCSSLoader = new mozilla::css::Loader(this);
  NS_ENSURE_TRUE(mCSSLoader, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(mCSSLoader);
  // Assume we're not quirky, until we know otherwise
  mCSSLoader->SetCompatibilityMode(eCompatibility_FullStandards);

  mNodeInfoManager = new nsNodeInfoManager();
  NS_ENSURE_TRUE(mNodeInfoManager, NS_ERROR_OUT_OF_MEMORY);

  nsresult  rv = mNodeInfoManager->Init(this);
  NS_ENSURE_SUCCESS(rv, rv);

  // mNodeInfo keeps NodeInfoManager alive!
  mNodeInfo = mNodeInfoManager->GetDocumentNodeInfo();
  NS_ENSURE_TRUE(mNodeInfo, NS_ERROR_OUT_OF_MEMORY);
  NS_ABORT_IF_FALSE(mNodeInfo->NodeType() == nsIDOMNode::DOCUMENT_NODE,
                    "Bad NodeType in aNodeInfo");

  NS_ASSERTION(OwnerDoc() == this, "Our nodeinfo is busted!");

  mScriptLoader = new nsScriptLoader(this);
  NS_ENSURE_TRUE(mScriptLoader, NS_ERROR_OUT_OF_MEMORY);

  if (!mImageTracker.Init()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

void 
nsIDocument::DeleteAllProperties()
{
  for (PRUint32 i = 0; i < GetPropertyTableCount(); ++i) {
    PropertyTable(i)->DeleteAllProperties();
  }
}

void
nsIDocument::DeleteAllPropertiesFor(nsINode* aNode)
{
  for (PRUint32 i = 0; i < GetPropertyTableCount(); ++i) {
    PropertyTable(i)->DeleteAllPropertiesFor(aNode);
  }
}

nsPropertyTable*
nsIDocument::GetExtraPropertyTable(PRUint16 aCategory)
{
  NS_ASSERTION(aCategory > 0, "Category 0 should have already been handled");
  while (aCategory >= mExtraPropertyTables.Length() + 1) {
    mExtraPropertyTables.AppendElement(new nsPropertyTable());
  }
  return mExtraPropertyTables[aCategory - 1];
}

void
nsDocument::AddXMLEventsContent(nsIContent *aXMLEventsElement)
{
  if (!mXMLEventsManager) {
    mXMLEventsManager = new nsXMLEventsManager();
    AddObserver(mXMLEventsManager);
  }
  mXMLEventsManager->AddXMLEventsContent(aXMLEventsElement);
}

void
nsDocument::Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup)
{
  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIPrincipal> principal;
  if (aChannel) {
    // Note: this code is duplicated in nsXULDocument::StartDocumentLoad and
    // nsScriptSecurityManager::GetChannelPrincipal.    
    // Note: this should match nsDocShell::OnLoadingSite
    NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));

    nsIScriptSecurityManager *securityManager =
      nsContentUtils::GetSecurityManager();
    if (securityManager) {
      securityManager->GetChannelPrincipal(aChannel,
                                           getter_AddRefs(principal));
    }
  }

  ResetToURI(uri, aLoadGroup, principal);

  nsCOMPtr<nsIPropertyBag2> bag = do_QueryInterface(aChannel);
  if (bag) {
    nsCOMPtr<nsIURI> baseURI;
    bag->GetPropertyAsInterface(NS_LITERAL_STRING("baseURI"),
                                NS_GET_IID(nsIURI), getter_AddRefs(baseURI));
    if (baseURI) {
      mDocumentBaseURI = baseURI;
    }
  }

  mChannel = aChannel;
}

void
nsDocument::ResetToURI(nsIURI *aURI, nsILoadGroup *aLoadGroup,
                       nsIPrincipal* aPrincipal)
{
  NS_PRECONDITION(aURI, "Null URI passed to ResetToURI");

#ifdef PR_LOGGING
  if (gDocumentLeakPRLog && PR_LOG_TEST(gDocumentLeakPRLog, PR_LOG_DEBUG)) {
    nsCAutoString spec;
    aURI->GetSpec(spec);
    PR_LogPrint("DOCUMENT %p ResetToURI %s", this, spec.get());
  }
#endif

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
  DestroyElementMaps();

  bool oldVal = mInUnlinkOrDeletion;
  mInUnlinkOrDeletion = true;
  PRUint32 count = mChildren.ChildCount();
  { // Scope for update
    MOZ_AUTO_DOC_UPDATE(this, UPDATE_CONTENT_MODEL, true);    
    for (PRInt32 i = PRInt32(count) - 1; i >= 0; i--) {
      nsCOMPtr<nsIContent> content = mChildren.ChildAt(i);

      nsIContent* previousSibling = content->GetPreviousSibling();

      if (nsINode::GetFirstChild() == content) {
        mFirstChild = content->GetNextSibling();
      }
      mChildren.RemoveChildAt(i);
      nsNodeUtils::ContentRemoved(this, content, i, previousSibling);
      content->UnbindFromTree();
    }
  }
  mInUnlinkOrDeletion = oldVal;
  mCachedRootElement = nsnull;

  // Reset our stylesheets
  ResetStylesheetsToURI(aURI);
  
  // Release the listener manager
  if (mListenerManager) {
    mListenerManager->Disconnect();
    mListenerManager = nsnull;
  }

  // Release the stylesheets list.
  mDOMStyleSheets = nsnull;

  // Clear the original URI so SetDocumentURI sets it.
  mOriginalURI = nsnull;

  SetDocumentURI(aURI);
  // If mDocumentBaseURI is null, nsIDocument::GetBaseURI() returns
  // mDocumentURI.
  mDocumentBaseURI = nsnull;

  if (aLoadGroup) {
    mDocumentLoadGroup = do_GetWeakReference(aLoadGroup);
    // there was an assertion here that aLoadGroup was not null.  This
    // is no longer valid: nsDocShell::SetDocument does not create a
    // load group, and it works just fine

    // XXXbz what does "just fine" mean exactly?  And given that there
    // is no nsDocShell::SetDocument, what is this talking about?
  }

  mLastModified.Truncate();
  // XXXbz I guess we're assuming that the caller will either pass in
  // a channel with a useful type or call SetContentType?
  SetContentTypeInternal(EmptyCString());
  mContentLanguage.Truncate();
  mBaseTarget.Truncate();
  mReferrer.Truncate();

  mXMLDeclarationBits = 0;

  // Now get our new principal
  if (aPrincipal) {
    SetPrincipal(aPrincipal);
  } else {
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
}

nsresult
nsDocument::ResetStylesheetsToURI(nsIURI* aURI)
{
  NS_PRECONDITION(aURI, "Null URI passed to ResetStylesheetsToURI");

  mozAutoDocUpdate upd(this, UPDATE_STYLE, true);
  
  // The stylesheets should forget us
  PRInt32 indx = mStyleSheets.Count();
  while (--indx >= 0) {
    nsIStyleSheet* sheet = mStyleSheets[indx];
    sheet->SetOwningDocument(nsnull);

    if (sheet->IsApplicable()) {
      RemoveStyleSheetFromStyleSets(sheet);
    }

    // XXX Tell observers?
  }

  indx = mCatalogSheets.Count();
  while (--indx >= 0) {
    nsIStyleSheet* sheet = mCatalogSheets[indx];
    sheet->SetOwningDocument(nsnull);

    if (sheet->IsApplicable()) {
      nsCOMPtr<nsIPresShell> shell = GetShell();
      if (shell) {
        shell->StyleSet()->RemoveStyleSheet(nsStyleSet::eAgentSheet, sheet);
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
  nsresult rv = NS_OK;
  if (mAttrStyleSheet) {
    // Remove this sheet from all style sets
    nsCOMPtr<nsIPresShell> shell = GetShell();
    if (shell) {
      shell->StyleSet()->RemoveStyleSheet(nsStyleSet::ePresHintSheet,
                                          mAttrStyleSheet);
    }
    mAttrStyleSheet->Reset(aURI);
  } else {
    rv = NS_NewHTMLStyleSheet(getter_AddRefs(mAttrStyleSheet), aURI, this);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Don't use AddStyleSheet, since it'll put the sheet into style
  // sets in the document level, which is not desirable here.
  mAttrStyleSheet->SetOwningDocument(this);
  
  if (mStyleAttrStyleSheet) {
    // Remove this sheet from all style sets
    nsCOMPtr<nsIPresShell> shell = GetShell();
    if (shell) {
      shell->StyleSet()->
        RemoveStyleSheet(nsStyleSet::eStyleAttrSheet, mStyleAttrStyleSheet);
    }
    mStyleAttrStyleSheet->Reset(aURI);
  } else {
    mStyleAttrStyleSheet = new nsHTMLCSSStyleSheet();
    NS_ENSURE_TRUE(mStyleAttrStyleSheet, NS_ERROR_OUT_OF_MEMORY);
    rv = mStyleAttrStyleSheet->Init(aURI, this);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // The loop over style sets below will handle putting this sheet
  // into style sets as needed.
  mStyleAttrStyleSheet->SetOwningDocument(this);

  // Now set up our style sets
  nsCOMPtr<nsIPresShell> shell = GetShell();
  if (shell) {
    FillStyleSet(shell->StyleSet());
  }

  return rv;
}

void
nsDocument::FillStyleSet(nsStyleSet* aStyleSet)
{
  NS_PRECONDITION(aStyleSet, "Must have a style set");
  NS_PRECONDITION(aStyleSet->SheetCount(nsStyleSet::ePresHintSheet) == 0,
                  "Style set already has a preshint sheet?");
  NS_PRECONDITION(aStyleSet->SheetCount(nsStyleSet::eDocSheet) == 0,
                  "Style set already has document sheets?");
  NS_PRECONDITION(aStyleSet->SheetCount(nsStyleSet::eStyleAttrSheet) == 0,
                  "Style set already has style attr sheets?");
  NS_PRECONDITION(mStyleAttrStyleSheet, "No style attr stylesheet?");
  NS_PRECONDITION(mAttrStyleSheet, "No attr stylesheet?");
  
  aStyleSet->AppendStyleSheet(nsStyleSet::ePresHintSheet, mAttrStyleSheet);

  aStyleSet->AppendStyleSheet(nsStyleSet::eStyleAttrSheet,
                              mStyleAttrStyleSheet);

  PRInt32 i;
  for (i = mStyleSheets.Count() - 1; i >= 0; --i) {
    nsIStyleSheet* sheet = mStyleSheets[i];
    if (sheet->IsApplicable()) {
      aStyleSet->AddDocStyleSheet(sheet, this);
    }
  }

  for (i = mCatalogSheets.Count() - 1; i >= 0; --i) {
    nsIStyleSheet* sheet = mCatalogSheets[i];
    if (sheet->IsApplicable()) {
      aStyleSet->AppendStyleSheet(nsStyleSet::eAgentSheet, sheet);
    }
  }
}

nsresult
nsDocument::StartDocumentLoad(const char* aCommand, nsIChannel* aChannel,
                              nsILoadGroup* aLoadGroup,
                              nsISupports* aContainer,
                              nsIStreamListener **aDocListener,
                              bool aReset, nsIContentSink* aSink)
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

  SetReadyStateInternal(READYSTATE_LOADING);

  if (nsCRT::strcmp(kLoadAsData, aCommand) == 0) {
    mLoadedAsData = true;
    // We need to disable script & style loading in this case.
    // We leave them disabled even in EndLoad(), and let anyone
    // who puts the document on display to worry about enabling.

    // Do not load/process scripts when loading as data
    ScriptLoader()->SetEnabled(false);

    // styles
    CSSLoader()->SetEnabled(false); // Do not load/process styles when loading as data
  } else if (nsCRT::strcmp("external-resource", aCommand) == 0) {
    // Allow CSS, but not scripts
    ScriptLoader()->SetEnabled(false);
  }

  mMayStartLayout = false;

  mHaveInputEncoding = true;

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
    SetContentTypeInternal(Substring(start, semicolon));
  }

  RetrieveRelevantHeaders(aChannel);

  mChannel = aChannel;

  nsresult rv = InitCSP();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsDocument::InitCSP()
{
  if (CSPService::sCSPEnabled) {
    nsAutoString cspHeaderValue;
    nsAutoString cspROHeaderValue;

    this->GetHeaderData(nsGkAtoms::headerCSP, cspHeaderValue);
    this->GetHeaderData(nsGkAtoms::headerCSPReportOnly, cspROHeaderValue);

    bool system = false;
    nsIScriptSecurityManager *ssm = nsContentUtils::GetSecurityManager();

    if (NS_SUCCEEDED(ssm->IsSystemPrincipal(NodePrincipal(), &system)) && system) {
      // only makes sense to register new CSP if this document is not priviliged
      return NS_OK;
    }

    if (cspHeaderValue.IsEmpty() && cspROHeaderValue.IsEmpty()) {
      // no CSP header present, stop processing
      return NS_OK;
    }

#ifdef PR_LOGGING 
    PR_LOG(gCspPRLog, PR_LOG_DEBUG, ("CSP header specified for document %p", this));
#endif

    nsresult rv;
    nsCOMPtr<nsIContentSecurityPolicy> mCSP;
    mCSP = do_CreateInstance("@mozilla.org/contentsecuritypolicy;1", &rv);

    if (NS_FAILED(rv)) {
#ifdef PR_LOGGING 
      PR_LOG(gCspPRLog, PR_LOG_DEBUG, ("Failed to create CSP object: %x", rv));
#endif
      return rv;
    }

    // Store the request context for violation reports
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
    mCSP->ScanRequestData(httpChannel);

    // Start parsing the policy
    nsCOMPtr<nsIURI> chanURI;
    mChannel->GetURI(getter_AddRefs(chanURI));

#ifdef PR_LOGGING 
    PR_LOG(gCspPRLog, PR_LOG_DEBUG, ("CSP Loaded"));
#endif

    // ReportOnly mode is enabled *only* if there are no regular-strength CSP
    // headers present.  If there are, then we ignore the ReportOnly mode and
    // toss a warning into the error console, proceeding with enforcing the
    // regular-strength CSP.
    if (cspHeaderValue.IsEmpty()) {
      mCSP->SetReportOnlyMode(true);
      mCSP->RefinePolicy(cspROHeaderValue, chanURI);
#ifdef PR_LOGGING 
      {
        PR_LOG(gCspPRLog, PR_LOG_DEBUG, 
                ("CSP (report only) refined, policy: \"%s\"", 
                  NS_ConvertUTF16toUTF8(cspROHeaderValue).get()));
      }
#endif
    } else {
      //XXX(sstamm): maybe we should post a warning when both read only and regular 
      // CSP headers are present.
      mCSP->RefinePolicy(cspHeaderValue, chanURI);
#ifdef PR_LOGGING 
      {
        PR_LOG(gCspPRLog, PR_LOG_DEBUG, 
               ("CSP refined, policy: \"%s\"",
                NS_ConvertUTF16toUTF8(cspHeaderValue).get()));
      }
#endif
    }

    // Check for frame-ancestor violation
    nsCOMPtr<nsIDocShell> docShell = do_QueryReferent(mDocumentContainer);
    if (docShell) {
        bool safeAncestry = false;

        // PermitsAncestry sends violation reports when necessary
        rv = mCSP->PermitsAncestry(docShell, &safeAncestry);
        NS_ENSURE_SUCCESS(rv, rv);

        if (!safeAncestry) {
#ifdef PR_LOGGING
            PR_LOG(gCspPRLog, PR_LOG_DEBUG, 
                   ("CSP doesn't like frame's ancestry, not loading."));
#endif
            // stop!  ERROR page!
            mChannel->Cancel(NS_ERROR_CSP_FRAME_ANCESTOR_VIOLATION);
        }
    }

    //Copy into principal
    nsIPrincipal* principal = GetPrincipal();

    if (principal) {
        principal->SetCsp(mCSP);
#ifdef PR_LOGGING
        PR_LOG(gCspPRLog, PR_LOG_DEBUG, 
                ("Inserted CSP into principal %p", principal));
    }
    else {
      PR_LOG(gCspPRLog, PR_LOG_DEBUG, 
              ("Couldn't copy CSP into absent principal %p", principal));
#endif
    }
  }
#ifdef PR_LOGGING
  else { //CSP was not enabled!
    PR_LOG(gCspPRLog, PR_LOG_DEBUG, 
           ("CSP is disabled, skipping CSP init for document %p", this));
  }
#endif
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
  nsCOMPtr<nsIURI> oldBase = GetDocBaseURI();
  mDocumentURI = NS_TryToMakeImmutable(aURI);
  nsIURI* newBase = GetDocBaseURI();

  bool equalBases = false;
  if (oldBase && newBase) {
    oldBase->Equals(newBase, &equalBases);
  }
  else {
    equalBases = !oldBase && !newBase;
  }

  // If this is the first time we're setting the document's URI, set the
  // document's original URI.
  if (!mOriginalURI)
    mOriginalURI = mDocumentURI;

  // If changing the document's URI changed the base URI of the document, we
  // need to refresh the hrefs of all the links on the page.
  if (!equalBases) {
    RefreshLinkHrefs();
  }
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

void
nsDocument::AddToNameTable(Element *aElement, nsIAtom* aName)
{
  nsIdentifierMapEntry *entry =
    mIdentifierMap.PutEntry(nsDependentAtomString(aName));

  // Null for out-of-memory
  if (entry) {
    entry->AddNameElement(this, aElement);
  }
}

void
nsDocument::RemoveFromNameTable(Element *aElement, nsIAtom* aName)
{
  // Speed up document teardown
  if (mIdentifierMap.Count() == 0)
    return;

  nsIdentifierMapEntry *entry =
    mIdentifierMap.GetEntry(nsDependentAtomString(aName));
  if (!entry) // Could be false if the element was anonymous, hence never added
    return;

  entry->RemoveNameElement(aElement);
}

void
nsDocument::AddToIdTable(Element *aElement, nsIAtom* aId)
{
  nsIdentifierMapEntry *entry =
    mIdentifierMap.PutEntry(nsDependentAtomString(aId));

  if (entry) { /* True except on OOM */
    entry->AddIdElement(aElement);
  }
}

void
nsDocument::RemoveFromIdTable(Element *aElement, nsIAtom* aId)
{
  NS_ASSERTION(aId, "huhwhatnow?");

  // Speed up document teardown
  if (mIdentifierMap.Count() == 0) {
    return;
  }

  nsIdentifierMapEntry *entry =
    mIdentifierMap.GetEntry(nsDependentAtomString(aId));
  if (!entry) // Can be null for XML elements with changing ids.
    return;

  entry->RemoveIdElement(aElement);
  if (entry->IsEmpty()) {
    mIdentifierMap.RawRemoveEntry(entry);
  }
}

nsIPrincipal*
nsDocument::GetPrincipal()
{
  return NodePrincipal();
}

extern bool sDisablePrefetchHTTPSPref;

void
nsDocument::SetPrincipal(nsIPrincipal *aNewPrincipal)
{
  if (aNewPrincipal && mAllowDNSPrefetch && sDisablePrefetchHTTPSPref) {
    nsCOMPtr<nsIURI> uri;
    aNewPrincipal->GetURI(getter_AddRefs(uri));
    bool isHTTPS;
    if (!uri || NS_FAILED(uri->SchemeIs("https", &isHTTPS)) ||
        isHTTPS) {
      mAllowDNSPrefetch = false;
    }
  }
  mNodeInfoManager->SetDocumentPrincipal(aNewPrincipal);
}

NS_IMETHODIMP
nsDocument::GetApplicationCache(nsIApplicationCache **aApplicationCache)
{
  NS_IF_ADDREF(*aApplicationCache = mApplicationCache);

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::SetApplicationCache(nsIApplicationCache *aApplicationCache)
{
  mApplicationCache = aApplicationCache;

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetContentType(nsAString& aContentType)
{
  CopyUTF8toUTF16(GetContentTypeInternal(), aContentType);

  return NS_OK;
}

void
nsDocument::SetContentType(const nsAString& aContentType)
{
  NS_ASSERTION(GetContentTypeInternal().IsEmpty() ||
               GetContentTypeInternal().Equals(NS_ConvertUTF16toUTF8(aContentType)),
               "Do you really want to change the content-type?");

  SetContentTypeInternal(NS_ConvertUTF16toUTF8(aContentType));
}

/* Return true if the document is in the focused top-level window, and is an
 * ancestor of the focused DOMWindow. */
NS_IMETHODIMP
nsDocument::HasFocus(bool* aResult)
{
  *aResult = false;

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm)
    return NS_ERROR_NOT_AVAILABLE;

  // Is there a focused DOMWindow?
  nsCOMPtr<nsIDOMWindow> focusedWindow;
  fm->GetFocusedWindow(getter_AddRefs(focusedWindow));
  if (!focusedWindow)
    return NS_OK;

  // Are we an ancestor of the focused DOMWindow?
  nsCOMPtr<nsIDOMDocument> domDocument;
  focusedWindow->GetDocument(getter_AddRefs(domDocument));
  nsCOMPtr<nsIDocument> document = do_QueryInterface(domDocument);

  for (nsIDocument* currentDoc = document; currentDoc;
       currentDoc = currentDoc->GetParentDocument()) {
    if (currentDoc == this) {
      // Yes, we are an ancestor
      *aResult = true;
      return NS_OK;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetReferrer(nsAString& aReferrer)
{
  CopyUTF8toUTF16(mReferrer, aReferrer);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetActiveElement(nsIDOMElement **aElement)
{
  *aElement = nsnull;

  // Get the focused element.
  nsCOMPtr<nsPIDOMWindow> window = GetWindow();
  if (window) {
    nsCOMPtr<nsPIDOMWindow> focusedWindow;
    nsIContent* focusedContent =
      nsFocusManager::GetFocusedDescendant(window, false,
                                           getter_AddRefs(focusedWindow));
    // be safe and make sure the element is from this document
    if (focusedContent && focusedContent->OwnerDoc() == this) {
      CallQueryInterface(focusedContent, aElement);
      return NS_OK;
    }
  }

  // No focused element anywhere in this document.  Try to get the BODY.
  nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryObject(this);
  if (htmlDoc) {
    nsCOMPtr<nsIDOMHTMLElement> bodyElement;
    htmlDoc->GetBody(getter_AddRefs(bodyElement));
    if (bodyElement) {
      *aElement = bodyElement;
      NS_ADDREF(*aElement);
    }
    // Because of IE compatibility, return null when html document doesn't have
    // a body.
    return NS_OK;
  }

  // If we couldn't get a BODY, return the root element.
  return GetDocumentElement(aElement);
}

NS_IMETHODIMP
nsDocument::GetCurrentScript(nsIDOMElement **aElement)
{
  nsIScriptElement* script = mScriptLoader->GetCurrentScript();
  if (script) {
    return CallQueryInterface(script, aElement);
  }
  
  *aElement = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::ElementFromPoint(float aX, float aY, nsIDOMElement** aReturn)
{
  return ElementFromPointHelper(aX, aY, false, true, aReturn);
}

nsresult
nsDocument::ElementFromPointHelper(float aX, float aY,
                                   bool aIgnoreRootScrollFrame,
                                   bool aFlushLayout,
                                   nsIDOMElement** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;
  // As per the the spec, we return null if either coord is negative
  if (!aIgnoreRootScrollFrame && (aX < 0 || aY < 0))
    return NS_OK;

  nscoord x = nsPresContext::CSSPixelsToAppUnits(aX);
  nscoord y = nsPresContext::CSSPixelsToAppUnits(aY);
  nsPoint pt(x, y);

  // Make sure the layout information we get is up-to-date, and
  // ensure we get a root frame (for everything but XUL)
  if (aFlushLayout)
    FlushPendingNotifications(Flush_Layout);

  nsIPresShell *ps = GetShell();
  NS_ENSURE_STATE(ps);
  nsIFrame *rootFrame = ps->GetRootFrame();

  // XUL docs, unlike HTML, have no frame tree until everything's done loading
  if (!rootFrame)
    return NS_OK; // return null to premature XUL callers as a reminder to wait

  nsIFrame *ptFrame = nsLayoutUtils::GetFrameForPoint(rootFrame, pt, true,
                                                      aIgnoreRootScrollFrame);
  if (!ptFrame)
    return NS_OK;

  nsIContent* ptContent = ptFrame->GetContent();
  NS_ENSURE_STATE(ptContent);

  // If the content is in a subdocument, try to get the element from |this| doc
  nsIDocument *currentDoc = ptContent->GetCurrentDoc();
  if (currentDoc && (currentDoc != this)) {
    *aReturn = CheckAncestryAndGetFrame(currentDoc).get();
    return NS_OK;
  }

  // If we have an anonymous element (such as an internal div from a textbox),
  // or a node that isn't an element (such as a text frame node),
  // replace it with the first non-anonymous parent node of type element.
  while (ptContent &&
         (!ptContent->IsElement() ||
          ptContent->IsInAnonymousSubtree())) {
    // XXXldb: Faster to jump to GetBindingParent if non-null?
    ptContent = ptContent->GetParent();
  }
 
  if (ptContent)
    CallQueryInterface(ptContent, aReturn);
  return NS_OK;
}

nsresult
nsDocument::NodesFromRectHelper(float aX, float aY,
                                float aTopSize, float aRightSize,
                                float aBottomSize, float aLeftSize,
                                bool aIgnoreRootScrollFrame,
                                bool aFlushLayout,
                                nsIDOMNodeList** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  
  nsSimpleContentList* elements = new nsSimpleContentList(this);
  NS_ADDREF(elements);
  *aReturn = elements;

  // Following the same behavior of elementFromPoint,
  // we don't return anything if either coord is negative
  if (!aIgnoreRootScrollFrame && (aX < 0 || aY < 0))
    return NS_OK;

  nscoord x = nsPresContext::CSSPixelsToAppUnits(aX - aLeftSize);
  nscoord y = nsPresContext::CSSPixelsToAppUnits(aY - aTopSize);
  nscoord w = nsPresContext::CSSPixelsToAppUnits(aLeftSize + aRightSize) + 1;
  nscoord h = nsPresContext::CSSPixelsToAppUnits(aTopSize + aBottomSize) + 1;

  nsRect rect(x, y, w, h);

  // Make sure the layout information we get is up-to-date, and
  // ensure we get a root frame (for everything but XUL)
  if (aFlushLayout) {
    FlushPendingNotifications(Flush_Layout);
  }

  nsIPresShell *ps = GetShell();
  NS_ENSURE_STATE(ps);
  nsIFrame *rootFrame = ps->GetRootFrame();

  // XUL docs, unlike HTML, have no frame tree until everything's done loading
  if (!rootFrame)
    return NS_OK; // return nothing to premature XUL callers as a reminder to wait

  nsAutoTArray<nsIFrame*,8> outFrames;
  nsLayoutUtils::GetFramesForArea(rootFrame, rect, outFrames,
                                  true, aIgnoreRootScrollFrame);

  PRInt32 length = outFrames.Length();
  if (!length)
    return NS_OK;

  // Used to filter out repeated elements in sequence.
  nsIContent* lastAdded = nsnull;

  for (PRInt32 i = 0; i < length; i++) {

    nsIContent* ptContent = outFrames.ElementAt(i)->GetContent();
    NS_ENSURE_STATE(ptContent);

    // If the content is in a subdocument, try to get the element from |this| doc
    nsIDocument *currentDoc = ptContent->GetCurrentDoc();
    if (currentDoc && (currentDoc != this)) {
      // XXX felipe: I can't get this type right without the intermediate vars
      nsCOMPtr<nsIDOMElement> x = CheckAncestryAndGetFrame(currentDoc);
      nsCOMPtr<nsIContent> elementDoc = do_QueryInterface(x);
      if (elementDoc != lastAdded) {
        elements->AppendElement(elementDoc);
        lastAdded = elementDoc;
      }
      continue;
    }

    // If we have an anonymous element (such as an internal div from a textbox),
    // or a node that isn't an element or a text node,
    // replace it with the first non-anonymous parent node.
    while (ptContent &&
           (!(ptContent->IsElement() ||
              ptContent->IsNodeOfType(nsINode::eTEXT)) ||
            ptContent->IsInAnonymousSubtree())) {
      // XXXldb: Faster to jump to GetBindingParent if non-null?
      ptContent = ptContent->GetParent();
    }
   
    if (ptContent && ptContent != lastAdded) {
      elements->AppendElement(ptContent);
      lastAdded = ptContent;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetElementsByClassName(const nsAString& aClasses,
                                   nsIDOMNodeList** aReturn)
{
  return nsContentUtils::GetElementsByClassName(this, aClasses, aReturn);
}

NS_IMETHODIMP
nsDocument::ReleaseCapture()
{
  // only release the capture if the caller can access it. This prevents a
  // page from stopping a scrollbar grab for example.
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(nsIPresShell::GetCapturingContent());
  if (node && nsContentUtils::CanCallerAccess(node)) {
    nsIPresShell::SetCapturingContent(nsnull, 0);
  }
  return NS_OK;
}

nsresult
nsDocument::SetBaseURI(nsIURI* aURI)
{
  if (!aURI && !mDocumentBaseURI) {
    return NS_OK;
  }
  
  // Don't do anything if the URI wasn't actually changed.
  if (aURI && mDocumentBaseURI) {
    bool equalBases = false;
    mDocumentBaseURI->Equals(aURI, &equalBases);
    if (equalBases) {
      return NS_OK;
    }
  }

  if (aURI) {
    mDocumentBaseURI = NS_TryToMakeImmutable(aURI);
  } else {
    mDocumentBaseURI = nsnull;
  }
  RefreshLinkHrefs();

  return NS_OK;
}

void
nsDocument::GetBaseTarget(nsAString &aBaseTarget)
{
  aBaseTarget = mBaseTarget;
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

    PRInt32 n = mCharSetObservers.Length();

    for (PRInt32 i = 0; i < n; i++) {
      nsIObserver* observer = mCharSetObservers.ElementAt(i);

      observer->Observe(static_cast<nsIDocument *>(this), "charset",
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
    bool found = false;
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
        found = true;

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

  if (aHeaderField == nsGkAtoms::headerContentLanguage) {
    CopyUTF16toUTF8(aData, mContentLanguage);
  }

  // Set the default script-type on the root element.
  if (aHeaderField == nsGkAtoms::headerContentScriptType) {
    Element *root = GetRootElement();
    if (root) {
      // Get the script-type ID for this value.
      nsresult rv;
      nsCOMPtr<nsIScriptRuntime> runtime;
      rv = NS_GetScriptRuntime(aData, getter_AddRefs(runtime));
      if (NS_FAILED(rv) || runtime == nsnull) {
        NS_WARNING("The script-type is unknown");
      } else {
        root->SetScriptTypeID(runtime->GetScriptTypeID());
      }
    }
  }

  if (aHeaderField == nsGkAtoms::headerDefaultStyle) {
    // Only mess with our stylesheets if we don't have a lastStyleSheetSet, per
    // spec.
    if (DOMStringIsNull(mLastStyleSheetSet)) {
      // Calling EnableStyleSheetsForSetInternal, not SetSelectedStyleSheetSet,
      // per spec.  The idea here is that we're changing our preferred set and
      // that shouldn't change the value of lastStyleSheetSet.  Also, we're
      // using the Internal version so we can update the CSSLoader and not have
      // to worry about null strings.
      EnableStyleSheetsForSetInternal(aData, true);
    }
  }

  if (aHeaderField == nsGkAtoms::refresh) {
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
                                           NS_ConvertUTF16toUTF8(aData));
    }
  }

  if (aHeaderField == nsGkAtoms::headerDNSPrefetchControl &&
      mAllowDNSPrefetch) {
    // Chromium treats any value other than 'on' (case insensitive) as 'off'.
    mAllowDNSPrefetch = aData.IsEmpty() || aData.LowerCaseEqualsLiteral("on");
  }
}

bool
nsDocument::TryChannelCharset(nsIChannel *aChannel,
                              PRInt32& aCharsetSource,
                              nsACString& aCharset)
{
  if(kCharsetFromChannel <= aCharsetSource) {
    return true;
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
          return true;
        }
      }
    }
  }
  return false;
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

  NS_ASSERTION(!mPresShell, "We have a presshell already!");

  NS_ENSURE_FALSE(GetBFCacheEntry(), NS_ERROR_FAILURE);

  FillStyleSet(aStyleSet);
  
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = NS_NewPresShell(getter_AddRefs(shell));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = shell->Init(this, aContext, aViewManager, aStyleSet, aCompatMode);
  NS_ENSURE_SUCCESS(rv, rv);

  // Note: we don't hold a ref to the shell (it holds a ref to us)
  mPresShell = shell;

  mExternalResourceMap.ShowViewers();

  MaybeRescheduleAnimationFrameNotifications();

  shell.swap(*aInstancePtrResult);

  return NS_OK;
}

void
nsDocument::MaybeRescheduleAnimationFrameNotifications()
{
  if (!mPresShell || !IsEventHandlingEnabled()) {
    // bail out for now, until one of those conditions changes
    return;
  }

  nsRefreshDriver* rd = mPresShell->GetPresContext()->RefreshDriver();
  if (mHavePendingPaint) {
    rd->ScheduleBeforePaintEvent(this);
  }
  if (!mAnimationFrameListeners.IsEmpty()) {
    rd->ScheduleAnimationFrameListeners(this);
  }
}

void
nsIDocument::TakeAnimationFrameListeners(AnimationListenerList& aListeners)
{
  aListeners.AppendElements(mAnimationFrameListeners);
  mAnimationFrameListeners.Clear();
}

void
nsDocument::DeleteShell()
{
  mExternalResourceMap.HideViewers();
  if (IsEventHandlingEnabled()) {
    RevokeAnimationFrameNotifications();
  }

  mPresShell = nsnull;
}

void
nsDocument::RevokeAnimationFrameNotifications()
{
  if (mHavePendingPaint) {
    mPresShell->GetPresContext()->RefreshDriver()->RevokeBeforePaintEvent(this);
  }
  if (!mAnimationFrameListeners.IsEmpty()) {
    mPresShell->GetPresContext()->RefreshDriver()->
      RevokeAnimationFrameListeners(this);
  }
}

static void
SubDocClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  SubDocMapEntry *e = static_cast<SubDocMapEntry *>(entry);

  NS_RELEASE(e->mKey);
  if (e->mSubDocument) {
    e->mSubDocument->SetParentDocument(nsnull);
    NS_RELEASE(e->mSubDocument);
  }
}

static bool
SubDocInitEntry(PLDHashTable *table, PLDHashEntryHdr *entry, const void *key)
{
  SubDocMapEntry *e =
    const_cast<SubDocMapEntry *>
              (static_cast<const SubDocMapEntry *>(entry));

  e->mKey = const_cast<Element*>(static_cast<const Element*>(key));
  NS_ADDREF(e->mKey);

  e->mSubDocument = nsnull;
  return true;
}

nsresult
nsDocument::SetSubDocumentFor(Element* aElement, nsIDocument* aSubDoc)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_UNEXPECTED);

  if (!aSubDoc) {
    // aSubDoc is nsnull, remove the mapping

    if (mSubDocuments) {
      SubDocMapEntry *entry =
        static_cast<SubDocMapEntry*>
                   (PL_DHashTableOperate(mSubDocuments, aElement,
                                         PL_DHASH_LOOKUP));

      if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
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
      static_cast<SubDocMapEntry*>
                 (PL_DHashTableOperate(mSubDocuments, aElement,
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
  if (mSubDocuments && aContent->IsElement()) {
    SubDocMapEntry *entry =
      static_cast<SubDocMapEntry*>
                 (PL_DHashTableOperate(mSubDocuments, aContent->AsElement(),
                                       PL_DHASH_LOOKUP));

    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      return entry->mSubDocument;
    }
  }

  return nsnull;
}

static PLDHashOperator
FindContentEnumerator(PLDHashTable *table, PLDHashEntryHdr *hdr,
                      PRUint32 number, void *arg)
{
  SubDocMapEntry *entry = static_cast<SubDocMapEntry*>(hdr);
  FindContentData *data = static_cast<FindContentData*>(arg);

  if (entry->mSubDocument == data->mSubDocument) {
    data->mResult = entry->mKey;

    return PL_DHASH_STOP;
  }

  return PL_DHASH_NEXT;
}

Element*
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

bool
nsDocument::IsNodeOfType(PRUint32 aFlags) const
{
    return !(aFlags & ~eDOCUMENT);
}

PRUint16
nsDocument::NodeType()
{
    return (PRUint16)nsIDOMNode::DOCUMENT_NODE;
}

void
nsDocument::NodeName(nsAString& aNodeName)
{
  aNodeName.AssignLiteral("#document");
}

Element*
nsIDocument::GetRootElement() const
{
  return (mCachedRootElement && mCachedRootElement->GetNodeParent() == this) ?
         mCachedRootElement : GetRootElementInternal();
}

Element*
nsDocument::GetRootElementInternal() const
{
  // Loop backwards because any non-elements, such as doctypes and PIs
  // are likely to appear before the root element.
  PRUint32 i;
  for (i = mChildren.ChildCount(); i > 0; --i) {
    nsIContent* child = mChildren.ChildAt(i - 1);
    if (child->IsElement()) {
      const_cast<nsDocument*>(this)->mCachedRootElement = child->AsElement();
      return child->AsElement();
    }
  }
  
  const_cast<nsDocument*>(this)->mCachedRootElement = nsnull;
  return nsnull;
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

nsIContent * const *
nsDocument::GetChildArray(PRUint32* aChildCount) const
{
  return mChildren.GetChildArray(aChildCount);
}
  

nsresult
nsDocument::InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                          bool aNotify)
{
  if (aKid->IsElement() && GetRootElement()) {
    NS_ERROR("Inserting element child when we already have one");
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  return doInsertChildAt(aKid, aIndex, aNotify, mChildren);
}

nsresult
nsDocument::AppendChildTo(nsIContent* aKid, bool aNotify)
{
  // Make sure to _not_ call the subclass InsertChildAt here.  If
  // subclasses wanted to hook into this stuff, they would have
  // overridden AppendChildTo.
  // XXXbz maybe this should just be a non-virtual method on nsINode?
  // Feels that way to me...
  return nsDocument::InsertChildAt(aKid, GetChildCount(), aNotify);
}

nsresult
nsDocument::RemoveChildAt(PRUint32 aIndex, bool aNotify)
{
  nsCOMPtr<nsIContent> oldKid = GetChildAt(aIndex);
  if (!oldKid) {
    return NS_OK;
  }

  if (oldKid->IsElement()) {
    // Destroy the link map up front before we mess with the child list.
    DestroyElementMaps();
  }

  nsresult rv =
    doRemoveChildAt(aIndex, aNotify, oldKid, mChildren);
  mCachedRootElement = nsnull;
  return rv;
}

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
  nsCOMPtr<nsIPresShell> shell = GetShell();
  if (shell) {
    shell->StyleSet()->AddDocStyleSheet(aSheet, this);
  }
}

void
nsDocument::AddStyleSheet(nsIStyleSheet* aSheet)
{
  NS_PRECONDITION(aSheet, "null arg");
  mStyleSheets.AppendObject(aSheet);
  aSheet->SetOwningDocument(this);

  if (aSheet->IsApplicable()) {
    AddStyleSheetToStyleSets(aSheet);
  }

  NS_DOCUMENT_NOTIFY_OBSERVERS(StyleSheetAdded, (this, aSheet, true));
}

void
nsDocument::RemoveStyleSheetFromStyleSets(nsIStyleSheet* aSheet)
{
  nsCOMPtr<nsIPresShell> shell = GetShell();
  if (shell) {
    shell->StyleSet()->RemoveStyleSheet(nsStyleSet::eDocSheet, aSheet);
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
    if (aSheet->IsApplicable()) {
      RemoveStyleSheetFromStyleSets(aSheet);
    }

    NS_DOCUMENT_NOTIFY_OBSERVERS(StyleSheetRemoved, (this, aSheet, true));
  }

  aSheet->SetOwningDocument(nsnull);
}

void
nsDocument::UpdateStyleSheets(nsCOMArray<nsIStyleSheet>& aOldSheets,
                              nsCOMArray<nsIStyleSheet>& aNewSheets)
{
  BeginUpdate(UPDATE_STYLE);

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
      if (newSheet->IsApplicable()) {
        AddStyleSheetToStyleSets(newSheet);
      }

      NS_DOCUMENT_NOTIFY_OBSERVERS(StyleSheetAdded, (this, newSheet, true));
    }
  }

  EndUpdate(UPDATE_STYLE);
}

void
nsDocument::InsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex)
{
  NS_PRECONDITION(aSheet, "null ptr");
  mStyleSheets.InsertObjectAt(aSheet, aIndex);

  aSheet->SetOwningDocument(this);

  if (aSheet->IsApplicable()) {
    AddStyleSheetToStyleSets(aSheet);
  }

  NS_DOCUMENT_NOTIFY_OBSERVERS(StyleSheetAdded, (this, aSheet, true));
}


void
nsDocument::SetStyleSheetApplicableState(nsIStyleSheet* aSheet,
                                         bool aApplicable)
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

  if (aSheet->IsApplicable()) {
    // This is like |AddStyleSheetToStyleSets|, but for an agent sheet.
    nsCOMPtr<nsIPresShell> shell = GetShell();
    if (shell) {
      shell->StyleSet()->AppendStyleSheet(nsStyleSet::eAgentSheet, aSheet);
    }
  }
                                                                                
  NS_DOCUMENT_NOTIFY_OBSERVERS(StyleSheetAdded, (this, aSheet, false));
}

void
nsDocument::EnsureCatalogStyleSheet(const char *aStyleSheetURI)
{
  mozilla::css::Loader* cssLoader = CSSLoader();
  if (cssLoader->GetEnabled()) {
    PRInt32 sheetCount = GetNumberOfCatalogStyleSheets();
    for (PRInt32 i = 0; i < sheetCount; i++) {
      nsIStyleSheet* sheet = GetCatalogStyleSheetAt(i);
      NS_ASSERTION(sheet, "unexpected null stylesheet in the document");
      if (sheet) {
        nsCAutoString uriStr;
        sheet->GetSheetURI()->GetSpec(uriStr);
        if (uriStr.Equals(aStyleSheetURI))
          return;
      }
    }

    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), aStyleSheetURI);
    if (uri) {
      nsRefPtr<nsCSSStyleSheet> sheet;
      cssLoader->LoadSheetSync(uri, true, true, getter_AddRefs(sheet));
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

   // We actually need to start returning the docshell's script global
   // object as soon as nsDocumentViewer::Close has called
   // RemovedFromDocShell on us.
   if (mRemovedFromDocShell) {
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

static void
NotifyActivityChanged(nsIContent *aContent, void *aUnused)
{
#ifdef MOZ_MEDIA
  nsCOMPtr<nsIDOMHTMLMediaElement> domMediaElem(do_QueryInterface(aContent));
  if (domMediaElem) {
    nsHTMLMediaElement* mediaElem = static_cast<nsHTMLMediaElement*>(aContent);
    mediaElem->NotifyOwnerDocumentActivityChanged();
  }
#endif
}

void
nsIDocument::SetContainer(nsISupports* aContainer)
{
  mDocumentContainer = do_GetWeakReference(aContainer);
  EnumerateFreezableElements(NotifyActivityChanged, nsnull);
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
  NS_ABORT_IF_FALSE(aScriptGlobalObject || !mAnimationController ||
                    mAnimationController->IsPausedByType(
                        nsSMILTimeContainer::PAUSE_PAGEHIDE |
                        nsSMILTimeContainer::PAUSE_BEGIN),
                    "Clearing window pointer while animations are unpaused");

  if (mScriptGlobalObject && !aScriptGlobalObject) {
    // We're detaching from the window.  We need to grab a pointer to
    // our layout history state now.
    mLayoutHistoryState = GetLayoutHistoryState();

    if (mPresShell && !EventHandlingSuppressed()) {
      RevokeAnimationFrameNotifications();
    }

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
    mScriptObject = nsnull;
    mHasHadScriptHandlingObject = true;
    // Go back to using the docshell for the layout history state
    mLayoutHistoryState = nsnull;
    mScopeObject = do_GetWeakReference(aScriptGlobalObject);

#ifdef DEBUG
    if (!mWillReparent) {
      // We really shouldn't have a wrapper here but if we do we need to make sure
      // it has the correct parent.
      JSObject *obj = GetWrapperPreserveColor();
      if (obj) {
        JSObject *newScope = aScriptGlobalObject->GetGlobalJSObject();
        nsIScriptContext *scx = aScriptGlobalObject->GetContext();
        JSContext *cx = scx ? scx->GetNativeContext() : nsnull;
        if (!cx) {
          nsContentUtils::ThreadJSContextStack()->Peek(&cx);
          if (!cx) {
            nsContentUtils::ThreadJSContextStack()->GetSafeJSContext(&cx);
            NS_ASSERTION(cx, "Uhoh, no context, this is bad!");
          }
        }
        if (cx) {
          NS_ASSERTION(JS_GetGlobalForObject(cx, obj) == newScope,
                       "Wrong scope, this is really bad!");
        }
      }
    }
#endif

    if (mAllowDNSPrefetch) {
      nsCOMPtr<nsIDocShell> docShell = do_QueryReferent(mDocumentContainer);
      if (docShell) {
#ifdef DEBUG
        nsCOMPtr<nsIWebNavigation> webNav =
          do_GetInterface(aScriptGlobalObject);
        NS_ASSERTION(SameCOMIdentity(webNav, docShell),
                     "Unexpected container or script global?");
#endif
        bool allowDNSPrefetch;
        docShell->GetAllowDNSPrefetch(&allowDNSPrefetch);
        mAllowDNSPrefetch = allowDNSPrefetch;
      }
    }

    MaybeRescheduleAnimationFrameNotifications();
  }

  // Remember the pointer to our window (or lack there of), to avoid
  // having to QI every time it's asked for.
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(mScriptGlobalObject);
  mWindow = window;

  // Set our visibility state, but do not fire the event.  This is correct
  // because either we're coming out of bfcache (in which case IsVisible() will
  // still test false at this point and no state change will happen) or we're
  // doing the initial document load and don't want to fire the event for this
  // change.
  mVisibilityState = GetVisibilityState();
}

nsIScriptGlobalObject*
nsDocument::GetScriptHandlingObjectInternal() const
{
  NS_ASSERTION(!mScriptGlobalObject,
               "Do not call this when mScriptGlobalObject is set!");

  nsCOMPtr<nsIScriptGlobalObject> scriptHandlingObject =
    do_QueryReferent(mScriptObject);
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(scriptHandlingObject);
  if (win) {
    NS_ASSERTION(win->IsInnerWindow(), "Should have inner window here!");
    nsPIDOMWindow* outer = win->GetOuterWindow();
    if (!outer || outer->GetCurrentInnerWindow() != win) {
      NS_WARNING("Wrong inner/outer window combination!");
      return nsnull;
    }
  }
  return scriptHandlingObject;
}
void
nsDocument::SetScriptHandlingObject(nsIScriptGlobalObject* aScriptObject)
{
  NS_ASSERTION(!mScriptGlobalObject ||
               mScriptGlobalObject == aScriptObject,
               "Wrong script object!");
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aScriptObject);
  NS_ASSERTION(!win || win->IsInnerWindow(), "Should have inner window here!");
  mScopeObject = mScriptObject = do_GetWeakReference(aScriptObject);
  if (aScriptObject) {
    mHasHadScriptHandlingObject = true;
  }
}

nsPIDOMWindow *
nsDocument::GetWindowInternal() const
{
  NS_ASSERTION(!mWindow, "This should not be called when mWindow is not null!");

  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(GetScriptGlobalObject()));

  if (!win) {
    return nsnull;
  }

  return win->GetOuterWindow();
}

nsPIDOMWindow *
nsDocument::GetInnerWindowInternal()
{
  NS_ASSERTION(mRemovedFromDocShell,
               "This document should have been removed from docshell!");

  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(GetScriptGlobalObject()));

  return win;
}

nsScriptLoader*
nsDocument::ScriptLoader()
{
  return mScriptLoader;
}

bool
nsDocument::InternalAllowXULXBL()
{
  if (nsContentUtils::AllowXULXBLForPrincipal(NodePrincipal())) {
    mAllowXULXBL = eTriTrue;
    return true;
  }

  mAllowXULXBL = eTriFalse;
  return false;
}

// Note: We don't hold a reference to the document observer; we assume
// that it has a live reference to the document.
void
nsDocument::AddObserver(nsIDocumentObserver* aObserver)
{
  NS_ASSERTION(mObservers.IndexOf(aObserver) == nsTArray<int>::NoIndex,
               "Observer already in the list");
  mObservers.AppendElement(aObserver);
  AddMutationObserver(aObserver);
}

bool
nsDocument::RemoveObserver(nsIDocumentObserver* aObserver)
{
  // If we're in the process of destroying the document (and we're
  // informing the observers of the destruction), don't remove the
  // observers from the list. This is not a big deal, since we
  // don't hold a live reference to the observers.
  if (!mInDestructor) {
    RemoveMutationObserver(aObserver);
    return mObservers.RemoveElement(aObserver);
  }

  return mObservers.Contains(aObserver);
}

void
nsDocument::MaybeEndOutermostXBLUpdate()
{
  // Only call BindingManager()->EndOutermostUpdate() when
  // we're not in an update and it is safe to run scripts.
  if (mUpdateNestLevel == 0 && mInXBLUpdate) {
    if (nsContentUtils::IsSafeToRunScript()) {
      mInXBLUpdate = false;
      BindingManager()->EndOutermostUpdate();
    } else if (!mInDestructor) {
      nsContentUtils::AddScriptRunner(
        NS_NewRunnableMethod(this, &nsDocument::MaybeEndOutermostXBLUpdate));
    }
  }
}

void
nsDocument::BeginUpdate(nsUpdateType aUpdateType)
{
  if (mUpdateNestLevel == 0 && !mInXBLUpdate) {
    mInXBLUpdate = true;
    BindingManager()->BeginOutermostUpdate();
  }
  
  ++mUpdateNestLevel;
  nsContentUtils::AddScriptBlocker();
  NS_DOCUMENT_NOTIFY_OBSERVERS(BeginUpdate, (this, aUpdateType));
}

void
nsDocument::EndUpdate(nsUpdateType aUpdateType)
{
  NS_DOCUMENT_NOTIFY_OBSERVERS(EndUpdate, (this, aUpdateType));

  nsContentUtils::RemoveScriptBlocker();

  --mUpdateNestLevel;

  // This set of updates may have created XBL bindings.  Let the
  // binding manager know we're done.
  MaybeEndOutermostXBLUpdate();

  MaybeInitializeFinalizeFrameLoaders();
}

void
nsDocument::BeginLoad()
{
  // Block onload here to prevent having to deal with blocking and
  // unblocking it while we know the document is loading.
  BlockOnload();

  if (mScriptLoader) {
    mScriptLoader->BeginDeferringScripts();
  }

  NS_DOCUMENT_NOTIFY_OBSERVERS(BeginLoad, (this));
}

void
nsDocument::ReportEmptyGetElementByIdArg()
{
  nsContentUtils::ReportToConsole(nsContentUtils::eDOM_PROPERTIES,
                                  "EmptyGetElementByIdParam",
                                  nsnull, 0,
                                  nsnull,
                                  EmptyString(), 0, 0,
                                  nsIScriptError::warningFlag,
                                  "DOM", this);
}

Element*
nsDocument::GetElementById(const nsAString& aElementId)
{
  if (!CheckGetElementByIdArg(aElementId)) {
    return nsnull;
  }

  nsIdentifierMapEntry *entry = mIdentifierMap.GetEntry(aElementId);
  return entry ? entry->GetIdElement() : nsnull;
}

const nsSmallVoidArray*
nsDocument::GetAllElementsForId(const nsAString& aElementId) const
{
  if (aElementId.IsEmpty()) {
    return nsnull;
  }

  nsIdentifierMapEntry *entry = mIdentifierMap.GetEntry(aElementId);
  return entry ? entry->GetIdElements() : nsnull;  
}

NS_IMETHODIMP
nsDocument::GetElementById(const nsAString& aId, nsIDOMElement** aReturn)
{
  Element *content = GetElementById(aId);
  if (content) {
    return CallQueryInterface(content, aReturn);
  }

  *aReturn = nsnull;

  return NS_OK;
}

Element*
nsDocument::AddIDTargetObserver(nsIAtom* aID, IDTargetObserver aObserver,
                                void* aData, bool aForImage)
{
  nsDependentAtomString id(aID);

  if (!CheckGetElementByIdArg(id))
    return nsnull;

  nsIdentifierMapEntry *entry = mIdentifierMap.PutEntry(id);
  NS_ENSURE_TRUE(entry, nsnull);

  entry->AddContentChangeCallback(aObserver, aData, aForImage);
  return aForImage ? entry->GetImageIdElement() : entry->GetIdElement();
}

void
nsDocument::RemoveIDTargetObserver(nsIAtom* aID, IDTargetObserver aObserver,
                                   void* aData, bool aForImage)
{
  nsDependentAtomString id(aID);

  if (!CheckGetElementByIdArg(id))
    return;

  nsIdentifierMapEntry *entry = mIdentifierMap.GetEntry(id);
  if (!entry) {
    return;
  }

  entry->RemoveContentChangeCallback(aObserver, aData, aForImage);
}

NS_IMETHODIMP
nsDocument::MozSetImageElement(const nsAString& aImageElementId,
                               nsIDOMElement* aImageElement)
{
  if (aImageElementId.IsEmpty())
    return NS_OK;

  // Hold a script blocker while calling SetImageElement since that can call
  // out to id-observers
  nsAutoScriptBlocker scriptBlocker;

  nsCOMPtr<nsIContent> content = do_QueryInterface(aImageElement);
  nsIdentifierMapEntry *entry = mIdentifierMap.PutEntry(aImageElementId);
  if (entry) {
    entry->SetImageElement(content ? content->AsElement() : nsnull);
    if (entry->IsEmpty()) {
      mIdentifierMap.RemoveEntry(aImageElementId);
    }
  }
  return NS_OK;
}

Element*
nsDocument::LookupImageElement(const nsAString& aId)
{
  if (aId.IsEmpty())
    return nsnull;

  nsIdentifierMapEntry *entry = mIdentifierMap.GetEntry(aId);
  return entry ? entry->GetImageIdElement() : nsnull;
}

void
nsDocument::DispatchContentLoadedEvents()
{
  NS_TIME_FUNCTION;
  // If you add early returns from this method, make sure you're
  // calling UnblockOnload properly.
  
  // Unpin references to preloaded images
  mPreloadingImages.Clear();

  if (mTiming) {
    mTiming->NotifyDOMContentLoadedStart(nsIDocument::GetDocumentURI());
  }
    
  // Fire a DOM event notifying listeners that this document has been
  // loaded (excluding images and other loads initiated by this
  // document).
  nsContentUtils::DispatchTrustedEvent(this, static_cast<nsIDocument*>(this),
                                       NS_LITERAL_STRING("DOMContentLoaded"),
                                       true, true);

  if (mTiming) {
    mTiming->NotifyDOMContentLoadedEnd(nsIDocument::GetDocumentURI());
  }

  // If this document is a [i]frame, fire a DOMFrameContentLoaded
  // event on all parent documents notifying that the HTML (excluding
  // other external files such as images and stylesheets) in a frame
  // has finished loading.

  // target_frame is the [i]frame element that will be used as the
  // target for the event. It's the [i]frame whose content is done
  // loading.
  nsCOMPtr<nsIDOMEventTarget> target_frame;

  if (mParentDocument) {
    target_frame =
      do_QueryInterface(mParentDocument->FindContentForSubDocument(this));
  }

  if (target_frame) {
    nsCOMPtr<nsIDocument> parent = mParentDocument;
    do {
      nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(parent);

      nsCOMPtr<nsIDOMEvent> event;
      nsCOMPtr<nsIPrivateDOMEvent> privateEvent;
      if (domDoc) {
        domDoc->CreateEvent(NS_LITERAL_STRING("Events"),
                            getter_AddRefs(event));

        privateEvent = do_QueryInterface(event);
      }

      if (event && privateEvent) {
        event->InitEvent(NS_LITERAL_STRING("DOMFrameContentLoaded"), true,
                         true);

        privateEvent->SetTarget(target_frame);
        privateEvent->SetTrusted(true);

        // To dispatch this event we must manually call
        // nsEventDispatcher::Dispatch() on the ancestor document since the
        // target is not in the same document, so the event would never reach
        // the ancestor document if we used the normal event
        // dispatching code.

        nsEvent* innerEvent = privateEvent->GetInternalNSEvent();
        if (innerEvent) {
          nsEventStatus status = nsEventStatus_eIgnore;

          nsIPresShell *shell = parent->GetShell();
          if (shell) {
            nsRefPtr<nsPresContext> context = shell->GetPresContext();

            if (context) {
              nsEventDispatcher::Dispatch(parent, context, innerEvent, event,
                                          &status);
            }
          }
        }
      }
      
      parent = parent->GetParentDocument();
    } while (parent);
  }

  // If the document has a manifest attribute, fire a MozApplicationManifest
  // event.
  Element* root = GetRootElement();
  if (root && root->HasAttr(kNameSpaceID_None, nsGkAtoms::manifest)) {
    nsContentUtils::DispatchChromeEvent(this, static_cast<nsIDocument*>(this),
                                        NS_LITERAL_STRING("MozApplicationManifest"),
                                        true, true);
  }

  UnblockOnload(true);
}

void
nsDocument::EndLoad()
{
  // Drop the ref to our parser, if any, but keep hold of the sink so that we
  // can flush it from FlushPendingNotifications as needed.  We might have to
  // do that to get a StartLayout() to happen.
  if (mParser) {
    mWeakSink = do_GetWeakReference(mParser->GetContentSink());
    mParser = nsnull;
  }
  
  NS_DOCUMENT_NOTIFY_OBSERVERS(EndLoad, (this));
  
  if (!mSynchronousDOMContentLoaded) {
    nsRefPtr<nsIRunnable> ev =
      NS_NewRunnableMethod(this, &nsDocument::DispatchContentLoadedEvents);
    NS_DispatchToCurrentThread(ev);
  } else {
    DispatchContentLoadedEvents();
  }
}

void
nsDocument::ContentStateChanged(nsIContent* aContent, nsEventStates aStateMask)
{
  NS_PRECONDITION(!nsContentUtils::IsSafeToRunScript(),
                  "Someone forgot a scriptblocker");
  NS_DOCUMENT_NOTIFY_OBSERVERS(ContentStateChanged,
                               (this, aContent, aStateMask));
}

void
nsDocument::DocumentStatesChanged(nsEventStates aStateMask)
{
  // Invalidate our cached state.
  mGotDocumentState &= ~aStateMask;
  mDocumentState &= ~aStateMask;

  NS_DOCUMENT_NOTIFY_OBSERVERS(DocumentStatesChanged, (this, aStateMask));
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
  for (i = 0; i < count; i++) {
    CallQueryInterface(mChildren.ChildAt(i), aDoctype);

    if (*aDoctype) {
      return NS_OK;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetImplementation(nsIDOMDOMImplementation** aImplementation)
{
  if (!mDOMImplementation) {
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), "about:blank");
    NS_ENSURE_TRUE(uri, NS_ERROR_OUT_OF_MEMORY);
    bool hasHadScriptObject = true;
    nsIScriptGlobalObject* scriptObject =
      GetScriptHandlingObject(hasHadScriptObject);
    NS_ENSURE_STATE(scriptObject || !hasHadScriptObject);
    mDOMImplementation = new nsDOMImplementation(this, scriptObject, uri, uri);
    if (!mDOMImplementation) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_ADDREF(*aImplementation = mDOMImplementation);

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetDocumentElement(nsIDOMElement** aDocumentElement)
{
  NS_ENSURE_ARG_POINTER(aDocumentElement);

  Element* root = GetRootElement();
  if (root) {
    return CallQueryInterface(root, aDocumentElement);
  }

  *aDocumentElement = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::CreateElement(const nsAString& aTagName,
                          nsIDOMElement** aReturn)
{
  *aReturn = nsnull;
  nsCOMPtr<nsIContent> content;
  nsresult rv = CreateElement(aTagName, getter_AddRefs(content));
  NS_ENSURE_SUCCESS(rv, rv);
  return CallQueryInterface(content, aReturn);
}

bool IsLowercaseASCII(const nsAString& aValue)
{
  PRInt32 len = aValue.Length();
  for (PRInt32 i = 0; i < len; ++i) {
    PRUnichar c = aValue[i];
    if (!(0x0061 <= (c) && ((c) <= 0x007a))) {
      return false;
    }
  }
  return true;
}

nsresult
nsDocument::CreateElement(const nsAString& aTagName,
                          nsIContent** aReturn)
{
  *aReturn = nsnull;

  nsresult rv = nsContentUtils::CheckQName(aTagName, false);
  NS_ENSURE_SUCCESS(rv, rv);

  bool needsLowercase = IsHTML() && !IsLowercaseASCII(aTagName);
  nsAutoString lcTagName;
  if (needsLowercase) {
    ToLowerCase(aTagName, lcTagName);
  }

  rv = CreateElem(needsLowercase ? lcTagName : aTagName,
                  nsnull, mDefaultElementType, aReturn);
  return rv;
}

NS_IMETHODIMP
nsDocument::CreateElementNS(const nsAString& aNamespaceURI,
                            const nsAString& aQualifiedName,
                            nsIDOMElement** aReturn)
{
  *aReturn = nsnull;
  nsCOMPtr<nsIContent> content;
  nsresult rv = CreateElementNS(aNamespaceURI, aQualifiedName,
                                getter_AddRefs(content));
  NS_ENSURE_SUCCESS(rv, rv);
  return CallQueryInterface(content, aReturn);
}

nsresult
nsDocument::CreateElementNS(const nsAString& aNamespaceURI,
                            const nsAString& aQualifiedName,
                            nsIContent** aReturn)
{
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsresult rv = nsContentUtils::GetNodeInfoFromQName(aNamespaceURI,
                                                     aQualifiedName,
                                                     mNodeInfoManager,
                                                     nsIDOMNode::ELEMENT_NODE,
                                                     getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_NewElement(aReturn, nodeInfo.forget(), NOT_FROM_PARSER);
}

NS_IMETHODIMP
nsDocument::CreateTextNode(const nsAString& aData, nsIDOMText** aReturn)
{
  *aReturn = nsnull;
  nsCOMPtr<nsIContent> content;
  nsresult rv = CreateTextNode(aData, getter_AddRefs(content));
  NS_ENSURE_SUCCESS(rv, rv);
  return CallQueryInterface(content, aReturn);
}

nsresult
nsDocument::CreateTextNode(const nsAString& aData, nsIContent** aReturn)
{
  nsresult rv = NS_NewTextNode(aReturn, mNodeInfoManager);
  if (NS_SUCCEEDED(rv)) {
    // Don't notify; this node is still being created.
    (*aReturn)->SetText(aData, false);
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

  // Make sure the substring "--" is not present in aData.  Otherwise
  // we'll create a document that can't be serialized.
  if (FindInReadable(NS_LITERAL_STRING("--"), aData)) {
    return NS_ERROR_DOM_INVALID_CHARACTER_ERR;
  }

  nsCOMPtr<nsIContent> comment;
  nsresult rv = NS_NewCommentNode(getter_AddRefs(comment), mNodeInfoManager);

  if (NS_SUCCEEDED(rv)) {
    // Don't notify; this node is still being created.
    comment->SetText(aData, false);

    rv = CallQueryInterface(comment, aReturn);
  }

  return rv;
}

NS_IMETHODIMP
nsDocument::CreateCDATASection(const nsAString& aData,
                               nsIDOMCDATASection** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  if (IsHTML()) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  if (FindInReadable(NS_LITERAL_STRING("]]>"), aData)) {
    return NS_ERROR_DOM_INVALID_CHARACTER_ERR;
  }

  nsCOMPtr<nsIContent> content;
  nsresult rv = NS_NewXMLCDATASection(getter_AddRefs(content),
                                      mNodeInfoManager);

  if (NS_SUCCEEDED(rv)) {
    // Don't notify; this node is still being created.
    content->SetText(aData, false);

    rv = CallQueryInterface(content, aReturn);
  }

  return rv;
}

NS_IMETHODIMP
nsDocument::CreateProcessingInstruction(const nsAString& aTarget,
                                        const nsAString& aData,
                                        nsIDOMProcessingInstruction** aReturn)
{
  *aReturn = nsnull;

  nsresult rv = nsContentUtils::CheckQName(aTarget, false);
  NS_ENSURE_SUCCESS(rv, rv);

  if (FindInReadable(NS_LITERAL_STRING("?>"), aData)) {
    return NS_ERROR_DOM_INVALID_CHARACTER_ERR;
  }

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

  WarnOnceAbout(eCreateAttribute);

  NS_ENSURE_TRUE(mNodeInfoManager, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = nsContentUtils::CheckQName(aName, false);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  rv = mNodeInfoManager->GetNodeInfo(aName, nsnull, kNameSpaceID_None,
                                     nsIDOMNode::ATTRIBUTE_NODE,
                                     getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString value;
  nsCOMPtr<nsIDOMAttr> attribute =
    new nsDOMAttribute(nsnull, nodeInfo.forget(), value, false);
  attribute.forget(aReturn);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::CreateAttributeNS(const nsAString & aNamespaceURI,
                              const nsAString & aQualifiedName,
                              nsIDOMAttr **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsnull;

  WarnOnceAbout(eCreateAttributeNS);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsresult rv = nsContentUtils::GetNodeInfoFromQName(aNamespaceURI,
                                                     aQualifiedName,
                                                     mNodeInfoManager,
                                                     nsIDOMNode::ATTRIBUTE_NODE,
                                                     getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString value;
  nsCOMPtr<nsIDOMAttr> attribute =
    new nsDOMAttribute(nsnull, nodeInfo.forget(), value, true);
  attribute.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetElementsByTagName(const nsAString& aTagname,
                                 nsIDOMNodeList** aReturn)
{
  nsRefPtr<nsContentList> list = GetElementsByTagName(aTagname);
  NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);

  // transfer ref to aReturn
  *aReturn = list.forget().get();
  return NS_OK;
}

already_AddRefed<nsContentList>
nsDocument::GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                                   const nsAString& aLocalName)
{
  PRInt32 nameSpaceId = kNameSpaceID_Wildcard;

  if (!aNamespaceURI.EqualsLiteral("*")) {
    nsresult rv =
      nsContentUtils::NameSpaceManager()->RegisterNameSpace(aNamespaceURI,
                                                            nameSpaceId);
    NS_ENSURE_SUCCESS(rv, nsnull);
  }

  NS_ASSERTION(nameSpaceId != kNameSpaceID_Unknown, "Unexpected namespace ID!");

  return NS_GetContentList(this, nameSpaceId, aLocalName);
}

NS_IMETHODIMP
nsDocument::GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                                   const nsAString& aLocalName,
                                   nsIDOMNodeList** aReturn)
{
  nsRefPtr<nsContentList> list = GetElementsByTagNameNS(aNamespaceURI,
                                                        aLocalName);
  NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);

  // transfer ref to aReturn
  *aReturn = list.forget().get();
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetAsync(bool *aAsync)
{
  NS_ERROR("nsDocument::GetAsync() should be overriden by subclass!");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::SetAsync(bool aAsync)
{
  NS_ERROR("nsDocument::SetAsync() should be overriden by subclass!");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::Load(const nsAString& aUrl, bool *aReturn)
{
  NS_ERROR("nsDocument::Load() should be overriden by subclass!");

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
nsDocument::GetSelectedStyleSheetSet(nsAString& aSheetSet)
{
  aSheetSet.Truncate();
  
  // Look through our sheets, find the selected set title
  PRInt32 count = GetNumberOfStyleSheets();
  nsAutoString title;
  for (PRInt32 index = 0; index < count; index++) {
    nsIStyleSheet* sheet = GetStyleSheetAt(index);
    NS_ASSERTION(sheet, "Null sheet in sheet list!");

    nsCOMPtr<nsIDOMStyleSheet> domSheet = do_QueryInterface(sheet);
    NS_ASSERTION(domSheet, "Sheet must QI to nsIDOMStyleSheet");
    bool disabled;
    domSheet->GetDisabled(&disabled);
    if (disabled) {
      // Disabled sheets don't affect the currently selected set
      continue;
    }
    
    sheet->GetTitle(title);

    if (aSheetSet.IsEmpty()) {
      aSheetSet = title;
    } else if (!title.IsEmpty() && !aSheetSet.Equals(title)) {
      // Sheets from multiple sets enabled; return null string, per spec.
      SetDOMStringToNull(aSheetSet);
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::SetSelectedStyleSheetSet(const nsAString& aSheetSet)
{
  if (DOMStringIsNull(aSheetSet)) {
    return NS_OK;
  }

  // Must update mLastStyleSheetSet before doing anything else with stylesheets
  // or CSSLoaders.
  mLastStyleSheetSet = aSheetSet;
  EnableStyleSheetsForSetInternal(aSheetSet, true);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetLastStyleSheetSet(nsAString& aSheetSet)
{
  aSheetSet = mLastStyleSheetSet;
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetPreferredStyleSheetSet(nsAString& aSheetSet)
{
  GetHeaderData(nsGkAtoms::headerDefaultStyle, aSheetSet);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetStyleSheetSets(nsIDOMDOMStringList** aList)
{
  if (!mStyleSheetSetList) {
    mStyleSheetSetList = new nsDOMStyleSheetSetList(this);
    if (!mStyleSheetSetList) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_ADDREF(*aList = mStyleSheetSetList);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::EnableStyleSheetsForSet(const nsAString& aSheetSet)
{
  // Per spec, passing in null is a no-op.
  if (!DOMStringIsNull(aSheetSet)) {
    // Note: must make sure to not change the CSSLoader's preferred sheet --
    // that value should be equal to either our lastStyleSheetSet (if that's
    // non-null) or to our preferredStyleSheetSet.  And this method doesn't
    // change either of those.
    EnableStyleSheetsForSetInternal(aSheetSet, false);
  }

  return NS_OK;
}

void
nsDocument::EnableStyleSheetsForSetInternal(const nsAString& aSheetSet,
                                            bool aUpdateCSSLoader)
{
  BeginUpdate(UPDATE_STYLE);
  PRInt32 count = GetNumberOfStyleSheets();
  nsAutoString title;
  for (PRInt32 index = 0; index < count; index++) {
    nsIStyleSheet* sheet = GetStyleSheetAt(index);
    NS_ASSERTION(sheet, "Null sheet in sheet list!");
    sheet->GetTitle(title);
    if (!title.IsEmpty()) {
      sheet->SetEnabled(title.Equals(aSheetSet));
    }
  }
  if (aUpdateCSSLoader) {
    CSSLoader()->SetPreferredSheet(aSheetSet);
  }
  EndUpdate(UPDATE_STYLE);
}

NS_IMETHODIMP
nsDocument::GetCharacterSet(nsAString& aCharacterSet)
{
  CopyASCIItoUTF16(GetDocumentCharacterSet(), aCharacterSet);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::ImportNode(nsIDOMNode* aImportedNode,
                       bool aDeep,
                       PRUint8 aArgc,
                       nsIDOMNode** aResult)
{
  NS_ENSURE_ARG(aImportedNode);
  if (aArgc == 0) {
    aDeep = true;
  }

  *aResult = nsnull;

  nsresult rv = nsContentUtils::CheckSameOrigin(this, aImportedNode);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRUint16 nodeType;
  aImportedNode->GetNodeType(&nodeType);
  switch (nodeType) {
    case nsIDOMNode::ATTRIBUTE_NODE:
    case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
    case nsIDOMNode::ELEMENT_NODE:
    case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
    case nsIDOMNode::TEXT_NODE:
    case nsIDOMNode::CDATA_SECTION_NODE:
    case nsIDOMNode::COMMENT_NODE:
    case nsIDOMNode::DOCUMENT_TYPE_NODE:
    {
      nsCOMPtr<nsINode> imported = do_QueryInterface(aImportedNode);
      NS_ENSURE_TRUE(imported, NS_ERROR_FAILURE);

      nsCOMPtr<nsIDOMNode> newNode;
      nsCOMArray<nsINode> nodesWithProperties;
      rv = nsNodeUtils::Clone(imported, aDeep, mNodeInfoManager,
                              nodesWithProperties, getter_AddRefs(newNode));
      NS_ENSURE_SUCCESS(rv, rv);

      nsIDocument *ownerDoc = imported->OwnerDoc();
      rv = nsNodeUtils::CallUserDataHandlers(nodesWithProperties, ownerDoc,
                                             nsIDOMUserDataHandler::NODE_IMPORTED,
                                             true);
      NS_ENSURE_SUCCESS(rv, rv);

      newNode.swap(*aResult);

      return NS_OK;
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
  rv = NS_NewURI(getter_AddRefs(uri), aURI);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Figure out the right principal to use
  nsCOMPtr<nsIPrincipal> subject;
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  if (secMan) {
    rv = secMan->GetSubjectPrincipal(getter_AddRefs(subject));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!subject) {
    // Fall back to our principal.  Or should we fall back to the null
    // principal?  The latter would just mean no binding loads....
    subject = NodePrincipal();
  }
  
  return BindingManager()->AddLayeredBinding(content, uri, subject);
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
  return BindingManager()->RemoveLayeredBinding(content, uri);
}

NS_IMETHODIMP
nsDocument::LoadBindingDocument(const nsAString& aURI)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURI,
                          mCharacterSet.get(),
                          GetDocBaseURI());
  NS_ENSURE_SUCCESS(rv, rv);

  // Figure out the right principal to use
  nsCOMPtr<nsIPrincipal> subject;
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  if (secMan) {
    rv = secMan->GetSubjectPrincipal(getter_AddRefs(subject));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!subject) {
    // Fall back to our principal.  Or should we fall back to the null
    // principal?  The latter would just mean no binding loads....
    subject = NodePrincipal();
  }
  
  BindingManager()->LoadBindingDocument(this, uri, subject);

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
                      const nsAString& aAttrValue, bool aUniversalMatch,
                      nsIDOMElement** aResult)
{
  if (aUniversalMatch ? aContent->HasAttr(kNameSpaceID_None, aAttrName) :
                        aContent->AttrValueIs(kNameSpaceID_None, aAttrName,
                                              aAttrValue, eCaseMatters)) {
    return CallQueryInterface(aContent, aResult);
  }

  for (nsIContent* child = aContent->GetFirstChild();
       child;
       child = child->GetNextSibling()) {

    GetElementByAttribute(child, aAttrName, aAttrValue, aUniversalMatch,
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

  bool universalMatch = aAttrValue.EqualsLiteral("*");

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
  return BindingManager()->GetAnonymousNodesFor(content, aResult);
}

NS_IMETHODIMP
nsDocument::CreateRange(nsIDOMRange** aReturn)
{
  nsRefPtr<nsRange> range = new nsRange();
  nsresult rv = range->Set(this, 0, this, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  range.forget(aReturn);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::CreateNodeIterator(nsIDOMNode *aRoot,
                               PRUint32 aWhatToShow,
                               nsIDOMNodeFilter *aFilter,
                               bool aEntityReferenceExpansion,
                               nsIDOMNodeIterator **_retval)
{
  *_retval = nsnull;

  if (!aRoot)
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;

  nsresult rv = nsContentUtils::CheckSameOrigin(this, aRoot);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_ARG_POINTER(_retval);

  nsCOMPtr<nsINode> root = do_QueryInterface(aRoot);
  NS_ENSURE_TRUE(root, NS_ERROR_DOM_NOT_SUPPORTED_ERR);

  nsNodeIterator *iterator = new nsNodeIterator(root,
                                                aWhatToShow,
                                                aFilter,
                                                aEntityReferenceExpansion);
  NS_ENSURE_TRUE(iterator, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*_retval = iterator);

  return NS_OK; 
}

NS_IMETHODIMP
nsDocument::CreateTreeWalker(nsIDOMNode *aRoot,
                             PRUint32 aWhatToShow,
                             nsIDOMNodeFilter *aFilter,
                             bool aEntityReferenceExpansion,
                             nsIDOMTreeWalker **_retval)
{
  *_retval = nsnull;

  if (!aRoot)
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;

  nsresult rv = nsContentUtils::CheckSameOrigin(this, aRoot);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_ARG_POINTER(_retval);

  nsCOMPtr<nsINode> root = do_QueryInterface(aRoot);
  NS_ENSURE_TRUE(root, NS_ERROR_DOM_NOT_SUPPORTED_ERR);

  nsTreeWalker* walker = new nsTreeWalker(root,
                                          aWhatToShow,
                                          aFilter,
                                          aEntityReferenceExpansion);
  NS_ENSURE_TRUE(walker, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*_retval = walker);

  return NS_OK;
}


NS_IMETHODIMP
nsDocument::GetDefaultView(nsIDOMWindow** aDefaultView)
{
  *aDefaultView = nsnull;
  nsPIDOMWindow* win = GetWindow();
  if (!win) {
    return NS_OK;
  }
  return CallQueryInterface(win, aDefaultView);
}

NS_IMETHODIMP
nsDocument::GetLocation(nsIDOMLocation **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  nsCOMPtr<nsIDOMWindow> w = do_QueryInterface(mScriptGlobalObject);

  if (!w) {
    return NS_OK;
  }

  return w->GetLocation(_retval);
}

Element*
nsIDocument::GetHtmlElement()
{
  Element* rootElement = GetRootElement();
  if (rootElement && rootElement->IsHTML(nsGkAtoms::html))
    return rootElement;
  return nsnull;
}

Element*
nsIDocument::GetHtmlChildElement(nsIAtom* aTag)
{
  Element* html = GetHtmlElement();
  if (!html)
    return nsnull;

  // Look for the element with aTag inside html. This needs to run
  // forwards to find the first such element.
  for (nsIContent* child = html->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->IsHTML(aTag))
      return child->AsElement();
  }
  return nsnull;
}

nsIContent*
nsDocument::GetTitleContent(PRUint32 aNamespace)
{
  // mMayHaveTitleElement will have been set to true if any HTML or SVG
  // <title> element has been bound to this document. So if it's false,
  // we know there is nothing to do here. This avoids us having to search
  // the whole DOM if someone calls document.title on a large document
  // without a title.
  if (!mMayHaveTitleElement)
    return nsnull;

  nsRefPtr<nsContentList> list =
    NS_GetContentList(this, aNamespace, NS_LITERAL_STRING("title"));

  return list->Item(0, false);
}

void
nsDocument::GetTitleFromElement(PRUint32 aNamespace, nsAString& aTitle)
{
  nsIContent* title = GetTitleContent(aNamespace);
  if (!title)
    return;
  nsContentUtils::GetNodeTextContent(title, false, aTitle);
}

NS_IMETHODIMP
nsDocument::GetTitle(nsAString& aTitle)
{
  aTitle.Truncate();

  nsIContent *rootElement = GetRootElement();
  if (!rootElement)
    return NS_OK;

  nsAutoString tmp;

  switch (rootElement->GetNameSpaceID()) {
#ifdef MOZ_XUL
    case kNameSpaceID_XUL:
      rootElement->GetAttr(kNameSpaceID_None, nsGkAtoms::title, tmp);
      break;
#endif
    case kNameSpaceID_SVG:
      if (rootElement->Tag() == nsGkAtoms::svg) {
        GetTitleFromElement(kNameSpaceID_SVG, tmp);
        break;
      } // else fall through
    default:
      GetTitleFromElement(kNameSpaceID_XHTML, tmp);
      break;
  }

  tmp.CompressWhitespace();
  aTitle = tmp;
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::SetTitle(const nsAString& aTitle)
{
  Element *rootElement = GetRootElement();
  if (!rootElement)
    return NS_OK;

  switch (rootElement->GetNameSpaceID()) {
    case kNameSpaceID_SVG:
      return NS_OK; // SVG doesn't support setting a title
#ifdef MOZ_XUL
    case kNameSpaceID_XUL:
      return rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::title,
                                  aTitle, true);
#endif
  }

  // Batch updates so that mutation events don't change "the title
  // element" under us
  mozAutoDocUpdate updateBatch(this, UPDATE_CONTENT_MODEL, true);

  nsIContent* title = GetTitleContent(kNameSpaceID_XHTML);
  if (!title) {
    Element *head = GetHeadElement();
    if (!head)
      return NS_OK;

    {
      nsCOMPtr<nsINodeInfo> titleInfo;
      titleInfo = mNodeInfoManager->GetNodeInfo(nsGkAtoms::title, nsnull,
                                                kNameSpaceID_XHTML,
                                                nsIDOMNode::ELEMENT_NODE);
      if (!titleInfo)
        return NS_OK;
      title = NS_NewHTMLTitleElement(titleInfo.forget());
      if (!title)
        return NS_OK;
    }

    head->AppendChildTo(title, true);
  }

  return nsContentUtils::SetNodeTextContent(title, aTitle, false);
}

void
nsDocument::NotifyPossibleTitleChange(bool aBoundTitleElement)
{
  NS_ASSERTION(!mInUnlinkOrDeletion || !aBoundTitleElement,
               "Setting a title while unlinking or destroying the element?");
  if (mInUnlinkOrDeletion) {
    return;
  }

  if (aBoundTitleElement) {
    mMayHaveTitleElement = true;
  }
  if (mPendingTitleChangeEvent.IsPending())
    return;

  nsRefPtr<nsRunnableMethod<nsDocument, void, false> > event =
    NS_NewNonOwningRunnableMethod(this,
      &nsDocument::DoNotifyPossibleTitleChange);
  nsresult rv = NS_DispatchToCurrentThread(event);
  if (NS_SUCCEEDED(rv)) {
    mPendingTitleChangeEvent = event;
  }
}

void
nsDocument::DoNotifyPossibleTitleChange()
{
  mPendingTitleChangeEvent.Forget();
  mHaveFiredTitleChange = true;

  nsAutoString title;
  GetTitle(title);

  nsCOMPtr<nsIPresShell> shell = GetShell();
  if (shell) {
    nsCOMPtr<nsISupports> container = shell->GetPresContext()->GetContainer();
    if (container) {
      nsCOMPtr<nsIBaseWindow> docShellWin = do_QueryInterface(container);
      if (docShellWin) {
        docShellWin->SetTitle(title.get());
      }
    }
  }

  // Fire a DOM event for the title change.
  nsContentUtils::DispatchChromeEvent(this, static_cast<nsIDocument*>(this),
                                      NS_LITERAL_STRING("DOMTitleChanged"),
                                      true, true);
}

NS_IMETHODIMP
nsDocument::GetBoxObjectFor(nsIDOMElement* aElement, nsIBoxObject** aResult)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(aElement));
  NS_ENSURE_TRUE(content, NS_ERROR_UNEXPECTED);

  nsIDocument* doc = content->OwnerDoc();
  NS_ENSURE_TRUE(doc == this, NS_ERROR_DOM_WRONG_DOCUMENT_ERR);

  if (!mHasWarnedAboutBoxObjects && !content->IsXUL()) {
    mHasWarnedAboutBoxObjects = true;
    nsContentUtils::ReportToConsole(nsContentUtils::eDOM_PROPERTIES,
                                    "UseOfGetBoxObjectForWarning",
                                    nsnull, 0,
                                    nsnull,
                                    EmptyString(), 0, 0,
                                    nsIScriptError::warningFlag,
                                    "BoxObjects", this);
  }

  *aResult = nsnull;

  if (!mBoxObjectTable) {
    mBoxObjectTable = new nsInterfaceHashtable<nsVoidPtrHashKey, nsPIBoxObject>;
    if (mBoxObjectTable && !mBoxObjectTable->Init(12)) {
      mBoxObjectTable = nsnull;
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
  nsCOMPtr<nsIAtom> tag = BindingManager()->ResolveTag(content, &namespaceID);

  nsCAutoString contractID("@mozilla.org/layout/xul-boxobject");
  if (namespaceID == kNameSpaceID_XUL) {
    if (tag == nsGkAtoms::browser ||
        tag == nsGkAtoms::editor ||
        tag == nsGkAtoms::iframe)
      contractID += "-container";
    else if (tag == nsGkAtoms::menu)
      contractID += "-menu";
    else if (tag == nsGkAtoms::popup ||
             tag == nsGkAtoms::menupopup ||
             tag == nsGkAtoms::panel ||
             tag == nsGkAtoms::tooltip)
      contractID += "-popup";
    else if (tag == nsGkAtoms::tree)
      contractID += "-tree";
    else if (tag == nsGkAtoms::listbox)
      contractID += "-listbox";
    else if (tag == nsGkAtoms::scrollbox)
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

nsresult
nsDocument::GetXBLChildNodesFor(nsIContent* aContent, nsIDOMNodeList** aResult)
{
  return BindingManager()->GetXBLChildNodesFor(aContent, aResult);
}

nsresult
nsDocument::GetContentListFor(nsIContent* aContent, nsIDOMNodeList** aResult)
{
  return BindingManager()->GetContentListFor(aContent, aResult);
}

void
nsDocument::FlushSkinBindings()
{
  BindingManager()->FlushSkinBindings();
}

nsresult
nsDocument::InitializeFrameLoader(nsFrameLoader* aLoader)
{
  mInitializableFrameLoaders.RemoveElement(aLoader);
  // Don't even try to initialize.
  if (mInDestructor) {
    NS_WARNING("Trying to initialize a frame loader while"
               "document is being deleted");
    return NS_ERROR_FAILURE;
  }

  mInitializableFrameLoaders.AppendElement(aLoader);
  if (!mFrameLoaderRunner) {
    mFrameLoaderRunner =
      NS_NewRunnableMethod(this, &nsDocument::MaybeInitializeFinalizeFrameLoaders);
    NS_ENSURE_TRUE(mFrameLoaderRunner, NS_ERROR_OUT_OF_MEMORY);
    nsContentUtils::AddScriptRunner(mFrameLoaderRunner);
  }
  return NS_OK;
}

nsresult
nsDocument::FinalizeFrameLoader(nsFrameLoader* aLoader)
{
  mInitializableFrameLoaders.RemoveElement(aLoader);
  if (mInDestructor) {
    return NS_ERROR_FAILURE;
  }

  mFinalizableFrameLoaders.AppendElement(aLoader);
  if (!mFrameLoaderRunner) {
    mFrameLoaderRunner =
      NS_NewRunnableMethod(this, &nsDocument::MaybeInitializeFinalizeFrameLoaders);
    NS_ENSURE_TRUE(mFrameLoaderRunner, NS_ERROR_OUT_OF_MEMORY);
    nsContentUtils::AddScriptRunner(mFrameLoaderRunner);
  }
  return NS_OK;
}

void
nsDocument::MaybeInitializeFinalizeFrameLoaders()
{
  if (mDelayFrameLoaderInitialization || mUpdateNestLevel != 0) {
    // This method will be recalled when mUpdateNestLevel drops to 0,
    // or when !mDelayFrameLoaderInitialization.
    mFrameLoaderRunner = nsnull;
    return;
  }

  // We're not in an update, but it is not safe to run scripts, so
  // postpone frameloader initialization and finalization.
  if (!nsContentUtils::IsSafeToRunScript()) {
    if (!mInDestructor && !mFrameLoaderRunner &&
        (mInitializableFrameLoaders.Length() ||
         mFinalizableFrameLoaders.Length())) {
      mFrameLoaderRunner =
        NS_NewRunnableMethod(this, &nsDocument::MaybeInitializeFinalizeFrameLoaders);
      nsContentUtils::AddScriptRunner(mFrameLoaderRunner);
    }
    return;
  }
  mFrameLoaderRunner = nsnull;

  // Don't use a temporary array for mInitializableFrameLoaders, because
  // loading a frame may cause some other frameloader to be removed from the
  // array. But be careful to keep the loader alive when starting the load!
  while (mInitializableFrameLoaders.Length()) {
    nsRefPtr<nsFrameLoader> loader = mInitializableFrameLoaders[0];
    mInitializableFrameLoaders.RemoveElementAt(0);
    NS_ASSERTION(loader, "null frameloader in the array?");
    loader->ReallyStartLoading();
  }

  PRUint32 length = mFinalizableFrameLoaders.Length();
  if (length > 0) {
    nsTArray<nsRefPtr<nsFrameLoader> > loaders;
    mFinalizableFrameLoaders.SwapElements(loaders);
    for (PRUint32 i = 0; i < length; ++i) {
      loaders[i]->Finalize();
    }
  }
}

void
nsDocument::TryCancelFrameLoaderInitialization(nsIDocShell* aShell)
{
  PRUint32 length = mInitializableFrameLoaders.Length();
  for (PRUint32 i = 0; i < length; ++i) {
    if (mInitializableFrameLoaders[i]->GetExistingDocShell() == aShell) {
      mInitializableFrameLoaders.RemoveElementAt(i);
      return;
    }
  }
}

bool
nsDocument::FrameLoaderScheduledToBeFinalized(nsIDocShell* aShell)
{
  if (aShell) {
    PRUint32 length = mFinalizableFrameLoaders.Length();
    for (PRUint32 i = 0; i < length; ++i) {
      if (mFinalizableFrameLoaders[i]->GetExistingDocShell() == aShell) {
        return true;
      }
    }
  }
  return false;
}

nsIDocument*
nsDocument::RequestExternalResource(nsIURI* aURI,
                                    nsINode* aRequestingNode,
                                    ExternalResourceLoad** aPendingLoad)
{
  NS_PRECONDITION(aURI, "Must have a URI");
  NS_PRECONDITION(aRequestingNode, "Must have a node");
  if (mDisplayDocument) {
    return mDisplayDocument->RequestExternalResource(aURI,
                                                     aRequestingNode,
                                                     aPendingLoad);
  }

  return mExternalResourceMap.RequestResource(aURI, aRequestingNode,
                                              this, aPendingLoad);
}

void
nsDocument::EnumerateExternalResources(nsSubDocEnumFunc aCallback, void* aData)
{
  mExternalResourceMap.EnumerateResources(aCallback, aData);
}

nsSMILAnimationController*
nsDocument::GetAnimationController()
{
  // We create the animation controller lazily because most documents won't want
  // one and only SVG documents and the like will call this
  if (mAnimationController)
    return mAnimationController;
  // Refuse to create an Animation Controller if SMIL is disabled, and also
  // for data documents.
  if (!NS_SMILEnabled() || mLoadedAsData || mLoadedAsInteractiveData)
    return nsnull;

  mAnimationController = new nsSMILAnimationController(this);
  
  // If there's a presContext then check the animation mode and pause if
  // necessary.
  nsIPresShell *shell = GetShell();
  if (mAnimationController && shell) {
    nsPresContext *context = shell->GetPresContext();
    if (context &&
        context->ImageAnimationMode() == imgIContainer::kDontAnimMode) {
      mAnimationController->Pause(nsSMILTimeContainer::PAUSE_USERPREF);
    }
  }

  // If we're hidden (or being hidden), notify the newly-created animation
  // controller. (Skip this check for SVG-as-an-image documents, though,
  // because they don't get OnPageShow / OnPageHide calls).
  if (!mIsShowing && !mIsBeingUsedAsImage) {
    mAnimationController->OnPageHide();
  }

  return mAnimationController;
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
        nsIPresShell *shell = GetShell();
        if (shell) {
          nsPresContext *context = shell->GetPresContext();
          NS_ENSURE_TRUE(context, NS_ERROR_UNEXPECTED);
          context->SetBidi(options, true);
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
nsDocument::GetParentElement(nsIDOMElement** aParentElement)
{
  *aParentElement = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  return nsINode::GetChildNodes(aChildNodes);
}

NS_IMETHODIMP
nsDocument::HasChildNodes(bool* aHasChildNodes)
{
  NS_ENSURE_ARG(aHasChildNodes);

  *aHasChildNodes = (mChildren.ChildCount() != 0);

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::HasAttributes(bool* aHasAttributes)
{
  NS_ENSURE_ARG(aHasAttributes);

  *aHasAttributes = false;

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetFirstChild(nsIDOMNode** aFirstChild)
{
  return nsINode::GetFirstChild(aFirstChild);
}

NS_IMETHODIMP
nsDocument::GetLastChild(nsIDOMNode** aLastChild)
{
  return nsINode::GetLastChild(aLastChild);
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
nsDocument::GetLocalName(nsAString& aLocalName)
{
  SetDOMStringToNull(aLocalName);

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                         nsIDOMNode** aReturn)
{
  return ReplaceOrInsertBefore(false, aNewChild, aRefChild, aReturn);
}

NS_IMETHODIMP
nsDocument::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                         nsIDOMNode** aReturn)
{
  return ReplaceOrInsertBefore(true, aNewChild, aOldChild, aReturn);
}

NS_IMETHODIMP
nsDocument::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return nsINode::RemoveChild(aOldChild, aReturn);
}

NS_IMETHODIMP
nsDocument::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
  return nsDocument::InsertBefore(aNewChild, nsnull, aReturn);
}

NS_IMETHODIMP
nsDocument::CloneNode(bool aDeep, nsIDOMNode** aReturn)
{
  return nsNodeUtils::CloneNodeImpl(this, aDeep, !mCreatingStaticClone, aReturn);
}

NS_IMETHODIMP
nsDocument::Normalize()
{
  return nsIDocument::Normalize();
}

NS_IMETHODIMP
nsDocument::IsSupported(const nsAString& aFeature, const nsAString& aVersion,
                        bool* aReturn)
{
  return nsGenericElement::InternalIsSupported(static_cast<nsIDOMDocument*>(this),
                                               aFeature, aVersion, aReturn);
}

NS_IMETHODIMP
nsDocument::GetDOMBaseURI(nsAString &aURI)
{
  return nsIDocument::GetDOMBaseURI(aURI);
}

NS_IMETHODIMP
nsDocument::GetTextContent(nsAString &aTextContent)
{
  SetDOMStringToNull(aTextContent);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::IsEqualNode(nsIDOMNode* aOther, bool* aResult)
{
  return nsINode::IsEqualNode(aOther, aResult);
}

NS_IMETHODIMP
nsDocument::CompareDocumentPosition(nsIDOMNode *other,
                                   PRUint16 *aResult)
{
  return nsINode::CompareDocumentPosition(other, aResult);
}

NS_IMETHODIMP
nsDocument::SetTextContent(const nsAString & aTextContent)
{
  return nsINode::SetTextContent(aTextContent);
}

NS_IMETHODIMP
nsDocument::LookupPrefix(const nsAString & namespaceURI, nsAString & aResult)
{
  SetDOMStringToNull(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::IsDefaultNamespace(const nsAString & namespaceURI,
                              bool *aResult)
{
  *aResult = namespaceURI.IsEmpty();
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::LookupNamespaceURI(const nsAString & prefix,
                              nsAString & aResult)
{
  SetDOMStringToNull(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::SetUserData(const nsAString & key,
                       nsIVariant *data, nsIDOMUserDataHandler *handler,
                       nsIVariant **aResult)
{
  return nsINode::SetUserData(key, data, handler, aResult);
}

NS_IMETHODIMP
nsDocument::GetUserData(const nsAString & key,
                        nsIVariant **aResult)
{
  return nsINode::GetUserData(key, aResult);
}

NS_IMETHODIMP
nsDocument::Contains(nsIDOMNode* aOther, bool* aReturn)
{
  return nsINode::Contains(aOther, aReturn);
}

NS_IMETHODIMP
nsDocument::GetInputEncoding(nsAString& aInputEncoding)
{
  WarnOnceAbout(eInputEncoding);
  if (mHaveInputEncoding) {
    return GetCharacterSet(aInputEncoding);
  }

  SetDOMStringToNull(aInputEncoding);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetMozSyntheticDocument(bool *aSyntheticDocument)
{
  *aSyntheticDocument = mIsSyntheticDocument;
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

static void BlastSubtreeToPieces(nsINode *aNode);

PLDHashOperator
BlastFunc(nsAttrHashKey::KeyType aKey, nsDOMAttribute *aData, void* aUserArg)
{
  nsCOMPtr<nsIAttribute> *attr =
    static_cast<nsCOMPtr<nsIAttribute>*>(aUserArg);

  *attr = aData;

  NS_ASSERTION(attr->get(),
               "non-nsIAttribute somehow made it into the hashmap?!");

  return PL_DHASH_STOP;
}

static void
BlastSubtreeToPieces(nsINode *aNode)
{
  PRUint32 i, count;
  if (aNode->IsElement()) {
    nsGenericElement *element = static_cast<nsGenericElement*>(aNode);
    const nsDOMAttributeMap *map = element->GetAttributeMap();
    if (map) {
      nsCOMPtr<nsIAttribute> attr;
      while (map->Enumerate(BlastFunc, &attr) > 0) {
        BlastSubtreeToPieces(attr);

#ifdef DEBUG
        nsresult rv =
#endif
          element->UnsetAttr(attr->NodeInfo()->NamespaceID(),
                             attr->NodeInfo()->NameAtom(),
                             false);

        // XXX Should we abort here?
        NS_ASSERTION(NS_SUCCEEDED(rv), "Uhoh, UnsetAttr shouldn't fail!");
      }
    }
  }

  count = aNode->GetChildCount();
  for (i = 0; i < count; ++i) {
    BlastSubtreeToPieces(aNode->GetFirstChild());
#ifdef DEBUG
    nsresult rv =
#endif
      aNode->RemoveChildAt(0, false);

    // XXX Should we abort here?
    NS_ASSERTION(NS_SUCCEEDED(rv), "Uhoh, RemoveChildAt shouldn't fail!");
  }
}


class nsUserDataCaller : public nsRunnable
{
public:
  nsUserDataCaller(nsCOMArray<nsINode>& aNodesWithProperties,
                   nsIDocument* aOwnerDoc)
    : mNodesWithProperties(aNodesWithProperties),
      mOwnerDoc(aOwnerDoc)
  {
  }

  NS_IMETHOD Run()
  {
    nsNodeUtils::CallUserDataHandlers(mNodesWithProperties, mOwnerDoc,
                                      nsIDOMUserDataHandler::NODE_ADOPTED,
                                      false);
    return NS_OK;
  }

private:
  nsCOMArray<nsINode> mNodesWithProperties;
  nsCOMPtr<nsIDocument> mOwnerDoc;
};

NS_IMETHODIMP
nsDocument::AdoptNode(nsIDOMNode *aAdoptedNode, nsIDOMNode **aResult)
{
  NS_ENSURE_ARG(aAdoptedNode);

  *aResult = nsnull;

  nsresult rv = nsContentUtils::CheckSameOrigin(this, aAdoptedNode);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsINode> adoptedNode = do_QueryInterface(aAdoptedNode);

  // Scope firing mutation events so that we don't carry any state that
  // might be stale
  {
    nsINode* parent = adoptedNode->GetNodeParent();
    if (parent) {
      nsContentUtils::MaybeFireNodeRemoved(adoptedNode, parent,
                                           adoptedNode->OwnerDoc());
    }
  }

  nsAutoScriptBlocker scriptBlocker;

  PRUint16 nodeType;
  aAdoptedNode->GetNodeType(&nodeType);
  switch (nodeType) {
    case nsIDOMNode::ATTRIBUTE_NODE:
    {
      // Remove from ownerElement.
      nsCOMPtr<nsIDOMAttr> adoptedAttr = do_QueryInterface(aAdoptedNode);
      NS_ASSERTION(adoptedAttr, "Attribute not implementing nsIDOMAttr");

      nsCOMPtr<nsIDOMElement> ownerElement;
      rv = adoptedAttr->GetOwnerElement(getter_AddRefs(ownerElement));
      NS_ENSURE_SUCCESS(rv, rv);

      if (ownerElement) {
        nsCOMPtr<nsIDOMAttr> newAttr;
        rv = ownerElement->RemoveAttributeNode(adoptedAttr,
                                               getter_AddRefs(newAttr));
        NS_ENSURE_SUCCESS(rv, rv);

        newAttr.swap(adoptedAttr);
      }

      break;
    }
    case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
    case nsIDOMNode::ELEMENT_NODE:
    case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
    case nsIDOMNode::TEXT_NODE:
    case nsIDOMNode::CDATA_SECTION_NODE:
    case nsIDOMNode::COMMENT_NODE:
    case nsIDOMNode::DOCUMENT_TYPE_NODE:
    {
      // We don't want to adopt an element into its own contentDocument or into
      // a descendant contentDocument, so we check if the frameElement of this
      // document or any of its parents is the adopted node or one of its
      // descendants.
      nsIDocument *doc = this;
      do {
        nsPIDOMWindow *win = doc->GetWindow();
        if (win) {
          nsCOMPtr<nsINode> node =
            do_QueryInterface(win->GetFrameElementInternal());
          if (node &&
              nsContentUtils::ContentIsDescendantOf(node, adoptedNode)) {
            return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
          }
        }
      } while ((doc = doc->GetParentDocument()));

      // Remove from parent.
      nsCOMPtr<nsINode> parent = adoptedNode->GetNodeParent();
      if (parent) {
        rv = parent->RemoveChildAt(parent->IndexOf(adoptedNode), true);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      break;
    }
    case nsIDOMNode::ENTITY_REFERENCE_NODE:
    {
      return NS_ERROR_NOT_IMPLEMENTED;
    }
    case nsIDOMNode::DOCUMENT_NODE:
    case nsIDOMNode::ENTITY_NODE:
    case nsIDOMNode::NOTATION_NODE:
    {
      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }
    default:
    {
      NS_WARNING("Don't know how to adopt this nodetype for adoptNode.");

      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }
  }

  nsIDocument *oldDocument = adoptedNode->OwnerDoc();
  bool sameDocument = oldDocument == this;

  JSContext *cx = nsnull;
  JSObject *newScope = nsnull;
  if (!sameDocument) {
    rv = nsContentUtils::GetContextAndScope(oldDocument, this, &cx, &newScope);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMArray<nsINode> nodesWithProperties;
  rv = nsNodeUtils::Adopt(adoptedNode, sameDocument ? nsnull : mNodeInfoManager,
                          cx, newScope, nodesWithProperties);
  if (NS_FAILED(rv)) {
    // Disconnect all nodes from their parents, since some have the old document
    // as their ownerDocument and some have this as their ownerDocument.
    BlastSubtreeToPieces(adoptedNode);

    if (!sameDocument && oldDocument) {
      PRUint32 count = nodesWithProperties.Count();
      for (PRUint32 j = 0; j < oldDocument->GetPropertyTableCount(); ++j) {
        for (PRUint32 i = 0; i < count; ++i) {
          // Remove all properties.
          oldDocument->PropertyTable(j)->
            DeleteAllPropertiesFor(nodesWithProperties[i]);
        }
      }
    }

    return rv;
  }

  PRUint32 count = nodesWithProperties.Count();
  if (!sameDocument && oldDocument) {
    for (PRUint32 j = 0; j < oldDocument->GetPropertyTableCount(); ++j) {
      nsPropertyTable *oldTable = oldDocument->PropertyTable(j);
      nsPropertyTable *newTable = PropertyTable(j);
      for (PRUint32 i = 0; i < count; ++i) {
        if (NS_SUCCEEDED(rv)) {
          rv = oldTable->TransferOrDeleteAllPropertiesFor(nodesWithProperties[i],
                                                          newTable);
        } else {
          oldTable->DeleteAllPropertiesFor(nodesWithProperties[i]);
        }
      }
    }

    if (NS_FAILED(rv)) {
      // Disconnect all nodes from their parents.
      BlastSubtreeToPieces(adoptedNode);

      return rv;
    }
  }

  if (nodesWithProperties.Count()) {
    nsContentUtils::AddScriptRunner(new nsUserDataCaller(nodesWithProperties,
                                                         this));
  }

  NS_ASSERTION(adoptedNode->OwnerDoc() == this,
               "Should still be in the document we just got adopted into");

  return CallQueryInterface(adoptedNode, aResult);
}

NS_IMETHODIMP
nsDocument::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  return nsINode::GetOwnerDocument(aOwnerDocument);
}

nsEventListenerManager*
nsDocument::GetListenerManager(bool aCreateIfNotFound)
{
  if (!mListenerManager && aCreateIfNotFound) {
    mListenerManager =
      new nsEventListenerManager(static_cast<nsIDOMEventTarget*>(this));
  }

  return mListenerManager;
}

nsresult
nsDocument::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = true;
   // FIXME! This is a hack to make middle mouse paste working also in Editor.
   // Bug 329119
  aVisitor.mForceContentDispatch = true;

  // Load events must not propagate to |window| object, see bug 335251.
  if (aVisitor.mEvent->message != NS_LOAD) {
    nsGlobalWindow* window = static_cast<nsGlobalWindow*>(GetWindow());
    aVisitor.mParentTarget = static_cast<nsIDOMEventTarget*>(window);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::CreateEvent(const nsAString& aEventType, nsIDOMEvent** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  // Obtain a presentation shell

  nsIPresShell *shell = GetShell();

  nsPresContext *presContext = nsnull;

  if (shell) {
    // Retrieve the context
    presContext = shell->GetPresContext();
  }

  // Create event even without presContext.
  return nsEventDispatcher::CreateEvent(presContext, nsnull,
                                        aEventType, aReturn);
}

void
nsDocument::FlushPendingNotifications(mozFlushType aType)
{
  if ((!IsHTML() || aType > Flush_ContentAndNotify) &&
      (mParser || mWeakSink)) {
    nsCOMPtr<nsIContentSink> sink;
    if (mParser) {
      sink = mParser->GetContentSink();
    } else {
      sink = do_QueryReferent(mWeakSink);
      if (!sink) {
        mWeakSink = nsnull;
      }
    }
    // Determine if it is safe to flush the sink notifications
    // by determining if it safe to flush all the presshells.
    if (sink && (aType == Flush_Content || IsSafeToFlush())) {
      sink->FlushPendingNotifications(aType);
    }
  }

  // Should we be flushing pending binding constructors in here?

  if (aType <= Flush_ContentAndNotify) {
    // Nothing to do here
    return;
  }

  // If we have a parent we must flush the parent too to ensure that our
  // container is reflowed if its size was changed.  But if it's not safe to
  // flush ourselves, then don't flush the parent, since that can cause things
  // like resizes of our frame's widget, which we can't handle while flushing
  // is unsafe.
  // Since media queries mean that a size change of our container can
  // affect style, we need to promote a style flush on ourself to a
  // layout flush on our parent, since we need our container to be the
  // correct size to determine the correct style.
  if (mParentDocument && IsSafeToFlush()) {
    mozFlushType parentType = aType;
    if (aType >= Flush_Style)
      parentType = NS_MAX(Flush_Layout, aType);
    mParentDocument->FlushPendingNotifications(parentType);
  }

  nsCOMPtr<nsIPresShell> shell = GetShell();
  if (shell) {
    shell->FlushPendingNotifications(aType);
  }
}

static bool
Flush(nsIDocument* aDocument, void* aData)
{
  const mozFlushType* type = static_cast<const mozFlushType*>(aData);
  aDocument->FlushPendingNotifications(*type);
  return true;
}

void
nsDocument::FlushExternalResources(mozFlushType aType)
{
  NS_ASSERTION(aType >= Flush_Style,
    "should only need to flush for style or higher in external resources");

  if (GetDisplayDocument()) {
    return;
  }
  EnumerateExternalResources(Flush, &aType);
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

bool
nsDocument::IsScriptEnabled()
{
  nsCOMPtr<nsIScriptSecurityManager> sm(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID));
  NS_ENSURE_TRUE(sm, false);

  nsIScriptGlobalObject* globalObject = GetScriptGlobalObject();
  NS_ENSURE_TRUE(globalObject, false);

  nsIScriptContext *scriptContext = globalObject->GetContext();
  NS_ENSURE_TRUE(scriptContext, false);

  JSContext* cx = scriptContext->GetNativeContext();
  NS_ENSURE_TRUE(cx, false);

  bool enabled;
  nsresult rv = sm->CanExecuteScripts(cx, NodePrincipal(), &enabled);
  NS_ENSURE_SUCCESS(rv, false);
  return enabled;
}

nsresult
nsDocument::GetRadioGroup(const nsAString& aName,
                          nsRadioGroupStruct **aRadioGroup)
{
  nsAutoString tmKey(aName);
  if(IsHTML())
     ToLowerCase(tmKey); //should case-insensitive.
  if (mRadioGroups.Get(tmKey, aRadioGroup))
    return NS_OK;

  nsAutoPtr<nsRadioGroupStruct> radioGroup(new nsRadioGroupStruct());
  NS_ENSURE_TRUE(radioGroup, NS_ERROR_OUT_OF_MEMORY);
  NS_ENSURE_TRUE(mRadioGroups.Put(tmKey, radioGroup), NS_ERROR_OUT_OF_MEMORY);

  *aRadioGroup = radioGroup;
  radioGroup.forget();

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
                               const bool aPrevious,
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
  bool disabled;
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
    radio = do_QueryInterface(radioGroup->mRadioButtons[index]);
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
    radioGroup->mRadioButtons.AppendObject(aRadio);

    nsCOMPtr<nsIContent> element = do_QueryInterface(aRadio);
    NS_ASSERTION(element, "radio controls have to be content elements");
    if (element->HasAttr(kNameSpaceID_None, nsGkAtoms::required)) {
      radioGroup->mRequiredRadioCount++;
    }
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
    radioGroup->mRadioButtons.RemoveObject(aRadio);

    nsCOMPtr<nsIContent> element = do_QueryInterface(aRadio);
    NS_ASSERTION(element, "radio controls have to be content elements");
    if (element->HasAttr(kNameSpaceID_None, nsGkAtoms::required)) {
      radioGroup->mRequiredRadioCount--;
      NS_ASSERTION(radioGroup->mRequiredRadioCount >= 0,
                   "mRequiredRadioCount shouldn't be negative!");
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::WalkRadioGroup(const nsAString& aName,
                           nsIRadioVisitor* aVisitor,
                           bool aFlushContent)
{
  nsRadioGroupStruct* radioGroup = nsnull;
  GetRadioGroup(aName, &radioGroup);
  if (!radioGroup) {
    return NS_OK;
  }

  for (int i = 0; i < radioGroup->mRadioButtons.Count(); i++) {
    if (!aVisitor->Visit(radioGroup->mRadioButtons[i])) {
      return NS_OK;
    }
  }

  return NS_OK;
}

PRUint32
nsDocument::GetRequiredRadioCount(const nsAString& aName) const
{
  nsRadioGroupStruct* radioGroup = nsnull;
  // TODO: we should call GetRadioGroup here (and make it const) but for that
  // we would need to have an explicit CreateRadioGroup() instead of create
  // one when GetRadioGroup is called. See bug 636123.
  nsAutoString tmKey(aName);
  if (IsHTML())
     ToLowerCase(tmKey); //should case-insensitive.
  mRadioGroups.Get(tmKey, &radioGroup);

  return radioGroup ? radioGroup->mRequiredRadioCount : 0;
}

void
nsDocument::RadioRequiredChanged(const nsAString& aName, nsIFormControl* aRadio)
{
  nsRadioGroupStruct* radioGroup = nsnull;
  GetRadioGroup(aName, &radioGroup);

  if (!radioGroup) {
    return;
  }

  nsCOMPtr<nsIContent> element = do_QueryInterface(aRadio);
  NS_ASSERTION(element, "radio controls have to be content elements");
  if (element->HasAttr(kNameSpaceID_None, nsGkAtoms::required)) {
    radioGroup->mRequiredRadioCount++;
  } else {
    radioGroup->mRequiredRadioCount--;
    NS_ASSERTION(radioGroup->mRequiredRadioCount >= 0,
                 "mRequiredRadioCount shouldn't be negative!");
  }
}

bool
nsDocument::GetValueMissingState(const nsAString& aName) const
{
  nsRadioGroupStruct* radioGroup = nsnull;
  // TODO: we should call GetRadioGroup here (and make it const) but for that
  // we would need to have an explicit CreateRadioGroup() instead of create
  // one when GetRadioGroup is called. See bug 636123.
  nsAutoString tmKey(aName);
  if (IsHTML())
     ToLowerCase(tmKey); //should case-insensitive.
  mRadioGroups.Get(tmKey, &radioGroup);

  return radioGroup && radioGroup->mGroupSuffersFromValueMissing;
}

void
nsDocument::SetValueMissingState(const nsAString& aName, bool aValue)
{
  nsRadioGroupStruct* radioGroup = nsnull;
  GetRadioGroup(aName, &radioGroup);

  if (!radioGroup) {
    return;
  }

  radioGroup->mGroupSuffersFromValueMissing = aValue;
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
      PRStatus st = PR_ParseTimeString(tmp.get(), true, &time);
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
      "x-dns-prefetch-control",
      "x-content-security-policy",
      "x-content-security-policy-report-only",
      "x-frame-options",
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
      nsCAutoString contentDisp;
      rv = aChannel->GetContentDispositionHeader(contentDisp);
      if (NS_SUCCEEDED(rv)) {
        SetHeaderData(nsGkAtoms::headerContentDisposition,
                      NS_ConvertASCIItoUTF16(contentDisp));
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
    char formatedTime[24];
    if (PR_snprintf(formatedTime, sizeof(formatedTime),
                    "%02ld/%02ld/%04hd %02ld:%02ld:%02ld",
                    prtime.tm_month + 1, prtime.tm_mday, prtime.tm_year,
                    prtime.tm_hour     ,  prtime.tm_min,  prtime.tm_sec)) {
      CopyASCIItoUTF16(nsDependentCString(formatedTime), mLastModified);
    }
  }
}

nsresult
nsDocument::CreateElem(const nsAString& aName, nsIAtom *aPrefix, PRInt32 aNamespaceID,
                       nsIContent **aResult)
{
#ifdef DEBUG
  nsAutoString qName;
  if (aPrefix) {
    aPrefix->ToString(qName);
    qName.Append(':');
  }
  qName.Append(aName);

  // Note: "a:b:c" is a valid name in non-namespaces XML, and
  // nsDocument::CreateElement can call us with such a name and no prefix,
  // which would cause an error if we just used true here.
  bool nsAware = aPrefix != nsnull || aNamespaceID != GetDefaultNamespaceID();
  NS_ASSERTION(NS_SUCCEEDED(nsContentUtils::CheckQName(qName, nsAware)),
               "Don't pass invalid prefixes to nsDocument::CreateElem, "
               "check caller.");
#endif

  *aResult = nsnull;
  
  nsCOMPtr<nsINodeInfo> nodeInfo;
  mNodeInfoManager->GetNodeInfo(aName, aPrefix, aNamespaceID,
                                nsIDOMNode::ELEMENT_NODE,
                                getter_AddRefs(nodeInfo));
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

  return NS_NewElement(aResult, nodeInfo.forget(), NOT_FROM_PARSER);
}

bool
nsDocument::IsSafeToFlush() const
{
  nsCOMPtr<nsIPresShell> shell = GetShell();
  if (!shell)
    return true;

  return shell->IsSafeToFlush();
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

    bool resetValue = false;

    input->GetAttribute(NS_LITERAL_STRING("autocomplete"), value);
    if (value.LowerCaseEqualsLiteral("off")) {
      resetValue = true;
    } else {
      input->GetType(value);
      if (value.LowerCaseEqualsLiteral("password"))
        resetValue = true;
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

static PLDHashOperator
SubDocHashEnum(PLDHashTable *table, PLDHashEntryHdr *hdr,
               PRUint32 number, void *arg)
{
  SubDocMapEntry *entry = static_cast<SubDocMapEntry*>(hdr);
  SubDocEnumArgs *args = static_cast<SubDocEnumArgs*>(arg);

  nsIDocument *subdoc = entry->mSubDocument;
  bool next = subdoc ? args->callback(subdoc, args->data) : true;

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

static PLDHashOperator
CanCacheSubDocument(PLDHashTable *table, PLDHashEntryHdr *hdr,
                    PRUint32 number, void *arg)
{
  SubDocMapEntry *entry = static_cast<SubDocMapEntry*>(hdr);
  bool *canCacheArg = static_cast<bool*>(arg);

  nsIDocument *subdoc = entry->mSubDocument;

  // The aIgnoreRequest we were passed is only for us, so don't pass it on.
  bool canCache = subdoc ? subdoc->CanSavePresentation(nsnull) : false;
  if (!canCache) {
    *canCacheArg = false;
    return PL_DHASH_STOP;
  }

  return PL_DHASH_NEXT;
}

#ifdef DEBUG_bryner
#define DEBUG_PAGE_CACHE
#endif

bool
nsDocument::CanSavePresentation(nsIRequest *aNewRequest)
{
  if (EventHandlingSuppressed()) {
    return false;
  }

  nsPIDOMWindow* win = GetInnerWindow();
  if (win && win->TimeoutSuspendCount()) {
    return false;
  }

  // Check our event listener manager for unload/beforeunload listeners.
  nsCOMPtr<nsIDOMEventTarget> piTarget = do_QueryInterface(mScriptGlobalObject);
  if (piTarget) {
    nsEventListenerManager* manager =
      piTarget->GetListenerManager(false);
    if (manager && manager->HasUnloadListeners()) {
      return false;
    }
  }

  // Check if we have pending network requests
  nsCOMPtr<nsILoadGroup> loadGroup = GetDocumentLoadGroup();
  if (loadGroup) {
    nsCOMPtr<nsISimpleEnumerator> requests;
    loadGroup->GetRequests(getter_AddRefs(requests));

    bool hasMore = false;

    // We want to bail out if we have any requests other than aNewRequest (or
    // in the case when aNewRequest is a part of a multipart response the base
    // channel the multipart response is coming in on).
    nsCOMPtr<nsIChannel> baseChannel;
    nsCOMPtr<nsIMultiPartChannel> part(do_QueryInterface(aNewRequest));
    if (part) {
      part->GetBaseChannel(getter_AddRefs(baseChannel));
    }

    while (NS_SUCCEEDED(requests->HasMoreElements(&hasMore)) && hasMore) {
      nsCOMPtr<nsISupports> elem;
      requests->GetNext(getter_AddRefs(elem));

      nsCOMPtr<nsIRequest> request = do_QueryInterface(elem);
      if (request && request != aNewRequest && request != baseChannel) {
#ifdef DEBUG_PAGE_CACHE
        nsCAutoString requestName, docSpec;
        request->GetName(requestName);
        if (mDocumentURI)
          mDocumentURI->GetSpec(docSpec);

        printf("document %s has request %s\n",
               docSpec.get(), requestName.get());
#endif
        return false;
      }
    }
  }

  // Check if we have running IndexedDB transactions
  indexedDB::IndexedDatabaseManager* idbManager =
    indexedDB::IndexedDatabaseManager::Get();
  if (idbManager && idbManager->HasOpenTransactions(win)) {
    return false;
  }

  bool canCache = true;
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

  mIsGoingAway = true;

  RemovedFromDocShell();

  bool oldVal = mInUnlinkOrDeletion;
  mInUnlinkOrDeletion = true;
  PRUint32 i, count = mChildren.ChildCount();
  for (i = 0; i < count; ++i) {
    mChildren.ChildAt(i)->DestroyContent();
  }
  mInUnlinkOrDeletion = oldVal;

  mLayoutHistoryState = nsnull;

  // Shut down our external resource map.  We might not need this for
  // leak-fixing if we fix DocumentViewerImpl to do cycle-collection, but
  // tearing down all those frame trees right now is the right thing to do.
  mExternalResourceMap.Shutdown();

  // XXX We really should let cycle collection do this, but that currently still
  //     leaks (see https://bugzilla.mozilla.org/show_bug.cgi?id=406684).
  nsContentUtils::ReleaseWrapper(static_cast<nsINode*>(this), this);
}

void
nsDocument::RemovedFromDocShell()
{
  if (mRemovedFromDocShell)
    return;

  mRemovedFromDocShell = true;
  EnumerateFreezableElements(NotifyActivityChanged, nsnull); 

  PRUint32 i, count = mChildren.ChildCount();
  for (i = 0; i < count; ++i) {
    mChildren.ChildAt(i)->SaveSubtreeState();
  }
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
nsDocument::EnsureOnloadBlocker()
{
  // If mScriptGlobalObject is null, we shouldn't be messing with the loadgroup
  // -- it's not ours.
  if (mOnloadBlockCount != 0 && mScriptGlobalObject) {
    nsCOMPtr<nsILoadGroup> loadGroup = GetDocumentLoadGroup();
    if (loadGroup) {
      // Check first to see if mOnloadBlocker is in the loadgroup.
      nsCOMPtr<nsISimpleEnumerator> requests;
      loadGroup->GetRequests(getter_AddRefs(requests));

      bool hasMore = false;
      while (NS_SUCCEEDED(requests->HasMoreElements(&hasMore)) && hasMore) {
        nsCOMPtr<nsISupports> elem;
        requests->GetNext(getter_AddRefs(elem));
        nsCOMPtr<nsIRequest> request = do_QueryInterface(elem);
        if (request && request == mOnloadBlocker) {
          return;
        }
      }

      // Not in the loadgroup, so add it.
      loadGroup->AddRequest(mOnloadBlocker, nsnull);
    }
  }
}

void
nsDocument::AsyncBlockOnload()
{
  while (mAsyncOnloadBlockCount) {
    --mAsyncOnloadBlockCount;
    BlockOnload();
  }
}

void
nsDocument::BlockOnload()
{
  if (mDisplayDocument) {
    mDisplayDocument->BlockOnload();
    return;
  }
  
  // If mScriptGlobalObject is null, we shouldn't be messing with the loadgroup
  // -- it's not ours.
  if (mOnloadBlockCount == 0 && mScriptGlobalObject) {
    if (!nsContentUtils::IsSafeToRunScript()) {
      // Because AddRequest may lead to OnStateChange calls in chrome,
      // block onload only when there are no script blockers.
      ++mAsyncOnloadBlockCount;
      if (mAsyncOnloadBlockCount == 1) {
        nsContentUtils::AddScriptRunner(
          NS_NewRunnableMethod(this, &nsDocument::AsyncBlockOnload));
      }
      return;
    }
    nsCOMPtr<nsILoadGroup> loadGroup = GetDocumentLoadGroup();
    if (loadGroup) {
      loadGroup->AddRequest(mOnloadBlocker, nsnull);
    }
  }
  ++mOnloadBlockCount;      
}

void
nsDocument::UnblockOnload(bool aFireSync)
{
  if (mDisplayDocument) {
    mDisplayDocument->UnblockOnload(aFireSync);
    return;
  }

  if (mOnloadBlockCount == 0 && mAsyncOnloadBlockCount == 0) {
    NS_NOTREACHED("More UnblockOnload() calls than BlockOnload() calls; dropping call");
    return;
  }

  --mOnloadBlockCount;

  // If mScriptGlobalObject is null, we shouldn't be messing with the loadgroup
  // -- it's not ours.
  if (mOnloadBlockCount == 0 && mScriptGlobalObject) {
    if (aFireSync && mAsyncOnloadBlockCount == 0) {
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
  NS_PRECONDITION(!mDisplayDocument,
                  "Shouldn't get here for resource document");
  NS_PRECONDITION(mOnloadBlockCount != 0,
                  "Shouldn't have a count of zero here, since we stabilized in "
                  "PostUnblockOnloadEvent");
  
  --mOnloadBlockCount;

  if (mOnloadBlockCount != 0) {
    // We blocked again after the last unblock.  Nothing to do here.  We'll
    // post a new event when we unblock again.
    return;
  }

  if (mAsyncOnloadBlockCount != 0) {
    // We need to wait until the async onload block has been handled.
    PostUnblockOnloadEvent();
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

/* See if document is a child of this.  If so, return the frame element in this
 * document that holds currentDoc (or an ancestor). */
already_AddRefed<nsIDOMElement>
nsDocument::CheckAncestryAndGetFrame(nsIDocument* aDocument) const
{
  nsIDocument* parentDoc;
  for (parentDoc = aDocument->GetParentDocument();
       parentDoc != static_cast<const nsIDocument* const>(this);
       parentDoc = parentDoc->GetParentDocument()) {
    if (!parentDoc) {
      return nsnull;
    }

    aDocument = parentDoc;
  }

  // In a child document.  Get the appropriate frame.
  nsPIDOMWindow* currentWindow = aDocument->GetWindow();
  if (!currentWindow) {
    return nsnull;
  }
  nsIDOMElement* frameElement = currentWindow->GetFrameElementInternal();
  if (!frameElement) {
    return nsnull;
  }

  // Sanity check result
  nsCOMPtr<nsIDOMDocument> domDocument;
  frameElement->GetOwnerDocument(getter_AddRefs(domDocument));
  if (domDocument != this) {
    NS_ERROR("Child documents should live in windows the parent owns");
    return nsnull;
  }

  NS_ADDREF(frameElement);
  return frameElement;
}

void
nsDocument::DispatchPageTransition(nsIDOMEventTarget* aDispatchTarget,
                                   const nsAString& aType,
                                   bool aPersisted)
{
  if (aDispatchTarget) {
    nsCOMPtr<nsIDOMEvent> event;
    CreateEvent(NS_LITERAL_STRING("pagetransition"), getter_AddRefs(event));
    nsCOMPtr<nsIDOMPageTransitionEvent> ptEvent = do_QueryInterface(event);
    nsCOMPtr<nsIPrivateDOMEvent> pEvent = do_QueryInterface(ptEvent);
    if (pEvent && NS_SUCCEEDED(ptEvent->InitPageTransitionEvent(aType, true,
                                                                true,
                                                                aPersisted))) {
      pEvent->SetTrusted(true);
      pEvent->SetTarget(this);
      nsEventDispatcher::DispatchDOMEvent(aDispatchTarget, nsnull, event,
                                          nsnull, nsnull);
    }
  }
}

static bool
NotifyPageShow(nsIDocument* aDocument, void* aData)
{
  const bool* aPersistedPtr = static_cast<const bool*>(aData);
  aDocument->OnPageShow(*aPersistedPtr, nsnull);
  return true;
}

void
nsDocument::OnPageShow(bool aPersisted,
                       nsIDOMEventTarget* aDispatchStartTarget)
{
  mVisible = true;

  EnumerateFreezableElements(NotifyActivityChanged, nsnull);
  EnumerateExternalResources(NotifyPageShow, &aPersisted);

  Element* root = GetRootElement();
  if (aPersisted && root) {
    // Send out notifications that our <link> elements are attached.
    nsRefPtr<nsContentList> links = NS_GetContentList(root,
                                                      kNameSpaceID_Unknown,
                                                      NS_LITERAL_STRING("link"));

    PRUint32 linkCount = links->Length(true);
    for (PRUint32 i = 0; i < linkCount; ++i) {
      nsCOMPtr<nsILink> link = do_QueryInterface(links->Item(i, false));
      if (link) {
        link->LinkAdded();
      }
    }
  }

  // See nsIDocument
  if (!aDispatchStartTarget) {
    // Set mIsShowing before firing events, in case those event handlers
    // move us around.
    mIsShowing = true;
  }
 
  if (mAnimationController) {
    mAnimationController->OnPageShow();
  }

  if (aPersisted) {
    SetImagesNeedAnimating(true);
  }

  UpdateVisibilityState();

  nsCOMPtr<nsIDOMEventTarget> target = aDispatchStartTarget;
  if (!target) {
    target = do_QueryInterface(GetWindow());
  }
  DispatchPageTransition(target, NS_LITERAL_STRING("pageshow"), aPersisted);
}

static bool
NotifyPageHide(nsIDocument* aDocument, void* aData)
{
  const bool* aPersistedPtr = static_cast<const bool*>(aData);
  aDocument->OnPageHide(*aPersistedPtr, nsnull);
  return true;
}

static bool
SetFullScreenState(nsIDocument* aDoc, Element* aElement, bool aIsFullScreen);

static void
SetWindowFullScreen(nsIDocument* aDoc, bool aValue);

static bool
ResetFullScreen(nsIDocument* aDocument, void* aData) {
  if (aDocument->IsFullScreenDoc()) {
    ::SetFullScreenState(aDocument, nsnull, false);
    aDocument->EnumerateSubDocuments(ResetFullScreen, nsnull);
  }
  return true;
}

void
nsDocument::OnPageHide(bool aPersisted,
                       nsIDOMEventTarget* aDispatchStartTarget)
{
  // Send out notifications that our <link> elements are detached,
  // but only if this is not a full unload.
  Element* root = GetRootElement();
  if (aPersisted && root) {
    nsRefPtr<nsContentList> links = NS_GetContentList(root,
                                                      kNameSpaceID_Unknown,
                                                      NS_LITERAL_STRING("link"));

    PRUint32 linkCount = links->Length(true);
    for (PRUint32 i = 0; i < linkCount; ++i) {
      nsCOMPtr<nsILink> link = do_QueryInterface(links->Item(i, false));
      if (link) {
        link->LinkRemoved();
      }
    }
  }

  // See nsIDocument
  if (!aDispatchStartTarget) {
    // Set mIsShowing before firing events, in case those event handlers
    // move us around.
    mIsShowing = false;
  }

  if (mAnimationController) {
    mAnimationController->OnPageHide();
  }
  
  if (aPersisted) {
    SetImagesNeedAnimating(false);
  }

  // Now send out a PageHide event.
  nsCOMPtr<nsIDOMEventTarget> target = aDispatchStartTarget;
  if (!target) {
    target = do_QueryInterface(GetWindow());
  }
  DispatchPageTransition(target, NS_LITERAL_STRING("pagehide"), aPersisted);

  mVisible = false;

  UpdateVisibilityState();
  
  EnumerateExternalResources(NotifyPageHide, &aPersisted);
  EnumerateFreezableElements(NotifyActivityChanged, nsnull);

  if (IsFullScreenDoc()) {
    // A full-screen doc has been hidden. We need to ensure we exit
    // full-screen, i.e. remove full-screen state from all full-screen
    // documents, and exit the top-level window from full-screen mode.
    // Unfortunately by the time a doc is hidden, it has been removed
    // from the doc tree, so we can't just call CancelFullScreen()...

    // So firstly reset full-screen state in *this* document. OnPageHide()
    // is called in every hidden document, so doing this ensures all hidden
    // documents have their state reset.
    ::SetFullScreenState(this, nsnull, false);

    // Next walk the document tree of still visible documents, and reset
    // their full-screen state. We then move the top-level window out
    // of full-screen mode.
    nsCOMPtr<nsIDocument> fullScreenRoot(do_QueryReferent(sFullScreenRootDoc));
    if (fullScreenRoot) {
      fullScreenRoot->EnumerateSubDocuments(ResetFullScreen, nsnull);
      SetWindowFullScreen(fullScreenRoot, false);
      sFullScreenRootDoc = nsnull;
    }
  }
}

void
nsDocument::WillDispatchMutationEvent(nsINode* aTarget)
{
  NS_ASSERTION(mSubtreeModifiedDepth != 0 ||
               mSubtreeModifiedTargets.Count() == 0,
               "mSubtreeModifiedTargets not cleared after dispatching?");
  ++mSubtreeModifiedDepth;
  if (aTarget) {
    // MayDispatchMutationEvent is often called just before this method,
    // so it has already appended the node to mSubtreeModifiedTargets.
    PRInt32 count = mSubtreeModifiedTargets.Count();
    if (!count || mSubtreeModifiedTargets[count - 1] != aTarget) {
      mSubtreeModifiedTargets.AppendObject(aTarget);
    }
  }
}

void
nsDocument::MutationEventDispatched(nsINode* aTarget)
{
  --mSubtreeModifiedDepth;
  if (mSubtreeModifiedDepth == 0) {
    PRInt32 count = mSubtreeModifiedTargets.Count();
    if (!count) {
      return;
    }

    nsCOMPtr<nsPIDOMWindow> window;
    window = do_QueryInterface(GetScriptGlobalObject());
    if (window &&
        !window->HasMutationListeners(NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED)) {
      mSubtreeModifiedTargets.Clear();
      return;
    }

    nsCOMArray<nsINode> realTargets;
    for (PRInt32 i = 0; i < count; ++i) {
      nsINode* possibleTarget = mSubtreeModifiedTargets[i];
      nsCOMPtr<nsIContent> content = do_QueryInterface(possibleTarget);
      if (content && content->IsInNativeAnonymousSubtree()) {
        continue;
      }

      nsINode* commonAncestor = nsnull;
      PRInt32 realTargetCount = realTargets.Count();
      for (PRInt32 j = 0; j < realTargetCount; ++j) {
        commonAncestor =
          nsContentUtils::GetCommonAncestor(possibleTarget, realTargets[j]);
        if (commonAncestor) {
          realTargets.ReplaceObjectAt(commonAncestor, j);
          break;
        }
      }
      if (!commonAncestor) {
        realTargets.AppendObject(possibleTarget);
      }
    }

    mSubtreeModifiedTargets.Clear();

    PRInt32 realTargetCount = realTargets.Count();
    for (PRInt32 k = 0; k < realTargetCount; ++k) {
      nsMutationEvent mutation(true, NS_MUTATION_SUBTREEMODIFIED);
      (new nsPLDOMEvent(realTargets[k], mutation))->RunDOMEventWhenSafe();
    }
  }
}

void
nsDocument::AddStyleRelevantLink(Link* aLink)
{
  NS_ASSERTION(aLink, "Passing in a null link.  Expect crashes RSN!");
#ifdef DEBUG
  nsPtrHashKey<Link>* entry = mStyledLinks.GetEntry(aLink);
  NS_ASSERTION(!entry, "Document already knows about this Link!");
  mStyledLinksCleared = false;
#endif
  (void)mStyledLinks.PutEntry(aLink);
}

void
nsDocument::ForgetLink(Link* aLink)
{
  NS_ASSERTION(aLink, "Passing in a null link.  Expect crashes RSN!");
#ifdef DEBUG
  nsPtrHashKey<Link>* entry = mStyledLinks.GetEntry(aLink);
  NS_ASSERTION(entry || mStyledLinksCleared,
               "Document knows nothing about this Link!");
#endif
  (void)mStyledLinks.RemoveEntry(aLink);
}

void
nsDocument::DestroyElementMaps()
{
#ifdef DEBUG
  mStyledLinksCleared = true;
#endif
  mStyledLinks.Clear();
  mIdentifierMap.Clear();
}

static
PLDHashOperator
EnumerateStyledLinks(nsPtrHashKey<Link>* aEntry, void* aArray)
{
  LinkArray* array = static_cast<LinkArray*>(aArray);
  (void)array->AppendElement(aEntry->GetKey());
  return PL_DHASH_NEXT;
}

void
nsDocument::RefreshLinkHrefs()
{
  // Get a list of all links we know about.  We will reset them, which will
  // remove them from the document, so we need a copy of what is in the
  // hashtable.
  LinkArray linksToNotify(mStyledLinks.Count());
  (void)mStyledLinks.EnumerateEntries(EnumerateStyledLinks, &linksToNotify);

  // Reset all of our styled links.
  nsAutoScriptBlocker scriptBlocker;
  for (LinkArray::size_type i = 0; i < linksToNotify.Length(); i++) {
    linksToNotify[i]->ResetLinkState(true);
  }
}

nsresult
nsDocument::CloneDocHelper(nsDocument* clone) const
{
  clone->mIsStaticDocument = mCreatingStaticClone;

  // Init document
  nsresult rv = clone->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  // Set URI/principal
  clone->nsDocument::SetDocumentURI(nsIDocument::GetDocumentURI());
  // Must set the principal first, since SetBaseURI checks it.
  clone->SetPrincipal(NodePrincipal());
  clone->mDocumentBaseURI = mDocumentBaseURI;

  if (mCreatingStaticClone) {
    nsCOMPtr<nsIChannel> channel = GetChannel();
    nsCOMPtr<nsILoadGroup> loadGroup = GetDocumentLoadGroup();
    if (channel && loadGroup) {
      clone->Reset(channel, loadGroup);
    } else {
      nsIURI* uri = static_cast<const nsIDocument*>(this)->GetDocumentURI();
      if (uri) {
        clone->ResetToURI(uri, loadGroup, NodePrincipal());
      }
    }
    nsCOMPtr<nsISupports> container = GetContainer();
    clone->SetContainer(container);
  }

  // Set scripting object
  bool hasHadScriptObject = true;
  nsIScriptGlobalObject* scriptObject =
    GetScriptHandlingObject(hasHadScriptObject);
  NS_ENSURE_STATE(scriptObject || !hasHadScriptObject);
  clone->SetScriptHandlingObject(scriptObject);

  // Make the clone a data document
  clone->SetLoadedAsData(true);

  // Misc state

  // State from nsIDocument
  clone->mCharacterSet = mCharacterSet;
  clone->mCharacterSetSource = mCharacterSetSource;
  clone->mCompatMode = mCompatMode;
  clone->mBidiOptions = mBidiOptions;
  clone->mContentLanguage = mContentLanguage;
  clone->SetContentTypeInternal(GetContentTypeInternal());
  clone->mSecurityInfo = mSecurityInfo;

  // State from nsDocument
  clone->mIsRegularHTML = mIsRegularHTML;
  clone->mXMLDeclarationBits = mXMLDeclarationBits;
  clone->mBaseTarget = mBaseTarget;
  return NS_OK;
}

void
nsDocument::SetReadyStateInternal(ReadyState rs)
{
  mReadyState = rs;
  if (mTiming) {
    switch (rs) {
      case READYSTATE_LOADING:
        mTiming->NotifyDOMLoading(nsIDocument::GetDocumentURI());
        break;
      case READYSTATE_INTERACTIVE:
        mTiming->NotifyDOMInteractive(nsIDocument::GetDocumentURI());
        break;
      case READYSTATE_COMPLETE:
        mTiming->NotifyDOMComplete(nsIDocument::GetDocumentURI());
        break;
      default:
        NS_WARNING("Unexpected ReadyState value");
        break;
    }
  }
  // At the time of loading start, we don't have timing object, record time.
  if (READYSTATE_LOADING == rs) {
    mLoadingTimeStamp = mozilla::TimeStamp::Now();
  }

  nsRefPtr<nsPLDOMEvent> plevent =
    new nsPLDOMEvent(this, NS_LITERAL_STRING("readystatechange"), false, false); 
  if (plevent) {
    plevent->RunDOMEventWhenSafe();
  }
}

nsIDocument::ReadyState
nsDocument::GetReadyStateEnum()
{
  return mReadyState;
}

NS_IMETHODIMP
nsDocument::GetReadyState(nsAString& aReadyState)
{
  switch(mReadyState) {
  case READYSTATE_LOADING :
    aReadyState.Assign(NS_LITERAL_STRING("loading"));
    break;
  case READYSTATE_INTERACTIVE :
    aReadyState.Assign(NS_LITERAL_STRING("interactive"));
    break;
  case READYSTATE_COMPLETE :
    aReadyState.Assign(NS_LITERAL_STRING("complete"));
    break;  
  default:
    aReadyState.Assign(NS_LITERAL_STRING("uninitialized"));
  }
  return NS_OK;
}

static bool
SuppressEventHandlingInDocument(nsIDocument* aDocument, void* aData)
{
  aDocument->SuppressEventHandling(*static_cast<PRUint32*>(aData));
  return true;
}

void
nsDocument::SuppressEventHandling(PRUint32 aIncrease)
{
  if (mEventsSuppressed == 0 && aIncrease != 0 && mPresShell &&
      mScriptGlobalObject) {
    RevokeAnimationFrameNotifications();
  }
  mEventsSuppressed += aIncrease;
  EnumerateSubDocuments(SuppressEventHandlingInDocument, &aIncrease);
}

static void
FireOrClearDelayedEvents(nsTArray<nsCOMPtr<nsIDocument> >& aDocuments,
                         bool aFireEvents)
{
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm)
    return;

  for (PRUint32 i = 0; i < aDocuments.Length(); ++i) {
    if (!aDocuments[i]->EventHandlingSuppressed()) {
      fm->FireDelayedEvents(aDocuments[i]);
      nsCOMPtr<nsIPresShell> shell = aDocuments[i]->GetShell();
      if (shell) {
        shell->FireOrClearDelayedEvents(aFireEvents);
      }
    }
  }
}

void
nsDocument::MaybePreLoadImage(nsIURI* uri, const nsAString &aCrossOriginAttr)
{
  // Early exit if the img is already present in the img-cache
  // which indicates that the "real" load has already started and
  // that we shouldn't preload it.
  PRInt16 blockingStatus;
  if (nsContentUtils::IsImageInCache(uri) ||
      !nsContentUtils::CanLoadImage(uri, static_cast<nsIDocument *>(this),
                                    this, NodePrincipal(), &blockingStatus)) {
    return;
  }

  nsLoadFlags loadFlags = nsIRequest::LOAD_NORMAL;
  if (aCrossOriginAttr.LowerCaseEqualsLiteral("anonymous")) {
    loadFlags |= imgILoader::LOAD_CORS_ANONYMOUS;
  } else if (aCrossOriginAttr.LowerCaseEqualsLiteral("use-credentials")) {
    loadFlags |= imgILoader::LOAD_CORS_USE_CREDENTIALS;
  }
  // else should we err on the side of not doing the preload if
  // aCrossOriginAttr is nonempty?  Let's err on the side of doing the
  // preload as CORS_NONE.

  // Image not in cache - trigger preload
  nsCOMPtr<imgIRequest> request;
  nsresult rv =
    nsContentUtils::LoadImage(uri,
                              this,
                              NodePrincipal(),
                              mDocumentURI, // uri of document used as referrer
                              nsnull,       // no observer
                              loadFlags,
                              getter_AddRefs(request));

  // Pin image-reference to avoid evicting it from the img-cache before
  // the "real" load occurs. Unpinned in DispatchContentLoadedEvents and
  // unlink
  if (NS_SUCCEEDED(rv)) {
    mPreloadingImages.AppendObject(request);
  }
}

nsEventStates
nsDocument::GetDocumentState()
{
  if (!mGotDocumentState.HasState(NS_DOCUMENT_STATE_RTL_LOCALE)) {
    if (IsDocumentRightToLeft()) {
      mDocumentState |= NS_DOCUMENT_STATE_RTL_LOCALE;
    }
    mGotDocumentState |= NS_DOCUMENT_STATE_RTL_LOCALE;
  }
  if (!mGotDocumentState.HasState(NS_DOCUMENT_STATE_WINDOW_INACTIVE)) {
    nsIPresShell* shell = GetShell();
    if (shell && shell->GetPresContext() &&
        shell->GetPresContext()->IsTopLevelWindowInactive()) {
      mDocumentState |= NS_DOCUMENT_STATE_WINDOW_INACTIVE;
    }
    mGotDocumentState |= NS_DOCUMENT_STATE_WINDOW_INACTIVE;
  }
  return mDocumentState;
}

namespace {

/**
 * Stub for LoadSheet(), since all we want is to get the sheet into
 * the CSSLoader's style cache
 */
class StubCSSLoaderObserver : public nsICSSLoaderObserver {
public:
  NS_IMETHOD
  StyleSheetLoaded(nsCSSStyleSheet*, bool, nsresult)
  {
    return NS_OK;
  }
  NS_DECL_ISUPPORTS
};
NS_IMPL_ISUPPORTS1(StubCSSLoaderObserver, nsICSSLoaderObserver)

}

void
nsDocument::PreloadStyle(nsIURI* uri, const nsAString& charset)
{
  // The CSSLoader will retain this object after we return.
  nsCOMPtr<nsICSSLoaderObserver> obs = new StubCSSLoaderObserver();

  // Charset names are always ASCII.
  CSSLoader()->LoadSheet(uri, NodePrincipal(),
                         NS_LossyConvertUTF16toASCII(charset),
                         obs);
}

nsresult
nsDocument::LoadChromeSheetSync(nsIURI* uri, bool isAgentSheet,
                                nsCSSStyleSheet** sheet)
{
  return CSSLoader()->LoadSheetSync(uri, isAgentSheet, isAgentSheet, sheet);
}

class nsDelayedEventDispatcher : public nsRunnable
{
public:
  nsDelayedEventDispatcher(nsTArray<nsCOMPtr<nsIDocument> >& aDocuments)
  {
    mDocuments.SwapElements(aDocuments);
  }
  virtual ~nsDelayedEventDispatcher() {}

  NS_IMETHOD Run()
  {
    FireOrClearDelayedEvents(mDocuments, true);
    return NS_OK;
  }

private:
  nsTArray<nsCOMPtr<nsIDocument> > mDocuments;
};

static bool
GetAndUnsuppressSubDocuments(nsIDocument* aDocument, void* aData)
{
  PRUint32 suppression = aDocument->EventHandlingSuppressed();
  if (suppression > 0) {
    static_cast<nsDocument*>(aDocument)->DecreaseEventSuppression();
  }
  nsTArray<nsCOMPtr<nsIDocument> >* docs =
    static_cast<nsTArray<nsCOMPtr<nsIDocument> >* >(aData);
  docs->AppendElement(aDocument);
  aDocument->EnumerateSubDocuments(GetAndUnsuppressSubDocuments, docs);
  return true;
}

void
nsDocument::UnsuppressEventHandlingAndFireEvents(bool aFireEvents)
{
  nsTArray<nsCOMPtr<nsIDocument> > documents;
  GetAndUnsuppressSubDocuments(this, &documents);

  if (aFireEvents) {
    NS_DispatchToCurrentThread(new nsDelayedEventDispatcher(documents));
  } else {
    FireOrClearDelayedEvents(documents, false);
  }
}

nsISupports*
nsDocument::GetCurrentContentSink()
{
  return mParser ? mParser->GetContentSink() : nsnull;
}

void
nsDocument::RegisterFileDataUri(const nsACString& aUri)
{
  mFileDataUris.AppendElement(aUri);
}

void
nsDocument::UnregisterFileDataUri(const nsACString& aUri)
{
  mFileDataUris.RemoveElement(aUri);
}

void
nsDocument::SetScrollToRef(nsIURI *aDocumentURI)
{
  if (!aDocumentURI) {
    return;
  }

  nsCAutoString ref;

  // Since all URI's that pass through here aren't URL's we can't
  // rely on the nsIURI implementation for providing a way for
  // finding the 'ref' part of the URI, we'll haveto revert to
  // string routines for finding the data past '#'

  aDocumentURI->GetSpec(ref);

  nsReadingIterator<char> start, end;

  ref.BeginReading(start);
  ref.EndReading(end);

  if (FindCharInReadable('#', start, end)) {
    ++start; // Skip over the '#'

    mScrollToRef = Substring(start, end);
  }
}

void
nsDocument::ScrollToRef()
{
  if (mScrolledToRefAlready) {
    return;
  }

  if (mScrollToRef.IsEmpty()) {
    return;
  }

  char* tmpstr = ToNewCString(mScrollToRef);
  if (!tmpstr) {
    return;
  }

  nsUnescape(tmpstr);
  nsCAutoString unescapedRef;
  unescapedRef.Assign(tmpstr);
  nsMemory::Free(tmpstr);

  nsresult rv = NS_ERROR_FAILURE;
  // We assume that the bytes are in UTF-8, as it says in the spec:
  // http://www.w3.org/TR/html4/appendix/notes.html#h-B.2.1
  NS_ConvertUTF8toUTF16 ref(unescapedRef);

  nsCOMPtr<nsIPresShell> shell = GetShell();
  if (shell) {
    // Check an empty string which might be caused by the UTF-8 conversion
    if (!ref.IsEmpty()) {
      // Note that GoToAnchor will handle flushing layout as needed.
      rv = shell->GoToAnchor(ref, mChangeScrollPosWhenScrollingToRef);
    } else {
      rv = NS_ERROR_FAILURE;
    }

    // If UTF-8 URI failed then try to assume the string as a
    // document's charset.

    if (NS_FAILED(rv)) {
      const nsACString &docCharset = GetDocumentCharacterSet();

      rv = nsContentUtils::ConvertStringFromCharset(docCharset, unescapedRef, ref);

      if (NS_SUCCEEDED(rv) && !ref.IsEmpty()) {
        rv = shell->GoToAnchor(ref, mChangeScrollPosWhenScrollingToRef);
      }
    }
    if (NS_SUCCEEDED(rv)) {
      mScrolledToRefAlready = true;
    }
  }
}

void
nsDocument::ResetScrolledToRefAlready()
{
  mScrolledToRefAlready = false;
}

void
nsDocument::SetChangeScrollPosWhenScrollingToRef(bool aValue)
{
  mChangeScrollPosWhenScrollingToRef = aValue;
}

void
nsIDocument::RegisterFreezableElement(nsIContent* aContent)
{
  if (!mFreezableElements) {
    mFreezableElements = new nsTHashtable<nsPtrHashKey<nsIContent> >();
    if (!mFreezableElements)
      return;
    mFreezableElements->Init();
  }
  mFreezableElements->PutEntry(aContent);
}

bool
nsIDocument::UnregisterFreezableElement(nsIContent* aContent)
{
  if (!mFreezableElements)
    return false;
  if (!mFreezableElements->GetEntry(aContent))
    return false;
  mFreezableElements->RemoveEntry(aContent);
  return true;
}

struct EnumerateFreezablesData {
  nsIDocument::FreezableElementEnumerator mEnumerator;
  void* mData;
};

static PLDHashOperator
EnumerateFreezables(nsPtrHashKey<nsIContent>* aEntry, void* aData)
{
  EnumerateFreezablesData* data = static_cast<EnumerateFreezablesData*>(aData);
  data->mEnumerator(aEntry->GetKey(), data->mData);
  return PL_DHASH_NEXT;
}

void
nsIDocument::EnumerateFreezableElements(FreezableElementEnumerator aEnumerator,
                                        void* aData)
{
  if (!mFreezableElements)
    return;
  EnumerateFreezablesData data = { aEnumerator, aData };
  mFreezableElements->EnumerateEntries(EnumerateFreezables, &data);
}

void
nsIDocument::RegisterPendingLinkUpdate(Link* aLink)
{
  mLinksToUpdate.PutEntry(aLink);
  mHasLinksToUpdate = true;
}

void
nsIDocument::UnregisterPendingLinkUpdate(Link* aLink)
{
  if (!mHasLinksToUpdate)
    return;
    
  mLinksToUpdate.RemoveEntry(aLink);
}
  
static PLDHashOperator
EnumeratePendingLinkUpdates(nsPtrHashKey<Link>* aEntry, void* aData)
{
  aEntry->GetKey()->GetElement()->UpdateLinkState(aEntry->GetKey()->LinkState());
  return PL_DHASH_NEXT;
}

void
nsIDocument::FlushPendingLinkUpdates() 
{
  if (!mHasLinksToUpdate)
    return;
    
  nsAutoScriptBlocker scriptBlocker;
  mLinksToUpdate.EnumerateEntries(EnumeratePendingLinkUpdates, nsnull);
  mLinksToUpdate.Clear();
  mHasLinksToUpdate = false;
}

already_AddRefed<nsIDocument>
nsIDocument::CreateStaticClone(nsISupports* aCloneContainer)
{
  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(this);
  NS_ENSURE_TRUE(domDoc, nsnull);
  mCreatingStaticClone = true;

  // Make document use different container during cloning.
  nsCOMPtr<nsISupports> originalContainer = GetContainer();
  SetContainer(aCloneContainer);
  nsCOMPtr<nsIDOMNode> clonedNode;
  nsresult rv = domDoc->CloneNode(true, getter_AddRefs(clonedNode));
  SetContainer(originalContainer);

  nsCOMPtr<nsIDocument> clonedDoc;
  if (NS_SUCCEEDED(rv)) {
    clonedDoc = do_QueryInterface(clonedNode);
    nsCOMPtr<nsIDOMDocument> clonedDOMDoc = do_QueryInterface(clonedDoc);
    if (clonedDOMDoc) {
      clonedDoc->mOriginalDocument = this;
      PRInt32 sheetsCount = GetNumberOfStyleSheets();
      for (PRInt32 i = 0; i < sheetsCount; ++i) {
        nsRefPtr<nsCSSStyleSheet> sheet = do_QueryObject(GetStyleSheetAt(i));
        if (sheet) {
          if (sheet->IsApplicable()) {
            nsRefPtr<nsCSSStyleSheet> clonedSheet =
              sheet->Clone(nsnull, nsnull, clonedDoc, nsnull);
            NS_WARN_IF_FALSE(clonedSheet, "Cloning a stylesheet didn't work!");
            if (clonedSheet) {
              clonedDoc->AddStyleSheet(clonedSheet);
            }
          }
        }
      }

      sheetsCount = GetNumberOfCatalogStyleSheets();
      for (PRInt32 i = 0; i < sheetsCount; ++i) {
        nsRefPtr<nsCSSStyleSheet> sheet =
          do_QueryObject(GetCatalogStyleSheetAt(i));
        if (sheet) {
          if (sheet->IsApplicable()) {
            nsRefPtr<nsCSSStyleSheet> clonedSheet =
              sheet->Clone(nsnull, nsnull, clonedDoc, nsnull);
            NS_WARN_IF_FALSE(clonedSheet, "Cloning a stylesheet didn't work!");
            if (clonedSheet) {
              clonedDoc->AddCatalogStyleSheet(clonedSheet);
            }
          }
        }
      }
    }
  }
  mCreatingStaticClone = false;
  return clonedDoc.forget();
}

void
nsIDocument::ScheduleBeforePaintEvent(nsIAnimationFrameListener* aListener)
{
  if (aListener) {
    bool alreadyRegistered = !mAnimationFrameListeners.IsEmpty();
    if (mAnimationFrameListeners.AppendElement(aListener) &&
        !alreadyRegistered && mPresShell && IsEventHandlingEnabled()) {
      mPresShell->GetPresContext()->RefreshDriver()->
        ScheduleAnimationFrameListeners(this);
    }

    return;
  }

  if (!mHavePendingPaint) {
    // We don't want to use GetShell() here, because we want to schedule the
    // paint even if we're frozen.  Either we'll get unfrozen and then the
    // event will fire, or we'll quietly go away at some point.
    mHavePendingPaint =
      !mPresShell ||
      !IsEventHandlingEnabled() ||
      mPresShell->GetPresContext()->RefreshDriver()->
        ScheduleBeforePaintEvent(this);
  }

}

nsresult
nsDocument::GetStateObject(nsIVariant** aState)
{
  // Get the document's current state object. This is the object backing both
  // history.state and popStateEvent.state.
  //
  // mStateObjectContainer may be null; this just means that there's no
  // current state object.

  nsCOMPtr<nsIVariant> stateObj;
  if (!mStateObjectCached && mStateObjectContainer) {
    JSContext *cx = nsContentUtils::GetContextFromDocument(this);
    mStateObjectContainer->
      DeserializeToVariant(cx, getter_AddRefs(mStateObjectCached));
  }

  NS_IF_ADDREF(*aState = mStateObjectCached);
  
  return NS_OK;
}

nsDOMNavigationTiming*
nsDocument::GetNavigationTiming() const
{
  return mTiming;
}

nsresult
nsDocument::SetNavigationTiming(nsDOMNavigationTiming* aTiming)
{
  mTiming = aTiming;
  if (!mLoadingTimeStamp.IsNull() && mTiming) {
    mTiming->SetDOMLoadingTimeStamp(nsIDocument::GetDocumentURI(), mLoadingTimeStamp);
  }
  return NS_OK;
}

Element*
nsDocument::FindImageMap(const nsAString& aUseMapValue)
{
  if (aUseMapValue.IsEmpty()) {
    return nsnull;
  }

  nsAString::const_iterator start, end;
  aUseMapValue.BeginReading(start);
  aUseMapValue.EndReading(end);

  PRInt32 hash = aUseMapValue.FindChar('#');
  if (hash < 0) {
    return nsnull;
  }
  // aUsemap contains a '#', set start to point right after the '#'
  start.advance(hash + 1);

  if (start == end) {
    return nsnull; // aUsemap == "#"
  }

  const nsAString& mapName = Substring(start, end);

  if (!mImageMaps) {
    mImageMaps = new nsContentList(this, kNameSpaceID_XHTML, nsGkAtoms::map, nsGkAtoms::map);
  }

  PRUint32 i, n = mImageMaps->Length(true);
  for (i = 0; i < n; ++i) {
    nsIContent* map = mImageMaps->GetNodeAt(i);
    if (map->AttrValueIs(kNameSpaceID_None, nsGkAtoms::id, mapName,
                         eCaseMatters) ||
        map->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name, mapName,
                         eIgnoreCase)) {
      return map->AsElement();
    }
  }

  return nsnull;
}

#define DEPRECATED_OPERATION(_op) #_op "Warning",
static const char* kWarnings[] = {
#include "nsDeprecatedOperationList.h"
  nsnull
};
#undef DEPRECATED_OPERATION

void
nsIDocument::WarnOnceAbout(DeprecatedOperations aOperation)
{
  PR_STATIC_ASSERT(eDeprecatedOperationCount <= 64);
  if (mWarnedAbout & (1 << aOperation)) {
    return;
  }
  mWarnedAbout |= (1 << aOperation);
  nsContentUtils::ReportToConsole(nsContentUtils::eDOM_PROPERTIES,
                                  kWarnings[aOperation],
                                  nsnull, 0,
                                  nsnull,
                                  EmptyString(), 0, 0,
                                  nsIScriptError::warningFlag,
                                  "DOM Core", this);
}

nsresult
nsDocument::AddImage(imgIRequest* aImage)
{
  NS_ENSURE_ARG_POINTER(aImage);

  // See if the image is already in the hashtable. If it is, get the old count.
  PRUint32 oldCount = 0;
  mImageTracker.Get(aImage, &oldCount);

  // Put the image in the hashtable, with the proper count.
  bool success = mImageTracker.Put(aImage, oldCount + 1);
  if (!success)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = NS_OK;

  // If this is the first insertion and we're locking images, lock this image
  // too.
  if (oldCount == 0 && mLockingImages) {
    rv = aImage->LockImage();
    if (NS_SUCCEEDED(rv))
      rv = aImage->RequestDecode();
  }

  // If this is the first insertion and we're animating images, request
  // that this image be animated too.
  if (oldCount == 0 && mAnimatingImages) {
    nsresult rv2 = aImage->IncrementAnimationConsumers();
    rv = NS_SUCCEEDED(rv) ? rv2 : rv;
  }

  return rv;
}

nsresult
nsDocument::RemoveImage(imgIRequest* aImage)
{
  NS_ENSURE_ARG_POINTER(aImage);

  // Get the old count. It should exist and be > 0.
  PRUint32 count;
#ifdef DEBUG
  bool found =
#endif
  mImageTracker.Get(aImage, &count);
  NS_ABORT_IF_FALSE(found, "Removing image that wasn't in the tracker!");
  NS_ABORT_IF_FALSE(count > 0, "Entry in the cache tracker with count 0!");

  // We're removing, so decrement the count.
  count--;

  // If the count is now zero, remove from the tracker.
  // Otherwise, set the new value.
  if (count == 0) {
    mImageTracker.Remove(aImage);
  } else {
    mImageTracker.Put(aImage, count);
  }

  nsresult rv = NS_OK;

  // If we removed the image from the tracker and we're locking images, unlock
  // this image.
  if (count == 0 && mLockingImages)
    rv = aImage->UnlockImage();

  // If we removed the image from the tracker and we're animating images,
  // remove our request to animate this image.
  if (count == 0 && mAnimatingImages) {
    nsresult rv2 = aImage->DecrementAnimationConsumers();
    rv = NS_SUCCEEDED(rv) ? rv2 : rv;
  }

  return rv;
}

PLDHashOperator LockEnumerator(imgIRequest* aKey,
                               PRUint32 aData,
                               void*    userArg)
{
  aKey->LockImage();
  aKey->RequestDecode();
  return PL_DHASH_NEXT;
}

PLDHashOperator UnlockEnumerator(imgIRequest* aKey,
                                 PRUint32 aData,
                                 void*    userArg)
{
  aKey->UnlockImage();
  return PL_DHASH_NEXT;
}


nsresult
nsDocument::SetImageLockingState(bool aLocked)
{
  if (XRE_GetProcessType() == GeckoProcessType_Content &&
      !Preferences::GetBool("content.image.allow_locking", true)) {
    return NS_OK;
  }

  // If there's no change, there's nothing to do.
  if (mLockingImages == aLocked)
    return NS_OK;

  // Otherwise, iterate over our images and perform the appropriate action.
  mImageTracker.EnumerateRead(aLocked ? LockEnumerator
                                      : UnlockEnumerator,
                              nsnull);

  // Update state.
  mLockingImages = aLocked;

  return NS_OK;
}

PLDHashOperator IncrementAnimationEnumerator(imgIRequest* aKey,
                                             PRUint32 aData,
                                             void*    userArg)
{
  aKey->IncrementAnimationConsumers();
  return PL_DHASH_NEXT;
}

PLDHashOperator DecrementAnimationEnumerator(imgIRequest* aKey,
                                             PRUint32 aData,
                                             void*    userArg)
{
  aKey->DecrementAnimationConsumers();
  return PL_DHASH_NEXT;
}

void
nsDocument::SetImagesNeedAnimating(bool aAnimating)
{
  // If there's no change, there's nothing to do.
  if (mAnimatingImages == aAnimating)
    return;

  // Otherwise, iterate over our images and perform the appropriate action.
  mImageTracker.EnumerateRead(aAnimating ? IncrementAnimationEnumerator
                                         : DecrementAnimationEnumerator,
                              nsnull);

  // Update state.
  mAnimatingImages = aAnimating;
}

NS_IMETHODIMP
nsDocument::CreateTouch(nsIDOMWindow* aView,
                        nsIDOMEventTarget* aTarget,
                        PRInt32 aIdentifier,
                        PRInt32 aPageX,
                        PRInt32 aPageY,
                        PRInt32 aScreenX,
                        PRInt32 aScreenY,
                        PRInt32 aClientX,
                        PRInt32 aClientY,
                        PRInt32 aRadiusX,
                        PRInt32 aRadiusY,
                        float aRotationAngle,
                        float aForce,
                        nsIDOMTouch** aRetVal)
{
  NS_ADDREF(*aRetVal = new nsDOMTouch(aTarget,
                                      aIdentifier,
                                      aPageX,
                                      aPageY,
                                      aScreenX,
                                      aScreenY,
                                      aClientX,
                                      aClientY,
                                      aRadiusX,
                                      aRadiusY,
                                      aRotationAngle,
                                      aForce));
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::CreateTouchList(nsIVariant* aPoints,
                            nsIDOMTouchList** aRetVal)
{
  nsRefPtr<nsDOMTouchList> retval = new nsDOMTouchList();
  if (aPoints) {
    PRUint16 type;
    aPoints->GetDataType(&type);
    if (type == nsIDataType::VTYPE_INTERFACE ||
        type == nsIDataType::VTYPE_INTERFACE_IS) {
      nsCOMPtr<nsISupports> data;
      aPoints->GetAsISupports(getter_AddRefs(data));
      nsCOMPtr<nsIDOMTouch> point = do_QueryInterface(data);
      if (point) {
        retval->Append(point);
      }
    } else if (type == nsIDataType::VTYPE_ARRAY) {
      PRUint16 valueType;
      nsIID iid;
      PRUint32 valueCount;
      void* rawArray;
      aPoints->GetAsArray(&valueType, &iid, &valueCount, &rawArray);
      if (valueType == nsIDataType::VTYPE_INTERFACE ||
          valueType == nsIDataType::VTYPE_INTERFACE_IS) {
        nsISupports** values = static_cast<nsISupports**>(rawArray);
        for (PRUint32 i = 0; i < valueCount; ++i) {
          nsCOMPtr<nsISupports> supports = dont_AddRef(values[i]);
          nsCOMPtr<nsIDOMTouch> point = do_QueryInterface(supports);
          if (point) {
            retval->Append(point);
          }
        }
      }
      nsMemory::Free(rawArray);
    }
  }

  *aRetVal = retval.forget().get();
  return NS_OK;
}

PRInt64
nsIDocument::SizeOf() const
{
  PRInt64 size = MemoryReporter::GetBasicSize<nsIDocument, nsINode>(this);

  for (nsIContent* node = GetFirstChild(); node;
       node = node->GetNextNode(this)) {
    size += node->SizeOf();
  }

  return size;
}

static void
DispatchFullScreenChange(nsINode* aTarget)
{
  nsRefPtr<nsPLDOMEvent> e =
    new nsPLDOMEvent(aTarget,
                     NS_LITERAL_STRING("mozfullscreenchange"),
                     true,
                     false);
  e->PostDOMEvent();
}

bool
nsDocument::SetFullScreenState(Element* aElement, bool aIsFullScreen)
{
  if (mFullScreenElement) {
    // Reset the ancestor and full-screen styles on the outgoing full-screen
    // element in the current document.
    nsEventStateManager::SetFullScreenState(mFullScreenElement, false);
    mFullScreenElement = nsnull;
  }
  if (aElement) {
    nsEventStateManager::SetFullScreenState(aElement, aIsFullScreen);
  }
  mFullScreenElement = aElement;

  if (mIsFullScreen == aIsFullScreen) {
    return false;
  }
  mIsFullScreen = aIsFullScreen;
  return true;
}

// Wrapper for the nsIDocument -> nsDocument cast required to call
// nsDocument::SetFullScreenState().
static bool
SetFullScreenState(nsIDocument* aDoc, Element* aElement, bool aIsFullScreen)
{
  return static_cast<nsDocument*>(aDoc)->
    SetFullScreenState(aElement, aIsFullScreen);
}

NS_IMETHODIMP
nsDocument::MozCancelFullScreen()
{
  if (!nsContentUtils::IsRequestFullScreenAllowed()) {
    return NS_OK;
  }
  CancelFullScreen();
  return NS_OK;
}

// Runnable to set window full-screen mode. Used as a script runner
// to ensure we only call nsGlobalWindow::SetFullScreen() when it's safe to 
// run script. nsGlobalWindow::SetFullScreen() dispatches a synchronous event
// (handled in chome code) which is unsafe to run if this is called in
// nsGenericElement::UnbindFromTree().
class nsSetWindowFullScreen : public nsRunnable {
public:
  nsSetWindowFullScreen(nsIDocument* aDoc, bool aValue)
    : mDoc(aDoc), mValue(aValue) {}

  NS_IMETHOD Run()
  {
    if (mDoc->GetWindow()) {
      mDoc->GetWindow()->SetFullScreen(mValue);
    }
    return NS_OK;
  }

private:
  nsCOMPtr<nsIDocument> mDoc;
  bool mValue;
};

static void
SetWindowFullScreen(nsIDocument* aDoc, bool aValue)
{
  nsContentUtils::AddScriptRunner(new nsSetWindowFullScreen(aDoc, aValue));
}

void
nsDocument::CancelFullScreen()
{
  NS_ASSERTION(!IsFullScreenDoc() || sFullScreenDoc != nsnull,
               "Should have a full-screen doc when full-screen!");

  if (!IsFullScreenDoc() || !GetWindow() || !sFullScreenDoc) {
    return;
  }

  // Reset full-screen state in all full-screen documents.
  nsCOMPtr<nsIDocument> doc(do_QueryReferent(sFullScreenDoc));
  while (doc != nsnull) {
    if (::SetFullScreenState(doc, nsnull, false)) {
      DispatchFullScreenChange(doc);
    }
    doc = doc->GetParentDocument();
  }
  sFullScreenDoc = nsnull;
  sFullScreenRootDoc = nsnull;

  // Move the window out of full-screen mode.
  SetWindowFullScreen(this, false);

  return;
}

bool
nsDocument::IsFullScreenDoc()
{
  return mIsFullScreen;
}

static nsIDocument*
GetCommonAncestor(nsIDocument* aDoc1, nsIDocument* aDoc2)
{
  nsIDocument* doc1 = aDoc1;
  nsIDocument* doc2 = aDoc2;

  nsAutoTArray<nsIDocument*, 30> parents1, parents2;
  do {
    parents1.AppendElement(doc1);
    doc1 = doc1->GetParentDocument();
  } while (doc1);
  do {
    parents2.AppendElement(doc2);
    doc2 = doc2->GetParentDocument();
  } while (doc2);

  PRUint32 pos1 = parents1.Length();
  PRUint32 pos2 = parents2.Length();
  nsIDocument* parent = nsnull;
  PRUint32 len;
  for (len = NS_MIN(pos1, pos2); len > 0; --len) {
    nsIDocument* child1 = parents1.ElementAt(--pos1);
    nsIDocument* child2 = parents2.ElementAt(--pos2);
    if (child1 != child2) {
      break;
    }
    parent = child1;
  }
  return parent;
}

class nsCallRequestFullScreen : public nsRunnable
{
public:
  nsCallRequestFullScreen(Element* aElement)
    : mElement(aElement),
      mDoc(aElement->OwnerDoc()),
      mWasCallerChrome(nsContentUtils::IsCallerChrome())
  {
  }

  NS_IMETHOD Run()
  {
    nsDocument* doc = static_cast<nsDocument*>(mDoc.get());
    doc->RequestFullScreen(mElement, mWasCallerChrome);
    return NS_OK;
  }

  nsRefPtr<Element> mElement;
  nsCOMPtr<nsIDocument> mDoc;
  bool mWasCallerChrome;
};

void
nsDocument::AsyncRequestFullScreen(Element* aElement)
{
  if (!aElement) {
    return;
  }
  // Request full-screen asynchronously.
  nsCOMPtr<nsIRunnable> event(new nsCallRequestFullScreen(aElement));
  NS_DispatchToCurrentThread(event);
}

void
nsDocument::RequestFullScreen(Element* aElement, bool aWasCallerChrome)
{
  if (!aElement ||
      !aElement->IsInDoc() ||
      aElement->OwnerDoc() != this ||
      !IsFullScreenEnabled(aWasCallerChrome) ||
      !GetWindow()) {
    nsRefPtr<nsPLDOMEvent> e =
      new nsPLDOMEvent(this,
                       NS_LITERAL_STRING("mozfullscreenerror"),
                       true,
                       false);
    e->PostDOMEvent();
    return;
  }

  // Turn off full-screen state in all documents which were previously
  // full-screen but which shouldn't be after this request is granted.
  // Note commonAncestor will be null when in a separate browser window
  // to the requesting document.
  nsIDocument* commonAncestor = nsnull;
  nsCOMPtr<nsIDocument> fullScreenDoc(do_QueryReferent(sFullScreenDoc));
  if (fullScreenDoc) {
    commonAncestor = GetCommonAncestor(fullScreenDoc, this);
  }
  nsIDocument* doc = fullScreenDoc;
  while (doc != commonAncestor) {
    if (::SetFullScreenState(doc, nsnull, false)) {
      DispatchFullScreenChange(doc);
    }
    doc = doc->GetParentDocument();
  }
  if (!commonAncestor && fullScreenDoc) {
    // Other doc is in another browser window. Move it out of full-screen.
    // Note that nsGlobalWindow::SetFullScreen() proxies to the root window
    // in its hierarchy, and does not operate on the a per-nsIDOMWindow basis.
    SetWindowFullScreen(fullScreenDoc, false);
  }

  // Remember the root document, so that if a full-screen document is hidden
  // we can reset full-screen state in the remaining visible full-screen documents.
  sFullScreenRootDoc = do_GetWeakReference(nsContentUtils::GetRootDocument(this));

  // Set the full-screen element. This sets the full-screen style on the
  // element, and the full-screen-ancestor styles on ancestors of the element
  // in this document.
  if (SetFullScreenState(aElement, true)) {
    DispatchFullScreenChange(aElement);
  }

  // Propagate up the document hierarchy, setting the full-screen element as
  // the element's container in ancestor documents. This also sets the
  // appropriate css styles as well. Note we don't propagate down the
  // document hierarchy, the full-screen element (or its container) is not
  // visible there.  
  nsIDocument* child = this;
  nsIDocument* parent;
  while ((parent = child->GetParentDocument())) {
    Element* element = parent->FindContentForSubDocument(child)->AsElement();
    if (::SetFullScreenState(parent, element, true)) {
      DispatchFullScreenChange(element);
    }
    child = parent;
  }

  // Make the window full-screen. Note we must make the state changes above
  // before making the window full-screen, as then the document reports as
  // being in full-screen mode when the chrome "fullscreen" event fires,
  // enabling chrome to distinguish between browser and dom full-screen
  // modes.
  SetWindowFullScreen(this, true);

  // Remember this is the requesting full-screen document.
  sFullScreenDoc = do_GetWeakReference(static_cast<nsIDocument*>(this));
}

NS_IMETHODIMP
nsDocument::GetMozFullScreenElement(nsIDOMHTMLElement **aFullScreenElement)
{
  NS_ENSURE_ARG_POINTER(aFullScreenElement);
  *aFullScreenElement = nsnull;
  if (IsFullScreenDoc()) {
    // Must have a full-screen element while in full-screen mode.
    NS_ENSURE_STATE(GetFullScreenElement());
    CallQueryInterface(GetFullScreenElement(), aFullScreenElement);
  }
  return NS_OK;
}

Element*
nsDocument::GetFullScreenElement()
{
  return mFullScreenElement;
}

NS_IMETHODIMP
nsDocument::GetMozFullScreen(bool *aFullScreen)
{
  NS_ENSURE_ARG_POINTER(aFullScreen);
  *aFullScreen = IsFullScreenDoc();
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetMozFullScreenEnabled(bool *aFullScreen)
{
  NS_ENSURE_ARG_POINTER(aFullScreen);
  *aFullScreen = IsFullScreenEnabled(nsContentUtils::IsCallerChrome());
  return NS_OK;
}

bool
nsDocument::IsFullScreenEnabled(bool aCallerIsChrome)
{
  if (nsContentUtils::IsFullScreenApiEnabled() && aCallerIsChrome) {
    // Chrome code can always use the full-screen API, provided it's not
    // explicitly disabled. Note IsCallerChrome() returns true when running
    // in an nsRunnable, so don't use GetMozFullScreenEnabled() from an
    // nsRunnable!
    return true;
  }

  if (!nsContentUtils::IsFullScreenApiEnabled() ||
      nsContentUtils::HasPluginWithUncontrolledEventDispatch(this) ||
      !IsVisible()) {
    return false;
  }

  // Ensure that all ancestor <iframe> elements have the mozallowfullscreen
  // boolean attribute set.
  nsINode* node = static_cast<nsINode*>(this);
  do {
    nsIContent* content = static_cast<nsIContent*>(node);
    if (content->IsHTML(nsGkAtoms::iframe) &&
        !content->HasAttr(kNameSpaceID_None, nsGkAtoms::mozallowfullscreen)) {
      // The node requesting fullscreen, or one of its crossdoc ancestors,
      // is an iframe which doesn't have the "mozalllowfullscreen" attribute.
      // This request is not authorized by the parent document.
      return false;
    }
    node = nsContentUtils::GetCrossDocParentNode(node);
  } while (node);

  return true;
}

PRInt64
nsDocument::SizeOf() const
{
  PRInt64 size = MemoryReporter::GetBasicSize<nsDocument, nsIDocument>(this);
  size += mAttrStyleSheet ? mAttrStyleSheet->DOMSizeOf() : 0;
  return size;
}

#define EVENT(name_, id_, type_, struct_)                                 \
  NS_IMETHODIMP nsDocument::GetOn##name_(JSContext *cx, jsval *vp) {      \
    return nsINode::GetOn##name_(cx, vp);                                 \
  }                                                                       \
  NS_IMETHODIMP nsDocument::SetOn##name_(JSContext *cx, const jsval &v) { \
    return nsINode::SetOn##name_(cx, v);                                  \
  }
#define TOUCH_EVENT EVENT
#define DOCUMENT_ONLY_EVENT EVENT
#include "nsEventNameList.h"
#undef DOCUMENT_ONLY_EVENT
#undef TOUCH_EVENT
#undef EVENT

void
nsDocument::UpdateVisibilityState()
{
  VisibilityState oldState = mVisibilityState;
  mVisibilityState = GetVisibilityState();
  if (oldState != mVisibilityState) {
    nsContentUtils::DispatchTrustedEvent(this, static_cast<nsIDocument*>(this),
                                         NS_LITERAL_STRING("mozvisibilitychange"),
                                         false, false);
  }
}

nsDocument::VisibilityState
nsDocument::GetVisibilityState() const
{
  // We have to check a few pieces of information here:
  // 1)  Are we in bfcache (!IsVisible())?  If so, nothing else matters.
  // 2)  Do we have an outer window?  If not, we're hidden.  Note that we don't
  //     want to use GetWindow here because it does weird groveling for windows
  //     in some cases.
  // 3)  Is our outer window background?  If so, we're hidden.
  // Otherwise, we're visible.
  if (!IsVisible() || !mWindow || !mWindow->GetOuterWindow() ||
      mWindow->GetOuterWindow()->IsBackground()) {
    return eHidden;
  }

  return eVisible;
}

/* virtual */ void
nsDocument::PostVisibilityUpdateEvent()
{
  nsCOMPtr<nsIRunnable> event =
    NS_NewRunnableMethod(this, &nsDocument::UpdateVisibilityState);
  NS_DispatchToMainThread(event);
}

NS_IMETHODIMP
nsDocument::GetMozHidden(bool* aHidden)
{
  *aHidden = mVisibilityState != eVisible;
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetMozVisibilityState(nsAString& aState)
{
  // This needs to stay in sync with the VisibilityState enum.
  static const char states[][8] = {
    "hidden",
    "visible"
  };
  PR_STATIC_ASSERT(NS_ARRAY_LENGTH(states) == eVisibilityStateCount);
  aState.AssignASCII(states[mVisibilityState]);
  return NS_OK;
}
