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
#include "nsIHTMLCSSStyleSheet.h"
#include "nsHTMLAtoms.h"
#include "nsLayoutAtoms.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsIDOMNode.h" // for Find
#include "nsIDOMNodeList.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"
#include "nsIDOMComment.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMWindowInternal.h"
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
#include "nsIWebNavigation.h"
#include "nsIBaseWindow.h"
#include "nsIWebShellServices.h"
#include "nsIDocumentLoader.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIXPConnect.h"
#include "nsContentList.h"
#include "nsDOMError.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScrollableView.h"

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
#include "nsINameSpaceManager.h"
#include "nsGenericHTMLElement.h"
#include "nsGenericDOMNodeList.h"
#include "nsICSSLoader.h"
#include "nsIHttpChannel.h"
#include "nsIFile.h"
#include "nsIEventListenerManager.h"
#include "nsISelectElement.h"
#include "nsIFrameSelection.h"
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
#include "nsICacheEntryDescriptor.h"
#include "nsIJSContextStack.h"
#include "nsIDocumentViewer.h"
#include "nsIWyciwygChannel.h"

#include "nsIPrompt.h"
//AHMED 12-2
#include "nsBidiUtils.h"

#include "nsIEditingSession.h"
#include "nsNodeInfoManager.h"

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


#define ID_NOT_IN_DOCUMENT ((nsIContent *)1)
#define NAME_NOT_VALID ((nsBaseContentList*)1)

static NS_DEFINE_CID(kCookieServiceCID, NS_COOKIESERVICE_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kCharsetAliasCID, NS_CHARSETALIAS_CID);

static PRBool
IsNamedItem(nsIContent* aContent, nsIAtom *aTag, nsAString& aName);

// MatchElementId is defined in nsXMLDocument.cpp
nsIContent *
MatchElementId(nsIContent *aContent, const nsACString& aUTF8Id, const nsAString& aId);


static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

nsIRDFService* nsHTMLDocument::gRDF;
nsrefcnt       nsHTMLDocument::gRefCntRDFService = 0;
PRUint32       nsHTMLDocument::gWyciwygSessionCnt = 0;

static int PR_CALLBACK
MyPrefChangedCallback(const char*aPrefName, void* instance_data)
{
  const nsAdoptingString& detector_name =
    nsContentUtils::GetLocalizedStringPref("intl.charset.detector");

  if (detector_name.Length() > 0) {
    PL_strncpy(g_detector_contractid, NS_CHARSET_DETECTOR_CONTRACTID_BASE,
               DETECTOR_CONTRACTID_MAX);
    PL_strncat(g_detector_contractid,
               NS_ConvertUCS2toUTF8(detector_name).get(),
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
nsresult
NS_NewHTMLDocument(nsIDocument** aInstancePtrResult)
{
  nsHTMLDocument* doc = new nsHTMLDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = doc->Init();

  if (NS_FAILED(rv)) {
    delete doc;

    return rv;
  }

  *aInstancePtrResult = doc;
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}

class IdAndNameMapEntry : public PLDHashEntryHdr
{
public:
  IdAndNameMapEntry(const nsAString& aString) :
    mKey(aString), mIdContent(nsnull), mContentList(nsnull)
  {
  }

  ~IdAndNameMapEntry()
  {
    if (mContentList && mContentList != NAME_NOT_VALID) {
      NS_RELEASE(mContentList);
    }
  }

  nsString mKey;
  nsIContent *mIdContent;
  nsBaseContentList *mContentList;
};


PR_STATIC_CALLBACK(const void *)
IdAndNameHashGetKey(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  IdAndNameMapEntry *e = NS_STATIC_CAST(IdAndNameMapEntry *, entry);

  return NS_STATIC_CAST(const nsAString *, &e->mKey);
}

PR_STATIC_CALLBACK(PLDHashNumber)
IdAndNameHashHashKey(PLDHashTable *table, const void *key)
{
  const nsAString *str = NS_STATIC_CAST(const nsAString *, key);

  return HashString(*str);
}

PR_STATIC_CALLBACK(PRBool)
IdAndNameHashMatchEntry(PLDHashTable *table, const PLDHashEntryHdr *entry,
                        const void *key)
{
  const IdAndNameMapEntry *e =
    NS_STATIC_CAST(const IdAndNameMapEntry *, entry);
  const nsAString *str = NS_STATIC_CAST(const nsAString *, key);

  return str->Equals(e->mKey);
}

PR_STATIC_CALLBACK(void)
IdAndNameHashClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  IdAndNameMapEntry *e = NS_STATIC_CAST(IdAndNameMapEntry *, entry);

  // An entry is being cleared, let the entry do its own cleanup.
  e->~IdAndNameMapEntry();
}

PR_STATIC_CALLBACK(PRBool)
IdAndNameHashInitEntry(PLDHashTable *table, PLDHashEntryHdr *entry,
                       const void *key)
{
  const nsAString *keyStr = NS_STATIC_CAST(const nsAString *, key);

  // Inititlize the entry with placement new
  new (entry) IdAndNameMapEntry(*keyStr);
  return PR_TRUE;
}

  // NOTE! nsDocument::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

nsHTMLDocument::nsHTMLDocument()
  : mCompatMode(eCompatibility_NavQuirks),
    mTexttype(IBMBIDI_TEXTTYPE_LOGICAL)
{

  // NOTE! nsDocument::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

  if (gRefCntRDFService++ == 0)
  {
    CallGetService(kRDFServiceCID, &gRDF);
  }

  mDefaultElementType = kNameSpaceID_XHTML;
}

nsHTMLDocument::~nsHTMLDocument()
{
  if (--gRefCntRDFService == 0) {
    NS_IF_RELEASE(gRDF);
  }

  if (mIdAndNameHashTable.ops) {
    PL_DHashTableFinish(&mIdAndNameHashTable);
  }
}

NS_IMPL_ADDREF_INHERITED(nsHTMLDocument, nsDocument)
NS_IMPL_RELEASE_INHERITED(nsHTMLDocument, nsDocument)


// QueryInterface implementation for nsHTMLDocument
NS_INTERFACE_MAP_BEGIN(nsHTMLDocument)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLDocument)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLDocument)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLDocument)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLDocument)
NS_INTERFACE_MAP_END_INHERITING(nsDocument)


