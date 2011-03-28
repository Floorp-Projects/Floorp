/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80: */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Kathleen Brade <brade@netscape.com>
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
#include "nsICharsetAlias.h"

#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsPrintfCString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsHTMLDocument.h"
#include "nsIParserFilter.h"
#include "nsIHTMLContentSink.h"
#include "nsIXMLContentSink.h"
#include "nsHTMLParts.h"
#include "nsHTMLStyleSheet.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIDOMNode.h" // for Find
#include "nsIDOMNodeList.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"
#include "nsIDOMComment.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMDocumentType.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsDOMString.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsIIOService.h"
#include "nsNetUtil.h"
#include "nsIContentViewerContainer.h"
#include "nsIContentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsDocShellLoadTypes.h"
#include "nsIWebNavigation.h"
#include "nsIBaseWindow.h"
#include "nsIWebShellServices.h"
#include "nsIScriptContext.h"
#include "nsIXPConnect.h"
#include "nsContentList.h"
#include "nsDOMError.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsAttrName.h"
#include "nsNodeUtils.h"

#include "nsNetCID.h"
#include "nsIIOService.h"
#include "nsICookieService.h"

#include "nsIServiceManager.h"
#include "nsIConsoleService.h"
#include "nsIComponentManager.h"
#include "nsParserCIID.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMHTMLHeadElement.h"
#include "nsINameSpaceManager.h"
#include "nsGenericHTMLElement.h"
#include "mozilla/css/Loader.h"
#include "nsIHttpChannel.h"
#include "nsIFile.h"
#include "nsIEventListenerManager.h"
#include "nsISelectElement.h"
#include "nsFrameSelection.h"
#include "nsISelectionPrivate.h"//for toStringwithformat code

#include "nsICharsetDetector.h"
#include "nsICharsetDetectionAdaptor.h"
#include "nsCharsetDetectionAdaptorCID.h"
#include "nsICharsetAlias.h"
#include "nsContentUtils.h"
#include "nsJSUtils.h"
#include "nsIDocumentCharsetInfo.h"
#include "nsIDocumentEncoder.h" //for outputting selection
#include "nsICharsetResolver.h"
#include "nsICachingChannel.h"
#include "nsIJSContextStack.h"
#include "nsIDocumentViewer.h"
#include "nsIWyciwygChannel.h"
#include "nsIScriptElement.h"
#include "nsIScriptError.h"
#include "nsIMutableArray.h"
#include "nsArrayUtils.h"
#include "nsIEffectiveTLDService.h"
#include "nsIEventStateManager.h"

#include "nsIPrompt.h"
//AHMED 12-2
#include "nsBidiUtils.h"

#include "nsIEditingSession.h"
#include "nsIEditor.h"
#include "nsNodeInfoManager.h"
#include "nsIPlaintextEditor.h"
#include "nsIHTMLEditor.h"
#include "nsIEditorDocShell.h"
#include "nsIEditorStyleSheets.h"
#include "nsIInlineSpellChecker.h"
#include "nsRange.h"
#include "mozAutoDocUpdate.h"
#include "nsCCUncollectableMarker.h"
#include "nsHtml5Module.h"
#include "prprf.h"
#include "mozilla/dom/Element.h"

using namespace mozilla::dom;

#define NS_MAX_DOCUMENT_WRITE_DEPTH 20

#define DETECTOR_CONTRACTID_MAX 127
static char g_detector_contractid[DETECTOR_CONTRACTID_MAX + 1];
static PRBool gInitDetector = PR_FALSE;
static PRBool gPlugDetector = PR_FALSE;

#include "prmem.h"
#include "prtime.h"

// Find/Search Includes
const PRInt32 kForward  = 0;
const PRInt32 kBackward = 1;

//#define DEBUG_charset

static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

PRUint32       nsHTMLDocument::gWyciwygSessionCnt = 0;

// this function will return false if the command is not recognized
// inCommandID will be converted as necessary for internal operations
// inParam will be converted as necessary for internal operations
// outParam will be Empty if no parameter is needed or if returning a boolean
// outIsBoolean will determine whether to send param as a boolean or string
// outBooleanParam will not be set unless outIsBoolean
static PRBool ConvertToMidasInternalCommand(const nsAString & inCommandID,
                                            const nsAString & inParam,
                                            nsACString& outCommandID,
                                            nsACString& outParam,
                                            PRBool& isBoolean,
                                            PRBool& boolValue);

static PRBool ConvertToMidasInternalCommand(const nsAString & inCommandID,
                                            nsACString& outCommandID);
static int
MyPrefChangedCallback(const char*aPrefName, void* instance_data)
{
  const nsAdoptingString& detector_name =
    nsContentUtils::GetLocalizedStringPref("intl.charset.detector");

  if (detector_name.Length() > 0) {
    PL_strncpy(g_detector_contractid, NS_CHARSET_DETECTOR_CONTRACTID_BASE,
               DETECTOR_CONTRACTID_MAX);
    PL_strncat(g_detector_contractid,
               NS_ConvertUTF16toUTF8(detector_name).get(),
               DETECTOR_CONTRACTID_MAX);
    gPlugDetector = PR_TRUE;
  } else {
    g_detector_contractid[0]=0;
    gPlugDetector = PR_FALSE;
  }

  return 0;
}

// ==================================================================
// =
// ==================================================================
static void
ReportUseOfDeprecatedMethod(nsHTMLDocument* aDoc, const char* aWarning)
{
  nsContentUtils::ReportToConsole(nsContentUtils::eDOM_PROPERTIES,
                                  aWarning,
                                  nsnull, 0,
                                  nsnull,
                                  EmptyString(), 0, 0,
                                  nsIScriptError::warningFlag,
                                  "DOM Events", aDoc);
}

static nsresult
RemoveFromAgentSheets(nsCOMArray<nsIStyleSheet> &aAgentSheets, const nsAString& url)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), url);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 i = aAgentSheets.Count() - 1; i >= 0; --i) {
    nsIStyleSheet* sheet = aAgentSheets[i];
    nsIURI* sheetURI = sheet->GetSheetURI();

    PRBool equals = PR_FALSE;
    uri->Equals(sheetURI, &equals);
    if (equals) {
      aAgentSheets.RemoveObjectAt(i);
    }
  }

  return NS_OK;
}

nsresult
NS_NewHTMLDocument(nsIDocument** aInstancePtrResult)
{
  nsHTMLDocument* doc = new nsHTMLDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(doc);
  nsresult rv = doc->Init();

  if (NS_FAILED(rv)) {
    NS_RELEASE(doc);
  }

  *aInstancePtrResult = doc;

  return rv;
}

  // NOTE! nsDocument::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

nsHTMLDocument::nsHTMLDocument()
  : nsDocument("text/html")
{
  // NOTE! nsDocument::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

  mIsRegularHTML = PR_TRUE;
  mDefaultElementType = kNameSpaceID_XHTML;
  mCompatMode = eCompatibility_NavQuirks;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHTMLDocument)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHTMLDocument, nsDocument)
  NS_ASSERTION(!nsCCUncollectableMarker::InGeneration(cb, tmp->GetMarkedCCGeneration()),
               "Shouldn't traverse nsHTMLDocument!");
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mImages)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mApplets)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mEmbeds)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mLinks)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mAnchors)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mForms, nsIDOMNodeList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mFormControls,
                                                       nsIDOMNodeList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mImageMaps,
                                                       nsIDOMNodeList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mWyciwygChannel)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mMidasCommandManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFragmentParser)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsHTMLDocument, nsDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mImages)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mApplets)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mEmbeds)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mLinks)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mAnchors)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mForms)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFormControls)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mImageMaps)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mWyciwygChannel)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mMidasCommandManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFragmentParser)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(nsHTMLDocument, nsDocument)
NS_IMPL_RELEASE_INHERITED(nsHTMLDocument, nsDocument)


DOMCI_NODE_DATA(HTMLDocument, nsHTMLDocument)

// QueryInterface implementation for nsHTMLDocument
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsHTMLDocument)
  NS_DOCUMENT_INTERFACE_TABLE_BEGIN(nsHTMLDocument)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLDocument, nsIHTMLDocument)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLDocument, nsIDOMHTMLDocument)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLDocument, nsIDOMNSHTMLDocument)
  NS_OFFSET_AND_INTERFACE_TABLE_END
  NS_OFFSET_AND_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(HTMLDocument)
NS_INTERFACE_MAP_END_INHERITING(nsDocument)


nsresult
nsHTMLDocument::Init()
{
  nsresult rv = nsDocument::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  // Now reset the compatibility mode of the CSSLoader
  // to match our compat mode.
  CSSLoader()->SetCompatibilityMode(mCompatMode);

  PrePopulateIdentifierMap();
  return NS_OK;
}


void
nsHTMLDocument::Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup)
{
  nsDocument::Reset(aChannel, aLoadGroup);

  if (aChannel) {
    aChannel->GetLoadFlags(&mLoadFlags);
  }
}

void
nsHTMLDocument::ResetToURI(nsIURI *aURI, nsILoadGroup *aLoadGroup,
                           nsIPrincipal* aPrincipal)
{
  mLoadFlags = nsIRequest::LOAD_NORMAL;

  nsDocument::ResetToURI(aURI, aLoadGroup, aPrincipal);

  PrePopulateIdentifierMap();

  mImages = nsnull;
  mApplets = nsnull;
  mEmbeds = nsnull;
  mLinks = nsnull;
  mAnchors = nsnull;

  mForms = nsnull;

  NS_ASSERTION(!mWyciwygChannel,
               "nsHTMLDocument::Reset() - Wyciwyg Channel  still exists!");

  mWyciwygChannel = nsnull;

  // Make the content type default to "text/html", we are a HTML
  // document, after all. Once we start getting data, this may be
  // changed.
  SetContentTypeInternal(nsDependentCString("text/html"));
}

nsresult
nsHTMLDocument::CreateShell(nsPresContext* aContext,
                            nsIViewManager* aViewManager,
                            nsStyleSet* aStyleSet,
                            nsIPresShell** aInstancePtrResult)
{
  return doCreateShell(aContext, aViewManager, aStyleSet, mCompatMode,
                       aInstancePtrResult);
}

// The following Try*Charset will return PR_FALSE only if the charset source
// should be considered (ie. aCharsetSource < thisCharsetSource) but we failed
// to get the charset from this source.

