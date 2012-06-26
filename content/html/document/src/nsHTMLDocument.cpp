/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsPrintfCString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsHTMLDocument.h"
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
#include "nsPIDOMWindow.h"
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
#include "nsJSPrincipals.h"
#include "nsIScriptSecurityManager.h"
#include "nsAttrName.h"
#include "nsNodeUtils.h"

#include "nsNetCID.h"
#include "nsICookieService.h"

#include "nsIServiceManager.h"
#include "nsIConsoleService.h"
#include "nsIComponentManager.h"
#include "nsParserCIID.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMHTMLHeadElement.h"
#include "nsINameSpaceManager.h"
#include "nsGenericHTMLElement.h"
#include "mozilla/css/Loader.h"
#include "nsIHttpChannel.h"
#include "nsIFile.h"
#include "nsEventListenerManager.h"
#include "nsFrameSelection.h"
#include "nsISelectionPrivate.h"//for toStringwithformat code

#include "nsContentUtils.h"
#include "nsJSUtils.h"
#include "nsIDocumentEncoder.h" //for outputting selection
#include "nsICachingChannel.h"
#include "nsIJSContextStack.h"
#include "nsIContentViewer.h"
#include "nsIWyciwygChannel.h"
#include "nsIScriptElement.h"
#include "nsIScriptError.h"
#include "nsIMutableArray.h"
#include "nsArrayUtils.h"
#include "nsIEffectiveTLDService.h"

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
#include "mozilla/Preferences.h"
#include "nsMimeTypes.h"
#include "nsIRequest.h"
#include "nsHtml5TreeOpExecutor.h"
#include "nsHtml5Parser.h"
#include "nsIDOMJSWindow.h"

using namespace mozilla;
using namespace mozilla::dom;

#define NS_MAX_DOCUMENT_WRITE_DEPTH 20

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
static bool ConvertToMidasInternalCommand(const nsAString & inCommandID,
                                            const nsAString & inParam,
                                            nsACString& outCommandID,
                                            nsACString& outParam,
                                            bool& isBoolean,
                                            bool& boolValue);

static bool ConvertToMidasInternalCommand(const nsAString & inCommandID,
                                            nsACString& outCommandID);

// ==================================================================
// =
// ==================================================================
static void
ReportUseOfDeprecatedMethod(nsHTMLDocument* aDoc, const char* aWarning)
{
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  "DOM Events", aDoc,
                                  nsContentUtils::eDOM_PROPERTIES,
                                  aWarning);
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

    bool equals = false;
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

  mIsRegularHTML = true;
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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mScripts)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mForms, nsIDOMNodeList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mFormControls,
                                                       nsIDOMNodeList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mWyciwygChannel)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mMidasCommandManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsHTMLDocument, nsDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mImages)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mApplets)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mEmbeds)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mLinks)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mAnchors)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mScripts)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mForms)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFormControls)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mWyciwygChannel)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mMidasCommandManager)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(nsHTMLDocument, nsDocument)
NS_IMPL_RELEASE_INHERITED(nsHTMLDocument, nsDocument)


DOMCI_NODE_DATA(HTMLDocument, nsHTMLDocument)

// QueryInterface implementation for nsHTMLDocument
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsHTMLDocument)
  NS_DOCUMENT_INTERFACE_TABLE_BEGIN(nsHTMLDocument)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLDocument, nsIHTMLDocument)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLDocument, nsIDOMHTMLDocument)
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

  mImages = nsnull;
  mApplets = nsnull;
  mEmbeds = nsnull;
  mLinks = nsnull;
  mAnchors = nsnull;
  mScripts = nsnull;

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

// The following Try*Charset will return false only if the charset source
// should be considered (ie. aCharsetSource < thisCharsetSource) but we failed
// to get the charset from this source.

bool
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
        return true;

      if(NS_SUCCEEDED(rv)) {
        aCharsetSource = requestCharsetSource;
        aCharset = requestCharset;

        return true;
      }
    }
  }
  return false;
}


bool
nsHTMLDocument::TryUserForcedCharset(nsIMarkupDocumentViewer* aMarkupDV,
                                     nsIDocShell*  aDocShell,
                                     PRInt32& aCharsetSource,
                                     nsACString& aCharset)
{
  nsresult rv = NS_OK;

  if(kCharsetFromUserForced <= aCharsetSource)
    return true;

  nsCAutoString forceCharsetFromDocShell;
  if (aMarkupDV) {
    rv = aMarkupDV->GetForceCharacterSet(forceCharsetFromDocShell);
  }

  if(NS_SUCCEEDED(rv) && !forceCharsetFromDocShell.IsEmpty()) {
    aCharset = forceCharsetFromDocShell;
    //TODO: we should define appropriate constant for force charset
    aCharsetSource = kCharsetFromUserForced;
  } else if (aDocShell) {
    nsCOMPtr<nsIAtom> csAtom;
    aDocShell->GetForcedCharset(getter_AddRefs(csAtom));
    if (csAtom) {
      csAtom->ToUTF8String(aCharset);
      aCharsetSource = kCharsetFromUserForced;
      aDocShell->SetForcedCharset(nsnull);
      return true;
    }
  }

  return false;
}

bool
nsHTMLDocument::TryCacheCharset(nsICachingChannel* aCachingChannel,
                                PRInt32& aCharsetSource,
                                nsACString& aCharset)
{
  nsresult rv;

  if (kCharsetFromCache <= aCharsetSource) {
    return true;
  }

  nsCString cachedCharset;
  rv = aCachingChannel->GetCacheTokenCachedCharset(cachedCharset);
  if (NS_SUCCEEDED(rv) && !cachedCharset.IsEmpty())
  {
    aCharset = cachedCharset;
    aCharsetSource = kCharsetFromCache;

    return true;
  }

  return false;
}

static bool
CheckSameOrigin(nsINode* aNode1, nsINode* aNode2)
{
  NS_PRECONDITION(aNode1, "Null node?");
  NS_PRECONDITION(aNode2, "Null node?");

  bool equal;
  return
    NS_SUCCEEDED(aNode1->NodePrincipal()->
                   Equals(aNode2->NodePrincipal(), &equal)) &&
    equal;
}

bool
nsHTMLDocument::TryParentCharset(nsIDocShell*  aDocShell,
                                 nsIDocument* aParentDocument,
                                 PRInt32& aCharsetSource,
                                 nsACString& aCharset)
{
  if (aDocShell) {
    PRInt32 source;
    nsCOMPtr<nsIAtom> csAtom;
    PRInt32 parentSource;
    aDocShell->GetParentCharsetSource(&parentSource);
    if (kCharsetFromParentForced <= parentSource)
      source = kCharsetFromParentForced;
    else if (kCharsetFromHintPrevDoc == parentSource) {
      // Make sure that's OK
      if (!aParentDocument || !CheckSameOrigin(this, aParentDocument)) {
        return false;
      }
      
      // if parent is posted doc, set this prevent autodections
      // I'm not sure this makes much sense... but whatever.
      source = kCharsetFromHintPrevDoc;
    }
    else if (kCharsetFromCache <= parentSource) {
      // Make sure that's OK
      if (!aParentDocument || !CheckSameOrigin(this, aParentDocument)) {
        return false;
      }

      source = kCharsetFromParentFrame;
    }
    else
      return false;

    if (source < aCharsetSource)
      return true;

    aDocShell->GetParentCharset(getter_AddRefs(csAtom));
    if (csAtom) {
      csAtom->ToUTF8String(aCharset);
      aCharsetSource = source;
      return true;
    }
  }
  return false;
}