nsresult
nsHTMLDocument::Init()
{
  nsresult rv = nsDocument::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  static PLDHashTableOps hash_table_ops =
  {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    IdAndNameHashGetKey,
    IdAndNameHashHashKey,
    IdAndNameHashMatchEntry,
    PL_DHashMoveEntryStub,
    IdAndNameHashClearEntry,
    PL_DHashFinalizeStub,
    IdAndNameHashInitEntry
  };

  PRBool ok = PL_DHashTableInit(&mIdAndNameHashTable, &hash_table_ops, nsnull,
                                sizeof(IdAndNameMapEntry), 16);
  if (!ok) {
    mIdAndNameHashTable.ops = nsnull;

    return NS_ERROR_OUT_OF_MEMORY;
  }

  PrePopulateHashTables();

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
nsHTMLDocument::ResetToURI(nsIURI *aURI, nsILoadGroup *aLoadGroup)
{
  mLoadFlags = nsIRequest::LOAD_NORMAL;

  nsDocument::ResetToURI(aURI, aLoadGroup);

  InvalidateHashTables();
  PrePopulateHashTables();

  mImages = nsnull;
  mApplets = nsnull;
  mEmbeds = nsnull;
  mLinks = nsnull;
  mAnchors = nsnull;

  mBodyContent = nsnull;

  mImageMaps.Clear();
  mForms = nsnull;

  NS_ASSERTION(!mWyciwygChannel,
               "nsHTMLDocument::Reset() - Wyciwyg Channel  still exists!");

  mWyciwygChannel = nsnull;

  // Make the content type default to "text/html", we are a HTML
  // document, after all. Once we start getting data, this may be
  // changed.
  mContentType = "text/html";
}

nsStyleSet::sheetType
nsHTMLDocument::GetAttrSheetType()
{
  if (IsXHTML()) {
    return nsDocument::GetAttrSheetType();
  }
  
  return nsStyleSet::eHTMLPresHintSheet;
}

nsresult
nsHTMLDocument::CreateShell(nsIPresContext* aContext,
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
nsHTMLDocument::TryCacheCharset(nsICacheEntryDescriptor* aCacheDescriptor,
                                PRInt32& aCharsetSource,
                                nsACString& aCharset)
{
  nsresult rv;

  if (kCharsetFromCache <= aCharsetSource) {
    return PR_TRUE;
  }

  nsXPIDLCString cachedCharset;
  rv = aCacheDescriptor->GetMetaDataElement("charset",
                                           getter_Copies(cachedCharset));
  if (NS_SUCCEEDED(rv) && !cachedCharset.IsEmpty())
  {
    aCharset = cachedCharset;
    aCharsetSource = kCharsetFromCache;

    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool
nsHTMLDocument::TryBookmarkCharset(nsIDocShell* aDocShell,
                                   nsIChannel* aChannel,
                                   PRInt32& aCharsetSource,
                                   nsACString& aCharset)
{
  if (kCharsetFromBookmarks <= aCharsetSource) {
    return PR_TRUE;
  }

  if (!gRDF) {
    return PR_FALSE;
  }

  nsCOMPtr<nsIRDFDataSource>  datasource;
  nsresult rv = gRDF->GetDataSource("rdf:bookmarks",
                                    getter_AddRefs(datasource));

  if (NS_FAILED(rv)) {
    return PR_FALSE;
  }

  nsCOMPtr<nsICharsetResolver> bookmarksResolver =
    do_QueryInterface(datasource);
  
  if (bookmarksResolver && aDocShell && aChannel) {

    PRBool wantCharset;         // ignored for now
    nsCAutoString charset;
    rv = bookmarksResolver->RequestCharset(aDocShell,
                                           aChannel,
                                           &aCharsetSource,
                                           &wantCharset,
                                           nsnull,
                                           charset);
    if (NS_SUCCEEDED(rv) && !charset.IsEmpty()) {
      aCharset = charset;
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

PRBool
nsHTMLDocument::TryParentCharset(nsIDocumentCharsetInfo*  aDocInfo,
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
    else if (kCharsetFromHintPrevDoc == parentSource)
      // if parent is posted doc, set this prevent autodections
      source = kCharsetFromHintPrevDoc;
    else if (kCharsetFromCache <= parentSource)
      source = kCharsetFromParentFrame;
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

nsresult
nsHTMLDocument::StartDocumentLoad(const char* aCommand,
                                  nsIChannel* aChannel,
                                  nsILoadGroup* aLoadGroup,
                                  nsISupports* aContainer,
                                  nsIStreamListener **aDocListener,
                                  PRBool aReset,
                                  nsIContentSink* aSink)
{
  nsCAutoString contentType;
  aChannel->GetContentType(contentType);

  if (contentType.Equals("application/xhtml+xml") &&
      (!aCommand || nsCRT::strcmp(aCommand, "view-source") != 0)) {
    // We're parsing XHTML as XML, remember that.

    mDefaultNamespaceID = kNameSpaceID_XHTML;
    mCompatMode = eCompatibility_FullStandards;
  }
#ifdef DEBUG
  else {
    NS_ASSERTION(mDefaultNamespaceID == kNameSpaceID_None,
                 "Hey, someone forgot to reset mDefaultNamespaceID!!!");
  }
#endif

  PRBool needsParser = PR_TRUE;
  if (aCommand)
  {
    if (!nsCRT::strcmp(aCommand, "view delayedContentLoad")) {
      needsParser = PR_FALSE;
    }
  }

  nsCOMPtr<nsICacheEntryDescriptor> cacheDescriptor;
  nsresult rv = nsDocument::StartDocumentLoad(aCommand,
                                              aChannel, aLoadGroup,
                                              aContainer,
                                              aDocListener, aReset);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Store the security info for future use with wyciwyg channels.
  aChannel->GetSecurityInfo(getter_AddRefs(mSecurityInfo));

  // Stash away a pointer to our channel (we need this for cookies)
  mChannel = aChannel;

  nsCOMPtr<nsIURI> uri;
  rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsICachingChannel> cachingChan = do_QueryInterface(aChannel);
  if (cachingChan) {
    nsCOMPtr<nsISupports> cacheToken;
    cachingChan->GetCacheToken(getter_AddRefs(cacheToken));
    if (cacheToken)
      cacheDescriptor = do_QueryInterface(cacheToken);
  }

  if (needsParser) {
    mParser = do_CreateInstance(kCParserCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_PRECONDITION(nsnull != aContainer, "No content viewer container");

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aContainer));

  nsCOMPtr<nsIDocumentCharsetInfo> dcInfo;
  docShell->GetDocumentCharsetInfo(getter_AddRefs(dcInfo));
  nsCOMPtr<nsIPresContext> cx;
  docShell->GetPresContext(getter_AddRefs(cx));
  if(cx){
    PRUint32 mBidiOption;
    cx->GetBidi(&mBidiOption);
    mTexttype = GET_BIDI_OPTION_TEXTTYPE(mBidiOption);
  }
  //
  // The following logic is mirrored in nsWebShell::Embed!
  //
  nsCOMPtr<nsIMarkupDocumentViewer> muCV;
  nsCOMPtr<nsIContentViewer> cv;
  docShell->GetContentViewer(getter_AddRefs(cv));
  if (cv) {
     muCV = do_QueryInterface(cv);
  } else {
    // in this block of code, if we get an error result, we return it
    // but if we get a null pointer, that's perfectly legal for parent
    // and parentContentViewer
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));
    NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
    docShellAsItem->GetSameTypeParent(getter_AddRefs(parentAsItem));

    nsCOMPtr<nsIDocShell> parent(do_QueryInterface(parentAsItem));
    if (parent) {
      nsCOMPtr<nsIContentViewer> parentContentViewer;
      rv = parent->GetContentViewer(getter_AddRefs(parentContentViewer));
      if (NS_FAILED(rv)) { return rv; }
      if (parentContentViewer) {
        muCV = do_QueryInterface(parentContentViewer);
      }
    }
  }

  nsCAutoString scheme;
  uri->GetScheme(scheme);

  nsCAutoString urlSpec;
  uri->GetSpec(urlSpec);

  PRInt32 charsetSource;
  nsCAutoString charset;

  if (IsXHTML()) {
    charsetSource = kCharsetFromDocTypeDefault;
    charset.AssignLiteral("UTF-8");
    TryChannelCharset(aChannel, charsetSource, charset);
  } else {
    charsetSource = kCharsetUninitialized;

    // The following charset resolving calls has implied knowledge
    // about charset source priority order. Each try will return true
    // if the source is higher or equal to the source as its name
    // describes. Some try call might change charset source to
    // multiple values, like TryHintCharset and TryParentCharset. It
    // should be always safe to try more sources.
    if (!TryUserForcedCharset(muCV, dcInfo, charsetSource, charset)) {
      TryHintCharset(muCV, charsetSource, charset);
      TryParentCharset(dcInfo, charsetSource, charset);
      if (TryChannelCharset(aChannel, charsetSource, charset)) {
        // Use the channel's charset (e.g., charset from HTTP
        // "Content-Type" header).
      }
      else if (!scheme.EqualsLiteral("about") &&          // don't try to access bookmarks for about:blank
               TryBookmarkCharset(docShell, aChannel, charsetSource, charset)) {
        // Use the bookmark's charset.
      }
      else if (cacheDescriptor && !urlSpec.IsEmpty() &&
               TryCacheCharset(cacheDescriptor, charsetSource, charset)) {
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

    if(kCharsetFromAutoDetection > charsetSource && !isPostPage) {
      StartAutodetection(docShell, charset, aCommand);
    }

    // ahmed
    // Check if 864 but in Implicit mode !
    if ((mTexttype == IBMBIDI_TEXTTYPE_LOGICAL) &&
        (charset.LowerCaseEqualsLiteral("ibm864"))) {
      charset.AssignLiteral("IBM864i");
    }
  }

  SetDocumentCharacterSet(charset);
  SetDocumentCharacterSetSource(charsetSource);

  // set doc charset to muCV for next document.
  if (muCV)
    muCV->SetPrevDocCharacterSet(charset);

  if(cacheDescriptor) {
    rv = cacheDescriptor->SetMetaDataElement("charset",
                                             charset.get());
    NS_ASSERTION(NS_SUCCEEDED(rv),"cannot SetMetaDataElement");
  }

  // Set the parser as the stream listener for the document loader...
  if (mParser) {
    rv = CallQueryInterface(mParser, aDocListener);
    if (NS_FAILED(rv)) {
      return rv;
    }

#ifdef DEBUG_charset
    char* cCharset = ToNewCString(charset);
    printf("set to parser charset = %s source %d\n",
          cCharset, charsetSource);
          Recycle(cCharset);
#endif
    mParser->SetDocumentCharset(charset, charsetSource);
    mParser->SetCommand(aCommand);

    // create the content sink
    nsCOMPtr<nsIContentSink> sink;

    if (aSink)
      sink = aSink;
    else {
      if (IsXHTML()) {
        nsCOMPtr<nsIXMLContentSink> xmlsink;
        rv = NS_NewXMLContentSink(getter_AddRefs(xmlsink), this, uri,
                                  docShell, aChannel);

        sink = xmlsink;
      } else {
        nsCOMPtr<nsIHTMLContentSink> htmlsink;

        rv = NS_NewHTMLContentSink(getter_AddRefs(htmlsink), this, uri,
                                   docShell, aChannel);

        sink = htmlsink;
      }
      NS_ENSURE_SUCCESS(rv, rv);

      NS_ASSERTION(sink,
                   "null sink with successful result from factory method");
    }

    mParser->SetContentSink(sink);
    // parser the content of the URI
    mParser->Parse(uri, nsnull, PR_FALSE, (void *)this);
  }

  return rv;
}

void
nsHTMLDocument::StopDocumentLoad()
{
  if (mParser) {
    mParser->Terminate();
  }
}

// static
void
nsHTMLDocument::DocumentWriteTerminationFunc(nsISupports *aRef)
{
  nsIDocument *doc = NS_REINTERPRET_CAST(nsIDocument *, aRef);
  nsHTMLDocument *htmldoc = NS_REINTERPRET_CAST(nsHTMLDocument *, doc);

  // If the document is in the middle of a document.write() call, this
  // most likely means that script on a page document.write()'d out a
  // script tag that did location="..." and we're right now finishing
  // up executing the script that was written with
  // document.write(). Since there's still script on the stack (the
  // script that called document.write()) we don't want to release the
  // parser now, that would cause the next document.write() call to
  // cancel the load that was initiated by the location="..." in the
  // script that was written out by document.write().

  if (!htmldoc->mIsWriting) {
    // Release the documents parser so that the call to EndLoad()
    // doesn't just return early and set the termination function again.

    htmldoc->mParser = nsnull;
  }

  htmldoc->EndLoad();
}

void
nsHTMLDocument::EndLoad()
{
  if (mParser) {
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

          scx->SetTerminationFunction(DocumentWriteTerminationFunc,
                                      NS_STATIC_CAST(nsIDocument *, this));

          return;
        }
      }
    }
  }

  mParser = nsnull;
  nsDocument::EndLoad();
}

NS_IMETHODIMP
nsHTMLDocument::SetTitle(const nsAString& aTitle)
{
  return nsDocument::SetTitle(aTitle);
}

nsresult
nsHTMLDocument::AddImageMap(nsIDOMHTMLMapElement* aMap)
{
  // XXX We should order the maps based on their order in the document.
  // XXX Otherwise scripts that add/remove maps with duplicate names
  // XXX will cause problems
  NS_PRECONDITION(nsnull != aMap, "null ptr");
  if (nsnull == aMap) {
    return NS_ERROR_NULL_POINTER;
  }
  if (mImageMaps.AppendObject(aMap)) {
    return NS_OK;
  }
  return NS_ERROR_OUT_OF_MEMORY;
}

void
nsHTMLDocument::RemoveImageMap(nsIDOMHTMLMapElement* aMap)
{
  NS_PRECONDITION(nsnull != aMap, "null ptr");
  mImageMaps.RemoveObject(aMap);
}

nsIDOMHTMLMapElement *
nsHTMLDocument::GetImageMap(const nsAString& aMapName)
{
  nsAutoString name;
  PRUint32 i, n = mImageMaps.Count();

  for (i = 0; i < n; ++i) {
    nsIDOMHTMLMapElement *map = mImageMaps[i];
    NS_ASSERTION(map, "Null map in map list!");

    PRBool match;
    nsresult rv;

    if (IsXHTML()) {
      rv = map->GetId(name);

      match = name.Equals(aMapName);
    } else {
      rv = map->GetName(name);

      match = name.Equals(aMapName, nsCaseInsensitiveStringComparator());
    }

    if (match && NS_SUCCEEDED(rv)) {
      return map;
    }
  }

  return nsnull;
}

// subclass hooks for sheet ordering
void
nsHTMLDocument::InternalAddStyleSheet(nsIStyleSheet* aSheet, PRUint32 aFlags)
{
  if (aSheet == mAttrStyleSheet) {  // always first
    NS_ASSERTION(mStyleSheets.Count() == 0 ||
                 mAttrStyleSheet != mStyleSheets[0],
                 "Adding attr sheet twice!");
    mStyleSheets.InsertObjectAt(aSheet, 0);
  }
  else if (aSheet == mStyleAttrStyleSheet) {  // always last
    NS_ASSERTION(mStyleSheets.Count() == 0 ||
                 mStyleSheets[mStyleSheets.Count() - 1] != mStyleAttrStyleSheet,
                 "Adding style attr sheet twice!");
    mStyleSheets.AppendObject(aSheet);
  }
  else {
    PRInt32 count = mStyleSheets.Count();
    if (count != 0 && mStyleAttrStyleSheet == mStyleSheets[count - 1]) {
      // keep attr sheet last
      mStyleSheets.InsertObjectAt(aSheet, count - 1);
    }
    else {
      mStyleSheets.AppendObject(aSheet);
    }
  }
}

void
nsHTMLDocument::InternalInsertStyleSheetAt(nsIStyleSheet* aSheet,
                                           PRInt32 aIndex)
{
  NS_ASSERTION(0 <= aIndex &&
               aIndex <= (
                          mStyleSheets.Count()
                          /* Don't count Attribute stylesheet */
                          - 1
                          /* No insertion allowed after StyleAttr stylesheet */
                          - ((mStyleAttrStyleSheet &&
                              mStyleSheets.Count() > 0 &&
                              mStyleAttrStyleSheet ==
                                mStyleSheets[mStyleSheets.Count() - 1]) ? 1: 0)
                          ),
               "index out of bounds");

  // offset one for the attr style sheet
  mStyleSheets.InsertObjectAt(aSheet, aIndex + 1);
}

nsIStyleSheet*
nsHTMLDocument::InternalGetStyleSheetAt(PRInt32 aIndex) const
{
  PRInt32 count = InternalGetNumberOfStyleSheets();
  if (aIndex >= 0 && aIndex < count) {
    return mStyleSheets[aIndex + 1];
  } else {
    NS_ERROR("Index out of range");
    return nsnull;
  }
}

PRInt32
nsHTMLDocument::InternalGetNumberOfStyleSheets() const
{
  PRInt32 count = mStyleSheets.Count();
  if (count != 0 && mStyleAttrStyleSheet == mStyleSheets[count - 1])
    --count;
  --count; // for the attr sheet
  NS_ASSERTION(count >= 0, "Why did we end up with a negative count?");
  return count;
}

nsICSSLoader*
nsHTMLDocument::GetCSSLoader()
{
  if (!mCSSLoader) {
    NS_NewCSSLoader(this, getter_AddRefs(mCSSLoader));
    if (mCSSLoader) {
      mCSSLoader->SetCaseSensitive(IsXHTML());
      mCSSLoader->SetCompatibilityMode(mCompatMode);
    }
  }
  
  return mCSSLoader;
}


nsCompatibility
nsHTMLDocument::GetCompatibilityMode()
{
  return mCompatMode;
}

void
nsHTMLDocument::SetCompatibilityMode(nsCompatibility aMode)
{
  NS_ASSERTION(!IsXHTML() || aMode == eCompatibility_FullStandards,
               "Bad compat mode for XHTML document!");

  mCompatMode = aMode;
  if (mCSSLoader) {
    mCSSLoader->SetCompatibilityMode(mCompatMode);
  }
  nsCOMPtr<nsIPresShell> shell = (nsIPresShell*)mPresShells.SafeElementAt(0);
  if (shell) {
    nsCOMPtr<nsIPresContext> pc;
    shell->GetPresContext(getter_AddRefs(pc));
    if (pc) {
      pc->SetCompatibilityMode(mCompatMode);
    }
  }
}

void
nsHTMLDocument::ContentAppended(nsIContent* aContainer,
                                PRInt32 aNewIndexInContainer)
{
  NS_ABORT_IF_FALSE(aContainer, "Null container!");

  // Register new content. That is the content numbered from
  // aNewIndexInContainer and upwards.
  PRUint32 count = aContainer->GetChildCount();

  for (PRUint32 i = aNewIndexInContainer; i < count; ++i) {
    RegisterNamedItems(aContainer->GetChildAt(i));
  }

  nsDocument::ContentAppended(aContainer, aNewIndexInContainer);
}

void
nsHTMLDocument::ContentInserted(nsIContent* aContainer, nsIContent* aContent,
                                PRInt32 aIndexInContainer)
{
  NS_ABORT_IF_FALSE(aContent, "Null content!");

  nsresult rv = RegisterNamedItems(aContent);

  if (NS_FAILED(rv)) {
    return;
  }

  nsDocument::ContentInserted(aContainer, aContent, aIndexInContainer);
}

void
nsHTMLDocument::ContentRemoved(nsIContent* aContainer, nsIContent* aContent,
                               PRInt32 aIndexInContainer)
{
  NS_ABORT_IF_FALSE(aContent, "Null content!");

  if (aContainer == mRootContent) {
    // Reset mBodyContent in case we got a new body.

    mBodyContent = nsnull;
  }

  nsresult rv = UnregisterNamedItems(aContent);

  if (NS_FAILED(rv)) {
    return;
  }

  nsDocument::ContentRemoved(aContainer, aContent, aIndexInContainer);
}

void
nsHTMLDocument::AttributeWillChange(nsIContent* aContent, PRInt32 aNameSpaceID,
                                    nsIAtom* aAttribute)
{
  NS_ABORT_IF_FALSE(aContent, "Null content!");
  NS_PRECONDITION(aAttribute, "Must have an attribute that's changing!");

  if (!IsXHTML() && aAttribute == nsHTMLAtoms::name &&
      aNameSpaceID == kNameSpaceID_None) {
    nsAutoString value;

    if (IsNamedItem(aContent, aContent->Tag(), value)) {
      nsresult rv = RemoveFromNameTable(value, aContent);

      if (NS_FAILED(rv)) {
        return;
      }
    }
  } else if (aAttribute == aContent->GetIDAttributeName() &&
             aNameSpaceID == kNameSpaceID_None) {
    nsresult rv = RemoveFromIdTable(aContent);

    if (NS_FAILED(rv)) {
      return;
    }
  }

  nsDocument::AttributeWillChange(aContent, aNameSpaceID, aAttribute);
}

void
nsHTMLDocument::AttributeChanged(nsIContent* aContent, PRInt32 aNameSpaceID,
                                 nsIAtom* aAttribute, PRInt32 aModType)
{
  NS_ABORT_IF_FALSE(aContent, "Null content!");
  NS_PRECONDITION(aAttribute, "Must have an attribute that's changing!");

  if (!IsXHTML() && aAttribute == nsHTMLAtoms::name &&
      aNameSpaceID == kNameSpaceID_None) {
    nsAutoString value;

    if (IsNamedItem(aContent, aContent->Tag(), value)) {
      nsresult rv = UpdateNameTableEntry(value, aContent);

      if (NS_FAILED(rv)) {
        return;
      }
    }
  } else if (aAttribute == aContent->GetIDAttributeName() &&
             aNameSpaceID == kNameSpaceID_None) {
    nsAutoString value;

    aContent->GetAttr(aNameSpaceID,
                      aContent->GetIDAttributeName(),
                      value);

    if (!value.IsEmpty()) {
      nsresult rv = AddToIdTable(value, aContent);

      if (NS_FAILED(rv)) {
        return;
      }
    }
  }

  nsDocument::AttributeChanged(aContent, aNameSpaceID, aAttribute, aModType);
}

void
nsHTMLDocument::FlushPendingNotifications(mozFlushType aType)
{
  if (aType & Flush_Content) {
    // Determine if it is safe to flush the sink
    // by determining if it safe to flush all the presshells.
    PRBool isSafeToFlush = PR_TRUE;
    if (aType & Flush_SinkNotifications) {
      PRInt32 i = 0, n = mPresShells.Count();
      while ((i < n) && (isSafeToFlush)) {
        nsCOMPtr<nsIPresShell> shell =
          NS_STATIC_CAST(nsIPresShell*, mPresShells[i]);

        if (shell) {
          shell->IsSafeToFlush(isSafeToFlush);
        }
        ++i;
      }
    }

    if (isSafeToFlush && mParser) {
      nsCOMPtr<nsIContentSink> sink = mParser->GetContentSink();
      if (sink) {
        PRBool notify = ((aType & Flush_SinkNotifications) != 0);
        sink->FlushContent(notify);
      }
    }
  }
  
  nsDocument::FlushPendingNotifications(aType);
}

PRBool
nsHTMLDocument::IsCaseSensitive()
{
  return IsXHTML();
}

//
// nsIDOMDocument interface implementation
//
NS_IMETHODIMP
nsHTMLDocument::CreateElement(const nsAString& aTagName,
                              nsIDOMElement** aReturn)
{
  *aReturn = nsnull;
  nsresult rv;

  // if we are in quirks, don't validate the tag name
  if (mCompatMode != eCompatibility_NavQuirks) {
    rv = nsContentUtils::CheckQName(aTagName, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }  

  nsAutoString tmp(aTagName);
  if (!IsXHTML()) {
    ToLowerCase(tmp);
  }

  nsCOMPtr<nsIAtom> name = do_GetAtom(tmp);

  nsCOMPtr<nsIContent> content;
  rv = CreateElem(name, nsnull, GetDefaultNamespaceID(), PR_TRUE,
                  getter_AddRefs(content));
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(content, aReturn);
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
  if (IsXHTML()) {
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
  if (IsXHTML()) {
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
  if (IsXHTML()) {
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
  nsAutoString tmp(aTagname);
  ToLowerCase(tmp); // HTML elements are lower case internally.
  return nsDocument::GetElementsByTagName(tmp, aReturn);
}

NS_IMETHODIMP
nsHTMLDocument::GetBaseURI(nsAString &aURI)
{
  aURI.Truncate();
  nsIURI *uri = mDocumentBaseURI; // WEAK

  if (!uri) {
    uri = mDocumentURI;
  }

  if (uri) {
    nsCAutoString spec;
    uri->GetSpec(spec);

    CopyUTF8toUTF16(spec, aURI);
  }

  return NS_OK;
}

// nsIDOM3Document interface implementation
NS_IMETHODIMP
nsHTMLDocument::GetXmlEncoding(nsAString& aXmlEncoding)
{
  if (IsXHTML()) {
    return nsDocument::GetXmlEncoding(aXmlEncoding);
  }

  SetDOMStringToNull(aXmlEncoding);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetXmlStandalone(PRBool *aXmlStandalone)
{
  if (IsXHTML()) {
    return nsDocument::GetXmlStandalone(aXmlStandalone);
  }

  *aXmlStandalone = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetXmlStandalone(PRBool aXmlStandalone)
{
  if (IsXHTML()) {
    return nsDocument::SetXmlStandalone(aXmlStandalone);
  }

  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}


NS_IMETHODIMP
nsHTMLDocument::GetXmlVersion(nsAString& aXmlVersion)
{
  if (IsXHTML()) {
    return nsDocument::GetXmlVersion(aXmlVersion);
  }

  SetDOMStringToNull(aXmlVersion);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetXmlVersion(const nsAString& aXmlVersion)
{
  if (IsXHTML()) {
    return nsDocument::SetXmlVersion(aXmlVersion);
  }

  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
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
  *aURI = nsnull;

  nsIPrincipal *principal = GetPrincipal();
  if (!principal)
    return;

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
  // Check new domain - must be a superdomain of the current host
  // For example, a page from foo.bar.com may set domain to bar.com,
  // but not to ar.com, baz.com, or fi.foo.bar.com.
  if (aDomain.IsEmpty())
    return NS_ERROR_DOM_BAD_DOCUMENT_DOMAIN;
  nsAutoString current;
  if (NS_FAILED(GetDomain(current)))
    return NS_ERROR_FAILURE;
  PRBool ok = PR_FALSE;
  if (current.Equals(aDomain)) {
    ok = PR_TRUE;
  } else if (aDomain.Length() < current.Length()) {
    nsAutoString suffix;
    current.Right(suffix, aDomain.Length());
    PRUnichar c = current.CharAt(current.Length() - aDomain.Length() - 1);
    if (suffix.Equals(aDomain, nsCaseInsensitiveStringComparator()) &&
        (c == '.'))
      ok = PR_TRUE;
  }
  if (!ok) {
    // Error: illegal domain
    return NS_ERROR_DOM_BAD_DOCUMENT_DOMAIN;
  }

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

  nsresult rv = mPrincipal->SetDomain(newURI);

  // Bug 13871: Frameset spoofing - note that document.domain was set
  if (NS_SUCCEEDED(rv)) {
    mDomainWasSet = PR_TRUE;
  }

  return rv;
}

PRBool
nsHTMLDocument::WasDomainSet()
{
  return mDomainWasSet;
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

NS_IMETHODIMP
nsHTMLDocument::GetBody(nsIDOMHTMLElement** aBody)
{
  NS_ENSURE_ARG_POINTER(aBody);
  *aBody = nsnull;

  nsISupports* element = nsnull;
  nsCOMPtr<nsIDOMNode> node;

  if (mBodyContent || GetBodyContent()) {
    // There is a body element, return that as the body.
    element = mBodyContent;
  } else {
    // The document is most likely a frameset document so look for the
    // outer most frameset element

    nsCOMPtr<nsIDOMNodeList> nodeList;

    nsresult rv;
    if (IsXHTML()) {
      rv = GetElementsByTagNameNS(NS_LITERAL_STRING("http://www.w3.org/1999/xhtml"),
                                  NS_LITERAL_STRING("frameset"),
                                  getter_AddRefs(nodeList));
    } else {
      rv = GetElementsByTagName(NS_LITERAL_STRING("frameset"),
                                getter_AddRefs(nodeList));
    }

    if (nodeList) {
      rv |= nodeList->Item(0, getter_AddRefs(node));

      element = node;
    }

    NS_ENSURE_SUCCESS(rv, rv);
  }

  return element ? CallQueryInterface(element, aBody) : NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetBody(nsIDOMHTMLElement* aBody)
{
  nsCOMPtr<nsIContent> body(do_QueryInterface(aBody));
  nsCOMPtr<nsIDOMElement> root(do_QueryInterface(mRootContent));

  // The body element must be either a body tag or a frameset tag.
  if (!body || !root || !(body->Tag() == nsHTMLAtoms::body ||
                          body->Tag() == nsHTMLAtoms::frameset)) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  nsCOMPtr<nsIDOMNode> tmp;

  if (mBodyContent || GetBodyContent()) {
    root->ReplaceChild(aBody, mBodyContent, getter_AddRefs(tmp));
  } else {
    root->AppendChild(aBody, getter_AddRefs(tmp));
  }

  mBodyContent = aBody;

  return PR_FALSE;
}

NS_IMETHODIMP
nsHTMLDocument::GetImages(nsIDOMHTMLCollection** aImages)
{
  if (!mImages) {
    mImages = new nsContentList(this, nsHTMLAtoms::img, mDefaultNamespaceID);
    if (!mImages) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  *aImages = mImages;
  NS_ADDREF(*aImages);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetApplets(nsIDOMHTMLCollection** aApplets)
{
  if (!mApplets) {
    mApplets = new nsContentList(this, nsHTMLAtoms::applet,
                                 mDefaultNamespaceID);
    if (!mApplets) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  *aApplets = mApplets;
  NS_ADDREF(*aApplets);

  return NS_OK;
}

PRInt32
GetHTMLDocumentNamespace(nsIContent *aContent)
{
  NS_ASSERTION(aContent->GetDocument(),
               "This method should never be called on content nodes "
               "that are not in a document!");

#ifdef DEBUG
  {
    nsCOMPtr<nsIHTMLDocument> htmldoc(do_QueryInterface(aContent->GetDocument()));

    if (!htmldoc) {
      NS_ERROR("Huh, how did this happen? This should only be used with "
               "HTML documents!");
    }
  }
#endif

  return aContent->GetDocument()->IsCaseSensitive() ?
    kNameSpaceID_XHTML : kNameSpaceID_None;
}

PRBool
nsHTMLDocument::MatchLinks(nsIContent *aContent, PRInt32 aNamespaceID,
                           nsIAtom* aAtom, const nsAString& aData)
{
  nsINodeInfo *ni = aContent->GetNodeInfo();

  if (ni) {
    PRInt32 namespaceID = GetHTMLDocumentNamespace(aContent);

    if (ni->Equals(nsHTMLAtoms::a, namespaceID) ||
        ni->Equals(nsHTMLAtoms::area, namespaceID)) {
      return aContent->HasAttr(kNameSpaceID_None, nsHTMLAtoms::href);
    }
  }

  return PR_FALSE;
}

NS_IMETHODIMP
nsHTMLDocument::GetLinks(nsIDOMHTMLCollection** aLinks)
{
  if (!mLinks) {
    mLinks = new nsContentList(this, MatchLinks, EmptyString());
    if (!mLinks) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  *aLinks = mLinks;
  NS_ADDREF(*aLinks);

  return NS_OK;
}

PRBool
nsHTMLDocument::MatchAnchors(nsIContent *aContent, PRInt32 aNamespaceID,
                             nsIAtom* aAtom, const nsAString& aData)
{
  nsINodeInfo *ni = aContent->GetNodeInfo();

  if (ni) {
    PRInt32 namespaceID = GetHTMLDocumentNamespace(aContent);

    if (ni->Equals(nsHTMLAtoms::a, namespaceID)) {
      return aContent->HasAttr(kNameSpaceID_None, nsHTMLAtoms::name);
    }
  }

  return PR_FALSE;
}

NS_IMETHODIMP
nsHTMLDocument::GetAnchors(nsIDOMHTMLCollection** aAnchors)
{
  if (!mAnchors) {
    mAnchors = new nsContentList(this, MatchAnchors, EmptyString());
    if (!mAnchors) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
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

  // not having a cookie service isn't an error
  nsCOMPtr<nsICookieService> service = do_GetService(kCookieServiceCID);
  if (service) {
    // Get a URI from the document principal. We use the original
    // codebase in case the codebase was changed by SetDomain
    nsCOMPtr<nsIURI> codebaseURI;
    mPrincipal->GetURI(getter_AddRefs(codebaseURI));

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
  // not having a cookie service isn't an error
  nsCOMPtr<nsICookieService> service = do_GetService(kCookieServiceCID);
  if (service && mDocumentURI) {
    nsCOMPtr<nsIPrompt> prompt;
    nsCOMPtr<nsIDOMWindowInternal> window =
      do_QueryInterface(GetScriptGlobalObject());
    if (window) {
      window->GetPrompter(getter_AddRefs(prompt));
    }

    nsCOMPtr<nsIURI> codebaseURI;
    mPrincipal->GetURI(getter_AddRefs(codebaseURI));

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

// static
nsresult
nsHTMLDocument::GetSourceDocumentURI(nsIURI** sourceURI)
{
  // XXX Tom said this reminded him of the "Six Degrees of
  // Kevin Bacon" game. We try to get from here to there using
  // whatever connections possible. The problem is that this
  // could break if any of the connections along the way change.
  // I wish there were a better way.
  *sourceURI = nsnull;

  // XXX This will fail on non-DOM contexts :(
  nsIDOMDocument *domDoc = nsContentUtils::GetDocumentFromCaller();

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  if (!doc) {
    return NS_OK; // No document in the window
  }

  NS_IF_ADDREF(*sourceURI = doc->GetDocumentURI());

  return sourceURI ? NS_OK : NS_ERROR_FAILURE;
}

// XXX TBI: accepting arguments to the open method.
nsresult
nsHTMLDocument::OpenCommon(nsIURI* aSourceURI, const nsACString& aContentType,
                           PRBool aReplace)
{
  // If we already have a parser we ignore the document.open call.
  if (mParser) {
    if (IsXHTML()) {
      // No calling document.open() while we're parsing XHTML

      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }

    return NS_OK;
  }

  nsCOMPtr<nsIDocument> callingDoc =
    do_QueryInterface(nsContentUtils::GetDocumentFromCaller());

  // Grab a reference to the calling documents security info (if any)
  // as it may be lost in the call to Reset().
  nsCOMPtr<nsISupports> securityInfo;

  if (callingDoc) {
    securityInfo = callingDoc->GetSecurityInfo();
  }

  nsCOMPtr<nsIDocShell> docshell = do_QueryReferent(mDocumentContainer);
  nsresult rv = NS_OK;

  // Stop current loads targeted at the window this document is in.
  if (mScriptGlobalObject && docshell) {
    nsCOMPtr<nsIContentViewer> cv;
    docshell->GetContentViewer(getter_AddRefs(cv));

    if (cv) {
      PRBool okToUnload;
      rv = cv->PermitUnload(&okToUnload);

      if (NS_SUCCEEDED(rv) && !okToUnload) {
        // We don't want to unload, so stop here, but don't throw an
        // exception.
        return NS_OK;
      }
    }

    nsCOMPtr<nsIWebNavigation> webnav(do_QueryInterface(docshell));
    webnav->Stop(nsIWebNavigation::STOP_NETWORK);
  }

  // The open occurred after the document finished loading.
  // So we reset the document and create a new one.
  nsCOMPtr<nsIChannel> channel;
  nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);

  rv = NS_NewChannel(getter_AddRefs(channel), aSourceURI, nsnull, group);

  if (NS_FAILED(rv)) {
    return rv;
  }

  // Before we reset the doc notify the globalwindow of the change.

  if (mScriptGlobalObject) {
    // Hold onto ourselves on the offchance that we're down to one ref

    nsCOMPtr<nsIDOMDocument> kungFuDeathGrip =
      do_QueryInterface((nsIHTMLDocument*)this);

    rv = mScriptGlobalObject->SetNewDocument(kungFuDeathGrip, PR_FALSE,
                                             PR_FALSE);

    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // XXX This is a nasty workaround for a scrollbar code bug
  // (http://bugzilla.mozilla.org/show_bug.cgi?id=55334).

  // Hold on to our root element
  nsCOMPtr<nsIContent> root(mRootContent);

  if (root) {
    PRUint32 count = root->GetChildCount();

    // Remove all the children from the root.
    while (count-- > 0) {
      root->RemoveChildAt(count, PR_TRUE);
    }

    count = mRootContent->GetAttrCount();

    // Remove all attributes from the root element
    while (count-- > 0) {
      nsCOMPtr<nsIAtom> name, prefix;
      PRInt32 nsid;

      root->GetAttrNameAt(count, &nsid, getter_AddRefs(name),
                          getter_AddRefs(prefix));

      root->UnsetAttr(nsid, name, PR_FALSE);
    }

    // Remove the root from the childlist
    mChildren.RemoveObject(root);

    mRootContent = nsnull;
  }

  // Call Reset(), this will now do the full reset, except removing
  // the root from the document, doing that confuses the scrollbar
  // code in mozilla since the document in the root element and all
  // the anonymous content (i.e. scrollbar elements) is set to
  // null.

  Reset(channel, group);

  if (root) {
    // Tear down the frames for the root element.
    ContentRemoved(nsnull, root, 0);

    // Put the root element back into the document, we don't notify
    // the document about this insertion since the sink will do that
    // for us, the sink will call InitialReflow() and that'll create
    // frames for the root element and the scrollbars work as expected
    // (since the document in the root element was never set to null)

    mChildren.AppendObject(root);
    mRootContent = root;
  }

  // Store the security info of the caller now that we're done
  // resetting the document.
  mSecurityInfo = securityInfo;

  mParser = do_CreateInstance(kCParserCID, &rv);

  // This will be propagated to the parser when someone actually calls write()
  mContentType = aContentType;

  mIsWriting = 1;

  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIHTMLContentSink> sink;

    rv = NS_NewHTMLContentSink(getter_AddRefs(sink), this, aSourceURI,
                               docshell, channel);
    NS_ENSURE_SUCCESS(rv, rv);

    static NS_DEFINE_CID(kNavDTDCID, NS_CNAVDTD_CID);
    nsCOMPtr<nsIDTD> theDTD(do_CreateInstance(kNavDTDCID));
    if(theDTD) {
      mParser->RegisterDTD(theDTD);
    }

    mParser->SetContentSink(sink);
  }

  // Prepare the docshell and the document viewer for the impending
  // out of band document.write()
  if (docshell) {
    docshell->PrepareForNewContentModel();

    // Now check whether we were opened with a "replace" argument.  If
    // so, we need to tell the docshell to not create a new history
    // entry for this load.
    // XXXbz we're basically duplicating the MAKE_LOAD_TYPE macro from
    // nsDocShell.h.  All this stuff needs better apis.
    PRUint32 loadType;
    if (aReplace) {
      loadType = nsIDocShell::LOAD_CMD_NORMAL |
        (nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY << 16);
    } else {
      // Make sure that we're doing a normal load, not whatever type
      // of load was previously done on this docshell.
      loadType = nsIDocShell::LOAD_CMD_NORMAL |
        (nsIWebNavigation::LOAD_FLAGS_NONE << 16);
    }
    docshell->SetLoadType(loadType);
    
    nsCOMPtr<nsIContentViewer> cv;
    docshell->GetContentViewer(getter_AddRefs(cv));
    nsCOMPtr<nsIDocumentViewer> docViewer = do_QueryInterface(cv);
    if (docViewer) {
      docViewer->LoadStart(NS_STATIC_CAST(nsIHTMLDocument *, this));
    }
  }

  // Add a wyciwyg channel request into the document load group
  NS_ASSERTION(!mWyciwygChannel, "nsHTMLDocument::OpenCommon(): wyciwyg "
               "channel already exists!");
  CreateAndAddWyciwygChannel();

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
  // XXX The URI of the newly created document will match
  // that of the source document. Is this right?

  // XXX This will fail on non-DOM contexts :(
  nsCOMPtr<nsIURI> sourceURI;
  nsresult rv = GetSourceDocumentURI(getter_AddRefs(sourceURI));

  // Recover if we had a problem obtaining the source URI
  if (!sourceURI) {
    rv = NS_NewURI(getter_AddRefs(sourceURI),
                   NS_LITERAL_CSTRING("about:blank"));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = OpenCommon(sourceURI, aContentType, aReplace);
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(this, aReturn);
}

#define NS_GENERATE_PARSER_KEY() NS_INT32_TO_PTR((mIsWriting << 31) | (mWriteLevel & 0x7fffffff))

NS_IMETHODIMP
nsHTMLDocument::Clear()
{
  // This method has been deprecated
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::Close()
{
  nsresult rv = NS_OK;

  if (mParser && mIsWriting) {
    ++mWriteLevel;
    if (mContentType.EqualsLiteral("text/html")) {
      rv = mParser->Parse(NS_LITERAL_STRING("</HTML>"),
                          NS_GENERATE_PARSER_KEY(),
                          mContentType, PR_FALSE,
                          PR_TRUE);
    }
    --mWriteLevel;
    mIsWriting = 0;
    mParser = nsnull;

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
    FlushPendingNotifications(Flush_Layout);

    // Remove the wyciwyg channel request from the document load group
    // that we added in OpenCommon().  If all other requests between
    // document.open() and document.close() have completed, then this
    // method should cause the firing of an onload event.
    NS_ASSERTION(mWyciwygChannel, "nsHTMLDocument::Close(): Trying to remove "
                 "non-existent wyciwyg channel!");
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
  nsresult rv = NS_OK;

  if (!mParser) {
    rv = Open();

    // If Open() fails, or if it didn't create a parser (as it won't
    // if the user chose to not discard the current document through
    // onbeforeunload), don't write anything.
    if (NS_FAILED(rv) || !mParser) {
      return rv;
    }
  } else if (IsXHTML()) {
    // No calling document.write*() while parsing XHTML!

    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  static NS_NAMED_LITERAL_STRING(new_line, "\n");

  // Save the data in cache
  if (mWyciwygChannel) {
    mWyciwygChannel->WriteToCacheEntry(aText);

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
                        NS_GENERATE_PARSER_KEY(),
                        mContentType, PR_FALSE,
                        (!mIsWriting || (mWriteLevel > 1)));
  } else {
    rv = mParser->Parse(aText,
                        NS_GENERATE_PARSER_KEY(),
                        mContentType, PR_FALSE,
                        (!mIsWriting || (mWriteLevel > 1)));
  }

  --mWriteLevel;

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

nsresult
nsHTMLDocument::ScriptWriteCommon(PRBool aNewlineTerminate)
{
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID()));

  nsCOMPtr<nsIXPCNativeCallContext> ncc;

  nsresult rv = NS_OK;

  if (xpc) {
    rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(ncc));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCAutoString spec;

  if (mDocumentURI) {
    rv = mDocumentURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!mDocumentURI || nsCRT::strcasecmp(spec.get(), "about:blank") == 0) {
    // The current document's URI and principal are empty or "about:blank".
    // By writing to this document, the script acquires responsibility for the
    // document for security purposes. Thus a document.write of a script tag
    // ends up producing a script with the same principals as the script
    // that performed the write.
    nsIScriptSecurityManager *secMan = nsContentUtils::GetSecurityManager();

    nsCOMPtr<nsIPrincipal> subject;
    rv = secMan->GetSubjectPrincipal(getter_AddRefs(subject));
    NS_ENSURE_SUCCESS(rv, rv);

    if (subject) {
      nsCOMPtr<nsIURI> subjectURI;
      subject->GetURI(getter_AddRefs(subjectURI));

      if (subjectURI) {
        mDocumentURI = subjectURI;
        mPrincipal = subject;
      }
    }
  }

  if (ncc) {
    // We're called from JS, concatenate the extra arguments into
    // string_buffer
    PRUint32 i, argc;

    ncc->GetArgc(&argc);

    JSContext *cx = nsnull;
    rv = ncc->GetJSContext(&cx);
    NS_ENSURE_SUCCESS(rv, rv);

    jsval *argv = nsnull;
    ncc->GetArgvPtr(&argv);
    NS_ENSURE_TRUE(argv, NS_ERROR_UNEXPECTED);

    if (argc == 1) {
      JSString *jsstr = JS_ValueToString(cx, argv[0]);
      NS_ENSURE_TRUE(jsstr, NS_ERROR_OUT_OF_MEMORY);

      nsDependentString str(NS_REINTERPRET_CAST(const PRUnichar *,
                                              ::JS_GetStringChars(jsstr)),
                          ::JS_GetStringLength(jsstr));

      return WriteCommon(str, aNewlineTerminate);
    }

    if (argc > 1) {
      nsAutoString string_buffer;

      for (i = 0; i < argc; ++i) {
        JSString *str = JS_ValueToString(cx, argv[i]);
        NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

        string_buffer.Append(NS_REINTERPRET_CAST(const PRUnichar *,
                                                 ::JS_GetStringChars(str)),
                             ::JS_GetStringLength(str));
      }

      return WriteCommon(string_buffer, aNewlineTerminate);
    }
  }

  // No arguments...
  return WriteCommon(EmptyString(), aNewlineTerminate);
}

NS_IMETHODIMP
nsHTMLDocument::Write()
{
  return ScriptWriteCommon(PR_FALSE);
}

NS_IMETHODIMP
nsHTMLDocument::Writeln()
{
  return ScriptWriteCommon(PR_TRUE);
}

NS_IMETHODIMP
nsHTMLDocument::GetElementById(const nsAString& aElementId,
                               nsIDOMElement** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  IdAndNameMapEntry *entry =
    NS_STATIC_CAST(IdAndNameMapEntry *,
                   PL_DHashTableOperate(&mIdAndNameHashTable, &aElementId,
                                        PL_DHASH_ADD));
  NS_ENSURE_TRUE(entry, NS_ERROR_OUT_OF_MEMORY);

  nsIContent *e = entry->mIdContent;

  if (e == ID_NOT_IN_DOCUMENT) {
    // We've looked for this id before and we didn't find it, so it
    // won't be in the document now either (since the
    // mIdAndNameHashTable is live for entries in the table)

    return NS_OK;
  } else if (!e) {
    NS_WARN_IF_FALSE(!aElementId.IsEmpty(),
                     "getElementById(\"\") called, fix caller?");

    if (mRootContent && !aElementId.IsEmpty()) {
      e = MatchElementId(mRootContent, NS_ConvertUCS2toUTF8(aElementId), aElementId);
    }

    if (!e) {
      // There is no element with the given id in the document, cache
      // the fact that it's not in the document
      entry->mIdContent = ID_NOT_IN_DOCUMENT;

      return NS_OK;
    }

    // We found an element with a matching id, store that in the hash
    entry->mIdContent = e;
  }

  return CallQueryInterface(e, aReturn);
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
  nsAutoString tmp(aLocalName);

  if (!IsXHTML()) {
    ToLowerCase(tmp); // HTML elements are lower case internally.
  }

  return nsDocument::GetElementsByTagNameNS(aNamespaceURI, tmp, aReturn);
}

PRBool
nsHTMLDocument::MatchNameAttribute(nsIContent* aContent, PRInt32 aNamespaceID,
                                   nsIAtom* aAtom, const nsAString& aData)
{
  NS_PRECONDITION(aContent, "Must have content node to work with!");
  
  // Getting attrs is expensive, so use HasAttr() first.
  if (!aContent->HasAttr(kNameSpaceID_None, nsHTMLAtoms::name)) {
    return PR_FALSE;
  }

  nsAutoString value;
  nsresult rv = aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::name, value);

  return NS_SUCCEEDED(rv) && value.Equals(aData);
}

NS_IMETHODIMP
nsHTMLDocument::GetElementsByName(const nsAString& aElementName,
                                  nsIDOMNodeList** aReturn)
{
  nsContentList* elements = new nsContentList(this,
                                              MatchNameAttribute,
                                              aElementName);
  NS_ENSURE_TRUE(elements, NS_ERROR_OUT_OF_MEMORY);

  *aReturn = elements;
  NS_ADDREF(*aReturn);

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

PRBool
nsHTMLDocument::MatchFormControls(nsIContent* aContent, PRInt32 aNamespaceID,
                                  nsIAtom* aAtom, const nsAString& aData)
{
  return aContent->IsContentOfType(nsIContent::eHTML_FORM_CONTROL);
}

already_AddRefed<nsIDOMNodeList>
nsHTMLDocument::GetFormControlElements()
{
  nsIDOMNodeList *list = new nsContentList(this, MatchFormControls,
                                           EmptyString());
  NS_IF_ADDREF(list);

  return list;
}

nsresult
nsHTMLDocument::GetPixelDimensions(nsIPresShell* aShell,
                                   PRInt32* aWidth,
                                   PRInt32* aHeight)
{
  *aWidth = *aHeight = 0;

  FlushPendingNotifications(Flush_Layout);

  // Find the <body> element: this is what we'll want to use for the
  // document's width and height values.
  if (!mBodyContent && !GetBodyContent()) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> body = do_QueryInterface(mBodyContent);

  // Now grab its frame
  nsIFrame* frame;
  nsresult rv = aShell->GetPrimaryFrameFor(body, &frame);
  if (NS_SUCCEEDED(rv) && frame) {
    nsSize                    size;
    nsCOMPtr<nsIPresContext>  presContext;

    aShell->GetPresContext(getter_AddRefs(presContext));
    nsIView* view = frame->GetView();

    // If we have a view check if it's scrollable. If not,
    // just use the view size itself
    if (view) {
      nsIScrollableView* scrollableView = nsnull;
      CallQueryInterface(view, &scrollableView);

      if (scrollableView) {
        scrollableView->GetScrolledView(view);
      }

      nsRect r = view->GetBounds();
      size.height = r.height;
      size.width = r.width;
    }
    // If we don't have a view, use the frame size
    else {
      size = frame->GetSize();
    }

    // Convert from twips to pixels
    nsCOMPtr<nsIPresContext> context;
    rv = aShell->GetPresContext(getter_AddRefs(context));

    if (NS_SUCCEEDED(rv)) {
      float scale;
      scale = context->TwipsToPixels();

      *aWidth = NSTwipsToIntPixels(size.width, scale);
      *aHeight = NSTwipsToIntPixels(size.height, scale);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetWidth(PRInt32* aWidth)
{
  NS_ENSURE_ARG_POINTER(aWidth);
  *aWidth = 0;

  // We make the assumption that the first presentation shell
  // is the one for which we need information.
  nsIPresShell *shell = GetShellAt(0);
  if (!shell) {
    return NS_OK;
  }

  PRInt32 dummy;

  // GetPixelDimensions() does the flushing for us, no need to flush
  // here too
  return GetPixelDimensions(shell, aWidth, &dummy);
}

NS_IMETHODIMP
nsHTMLDocument::GetHeight(PRInt32* aHeight)
{
  NS_ENSURE_ARG_POINTER(aHeight);
  *aHeight = 0;

  // We make the assumption that the first presentation shell
  // is the one for which we need information.
  nsIPresShell *shell = GetShellAt(0);
  if (!shell) {
    return NS_OK;
  }

  PRInt32 dummy;

  // GetPixelDimensions() does the flushing for us, no need to flush
  // here too
  return GetPixelDimensions(shell, &dummy, aHeight);
}

NS_IMETHODIMP
nsHTMLDocument::GetAlinkColor(nsAString& aAlinkColor)
{
  aAlinkColor.Truncate();

  nsCOMPtr<nsIDOMHTMLBodyElement> body;
  GetBodyElement(getter_AddRefs(body));

  if (body) {
    body->GetALink(aAlinkColor);
  } else if (mAttrStyleSheet) {
    nscolor color;
    nsresult rv = mAttrStyleSheet->GetActiveLinkColor(color);
    if (NS_SUCCEEDED(rv)) {
      NS_RGBToHex(color, aAlinkColor);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetAlinkColor(const nsAString& aAlinkColor)
{
  nsCOMPtr<nsIDOMHTMLBodyElement> body;
  GetBodyElement(getter_AddRefs(body));

  if (body) {
    body->SetALink(aAlinkColor);
  } else if (mAttrStyleSheet) {
    nsAttrValue value;
    if (value.ParseColor(aAlinkColor, this)) {
      nscolor color;
      value.GetColorValue(color);
      mAttrStyleSheet->SetActiveLinkColor(color);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetLinkColor(nsAString& aLinkColor)
{
  aLinkColor.Truncate();

  nsCOMPtr<nsIDOMHTMLBodyElement> body;
  GetBodyElement(getter_AddRefs(body));

  if (body) {
    body->GetLink(aLinkColor);
  } else if (mAttrStyleSheet) {
    nscolor color;
    nsresult rv = mAttrStyleSheet->GetLinkColor(color);
    if (NS_SUCCEEDED(rv)) {
      NS_RGBToHex(color, aLinkColor);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetLinkColor(const nsAString& aLinkColor)
{
  nsCOMPtr<nsIDOMHTMLBodyElement> body;
  GetBodyElement(getter_AddRefs(body));

  if (body) {
    body->SetLink(aLinkColor);
  } else if (mAttrStyleSheet) {
    nsAttrValue value;
    if (value.ParseColor(aLinkColor, this)) {
      nscolor color;
      value.GetColorValue(color);
      mAttrStyleSheet->SetLinkColor(color);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetVlinkColor(nsAString& aVlinkColor)
{
  aVlinkColor.Truncate();

  nsCOMPtr<nsIDOMHTMLBodyElement> body;
  GetBodyElement(getter_AddRefs(body));

  if (body) {
    body->GetVLink(aVlinkColor);
  } else if (mAttrStyleSheet) {
    nscolor color;
    nsresult rv = mAttrStyleSheet->GetVisitedLinkColor(color);
    if (NS_SUCCEEDED(rv)) {
      NS_RGBToHex(color, aVlinkColor);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetVlinkColor(const nsAString& aVlinkColor)
{
  nsCOMPtr<nsIDOMHTMLBodyElement> body;
  GetBodyElement(getter_AddRefs(body));

  if (body) {
    body->SetVLink(aVlinkColor);
  } else if (mAttrStyleSheet) {
    nsAttrValue value;
    if (value.ParseColor(aVlinkColor, this)) {
      nscolor color;
      value.GetColorValue(color);
      mAttrStyleSheet->SetVisitedLinkColor(color);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetBgColor(nsAString& aBgColor)
{
  aBgColor.Truncate();

  nsCOMPtr<nsIDOMHTMLBodyElement> body;
  GetBodyElement(getter_AddRefs(body));

  if (body) {
    body->GetBgColor(aBgColor);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetBgColor(const nsAString& aBgColor)
{
  nsCOMPtr<nsIDOMHTMLBodyElement> body;
  GetBodyElement(getter_AddRefs(body));

  if (body) {
    body->SetBgColor(aBgColor);
  }
  // XXXldb And otherwise?

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetFgColor(nsAString& aFgColor)
{
  aFgColor.Truncate();

  nsCOMPtr<nsIDOMHTMLBodyElement> body;
  GetBodyElement(getter_AddRefs(body));

  if (body) {
    body->GetText(aFgColor);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetFgColor(const nsAString& aFgColor)
{
  nsCOMPtr<nsIDOMHTMLBodyElement> body;
  GetBodyElement(getter_AddRefs(body));

  if (body) {
    body->SetText(aFgColor);
  }
  // XXXldb And otherwise?

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLDocument::GetEmbeds(nsIDOMHTMLCollection** aEmbeds)
{
  if (!mEmbeds) {
    mEmbeds = new nsContentList(this, nsHTMLAtoms::embed, mDefaultNamespaceID);
    if (!mEmbeds) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  *aEmbeds = mEmbeds;
  NS_ADDREF(*aEmbeds);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetSelection(nsAString& aReturn)
{
  aReturn.Truncate();

  nsCOMPtr<nsIConsoleService> consoleService
    (do_GetService("@mozilla.org/consoleservice;1"));

  if (consoleService) {
    consoleService->LogStringMessage(NS_LITERAL_STRING("Deprecated method document.getSelection() called.  Please use window.getSelection() instead.").get());
  }

  nsCOMPtr<nsIPresShell> shell = (nsIPresShell*)mPresShells.SafeElementAt(0);

  if (!shell) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresContext> cx;

  shell->GetPresContext(getter_AddRefs(cx));
  NS_ENSURE_TRUE(cx, NS_OK);

  nsCOMPtr<nsISupports> container = cx->GetContainer();
  NS_ENSURE_TRUE(container, NS_OK);

  nsCOMPtr<nsIDOMWindow> window(do_GetInterface(container));
  NS_ENSURE_TRUE(window, NS_OK);

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
  nsIEventListenerManager *manager;

  if (NS_OK == GetListenerManager(&manager)) {
    manager->CaptureEvent(aEventFlags);
    NS_RELEASE(manager);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsHTMLDocument::ReleaseEvents(PRInt32 aEventFlags)
{
  nsIEventListenerManager *manager;

  if (NS_OK == GetListenerManager(&manager)) {
    manager->ReleaseEvent(aEventFlags);
    NS_RELEASE(manager);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsHTMLDocument::RouteEvent(nsIDOMEvent* aEvt)
{
  //XXX Not the best solution -joki
  return NS_OK;
}

// readonly attribute DOMString compatMode;
// Returns "BackCompat" if we are in quirks mode, "CSS1Compat" if we are
// in almost standards or full standards mode. See bug 105640.  This was
// implemented to match MSIE's compatMode property
NS_IMETHODIMP
nsHTMLDocument::GetCompatMode(nsAString& aCompatMode)
{
  aCompatMode.Truncate();
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

PR_STATIC_CALLBACK(PLDHashOperator)
IdAndNameMapEntryRemoveCallback(PLDHashTable *table, PLDHashEntryHdr *hdr,
                                PRUint32 number, void *arg)
{
  return PL_DHASH_REMOVE;
}


void
nsHTMLDocument::InvalidateHashTables()
{
  PL_DHashTableEnumerate(&mIdAndNameHashTable, IdAndNameMapEntryRemoveCallback,
                         nsnull);
}

static nsresult
ReserveNameInHash(const nsAString& aName, PLDHashTable *aHash)
{
  IdAndNameMapEntry *entry =
    NS_STATIC_CAST(IdAndNameMapEntry *,
                   PL_DHashTableOperate(aHash, &aName, PL_DHASH_ADD));

  NS_ENSURE_TRUE(entry, NS_ERROR_OUT_OF_MEMORY);

  entry->mContentList = NAME_NOT_VALID;

  return NS_OK;
}

// Pre-fill the name hash with names that are likely to be resolved in
// this document to avoid walking the tree looking for elements with
// these names.

nsresult
nsHTMLDocument::PrePopulateHashTables()
{
  nsresult rv = NS_OK;

  rv = ReserveNameInHash(NS_LITERAL_STRING("write"), &mIdAndNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ReserveNameInHash(NS_LITERAL_STRING("writeln"), &mIdAndNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ReserveNameInHash(NS_LITERAL_STRING("open"), &mIdAndNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ReserveNameInHash(NS_LITERAL_STRING("close"), &mIdAndNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ReserveNameInHash(NS_LITERAL_STRING("forms"), &mIdAndNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ReserveNameInHash(NS_LITERAL_STRING("elements"), &mIdAndNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ReserveNameInHash(NS_LITERAL_STRING("characterSet"),
                          &mIdAndNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ReserveNameInHash(NS_LITERAL_STRING("nodeType"), &mIdAndNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ReserveNameInHash(NS_LITERAL_STRING("parentNode"),
                          &mIdAndNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ReserveNameInHash(NS_LITERAL_STRING("cookie"), &mIdAndNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

static PRBool
IsNamedItem(nsIContent* aContent, nsIAtom *aTag, nsAString& aName)
{
  // Only the content types reflected in Level 0 with a NAME
  // attribute are registered. Images, layers and forms always get
  // reflected up to the document. Applets and embeds only go
  // to the closest container (which could be a form).
  if (aTag == nsHTMLAtoms::img    ||
      aTag == nsHTMLAtoms::form   ||
      aTag == nsHTMLAtoms::applet ||
      aTag == nsHTMLAtoms::embed  ||
      aTag == nsHTMLAtoms::object) {
    aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::name, aName);

    if (!aName.IsEmpty()) {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

nsresult
nsHTMLDocument::UpdateNameTableEntry(const nsAString& aName,
                                     nsIContent *aContent)
{
  NS_ASSERTION(!IsXHTML(), "Don't call me on an XHTML document!!!");

  IdAndNameMapEntry *entry =
    NS_STATIC_CAST(IdAndNameMapEntry *,
                   PL_DHashTableOperate(&mIdAndNameHashTable, &aName,
                                        PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_FREE(entry)) {
    return NS_OK;
  }

  nsBaseContentList *list = entry->mContentList;

  if (!list || list == NAME_NOT_VALID) {
    return NS_OK;
  }

  // NOTE: this indexof is absolutely needed, since we don't flush
  // content notifications when we do document.foo resolution.  So
  // aContent may be in our list already and just now getting notified
  // for!
  if (list->IndexOf(aContent, PR_FALSE) < 0) {
    list->AppendElement(aContent);
  }

  return NS_OK;
}

nsresult
nsHTMLDocument::AddToIdTable(const nsAString& aId, nsIContent *aContent)
{
  IdAndNameMapEntry *entry =
    NS_STATIC_CAST(IdAndNameMapEntry *,
                   PL_DHashTableOperate(&mIdAndNameHashTable, &aId,
                                        PL_DHASH_ADD));
  NS_ENSURE_TRUE(entry, NS_ERROR_OUT_OF_MEMORY);

  const nsIContent *e = entry->mIdContent;

  if (!e || e == ID_NOT_IN_DOCUMENT) {
    entry->mIdContent = aContent;
  }

  return NS_OK;
}

nsresult
nsHTMLDocument::UpdateIdTableEntry(const nsAString& aId, nsIContent *aContent)
{
  IdAndNameMapEntry *entry =
    NS_STATIC_CAST(IdAndNameMapEntry *,
                   PL_DHashTableOperate(&mIdAndNameHashTable, &aId,
                                        PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
    entry->mIdContent = aContent;
  }

  return NS_OK;
}

nsresult
nsHTMLDocument::RemoveFromNameTable(const nsAString& aName,
                                    nsIContent *aContent)
{
  NS_ASSERTION(!IsXHTML(), "Don't call me on an XHTML document!!!");

  IdAndNameMapEntry *entry =
    NS_STATIC_CAST(IdAndNameMapEntry *,
                   PL_DHashTableOperate(&mIdAndNameHashTable, &aName,
                                        PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_BUSY(entry) && entry->mContentList &&
      entry->mContentList != NAME_NOT_VALID) {
    entry->mContentList->RemoveElement(aContent);
  }

  return NS_OK;
}

nsresult
nsHTMLDocument::RemoveFromIdTable(nsIContent *aContent)
{
  nsIAtom* idAttr = aContent->GetIDAttributeName();
  
  if (!idAttr || !aContent->HasAttr(kNameSpaceID_None, idAttr)) {
    return NS_OK;
  }

  nsAutoString value;
  aContent->GetAttr(kNameSpaceID_None, idAttr, value);

  if (value.IsEmpty()) {
    return NS_OK;
  }

  IdAndNameMapEntry *entry =
    NS_STATIC_CAST(IdAndNameMapEntry *,
                   PL_DHashTableOperate(&mIdAndNameHashTable,
                                        NS_STATIC_CAST(const nsAString *,
                                                       &value),
                                        PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_FREE(entry) || entry->mIdContent != aContent) {
    return NS_OK;
  }

  PL_DHashTableRawRemove(&mIdAndNameHashTable, entry);

  return NS_OK;
}

nsresult
nsHTMLDocument::UnregisterNamedItems(nsIContent *aContent)
{
  nsIAtom *tag = aContent->Tag();

  if (tag == nsLayoutAtoms::textTagName) {
    // Text nodes are not named items nor can they have children.

    return NS_OK;
  }

  nsAutoString value;
  nsresult rv = NS_OK;

  if (!IsXHTML() && IsNamedItem(aContent, tag, value)) {
    rv = RemoveFromNameTable(value, aContent);

    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  rv = RemoveFromIdTable(aContent);

  if (NS_FAILED(rv)) {
    return rv;
  }

  PRUint32 i, count = aContent->GetChildCount();

  for (i = 0; i < count; ++i) {
    UnregisterNamedItems(aContent->GetChildAt(i));
  }

  return NS_OK;
}

nsresult
nsHTMLDocument::RegisterNamedItems(nsIContent *aContent)
{
  nsIAtom *tag = aContent->Tag();

  if (tag == nsLayoutAtoms::textTagName) {
    // Text nodes are not named items nor can they have children.

    return NS_OK;
  }

  nsAutoString value;

  if (!IsXHTML() && IsNamedItem(aContent, tag, value)) {
    UpdateNameTableEntry(value, aContent);
  }

  nsIAtom* idAttr = aContent->GetIDAttributeName();
  if (idAttr) {
    aContent->GetAttr(kNameSpaceID_None, idAttr, value);
    if (!value.IsEmpty()) {
      nsresult rv = UpdateIdTableEntry(value, aContent);
      
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  PRUint32 i, count = aContent->GetChildCount();

  for (i = 0; i < count; ++i) {
    RegisterNamedItems(aContent->GetChildAt(i));
  }

  return NS_OK;
}

static void
FindNamedItems(const nsAString& aName, nsIContent *aContent,
               IdAndNameMapEntry& aEntry, PRBool aIsXHTML)
{
  NS_ASSERTION(aEntry.mContentList,
               "Entry w/o content list passed to FindNamedItems()!");
  NS_ASSERTION(aEntry.mContentList != NAME_NOT_VALID,
               "Entry that should never have a list passed to FindNamedItems()!");

  nsIAtom *tag = aContent->Tag();

  if (tag == nsLayoutAtoms::textTagName) {
    // Text nodes are not named items nor can they have children.

    return;
  }

  nsAutoString value;

  if (!aIsXHTML && IsNamedItem(aContent, tag, value) && value.Equals(aName)) {
    aEntry.mContentList->AppendElement(aContent);
  }

  if (!aEntry.mIdContent) {
    nsIAtom* idAttr = aContent->GetIDAttributeName();
    if (idAttr) {
      aContent->GetAttr(kNameSpaceID_None, idAttr, value);

      if (value.Equals(aName)) {
        aEntry.mIdContent = aContent;
      }
    }
  }

  PRUint32 i, count = aContent->GetChildCount();

  for (i = 0; i < count; ++i) {
    FindNamedItems(aName, aContent->GetChildAt(i), aEntry, aIsXHTML);
  }
}

nsresult
nsHTMLDocument::ResolveName(const nsAString& aName,
                            nsIDOMHTMLFormElement *aForm,
                            nsISupports **aResult)
{
  *aResult = nsnull;

  if (IsXHTML()) {
    // We don't dynamically resolve names on XHTML documents.

    return NS_OK;
  }

  // We have built a table and cache the named items. The table will
  // be updated as content is added and removed.

  IdAndNameMapEntry *entry =
    NS_STATIC_CAST(IdAndNameMapEntry *,
                   PL_DHashTableOperate(&mIdAndNameHashTable, &aName,
                                        PL_DHASH_ADD));
  NS_ENSURE_TRUE(entry, NS_ERROR_OUT_OF_MEMORY);

  if (entry->mContentList == NAME_NOT_VALID) {
    // There won't be any named items by this name -- it's reserved
    return NS_OK;
  }

  // Now we know we _might_ have items.  Before looking at
  // entry->mContentList, make sure to flush out content (see
  // bug 69826).
  // This is a perf killer while the document is loading!

  // Make sure to stash away the current generation so we can check whether the
  // table changes when we flush.
  PRUint32 generation = mIdAndNameHashTable.generation;
  
  FlushPendingNotifications(Flush_Content);

  if (generation != mIdAndNameHashTable.generation) {
    // Table changed, so the entry pointer is no longer valid; look up the
    // entry again, adding if necessary (the adding may be necessary in case
    // the flush actually deleted entries).
    entry =
      NS_STATIC_CAST(IdAndNameMapEntry *,
                     PL_DHashTableOperate(&mIdAndNameHashTable, &aName,
                                          PL_DHASH_ADD));
    NS_ENSURE_TRUE(entry, NS_ERROR_OUT_OF_MEMORY);
  }
    

  nsBaseContentList *list = entry->mContentList;

  if (!list) {
#ifdef DEBUG_jst
    {
      printf ("nsHTMLDocument name cache miss for name '%s'\n",
              NS_ConvertUCS2toUTF8(aName).get());
    }
#endif

    list = new nsBaseContentList();
    NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);

    entry->mContentList = list;
    NS_ADDREF(entry->mContentList);

    if (mRootContent && !aName.IsEmpty()) {
      // We'll never get here if !IsXHTML(), so we can just pass
      // PR_FALSE to FindNamedItems().
      FindNamedItems(aName, mRootContent, *entry, PR_FALSE);
    }
  }

  PRUint32 length;
  list->GetLength(&length);

  if (length > 0) {
    if (length == 1) {
      // Only one element in the list, return the element instead of
      // returning the list

      nsCOMPtr<nsIDOMNode> node;

      list->Item(0, getter_AddRefs(node));

      nsCOMPtr<nsIContent> ourContent(do_QueryInterface(node));
      if (aForm && ourContent &&
          !nsContentUtils::BelongsInForm(aForm, ourContent)) {
        // This is not the content you are looking for
        node = nsnull;
      }

      *aResult = node;
      NS_IF_ADDREF(*aResult);

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

        nsCOMPtr<nsIDOMNode> node;

        fc_list->Item(0, getter_AddRefs(node));

        NS_IF_ADDREF(*aResult = node);

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
  // the id aName, it'll be in entry->mIdContent.

  nsIContent *e = entry->mIdContent;

  if (e && e != ID_NOT_IN_DOCUMENT && e->IsContentOfType(nsIContent::eHTML)) {
    nsIAtom *tag = e->Tag();

    if ((tag == nsHTMLAtoms::embed  ||
         tag == nsHTMLAtoms::img    ||
         tag == nsHTMLAtoms::object ||
         tag == nsHTMLAtoms::applet) &&
        (!aForm || nsContentUtils::BelongsInForm(aForm, e))) {
      NS_ADDREF(*aResult = e);
    }
  }

  return NS_OK;
}

//----------------------------

PRBool
nsHTMLDocument::GetBodyContent()
{
  if (!mRootContent) {
    return PR_FALSE;
  }

  PRUint32 i, child_count = mRootContent->GetChildCount();

  for (i = 0; i < child_count; ++i) {
    nsIContent *child = mRootContent->GetChildAt(i);
    NS_ENSURE_TRUE(child, NS_ERROR_UNEXPECTED);

    if (child->IsContentOfType(nsIContent::eHTML) &&
        child->GetNodeInfo()->Equals(nsHTMLAtoms::body,
                                     mDefaultNamespaceID)) {
      mBodyContent = do_QueryInterface(child);

      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

void
nsHTMLDocument::GetBodyElement(nsIDOMHTMLBodyElement** aBody)
{
  *aBody = nsnull;

  if (!mBodyContent && !GetBodyContent()) {
    // No body in this document.

    return;
  }

  CallQueryInterface(mBodyContent, aBody);
}

// forms related stuff

NS_IMETHODIMP
nsHTMLDocument::GetForms(nsIDOMHTMLCollection** aForms)
{
  if (!mForms) {
    mForms = new nsContentList(this, nsHTMLAtoms::form, mDefaultNamespaceID);
    if (!mForms) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  *aForms = mForms;
  NS_ADDREF(*aForms);

  return NS_OK;
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
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to add request to load group.");
  }

  return rv;
}

nsresult
nsHTMLDocument::RemoveWyciwygChannel(void)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsILoadGroup> loadGroup = GetDocumentLoadGroup();

  // note there can be a write request without a load group if
  // this is a synchronously constructed about:blank document
  if (loadGroup && mWyciwygChannel) {
    mWyciwygChannel->CloseCacheEntry(NS_OK);
    rv = loadGroup->RemoveRequest(mWyciwygChannel, nsnull, NS_OK);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mWyciwygChannel = nsnull;

  return rv;
}

/* attribute DOMString designMode; */
NS_IMETHODIMP
nsHTMLDocument::GetDesignMode(nsAString & aDesignMode)
{
  if (mEditingIsOn) {
    aDesignMode.AssignLiteral("on");
  }
  else {
    aDesignMode.AssignLiteral("off");
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetDesignMode(const nsAString & aDesignMode)
{
  // get editing session
  if (!mScriptGlobalObject)
    return NS_ERROR_FAILURE;

  nsIDocShell *docshell = mScriptGlobalObject->GetDocShell();
  if (!docshell)
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCAutoString url;
  mDocumentURI->GetSpec(url);
  // test if the above works if document.domain is set for Midas document
  // (www.netscape.com --> netscape.com)
  if (!url.Equals("about:blank")) {
    // If we're 'about:blank' then we don't care who can edit us.
    // If we're not about:blank, then we need to check sameOrigin.
    rv = nsContentUtils::GetSecurityManager()->CheckSameOrigin(nsnull,
                                                               mDocumentURI);
    if (NS_FAILED(rv))
      return rv;
  }

  nsCOMPtr<nsIEditingSession> editSession = do_GetInterface(docshell);
  if (!editSession)
    return NS_ERROR_FAILURE;

  if (aDesignMode.LowerCaseEqualsLiteral("on")) {
    // go through hoops to get dom window (see nsHTMLDocument::GetSelection)
    nsCOMPtr<nsIPresShell> shell = (nsIPresShell*)mPresShells.SafeElementAt(0);
    NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);

    nsCOMPtr<nsIPresContext> cx;
    shell->GetPresContext(getter_AddRefs(cx));
    NS_ENSURE_TRUE(cx, NS_ERROR_FAILURE);

    nsCOMPtr<nsISupports> container = cx->GetContainer();
    NS_ENSURE_TRUE(container, NS_OK);

    // get content root frame
    nsCOMPtr<nsIDOMWindow> domwindow(do_GetInterface(container));
    NS_ENSURE_TRUE(domwindow, NS_ERROR_FAILURE);

    rv = editSession->MakeWindowEditable(domwindow, "html", PR_FALSE);
    if (NS_FAILED(rv))
      return rv;

    // now that we've successfully created the editor, we can reset our flag
    mEditingIsOn = PR_TRUE;
  }
  else { // turn editing off
    // right now we don't have a way to turn off editing  :-(
    mEditingIsOn = PR_FALSE;
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
  if (!mScriptGlobalObject)
    return NS_ERROR_FAILURE;

  nsIDocShell *docshell = mScriptGlobalObject->GetDocShell();
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
  { "useCSS",        "cmd_setDocumentUseCSS",   "", PR_FALSE, PR_TRUE },
  { "readonly",      "cmd_setDocumentReadOnly", "", PR_FALSE, PR_TRUE },
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

struct MidasParam {
  const char*  incomingParamString;
  const char*  internalParamString;
};

static const struct MidasParam gMidasParamTable[] = {
  { "<P>",                "P" },
  { "<H1>",               "H1" },
  { "<H2>",               "H2" },
  { "<H3>",               "H3" },
  { "<H4>",               "H4" },
  { "<H5>",               "H5" },
  { "<H6>",               "H6" },
  { "<PRE>",              "PRE" },
  { "<ADDRESS>",          "ADDRESS" },
  { NULL, NULL }
};

#define MidasParamCount ((sizeof(gMidasParamTable) / sizeof(struct MidasParam)) - 1)

// this function will return false if the command is not recognized
// inCommandID will be converted as necessary for internal operations
// inParam will be converted as necessary for internal operations
// outParam will be Empty if no parameter is needed or if returning a boolean
// outIsBoolean will determine whether to send param as a boolean or string
// outBooleanParam will not be set unless outIsBoolean
PRBool
nsHTMLDocument::ConvertToMidasInternalCommand(const nsAString & inCommandID,
                                              const nsAString & inParam,
                                              nsACString& outCommandID,
                                              nsACString& outParam,
                                              PRBool& outIsBoolean,
                                              PRBool& outBooleanValue)
{
  NS_ConvertUCS2toUTF8 convertedCommandID(inCommandID);
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

    if (gMidasCommandTable[i].useNewParam) {
      outParam.Assign(gMidasCommandTable[i].internalParamString);
    }
    else {
      // handle checking of param passed in
      NS_ConvertUCS2toUTF8 convertedParam(inParam);

      if (outIsBoolean) {
        // if this is a boolean value and it's not explicitly false
        // (e.g. no value) we default to "true"
        outBooleanValue = convertedParam.Equals("false",
                                   nsCaseInsensitiveCStringComparator());
        outParam.SetLength(0);
      }
      else {
        // check to see if we need to convert the parameter
        PRUint32 j;
        for (j = 0; j < MidasParamCount; ++j) {
          if (convertedParam.Equals(gMidasParamTable[j].incomingParamString,
                                    nsCaseInsensitiveCStringComparator())) {
            outParam.Assign(gMidasParamTable[j].internalParamString);
            break;
          }
        }

        // if we didn't convert the parameter, just
        // pass through the parameter that was passed to us
        if (j == MidasParamCount)
          outParam.Assign(convertedParam);
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

jsval
nsHTMLDocument::sCutCopyInternal_id = JSVAL_VOID;
jsval
nsHTMLDocument::sPasteInternal_id = JSVAL_VOID;

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

    NS_NAMED_LITERAL_CSTRING(classNameStr, "Clipboard");

    nsIScriptSecurityManager *secMan =
      nsContentUtils::GetSecurityManager();

    if (aPaste) {
      if (nsHTMLDocument::sPasteInternal_id == JSVAL_VOID) {
        nsHTMLDocument::sPasteInternal_id =
          STRING_TO_JSVAL(::JS_InternString(cx, "paste"));
      }
      rv = secMan->CheckPropertyAccess(cx, nsnull, classNameStr.get(),
                                       nsHTMLDocument::sPasteInternal_id,
                                       nsIXPCSecurityManager::ACCESS_GET_PROPERTY);
    } else {
      if (nsHTMLDocument::sCutCopyInternal_id == JSVAL_VOID) {
        nsHTMLDocument::sCutCopyInternal_id =
          STRING_TO_JSVAL(::JS_InternString(cx, "cutcopy"));
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
  if (!mEditingIsOn)
    return NS_ERROR_FAILURE;

  // if they are requesting UI from us, let's fail since we have no UI
  if (doShowUI)
    return NS_ERROR_NOT_IMPLEMENTED;

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

  nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(mScriptGlobalObject);
  if (!window)
    return NS_ERROR_FAILURE;

  nsCAutoString cmdToDispatch, paramStr;
  PRBool isBool, boolVal;
  if (!ConvertToMidasInternalCommand(commandID, value,
                                     cmdToDispatch, paramStr, isBool, boolVal))
    return NS_ERROR_NOT_IMPLEMENTED;

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
  if (!mEditingIsOn)
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
  if (!mEditingIsOn)
    return NS_ERROR_FAILURE;

  // get command manager and dispatch command to our window if it's acceptable
  nsCOMPtr<nsICommandManager> cmdMgr;
  GetMidasCommandManager(getter_AddRefs(cmdMgr));
  if (!cmdMgr)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(mScriptGlobalObject);
  if (!window)
    return NS_ERROR_FAILURE;

  nsCAutoString cmdToDispatch, paramStr;
  PRBool isBool, boolVal;
  if (!ConvertToMidasInternalCommand(commandID, commandID,
                                     cmdToDispatch, paramStr, isBool, boolVal))
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
  if (!mEditingIsOn)
    return NS_ERROR_FAILURE;

  // get command manager and dispatch command to our window if it's acceptable
  nsCOMPtr<nsICommandManager> cmdMgr;
  GetMidasCommandManager(getter_AddRefs(cmdMgr));
  if (!cmdMgr)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(mScriptGlobalObject);
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
  if (!mEditingIsOn)
    return NS_ERROR_FAILURE;

  // get command manager and dispatch command to our window if it's acceptable
  nsCOMPtr<nsICommandManager> cmdMgr;
  GetMidasCommandManager(getter_AddRefs(cmdMgr));
  if (!cmdMgr)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(mScriptGlobalObject);
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
  if (!mEditingIsOn)
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
  if (!mEditingIsOn)
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
  if (!mEditingIsOn)
    return NS_ERROR_FAILURE;

  // get command manager and dispatch command to our window if it's acceptable
  nsCOMPtr<nsICommandManager> cmdMgr;
  GetMidasCommandManager(getter_AddRefs(cmdMgr));
  if (!cmdMgr)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(mScriptGlobalObject);
  if (!window)
    return NS_ERROR_FAILURE;

  nsCAutoString cmdToDispatch, paramStr;
  PRBool isBool, boolVal;
  if (!ConvertToMidasInternalCommand(commandID, commandID,
                                     cmdToDispatch, paramStr, isBool, boolVal))
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

#ifdef DEBUG
nsresult
nsHTMLDocument::CreateElem(nsIAtom *aName, nsIAtom *aPrefix,
                           PRInt32 aNamespaceID, PRBool aDocumentDefaultType,
                           nsIContent** aResult)
{
  NS_ASSERTION(!aDocumentDefaultType || IsXHTML() ||
               aNamespaceID == kNameSpaceID_None,
               "HTML elements in an HTML document should have "
               "kNamespaceID_None as their namespace ID.");

  return nsDocument::CreateElem(aName, aPrefix, aNamespaceID,
                                aDocumentDefaultType, aResult);
}
#endif