PRBool
nsHTMLDocument::TryHintCharset(nsIMarkupDocumentViewer* aMarkupDV,
                               PRInt32& aCharsetSource, nsACString& aCharset)
{
  if (aMarkupDV) {
    PRInt32 requestCharsetSource;
    nsresult rv = aMarkupDV->GetHintCharacterSetSource(&requestCharsetSource);

    if(NS_SUCCEEDED(rv) && kCharsetUninitialized != requestCharsetSource) {
      nsCAutoString requestCharset;
      rv = aMarkupDV->GetHintCharacterSet(requestCharset);
      aMarkupDV->SetHintCharacterSetSource((PRInt32)(kCharsetUninitialized));

      if(requestCharsetSource <= aCharsetSource)
        return PR_TRUE;

      if(NS_SUCCEEDED(rv)) {
        aCharsetSource = requestCharsetSource;
        aCharset = requestCharset;

        return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}


PRBool
nsHTMLDocument::TryUserForcedCharset(nsIMarkupDocumentViewer* aMarkupDV,
                                     nsIDocumentCharsetInfo*  aDocInfo,
                                     PRInt32& aCharsetSource,
                                     nsACString& aCharset)
{
  nsresult rv = NS_OK;

  if(kCharsetFromUserForced <= aCharsetSource)
    return PR_TRUE;

  nsCAutoString forceCharsetFromDocShell;
  if (aMarkupDV) {
    rv = aMarkupDV->GetForceCharacterSet(forceCharsetFromDocShell);
  }

  if(NS_SUCCEEDED(rv) && !forceCharsetFromDocShell.IsEmpty()) {
    aCharset = forceCharsetFromDocShell;
    //TODO: we should define appropriate constant for force charset
    aCharsetSource = kCharsetFromUserForced;
  } else if (aDocInfo) {
    nsCOMPtr<nsIAtom> csAtom;
    aDocInfo->GetForcedCharset(getter_AddRefs(csAtom));
    if (csAtom) {
      csAtom->ToUTF8String(aCharset);
      aCharsetSource = kCharsetFromUserForced;
      aDocInfo->SetForcedCharset(nsnull);
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

PRBool
nsHTMLDocument::TryCacheCharset(nsICachingChannel* aCachingChannel,
                                PRInt32& aCharsetSource,
                                nsACString& aCharset)
{
  nsresult rv;

  if (kCharsetFromCache <= aCharsetSource) {
    return PR_TRUE;
  }

  nsCString cachedCharset;
  rv = aCachingChannel->GetCacheTokenCachedCharset(cachedCharset);
  if (NS_SUCCEEDED(rv) && !cachedCharset.IsEmpty())
  {
    aCharset = cachedCharset;
    aCharsetSource = kCharsetFromCache;

    return PR_TRUE;
  }

  return PR_FALSE;
}

static PRBool
CheckSameOrigin(nsINode* aNode1, nsINode* aNode2)
{
  NS_PRECONDITION(aNode1, "Null node?");
  NS_PRECONDITION(aNode2, "Null node?");

  PRBool equal;
  return
    NS_SUCCEEDED(aNode1->NodePrincipal()->
                   Equals(aNode2->NodePrincipal(), &equal)) &&
    equal;
}

PRBool
nsHTMLDocument::TryParentCharset(nsIDocumentCharsetInfo*  aDocInfo,
                                 nsIDocument* aParentDocument,
                                 PRInt32& aCharsetSource,
                                 nsACString& aCharset)
{
  if (aDocInfo) {
    PRInt32 source;
    nsCOMPtr<nsIAtom> csAtom;
    PRInt32 parentSource;
    aDocInfo->GetParentCharsetSource(&parentSource);
    if (kCharsetFromParentForced <= parentSource)
      source = kCharsetFromParentForced;
    else if (kCharsetFromHintPrevDoc == parentSource) {
      // Make sure that's OK
      if (!aParentDocument || !CheckSameOrigin(this, aParentDocument)) {
        return PR_FALSE;
      }
      
      // if parent is posted doc, set this prevent autodections
      // I'm not sure this makes much sense... but whatever.
      source = kCharsetFromHintPrevDoc;
    }
    else if (kCharsetFromCache <= parentSource) {
      // Make sure that's OK
      if (!aParentDocument || !CheckSameOrigin(this, aParentDocument)) {
        return PR_FALSE;
      }

      source = kCharsetFromParentFrame;
    }
    else
      return PR_FALSE;

    if (source < aCharsetSource)
      return PR_TRUE;

    aDocInfo->GetParentCharset(getter_AddRefs(csAtom));
    if (csAtom) {
      csAtom->ToUTF8String(aCharset);
      aCharsetSource = source;
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool
nsHTMLDocument::UseWeakDocTypeDefault(PRInt32& aCharsetSource,
                                      nsACString& aCharset)
{
  if (kCharsetFromWeakDocTypeDefault <= aCharsetSource)
    return PR_TRUE;
  // fallback value in case docshell return error
  aCharset.AssignLiteral("ISO-8859-1");

  const nsAdoptingString& defCharset =
    nsContentUtils::GetLocalizedStringPref("intl.charset.default");

  if (!defCharset.IsEmpty()) {
    LossyCopyUTF16toASCII(defCharset, aCharset);
    aCharsetSource = kCharsetFromWeakDocTypeDefault;
  }
  return PR_TRUE;
}

PRBool
nsHTMLDocument::TryDefaultCharset( nsIMarkupDocumentViewer* aMarkupDV,
                                   PRInt32& aCharsetSource,
                                   nsACString& aCharset)
{
  if(kCharsetFromUserDefault <= aCharsetSource)
    return PR_TRUE;

  nsCAutoString defaultCharsetFromDocShell;
  if (aMarkupDV) {
    nsresult rv =
      aMarkupDV->GetDefaultCharacterSet(defaultCharsetFromDocShell);
    if(NS_SUCCEEDED(rv)) {
      aCharset = defaultCharsetFromDocShell;

      aCharsetSource = kCharsetFromUserDefault;
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

void
nsHTMLDocument::StartAutodetection(nsIDocShell *aDocShell, nsACString& aCharset,
                                   const char* aCommand)
{
  if (mIsRegularHTML && 
      nsHtml5Module::sEnabled && 
      aCommand && 
      !nsCRT::strcmp(aCommand, "view")) {
    return; // the HTML5 parser uses chardet directly
  }
  nsCOMPtr <nsIParserFilter> cdetflt;

  nsresult rv_detect;
  if(!gInitDetector) {
    const nsAdoptingString& detector_name =
      nsContentUtils::GetLocalizedStringPref("intl.charset.detector");

    if(!detector_name.IsEmpty()) {
      PL_strncpy(g_detector_contractid, NS_CHARSET_DETECTOR_CONTRACTID_BASE,
                 DETECTOR_CONTRACTID_MAX);
      PL_strncat(g_detector_contractid,
                 NS_ConvertUTF16toUTF8(detector_name).get(),
                 DETECTOR_CONTRACTID_MAX);
      gPlugDetector = PR_TRUE;
    }

    nsContentUtils::RegisterPrefCallback("intl.charset.detector",
                                         MyPrefChangedCallback,
                                         nsnull);

    gInitDetector = PR_TRUE;
  }

  if (gPlugDetector) {
    nsCOMPtr <nsICharsetDetector> cdet =
      do_CreateInstance(g_detector_contractid, &rv_detect);
    if (NS_SUCCEEDED(rv_detect)) {
      cdetflt = do_CreateInstance(NS_CHARSET_DETECTION_ADAPTOR_CONTRACTID,
                                  &rv_detect);

      nsCOMPtr<nsICharsetDetectionAdaptor> adp = do_QueryInterface(cdetflt);
      if (adp) {
        nsCOMPtr<nsIWebShellServices> wss = do_QueryInterface(aDocShell);
        if (wss) {
          rv_detect = adp->Init(wss, cdet, this, mParser,
                                PromiseFlatCString(aCharset).get(), aCommand);

          if (mParser)
            mParser->SetParserFilter(cdetflt);
        }
      }
    }
    else {
      // IF we cannot create the detector, don't bother to
      // create one next time.
      gPlugDetector = PR_FALSE;
    }
  }
}

void
nsHTMLDocument::SetDocumentCharacterSet(const nsACString& aCharSetID)
{
  nsDocument::SetDocumentCharacterSet(aCharSetID);
  // Make sure to stash this charset on our channel as needed if it's a wyciwyg
  // channel.
  nsCOMPtr<nsIWyciwygChannel> wyciwygChannel = do_QueryInterface(mChannel);
  if (wyciwygChannel) {
    wyciwygChannel->SetCharsetAndSource(GetDocumentCharacterSetSource(),
                                        aCharSetID);
  }
}

nsresult
nsHTMLDocument::StartDocumentLoad(const char* aCommand,
                                  nsIChannel* aChannel,
                                  nsILoadGroup* aLoadGroup,
                                  nsISupports* aContainer,
                                  nsIStreamListener **aDocListener,
                                  PRBool aReset,
                                  nsIContentSink* aSink)
{
  PRBool loadAsHtml5 = nsHtml5Module::sEnabled;
  if (aSink) {
    loadAsHtml5 = PR_FALSE;
  }

  nsCAutoString contentType;
  aChannel->GetContentType(contentType);

  if (contentType.Equals("application/xhtml+xml") &&
      (!aCommand || nsCRT::strcmp(aCommand, "view-source") != 0)) {
    // We're parsing XHTML as XML, remember that.

    mIsRegularHTML = PR_FALSE;
    mCompatMode = eCompatibility_FullStandards;
    loadAsHtml5 = PR_FALSE;
  }
#ifdef DEBUG
  else {
    NS_ASSERTION(mIsRegularHTML,
                 "Hey, someone forgot to reset mIsRegularHTML!!!");
  }
#endif
  
  if (loadAsHtml5 && 
      !(contentType.EqualsLiteral("text/html") && 
        aCommand && 
        !nsCRT::strcmp(aCommand, "view"))) {
    loadAsHtml5 = PR_FALSE;
  }
  
  // TODO: Proper about:blank treatment is bug 543435
  if (loadAsHtml5) {
    // mDocumentURI hasn't been set, yet, so get the URI from the channel
    nsCOMPtr<nsIURI> uri;
    aChannel->GetOriginalURI(getter_AddRefs(uri));
    // Adapted from nsDocShell:
    // GetSpec can be expensive for some URIs, so check the scheme first.
    PRBool isAbout = PR_FALSE;
    if (uri && NS_SUCCEEDED(uri->SchemeIs("about", &isAbout)) && isAbout) {
      nsCAutoString str;
      uri->GetSpec(str);
      if (str.EqualsLiteral("about:blank")) {
        loadAsHtml5 = PR_FALSE;    
      }
    }
  }
  
  CSSLoader()->SetCompatibilityMode(mCompatMode);
  
  PRBool needsParser = PR_TRUE;
  if (aCommand)
  {
    if (!nsCRT::strcmp(aCommand, "view delayedContentLoad")) {
      needsParser = PR_FALSE;
    }
  }

  nsresult rv = nsDocument::StartDocumentLoad(aCommand,
                                              aChannel, aLoadGroup,
                                              aContainer,
                                              aDocListener, aReset);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Store the security info for future use with wyciwyg channels.
  aChannel->GetSecurityInfo(getter_AddRefs(mSecurityInfo));

  nsCOMPtr<nsIURI> uri;
  rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsICachingChannel> cachingChan = do_QueryInterface(aChannel);

  if (needsParser) {
    if (loadAsHtml5) {
      mParser = nsHtml5Module::NewHtml5Parser();
      mParser->MarkAsNotScriptCreated();
    } else {
      mParser = do_CreateInstance(kCParserCID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  PRInt32 textType = GET_BIDI_OPTION_TEXTTYPE(GetBidiOptions());

  // Look for the parent document.  Note that at this point we don't have our
  // content viewer set up yet, and therefore do not have a useful
  // mParentDocument.

  // in this block of code, if we get an error result, we return it
  // but if we get a null pointer, that's perfectly legal for parent
  // and parentContentViewer
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aContainer));

  // No support yet for docshell-less HTML
  NS_ENSURE_TRUE(docShell || !IsHTML(), NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));

  nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
  if (docShellAsItem) {
    docShellAsItem->GetSameTypeParent(getter_AddRefs(parentAsItem));
  }

  nsCOMPtr<nsIDocShell> parent(do_QueryInterface(parentAsItem));
  nsCOMPtr<nsIDocument> parentDocument;
  nsCOMPtr<nsIContentViewer> parentContentViewer;
  if (parent) {
    rv = parent->GetContentViewer(getter_AddRefs(parentContentViewer));
    NS_ENSURE_SUCCESS(rv, rv);
    if (parentContentViewer) {
      parentDocument = parentContentViewer->GetDocument();
    }
  }

  //
  // The following logic is mirrored in nsWebShell::Embed!
  //
  nsCOMPtr<nsIMarkupDocumentViewer> muCV;
  PRBool muCVIsParent = PR_FALSE;
  nsCOMPtr<nsIContentViewer> cv;
  if (docShell) {
    docShell->GetContentViewer(getter_AddRefs(cv));
  }
  if (cv) {
     muCV = do_QueryInterface(cv);
  } else {
    muCV = do_QueryInterface(parentContentViewer);
    if (muCV) {
      muCVIsParent = PR_TRUE;
    }
  }

  nsCAutoString scheme;
  uri->GetScheme(scheme);

  nsCAutoString urlSpec;
  uri->GetSpec(urlSpec);
#ifdef DEBUG_charset
  printf("Determining charset for %s\n", urlSpec.get());
#endif

  // These are the charset source and charset for our document
  PRInt32 charsetSource;
  nsCAutoString charset;

  // These are the charset source and charset for the parser.  This can differ
  // from that for the document if the channel is a wyciwyg channel.
  PRInt32 parserCharsetSource;
  nsCAutoString parserCharset;

  nsCOMPtr<nsIWyciwygChannel> wyciwygChannel;
  
  if (!IsHTML()) {
    charsetSource = kCharsetFromDocTypeDefault;
    charset.AssignLiteral("UTF-8");
    TryChannelCharset(aChannel, charsetSource, charset);
    parserCharsetSource = charsetSource;
    parserCharset = charset;
  } else {
    NS_ASSERTION(docShell && docShellAsItem, "Unexpected null value");
    
    nsCOMPtr<nsIDocumentCharsetInfo> dcInfo;
    docShell->GetDocumentCharsetInfo(getter_AddRefs(dcInfo));

    charsetSource = kCharsetUninitialized;
    wyciwygChannel = do_QueryInterface(aChannel);

    // The following charset resolving calls has implied knowledge
    // about charset source priority order. Each try will return true
    // if the source is higher or equal to the source as its name
    // describes. Some try call might change charset source to
    // multiple values, like TryHintCharset and TryParentCharset. It
    // should be always safe to try more sources.
    if (!TryUserForcedCharset(muCV, dcInfo, charsetSource, charset)) {
      TryHintCharset(muCV, charsetSource, charset);
      TryParentCharset(dcInfo, parentDocument, charsetSource, charset);

      // Don't actually get the charset from the channel if this is a
      // wyciwyg channel; it'll always be UTF-16
      if (!wyciwygChannel &&
          TryChannelCharset(aChannel, charsetSource, charset)) {
        // Use the channel's charset (e.g., charset from HTTP
        // "Content-Type" header).
      }
      else if (cachingChan && !urlSpec.IsEmpty() &&
               TryCacheCharset(cachingChan, charsetSource, charset)) {
        // Use the cache's charset.
      }
      else if (TryDefaultCharset(muCV, charsetSource, charset)) {
        // Use the default charset.
        // previous document charset might be inherited as default charset.
      }
      else {
        // Use the weak doc type default charset
        UseWeakDocTypeDefault(charsetSource, charset);
      }
    }

    PRBool isPostPage = PR_FALSE;
    // check if current doc is from POST command
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));
    if (httpChannel) {
      nsCAutoString methodStr;
      rv = httpChannel->GetRequestMethod(methodStr);
      isPostPage = (NS_SUCCEEDED(rv) &&
                    methodStr.EqualsLiteral("POST"));
    }

    if (isPostPage && muCV && kCharsetFromHintPrevDoc > charsetSource) {
      nsCAutoString requestCharset;
      muCV->GetPrevDocCharacterSet(requestCharset);
      if (!requestCharset.IsEmpty()) {
        charsetSource = kCharsetFromHintPrevDoc;
        charset = requestCharset;
      }
    }

    if (wyciwygChannel) {
      // We know for sure that the parser needs to be using UTF16.
      parserCharset = "UTF-16";
      parserCharsetSource = charsetSource < kCharsetFromChannel ?
        kCharsetFromChannel : charsetSource;
        
      nsCAutoString cachedCharset;
      PRInt32 cachedSource;
      rv = wyciwygChannel->GetCharsetAndSource(&cachedSource, cachedCharset);
      if (NS_SUCCEEDED(rv)) {
        if (cachedSource > charsetSource) {
          charsetSource = cachedSource;
          charset = cachedCharset;
        }
      } else {
        // Don't propagate this error.
        rv = NS_OK;
      }
      
    } else {
      parserCharset = charset;
      parserCharsetSource = charsetSource;
    }

    if(kCharsetFromAutoDetection > charsetSource && !isPostPage) {
      StartAutodetection(docShell, charset, aCommand);
    }

    // ahmed
    // Check if 864 but in Implicit mode !
    // XXXbz why is this happening after StartAutodetection ?
    if ((textType == IBMBIDI_TEXTTYPE_LOGICAL) &&
        (charset.LowerCaseEqualsLiteral("ibm864"))) {
      charset.AssignLiteral("IBM864i");
    }
  }

  SetDocumentCharacterSetSource(charsetSource);
  SetDocumentCharacterSet(charset);

  // set doc charset to muCV for next document.
  // Don't propagate this back up to the parent document if we have one.
  if (muCV && !muCVIsParent)
    muCV->SetPrevDocCharacterSet(charset);

  if (cachingChan) {
    NS_ASSERTION(charset == parserCharset,
                 "How did those end up different here?  wyciwyg channels are "
                 "not nsICachingChannel");
    rv = cachingChan->SetCacheTokenCachedCharset(charset);
    NS_ASSERTION(NS_SUCCEEDED(rv),"cannot SetMetaDataElement");
  }

  // Set the parser as the stream listener for the document loader...
  if (mParser) {
    rv = mParser->GetStreamListener(aDocListener);
    if (NS_FAILED(rv)) {
      return rv;
    }

#ifdef DEBUG_charset
    printf(" charset = %s source %d\n",
          charset.get(), charsetSource);
#endif
    mParser->SetDocumentCharset(parserCharset, parserCharsetSource);
    mParser->SetCommand(aCommand);

    // create the content sink
    nsCOMPtr<nsIContentSink> sink;

    if (aSink) {
      NS_ASSERTION(!loadAsHtml5, "Panic: We are loading as HTML5 and someone tries to set an external sink!");
      sink = aSink;
    } else {
      if (!IsHTML()) {
        nsCOMPtr<nsIXMLContentSink> xmlsink;
        rv = NS_NewXMLContentSink(getter_AddRefs(xmlsink), this, uri,
                                  docShell, aChannel);

        sink = xmlsink;
      } else {
        if (loadAsHtml5) {
          nsHtml5Module::Initialize(mParser, this, uri, docShell, aChannel);
          sink = mParser->GetContentSink();
        } else {
          nsCOMPtr<nsIHTMLContentSink> htmlsink;

          rv = NS_NewHTMLContentSink(getter_AddRefs(htmlsink), this, uri,
                                     docShell, aChannel);

          sink = htmlsink;
        }
      }
      NS_ENSURE_SUCCESS(rv, rv);

      NS_ASSERTION(sink,
                   "null sink with successful result from factory method");
    }

    mParser->SetContentSink(sink);
    // parser the content of the URI
    mParser->Parse(uri, nsnull, (void *)this);
  }

  return rv;
}

void
nsHTMLDocument::StopDocumentLoad()
{
  if (nsHtml5Module::sEnabled) {
    BlockOnload();
    if (mWriteState == eDocumentOpened) {
      NS_ASSERTION(IsHTML(), "document.open()ed doc is not HTML?");

      // Marking the document as closed, since pending scripts will be
      // stopped by nsDocument::StopDocumentLoad() below
      mWriteState = eDocumentClosed;

      // Remove the wyciwyg channel request from the document load group
      // that we added in OpenCommon().
      NS_ASSERTION(mWyciwygChannel, "nsHTMLDocument::StopDocumentLoad(): "
                   "Trying to remove nonexistent wyciwyg channel!");
      RemoveWyciwygChannel();
      NS_ASSERTION(!mWyciwygChannel, "nsHTMLDocument::StopDocumentLoad(): "
                   "nsIWyciwygChannel could not be removed!");
    }
    nsDocument::StopDocumentLoad();
    UnblockOnload(PR_FALSE);
    return;
  }
  // Code for the old parser:

  // If we're writing (i.e., there's been a document.open call), then
  // nsDocument::StopDocumentLoad will do the wrong thing and simply terminate
  // our parser.
  if (mWriteState != eNotWriting) {
    Close();
  } else {
    nsDocument::StopDocumentLoad();
  }
}

// static
void
nsHTMLDocument::DocumentWriteTerminationFunc(nsISupports *aRef)
{
  nsCOMPtr<nsIArray> arr = do_QueryInterface(aRef);
  NS_ASSERTION(arr, "Must have array!");

  nsCOMPtr<nsIDocument> doc = do_QueryElementAt(arr, 0);
  NS_ASSERTION(doc, "Must have document!");
  
  nsCOMPtr<nsIParser> parser = do_QueryElementAt(arr, 1);
  NS_ASSERTION(parser, "Must have parser!");

  nsHTMLDocument *htmldoc = static_cast<nsHTMLDocument*>(doc.get());

  // Check whether htmldoc still has the same parser.  If not, it's
  // not for us to mess with it.
  if (htmldoc->mParser != parser) {
    return;
  }

  // If the document is in the middle of a document.write() call, this
  // most likely means that script on a page document.write()'d out a
  // script tag that did location="..." and we're right now finishing
  // up executing the script that was written with
  // document.write(). Since there's still script on the stack (the
  // script that called document.write()) we don't want to release the
  // parser now, that would cause the next document.write() call to
  // cancel the load that was initiated by the location="..." in the
  // script that was written out by document.write().

  if (!htmldoc->mWriteLevel && htmldoc->mWriteState != eDocumentOpened) {
    // Release the document's parser so that the call to EndLoad()
    // doesn't just return early and set the termination function again.

    htmldoc->mParser = nsnull;
  }

  htmldoc->EndLoad();
}

void
nsHTMLDocument::BeginLoad()
{
  if (IsEditingOn()) {
    // Reset() blows away all event listeners in the document, and our
    // editor relies heavily on those. Midas is turned on, to make it
    // work, re-initialize it to give it a chance to add its event
    // listeners again.

    TurnEditingOff();
    EditingStateChanged();
  }
  nsDocument::BeginLoad();
}

void
nsHTMLDocument::EndLoad()
{
  if (mParser && mWriteState != eDocumentClosed) {
    nsCOMPtr<nsIJSContextStack> stack =
      do_GetService("@mozilla.org/js/xpc/ContextStack;1");

    if (stack) {
      JSContext *cx = nsnull;
      stack->Peek(&cx);

      if (cx) {
        nsIScriptContext *scx = nsJSUtils::GetDynamicScriptContext(cx);

        if (scx) {
          // The load of the document was terminated while we're
          // called from within JS and we have a parser (i.e. we're in
          // the middle of doing document.write()). In stead of
          // releasing the parser and ending the document load
          // directly, we'll make that happen once the script is done
          // executing. This way subsequent document.write() calls
          // won't end up creating a new parser and interrupting other
          // loads that were started while the script was
          // running. I.e. this makes the following case work as
          // expected:
          //
          //   document.write("foo");
          //   location.href = "http://www.mozilla.org";
          //   document.write("bar");

          nsresult rv;

          nsCOMPtr<nsIMutableArray> arr =
            do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
          if (NS_SUCCEEDED(rv)) {
            rv = arr->AppendElement(static_cast<nsIDocument*>(this),
                                    PR_FALSE);
            if (NS_SUCCEEDED(rv)) {
              rv = arr->AppendElement(mParser, PR_FALSE);
              if (NS_SUCCEEDED(rv)) {
                rv = scx->SetTerminationFunction(DocumentWriteTerminationFunc,
                                                 arr);
                // If we fail to set the termination function, just go ahead
                // and EndLoad now.  The slight bugginess involved is better
                // than leaking.
                if (NS_SUCCEEDED(rv)) {
                  return;
                }
              }
            }
          }
        }
      }
    }
  }

  // Reset this now, since we're really done "loading" this document.written
  // document.
  NS_ASSERTION(mWriteState == eNotWriting || mWriteState == ePendingClose ||
               mWriteState == eDocumentClosed, "EndLoad called early");
  mWriteState = eNotWriting;

  PRBool turnOnEditing =
    mParser && (HasFlag(NODE_IS_EDITABLE) || mContentEditableCount > 0);
  // Note: nsDocument::EndLoad nulls out mParser.
  nsDocument::EndLoad();
  if (turnOnEditing) {
    EditingStateChanged();
  }
}

NS_IMETHODIMP
nsHTMLDocument::SetTitle(const nsAString& aTitle)
{
  return nsDocument::SetTitle(aTitle);
}

Element*
nsHTMLDocument::GetImageMap(const nsAString& aMapName)
{
  if (!mImageMaps) {
    mImageMaps = new nsContentList(this, kNameSpaceID_XHTML, nsGkAtoms::map, nsGkAtoms::map);
  }

  nsAutoString name;
  PRUint32 i, n = mImageMaps->Length(PR_TRUE);
  for (i = 0; i < n; ++i) {
    nsIContent* map = mImageMaps->GetNodeAt(i);
    if (map->AttrValueIs(kNameSpaceID_None, nsGkAtoms::id, aMapName,
                         eCaseMatters) ||
        map->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name, aMapName,
                         eIgnoreCase)) {
      return map->AsElement();
    }
  }

  return NULL;
}

void
nsHTMLDocument::SetCompatibilityMode(nsCompatibility aMode)
{
  NS_ASSERTION(IsHTML() || aMode == eCompatibility_FullStandards,
               "Bad compat mode for XHTML document!");

  mCompatMode = aMode;
  CSSLoader()->SetCompatibilityMode(mCompatMode);
  nsCOMPtr<nsIPresShell> shell = GetShell();
  if (shell) {
    nsPresContext *pc = shell->GetPresContext();
    if (pc) {
      pc->CompatibilityModeChanged();
    }
  }
}

//
// nsIDOMDocument interface implementation
//
NS_IMETHODIMP
nsHTMLDocument::CreateElement(const nsAString& aTagName,
                              nsIDOMElement** aReturn)
{
  return nsDocument::CreateElement(aTagName, aReturn);
}

NS_IMETHODIMP
nsHTMLDocument::CreateElementNS(const nsAString& aNamespaceURI,
                                const nsAString& aQualifiedName,
                                nsIDOMElement** aReturn)
{
  return nsDocument::CreateElementNS(aNamespaceURI, aQualifiedName, aReturn);
}

NS_IMETHODIMP
nsHTMLDocument::CreateProcessingInstruction(const nsAString& aTarget,
                                            const nsAString& aData,
                                            nsIDOMProcessingInstruction** aReturn)
{
  if (!IsHTML()) {
    return nsDocument::CreateProcessingInstruction(aTarget, aData, aReturn);
  }

  // There are no PIs for HTML
  *aReturn = nsnull;

  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsHTMLDocument::CreateCDATASection(const nsAString& aData,
                                   nsIDOMCDATASection** aReturn)
{
  if (!IsHTML()) {
    return nsDocument::CreateCDATASection(aData, aReturn);
  }

  // There are no CDATASections in HTML
  *aReturn = nsnull;

  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsHTMLDocument::CreateEntityReference(const nsAString& aName,
                                      nsIDOMEntityReference** aReturn)
{
  if (!IsHTML()) {
    return nsDocument::CreateEntityReference(aName, aReturn);
  }

  // There are no EntityReferences in HTML
  *aReturn = nsnull;

  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsHTMLDocument::GetDoctype(nsIDOMDocumentType** aDocumentType)
{
  return nsDocument::GetDoctype(aDocumentType);
}

NS_IMETHODIMP
nsHTMLDocument::GetImplementation(nsIDOMDOMImplementation** aImplementation)
{
  return nsDocument::GetImplementation(aImplementation);
}

NS_IMETHODIMP
nsHTMLDocument::GetDocumentElement(nsIDOMElement** aDocumentElement)
{
  return nsDocument::GetDocumentElement(aDocumentElement);
}

NS_IMETHODIMP
nsHTMLDocument::CreateDocumentFragment(nsIDOMDocumentFragment** aReturn)
{
  return nsDocument::CreateDocumentFragment(aReturn);
}

NS_IMETHODIMP
nsHTMLDocument::CreateComment(const nsAString& aData, nsIDOMComment** aReturn)
{
  return nsDocument::CreateComment(aData, aReturn);
}

NS_IMETHODIMP
nsHTMLDocument::CreateAttribute(const nsAString& aName, nsIDOMAttr** aReturn)
{
  return nsDocument::CreateAttribute(aName, aReturn);
}

NS_IMETHODIMP
nsHTMLDocument::CreateTextNode(const nsAString& aData, nsIDOMText** aReturn)
{
  return nsDocument::CreateTextNode(aData, aReturn);
}

NS_IMETHODIMP
nsHTMLDocument::GetElementsByTagName(const nsAString& aTagname,
                                     nsIDOMNodeList** aReturn)
{
  return nsDocument::GetElementsByTagName(aTagname, aReturn);
}

NS_IMETHODIMP
nsHTMLDocument::GetInputEncoding(nsAString& aInputEncoding)
{
  return nsDocument::GetInputEncoding(aInputEncoding);
}

NS_IMETHODIMP
nsHTMLDocument::GetXmlEncoding(nsAString& aXmlEncoding)
{
  if (!IsHTML()) {
    return nsDocument::GetXmlEncoding(aXmlEncoding);
  }

  SetDOMStringToNull(aXmlEncoding);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetXmlStandalone(PRBool* aXmlStandalone)
{
  if (!IsHTML()) {
    return nsDocument::GetXmlStandalone(aXmlStandalone);
  }

  *aXmlStandalone = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetXmlStandalone(PRBool aXmlStandalone)
{
  if (!IsHTML()) {
    return nsDocument::SetXmlStandalone(aXmlStandalone);
  }

  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsHTMLDocument::GetXmlVersion(nsAString& aXmlVersion)
{
  if (!IsHTML()) {
    return nsDocument::GetXmlVersion(aXmlVersion);
  }

  SetDOMStringToNull(aXmlVersion);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetXmlVersion(const nsAString& aXmlVersion)
{
  if (!IsHTML()) {
    return nsDocument::SetXmlVersion(aXmlVersion);
  }

  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsHTMLDocument::GetStrictErrorChecking(PRBool* aStrictErrorChecking)
{
  return nsDocument::GetStrictErrorChecking(aStrictErrorChecking);
}

NS_IMETHODIMP
nsHTMLDocument::SetStrictErrorChecking(PRBool aStrictErrorChecking)
{
  return nsDocument::SetStrictErrorChecking(aStrictErrorChecking);
}

NS_IMETHODIMP
nsHTMLDocument::GetDocumentURI(nsAString& aDocumentURI)
{
  return nsDocument::GetDocumentURI(aDocumentURI);
}

NS_IMETHODIMP
nsHTMLDocument::SetDocumentURI(const nsAString& aDocumentURI)
{
  return nsDocument::SetDocumentURI(aDocumentURI);
}

NS_IMETHODIMP
nsHTMLDocument::AdoptNode(nsIDOMNode* aSource, nsIDOMNode** aRetval)
{
  return nsDocument::AdoptNode(aSource, aRetval);
}

NS_IMETHODIMP
nsHTMLDocument::GetDomConfig(nsIDOMDOMConfiguration** aDomConfig)
{
  return nsDocument::GetDomConfig(aDomConfig);
}

NS_IMETHODIMP
nsHTMLDocument::NormalizeDocument()
{
  return nsDocument::NormalizeDocument();
}

NS_IMETHODIMP
nsHTMLDocument::RenameNode(nsIDOMNode* aNode,
                           const nsAString& aNamespaceURI,
                           const nsAString& aQualifiedName,
                           nsIDOMNode** aRetval)
{
  return nsDocument::RenameNode(aNode, aNamespaceURI, aQualifiedName, aRetval);
}

//
// nsIDOMHTMLDocument interface implementation
//
NS_IMETHODIMP
nsHTMLDocument::GetTitle(nsAString& aTitle)
{
  return nsDocument::GetTitle(aTitle);
}

NS_IMETHODIMP
nsHTMLDocument::GetReferrer(nsAString& aReferrer)
{
  return nsDocument::GetReferrer(aReferrer);
}

void
nsHTMLDocument::GetDomainURI(nsIURI **aURI)
{
  nsIPrincipal *principal = NodePrincipal();

  principal->GetDomain(aURI);
  if (!*aURI) {
    principal->GetURI(aURI);
  }
}


NS_IMETHODIMP
nsHTMLDocument::GetDomain(nsAString& aDomain)
{
  nsCOMPtr<nsIURI> uri;
  GetDomainURI(getter_AddRefs(uri));

  if (!uri) {
    return NS_ERROR_FAILURE;
  }

  nsCAutoString hostName;

  if (NS_SUCCEEDED(uri->GetHost(hostName))) {
    CopyUTF8toUTF16(hostName, aDomain);
  } else {
    // If we can't get the host from the URI (e.g. about:, javascript:,
    // etc), just return an null string.
    SetDOMStringToNull(aDomain);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetDomain(const nsAString& aDomain)
{
  if (aDomain.IsEmpty())
    return NS_ERROR_DOM_BAD_DOCUMENT_DOMAIN;

  // Create new URI
  nsCOMPtr<nsIURI> uri;
  GetDomainURI(getter_AddRefs(uri));

  if (!uri) {
    return NS_ERROR_FAILURE;
  }

  nsCAutoString newURIString;
  if (NS_FAILED(uri->GetScheme(newURIString)))
    return NS_ERROR_FAILURE;
  nsCAutoString path;
  if (NS_FAILED(uri->GetPath(path)))
    return NS_ERROR_FAILURE;
  newURIString.AppendLiteral("://");
  AppendUTF16toUTF8(aDomain, newURIString);
  newURIString.Append(path);

  nsCOMPtr<nsIURI> newURI;
  if (NS_FAILED(NS_NewURI(getter_AddRefs(newURI), newURIString)))
    return NS_ERROR_FAILURE;

  // Check new domain - must be a superdomain of the current host
  // For example, a page from foo.bar.com may set domain to bar.com,
  // but not to ar.com, baz.com, or fi.foo.bar.com.
  nsCAutoString current, domain;
  if (NS_FAILED(uri->GetAsciiHost(current)))
    current.Truncate();
  if (NS_FAILED(newURI->GetAsciiHost(domain)))
    domain.Truncate();

  PRBool ok = current.Equals(domain);
  if (current.Length() > domain.Length() &&
      StringEndsWith(current, domain) &&
      current.CharAt(current.Length() - domain.Length() - 1) == '.') {
    // We're golden if the new domain is the current page's base domain or a
    // subdomain of it.
    nsCOMPtr<nsIEffectiveTLDService> tldService =
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
    if (!tldService)
      return NS_ERROR_NOT_AVAILABLE;

    nsCAutoString currentBaseDomain;
    ok = NS_SUCCEEDED(tldService->GetBaseDomain(uri, 0, currentBaseDomain));
    NS_ASSERTION(StringEndsWith(domain, currentBaseDomain) ==
                 (domain.Length() >= currentBaseDomain.Length()),
                 "uh-oh!  slight optimization wasn't valid somehow!");
    ok = ok && domain.Length() >= currentBaseDomain.Length();
  }
  if (!ok) {
    // Error: illegal domain
    return NS_ERROR_DOM_BAD_DOCUMENT_DOMAIN;
  }

  return NodePrincipal()->SetDomain(newURI);
}

NS_IMETHODIMP
nsHTMLDocument::GetURL(nsAString& aURL)
{
  nsCAutoString str;

  if (mDocumentURI) {
    mDocumentURI->GetSpec(str);
  }

  CopyUTF8toUTF16(str, aURL);

  return NS_OK;
}

nsIContent*
nsHTMLDocument::GetBody(nsresult *aResult)
{
  Element* body = GetBodyElement();

  *aResult = NS_OK;

  if (body) {
    // There is a body element, return that as the body.
    return body;
  }

  // The document is most likely a frameset document so look for the
  // outer most frameset element
  nsRefPtr<nsContentList> nodeList =
    NS_GetContentList(this, kNameSpaceID_XHTML, NS_LITERAL_STRING("frameset"));

  return nodeList->GetNodeAt(0);
}

NS_IMETHODIMP
nsHTMLDocument::GetBody(nsIDOMHTMLElement** aBody)
{
  *aBody = nsnull;

  nsresult rv;
  nsIContent *body = GetBody(&rv);

  return body ? CallQueryInterface(body, aBody) : rv;
}

NS_IMETHODIMP
nsHTMLDocument::SetBody(nsIDOMHTMLElement* aBody)
{
  nsCOMPtr<nsIContent> newBody = do_QueryInterface(aBody);
  Element* root = GetRootElement();

  // The body element must be either a body tag or a frameset tag. And we must
  // have a html root tag, otherwise GetBody will not return the newly set
  // body.
  if (!newBody || !(newBody->Tag() == nsGkAtoms::body ||
                    newBody->Tag() == nsGkAtoms::frameset) ||
      !root || !root->IsHTML() ||
      root->Tag() != nsGkAtoms::html) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  nsCOMPtr<nsIDOMElement> rootElem = do_QueryInterface(root);
  nsCOMPtr<nsIDOMNode> tmp;

  // Use DOM methods so that we pass through the appropriate security checks.
  nsCOMPtr<nsIDOMNode> currentBody = do_QueryInterface(GetBodyElement());
  if (currentBody) {
    return rootElem->ReplaceChild(aBody, currentBody, getter_AddRefs(tmp));
  }

  return rootElem->AppendChild(aBody, getter_AddRefs(tmp));
}

NS_IMETHODIMP
nsHTMLDocument::GetHead(nsIDOMHTMLHeadElement** aHead)
{
  *aHead = nsnull;

  Element* head = GetHeadElement();

  return head ? CallQueryInterface(head, aHead) : NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetImages(nsIDOMHTMLCollection** aImages)
{
  if (!mImages) {
    mImages = new nsContentList(this, kNameSpaceID_XHTML, nsGkAtoms::img, nsGkAtoms::img);
  }

  *aImages = mImages;
  NS_ADDREF(*aImages);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetApplets(nsIDOMHTMLCollection** aApplets)
{
  if (!mApplets) {
    mApplets = new nsContentList(this, kNameSpaceID_XHTML, nsGkAtoms::applet, nsGkAtoms::applet);
  }

  *aApplets = mApplets;
  NS_ADDREF(*aApplets);

  return NS_OK;
}

PRBool
nsHTMLDocument::MatchLinks(nsIContent *aContent, PRInt32 aNamespaceID,
                           nsIAtom* aAtom, void* aData)
{
  nsIDocument* doc = aContent->GetCurrentDoc();

  if (doc) {
    NS_ASSERTION(aContent->IsInDoc(),
                 "This method should never be called on content nodes that "
                 "are not in a document!");
#ifdef DEBUG
    {
      nsCOMPtr<nsIHTMLDocument> htmldoc =
        do_QueryInterface(aContent->GetCurrentDoc());
      NS_ASSERTION(htmldoc,
                   "Huh, how did this happen? This should only be used with "
                   "HTML documents!");
    }
#endif

    nsINodeInfo *ni = aContent->NodeInfo();

    nsIAtom *localName = ni->NameAtom();
    if (ni->NamespaceID() == kNameSpaceID_XHTML &&
        (localName == nsGkAtoms::a || localName == nsGkAtoms::area)) {
      return aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::href);
    }
  }

  return PR_FALSE;
}

NS_IMETHODIMP
nsHTMLDocument::GetLinks(nsIDOMHTMLCollection** aLinks)
{
  if (!mLinks) {
    mLinks = new nsContentList(this, MatchLinks, nsnull, nsnull);
  }

  *aLinks = mLinks;
  NS_ADDREF(*aLinks);

  return NS_OK;
}

PRBool
nsHTMLDocument::MatchAnchors(nsIContent *aContent, PRInt32 aNamespaceID,
                             nsIAtom* aAtom, void* aData)
{
  NS_ASSERTION(aContent->IsInDoc(),
               "This method should never be called on content nodes that "
               "are not in a document!");
#ifdef DEBUG
  {
    nsCOMPtr<nsIHTMLDocument> htmldoc =
      do_QueryInterface(aContent->GetCurrentDoc());
    NS_ASSERTION(htmldoc,
                 "Huh, how did this happen? This should only be used with "
                 "HTML documents!");
  }
#endif

  if (aContent->NodeInfo()->Equals(nsGkAtoms::a, kNameSpaceID_XHTML)) {
    return aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::name);
  }

  return PR_FALSE;
}

NS_IMETHODIMP
nsHTMLDocument::GetAnchors(nsIDOMHTMLCollection** aAnchors)
{
  if (!mAnchors) {
    mAnchors = new nsContentList(this, MatchAnchors, nsnull, nsnull);
  }

  *aAnchors = mAnchors;
  NS_ADDREF(*aAnchors);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetCookie(nsAString& aCookie)
{
  aCookie.Truncate(); // clear current cookie in case service fails;
                      // no cookie isn't an error condition.

  if (mDisableCookieAccess) {
    return NS_OK;
  }

  // not having a cookie service isn't an error
  nsCOMPtr<nsICookieService> service = do_GetService(NS_COOKIESERVICE_CONTRACTID);
  if (service) {
    // Get a URI from the document principal. We use the original
    // codebase in case the codebase was changed by SetDomain
    nsCOMPtr<nsIURI> codebaseURI;
    NodePrincipal()->GetURI(getter_AddRefs(codebaseURI));

    if (!codebaseURI) {
      // Document's principal is not a codebase (may be system), so
      // can't set cookies

      return NS_OK;
    }

    nsXPIDLCString cookie;
    service->GetCookieString(codebaseURI, mChannel, getter_Copies(cookie));
    CopyASCIItoUTF16(cookie, aCookie);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetCookie(const nsAString& aCookie)
{
  if (mDisableCookieAccess) {
    return NS_OK;
  }

  // not having a cookie service isn't an error
  nsCOMPtr<nsICookieService> service = do_GetService(NS_COOKIESERVICE_CONTRACTID);
  if (service && mDocumentURI) {
    nsCOMPtr<nsIPrompt> prompt;
    nsCOMPtr<nsPIDOMWindow> window = GetWindow();
    if (window) {
      window->GetPrompter(getter_AddRefs(prompt));
    }

    nsCOMPtr<nsIURI> codebaseURI;
    NodePrincipal()->GetURI(getter_AddRefs(codebaseURI));

    if (!codebaseURI) {
      // Document's principal is not a codebase (may be system), so
      // can't set cookies

      return NS_OK;
    }

    NS_LossyConvertUTF16toASCII cookie(aCookie);
    service->SetCookieString(codebaseURI, prompt, cookie.get(), mChannel);
  }

  return NS_OK;
}

// XXX TBI: accepting arguments to the open method.
nsresult
nsHTMLDocument::OpenCommon(const nsACString& aContentType, PRBool aReplace)
{
  if (!IsHTML() || mDisableDocWrite) {
    // No calling document.open() on XHTML

    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  PRBool loadAsHtml5 = nsHtml5Module::sEnabled;

  nsresult rv = NS_OK;

  // If we already have a parser we ignore the document.open call.
  if (mParser) {

    return NS_OK;
  }

  NS_ASSERTION(nsContentUtils::CanCallerAccess(static_cast<nsIDOMHTMLDocument*>(this)),
               "XOW should have caught this!");

  if (!aContentType.EqualsLiteral("text/html") &&
      !aContentType.EqualsLiteral("text/plain")) {
    NS_WARNING("Unsupported type; fix the caller");
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  // check whether we're in the middle of unload.  If so, ignore this call.
  nsCOMPtr<nsIDocShell> shell = do_QueryReferent(mDocumentContainer);
  if (!shell) {
    // We won't be able to create a parser anyway.
    return NS_OK;
  }

  PRBool inUnload;
  shell->GetIsInUnload(&inUnload);
  if (inUnload) {
    return NS_OK;
  }

  // Note: We want to use GetDocumentFromContext here because this document
  // should inherit the security information of the document that's opening us,
  // (since if it's secure, then it's presumably trusted).
  nsCOMPtr<nsIDocument> callerDoc =
    do_QueryInterface(nsContentUtils::GetDocumentFromContext());
  if (!callerDoc) {
    // If we're called from C++ or in some other way without an originating
    // document we can't do a document.open w/o changing the principal of the
    // document to something like about:blank (as that's the only sane thing to
    // do when we don't know the origin of this call), and since we can't
    // change the principals of a document for security reasons we'll have to
    // refuse to go ahead with this call.

    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // Grab a reference to the calling documents security info (if any)
  // and URIs as they may be lost in the call to Reset().
  nsCOMPtr<nsISupports> securityInfo = callerDoc->GetSecurityInfo();
  nsCOMPtr<nsIURI> uri = callerDoc->GetDocumentURI();
  nsCOMPtr<nsIURI> baseURI = callerDoc->GetBaseURI();
  nsCOMPtr<nsIPrincipal> callerPrincipal = callerDoc->NodePrincipal();

  // We're called from script. Make sure the script is from the same
  // origin, not just that the caller can access the document. This is
  // needed to keep document principals from ever changing, which is
  // needed because of the way we use our XOW code, and is a sane
  // thing to do anyways.

  PRBool equals = PR_FALSE;
  if (NS_FAILED(callerPrincipal->Equals(NodePrincipal(), &equals)) ||
      !equals) {

#ifdef DEBUG
    nsCOMPtr<nsIURI> callerDocURI = callerDoc->GetDocumentURI();
    nsCOMPtr<nsIURI> thisURI = nsIDocument::GetDocumentURI();
    nsCAutoString callerSpec;
    nsCAutoString thisSpec;
    if (callerDocURI) {
      callerDocURI->GetSpec(callerSpec);
    }
    if (thisURI) {
      thisURI->GetSpec(thisSpec);
    }
    printf("nsHTMLDocument::OpenCommon callerDoc %s this %s\n", callerSpec.get(), thisSpec.get());
#endif

    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // Stop current loads targeted at the window this document is in.
  if (mScriptGlobalObject) {
    nsCOMPtr<nsIContentViewer> cv;
    shell->GetContentViewer(getter_AddRefs(cv));

    if (cv) {
      PRBool okToUnload;
      rv = cv->PermitUnload(PR_FALSE, &okToUnload);

      if (NS_SUCCEEDED(rv) && !okToUnload) {
        // We don't want to unload, so stop here, but don't throw an
        // exception.
        return NS_OK;
      }
    }

    nsCOMPtr<nsIWebNavigation> webnav(do_QueryInterface(shell));
    webnav->Stop(nsIWebNavigation::STOP_NETWORK);

    // The Stop call may have cancelled the onload blocker request or prevented
    // it from getting added, so we need to make sure it gets added to the
    // document again otherwise the document could have a non-zero onload block
    // count without the onload blocker request being in the loadgroup.
    EnsureOnloadBlocker();
  }

  // The open occurred after the document finished loading.
  // So we reset the document and create a new one.
  nsCOMPtr<nsIChannel> channel;
  nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);

  rv = NS_NewChannel(getter_AddRefs(channel), uri, nsnull, group);

  if (NS_FAILED(rv)) {
    return rv;
  }

  // We can't depend on channels implementing property bags, so do our
  // base URI manually after reset.

  // Set the caller principal, if any, on the channel so that we'll
  // make sure to use it when we reset.
  rv = channel->SetOwner(callerPrincipal);
  NS_ENSURE_SUCCESS(rv, rv);

  // Before we reset the doc notify the globalwindow of the change,
  // but only if we still have a window (i.e. our window object the
  // current inner window in our outer window).

  // Hold onto ourselves on the offchance that we're down to one ref
  nsCOMPtr<nsIDOMDocument> kungFuDeathGrip =
    do_QueryInterface((nsIHTMLDocument*)this);

  nsPIDOMWindow *window = GetInnerWindow();
  if (window) {
    // Remember the old scope in case the call to SetNewDocument changes it.
    nsCOMPtr<nsIScriptGlobalObject> oldScope(do_QueryReferent(mScopeObject));

#ifdef DEBUG
    PRBool willReparent = mWillReparent;
    mWillReparent = PR_TRUE;
#endif

    // Should this pass PR_TRUE for aForceReuseInnerWindow?
    rv = window->SetNewDocument(this, nsnull, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
    mWillReparent = willReparent;
#endif

    // Now make sure we're not flagged as the initial document anymore, now
    // that we've had stuff done to us.  From now on, if anyone tries to
    // document.open() us, they get a new inner window.
    SetIsInitialDocument(PR_FALSE);

    nsCOMPtr<nsIScriptGlobalObject> newScope(do_QueryReferent(mScopeObject));
    if (oldScope && newScope != oldScope) {
      nsContentUtils::ReparentContentWrappersInScope(oldScope, newScope);
    }
  }

  // Call Reset(), this will now do the full reset
  Reset(channel, group);
  if (baseURI) {
    mDocumentBaseURI = baseURI;
  }

  // Store the security info of the caller now that we're done
  // resetting the document.
  mSecurityInfo = securityInfo;

  if (loadAsHtml5) {
    mParser = nsHtml5Module::NewHtml5Parser();
    rv = NS_OK;
  } else {
    mParser = do_CreateInstance(kCParserCID, &rv);  
  }

  // This will be propagated to the parser when someone actually calls write()
  SetContentTypeInternal(aContentType);

  mWriteState = eDocumentOpened;

  if (NS_SUCCEEDED(rv)) {
    if (loadAsHtml5) {
      nsHtml5Module::Initialize(mParser, this, uri, shell, channel);
    } else {
      nsCOMPtr<nsIHTMLContentSink> sink;

      rv = NS_NewHTMLContentSink(getter_AddRefs(sink), this, uri, shell,
                                 channel);
      if (NS_FAILED(rv)) {
        // Don't use a parser without a content sink.
        mParser = nsnull;
        mWriteState = eNotWriting;
        return rv;
      }

      mParser->SetContentSink(sink);
    }
  }

  // Prepare the docshell and the document viewer for the impending
  // out of band document.write()
  shell->PrepareForNewContentModel();

  // Now check whether we were opened with a "replace" argument.  If
  // so, we need to tell the docshell to not create a new history
  // entry for this load. Otherwise, make sure that we're doing a normal load,
  // not whatever type of load was previously done on this docshell.
  shell->SetLoadType(aReplace ? LOAD_NORMAL_REPLACE : LOAD_NORMAL);

  nsCOMPtr<nsIContentViewer> cv;
  shell->GetContentViewer(getter_AddRefs(cv));
  nsCOMPtr<nsIDocumentViewer> docViewer = do_QueryInterface(cv);
  if (docViewer) {
    docViewer->LoadStart(static_cast<nsIHTMLDocument *>(this));
  }

  // Add a wyciwyg channel request into the document load group
  NS_ASSERTION(!mWyciwygChannel, "nsHTMLDocument::OpenCommon(): wyciwyg "
               "channel already exists!");

  // In case the editor is listening and will see the new channel
  // being added, make sure mWriteLevel is non-zero so that the editor
  // knows that document.open/write/close() is being called on this
  // document.
  ++mWriteLevel;

  CreateAndAddWyciwygChannel();

  --mWriteLevel;

  return rv;
}

NS_IMETHODIMP
nsHTMLDocument::Open()
{
  nsCOMPtr<nsIDOMDocument> doc;
  return Open(NS_LITERAL_CSTRING("text/html"), PR_FALSE, getter_AddRefs(doc));
}

NS_IMETHODIMP
nsHTMLDocument::Open(const nsACString& aContentType, PRBool aReplace,
                     nsIDOMDocument** aReturn)
{
  nsresult rv = OpenCommon(aContentType, aReplace);
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(this, aReturn);
}

NS_IMETHODIMP
nsHTMLDocument::Clear()
{
  // This method has been deprecated
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::Close()
{
  if (!IsHTML()) {
    // No calling document.close() on XHTML!

    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsresult rv = NS_OK;

  if (mParser && mWriteState == eDocumentOpened) {
    mPendingScripts.RemoveElement(GenerateParserKey());

    mWriteState = mPendingScripts.IsEmpty() ? eDocumentClosed : ePendingClose;

    ++mWriteLevel;
    rv = mParser->Parse(EmptyString(), mParser->GetRootContextKey(),
                        GetContentTypeInternal(), PR_TRUE);
    --mWriteLevel;

    // XXX Make sure that all the document.written content is
    // reflowed.  We should remove this call once we change
    // nsHTMLDocument::OpenCommon() so that it completely destroys the
    // earlier document's content and frame hierarchy.  Right now, it
    // re-uses the earlier document's root content object and
    // corresponding frame objects.  These re-used frame objects think
    // that they have already been reflowed, so they drop initial
    // reflows.  For certain cases of document.written content, like a
    // frameset document, the dropping of the initial reflow means
    // that we end up in document.close() without appended any reflow
    // commands to the reflow queue and, consequently, without adding
    // the dummy layout request to the load group.  Since the dummy
    // layout request is not added to the load group, the onload
    // handler of the frameset fires before the frames get reflowed
    // and loaded.  That is the long explanation for why we need this
    // one line of code here!
    // XXXbz as far as I can tell this may not be needed anymore; all
    // the testcases in bug 57636 pass without this line...  Leaving
    // it be for now, though.  In any case, there's no reason to do
    // this if we have no presshell, since in that case none of the
    // above about reusing frames applies.
    if (GetShell()) {
      FlushPendingNotifications(Flush_Layout);
    }

    // Remove the wyciwyg channel request from the document load group
    // that we added in OpenCommon().  If all other requests between
    // document.open() and document.close() have completed, then this
    // method should cause the firing of an onload event.
    NS_ASSERTION(mWyciwygChannel, "nsHTMLDocument::Close(): Trying to remove "
                 "nonexistent wyciwyg channel!");
    RemoveWyciwygChannel();
    NS_ASSERTION(!mWyciwygChannel, "nsHTMLDocument::Close(): "
                 "nsIWyciwygChannel could not be removed!");
  }

  return NS_OK;
}

nsresult
nsHTMLDocument::WriteCommon(const nsAString& aText,
                            PRBool aNewlineTerminate)
{
  mTooDeepWriteRecursion =
    (mWriteLevel > NS_MAX_DOCUMENT_WRITE_DEPTH || mTooDeepWriteRecursion);
  NS_ENSURE_STATE(!mTooDeepWriteRecursion);

  if (!IsHTML() || mDisableDocWrite) {
    // No calling document.write*() on XHTML!

    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsresult rv = NS_OK;

  void *key = GenerateParserKey();
  if (mWriteState == eDocumentClosed ||
      (mWriteState == ePendingClose &&
       !mPendingScripts.Contains(key)) ||
      (mParser && !mParser->IsInsertionPointDefined())) {
    if (mExternalScriptsBeingEvaluated) {
      // Instead of implying a call to document.open(), ignore the call.
      nsContentUtils::ReportToConsole(nsContentUtils::eDOM_PROPERTIES,
                                      "DocumentWriteIgnored",
                                      nsnull, 0,
                                      mDocumentURI,
                                      EmptyString(), 0, 0,
                                      nsIScriptError::warningFlag,
                                      "DOM Events", this);
      return NS_OK;
    }
    mWriteState = eDocumentClosed;
    mParser->Terminate();
    NS_ASSERTION(!mParser, "mParser should have been null'd out");
  }

  if (!mParser) {
    if (mExternalScriptsBeingEvaluated) {
      // Instead of implying a call to document.open(), ignore the call.
      nsContentUtils::ReportToConsole(nsContentUtils::eDOM_PROPERTIES,
                                      "DocumentWriteIgnored",
                                      nsnull, 0,
                                      mDocumentURI,
                                      EmptyString(), 0, 0,
                                      nsIScriptError::warningFlag,
                                      "DOM Events", this);
      return NS_OK;
    }
    rv = Open();

    // If Open() fails, or if it didn't create a parser (as it won't
    // if the user chose to not discard the current document through
    // onbeforeunload), don't write anything.
    if (NS_FAILED(rv) || !mParser) {
      return rv;
    }
  }

  static NS_NAMED_LITERAL_STRING(new_line, "\n");

  // Save the data in cache
  if (mWyciwygChannel) {
    if (!aText.IsEmpty()) {
      mWyciwygChannel->WriteToCacheEntry(aText);
    }

    if (aNewlineTerminate) {
      mWyciwygChannel->WriteToCacheEntry(new_line);
    }
  }

  ++mWriteLevel;

  // This could be done with less code, but for performance reasons it
  // makes sense to have the code for two separate Parse() calls here
  // since the concatenation of strings costs more than we like. And
  // why pay that price when we don't need to?
  if (aNewlineTerminate) {
    rv = mParser->Parse(aText + new_line,
                        key, GetContentTypeInternal(),
                        (mWriteState == eNotWriting || (mWriteLevel > 1)));
  } else {
    rv = mParser->Parse(aText,
                        key, GetContentTypeInternal(),
                        (mWriteState == eNotWriting || (mWriteLevel > 1)));
  }

  --mWriteLevel;

  mTooDeepWriteRecursion = (mWriteLevel != 0 && mTooDeepWriteRecursion);

  return rv;
}

NS_IMETHODIMP
nsHTMLDocument::Write(const nsAString& aText)
{
  return WriteCommon(aText, PR_FALSE);
}

NS_IMETHODIMP
nsHTMLDocument::Writeln(const nsAString& aText)
{
  return WriteCommon(aText, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLDocument::ImportNode(nsIDOMNode* aImportedNode,
                           PRBool aDeep,
                           nsIDOMNode** aReturn)
{
  return nsDocument::ImportNode(aImportedNode, aDeep, aReturn);
}

NS_IMETHODIMP
nsHTMLDocument::CreateAttributeNS(const nsAString& aNamespaceURI,
                                  const nsAString& aQualifiedName,
                                  nsIDOMAttr** aReturn)
{
  return nsDocument::CreateAttributeNS(aNamespaceURI, aQualifiedName, aReturn);
}

NS_IMETHODIMP
nsHTMLDocument::GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                                       const nsAString& aLocalName,
                                       nsIDOMNodeList** aReturn)
{
  return nsDocument::GetElementsByTagNameNS(aNamespaceURI, aLocalName, aReturn);
}

NS_IMETHODIMP
nsHTMLDocument::GetElementById(const nsAString& aElementId,
                               nsIDOMElement** aReturn)
{
  return nsDocument::GetElementById(aElementId, aReturn);
}

PRBool
nsHTMLDocument::MatchNameAttribute(nsIContent* aContent, PRInt32 aNamespaceID,
                                   nsIAtom* aAtom, void* aData)
{
  NS_PRECONDITION(aContent, "Must have content node to work with!");
  nsString* elementName = static_cast<nsString*>(aData);
  return
    aContent->GetNameSpaceID() == kNameSpaceID_XHTML &&
    aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                          *elementName, eCaseMatters);
}

/* static */
void*
nsHTMLDocument::UseExistingNameString(nsINode* aRootNode, const nsString* aName)
{
  return const_cast<nsString*>(aName);
}

NS_IMETHODIMP
nsHTMLDocument::GetElementsByName(const nsAString& aElementName,
                                  nsIDOMNodeList** aReturn)
{
  nsRefPtr<nsContentList> list = GetElementsByName(aElementName);
  NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);

  // Transfer ownership
  list.forget(aReturn);

  return NS_OK;
}

void
nsHTMLDocument::ScriptLoading(nsIScriptElement *aScript)
{
  if (mWriteState == eNotWriting) {
    return;
  }

  mPendingScripts.AppendElement(aScript);
}

void
nsHTMLDocument::ScriptExecuted(nsIScriptElement *aScript)
{
  if (mWriteState == eNotWriting) {
    return;
  }

  mPendingScripts.RemoveElement(aScript);
  if (mPendingScripts.IsEmpty() && mWriteState == ePendingClose) {
    // The last pending script just finished, terminate our parser now.
    mWriteState = eDocumentClosed;
  }
}

void
nsHTMLDocument::AddedForm()
{
  ++mNumForms;
}

void
nsHTMLDocument::RemovedForm()
{
  --mNumForms;
}

PRInt32
nsHTMLDocument::GetNumFormsSynchronous()
{
  return mNumForms;
}

nsresult
nsHTMLDocument::GetBodySize(PRInt32* aWidth,
                            PRInt32* aHeight)
{
  *aWidth = *aHeight = 0;

  FlushPendingNotifications(Flush_Layout);

  // Find the <body> element: this is what we'll want to use for the
  // document's width and height values.
  Element* body = GetBodyElement();
  if (!body) {
    return NS_OK;
  }

  // Now grab its frame
  nsIFrame* frame = body->GetPrimaryFrame();
  if (!frame)
    return NS_OK;
  
  nsSize size = frame->GetSize();

  *aWidth = nsPresContext::AppUnitsToIntCSSPixels(size.width);
  *aHeight = nsPresContext::AppUnitsToIntCSSPixels(size.height);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetWidth(PRInt32* aWidth)
{
  NS_ENSURE_ARG_POINTER(aWidth);
  if (!mWarnedWidthHeight) {
    ReportUseOfDeprecatedMethod(this, "UseOfDocumentWidthWarning");
    mWarnedWidthHeight = true;
  }
  PRInt32 height;
  return GetBodySize(aWidth, &height);
}

NS_IMETHODIMP
nsHTMLDocument::GetHeight(PRInt32* aHeight)
{
  NS_ENSURE_ARG_POINTER(aHeight);
  if (!mWarnedWidthHeight) {
    ReportUseOfDeprecatedMethod(this, "UseOfDocumentHeightWarning");
    mWarnedWidthHeight = true;
  }
  PRInt32 width;
  return GetBodySize(&width, aHeight);
}

NS_IMETHODIMP
nsHTMLDocument::GetAlinkColor(nsAString& aAlinkColor)
{
  aAlinkColor.Truncate();

  nsCOMPtr<nsIDOMHTMLBodyElement> body = do_QueryInterface(GetBodyElement());
  if (body) {
    body->GetALink(aAlinkColor);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetAlinkColor(const nsAString& aAlinkColor)
{
  nsCOMPtr<nsIDOMHTMLBodyElement> body = do_QueryInterface(GetBodyElement());
  if (body) {
    body->SetALink(aAlinkColor);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetLinkColor(nsAString& aLinkColor)
{
  aLinkColor.Truncate();

  nsCOMPtr<nsIDOMHTMLBodyElement> body = do_QueryInterface(GetBodyElement());
  if (body) {
    body->GetLink(aLinkColor);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetLinkColor(const nsAString& aLinkColor)
{
  nsCOMPtr<nsIDOMHTMLBodyElement> body = do_QueryInterface(GetBodyElement());
  if (body) {
    body->SetLink(aLinkColor);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetVlinkColor(nsAString& aVlinkColor)
{
  aVlinkColor.Truncate();

  nsCOMPtr<nsIDOMHTMLBodyElement> body = do_QueryInterface(GetBodyElement());
  if (body) {
    body->GetVLink(aVlinkColor);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetVlinkColor(const nsAString& aVlinkColor)
{
  nsCOMPtr<nsIDOMHTMLBodyElement> body = do_QueryInterface(GetBodyElement());
  if (body) {
    body->SetVLink(aVlinkColor);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetBgColor(nsAString& aBgColor)
{
  aBgColor.Truncate();

  nsCOMPtr<nsIDOMHTMLBodyElement> body = do_QueryInterface(GetBodyElement());
  if (body) {
    body->GetBgColor(aBgColor);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetBgColor(const nsAString& aBgColor)
{
  nsCOMPtr<nsIDOMHTMLBodyElement> body = do_QueryInterface(GetBodyElement());
  if (body) {
    body->SetBgColor(aBgColor);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetFgColor(nsAString& aFgColor)
{
  aFgColor.Truncate();

  nsCOMPtr<nsIDOMHTMLBodyElement> body = do_QueryInterface(GetBodyElement());
  if (body) {
    body->GetText(aFgColor);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetFgColor(const nsAString& aFgColor)
{
  nsCOMPtr<nsIDOMHTMLBodyElement> body = do_QueryInterface(GetBodyElement());
  if (body) {
    body->SetText(aFgColor);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLDocument::GetEmbeds(nsIDOMHTMLCollection** aEmbeds)
{
  if (!mEmbeds) {
    mEmbeds = new nsContentList(this, kNameSpaceID_XHTML, nsGkAtoms::embed, nsGkAtoms::embed);
  }

  *aEmbeds = mEmbeds;
  NS_ADDREF(*aEmbeds);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetSelection(nsAString& aReturn)
{
  aReturn.Truncate();

  nsCOMPtr<nsIJSContextStack> stack = do_GetService("@mozilla.org/js/xpc/ContextStack;1");
  JSContext* ccx = nsnull;
  if (stack && NS_SUCCEEDED(stack->Peek(&ccx)) && ccx) {
    JS_ReportWarning(ccx, "Deprecated method document.getSelection() called.  Please use window.getSelection() instead.");
  }

  nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(GetScopeObject());
  nsCOMPtr<nsPIDOMWindow> pwin = do_QueryInterface(window);
  NS_ENSURE_TRUE(pwin, NS_OK);
  NS_ASSERTION(pwin->IsInnerWindow(), "Should have inner window here!");
  NS_ENSURE_TRUE(pwin->GetOuterWindow() &&
                 pwin->GetOuterWindow()->GetCurrentInnerWindow() == pwin,
                 NS_OK);

  nsCOMPtr<nsISelection> selection;
  nsresult rv = window->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_TRUE(selection && NS_SUCCEEDED(rv), rv);

  nsXPIDLString str;

  rv = selection->ToString(getter_Copies(str));

  aReturn.Assign(str);

  return rv;
}

NS_IMETHODIMP
nsHTMLDocument::CaptureEvents(PRInt32 aEventFlags)
{
  ReportUseOfDeprecatedMethod(this, "UseOfCaptureEventsWarning");
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::ReleaseEvents(PRInt32 aEventFlags)
{
  ReportUseOfDeprecatedMethod(this, "UseOfReleaseEventsWarning");
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::RouteEvent(nsIDOMEvent* aEvt)
{
  ReportUseOfDeprecatedMethod(this, "UseOfRouteEventWarning");
  return NS_OK;
}

// readonly attribute DOMString compatMode;
// Returns "BackCompat" if we are in quirks mode, "CSS1Compat" if we are
// in almost standards or full standards mode. See bug 105640.  This was
// implemented to match MSIE's compatMode property
NS_IMETHODIMP
nsHTMLDocument::GetCompatMode(nsAString& aCompatMode)
{
  NS_ASSERTION(mCompatMode == eCompatibility_NavQuirks ||
               mCompatMode == eCompatibility_AlmostStandards ||
               mCompatMode == eCompatibility_FullStandards,
               "mCompatMode is neither quirks nor strict for this document");

  if (mCompatMode == eCompatibility_NavQuirks) {
    aCompatMode.AssignLiteral("BackCompat");
  } else {
    aCompatMode.AssignLiteral("CSS1Compat");
  }

  return NS_OK;
}

// Mapped to document.embeds for NS4 compatibility
NS_IMETHODIMP
nsHTMLDocument::GetPlugins(nsIDOMHTMLCollection** aPlugins)
{
  *aPlugins = nsnull;

  return GetEmbeds(aPlugins);
}

static void
FindNamedItems(nsIAtom* aName, nsIContent *aContent,
               nsIdentifierMapEntry* aEntry)
{
  NS_ASSERTION(aEntry->HasNameContentList(),
               "Entry w/o content list passed to FindNamedItems()!");
  NS_ASSERTION(!aEntry->IsInvalidName(),
               "Entry that should never have a list passed to FindNamedItems()!");

  if (aContent->HasFlag(NODE_HAS_NAME)) {
    NS_ASSERTION(nsGenericHTMLElement::FromContent(aContent),
                 "Only HTML Elements should have a name");
  
    nsGenericHTMLElement* elm = static_cast<nsGenericHTMLElement*>(aContent);
    if (elm->GetParsedAttr(nsGkAtoms::name)->GetAtomValue() == aName) {
      aEntry->AddNameElement(elm);
    }
  }

  for (nsINode::ChildIterator iter(aContent); !iter.IsDone(); iter.Next()) {
    FindNamedItems(aName, iter, aEntry);
  }
}

nsresult
nsHTMLDocument::ResolveName(const nsAString& aName,
                            nsIDOMHTMLFormElement *aForm,
                            nsISupports **aResult,
                            nsWrapperCache **aCache)
{
  *aResult = nsnull;
  *aCache = nsnull;

  // We have built a table and cache the named items. The table will
  // be updated as content is added and removed.
  nsIdentifierMapEntry *entry = mIdentifierMap.PutEntry(aName);
  NS_ENSURE_TRUE(entry, NS_ERROR_OUT_OF_MEMORY);

  if (entry->IsInvalidName()) {
    // There won't be any named items by this name -- it's reserved
    return NS_OK;
  }

  // Now we know we _might_ have items.

  if (!entry->HasNameContentList()) {
#ifdef DEBUG_jst
    {
      printf ("nsHTMLDocument name cache miss for name '%s'\n",
              NS_ConvertUTF16toUTF8(aName).get());
    }
#endif

    nsresult rv = entry->CreateNameContentList();
    if (NS_FAILED(rv))
      return rv;

    Element* root = GetRootElement();
    if (root && !aName.IsEmpty()) {
      // do_GetAtom() can fail on OOM, but it'll only do that if the
      // atom doesn't already exist, which means the named item
      // doesn't exist either.
      nsCOMPtr<nsIAtom> name(do_GetAtom(aName));

      if (name) {
        FindNamedItems(name, root, entry);
      }
    }
  }

  nsBaseContentList *list = entry->GetNameContentList();

  PRUint32 length;
  list->GetLength(&length);

  if (length > 0) {
    if (length == 1) {
      // Only one element in the list, return the element instead of
      // returning the list

      nsIContent *node = list->GetNodeAt(0);
      if (!aForm || nsContentUtils::BelongsInForm(aForm, node)) {
        NS_ADDREF(*aResult = node);
        *aCache = node;
      }

      return NS_OK;
    }

    // The list contains more than one element, return the whole
    // list, unless...

    if (aForm) {
      // ... we're called from a form, in that case we create a
      // nsFormContentList which will filter out the elements in the
      // list that don't belong to aForm

      nsFormContentList *fc_list = new nsFormContentList(aForm, *list);
      NS_ENSURE_TRUE(fc_list, NS_ERROR_OUT_OF_MEMORY);

      PRUint32 len;
      fc_list->GetLength(&len);

      if (len < 2) {
        // After the nsFormContentList is done filtering there's either
        // nothing or one element in the list.  Return that element, or null
        // if there's no element in the list.

        nsIContent *node = fc_list->GetNodeAt(0);

        NS_IF_ADDREF(*aResult = node);
        *aCache = node;

        delete fc_list;

        return NS_OK;
      }

      list = fc_list;
    }

    return CallQueryInterface(list, aResult);
  }

  // No named items were found, see if there's one registerd by id for
  // aName. If we get this far, FindNamedItems() will have been called
  // for aName, so we're guaranteed that if there is an element with
  // the id aName, it'll be entry's IdContent.

  Element *e = entry->GetIdElement();

  if (e && e->IsHTML()) {
    nsIAtom *tag = e->Tag();

    if ((tag == nsGkAtoms::embed  ||
         tag == nsGkAtoms::img    ||
         tag == nsGkAtoms::object ||
         tag == nsGkAtoms::applet) &&
        (!aForm || nsContentUtils::BelongsInForm(aForm, e))) {
      NS_ADDREF(*aResult = e);
      *aCache = e;
    }
  }

  return NS_OK;
}

// Pre-fill the name hash with names that are likely to be resolved in
// this document to avoid walking the tree looking for elements with
// these names.

nsresult
nsHTMLDocument::PrePopulateIdentifierMap()
{
  static const char names[][13] = {
    "write", "writeln", "open", "close", "forms", "elements",
    "characterSet", "nodeType", "parentNode", "cookie"
  };

  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(names); ++i) {
    nsCOMPtr<nsIAtom> atom(do_GetAtom(names[i]));
    NS_ENSURE_TRUE(atom, NS_ERROR_OUT_OF_MEMORY);
  
    nsIdentifierMapEntry* entry =
      mIdentifierMap.PutEntry(nsDependentAtomString(atom));
    NS_ENSURE_TRUE(entry, NS_ERROR_OUT_OF_MEMORY);

    entry->SetInvalidName();
  }

  return NS_OK;
}

//----------------------------

/* virtual */ nsIContent*
nsHTMLDocument::GetBodyContentExternal()
{
  return GetBodyElement();
}

// forms related stuff

NS_IMETHODIMP
nsHTMLDocument::GetForms(nsIDOMHTMLCollection** aForms)
{
  nsContentList *forms = nsHTMLDocument::GetForms();
  if (!forms)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aForms = forms);
  return NS_OK;
}

nsContentList*
nsHTMLDocument::GetForms()
{
  if (!mForms) {
    mForms = new nsContentList(this, kNameSpaceID_XHTML, nsGkAtoms::form, nsGkAtoms::form);
  }

  return mForms;
}

static PRBool MatchFormControls(nsIContent* aContent, PRInt32 aNamespaceID,
                                nsIAtom* aAtom, void* aData)
{
  return aContent->IsNodeOfType(nsIContent::eHTML_FORM_CONTROL);
}

nsContentList*
nsHTMLDocument::GetFormControls()
{
  if (!mFormControls) {
    mFormControls = new nsContentList(this, MatchFormControls, nsnull, nsnull);
  }

  return mFormControls;
}

nsresult
nsHTMLDocument::CreateAndAddWyciwygChannel(void)
{
  nsresult rv = NS_OK;
  nsCAutoString url, originalSpec;

  mDocumentURI->GetSpec(originalSpec);

  // Generate the wyciwyg url
  url = NS_LITERAL_CSTRING("wyciwyg://")
      + nsPrintfCString("%d", gWyciwygSessionCnt++)
      + NS_LITERAL_CSTRING("/")
      + originalSpec;

  nsCOMPtr<nsIURI> wcwgURI;
  NS_NewURI(getter_AddRefs(wcwgURI), url);

  // Create the nsIWyciwygChannel to store out-of-band
  // document.write() script to cache
  nsCOMPtr<nsIChannel> channel;
  // Create a wyciwyg Channel
  rv = NS_NewChannel(getter_AddRefs(channel), wcwgURI);
  NS_ENSURE_SUCCESS(rv, rv);

  mWyciwygChannel = do_QueryInterface(channel);

  mWyciwygChannel->SetSecurityInfo(mSecurityInfo);

  // Note: we want to treat this like a "previous document" hint so that,
  // e.g. a <meta> tag in the document.write content can override it.
  SetDocumentCharacterSetSource(kCharsetFromHintPrevDoc);
  mWyciwygChannel->SetCharsetAndSource(kCharsetFromHintPrevDoc,
                                       GetDocumentCharacterSet());

  // Use our new principal
  channel->SetOwner(NodePrincipal());

  // Inherit load flags from the original document's channel
  channel->SetLoadFlags(mLoadFlags);

  nsCOMPtr<nsILoadGroup> loadGroup = GetDocumentLoadGroup();

  // Use the Parent document's loadgroup to trigger load notifications
  if (loadGroup && channel) {
    rv = channel->SetLoadGroup(loadGroup);
    NS_ENSURE_SUCCESS(rv, rv);

    nsLoadFlags loadFlags = 0;
    channel->GetLoadFlags(&loadFlags);
    loadFlags |= nsIChannel::LOAD_DOCUMENT_URI;
    channel->SetLoadFlags(loadFlags);

    channel->SetOriginalURI(wcwgURI);

    rv = loadGroup->AddRequest(mWyciwygChannel, nsnull);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to add request to load group.");
  }

  return rv;
}

nsresult
nsHTMLDocument::RemoveWyciwygChannel(void)
{
  nsCOMPtr<nsILoadGroup> loadGroup = GetDocumentLoadGroup();

  // note there can be a write request without a load group if
  // this is a synchronously constructed about:blank document
  if (loadGroup && mWyciwygChannel) {
    mWyciwygChannel->CloseCacheEntry(NS_OK);
    loadGroup->RemoveRequest(mWyciwygChannel, nsnull, NS_OK);
  }

  mWyciwygChannel = nsnull;

  return NS_OK;
}

void *
nsHTMLDocument::GenerateParserKey(void)
{
  if (!mScriptLoader) {
    // If we don't have a script loader, then the parser probably isn't parsing
    // anything anyway, so just return null.
    return nsnull;
  }

  // The script loader provides us with the currently executing script element,
  // which is guaranteed to be unique per script.
  if (nsHtml5Module::sEnabled) {
    nsIScriptElement* script = mScriptLoader->GetCurrentParserInsertedScript();
    if (script && mParser && mParser->IsScriptCreated()) {
      nsCOMPtr<nsIParser> creatorParser = script->GetCreatorParser();
      if (creatorParser != mParser) {
        // Make scripts that aren't inserted by the active parser of this document
        // participate in the context of the script that document.open()ed 
        // this document.
        return mParser->GetRootContextKey();
      }
    }
    return script;
  } else {
    return mScriptLoader->GetCurrentScript();
  }
}

/* attribute DOMString designMode; */
NS_IMETHODIMP
nsHTMLDocument::GetDesignMode(nsAString & aDesignMode)
{
  if (HasFlag(NODE_IS_EDITABLE)) {
    aDesignMode.AssignLiteral("on");
  }
  else {
    aDesignMode.AssignLiteral("off");
  }
  return NS_OK;
}

void
nsHTMLDocument::MaybeEditingStateChanged()
{
  if (mUpdateNestLevel == 0 && (mContentEditableCount > 0) != IsEditingOn()) {
    if (nsContentUtils::IsSafeToRunScript()) {
      EditingStateChanged();
    } else if (!mInDestructor) {
      nsContentUtils::AddScriptRunner(
        NS_NewRunnableMethod(this, &nsHTMLDocument::MaybeEditingStateChanged));
    }
  }
}

void
nsHTMLDocument::EndUpdate(nsUpdateType aUpdateType)
{
  nsDocument::EndUpdate(aUpdateType);

  MaybeEditingStateChanged();
}


// Helper class, used below in ChangeContentEditableCount().
class DeferredContentEditableCountChangeEvent : public nsRunnable
{
public:
  DeferredContentEditableCountChangeEvent(nsHTMLDocument *aDoc,
                                          nsIContent *aElement)
    : mDoc(aDoc)
    , mElement(aElement)
  {
  }

  NS_IMETHOD Run() {
    if (mElement && mElement->GetOwnerDoc() == mDoc) {
      mDoc->DeferredContentEditableCountChange(mElement);
    }
    return NS_OK;
  }

private:
  nsRefPtr<nsHTMLDocument> mDoc;
  nsCOMPtr<nsIContent> mElement;
};

nsresult
nsHTMLDocument::ChangeContentEditableCount(nsIContent *aElement,
                                           PRInt32 aChange)
{
  NS_ASSERTION(mContentEditableCount + aChange >= 0,
               "Trying to decrement too much.");

  mContentEditableCount += aChange;

  nsContentUtils::AddScriptRunner(
    new DeferredContentEditableCountChangeEvent(this, aElement));

  return NS_OK;
}

void
nsHTMLDocument::DeferredContentEditableCountChange(nsIContent *aElement)
{
  if (mParser ||
      (mUpdateNestLevel > 0 && (mContentEditableCount > 0) != IsEditingOn())) {
    return;
  }

  EditingState oldState = mEditingState;

  nsresult rv = EditingStateChanged();
  NS_ENSURE_SUCCESS(rv, );

  if (oldState == mEditingState && mEditingState == eContentEditable) {
    // We just changed the contentEditable state of a node, we need to reset
    // the spellchecking state of that node.
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aElement);
    if (node) {
      nsPIDOMWindow *window = GetWindow();
      if (!window)
        return;

      nsIDocShell *docshell = window->GetDocShell();
      if (!docshell)
        return;

      nsCOMPtr<nsIEditorDocShell> editorDocShell =
        do_QueryInterface(docshell, &rv);
      NS_ENSURE_SUCCESS(rv, );

      nsCOMPtr<nsIEditor> editor;
      editorDocShell->GetEditor(getter_AddRefs(editor));
      if (editor) {
        nsCOMPtr<nsIDOMRange> range;
        rv = NS_NewRange(getter_AddRefs(range));
        NS_ENSURE_SUCCESS(rv, );

        rv = range->SelectNode(node);
        NS_ENSURE_SUCCESS(rv, );

        nsCOMPtr<nsIInlineSpellChecker> spellChecker;
        rv = editor->GetInlineSpellChecker(PR_FALSE,
                                           getter_AddRefs(spellChecker));
        NS_ENSURE_SUCCESS(rv, );

        if (spellChecker) {
          rv = spellChecker->SpellCheckRange(range);
        }
      }
    }
  }
}

static PRBool
DocAllResultMatch(nsIContent* aContent, PRInt32 aNamespaceID, nsIAtom* aAtom,
                  void* aData)
{
  if (aContent->GetID() == aAtom) {
    return PR_TRUE;
  }

  nsGenericHTMLElement* elm = nsGenericHTMLElement::FromContent(aContent);
  if (!elm) {
    return PR_FALSE;
  }

  nsIAtom* tag = elm->Tag();
  if (tag != nsGkAtoms::a &&
      tag != nsGkAtoms::applet &&
      tag != nsGkAtoms::button &&
      tag != nsGkAtoms::embed &&
      tag != nsGkAtoms::form &&
      tag != nsGkAtoms::iframe &&
      tag != nsGkAtoms::img &&
      tag != nsGkAtoms::input &&
      tag != nsGkAtoms::map &&
      tag != nsGkAtoms::meta &&
      tag != nsGkAtoms::object &&
      tag != nsGkAtoms::select &&
      tag != nsGkAtoms::textarea) {
    return PR_FALSE;
  }

  const nsAttrValue* val = elm->GetParsedAttr(nsGkAtoms::name);
  return val && val->Type() == nsAttrValue::eAtom &&
         val->GetAtomValue() == aAtom;
}


nsISupports*
nsHTMLDocument::GetDocumentAllResult(const nsAString& aID,
                                     nsWrapperCache** aCache,
                                     nsresult *aResult)
{
  *aCache = nsnull;
  *aResult = NS_OK;

  nsIdentifierMapEntry *entry = mIdentifierMap.PutEntry(aID);
  if (!entry) {
    *aResult = NS_ERROR_OUT_OF_MEMORY;

    return nsnull;
  }

  Element* root = GetRootElement();
  if (!root) {
    return nsnull;
  }

  nsRefPtr<nsContentList> docAllList = entry->GetDocAllList();
  if (!docAllList) {
    nsCOMPtr<nsIAtom> id = do_GetAtom(aID);

    docAllList = new nsContentList(root, DocAllResultMatch,
                                   nsnull, nsnull, PR_TRUE, id);
    entry->SetDocAllList(docAllList);
  }

  // Check if there are more than 1 entries. Do this by getting the second one
  // rather than the length since getting the length always requires walking
  // the entire document.

  nsIContent* cont = docAllList->Item(1, PR_TRUE);
  if (cont) {
    *aCache = docAllList;
    return static_cast<nsINodeList*>(docAllList);
  }

  // There's only 0 or 1 items. Return the first one or null.
  *aCache = cont = docAllList->Item(0, PR_TRUE);

  return cont;
}

static void
NotifyEditableStateChange(nsINode *aNode, nsIDocument *aDocument,
                          PRBool aEditable)
{
  PRUint32 i, n = aNode->GetChildCount();
  for (i = 0; i < n; ++i) {
    nsIContent *child = aNode->GetChildAt(i);
    if (child->HasFlag(NODE_IS_EDITABLE) != aEditable) {
      aDocument->ContentStatesChanged(child, nsnull,
                                      NS_EVENT_STATE_MOZ_READONLY |
                                      NS_EVENT_STATE_MOZ_READWRITE);
    }
    NotifyEditableStateChange(child, aDocument, aEditable);
  }
}

void
nsHTMLDocument::TearingDownEditor(nsIEditor *aEditor)
{
  if (IsEditingOn()) {
    EditingState oldState = mEditingState;
    mEditingState = eTearingDown;

    nsCOMPtr<nsIPresShell> presShell = GetShell();
    if (!presShell)
      return;

    nsCOMArray<nsIStyleSheet> agentSheets;
    presShell->GetAgentStyleSheets(agentSheets);

    RemoveFromAgentSheets(agentSheets, NS_LITERAL_STRING("resource://gre/res/contenteditable.css"));
    if (oldState == eDesignMode)
      RemoveFromAgentSheets(agentSheets, NS_LITERAL_STRING("resource://gre/res/designmode.css"));

    presShell->SetAgentStyleSheets(agentSheets);

    presShell->ReconstructStyleData();
  }
}

nsresult
nsHTMLDocument::TurnEditingOff()
{
  NS_ASSERTION(mEditingState != eOff, "Editing is already off.");

  nsPIDOMWindow *window = GetWindow();
  if (!window)
    return NS_ERROR_FAILURE;

  nsIDocShell *docshell = window->GetDocShell();
  if (!docshell)
    return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIEditingSession> editSession = do_GetInterface(docshell, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // turn editing off
  rv = editSession->TearDownEditorOnWindow(window);
  NS_ENSURE_SUCCESS(rv, rv);

  mEditingState = eOff;

  return NS_OK;
}

static PRBool HasPresShell(nsPIDOMWindow *aWindow)
{
  nsIDocShell *docShell = aWindow->GetDocShell();
  if (!docShell)
    return PR_FALSE;
  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));
  return presShell != nsnull;
}

nsresult
nsHTMLDocument::SetEditingState(EditingState aState)
{
  mEditingState = aState;
  return NS_OK;
}

nsresult
nsHTMLDocument::EditingStateChanged()
{
  if (mRemovedFromDocShell) {
    return NS_OK;
  }

  if (mEditingState == eSettingUp || mEditingState == eTearingDown) {
    // XXX We shouldn't recurse.
    return NS_OK;
  }

  PRBool designMode = HasFlag(NODE_IS_EDITABLE);
  EditingState newState = designMode ? eDesignMode :
                          (mContentEditableCount > 0 ? eContentEditable : eOff);
  if (mEditingState == newState) {
    // No changes in editing mode.
    return NS_OK;
  }

  if (newState == eOff) {
    // Editing is being turned off.
    return TurnEditingOff();
  }

  // Flush out style changes on our _parent_ document, if any, so that
  // our check for a presshell won't get stale information.
  if (mParentDocument) {
    mParentDocument->FlushPendingNotifications(Flush_Style);
  }

  // get editing session, make sure this is a strong reference so the
  // window can't get deleted during the rest of this call.
  nsCOMPtr<nsPIDOMWindow> window = GetWindow();
  if (!window)
    return NS_ERROR_FAILURE;

  nsIDocShell *docshell = window->GetDocShell();
  if (!docshell)
    return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIEditingSession> editSession = do_GetInterface(docshell, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIEditor> existingEditor;
  editSession->GetEditorForWindow(window, getter_AddRefs(existingEditor));
  if (existingEditor) {
    // We might already have an editor if it was set up for mail, let's see
    // if this is actually the case.
    nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(existingEditor);
    NS_ABORT_IF_FALSE(htmlEditor, "If we have an editor, it must be an HTML editor");
    PRUint32 flags = 0;
    existingEditor->GetFlags(&flags);
    if (flags & nsIPlaintextEditor::eEditorMailMask) {
      // We already have a mail editor, then we should not attempt to create
      // another one.
      return NS_OK;
    }
  }

  if (!HasPresShell(window)) {
    // We should not make the window editable or setup its editor.
    // It's probably style=display:none.
    return NS_OK;
  }

  PRBool makeWindowEditable = mEditingState == eOff;
  PRBool updateState;
  PRBool spellRecheckAll = PR_FALSE;
  nsCOMPtr<nsIEditor> editor;

  {
    EditingState oldState = mEditingState;
    nsAutoEditingState push(this, eSettingUp);

    if (makeWindowEditable) {
      // Editing is being turned on (through designMode or contentEditable)
      // Turn on editor.
      // XXX This can cause flushing which can change the editing state, so make
      //     sure to avoid recursing.
      rv = editSession->MakeWindowEditable(window, "html", PR_FALSE, PR_FALSE,
                                           PR_TRUE);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // XXX Need to call TearDownEditorOnWindow for all failures.
    nsCOMPtr<nsIEditorDocShell> editorDocShell =
      do_QueryInterface(docshell, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    editorDocShell->GetEditor(getter_AddRefs(editor));
    if (!editor)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIPresShell> presShell = GetShell();
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

    // If we're entering the design mode, put the selection at the beginning of
    // the document for compatibility reasons.
    if (designMode && oldState == eOff) {
      editor->BeginningOfDocument();
    }

    nsCOMArray<nsIStyleSheet> agentSheets;
    rv = presShell->GetAgentStyleSheets(agentSheets);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), NS_LITERAL_STRING("resource://gre/res/contenteditable.css"));
    NS_ENSURE_SUCCESS(rv, rv);

    nsRefPtr<nsCSSStyleSheet> sheet;
    rv = LoadChromeSheetSync(uri, PR_TRUE, getter_AddRefs(sheet));
    NS_ENSURE_TRUE(sheet, rv);

    rv = agentSheets.AppendObject(sheet);
    NS_ENSURE_SUCCESS(rv, rv);

    // Should we update the editable state of all the nodes in the document? We
    // need to do this when the designMode value changes, as that overrides
    // specific states on the elements.
    if (designMode) {
      // designMode is being turned on (overrides contentEditable).
      rv = NS_NewURI(getter_AddRefs(uri), NS_LITERAL_STRING("resource://gre/res/designmode.css"));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = LoadChromeSheetSync(uri, PR_TRUE, getter_AddRefs(sheet));
      NS_ENSURE_TRUE(sheet, rv);

      rv = agentSheets.AppendObject(sheet);
      NS_ENSURE_SUCCESS(rv, rv);

      // Disable scripting and plugins.
      rv = editSession->DisableJSAndPlugins(window);
      NS_ENSURE_SUCCESS(rv, rv);

      updateState = PR_TRUE;
      spellRecheckAll = oldState == eContentEditable;
    }
    else if (oldState == eDesignMode) {
      // designMode is being turned off (contentEditable is still on).
      RemoveFromAgentSheets(agentSheets, NS_LITERAL_STRING("resource://gre/res/designmode.css"));

      rv = editSession->RestoreJSAndPlugins(window);
      NS_ENSURE_SUCCESS(rv, rv);

      updateState = PR_TRUE;
    }
    else {
      // contentEditable is being turned on (and designMode is off).
      updateState = PR_FALSE;
    }

    rv = presShell->SetAgentStyleSheets(agentSheets);
    NS_ENSURE_SUCCESS(rv, rv);

    presShell->ReconstructStyleData();
  }

  mEditingState = newState;

  if (makeWindowEditable) {
    // Set the editor to not insert br's on return when in p
    // elements by default.
    // XXX Do we only want to do this for designMode?
    PRBool unused;
    rv = ExecCommand(NS_LITERAL_STRING("insertBrOnReturn"), PR_FALSE,
                     NS_LITERAL_STRING("false"), &unused);

    if (NS_FAILED(rv)) {
      // Editor setup failed. Editing is not on after all.
      // XXX Should we reset the editable flag on nodes?
      editSession->TearDownEditorOnWindow(window);
      mEditingState = eOff;

      return rv;
    }
  }

  if (updateState) {
    mozAutoDocUpdate upd(this, UPDATE_CONTENT_STATE, PR_TRUE);
    NotifyEditableStateChange(this, this, !designMode);
  }

  // Resync the editor's spellcheck state.
  if (spellRecheckAll) {
    nsCOMPtr<nsISelectionController> selcon;
    nsresult rv = editor->GetSelectionController(getter_AddRefs(selcon));
    NS_ENSURE_SUCCESS(rv, rv); 

    nsCOMPtr<nsISelection> spellCheckSelection;
    rv = selcon->GetSelection(nsISelectionController::SELECTION_SPELLCHECK,
                              getter_AddRefs(spellCheckSelection));
    if (NS_SUCCEEDED(rv)) {
      spellCheckSelection->RemoveAllRanges();
    }
  }
  editor->SyncRealTimeSpell();

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetDesignMode(const nsAString & aDesignMode)
{
  nsresult rv = NS_OK;

  if (!nsContentUtils::IsCallerTrustedForWrite()) {
    nsCOMPtr<nsIPrincipal> subject;
    nsIScriptSecurityManager *secMan = nsContentUtils::GetSecurityManager();
    rv = secMan->GetSubjectPrincipal(getter_AddRefs(subject));
    NS_ENSURE_SUCCESS(rv, rv);
    if (subject) {
      PRBool subsumes;
      rv = subject->Subsumes(NodePrincipal(), &subsumes);
      NS_ENSURE_SUCCESS(rv, rv);

      NS_ENSURE_TRUE(subsumes, NS_ERROR_DOM_PROP_ACCESS_DENIED);
    }
  }

  PRBool editableMode = HasFlag(NODE_IS_EDITABLE);
  if (aDesignMode.LowerCaseEqualsASCII(editableMode ? "off" : "on")) {
    SetEditableFlag(!editableMode);

    return EditingStateChanged();
  }

  return NS_OK;
}

nsresult
nsHTMLDocument::GetMidasCommandManager(nsICommandManager** aCmdMgr)
{
  // initialize return value
  NS_ENSURE_ARG_POINTER(aCmdMgr);

  // check if we have it cached
  if (mMidasCommandManager) {
    NS_ADDREF(*aCmdMgr = mMidasCommandManager);
    return NS_OK;
  }

  *aCmdMgr = nsnull;

  nsPIDOMWindow *window = GetWindow();
  if (!window)
    return NS_ERROR_FAILURE;

  nsIDocShell *docshell = window->GetDocShell();
  if (!docshell)
    return NS_ERROR_FAILURE;

  mMidasCommandManager = do_GetInterface(docshell);
  if (!mMidasCommandManager)
    return NS_ERROR_FAILURE;

  NS_ADDREF(*aCmdMgr = mMidasCommandManager);

  return NS_OK;
}


struct MidasCommand {
  const char*  incomingCommandString;
  const char*  internalCommandString;
  const char*  internalParamString;
  PRPackedBool useNewParam;
  PRPackedBool convertToBoolean;
};

static const struct MidasCommand gMidasCommandTable[] = {
  { "bold",          "cmd_bold",            "", PR_TRUE,  PR_FALSE },
  { "italic",        "cmd_italic",          "", PR_TRUE,  PR_FALSE },
  { "underline",     "cmd_underline",       "", PR_TRUE,  PR_FALSE },
  { "strikethrough", "cmd_strikethrough",   "", PR_TRUE,  PR_FALSE },
  { "subscript",     "cmd_subscript",       "", PR_TRUE,  PR_FALSE },
  { "superscript",   "cmd_superscript",     "", PR_TRUE,  PR_FALSE },
  { "cut",           "cmd_cut",             "", PR_TRUE,  PR_FALSE },
  { "copy",          "cmd_copy",            "", PR_TRUE,  PR_FALSE },
  { "paste",         "cmd_paste",           "", PR_TRUE,  PR_FALSE },
  { "delete",        "cmd_delete",          "", PR_TRUE,  PR_FALSE },
  { "selectall",     "cmd_selectAll",       "", PR_TRUE,  PR_FALSE },
  { "undo",          "cmd_undo",            "", PR_TRUE,  PR_FALSE },
  { "redo",          "cmd_redo",            "", PR_TRUE,  PR_FALSE },
  { "indent",        "cmd_indent",          "", PR_TRUE,  PR_FALSE },
  { "outdent",       "cmd_outdent",         "", PR_TRUE,  PR_FALSE },
  { "backcolor",     "cmd_backgroundColor", "", PR_FALSE, PR_FALSE },
  { "forecolor",     "cmd_fontColor",       "", PR_FALSE, PR_FALSE },
  { "hilitecolor",   "cmd_highlight",       "", PR_FALSE, PR_FALSE },
  { "fontname",      "cmd_fontFace",        "", PR_FALSE, PR_FALSE },
  { "fontsize",      "cmd_fontSize",        "", PR_FALSE, PR_FALSE },
  { "increasefontsize", "cmd_increaseFont", "", PR_FALSE, PR_FALSE },
  { "decreasefontsize", "cmd_decreaseFont", "", PR_FALSE, PR_FALSE },
  { "inserthorizontalrule", "cmd_insertHR", "", PR_TRUE,  PR_FALSE },
  { "createlink",    "cmd_insertLinkNoUI",  "", PR_FALSE, PR_FALSE },
  { "insertimage",   "cmd_insertImageNoUI", "", PR_FALSE, PR_FALSE },
  { "inserthtml",    "cmd_insertHTML",      "", PR_FALSE, PR_FALSE },
  { "gethtml",       "cmd_getContents",     "", PR_FALSE, PR_FALSE },
  { "justifyleft",   "cmd_align",       "left", PR_TRUE,  PR_FALSE },
  { "justifyright",  "cmd_align",      "right", PR_TRUE,  PR_FALSE },
  { "justifycenter", "cmd_align",     "center", PR_TRUE,  PR_FALSE },
  { "justifyfull",   "cmd_align",    "justify", PR_TRUE,  PR_FALSE },
  { "removeformat",  "cmd_removeStyles",    "", PR_TRUE,  PR_FALSE },
  { "unlink",        "cmd_removeLinks",     "", PR_TRUE,  PR_FALSE },
  { "insertorderedlist",   "cmd_ol",        "", PR_TRUE,  PR_FALSE },
  { "insertunorderedlist", "cmd_ul",        "", PR_TRUE,  PR_FALSE },
  { "insertparagraph", "cmd_paragraphState", "p", PR_TRUE, PR_FALSE },
  { "formatblock",   "cmd_paragraphState",  "", PR_FALSE, PR_FALSE },
  { "heading",       "cmd_paragraphState",  "", PR_FALSE, PR_FALSE },
  { "styleWithCSS",  "cmd_setDocumentUseCSS", "", PR_FALSE, PR_TRUE },
  { "contentReadOnly", "cmd_setDocumentReadOnly", "", PR_FALSE, PR_TRUE },
  { "insertBrOnReturn", "cmd_insertBrOnReturn", "", PR_FALSE, PR_TRUE },
  { "enableObjectResizing", "cmd_enableObjectResizing", "", PR_FALSE, PR_TRUE },
  { "enableInlineTableEditing", "cmd_enableInlineTableEditing", "", PR_FALSE, PR_TRUE },
#if 0
// no editor support to remove alignments right now
  { "justifynone",   "cmd_align",           "", PR_TRUE,  PR_FALSE },

// the following will need special review before being turned on
  { "saveas",        "cmd_saveAs",          "", PR_TRUE,  PR_FALSE },
  { "print",         "cmd_print",           "", PR_TRUE,  PR_FALSE },
#endif
  { NULL, NULL, NULL, PR_FALSE, PR_FALSE }
};

#define MidasCommandCount ((sizeof(gMidasCommandTable) / sizeof(struct MidasCommand)) - 1)

static const char* const gBlocks[] = {
  "ADDRESS",
  "BLOCKQUOTE",
  "DD",
  "DIV",
  "DL",
  "DT",
  "H1",
  "H2",
  "H3",
  "H4",
  "H5",
  "H6",
  "P",
  "PRE"
};

static PRBool
ConvertToMidasInternalCommandInner(const nsAString & inCommandID,
                                   const nsAString & inParam,
                                   nsACString& outCommandID,
                                   nsACString& outParam,
                                   PRBool& outIsBoolean,
                                   PRBool& outBooleanValue,
                                   PRBool aIgnoreParams)
{
  NS_ConvertUTF16toUTF8 convertedCommandID(inCommandID);

  // Hack to support old boolean commands that were backwards (see bug 301490).
  PRBool invertBool = PR_FALSE;
  if (convertedCommandID.LowerCaseEqualsLiteral("usecss")) {
    convertedCommandID.Assign("styleWithCSS");
    invertBool = PR_TRUE;
  }
  else if (convertedCommandID.LowerCaseEqualsLiteral("readonly")) {
    convertedCommandID.Assign("contentReadOnly");
    invertBool = PR_TRUE;
  }

  PRUint32 i;
  PRBool found = PR_FALSE;
  for (i = 0; i < MidasCommandCount; ++i) {
    if (convertedCommandID.Equals(gMidasCommandTable[i].incomingCommandString,
                                  nsCaseInsensitiveCStringComparator())) {
      found = PR_TRUE;
      break;
    }
  }

  if (found) {
    // set outCommandID (what we use internally)
    outCommandID.Assign(gMidasCommandTable[i].internalCommandString);

    // set outParam & outIsBoolean based on flags from the table
    outIsBoolean = gMidasCommandTable[i].convertToBoolean;

    if (!aIgnoreParams) {
      if (gMidasCommandTable[i].useNewParam) {
        outParam.Assign(gMidasCommandTable[i].internalParamString);
      }
      else {
        // handle checking of param passed in
        if (outIsBoolean) {
          // if this is a boolean value and it's not explicitly false
          // (e.g. no value) we default to "true". For old backwards commands
          // we invert the check (see bug 301490).
          if (invertBool) {
            outBooleanValue = inParam.LowerCaseEqualsLiteral("false");
          }
          else {
            outBooleanValue = !inParam.LowerCaseEqualsLiteral("false");
          }
          outParam.Truncate();
        }
        else {
          // check to see if we need to convert the parameter
          if (outCommandID.EqualsLiteral("cmd_paragraphState")) {
            const PRUnichar *start = inParam.BeginReading();
            const PRUnichar *end = inParam.EndReading();
            if (start != end && *start == '<' && *(end - 1) == '>') {
              ++start;
              --end;
            }

            NS_ConvertUTF16toUTF8 convertedParam(Substring(start, end));
            PRUint32 j;
            for (j = 0; j < NS_ARRAY_LENGTH(gBlocks); ++j) {
              if (convertedParam.Equals(gBlocks[j],
                                        nsCaseInsensitiveCStringComparator())) {
                outParam.Assign(gBlocks[j]);
                break;
              }
            }

            return j != NS_ARRAY_LENGTH(gBlocks);
          }
          else {
            CopyUTF16toUTF8(inParam, outParam);
          }
        }
      }
    }
  } // end else for useNewParam (do convert existing param)
  else {
    // reset results if the command is not found in our table
    outCommandID.SetLength(0);
    outParam.SetLength(0);
    outIsBoolean = PR_FALSE;
  }

  return found;
}

static PRBool
ConvertToMidasInternalCommand(const nsAString & inCommandID,
                              const nsAString & inParam,
                              nsACString& outCommandID,
                              nsACString& outParam,
                              PRBool& outIsBoolean,
                              PRBool& outBooleanValue)
{
  return ConvertToMidasInternalCommandInner(inCommandID, inParam, outCommandID,
                                            outParam, outIsBoolean,
                                            outBooleanValue, PR_FALSE);
}

static PRBool
ConvertToMidasInternalCommand(const nsAString & inCommandID,
                              nsACString& outCommandID)
{
  nsCAutoString dummyCString;
  nsAutoString dummyString;
  PRBool dummyBool;
  return ConvertToMidasInternalCommandInner(inCommandID, dummyString,
                                            outCommandID, dummyCString,
                                            dummyBool, dummyBool, PR_TRUE);
}

jsid
nsHTMLDocument::sCutCopyInternal_id = JSID_VOID;
jsid
nsHTMLDocument::sPasteInternal_id = JSID_VOID;

/* Helper function to check security of clipboard commands. If aPaste is */
/* true, we check paste, else we check cutcopy */
nsresult
nsHTMLDocument::DoClipboardSecurityCheck(PRBool aPaste)
{
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1");

  if (stack) {
    JSContext *cx = nsnull;
    stack->Peek(&cx);
    if (!cx) {
      return NS_OK;
    }

    JSAutoRequest ar(cx);

    NS_NAMED_LITERAL_CSTRING(classNameStr, "Clipboard");

    nsIScriptSecurityManager *secMan = nsContentUtils::GetSecurityManager();

    if (aPaste) {
      if (nsHTMLDocument::sPasteInternal_id == JSID_VOID) {
        nsHTMLDocument::sPasteInternal_id =
          INTERNED_STRING_TO_JSID(::JS_InternString(cx, "paste"));
      }
      rv = secMan->CheckPropertyAccess(cx, nsnull, classNameStr.get(),
                                       nsHTMLDocument::sPasteInternal_id,
                                       nsIXPCSecurityManager::ACCESS_GET_PROPERTY);
    } else {
      if (nsHTMLDocument::sCutCopyInternal_id == JSID_VOID) {
        nsHTMLDocument::sCutCopyInternal_id =
          INTERNED_STRING_TO_JSID(::JS_InternString(cx, "cutcopy"));
      }
      rv = secMan->CheckPropertyAccess(cx, nsnull, classNameStr.get(),
                                       nsHTMLDocument::sCutCopyInternal_id,
                                       nsIXPCSecurityManager::ACCESS_GET_PROPERTY);
    }
  }
  return rv;
}

/* TODO: don't let this call do anything if the page is not done loading */
/* boolean execCommand(in DOMString commandID, in boolean doShowUI,
                                               in DOMString value); */
NS_IMETHODIMP
nsHTMLDocument::ExecCommand(const nsAString & commandID,
                            PRBool doShowUI,
                            const nsAString & value,
                            PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  //  for optional parameters see dom/src/base/nsHistory.cpp: HistoryImpl::Go()
  //  this might add some ugly JS dependencies?

  *_retval = PR_FALSE;

  // if editing is not on, bail
  if (!IsEditingOnAfterFlush())
    return NS_ERROR_FAILURE;

  // if they are requesting UI from us, let's fail since we have no UI
  if (doShowUI)
    return NS_OK;

  nsresult rv = NS_OK;

  if (commandID.LowerCaseEqualsLiteral("gethtml"))
    return NS_ERROR_FAILURE;

  if (commandID.LowerCaseEqualsLiteral("cut") ||
      (commandID.LowerCaseEqualsLiteral("copy"))) {
    rv = DoClipboardSecurityCheck(PR_FALSE);
  } else if (commandID.LowerCaseEqualsLiteral("paste")) {
    rv = DoClipboardSecurityCheck(PR_TRUE);
  }

  if (NS_FAILED(rv))
    return rv;

  // get command manager and dispatch command to our window if it's acceptable
  nsCOMPtr<nsICommandManager> cmdMgr;
  GetMidasCommandManager(getter_AddRefs(cmdMgr));
  if (!cmdMgr)
    return NS_ERROR_FAILURE;

  nsIDOMWindow *window = GetWindow();
  if (!window)
    return NS_ERROR_FAILURE;

  nsCAutoString cmdToDispatch, paramStr;
  PRBool isBool, boolVal;
  if (!ConvertToMidasInternalCommand(commandID, value,
                                     cmdToDispatch, paramStr, isBool, boolVal))
    return NS_OK;

  if (!isBool && paramStr.IsEmpty()) {
    rv = cmdMgr->DoCommand(cmdToDispatch.get(), nsnull, window);
  } else {
    // we have a command that requires a parameter, create params
    nsCOMPtr<nsICommandParams> cmdParams = do_CreateInstance(
                                            NS_COMMAND_PARAMS_CONTRACTID, &rv);
    if (!cmdParams)
      return NS_ERROR_OUT_OF_MEMORY;

    if (isBool)
      rv = cmdParams->SetBooleanValue("state_attribute", boolVal);
    else if (cmdToDispatch.Equals("cmd_fontFace"))
      rv = cmdParams->SetStringValue("state_attribute", value);
    else if (cmdToDispatch.Equals("cmd_insertHTML"))
      rv = cmdParams->SetStringValue("state_data", value);
    else
      rv = cmdParams->SetCStringValue("state_attribute", paramStr.get());
    if (NS_FAILED(rv))
      return rv;
    rv = cmdMgr->DoCommand(cmdToDispatch.get(), cmdParams, window);
  }

  *_retval = NS_SUCCEEDED(rv);

  return rv;
}

/* TODO: don't let this call do anything if the page is not done loading */
/* boolean execCommandShowHelp(in DOMString commandID); */
NS_IMETHODIMP
nsHTMLDocument::ExecCommandShowHelp(const nsAString & commandID,
                                    PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;

  // if editing is not on, bail
  if (!IsEditingOnAfterFlush())
    return NS_ERROR_FAILURE;

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean queryCommandEnabled(in DOMString commandID); */
NS_IMETHODIMP
nsHTMLDocument::QueryCommandEnabled(const nsAString & commandID,
                                    PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;

  // if editing is not on, bail
  if (!IsEditingOnAfterFlush())
    return NS_ERROR_FAILURE;

  // get command manager and dispatch command to our window if it's acceptable
  nsCOMPtr<nsICommandManager> cmdMgr;
  GetMidasCommandManager(getter_AddRefs(cmdMgr));
  if (!cmdMgr)
    return NS_ERROR_FAILURE;

  nsIDOMWindow *window = GetWindow();
  if (!window)
    return NS_ERROR_FAILURE;

  nsCAutoString cmdToDispatch, paramStr;
  if (!ConvertToMidasInternalCommand(commandID, cmdToDispatch))
    return NS_ERROR_NOT_IMPLEMENTED;

  return cmdMgr->IsCommandEnabled(cmdToDispatch.get(), window, _retval);
}

/* boolean queryCommandIndeterm (in DOMString commandID); */
NS_IMETHODIMP
nsHTMLDocument::QueryCommandIndeterm(const nsAString & commandID,
                                     PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;

  // if editing is not on, bail
  if (!IsEditingOnAfterFlush())
    return NS_ERROR_FAILURE;

  // get command manager and dispatch command to our window if it's acceptable
  nsCOMPtr<nsICommandManager> cmdMgr;
  GetMidasCommandManager(getter_AddRefs(cmdMgr));
  if (!cmdMgr)
    return NS_ERROR_FAILURE;

  nsIDOMWindow *window = GetWindow();
  if (!window)
    return NS_ERROR_FAILURE;

  nsCAutoString cmdToDispatch, paramToCheck;
  PRBool dummy;
  if (!ConvertToMidasInternalCommand(commandID, commandID,
                                     cmdToDispatch, paramToCheck, dummy, dummy))
    return NS_ERROR_NOT_IMPLEMENTED;

  nsresult rv;
  nsCOMPtr<nsICommandParams> cmdParams = do_CreateInstance(
                                           NS_COMMAND_PARAMS_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = cmdMgr->GetCommandState(cmdToDispatch.get(), window, cmdParams);
  if (NS_FAILED(rv))
    return rv;

  // if command does not have a state_mixed value, this call fails, so we fail too,
  // which is what is expected
  rv = cmdParams->GetBooleanValue("state_mixed", _retval);
  return rv;
}

/* boolean queryCommandState(in DOMString commandID); */
NS_IMETHODIMP
nsHTMLDocument::QueryCommandState(const nsAString & commandID, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;

  // if editing is not on, bail
  if (!IsEditingOnAfterFlush())
    return NS_ERROR_FAILURE;

  // get command manager and dispatch command to our window if it's acceptable
  nsCOMPtr<nsICommandManager> cmdMgr;
  GetMidasCommandManager(getter_AddRefs(cmdMgr));
  if (!cmdMgr)
    return NS_ERROR_FAILURE;

  nsIDOMWindow *window = GetWindow();
  if (!window)
    return NS_ERROR_FAILURE;

  nsCAutoString cmdToDispatch, paramToCheck;
  PRBool dummy, dummy2;
  if (!ConvertToMidasInternalCommand(commandID, commandID,
                                     cmdToDispatch, paramToCheck, dummy, dummy2))
    return NS_ERROR_NOT_IMPLEMENTED;

  nsresult rv;
  nsCOMPtr<nsICommandParams> cmdParams = do_CreateInstance(
                                           NS_COMMAND_PARAMS_CONTRACTID, &rv);
  if (!cmdParams)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = cmdMgr->GetCommandState(cmdToDispatch.get(), window, cmdParams);
  if (NS_FAILED(rv))
    return rv;

  // handle alignment as a special case (possibly other commands too?)
  // Alignment is special because the external api is individual
  // commands but internally we use cmd_align with different
  // parameters.  When getting the state of this command, we need to
  // return the boolean for this particular alignment rather than the
  // string of 'which alignment is this?'
  if (cmdToDispatch.Equals("cmd_align")) {
    char * actualAlignmentType = nsnull;
    rv = cmdParams->GetCStringValue("state_attribute", &actualAlignmentType);
    if (NS_SUCCEEDED(rv) && actualAlignmentType && actualAlignmentType[0]) {
      *_retval = paramToCheck.Equals(actualAlignmentType);
    }
    if (actualAlignmentType)
      nsMemory::Free(actualAlignmentType);
  }
  else {
    rv = cmdParams->GetBooleanValue("state_all", _retval);
    if (NS_FAILED(rv))
      *_retval = PR_FALSE;
  }

  return rv;
}

/* boolean queryCommandSupported(in DOMString commandID); */
NS_IMETHODIMP
nsHTMLDocument::QueryCommandSupported(const nsAString & commandID,
                                      PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;

  // if editing is not on, bail
  if (!IsEditingOnAfterFlush())
    return NS_ERROR_FAILURE;

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* DOMString queryCommandText(in DOMString commandID); */
NS_IMETHODIMP
nsHTMLDocument::QueryCommandText(const nsAString & commandID,
                                 nsAString & _retval)
{
  _retval.SetLength(0);

  // if editing is not on, bail
  if (!IsEditingOnAfterFlush())
    return NS_ERROR_FAILURE;

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* DOMString queryCommandValue(in DOMString commandID); */
NS_IMETHODIMP
nsHTMLDocument::QueryCommandValue(const nsAString & commandID,
                                  nsAString &_retval)
{
  _retval.SetLength(0);

  // if editing is not on, bail
  if (!IsEditingOnAfterFlush())
    return NS_ERROR_FAILURE;

  // get command manager and dispatch command to our window if it's acceptable
  nsCOMPtr<nsICommandManager> cmdMgr;
  GetMidasCommandManager(getter_AddRefs(cmdMgr));
  if (!cmdMgr)
    return NS_ERROR_FAILURE;

  nsIDOMWindow *window = GetWindow();
  if (!window)
    return NS_ERROR_FAILURE;

  nsCAutoString cmdToDispatch, paramStr;
  if (!ConvertToMidasInternalCommand(commandID, cmdToDispatch))
    return NS_ERROR_NOT_IMPLEMENTED;

  // create params
  nsresult rv;
  nsCOMPtr<nsICommandParams> cmdParams = do_CreateInstance(
                                           NS_COMMAND_PARAMS_CONTRACTID, &rv);
  if (!cmdParams)
    return NS_ERROR_OUT_OF_MEMORY;

  // this is a special command since we are calling "DoCommand rather than
  // GetCommandState like the other commands
  if (cmdToDispatch.Equals("cmd_getContents"))
  {
    rv = cmdParams->SetBooleanValue("selection_only", PR_TRUE);
    if (NS_FAILED(rv)) return rv;
    rv = cmdParams->SetCStringValue("format", "text/html");
    if (NS_FAILED(rv)) return rv;
    rv = cmdMgr->DoCommand(cmdToDispatch.get(), cmdParams, window);
    if (NS_FAILED(rv)) return rv;
    return cmdParams->GetStringValue("result", _retval);
  }

  rv = cmdParams->SetCStringValue("state_attribute", paramStr.get());
  if (NS_FAILED(rv))
    return rv;

  rv = cmdMgr->GetCommandState(cmdToDispatch.get(), window, cmdParams);
  if (NS_FAILED(rv))
    return rv;

  nsXPIDLCString cStringResult;
  rv = cmdParams->GetCStringValue("state_attribute",
                                  getter_Copies(cStringResult));
  CopyUTF8toUTF16(cStringResult, _retval);

  return rv;
}

nsresult
nsHTMLDocument::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
{
  NS_ASSERTION(aNodeInfo->NodeInfoManager() == mNodeInfoManager,
               "Can't import this document into another document!");

  nsRefPtr<nsHTMLDocument> clone = new nsHTMLDocument();
  NS_ENSURE_TRUE(clone, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = CloneDocHelper(clone.get());
  NS_ENSURE_SUCCESS(rv, rv);

  // State from nsHTMLDocument
  clone->mLoadFlags = mLoadFlags;

  return CallQueryInterface(clone.get(), aResult);
}

PRBool
nsHTMLDocument::IsEditingOnAfterFlush()
{
  nsIDocument* doc = GetParentDocument();
  if (doc) {
    // Make sure frames are up to date, since that can affect whether
    // we're editable.
    doc->FlushPendingNotifications(Flush_Frames);
  }

  return IsEditingOn();
}

void
nsHTMLDocument::RemovedFromDocShell()
{
  mEditingState = eOff;
  nsDocument::RemovedFromDocShell();
}