bool
nsHTMLDocument::UseWeakDocTypeDefault(PRInt32& aCharsetSource,
                                      nsACString& aCharset)
{
  if (kCharsetFromWeakDocTypeDefault <= aCharsetSource)
    return true;
  // fallback value in case docshell return error
  aCharset.AssignLiteral("ISO-8859-1");

  const nsAdoptingCString& defCharset =
    Preferences::GetLocalizedCString("intl.charset.default");

  if (!defCharset.IsEmpty()) {
    aCharset = defCharset;
    aCharsetSource = kCharsetFromWeakDocTypeDefault;
  }
  return true;
}

bool
nsHTMLDocument::TryDefaultCharset( nsIMarkupDocumentViewer* aMarkupDV,
                                   PRInt32& aCharsetSource,
                                   nsACString& aCharset)
{
  if(kCharsetFromUserDefault <= aCharsetSource)
    return true;

  nsCAutoString defaultCharsetFromDocShell;
  if (aMarkupDV) {
    nsresult rv =
      aMarkupDV->GetDefaultCharacterSet(defaultCharsetFromDocShell);
    if(NS_SUCCEEDED(rv)) {
      aCharset = defaultCharsetFromDocShell;

      aCharsetSource = kCharsetFromUserDefault;
      return true;
    }
  }
  return false;
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
                                  bool aReset,
                                  nsIContentSink* aSink)
{
  if (!aCommand) {
    MOZ_ASSERT(false, "Command is mandatory");
    return NS_ERROR_INVALID_POINTER;
  }
  if (aSink) {
    MOZ_ASSERT(false, "Got a sink override. Should not happen for HTML doc.");
    return NS_ERROR_INVALID_ARG;
  }
  if (!mIsRegularHTML) {
    MOZ_ASSERT(false, "Must not set HTML doc to XHTML mode before load start.");
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsCAutoString contentType;
  aChannel->GetContentType(contentType);

  bool view = !strcmp(aCommand, "view") ||
              !strcmp(aCommand, "external-resource");
  bool viewSource = !strcmp(aCommand, "view-source");
  bool asData = !strcmp(aCommand, kLoadAsData);
  if(!(view || viewSource || asData)) {
    MOZ_ASSERT(false, "Bad parser command");
    return NS_ERROR_INVALID_ARG;
  }

  bool html = contentType.EqualsLiteral(TEXT_HTML);
  bool xhtml = !html && contentType.EqualsLiteral(APPLICATION_XHTML_XML);
  bool plainText = !html && !xhtml && (contentType.EqualsLiteral(TEXT_PLAIN) ||
    contentType.EqualsLiteral(TEXT_CSS) ||
    contentType.EqualsLiteral(APPLICATION_JAVASCRIPT) ||
    contentType.EqualsLiteral(APPLICATION_XJAVASCRIPT) ||
    contentType.EqualsLiteral(TEXT_ECMASCRIPT) ||
    contentType.EqualsLiteral(APPLICATION_ECMASCRIPT) ||
    contentType.EqualsLiteral(TEXT_JAVASCRIPT) ||
    contentType.EqualsLiteral(APPLICATION_JSON));
  if (!(html || xhtml || plainText || viewSource)) {
    MOZ_ASSERT(false, "Channel with bad content type.");
    return NS_ERROR_INVALID_ARG;
  }

  bool loadAsHtml5 = true;

  if (!viewSource && xhtml) {
      // We're parsing XHTML as XML, remember that.
      mIsRegularHTML = false;
      mCompatMode = eCompatibility_FullStandards;
      loadAsHtml5 = false;
  }
  
  // TODO: Proper about:blank treatment is bug 543435
  if (loadAsHtml5 && view) {
    // mDocumentURI hasn't been set, yet, so get the URI from the channel
    nsCOMPtr<nsIURI> uri;
    aChannel->GetOriginalURI(getter_AddRefs(uri));
    // Adapted from nsDocShell:
    // GetSpec can be expensive for some URIs, so check the scheme first.
    bool isAbout = false;
    if (uri && NS_SUCCEEDED(uri->SchemeIs("about", &isAbout)) && isAbout) {
      nsCAutoString str;
      uri->GetSpec(str);
      if (str.EqualsLiteral("about:blank")) {
        loadAsHtml5 = false;    
      }
    }
  }
  
  CSSLoader()->SetCompatibilityMode(mCompatMode);
  
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

  if (loadAsHtml5) {
    mParser = nsHtml5Module::NewHtml5Parser();
    if (plainText) {
      if (viewSource) {
        mParser->MarkAsNotScriptCreated("view-source-plain");
      } else {
        mParser->MarkAsNotScriptCreated("plain-text");
      }
    } else if (viewSource && !html) {
      mParser->MarkAsNotScriptCreated("view-source-xml");
    } else {
      mParser->MarkAsNotScriptCreated(aCommand);
    }
  } else {
    mParser = do_CreateInstance(kCParserCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRInt32 textType = GET_BIDI_OPTION_TEXTTYPE(GetBidiOptions());

  // Look for the parent document.  Note that at this point we don't have our
  // content viewer set up yet, and therefore do not have a useful
  // mParentDocument.

  // in this block of code, if we get an error result, we return it
  // but if we get a null pointer, that's perfectly legal for parent
  // and parentContentViewer
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aContainer));

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
  bool muCVIsParent = false;
  nsCOMPtr<nsIContentViewer> cv;
  if (docShell) {
    docShell->GetContentViewer(getter_AddRefs(cv));
  }
  if (cv) {
     muCV = do_QueryInterface(cv);
  } else {
    muCV = do_QueryInterface(parentContentViewer);
    if (muCV) {
      muCVIsParent = true;
    }
  }

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
  
  // For error reporting
  nsHtml5TreeOpExecutor* executor = nsnull;
  if (loadAsHtml5) {
    executor = static_cast<nsHtml5TreeOpExecutor*> (mParser->GetContentSink());
  }

  if (!IsHTML() || !docShell) { // no docshell for text/html XHR
    charsetSource = IsHTML() ? kCharsetFromWeakDocTypeDefault
                             : kCharsetFromDocTypeDefault;
    charset.AssignLiteral("UTF-8");
    TryChannelCharset(aChannel, charsetSource, charset, executor);
    parserCharsetSource = charsetSource;
    parserCharset = charset;
  } else {
    NS_ASSERTION(docShell && docShellAsItem, "Unexpected null value");

    charsetSource = kCharsetUninitialized;
    wyciwygChannel = do_QueryInterface(aChannel);

    // The following charset resolving calls has implied knowledge
    // about charset source priority order. Each try will return true
    // if the source is higher or equal to the source as its name
    // describes. Some try call might change charset source to
    // multiple values, like TryHintCharset and TryParentCharset. It
    // should be always safe to try more sources.
    if (!TryUserForcedCharset(muCV, docShell, charsetSource, charset)) {
      TryHintCharset(muCV, charsetSource, charset);
      TryParentCharset(docShell, parentDocument, charsetSource, charset);

      // Don't actually get the charset from the channel if this is a
      // wyciwyg channel; it'll always be UTF-16
      if (!wyciwygChannel &&
          TryChannelCharset(aChannel, charsetSource, charset, executor)) {
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

    bool isPostPage = false;
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

    // ahmed
    // Check if 864 but in Implicit mode !
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
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "cannot SetMetaDataElement");
    rv = NS_OK; // don't propagate error
  }

  // Set the parser as the stream listener for the document loader...
  rv = NS_OK;
  nsCOMPtr<nsIStreamListener> listener = mParser->GetStreamListener();
  listener.forget(aDocListener);

#ifdef DEBUG_charset
  printf(" charset = %s source %d\n",
        charset.get(), charsetSource);
#endif
  mParser->SetDocumentCharset(parserCharset, parserCharsetSource);
  mParser->SetCommand(aCommand);

  if (!IsHTML()) {
    MOZ_ASSERT(!loadAsHtml5);
    nsCOMPtr<nsIXMLContentSink> xmlsink;
    NS_NewXMLContentSink(getter_AddRefs(xmlsink), this, uri,
                         docShell, aChannel);
    mParser->SetContentSink(xmlsink);
  } else {
    if (loadAsHtml5) {
      nsHtml5Module::Initialize(mParser, this, uri, docShell, aChannel);
    } else {
      // about:blank *only*
      nsCOMPtr<nsIHTMLContentSink> htmlsink;
      NS_NewHTMLContentSink(getter_AddRefs(htmlsink), this, uri,
                            docShell, aChannel);
      mParser->SetContentSink(htmlsink);
    }
  }

  // parser the content of the URI
  mParser->Parse(uri, nsnull, (void *)this);

  return rv;
}

void
nsHTMLDocument::StopDocumentLoad()
{
  BlockOnload();

  // Remove the wyciwyg channel request from the document load group
  // that we added in Open() if Open() was called on this doc.
  RemoveWyciwygChannel();
  NS_ASSERTION(!mWyciwygChannel, "nsHTMLDocument::StopDocumentLoad(): "
               "nsIWyciwygChannel could not be removed!");

  nsDocument::StopDocumentLoad();
  UnblockOnload(false);
  return;
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
  bool turnOnEditing =
    mParser && (HasFlag(NODE_IS_EDITABLE) || mContentEditableCount > 0);
  // Note: nsDocument::EndLoad nulls out mParser.
  nsDocument::EndLoad();
  if (turnOnEditing) {
    EditingStateChanged();
  }
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
// nsIDOMHTMLDocument interface implementation
//
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

  bool ok = current.Equals(domain);
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
nsHTMLDocument::GetBody()
{
  Element* body = GetBodyElement();

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

  nsIContent *body = GetBody();

  return body ? CallQueryInterface(body, aBody) : NS_OK;
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

bool
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

  return false;
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

bool
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

  return false;
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
nsHTMLDocument::GetScripts(nsIDOMHTMLCollection** aScripts)
{
  if (!mScripts) {
    mScripts = new nsContentList(this, kNameSpaceID_XHTML, nsGkAtoms::script, nsGkAtoms::script);
  }

  *aScripts = mScripts;
  NS_ADDREF(*aScripts);

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

    // The for getting the URI matches nsNavigator::GetCookieEnabled
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

NS_IMETHODIMP
nsHTMLDocument::Open(const nsAString& aContentTypeOrUrl,
                     const nsAString& aReplaceOrName,
                     const nsAString& aFeatures,
                     JSContext* cx, PRUint8 aOptionalArgCount,
                     nsISupports** aReturn)
{
  NS_ASSERTION(nsContentUtils::CanCallerAccess(static_cast<nsIDOMHTMLDocument*>(this)),
               "XOW should have caught this!");

  // When called with 3 or more arguments, document.open() calls window.open().
  if (aOptionalArgCount > 2) {
    nsCOMPtr<nsIDOMWindow> window = GetWindowInternal();
    if (!window) {
      return NS_OK;
    }
    nsCOMPtr<nsIDOMJSWindow> win = do_QueryInterface(window);
    nsCOMPtr<nsIDOMWindow> newWindow;
    nsresult rv = win->OpenJS(aContentTypeOrUrl, aReplaceOrName, aFeatures,
                              getter_AddRefs(newWindow));
    *aReturn = newWindow.forget().get();
    return rv;
  }

  if (!IsHTML() || mDisableDocWrite) {
    // No calling document.open() on XHTML
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsCAutoString contentType;
  contentType.AssignLiteral("text/html");
  if (aOptionalArgCount > 0) {
    nsAutoString type;
    nsContentUtils::ASCIIToLower(aContentTypeOrUrl, type);
    nsCAutoString actualType, dummy;
    NS_ParseContentType(NS_ConvertUTF16toUTF8(type), actualType, dummy);
    if (!actualType.EqualsLiteral("text/html") &&
        !type.EqualsLiteral("replace")) {
      contentType.AssignLiteral("text/plain");
    }
  }

  // If we already have a parser we ignore the document.open call.
  if (mParser || mParserAborted) {
    // The WHATWG spec says: "If the document has an active parser that isn't
    // a script-created parser, and the insertion point associated with that
    // parser's input stream is not undefined (that is, it does point to
    // somewhere in the input stream), then the method does nothing. Abort
    // these steps and return the Document object on which the method was
    // invoked."
    // Note that aborting a parser leaves the parser "active" with its
    // insertion point "not undefined". We track this using mParserAborted,
    // because aborting a parser nulls out mParser.
    return NS_OK;
  }

  // No calling document.open() without a script global object
  if (!mScriptGlobalObject) {
    return NS_OK;
  }

  nsPIDOMWindow* outer = GetWindow();
  if (!outer || (GetInnerWindow() != outer->GetCurrentInnerWindow())) {
    return NS_OK;
  }

  // check whether we're in the middle of unload.  If so, ignore this call.
  nsCOMPtr<nsIDocShell> shell = do_QueryReferent(mDocumentContainer);
  if (!shell) {
    // We won't be able to create a parser anyway.
    return NS_OK;
  }

  bool inUnload;
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
  nsCOMPtr<nsIChannel> callerChannel = callerDoc->GetChannel();

  // We're called from script. Make sure the script is from the same
  // origin, not just that the caller can access the document. This is
  // needed to keep document principals from ever changing, which is
  // needed because of the way we use our XOW code, and is a sane
  // thing to do anyways.

  bool equals = false;
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
    printf("nsHTMLDocument::Open callerDoc %s this %s\n", callerSpec.get(), thisSpec.get());
#endif

    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // Stop current loads targeted at the window this document is in.
  if (mScriptGlobalObject) {
    nsCOMPtr<nsIContentViewer> cv;
    shell->GetContentViewer(getter_AddRefs(cv));

    if (cv) {
      bool okToUnload;
      if (NS_SUCCEEDED(cv->PermitUnload(false, &okToUnload)) && !okToUnload) {
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

  nsresult rv = NS_NewChannel(getter_AddRefs(channel), uri, nsnull, group);

  if (NS_FAILED(rv)) {
    return rv;
  }

  // We can't depend on channels implementing property bags, so do our
  // base URI manually after reset.

  // Set the caller principal, if any, on the channel so that we'll
  // make sure to use it when we reset.
  rv = channel->SetOwner(callerPrincipal);
  NS_ENSURE_SUCCESS(rv, rv);

  if (callerChannel) {
    nsLoadFlags callerLoadFlags;
    rv = callerChannel->GetLoadFlags(&callerLoadFlags);
    NS_ENSURE_SUCCESS(rv, rv);

    nsLoadFlags loadFlags;
    rv = channel->GetLoadFlags(&loadFlags);
    NS_ENSURE_SUCCESS(rv, rv);

    loadFlags |= callerLoadFlags & nsIRequest::INHIBIT_PERSISTENT_CACHING;

    rv = channel->SetLoadFlags(loadFlags);
    NS_ENSURE_SUCCESS(rv, rv);
  }

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
    bool willReparent = mWillReparent;
    mWillReparent = true;
#endif

    // Should this pass true for aForceReuseInnerWindow?
    rv = window->SetNewDocument(this, nsnull, false);
    NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
    mWillReparent = willReparent;
#endif

    // Now make sure we're not flagged as the initial document anymore, now
    // that we've had stuff done to us.  From now on, if anyone tries to
    // document.open() us, they get a new inner window.
    SetIsInitialDocument(false);

    nsCOMPtr<nsIScriptGlobalObject> newScope(do_QueryReferent(mScopeObject));
    if (oldScope && newScope != oldScope) {
      rv = nsContentUtils::ReparentContentWrappersInScope(cx, oldScope, newScope);
      NS_ENSURE_SUCCESS(rv, rv);
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

  mParserAborted = false;
  mParser = nsHtml5Module::NewHtml5Parser();
  nsHtml5Module::Initialize(mParser, this, uri, shell, channel);
  rv = NS_OK;

  // This will be propagated to the parser when someone actually calls write()
  SetContentTypeInternal(contentType);

  // Prepare the docshell and the document viewer for the impending
  // out of band document.write()
  shell->PrepareForNewContentModel();

  // Now check whether we were opened with a "replace" argument.  If
  // so, we need to tell the docshell to not create a new history
  // entry for this load. Otherwise, make sure that we're doing a normal load,
  // not whatever type of load was previously done on this docshell.
  shell->SetLoadType(
    (aOptionalArgCount > 1 && aReplaceOrName.EqualsLiteral("replace"))
    ? LOAD_NORMAL_REPLACE : LOAD_NORMAL);

  nsCOMPtr<nsIContentViewer> cv;
  shell->GetContentViewer(getter_AddRefs(cv));
  if (cv) {
    cv->LoadStart(static_cast<nsIHTMLDocument *>(this));
  }

  // Add a wyciwyg channel request into the document load group
  NS_ASSERTION(!mWyciwygChannel, "nsHTMLDocument::Open(): wyciwyg "
               "channel already exists!");

  // In case the editor is listening and will see the new channel
  // being added, make sure mWriteLevel is non-zero so that the editor
  // knows that document.open/write/close() is being called on this
  // document.
  ++mWriteLevel;

  CreateAndAddWyciwygChannel();

  --mWriteLevel;

  SetReadyStateInternal(nsIDocument::READYSTATE_LOADING);

  // After changing everything around, make sure that the principal on the
  // document's compartment exactly matches NodePrincipal().
  DebugOnly<JSObject*> wrapper = GetWrapperPreserveColor();
  MOZ_ASSERT_IF(wrapper,
                JS_GetCompartmentPrincipals(js::GetObjectCompartment(wrapper)) ==
                nsJSPrincipals::get(NodePrincipal()));

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

  if (!mParser || !mParser->IsScriptCreated()) {
    return NS_OK;
  }

  ++mWriteLevel;
  nsresult rv = (static_cast<nsHtml5Parser*>(mParser.get()))->Parse(
    EmptyString(), nsnull, GetContentTypeInternal(), true);
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
  //
  // XXXhsivonen keeping this around for bug 577508 / 253951 still :-(
  if (GetShell()) {
    FlushPendingNotifications(Flush_Layout);
  }

  // Removing the wyciwygChannel here is wrong when document.close() is
  // called from within the document itself. However, legacy requires the
  // channel to be removed here. Otherwise, the load event never fires.
  NS_ASSERTION(mWyciwygChannel, "nsHTMLDocument::Close(): Trying to remove "
               "nonexistent wyciwyg channel!");
  RemoveWyciwygChannel();
  NS_ASSERTION(!mWyciwygChannel, "nsHTMLDocument::Close(): "
               "nsIWyciwygChannel could not be removed!");
  return rv;
}

nsresult
nsHTMLDocument::WriteCommon(JSContext *cx,
                            const nsAString& aText,
                            bool aNewlineTerminate)
{
  mTooDeepWriteRecursion =
    (mWriteLevel > NS_MAX_DOCUMENT_WRITE_DEPTH || mTooDeepWriteRecursion);
  NS_ENSURE_STATE(!mTooDeepWriteRecursion);

  if (!IsHTML() || mDisableDocWrite) {
    // No calling document.write*() on XHTML!

    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (mParserAborted) {
    // Hixie says aborting the parser doesn't undefine the insertion point.
    // However, since we null out mParser in that case, we track the
    // theoretically defined insertion point using mParserAborted.
    return NS_OK;
  }

  nsresult rv = NS_OK;

  void *key = GenerateParserKey();
  if (mParser && !mParser->IsInsertionPointDefined()) {
    if (mExternalScriptsBeingEvaluated) {
      // Instead of implying a call to document.open(), ignore the call.
      nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                      "DOM Events", this,
                                      nsContentUtils::eDOM_PROPERTIES,
                                      "DocumentWriteIgnored",
                                      nsnull, 0,
                                      mDocumentURI);
      return NS_OK;
    }
    mParser->Terminate();
    NS_ASSERTION(!mParser, "mParser should have been null'd out");
  }

  if (!mParser) {
    if (mExternalScriptsBeingEvaluated) {
      // Instead of implying a call to document.open(), ignore the call.
      nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                      "DOM Events", this,
                                      nsContentUtils::eDOM_PROPERTIES,
                                      "DocumentWriteIgnored",
                                      nsnull, 0,
                                      mDocumentURI);
      return NS_OK;
    }
    nsCOMPtr<nsISupports> ignored;
    rv = Open(NS_LITERAL_STRING("text/html"), EmptyString(), EmptyString(), cx,
              1, getter_AddRefs(ignored));

    // If Open() fails, or if it didn't create a parser (as it won't
    // if the user chose to not discard the current document through
    // onbeforeunload), don't write anything.
    if (NS_FAILED(rv) || !mParser) {
      return rv;
    }
    NS_ABORT_IF_FALSE(!JS_IsExceptionPending(cx),
                      "Open() succeeded but JS exception is pending");
  }

  static NS_NAMED_LITERAL_STRING(new_line, "\n");

  // Save the data in cache if the write isn't from within the doc
  if (mWyciwygChannel && !key) {
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
    rv = (static_cast<nsHtml5Parser*>(mParser.get()))->Parse(
      aText + new_line, key, GetContentTypeInternal(), false);
  } else {
    rv = (static_cast<nsHtml5Parser*>(mParser.get()))->Parse(
      aText, key, GetContentTypeInternal(), false);
  }

  --mWriteLevel;

  mTooDeepWriteRecursion = (mWriteLevel != 0 && mTooDeepWriteRecursion);

  return rv;
}

NS_IMETHODIMP
nsHTMLDocument::Write(const nsAString& aText, JSContext *cx)
{
  return WriteCommon(cx, aText, false);
}

NS_IMETHODIMP
nsHTMLDocument::Writeln(const nsAString& aText, JSContext *cx)
{
  return WriteCommon(cx, aText, true);
}

bool
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

static bool MatchItems(nsIContent* aContent, PRInt32 aNameSpaceID, 
                       nsIAtom* aAtom, void* aData)
{
  if (!(aContent->IsElement() && aContent->AsElement()->IsHTML())) {
    return false;
  }

  nsGenericHTMLElement* elem = static_cast<nsGenericHTMLElement*>(aContent);
  if (!elem->HasAttr(kNameSpaceID_None, nsGkAtoms::itemscope) ||
      elem->HasAttr(kNameSpaceID_None, nsGkAtoms::itemprop)) {
    return false;
  }

  nsTArray<nsCOMPtr<nsIAtom> >* tokens = static_cast<nsTArray<nsCOMPtr<nsIAtom> >*>(aData);
  if (tokens->IsEmpty()) {
    return true;
  }
 
  const nsAttrValue* attr = elem->GetParsedAttr(nsGkAtoms::itemtype);
  if (!attr)
    return false;

  for (PRUint32 i = 0; i < tokens->Length(); i++) {
    if (!attr->Contains(tokens->ElementAt(i), eCaseMatters)) {
      return false;
    }
  }
  return true;
}

static void DestroyTokens(void* aData)
{
  nsTArray<nsCOMPtr<nsIAtom> >* tokens = static_cast<nsTArray<nsCOMPtr<nsIAtom> >*>(aData);
  delete tokens;
}

static void* CreateTokens(nsINode* aRootNode, const nsString* types)
{
  nsTArray<nsCOMPtr<nsIAtom> >* tokens = new nsTArray<nsCOMPtr<nsIAtom> >();
  nsAString::const_iterator iter, end;
  types->BeginReading(iter);
  types->EndReading(end);
  
  // skip initial whitespace
  while (iter != end && nsContentUtils::IsHTMLWhitespace(*iter)) {
    ++iter;
  }

  // parse the tokens
  while (iter != end) {
    nsAString::const_iterator start(iter);

    do {
      ++iter;
    } while (iter != end && !nsContentUtils::IsHTMLWhitespace(*iter));

    tokens->AppendElement(do_GetAtom(Substring(start, iter)));

    // skip whitespace
    while (iter != end && nsContentUtils::IsHTMLWhitespace(*iter)) {
      ++iter;
    }
  }
  return tokens;
}

NS_IMETHODIMP
nsHTMLDocument::GetItems(const nsAString& types, nsIDOMNodeList** aReturn)
{
  nsRefPtr<nsContentList> elements = 
    NS_GetFuncStringContentList(this, MatchItems, DestroyTokens, 
                                CreateTokens, types);
  NS_ENSURE_TRUE(elements, NS_ERROR_OUT_OF_MEMORY);
  elements.forget(aReturn);
  return NS_OK;
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
nsHTMLDocument::GetSelection(nsISelection** aReturn)
{
  nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(GetScopeObject());
  nsCOMPtr<nsPIDOMWindow> pwin = do_QueryInterface(window);
  NS_ENSURE_TRUE(pwin, NS_OK);
  NS_ASSERTION(pwin->IsInnerWindow(), "Should have inner window here!");
  NS_ENSURE_TRUE(pwin->GetOuterWindow() &&
                 pwin->GetOuterWindow()->GetCurrentInnerWindow() == pwin,
                 NS_OK);

  return window->GetSelection(aReturn);
  
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

nsresult
nsHTMLDocument::ResolveName(const nsAString& aName,
                            nsIContent *aForm,
                            nsISupports **aResult,
                            nsWrapperCache **aCache)
{
  *aResult = nsnull;
  *aCache = nsnull;

  nsIdentifierMapEntry *entry = mIdentifierMap.GetEntry(aName);
  if (!entry) {
    return NS_OK;
  }

  PRUint32 length = 0;
  nsBaseContentList *list = entry->GetNameContentList();
  if (list) {
    list->GetLength(&length);
  }

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

//----------------------------

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

static bool MatchFormControls(nsIContent* aContent, PRInt32 aNamespaceID,
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
  nsIScriptElement* script = mScriptLoader->GetCurrentParserInsertedScript();
  if (script && mParser && mParser->IsScriptCreated()) {
    nsCOMPtr<nsIParser> creatorParser = script->GetCreatorParser();
    if (creatorParser != mParser) {
      // Make scripts that aren't inserted by the active parser of this document
      // participate in the context of the script that document.open()ed
      // this document.
      return nsnull;
    }
  }
  return script;
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
    if (mElement && mElement->OwnerDoc() == mDoc) {
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
  NS_ASSERTION(PRInt32(mContentEditableCount) + aChange >= 0,
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
        nsRefPtr<nsRange> range = new nsRange();
        rv = range->SelectNode(node);
        if (NS_FAILED(rv)) {
          // The node might be detached from the document at this point,
          // which would cause this call to fail.  In this case, we can
          // safely ignore the contenteditable count change.
          return;
        }

        nsCOMPtr<nsIInlineSpellChecker> spellChecker;
        rv = editor->GetInlineSpellChecker(false,
                                           getter_AddRefs(spellChecker));
        NS_ENSURE_SUCCESS(rv, );

        if (spellChecker) {
          rv = spellChecker->SpellCheckRange(range);
        }
      }
    }
  }
}

static bool
DocAllResultMatch(nsIContent* aContent, PRInt32 aNamespaceID, nsIAtom* aAtom,
                  void* aData)
{
  if (aContent->GetID() == aAtom) {
    return true;
  }

  nsGenericHTMLElement* elm = nsGenericHTMLElement::FromContent(aContent);
  if (!elm) {
    return false;
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
    return false;
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
                                   nsnull, nsnull, true, id);
    entry->SetDocAllList(docAllList);
  }

  // Check if there are more than 1 entries. Do this by getting the second one
  // rather than the length since getting the length always requires walking
  // the entire document.

  nsIContent* cont = docAllList->Item(1, true);
  if (cont) {
    *aCache = docAllList;
    return static_cast<nsINodeList*>(docAllList);
  }

  // There's only 0 or 1 items. Return the first one or null.
  *aCache = cont = docAllList->Item(0, true);

  return cont;
}

static void
NotifyEditableStateChange(nsINode *aNode, nsIDocument *aDocument)
{
  for (nsIContent* child = aNode->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->IsElement()) {
      child->AsElement()->UpdateState(true);
    }
    NotifyEditableStateChange(child, aDocument);
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

static bool HasPresShell(nsPIDOMWindow *aWindow)
{
  nsIDocShell *docShell = aWindow->GetDocShell();
  if (!docShell)
    return false;
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

  bool designMode = HasFlag(NODE_IS_EDITABLE);
  EditingState newState = designMode ? eDesignMode :
                          (mContentEditableCount > 0 ? eContentEditable : eOff);
  if (mEditingState == newState) {
    // No changes in editing mode.
    return NS_OK;
  }

  if (newState == eOff) {
    // Editing is being turned off.
    nsAutoScriptBlocker scriptBlocker;
    NotifyEditableStateChange(this, this);
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

  bool makeWindowEditable = mEditingState == eOff;
  bool updateState = false;
  bool spellRecheckAll = false;
  nsCOMPtr<nsIEditor> editor;

  {
    EditingState oldState = mEditingState;
    nsAutoEditingState push(this, eSettingUp);

    if (makeWindowEditable) {
      // Editing is being turned on (through designMode or contentEditable)
      // Turn on editor.
      // XXX This can cause flushing which can change the editing state, so make
      //     sure to avoid recursing.
      rv = editSession->MakeWindowEditable(window, "html", false, false,
                                           true);
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
    rv = LoadChromeSheetSync(uri, true, getter_AddRefs(sheet));
    NS_ENSURE_TRUE(sheet, rv);

    bool result = agentSheets.AppendObject(sheet);
    NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);

    // Should we update the editable state of all the nodes in the document? We
    // need to do this when the designMode value changes, as that overrides
    // specific states on the elements.
    if (designMode) {
      // designMode is being turned on (overrides contentEditable).
      rv = NS_NewURI(getter_AddRefs(uri), NS_LITERAL_STRING("resource://gre/res/designmode.css"));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = LoadChromeSheetSync(uri, true, getter_AddRefs(sheet));
      NS_ENSURE_TRUE(sheet, rv);

      result = agentSheets.AppendObject(sheet);
      NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);

      // Disable scripting and plugins.
      rv = editSession->DisableJSAndPlugins(window);
      NS_ENSURE_SUCCESS(rv, rv);

      updateState = true;
      spellRecheckAll = oldState == eContentEditable;
    }
    else if (oldState == eDesignMode) {
      // designMode is being turned off (contentEditable is still on).
      RemoveFromAgentSheets(agentSheets, NS_LITERAL_STRING("resource://gre/res/designmode.css"));

      rv = editSession->RestoreJSAndPlugins(window);
      NS_ENSURE_SUCCESS(rv, rv);

      updateState = true;
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
    bool unused;
    rv = ExecCommand(NS_LITERAL_STRING("insertBrOnReturn"), false,
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
    nsAutoScriptBlocker scriptBlocker;
    NotifyEditableStateChange(this, this);
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
      bool subsumes;
      rv = subject->Subsumes(NodePrincipal(), &subsumes);
      NS_ENSURE_SUCCESS(rv, rv);

      NS_ENSURE_TRUE(subsumes, NS_ERROR_DOM_PROP_ACCESS_DENIED);
    }
  }

  bool editableMode = HasFlag(NODE_IS_EDITABLE);
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
  bool useNewParam;
  bool convertToBoolean;
};

static const struct MidasCommand gMidasCommandTable[] = {
  { "bold",          "cmd_bold",            "", true,  false },
  { "italic",        "cmd_italic",          "", true,  false },
  { "underline",     "cmd_underline",       "", true,  false },
  { "strikethrough", "cmd_strikethrough",   "", true,  false },
  { "subscript",     "cmd_subscript",       "", true,  false },
  { "superscript",   "cmd_superscript",     "", true,  false },
  { "cut",           "cmd_cut",             "", true,  false },
  { "copy",          "cmd_copy",            "", true,  false },
  { "paste",         "cmd_paste",           "", true,  false },
  { "delete",        "cmd_deleteCharBackward", "", true,  false },
  { "forwarddelete", "cmd_deleteCharForward", "", true,  false },
  { "selectall",     "cmd_selectAll",       "", true,  false },
  { "undo",          "cmd_undo",            "", true,  false },
  { "redo",          "cmd_redo",            "", true,  false },
  { "indent",        "cmd_indent",          "", true,  false },
  { "outdent",       "cmd_outdent",         "", true,  false },
  { "backcolor",     "cmd_highlight",       "", false, false },
  { "forecolor",     "cmd_fontColor",       "", false, false },
  { "hilitecolor",   "cmd_highlight",       "", false, false },
  { "fontname",      "cmd_fontFace",        "", false, false },
  { "fontsize",      "cmd_fontSize",        "", false, false },
  { "increasefontsize", "cmd_increaseFont", "", false, false },
  { "decreasefontsize", "cmd_decreaseFont", "", false, false },
  { "inserthorizontalrule", "cmd_insertHR", "", true,  false },
  { "createlink",    "cmd_insertLinkNoUI",  "", false, false },
  { "insertimage",   "cmd_insertImageNoUI", "", false, false },
  { "inserthtml",    "cmd_insertHTML",      "", false, false },
  { "inserttext",    "cmd_insertText",      "", false, false },
  { "gethtml",       "cmd_getContents",     "", false, false },
  { "justifyleft",   "cmd_align",       "left", true,  false },
  { "justifyright",  "cmd_align",      "right", true,  false },
  { "justifycenter", "cmd_align",     "center", true,  false },
  { "justifyfull",   "cmd_align",    "justify", true,  false },
  { "removeformat",  "cmd_removeStyles",    "", true,  false },
  { "unlink",        "cmd_removeLinks",     "", true,  false },
  { "insertorderedlist",   "cmd_ol",        "", true,  false },
  { "insertunorderedlist", "cmd_ul",        "", true,  false },
  { "insertparagraph", "cmd_paragraphState", "p", true,  false },
  { "formatblock",   "cmd_paragraphState",  "", false, false },
  { "heading",       "cmd_paragraphState",  "", false, false },
  { "styleWithCSS",  "cmd_setDocumentUseCSS", "", false, true },
  { "contentReadOnly", "cmd_setDocumentReadOnly", "", false, true },
  { "insertBrOnReturn", "cmd_insertBrOnReturn", "", false, true },
  { "enableObjectResizing", "cmd_enableObjectResizing", "", false, true },
  { "enableInlineTableEditing", "cmd_enableInlineTableEditing", "", false, true },
#if 0
// no editor support to remove alignments right now
  { "justifynone",   "cmd_align",           "", true,  false },

// the following will need special review before being turned on
  { "saveas",        "cmd_saveAs",          "", true,  false },
  { "print",         "cmd_print",           "", true,  false },
#endif
  { NULL, NULL, NULL, false, false }
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

static bool
ConvertToMidasInternalCommandInner(const nsAString& inCommandID,
                                   const nsAString& inParam,
                                   nsACString& outCommandID,
                                   nsACString& outParam,
                                   bool& outIsBoolean,
                                   bool& outBooleanValue,
                                   bool aIgnoreParams)
{
  NS_ConvertUTF16toUTF8 convertedCommandID(inCommandID);

  // Hack to support old boolean commands that were backwards (see bug 301490).
  bool invertBool = false;
  if (convertedCommandID.LowerCaseEqualsLiteral("usecss")) {
    convertedCommandID.Assign("styleWithCSS");
    invertBool = true;
  } else if (convertedCommandID.LowerCaseEqualsLiteral("readonly")) {
    convertedCommandID.Assign("contentReadOnly");
    invertBool = true;
  }

  PRUint32 i;
  bool found = false;
  for (i = 0; i < MidasCommandCount; ++i) {
    if (convertedCommandID.Equals(gMidasCommandTable[i].incomingCommandString,
                                  nsCaseInsensitiveCStringComparator())) {
      found = true;
      break;
    }
  }

  if (!found) {
    // reset results if the command is not found in our table
    outCommandID.SetLength(0);
    outParam.SetLength(0);
    outIsBoolean = false;
    return false;
  }

  // set outCommandID (what we use internally)
  outCommandID.Assign(gMidasCommandTable[i].internalCommandString);

  // set outParam & outIsBoolean based on flags from the table
  outIsBoolean = gMidasCommandTable[i].convertToBoolean;

  if (aIgnoreParams) {
    // No further work to do
    return true;
  }

  if (gMidasCommandTable[i].useNewParam) {
    // Just have to copy it, no checking
    outParam.Assign(gMidasCommandTable[i].internalParamString);
    return true;
  }

  // handle checking of param passed in
  if (outIsBoolean) {
    // If this is a boolean value and it's not explicitly false (e.g. no value)
    // we default to "true". For old backwards commands we invert the check (see
    // bug 301490).
    if (invertBool) {
      outBooleanValue = inParam.LowerCaseEqualsLiteral("false");
    } else {
      outBooleanValue = !inParam.LowerCaseEqualsLiteral("false");
    }
    outParam.Truncate();

    return true;
  }

  // String parameter -- see if we need to convert it (necessary for
  // cmd_paragraphState and cmd_fontSize)
  if (outCommandID.EqualsLiteral("cmd_paragraphState")) {
    const PRUnichar* start = inParam.BeginReading();
    const PRUnichar* end = inParam.EndReading();
    if (start != end && *start == '<' && *(end - 1) == '>') {
      ++start;
      --end;
    }

    NS_ConvertUTF16toUTF8 convertedParam(Substring(start, end));
    PRUint32 j;
    for (j = 0; j < ArrayLength(gBlocks); ++j) {
      if (convertedParam.Equals(gBlocks[j],
                                nsCaseInsensitiveCStringComparator())) {
        outParam.Assign(gBlocks[j]);
        break;
      }
    }

    if (j == ArrayLength(gBlocks)) {
      outParam.Truncate();
    }
  } else if (outCommandID.EqualsLiteral("cmd_fontSize")) {
    // Per editing spec as of April 23, 2012, we need to reject the value if
    // it's not a valid floating-point number surrounded by optional whitespace.
    // Otherwise, we parse it as a legacy font size.  For now, we just parse as
    // a legacy font size regardless (matching WebKit) -- bug 747879.
    outParam.Truncate();
    PRInt32 size = nsContentUtils::ParseLegacyFontSize(inParam);
    if (size) {
      outParam.AppendInt(size);
    }
  } else {
    CopyUTF16toUTF8(inParam, outParam);
  }

  return true;
}

static bool
ConvertToMidasInternalCommand(const nsAString & inCommandID,
                              const nsAString & inParam,
                              nsACString& outCommandID,
                              nsACString& outParam,
                              bool& outIsBoolean,
                              bool& outBooleanValue)
{
  return ConvertToMidasInternalCommandInner(inCommandID, inParam, outCommandID,
                                            outParam, outIsBoolean,
                                            outBooleanValue, false);
}

static bool
ConvertToMidasInternalCommand(const nsAString & inCommandID,
                              nsACString& outCommandID)
{
  nsCAutoString dummyCString;
  nsAutoString dummyString;
  bool dummyBool;
  return ConvertToMidasInternalCommandInner(inCommandID, dummyString,
                                            outCommandID, dummyCString,
                                            dummyBool, dummyBool, true);
}

jsid
nsHTMLDocument::sCutCopyInternal_id = JSID_VOID;
jsid
nsHTMLDocument::sPasteInternal_id = JSID_VOID;

/* Helper function to check security of clipboard commands. If aPaste is */
/* true, we check paste, else we check cutcopy */
nsresult
nsHTMLDocument::DoClipboardSecurityCheck(bool aPaste)
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
          INTERNED_STRING_TO_JSID(cx, ::JS_InternString(cx, "paste"));
      }
      rv = secMan->CheckPropertyAccess(cx, nsnull, classNameStr.get(),
                                       nsHTMLDocument::sPasteInternal_id,
                                       nsIXPCSecurityManager::ACCESS_GET_PROPERTY);
    } else {
      if (nsHTMLDocument::sCutCopyInternal_id == JSID_VOID) {
        nsHTMLDocument::sCutCopyInternal_id =
          INTERNED_STRING_TO_JSID(cx, ::JS_InternString(cx, "cutcopy"));
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
nsHTMLDocument::ExecCommand(const nsAString& commandID,
                            bool doShowUI,
                            const nsAString& value,
                            bool* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  //  for optional parameters see dom/src/base/nsHistory.cpp: HistoryImpl::Go()
  //  this might add some ugly JS dependencies?

  *_retval = false;

  nsCAutoString cmdToDispatch, paramStr;
  bool isBool, boolVal;
  if (!ConvertToMidasInternalCommand(commandID, value,
                                     cmdToDispatch, paramStr,
                                     isBool, boolVal)) {
    // Return false
    return NS_OK;
  }

  // if editing is not on, bail
  NS_ENSURE_TRUE(IsEditingOnAfterFlush(), NS_ERROR_FAILURE);

  // if they are requesting UI from us, let's fail since we have no UI
  if (doShowUI) {
    return NS_OK;
  }

  if (commandID.LowerCaseEqualsLiteral("gethtml")) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_OK;

  if (commandID.LowerCaseEqualsLiteral("cut") ||
      commandID.LowerCaseEqualsLiteral("copy")) {
    rv = DoClipboardSecurityCheck(false);
  } else if (commandID.LowerCaseEqualsLiteral("paste")) {
    rv = DoClipboardSecurityCheck(true);
  }

  NS_ENSURE_SUCCESS(rv, rv);

  // get command manager and dispatch command to our window if it's acceptable
  nsCOMPtr<nsICommandManager> cmdMgr;
  GetMidasCommandManager(getter_AddRefs(cmdMgr));
  NS_ENSURE_TRUE(cmdMgr, NS_ERROR_FAILURE);

  nsIDOMWindow* window = GetWindow();
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  if ((cmdToDispatch.EqualsLiteral("cmd_fontSize") ||
       cmdToDispatch.EqualsLiteral("cmd_insertImageNoUI") ||
       cmdToDispatch.EqualsLiteral("cmd_insertLinkNoUI") ||
       cmdToDispatch.EqualsLiteral("cmd_paragraphState")) &&
      paramStr.IsEmpty()) {
    // Invalid value, return false
    return NS_OK;
  }

  // Return false for disabled commands (bug 760052)
  bool enabled = false;
  cmdMgr->IsCommandEnabled(cmdToDispatch.get(), window, &enabled);
  if (!enabled) {
    return NS_OK;
  }

  if (!isBool && paramStr.IsEmpty()) {
    rv = cmdMgr->DoCommand(cmdToDispatch.get(), nsnull, window);
  } else {
    // we have a command that requires a parameter, create params
    nsCOMPtr<nsICommandParams> cmdParams = do_CreateInstance(
                                            NS_COMMAND_PARAMS_CONTRACTID, &rv);
    NS_ENSURE_TRUE(cmdParams, NS_ERROR_OUT_OF_MEMORY);

    if (isBool) {
      rv = cmdParams->SetBooleanValue("state_attribute", boolVal);
    } else if (cmdToDispatch.EqualsLiteral("cmd_fontFace")) {
      rv = cmdParams->SetStringValue("state_attribute", value);
    } else if (cmdToDispatch.EqualsLiteral("cmd_insertHTML") ||
               cmdToDispatch.EqualsLiteral("cmd_insertText")) {
      rv = cmdParams->SetStringValue("state_data", value);
    } else {
      rv = cmdParams->SetCStringValue("state_attribute", paramStr.get());
    }
    NS_ENSURE_SUCCESS(rv, rv);
    rv = cmdMgr->DoCommand(cmdToDispatch.get(), cmdParams, window);
  }

  *_retval = NS_SUCCEEDED(rv);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

/* boolean queryCommandEnabled(in DOMString commandID); */
NS_IMETHODIMP
nsHTMLDocument::QueryCommandEnabled(const nsAString& commandID,
                                    bool* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = false;

  nsCAutoString cmdToDispatch;
  if (!ConvertToMidasInternalCommand(commandID, cmdToDispatch)) {
    // Return false
    return NS_OK;
  }

  // if editing is not on, bail
  NS_ENSURE_TRUE(IsEditingOnAfterFlush(), NS_ERROR_FAILURE);

  // get command manager and dispatch command to our window if it's acceptable
  nsCOMPtr<nsICommandManager> cmdMgr;
  GetMidasCommandManager(getter_AddRefs(cmdMgr));
  NS_ENSURE_TRUE(cmdMgr, NS_ERROR_FAILURE);

  nsIDOMWindow* window = GetWindow();
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  return cmdMgr->IsCommandEnabled(cmdToDispatch.get(), window, _retval);
}

/* boolean queryCommandIndeterm (in DOMString commandID); */
NS_IMETHODIMP
nsHTMLDocument::QueryCommandIndeterm(const nsAString & commandID,
                                     bool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = false;

  nsCAutoString cmdToDispatch;
  if (!ConvertToMidasInternalCommand(commandID, cmdToDispatch)) {
    // Return false
    return NS_OK;
  }

  // if editing is not on, bail
  NS_ENSURE_TRUE(IsEditingOnAfterFlush(), NS_ERROR_FAILURE);

  // get command manager and dispatch command to our window if it's acceptable
  nsCOMPtr<nsICommandManager> cmdMgr;
  GetMidasCommandManager(getter_AddRefs(cmdMgr));
  NS_ENSURE_TRUE(cmdMgr, NS_ERROR_FAILURE);

  nsIDOMWindow* window = GetWindow();
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsresult rv;
  nsCOMPtr<nsICommandParams> cmdParams = do_CreateInstance(
                                           NS_COMMAND_PARAMS_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = cmdMgr->GetCommandState(cmdToDispatch.get(), window, cmdParams);
  NS_ENSURE_SUCCESS(rv, rv);

  // If command does not have a state_mixed value, this call fails and sets
  // *_retval to false.  This is fine -- we want to return false in that case
  // anyway (bug 738385), so we just return NS_OK regardless.
  cmdParams->GetBooleanValue("state_mixed", _retval);
  return NS_OK;
}

/* boolean queryCommandState(in DOMString commandID); */
NS_IMETHODIMP
nsHTMLDocument::QueryCommandState(const nsAString & commandID, bool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = false;

  nsCAutoString cmdToDispatch, paramToCheck;
  bool dummy, dummy2;
  if (!ConvertToMidasInternalCommand(commandID, commandID,
                                     cmdToDispatch, paramToCheck,
                                     dummy, dummy2)) {
    // Return false
    return NS_OK;
  }

  // if editing is not on, bail
  NS_ENSURE_TRUE(IsEditingOnAfterFlush(), NS_ERROR_FAILURE);

  // get command manager and dispatch command to our window if it's acceptable
  nsCOMPtr<nsICommandManager> cmdMgr;
  GetMidasCommandManager(getter_AddRefs(cmdMgr));
  NS_ENSURE_TRUE(cmdMgr, NS_ERROR_FAILURE);

  nsIDOMWindow* window = GetWindow();
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  if (commandID.LowerCaseEqualsLiteral("usecss")) {
    // Per spec, state is supported for styleWithCSS but not useCSS, so we just
    // return false always.
    *_retval = false;
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsICommandParams> cmdParams = do_CreateInstance(
                                           NS_COMMAND_PARAMS_CONTRACTID, &rv);
  NS_ENSURE_TRUE(cmdParams, NS_ERROR_OUT_OF_MEMORY);

  rv = cmdMgr->GetCommandState(cmdToDispatch.get(), window, cmdParams);
  NS_ENSURE_SUCCESS(rv, rv);

  // handle alignment as a special case (possibly other commands too?)
  // Alignment is special because the external api is individual
  // commands but internally we use cmd_align with different
  // parameters.  When getting the state of this command, we need to
  // return the boolean for this particular alignment rather than the
  // string of 'which alignment is this?'
  if (cmdToDispatch.EqualsLiteral("cmd_align")) {
    char * actualAlignmentType = nsnull;
    rv = cmdParams->GetCStringValue("state_attribute", &actualAlignmentType);
    if (NS_SUCCEEDED(rv) && actualAlignmentType && actualAlignmentType[0]) {
      *_retval = paramToCheck.Equals(actualAlignmentType);
    }
    if (actualAlignmentType) {
      nsMemory::Free(actualAlignmentType);
    }
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  // If command does not have a state_all value, this call fails and sets
  // *_retval to false.  This is fine -- we want to return false in that case
  // anyway (bug 738385), so we just return NS_OK regardless.
  cmdParams->GetBooleanValue("state_all", _retval);
  return NS_OK;
}

/* boolean queryCommandSupported(in DOMString commandID); */
NS_IMETHODIMP
nsHTMLDocument::QueryCommandSupported(const nsAString & commandID,
                                      bool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  // commandID is supported if it can be converted to a Midas command
  nsCAutoString cmdToDispatch;
  *_retval = ConvertToMidasInternalCommand(commandID, cmdToDispatch);

  return NS_OK;
}

/* DOMString queryCommandValue(in DOMString commandID); */
NS_IMETHODIMP
nsHTMLDocument::QueryCommandValue(const nsAString & commandID,
                                  nsAString &_retval)
{
  _retval.SetLength(0);

  nsCAutoString cmdToDispatch, paramStr;
  if (!ConvertToMidasInternalCommand(commandID, cmdToDispatch)) {
    // Return empty string
    return NS_OK;
  }

  // if editing is not on, bail
  NS_ENSURE_TRUE(IsEditingOnAfterFlush(), NS_ERROR_FAILURE);

  // get command manager and dispatch command to our window if it's acceptable
  nsCOMPtr<nsICommandManager> cmdMgr;
  GetMidasCommandManager(getter_AddRefs(cmdMgr));
  NS_ENSURE_TRUE(cmdMgr, NS_ERROR_FAILURE);

  nsIDOMWindow* window = GetWindow();
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  // create params
  nsresult rv;
  nsCOMPtr<nsICommandParams> cmdParams = do_CreateInstance(
                                           NS_COMMAND_PARAMS_CONTRACTID, &rv);
  NS_ENSURE_TRUE(cmdParams, NS_ERROR_OUT_OF_MEMORY);

  // this is a special command since we are calling DoCommand rather than
  // GetCommandState like the other commands
  if (cmdToDispatch.EqualsLiteral("cmd_getContents")) {
    rv = cmdParams->SetBooleanValue("selection_only", true);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = cmdParams->SetCStringValue("format", "text/html");
    NS_ENSURE_SUCCESS(rv, rv);
    rv = cmdMgr->DoCommand(cmdToDispatch.get(), cmdParams, window);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = cmdParams->GetStringValue("result", _retval);
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  rv = cmdParams->SetCStringValue("state_attribute", paramStr.get());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = cmdMgr->GetCommandState(cmdToDispatch.get(), window, cmdParams);
  NS_ENSURE_SUCCESS(rv, rv);

  // If command does not have a state_attribute value, this call fails, and
  // _retval will wind up being the empty string.  This is fine -- we want to
  // return "" in that case anyway (bug 738385), so we just return NS_OK
  // regardless.
  nsXPIDLCString cStringResult;
  cmdParams->GetCStringValue("state_attribute",
                             getter_Copies(cStringResult));
  CopyUTF8toUTF16(cStringResult, _retval);

  return NS_OK;
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

bool
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

/* virtual */ void
nsHTMLDocument::DocSizeOfExcludingThis(nsWindowSizes* aWindowSizes) const
{
  nsDocument::DocSizeOfExcludingThis(aWindowSizes);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mImages
  // - mApplets
  // - mEmbeds
  // - mLinks
  // - mAnchors
  // - mScripts
  // - mForms
  // - mFormControls
  // - mWyciwygChannel
  // - mMidasCommandManager
}
