/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsICharsetAlias.h"

#include "nsCOMPtr.h"
#include "nsIFileChannel.h"
#include "nsXPIDLString.h"
#include "nsPrintfCString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsHTMLDocument.h"
#include "nsIParserFilter.h"
#include "nsIHTMLContentSink.h"
#include "nsHTMLParts.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsIStyleSet.h"
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
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsIIOService.h"
#include "nsNetUtil.h"
#include "nsIContentViewerContainer.h"
#include "nsIContentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIWebShell.h"
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
#include "nsICodebasePrincipal.h"
#include "nsIAggregatePrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScrollableView.h"

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
#include "nsIElementFactory.h"

#include "nsICharsetDetector.h"
#include "nsICharsetDetectionAdaptor.h"
#include "nsCharsetDetectionAdaptorCID.h"
#include "nsICharsetAlias.h"
#include "nsIPref.h"
#include "nsContentUtils.h"
#include "nsIDocumentCharsetInfo.h"
#include "nsIDocumentEncoder.h" //for outputting selection
#include "nsIBookmarksService.h"
#include "nsICachingChannel.h"
#include "nsICacheEntryDescriptor.h"
#include "nsIXMLContent.h" //for createelementNS
#include "nsHTMLParts.h" //for createelementNS
#include "nsIJSContextStack.h"
#include "nsContentUtils.h"
#include "nsIDocumentViewer.h"
#include "nsIWyciwygChannel.h"

#include "nsContentCID.h"
#include "nsIPrompt.h"
//AHMED 12-2 
#ifdef IBMBIDI
#include "nsBidiUtils.h"
#endif

#include "nsIEditingSession.h"

#define DETECTOR_CONTRACTID_MAX 127
static char g_detector_contractid[DETECTOR_CONTRACTID_MAX + 1];
static PRBool gInitDetector = PR_FALSE;
static PRBool gPlugDetector = PR_FALSE;
//static PRBool gBookmarkCharset = PR_TRUE;

#include "prmem.h"
#include "prtime.h"

// Find/Search Includes
const PRInt32 kForward  = 0;
const PRInt32 kBackward = 1;

//#define DEBUG_charset


#define ID_NOT_IN_DOCUMENT ((nsIContent *)1)

static NS_DEFINE_CID(kCookieServiceCID, NS_COOKIESERVICE_CID);

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

static PRBool
IsNamedItem(nsIContent* aContent, nsIAtom *aTag, nsAString& aName);

static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

static NS_DEFINE_CID(kHTMLStyleSheetCID,NS_HTMLSTYLESHEET_CID);

nsIRDFService* nsHTMLDocument::gRDF;
nsrefcnt       nsHTMLDocument::gRefCntRDFService = 0;

static int PR_CALLBACK
MyPrefChangedCallback(const char*aPrefName, void* instance_data)
{
        nsresult rv;
        nsCOMPtr<nsIPref> prefs = 
                 do_GetService(NS_PREF_CONTRACTID, &rv);
        PRUnichar* detector_name = nsnull;
        if(NS_SUCCEEDED(rv) && NS_SUCCEEDED(
             rv = prefs->GetLocalizedUnicharPref("intl.charset.detector",
                                     &detector_name)))
        {
			if(detector_name && *detector_name) {
				PL_strncpy(g_detector_contractid, NS_CHARSET_DETECTOR_CONTRACTID_BASE,DETECTOR_CONTRACTID_MAX);
				PL_strncat(g_detector_contractid, NS_ConvertUCS2toUTF8(detector_name).get(),DETECTOR_CONTRACTID_MAX);
				gPlugDetector = PR_TRUE;
			} else {
				g_detector_contractid[0]=0;
				gPlugDetector = PR_FALSE;
			}
            PR_FREEIF(detector_name);
        }
	return 0;
}

// ==================================================================
// =
// ==================================================================
NS_EXPORT nsresult
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
    NS_IF_RELEASE(mContentList);
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

  // An entry is being cleared, let the entry its own cleanup.
  e->~IdAndNameMapEntry();
}

PR_STATIC_CALLBACK(void)
IdAndNameHashInitEntry(PLDHashTable *table, PLDHashEntryHdr *entry,
                       const void *key)
{
  const nsAString *keyStr = NS_STATIC_CAST(const nsAString *, key);

  // Inititlize the entry with placement new
  new (entry) IdAndNameMapEntry(*keyStr);
}

nsHTMLDocument::nsHTMLDocument()
  : nsMarkupDocument(),
    mAttrStyleSheet(nsnull),
    mStyleAttrStyleSheet(nsnull),
    mBaseURL(nsnull),
    mBaseTarget(nsnull),
    mLastModified(nsnull),
    mReferrer(nsnull),
    mNumForms(0),
    mIsWriting(0),
    mDomainWasSet(PR_FALSE),
    mEditingIsOn(PR_FALSE)
{
  mImages = nsnull;
  mApplets = nsnull;
  mEmbeds = nsnull;
  mLinks = nsnull;
  mAnchors = nsnull;
  mLayers = nsnull;
  mParser = nsnull;
  mCompatMode = eCompatibility_NavQuirks;
  mCSSLoader = nsnull;

  mForms = nsnull;
  mIsWriting = 0;
  mWriteLevel = 0;
  mWyciwygSessionCnt = 0;

#ifdef IBMBIDI
  mTexttype = IBMBIDI_TEXTTYPE_LOGICAL;
#endif
  
  if (gRefCntRDFService++ == 0)
  {
    nsresult rv;    
		rv = nsServiceManager::GetService(kRDFServiceCID,
						  NS_GET_IID(nsIRDFService),
						  (nsISupports**) &gRDF);

    //nsCOMPtr<nsIRDFService> gRDF(do_GetService(kRDFServiceCID,
    //&rv));
  }
}

nsHTMLDocument::~nsHTMLDocument()
{
  NS_IF_RELEASE(mImages);
  NS_IF_RELEASE(mApplets);
  NS_IF_RELEASE(mEmbeds);
  NS_IF_RELEASE(mLinks);
  NS_IF_RELEASE(mAnchors);
  NS_IF_RELEASE(mLayers);
  if (mAttrStyleSheet) {
    mAttrStyleSheet->SetOwningDocument(nsnull);
  }
  if (mStyleAttrStyleSheet) {
    mStyleAttrStyleSheet->SetOwningDocument(nsnull);
  }
  NS_IF_RELEASE(mBaseURL);
  if (nsnull != mBaseTarget) {
    delete mBaseTarget;
    mBaseTarget = nsnull;
  }
  if (nsnull != mLastModified) {
    delete mLastModified;
    mLastModified = nsnull;
  }
  if (nsnull != mReferrer) {
    delete mReferrer;
    mReferrer = nsnull;
  }
  NS_IF_RELEASE(mParser);
  mImageMaps->Clear();
  NS_IF_RELEASE(mForms);
  if (mCSSLoader) {
    mCSSLoader->DropDocumentReference();  // release weak ref
  }

  if (--gRefCntRDFService == 0)
  {     
     nsServiceManager::ReleaseService("@mozilla.org/rdf/rdf-service;1", gRDF);
  }

  if (mIdAndNameHashIsLive) {
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
  NS_INTERFACE_MAP_ENTRY(nsIHTMLContentContainer)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLDocument)
NS_INTERFACE_MAP_END_INHERITING(nsDocument)


nsresult
nsHTMLDocument::Init()
{
  nsresult rv = nsDocument::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewISupportsArray(getter_AddRefs(mImageMaps));
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

  mIdAndNameHashIsLive = PL_DHashTableInit(&mIdAndNameHashTable,
                                           &hash_table_ops, nsnull,
                                           sizeof(IdAndNameMapEntry), 16);
  NS_ENSURE_TRUE(mIdAndNameHashIsLive, NS_ERROR_OUT_OF_MEMORY);

  PrePopulateHashTables();

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLDocument::ResetToURI(nsIURI *aURI, nsILoadGroup *aLoadGroup)
{
  nsresult result = nsDocument::ResetToURI(aURI, aLoadGroup);

  if (NS_SUCCEEDED(result))
    result = BaseResetToURI(aURI);

  return result;
}


nsresult
nsHTMLDocument::BaseResetToURI(nsIURI *aURL)
{
  nsresult result = NS_OK;

  InvalidateHashTables();
  PrePopulateHashTables();

  NS_IF_RELEASE(mImages);
  NS_IF_RELEASE(mApplets);
  NS_IF_RELEASE(mEmbeds);
  NS_IF_RELEASE(mLinks);
  NS_IF_RELEASE(mAnchors);
  NS_IF_RELEASE(mLayers);

  mBodyContent = nsnull;

  mImageMaps->Clear();
  NS_IF_RELEASE(mForms);

  if (aURL) {
    if (!mAttrStyleSheet) {
      //result = NS_NewHTMLStyleSheet(&mAttrStyleSheet, aURL, this);
      result = nsComponentManager::CreateInstance(kHTMLStyleSheetCID, nsnull,
                                                  NS_GET_IID(nsIHTMLStyleSheet),
                                                  getter_AddRefs(mAttrStyleSheet));
      if (NS_SUCCEEDED(result)) {
        result = mAttrStyleSheet->Init(aURL, this);
        if (NS_FAILED(result)) {
          mAttrStyleSheet = nsnull;
        }
      }
    }
    else {
      result = mAttrStyleSheet->Reset(aURL);
    }
    if (NS_SUCCEEDED(result)) {
      AddStyleSheet(mAttrStyleSheet, 0); // tell the world about our new style sheet

      if (!mStyleAttrStyleSheet) {
        result = NS_NewHTMLCSSStyleSheet(getter_AddRefs(mStyleAttrStyleSheet),
                                         aURL, this);
      }
      else {
        result = mStyleAttrStyleSheet->Reset(aURL);
      }
      if (NS_SUCCEEDED(result)) {
        AddStyleSheet(mStyleAttrStyleSheet, 0); // tell the world about our new style sheet
      }
    }
  }

  NS_ASSERTION(mWyciwygChannel == nsnull, "nsHTMLDocument::Reset() - Wyciwyg Channel  still exists!");
  mWyciwygChannel = nsnull;

  return result;
}


NS_IMETHODIMP
nsHTMLDocument::CreateShell(nsIPresContext* aContext,
                            nsIViewManager* aViewManager,
                            nsIStyleSet* aStyleSet,
                            nsIPresShell** aInstancePtrResult)
{
  return doCreateShell(aContext, aViewManager, aStyleSet,
                       mCompatMode, aInstancePtrResult);
}

// The following Try*Charset will return PR_FALSE only if the charset source 
// should be considered (ie. aCharsetSource < thisCharsetSource) but we failed 
// to get the charset from this source. 

PRBool 
nsHTMLDocument::TryHintCharset(nsIMarkupDocumentViewer* aMarkupDV,
                               PRInt32& aCharsetSource, 
                               nsAString& aCharset)
{
  PRInt32 requestCharsetSource;
  nsresult rv;

  if (aMarkupDV) {
    rv = aMarkupDV->GetHintCharacterSetSource(&requestCharsetSource);
    if(NS_SUCCEEDED(rv) && kCharsetUninitialized != requestCharsetSource) {
      PRUnichar* requestCharset;
      rv = aMarkupDV->GetHintCharacterSet(&requestCharset);
      aMarkupDV->SetHintCharacterSetSource((PRInt32)(kCharsetUninitialized));
      if(requestCharsetSource <= aCharsetSource) 
        return PR_TRUE;
      if(NS_SUCCEEDED(rv)) {
        aCharsetSource = requestCharsetSource;
        aCharset = requestCharset;
        Recycle(requestCharset);
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
                                     nsAString& aCharset)
{
  nsresult rv = NS_OK;

  if(kCharsetFromUserForced <= aCharsetSource) 
    return PR_TRUE;

  PRUnichar* forceCharsetFromWebShell = nsnull;
  if (aMarkupDV) 
    rv = aMarkupDV->GetForceCharacterSet(&forceCharsetFromWebShell);

  if(NS_SUCCEEDED(rv) && forceCharsetFromWebShell)
  {
    aCharset = forceCharsetFromWebShell;
    Recycle(forceCharsetFromWebShell);
    //TODO: we should define appropriate constant for force charset
    aCharsetSource = kCharsetFromUserForced;  
  } else if (aDocInfo) {
    nsCOMPtr<nsIAtom> csAtom;
    aDocInfo->GetForcedCharset(getter_AddRefs(csAtom));
    if (csAtom.get() != nsnull) {
      csAtom->ToString(aCharset);
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
                                nsAString& aCharset)
{
  nsresult rv;
  
  if (kCharsetFromCache <= aCharsetSource) 
    return PR_TRUE;

  nsXPIDLCString cachedCharset;
  rv = aCacheDescriptor->GetMetaDataElement("charset",
                                           getter_Copies(cachedCharset));
  if (NS_SUCCEEDED(rv) && !cachedCharset.IsEmpty())
  {
    aCharset.Assign(NS_ConvertASCIItoUCS2(cachedCharset));
    aCharsetSource = kCharsetFromCache;
    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool 
nsHTMLDocument::TryBookmarkCharset(nsAFlatCString* aUrlSpec,
                                   PRInt32& aCharsetSource, 
                                   nsAString& aCharset)
{
  nsresult rv;
  if (kCharsetFromBookmarks <= aCharsetSource)
    return PR_TRUE;

  nsCOMPtr<nsIRDFDataSource>  datasource;
  if (gRDF && NS_SUCCEEDED(rv = gRDF->GetDataSource("rdf:bookmarks", getter_AddRefs(datasource)))) {
    nsCOMPtr<nsIBookmarksService>   bookmarks = do_QueryInterface(datasource);
    if (bookmarks) {
      if (aUrlSpec) {
        nsXPIDLString pBookmarkedCharset;
        rv = bookmarks->GetLastCharset(aUrlSpec->get(), getter_Copies(pBookmarkedCharset));
        if (NS_SUCCEEDED(rv) && (rv != NS_RDF_NO_VALUE)) {
          aCharset = pBookmarkedCharset;
          aCharsetSource = kCharsetFromBookmarks;
          return PR_TRUE;
        }    
      } 
    }
  }
  return PR_FALSE;
}

PRBool 
nsHTMLDocument::TryParentCharset(nsIDocumentCharsetInfo*  aDocInfo,
                                 PRInt32& aCharsetSource, 
                                 nsAString& aCharset)
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
      csAtom->ToString(aCharset);
      aCharsetSource = source;  
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool 
nsHTMLDocument::UseWeakDocTypeDefault(PRInt32& aCharsetSource, 
                                      nsAString& aCharset)
{
  if (kCharsetFromWeakDocTypeDefault <= aCharsetSource)
    return PR_TRUE;
  
  aCharset.Assign(NS_LITERAL_STRING("ISO-8859-1")); // fallback value in case webShell return error
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
  if (prefs) {
    nsXPIDLString defCharset;
    nsresult rv = prefs->GetLocalizedUnicharPref("intl.charset.default", getter_Copies(defCharset));
    if (NS_SUCCEEDED(rv) && !defCharset.IsEmpty()) {
      aCharset.Assign(defCharset);
      aCharsetSource = kCharsetFromWeakDocTypeDefault;
    }
  }
  return PR_TRUE;
}

PRBool 
nsHTMLDocument::TryChannelCharset(nsIChannel *aChannel, 
                                  PRInt32& aCharsetSource, 
                                  nsAString& aCharset)
{
  if(kCharsetFromChannel <= aCharsetSource)
    return PR_TRUE;

  if (aChannel) {
    nsCAutoString charsetVal;
    nsresult rv = aChannel->GetContentCharset(charsetVal);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsICharsetAlias> calias(do_CreateInstance(kCharsetAliasCID, &rv));
      if (calias) {
        nsAutoString preferred;
        rv = calias->GetPreferred(NS_ConvertASCIItoUCS2(charsetVal), preferred);
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

PRBool 
nsHTMLDocument::TryDefaultCharset( nsIMarkupDocumentViewer* aMarkupDV,
                                       PRInt32& aCharsetSource, 
                                       nsAString& aCharset)
{
  if(kCharsetFromUserDefault <= aCharsetSource) 
    return PR_TRUE;

  PRUnichar* defaultCharsetFromWebShell = NULL;
  if (aMarkupDV) {
    nsresult rv = aMarkupDV->GetDefaultCharacterSet(&defaultCharsetFromWebShell);
    if(NS_SUCCEEDED(rv)) {
      aCharset = defaultCharsetFromWebShell;
      Recycle(defaultCharsetFromWebShell);
      aCharsetSource = kCharsetFromUserDefault;
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

void 
nsHTMLDocument::StartAutodetection(nsIDocShell *aDocShell, 
                                   nsAString& aCharset,
                                   const char* aCommand)
{
  nsCOMPtr <nsIParserFilter> cdetflt;

  nsresult rv_detect;
  if(! gInitDetector) {
    nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_CONTRACTID));
    if(pref) {
      PRUnichar* detector_name = nsnull;
      rv_detect = pref->GetLocalizedUnicharPref("intl.charset.detector", &detector_name);
      if(NS_SUCCEEDED(rv_detect)) {
        PL_strncpy(g_detector_contractid, NS_CHARSET_DETECTOR_CONTRACTID_BASE,DETECTOR_CONTRACTID_MAX);
        PL_strncat(g_detector_contractid, NS_ConvertUCS2toUTF8(detector_name).get(),DETECTOR_CONTRACTID_MAX);
        gPlugDetector = PR_TRUE;
        PR_FREEIF(detector_name);
      }
      pref->RegisterCallback("intl.charset.detector", MyPrefChangedCallback, nsnull);
    }
    gInitDetector = PR_TRUE;
  }

  if (gPlugDetector) {
    nsCOMPtr <nsICharsetDetector> cdet = do_CreateInstance(g_detector_contractid, &rv_detect);
    if(NS_SUCCEEDED(rv_detect)) {
      cdetflt = do_CreateInstance(NS_CHARSET_DETECTION_ADAPTOR_CONTRACTID, &rv_detect);
      if(NS_SUCCEEDED(rv_detect)) {
        nsCOMPtr<nsICharsetDetectionAdaptor> adp = do_QueryInterface(cdetflt, &rv_detect);
        if(cdetflt && NS_SUCCEEDED( rv_detect )) {
          nsCOMPtr<nsIWebShellServices> wss = do_QueryInterface(aDocShell, &rv_detect);
          if( NS_SUCCEEDED( rv_detect )) {
            rv_detect = adp->Init(wss, cdet, this, mParser, PromiseFlatString(aCharset).get(), aCommand);

            // The current implementation for SetParserFilter needs to 
            // be changed to be more XPCOM friendly. See bug #40149
            if (mParser) 
              nsCOMPtr<nsIParserFilter> oldFilter = getter_AddRefs(mParser->SetParserFilter(cdetflt));
          }
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

NS_IMETHODIMP
nsHTMLDocument::StartDocumentLoad(const char* aCommand,
                                  nsIChannel* aChannel,
                                  nsILoadGroup* aLoadGroup,
                                  nsISupports* aContainer,
                                  nsIStreamListener **aDocListener,
                                  PRBool aReset,
                                  nsIContentSink* aSink)
{
  PRBool needsParser=PR_TRUE;
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

  nsCOMPtr<nsIURI> aURL;
  rv = aChannel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoString lastModified;
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);

  if (httpChannel) {
    nsCAutoString lastModHeader;
    rv = httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("last-modified"),
                                        lastModHeader);

    if (NS_SUCCEEDED(rv)) {
      CopyASCIItoUCS2(lastModHeader, lastModified);
      SetLastModified(lastModified);
    }

    nsCAutoString referrerHeader;
    // The misspelled key 'referer' is as per the HTTP spec
    rv = httpChannel->GetRequestHeader(NS_LITERAL_CSTRING("referer"),
                                       referrerHeader);

    if (NS_SUCCEEDED(rv)) {
      SetReferrer(NS_ConvertASCIItoUCS2(referrerHeader));
    }

    mHttpChannel = httpChannel;

    nsCOMPtr<nsICachingChannel> cachingChan = do_QueryInterface(httpChannel);
    if (cachingChan) {
      nsCOMPtr<nsISupports> cacheToken;
      cachingChan->GetCacheToken(getter_AddRefs(cacheToken));
      if (cacheToken)
        cacheDescriptor = do_QueryInterface(cacheToken);
    }

    // Don't propagate the result code beyond here, since it
    // could just be that the response header wasn't found.
    rv = NS_OK;
  }

  nsCOMPtr<nsIFileChannel> fileChannel = do_QueryInterface(aChannel);
  if (fileChannel) {
    PRTime modDate, usecs;

    nsCOMPtr<nsIFile> file;
    rv = fileChannel->GetFile(getter_AddRefs(file));
    if (NS_SUCCEEDED(rv)) {
      // if we failed to get a last modification date, then we don't
      // want to necessarily fail to create a document for this
      // file. Just don't set the last modified date on it...
      rv = file->GetLastModifiedTime(&modDate);
      if (NS_SUCCEEDED(rv)) {
        PRExplodedTime prtime;
        char buf[100];
        PRInt64 intermediateValue;

        LL_I2L(intermediateValue, PR_USEC_PER_MSEC);
        LL_MUL(usecs, modDate, intermediateValue);
        PR_ExplodeTime(usecs, PR_LocalTimeParameters, &prtime);

        // Use '%#c' for windows, because '%c' is backward-compatible and
        // non-y2k with msvc; '%#c' requests that a full year be used in the
        // result string.  Other OSes just use "%c".
        PR_FormatTime(buf, sizeof buf,
#if defined(XP_PC) && !defined(XP_OS2)
                      "%#c",
#else
                      "%c",
#endif
                      &prtime);
        lastModified.AssignWithConversion(buf);
        SetLastModified(lastModified);
      }
    }
  }

  if (needsParser) {
    rv = nsComponentManager::CreateInstance(kCParserCID, nsnull, 
                                            NS_GET_IID(nsIParser), 
                                            (void **)&mParser);
    if (NS_FAILED(rv)) { return rv; }
  }

  nsCOMPtr<nsIHTMLContentSink> sink;
#ifdef rickgdebug
  nsString outString;   // added out. Redirect to stdout if desired -- gpk 04/01/99
  rv = NS_New_HTML_ContentSinkStream(getter_AddRefs(sink),&outString,0);
  if (NS_FAILED(rv)) return rv;
  NS_ASSERTION(sink, "null sink in debug code variant.");
#else
  NS_PRECONDITION(nsnull != aContainer, "No content viewer container");
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aContainer));

  nsCOMPtr<nsIDocumentCharsetInfo> dcInfo;
  docShell->GetDocumentCharsetInfo(getter_AddRefs(dcInfo));  
#ifdef IBMBIDI
  nsCOMPtr<nsIPresContext> cx;
  docShell->GetPresContext(getter_AddRefs(cx));
  if(cx){
    PRUint32 mBidiOption;
    cx->GetBidi(&mBidiOption);
    mTexttype = GET_BIDI_OPTION_TEXTTYPE(mBidiOption);
  }
#endif // IBMBIDI
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
    // but if we get a null pointer, that's perfectly legal for parent and parentContentViewer
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
  aURL->GetScheme(scheme);

  nsCAutoString urlSpec;
  aURL->GetSpec(urlSpec);

  PRInt32 charsetSource = kCharsetUninitialized;
  nsAutoString charset; 
  
  // The following charset resolving calls has implied knowledge about 
  // charset source priority order. Each try will return true if the 
  // source is higher or equal to the source as its name describes. Some
  // try call might change charset source to multiple values, like 
  // TryHintCharset and TryParentCharset. It should be always safe to try more 
  // sources. 
  if (! TryUserForcedCharset(muCV, dcInfo, charsetSource, charset)) {
    TryHintCharset(muCV, charsetSource, charset);
    TryParentCharset(dcInfo, charsetSource, charset);
    if (TryChannelCharset(aChannel, charsetSource, charset)) {
      // Use the channel's charset (e.g., charset from HTTP "Content-Type" header).
    }
    else if (!scheme.Equals(NS_LITERAL_CSTRING("about")) &&          // don't try to access bookmarks for about:blank
             TryBookmarkCharset(&urlSpec, charsetSource, charset)) {
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
  //check if current doc is from POST command
  if (httpChannel) {  
    nsCAutoString methodStr;
    rv = httpChannel->GetRequestMethod(methodStr);
    if (NS_SUCCEEDED(rv) && methodStr.Equals(NS_LITERAL_CSTRING("POST")))
      isPostPage = PR_TRUE;
  }

  if (isPostPage && muCV && kCharsetFromHintPrevDoc > charsetSource) {
    nsXPIDLString requestCharset;
    muCV->GetPrevDocCharacterSet(getter_Copies(requestCharset));
    if (!requestCharset.IsEmpty()) {
      charsetSource = kCharsetFromHintPrevDoc;
      charset = requestCharset;
    }
  }

  if(kCharsetFromAutoDetection > charsetSource && !isPostPage) {
    StartAutodetection(docShell, charset, aCommand);
  }
#endif

//ahmed
#ifdef IBMBIDI
  // Check if 864 but in Implicit mode !
  if( (mTexttype == IBMBIDI_TEXTTYPE_LOGICAL)&&(charset.EqualsIgnoreCase("ibm864")) )
    charset.Assign(NS_LITERAL_STRING("IBM864i"));
#endif // IBMBIDI

  SetDocumentCharacterSet(charset);
  SetDocumentCharacterSetSource(charsetSource);
  
  // set doc charset to muCV for next document.
  if (muCV)
    muCV->SetPrevDocCharacterSet(charset.get());
    
  if(cacheDescriptor) {
    rv = cacheDescriptor->SetMetaDataElement("charset",
                                    NS_ConvertUCS2toUTF8(charset).get());
    NS_ASSERTION(NS_SUCCEEDED(rv),"cannot SetMetaDataElement");
  }

  // Set the parser as the stream listener for the document loader...
  if (mParser) {
    rv = mParser->QueryInterface(NS_GET_IID(nsIStreamListener),
                                 (void**)aDocListener);
    if (NS_FAILED(rv)) {
      return rv;
    }

    //The following lines were added by Rick.
    //These perform "dynamic" DTD registration, allowing
    //the caller total control over process, and decoupling
    //parser from any given grammar.

//        nsCOMPtr<nsIDTD> theDTD;
//        NS_NewNavHTMLDTD(getter_AddRefs(theDTD));
//        mParser->RegisterDTD(theDTD);

#ifdef DEBUG_charset
	  char* cCharset = ToNewCString(charset);
	  printf("set to parser charset = %s source %d\n", 
		  		cCharset, charsetSource);
				  Recycle(cCharset);
#endif
    mParser->SetDocumentCharset( charset, charsetSource);
    mParser->SetCommand(aCommand);
    // create the content sink
    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(docShell));

    if (aSink)
      sink = do_QueryInterface(aSink);
    else {
      rv = NS_NewHTMLContentSink(getter_AddRefs(sink), this, aURL, webShell,aChannel);
      if (NS_FAILED(rv)) { return rv; }
      NS_ASSERTION(sink, "null sink with successful result from factory method");
    }

    mParser->SetContentSink(sink); 
    // parser the content of the URL
    mParser->Parse(aURL, nsnull, PR_FALSE, (void *)this);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLDocument::StopDocumentLoad()
{
  if (mParser) {
    mParser->Terminate();
  }
 
  return NS_OK;
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

    NS_IF_RELEASE(htmldoc->mParser);
  }

  htmldoc->EndLoad();
}

NS_IMETHODIMP
nsHTMLDocument::EndLoad()
{
  if (mParser) {
    nsCOMPtr<nsIJSContextStack> stack =
      do_GetService("@mozilla.org/js/xpc/ContextStack;1");

    if (stack) {
      JSContext *cx = nsnull;
      stack->Peek(&cx);

      if (cx) {
        nsCOMPtr<nsIScriptContext> scx;
        nsContentUtils::GetDynamicScriptContext(cx, getter_AddRefs(scx));

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

          return NS_OK;
        }
      }
    }
  }

  NS_IF_RELEASE(mParser);
  return nsDocument::EndLoad();
}

NS_IMETHODIMP
nsHTMLDocument::SetTitle(const nsAString& aTitle)
{
  return nsDocument::SetTitle(aTitle);
}

NS_IMETHODIMP
nsHTMLDocument::AddImageMap(nsIDOMHTMLMapElement* aMap)
{
  // XXX We should order the maps based on their order in the document.
  // XXX Otherwise scripts that add/remove maps with duplicate names 
  // XXX will cause problems
  NS_PRECONDITION(nsnull != aMap, "null ptr");
  if (nsnull == aMap) {
    return NS_ERROR_NULL_POINTER;
  }
  if (mImageMaps->AppendElement(aMap)) {
    return NS_OK;
  }
  return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP 
nsHTMLDocument::RemoveImageMap(nsIDOMHTMLMapElement* aMap)
{
  NS_PRECONDITION(nsnull != aMap, "null ptr");
  if (nsnull == aMap) {
    return NS_ERROR_NULL_POINTER;
  }
  mImageMaps->RemoveElement(aMap);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetImageMap(const nsString& aMapName,
                            nsIDOMHTMLMapElement** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsAutoString name;
  PRUint32 i, n;
  mImageMaps->Count(&n);
  for (i = 0; i < n; i++) {
    nsCOMPtr<nsIDOMHTMLMapElement> map;
    mImageMaps->QueryElementAt(i, NS_GET_IID(nsIDOMHTMLMapElement), getter_AddRefs(map));
    if (map && NS_SUCCEEDED(map->GetName(name))) {
      if (name.Equals(aMapName, nsCaseInsensitiveStringComparator())) {
        *aResult = map;
        NS_ADDREF(*aResult);
        return NS_OK;
      }
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsHTMLDocument::GetAttributeStyleSheet(nsIHTMLStyleSheet** aResult)
{
  NS_PRECONDITION(aResult, "null ptr");
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mAttrStyleSheet;
  if (!mAttrStyleSheet) {
    return NS_ERROR_NOT_AVAILABLE;  // probably not the right error...
  }

  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aResult)
{
  NS_PRECONDITION(aResult, "null ptr");
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mStyleAttrStyleSheet;
  if (!mStyleAttrStyleSheet) {
    return NS_ERROR_NOT_AVAILABLE;  // probably not the right error...
  }

  NS_ADDREF(*aResult);
  return NS_OK;
}

// subclass hooks for sheet ordering
void
nsHTMLDocument::InternalAddStyleSheet(nsIStyleSheet* aSheet, PRUint32 aFlags)
{
  if (aSheet == mAttrStyleSheet) {  // always first
    mStyleSheets.InsertObjectAt(aSheet, 0);
  }
  else if (aSheet == mStyleAttrStyleSheet) {  // always last
    mStyleSheets.AppendObject(aSheet);
  }
  else {
    if (mStyleSheets.Count() != 0 &&
        mStyleAttrStyleSheet == mStyleSheets.ObjectAt(mStyleSheets.Count() - 1)) {
      // keep attr sheet last
      mStyleSheets.InsertObjectAt(aSheet, mStyleSheets.Count() - 1);
    }
    else {
      mStyleSheets.AppendObject(aSheet);
    }
  }
}

void
nsHTMLDocument::InternalInsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex)
{
  mStyleSheets.InsertObjectAt(aSheet, aIndex + 1); // offset one for the attr style sheet
}

NS_IMETHODIMP
nsHTMLDocument::GetBaseURL(nsIURI*& aURL) const
{
  if (mDocumentBaseURL) {    
    aURL = mDocumentBaseURL.get();
    NS_ADDREF(aURL);
  }
  else {
    GetDocumentURL(&aURL);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetBaseTarget(nsAString& aTarget)
{
  if (nsnull != mBaseTarget) {
    aTarget.Assign(*mBaseTarget);
  }
  else {
    aTarget.Truncate();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetBaseTarget(const nsAString& aTarget)
{
  if (!aTarget.IsEmpty()) {
    if (nsnull != mBaseTarget) {
      *mBaseTarget = aTarget;
    }
    else {
      mBaseTarget = new nsString(aTarget);
    }
  }
  else {
    if (nsnull != mBaseTarget) {
      delete mBaseTarget;
      mBaseTarget = nsnull;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLDocument::SetLastModified(const nsAString& aLastModified)
{
  if (!aLastModified.IsEmpty()) {
    if (nsnull != mLastModified) {
      *mLastModified = aLastModified;
    }
    else {
      mLastModified = new nsString(aLastModified);
    }
  }
  else if (nsnull != mLastModified) {
    delete mLastModified;
    mLastModified = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLDocument::SetReferrer(const nsAString& aReferrer)
{
  if (!aReferrer.IsEmpty()) {
    if (nsnull != mReferrer) {
      *mReferrer = aReferrer;
    }
    else {
      mReferrer = new nsString(aReferrer);
    }
  }
  else if (nsnull != mReferrer) {
    delete mReferrer;
    mReferrer = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetCSSLoader(nsICSSLoader*& aLoader)
{
  nsresult result = NS_OK;
  if (! mCSSLoader) {
    result = NS_NewCSSLoader(this, getter_AddRefs(mCSSLoader));
  }
  if (mCSSLoader) {
    mCSSLoader->SetCaseSensitive(PR_FALSE);
    mCSSLoader->SetCompatibilityMode(mCompatMode);
  }
  aLoader = mCSSLoader;
  NS_IF_ADDREF(aLoader);
  return result;
}


NS_IMETHODIMP
nsHTMLDocument::GetCompatibilityMode(nsCompatibility& aMode)
{
  aMode = mCompatMode;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetCompatibilityMode(nsCompatibility aMode)
{
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

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLDocument::ContentAppended(nsIContent* aContainer,
                                PRInt32 aNewIndexInContainer)
{
  NS_ABORT_IF_FALSE(aContainer, "Null container!");

  // Register new content. That is the content numbered from
  // aNewIndexInContainer and upwards.
  PRInt32 count=0;
  aContainer->ChildCount(count);

  PRInt32 i;
  nsCOMPtr<nsIContent> newChild;
  for (i = aNewIndexInContainer; i < count; ++i) {
    aContainer->ChildAt(i, *getter_AddRefs(newChild));
    if (newChild) 
      RegisterNamedItems(newChild);
  }

  return nsDocument::ContentAppended(aContainer, aNewIndexInContainer);
}

NS_IMETHODIMP 
nsHTMLDocument::ContentInserted(nsIContent* aContainer, nsIContent* aContent,
                                PRInt32 aIndexInContainer)
{
  NS_ABORT_IF_FALSE(aContent, "Null content!");

  nsresult rv = RegisterNamedItems(aContent);

  if (NS_FAILED(rv)) {
    return rv;
  }

  return nsDocument::ContentInserted(aContainer, aContent, aIndexInContainer);
}

NS_IMETHODIMP
nsHTMLDocument::ContentReplaced(nsIContent* aContainer, nsIContent* aOldChild,
                                nsIContent* aNewChild,
                                PRInt32 aIndexInContainer)
{
  NS_ABORT_IF_FALSE(aOldChild && aNewChild, "Null new or old child!");

  nsresult rv = UnregisterNamedItems(aOldChild);

  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = RegisterNamedItems(aNewChild);

  if (NS_FAILED(rv)) {
    return rv;
  }

  return nsDocument::ContentReplaced(aContainer, aOldChild, 
                                     aNewChild, aIndexInContainer);
}

NS_IMETHODIMP 
nsHTMLDocument::ContentRemoved(nsIContent* aContainer, nsIContent* aContent,
                               PRInt32 aIndexInContainer)
{
  NS_ABORT_IF_FALSE(aContent, "Null content!");

  nsresult rv = UnregisterNamedItems(aContent);

  if (NS_FAILED(rv)) {
    return rv;
  }

  return nsDocument::ContentRemoved(aContainer, aContent, aIndexInContainer);
}

NS_IMETHODIMP
nsHTMLDocument::AttributeWillChange(nsIContent* aContent, PRInt32 aNameSpaceID,
                                    nsIAtom* aAttribute)
{
  NS_ABORT_IF_FALSE(aContent, "Null content!");

  // XXX: Check namespaces!!!

  if (aAttribute == nsHTMLAtoms::name) {
    nsCOMPtr<nsIAtom> tag;
    nsAutoString value;

    aContent->GetTag(*getter_AddRefs(tag));

    if (IsNamedItem(aContent, tag, value)) {
      nsresult rv = RemoveFromNameTable(value, aContent);

      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  } else if (aAttribute == nsHTMLAtoms::id) {
    nsresult rv = RemoveFromIdTable(aContent);

    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return nsDocument::AttributeWillChange(aContent, aNameSpaceID, aAttribute);
}

NS_IMETHODIMP
nsHTMLDocument::AttributeChanged(nsIContent* aContent, PRInt32 aNameSpaceID,
                                 nsIAtom* aAttribute, PRInt32 aModType, nsChangeHint aHint)
{
  NS_ABORT_IF_FALSE(aContent, "Null content!");

  // XXX: Check namespaces!

  if (aAttribute == nsHTMLAtoms::name) {
    nsCOMPtr<nsIAtom> tag;
    nsAutoString value;

    aContent->GetTag(*getter_AddRefs(tag));

    if (IsNamedItem(aContent, tag, value)) {
      nsresult rv = UpdateNameTableEntry(value, aContent);

      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  } else if (aAttribute == nsHTMLAtoms::id) {
    nsAutoString value;

    aContent->GetAttr(aNameSpaceID, nsHTMLAtoms::id, value);

    if (!value.IsEmpty()) {
      nsresult rv = AddToIdTable(value, aContent);

      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  return nsDocument::AttributeChanged(aContent, aNameSpaceID, aAttribute, aModType, 
                                      aHint);
}

NS_IMETHODIMP 
nsHTMLDocument::FlushPendingNotifications(PRBool aFlushReflows,
                                          PRBool aUpdateViews)
{
  // Determine if it is safe to flush the sink
  // by determining if it safe to flush all the presshells.
  PRBool isSafeToFlush = PR_TRUE;
  if (aFlushReflows) {
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
    nsCOMPtr<nsIContentSink> sink;
    
    // XXX Ack! Parser doesn't addref sink before passing it back
    sink = mParser->GetContentSink();
    if (sink) {
      nsresult rv = sink->FlushPendingNotifications();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return nsDocument::FlushPendingNotifications(aFlushReflows, aUpdateViews);
}

NS_IMETHODIMP
nsHTMLDocument::CreateElementNS(const nsAString& aNamespaceURI,
                                const nsAString& aQualifiedName,
                                nsIDOMElement** aReturn)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsINodeInfo> nodeInfo;
  rv = mNodeInfoManager->GetNodeInfo(aQualifiedName, aNamespaceURI,
                                     *getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 namespaceID;
  nodeInfo->GetNamespaceID(namespaceID);

  nsCOMPtr<nsIElementFactory> elementFactory;
  mNameSpaceManager->GetElementFactory(namespaceID,
                                       getter_AddRefs(elementFactory));

  nsCOMPtr<nsIContent> content;

  if (elementFactory) {
    rv = elementFactory->CreateInstanceByTag(nodeInfo,
                                             getter_AddRefs(content));
  } else {
    rv = NS_NewXMLElement(getter_AddRefs(content), nodeInfo);
  }

  NS_ENSURE_SUCCESS(rv, rv);

  content->SetContentID(mNextContentID++);

  return CallQueryInterface(content, aReturn);
}


//
// nsIDOMDocument interface implementation
//
NS_IMETHODIMP    
nsHTMLDocument::CreateElement(const nsAString& aTagName, 
                              nsIDOMElement** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  NS_ENSURE_TRUE(!aTagName.IsEmpty(), NS_ERROR_DOM_INVALID_CHARACTER_ERR);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsAutoString tmp(aTagName);
  ToLowerCase(tmp);

  mNodeInfoManager->GetNodeInfo(tmp, nsnull, kNameSpaceID_None,
                                *getter_AddRefs(nodeInfo));

  nsCOMPtr<nsIHTMLContent> content;
  nsresult rv = NS_CreateHTMLElement(getter_AddRefs(content), nodeInfo,
                                     PR_FALSE);
  if (NS_SUCCEEDED(rv)) {
    content->SetContentID(mNextContentID++);
    rv = content->QueryInterface(NS_GET_IID(nsIDOMElement), (void**)aReturn);
  }
  return rv;
}

NS_IMETHODIMP    
nsHTMLDocument::CreateProcessingInstruction(const nsAString& aTarget, 
                                            const nsAString& aData, 
                                            nsIDOMProcessingInstruction** aReturn)
{
  // There are no PIs for HTML
  *aReturn = nsnull;
  
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP    
nsHTMLDocument::CreateCDATASection(const nsAString& aData, 
                                   nsIDOMCDATASection** aReturn)
{
  // There are no CDATASections in HTML
  *aReturn = nsnull;

  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}
 
NS_IMETHODIMP    
nsHTMLDocument::CreateEntityReference(const nsAString& aName, 
                                      nsIDOMEntityReference** aReturn)
{
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
nsHTMLDocument::GetElementsByTagName(const nsAString& aTagname, nsIDOMNodeList** aReturn)
{ 
  nsAutoString tmp(aTagname);
  ToLowerCase(tmp); // HTML elements are lower case internally.
  return nsDocument::GetElementsByTagName(tmp, aReturn); 
}

//
// nsIDOMNode interface implementation
//
NS_IMETHODIMP    
nsHTMLDocument::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  return nsDocument::GetChildNodes(aChildNodes);
}

NS_IMETHODIMP    
nsHTMLDocument::GetFirstChild(nsIDOMNode** aFirstChild)
{
  return nsDocument::GetFirstChild(aFirstChild);
}

NS_IMETHODIMP    
nsHTMLDocument::GetLastChild(nsIDOMNode** aLastChild)
{
  return nsDocument::GetLastChild(aLastChild);
}

NS_IMETHODIMP    
nsHTMLDocument::InsertBefore(nsIDOMNode* aNewChild, 
                             nsIDOMNode* aRefChild, 
                             nsIDOMNode** aReturn)
{
  return nsDocument::InsertBefore(aNewChild, aRefChild, aReturn);
}

NS_IMETHODIMP    
nsHTMLDocument::ReplaceChild(nsIDOMNode* aNewChild, 
                             nsIDOMNode* aOldChild, 
                             nsIDOMNode** aReturn)
{
  return nsDocument::ReplaceChild(aNewChild, aOldChild, aReturn);
}

NS_IMETHODIMP    
nsHTMLDocument::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return nsDocument::RemoveChild(aOldChild, aReturn);
}

NS_IMETHODIMP    
nsHTMLDocument::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
  return nsDocument::AppendChild(aNewChild, aReturn);
}

NS_IMETHODIMP    
nsHTMLDocument::HasChildNodes(PRBool* aReturn)
{
  return nsDocument::HasChildNodes(aReturn);
}

NS_IMETHODIMP    
nsHTMLDocument::HasAttributes(PRBool* aReturn)
{
  return nsDocument::HasAttributes(aReturn);
}

NS_IMETHODIMP    
nsHTMLDocument::GetNodeName(nsAString& aNodeName)
{ 
  return nsDocument::GetNodeName(aNodeName); 
}

NS_IMETHODIMP    
nsHTMLDocument::GetNodeValue(nsAString& aNodeValue)
{ 
  return nsDocument::GetNodeValue(aNodeValue); 
}

NS_IMETHODIMP    
nsHTMLDocument::SetNodeValue(const nsAString& aNodeValue)
{ 
  return nsDocument::SetNodeValue(aNodeValue); 
}

NS_IMETHODIMP    
nsHTMLDocument::GetNodeType(PRUint16* aNodeType)
{ 
  return nsDocument::GetNodeType(aNodeType); 
}

NS_IMETHODIMP
nsHTMLDocument::GetNamespaceURI(nsAString& aNamespaceURI)
{ 
  return nsDocument::GetNamespaceURI(aNamespaceURI);
}

NS_IMETHODIMP
nsHTMLDocument::GetPrefix(nsAString& aPrefix)
{
  return nsDocument::GetPrefix(aPrefix);
}

NS_IMETHODIMP
nsHTMLDocument::SetPrefix(const nsAString& aPrefix)
{
  return nsDocument::SetPrefix(aPrefix);
}

NS_IMETHODIMP
nsHTMLDocument::GetLocalName(nsAString& aLocalName)
{
  return nsDocument::GetLocalName(aLocalName);
}

NS_IMETHODIMP    
nsHTMLDocument::GetParentNode(nsIDOMNode** aParentNode)
{ 
  return nsDocument::GetParentNode(aParentNode); 
}

NS_IMETHODIMP    
nsHTMLDocument::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{ 
  return nsDocument::GetPreviousSibling(aPreviousSibling); 
}

NS_IMETHODIMP    
nsHTMLDocument::GetNextSibling(nsIDOMNode** aNextSibling)
{ 
  return nsDocument::GetNextSibling(aNextSibling); 
}

NS_IMETHODIMP    
nsHTMLDocument::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{ 
  return nsDocument::GetAttributes(aAttributes); 
}

NS_IMETHODIMP    
nsHTMLDocument::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{ 
  return nsDocument::GetOwnerDocument(aOwnerDocument); 
}

NS_IMETHODIMP    
nsHTMLDocument::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{ 
  return nsDocument::CloneNode(aDeep, aReturn);
}

NS_IMETHODIMP
nsHTMLDocument::Normalize()
{
  return nsDocument::Normalize();
}

NS_IMETHODIMP
nsHTMLDocument::IsSupported(const nsAString& aFeature,
                            const nsAString& aVersion,
                            PRBool* aReturn)
{
  return nsDocument::IsSupported(aFeature, aVersion, aReturn);
}

NS_IMETHODIMP
nsHTMLDocument::GetBaseURI(nsAString &aURI)
{
  aURI.Truncate();
  nsCOMPtr<nsIURI> uri(do_QueryInterface(mBaseURL ? mBaseURL : mDocumentURL));
  if (uri) {
    nsCAutoString spec;
    uri->GetSpec(spec);
    aURI = NS_ConvertUTF8toUCS2(spec);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::CompareTreePosition(nsIDOMNode* aOther,
                                    PRUint16* aReturn)
{
  return nsDocument::CompareTreePosition(aOther, aReturn);
}

NS_IMETHODIMP
nsHTMLDocument::IsSameNode(nsIDOMNode* aOther,
                           PRBool* aReturn)
{
  return nsDocument::IsSameNode(aOther, aReturn);
}


NS_IMETHODIMP    
nsHTMLDocument::LookupNamespacePrefix(const nsAString& aNamespaceURI,
                                      nsAString& aPrefix) 
{
  aPrefix.Truncate();
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::LookupNamespaceURI(const nsAString& aNamespacePrefix,
                                   nsAString& aNamespaceURI)
{
  aNamespaceURI.Truncate();
  return NS_OK;
}


//
// nsIDOMHTMLDocument interface implementation
//
// see http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-html.html#ID-1006298752
// for full specification.
//
NS_IMETHODIMP
nsHTMLDocument::GetTitle(nsAString& aTitle)
{
  return nsDocument::GetTitle(aTitle);
}

NS_IMETHODIMP    
nsHTMLDocument::GetReferrer(nsAString& aReferrer)
{
  if (nsnull != mReferrer) {
    aReferrer.Assign(*mReferrer);
  }
  else {
    aReferrer.Truncate();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetDomainURI(nsIURI **uri) 
{
  nsCOMPtr<nsIPrincipal> principal;
  if (NS_FAILED(GetPrincipal(getter_AddRefs(principal))))
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsICodebasePrincipal> codebase = do_QueryInterface(principal);
  if (!codebase)
    return NS_ERROR_FAILURE;
  return codebase->GetURI(uri);
}


NS_IMETHODIMP    
nsHTMLDocument::GetDomain(nsAString& aDomain)
{
  nsCOMPtr<nsIURI> uri;
  if (NS_FAILED(GetDomainURI(getter_AddRefs(uri))))
    return NS_ERROR_FAILURE;

  nsCAutoString hostName;
  if (NS_FAILED(uri->GetHost(hostName)))
    return NS_ERROR_FAILURE;
  aDomain.Assign(NS_ConvertUTF8toUCS2(hostName));

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::SetDomain(const nsAString& aDomain)
{
  // Check new domain - must be a superdomain of the current host
  // For example, a page from foo.bar.com may set domain to bar.com,
  // but not to ar.com, baz.com, or fi.foo.bar.com.
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
        (c == '.' || c == '/'))
      ok = PR_TRUE;
  }
  if (!ok) {
    // Error: illegal domain
    return NS_ERROR_DOM_BAD_DOCUMENT_DOMAIN;
  }
  
  // Create new URI
  nsCOMPtr<nsIURI> uri;
  if (NS_FAILED(GetDomainURI(getter_AddRefs(uri))))
    return NS_ERROR_FAILURE;
  nsCAutoString scheme;
  if (NS_FAILED(uri->GetScheme(scheme)))
    return NS_ERROR_FAILURE;
  nsCAutoString path;
  if (NS_FAILED(uri->GetPath(path)))
    return NS_ERROR_FAILURE;
  NS_ConvertUTF8toUCS2 newURIString(scheme);
  newURIString += NS_LITERAL_STRING("://") + aDomain + NS_ConvertUTF8toUCS2(path);
  nsCOMPtr<nsIURI> newURI;
  if (NS_FAILED(NS_NewURI(getter_AddRefs(newURI), newURIString)))
    return NS_ERROR_FAILURE;

  // Get codebase principal
  nsresult rv;
  nsCOMPtr<nsIScriptSecurityManager> securityManager = 
           do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) 
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIPrincipal> newCodebase;
  rv = securityManager->GetCodebasePrincipal(newURI, getter_AddRefs(newCodebase));
    if (NS_FAILED(rv)) 
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIAggregatePrincipal> agg = do_QueryInterface(mPrincipal, &rv);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Principal not an aggregate.");
  if (NS_FAILED(rv)) 
    return NS_ERROR_FAILURE;

  rv = agg->SetCodebase(newCodebase);

  // Bug 13871: Frameset spoofing - note that document.domain was set
  if (NS_SUCCEEDED(rv))
    mDomainWasSet = PR_TRUE;

  return rv;
}

NS_IMETHODIMP
nsHTMLDocument::WasDomainSet(PRBool* aDomainWasSet)
{
  NS_ENSURE_ARG_POINTER(aDomainWasSet);
  *aDomainWasSet = mDomainWasSet;
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::GetURL(nsAString& aURL)
{
  if (nsnull != mDocumentURL) {
    nsCAutoString str;
    mDocumentURL->GetSpec(str);
    aURL.Assign(NS_ConvertUTF8toUCS2(str));
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::GetBody(nsIDOMHTMLElement** aBody)
{
  NS_ENSURE_ARG_POINTER(aBody);
  *aBody = nsnull;

  nsISupports* element = nsnull;
  nsCOMPtr<nsIDOMNode> node;

  if (mBodyContent || (GetBodyContent() && mBodyContent)) {
    // There is a body element, return that as the body.
    element = mBodyContent;
  } else {
    // The document is most likely a frameset document so look for the
    // outer most frameset element

    nsCOMPtr<nsIDOMNodeList> nodeList;

    nsresult rv = GetElementsByTagName(NS_LITERAL_STRING("frameset"),
                                       getter_AddRefs(nodeList));
    if (NS_FAILED(rv))
      return rv;

    if (nodeList) {
      rv = nodeList->Item(0, getter_AddRefs(node));
      if (NS_FAILED(rv))
        return rv;

      element = node;
    }
  }

  return element ? CallQueryInterface(element, aBody) : NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::SetBody(nsIDOMHTMLElement* aBody)
{
  nsCOMPtr<nsIDOMHTMLBodyElement> bodyElement(do_QueryInterface(aBody));

  // The body element must be of type nsIDOMHTMLBodyElement.
  if (!bodyElement) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  nsCOMPtr<nsIDOMElement> root;
  GetDocumentElement(getter_AddRefs(root));

  if (!root) {  
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  NS_NAMED_LITERAL_STRING(bodyStr, "BODY");
  nsCOMPtr<nsIDOMNode> child;
  root->GetFirstChild(getter_AddRefs(child));

  while (child) {
    nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(child));

    if (domElement) {
      nsAutoString tagName;

      domElement->GetTagName(tagName);

      ToUpperCase(tagName);
      if (bodyStr.Equals(tagName)) {
        nsCOMPtr<nsIDOMNode> ret;

        nsresult rv = root->ReplaceChild(aBody, child, getter_AddRefs(ret));

        mBodyContent = nsnull;

        return rv;
      }
    }

    nsIDOMNode *tmpNode = child;
    tmpNode->GetNextSibling(getter_AddRefs(child));
  }

  return PR_FALSE;
}

NS_IMETHODIMP    
nsHTMLDocument::GetImages(nsIDOMHTMLCollection** aImages)
{
  if (!mImages) {
    mImages = new nsContentList(this, nsHTMLAtoms::img, kNameSpaceID_Unknown);
    if (!mImages) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mImages);
  }

  *aImages = (nsIDOMHTMLCollection *)mImages;
  NS_ADDREF(*aImages);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::GetApplets(nsIDOMHTMLCollection** aApplets)
{
  if (!mApplets) {
    mApplets = new nsContentList(this, nsHTMLAtoms::applet,
                                 kNameSpaceID_None);
    if (!mApplets) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mApplets);
  }

  *aApplets = (nsIDOMHTMLCollection *)mApplets;
  NS_ADDREF(*aApplets);

  return NS_OK;
}

PRBool
nsHTMLDocument::MatchLinks(nsIContent *aContent, nsString* aData)
{
  nsCOMPtr<nsIAtom> name;
  aContent->GetTag(*getter_AddRefs(name));

  if (name == nsHTMLAtoms::area || name == nsHTMLAtoms::a) {
    return aContent->HasAttr(kNameSpaceID_None, nsHTMLAtoms::href);
  }

  return PR_FALSE;
}

NS_IMETHODIMP    
nsHTMLDocument::GetLinks(nsIDOMHTMLCollection** aLinks)
{
  if (nsnull == mLinks) {
    mLinks = new nsContentList(this, MatchLinks, nsString());
    if (nsnull == mLinks) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mLinks);
  }

  *aLinks = (nsIDOMHTMLCollection *)mLinks;
  NS_ADDREF(mLinks);

  return NS_OK;
}

PRBool
nsHTMLDocument::MatchAnchors(nsIContent *aContent, nsString* aData)
{
  nsCOMPtr<nsIAtom> name;
  aContent->GetTag(*getter_AddRefs(name));

  if (name == nsHTMLAtoms::a) {
    return aContent->HasAttr(kNameSpaceID_None, nsHTMLAtoms::name);
  }

  return PR_FALSE;
}

NS_IMETHODIMP    
nsHTMLDocument::GetAnchors(nsIDOMHTMLCollection** aAnchors)
{
  if (!mAnchors) {
    mAnchors = new nsContentList(this, MatchAnchors, nsString());
    NS_ENSURE_TRUE(mAnchors, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(mAnchors);
  }

  *aAnchors = (nsIDOMHTMLCollection *)mAnchors;
  NS_ADDREF(*aAnchors);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::GetCookie(nsAString& aCookie)
{
  aCookie.Truncate(); // clear current cookie in case service fails;
                      // no cookie isn't an error condition.

  // If caller is not chrome and dom.disable_cookie_get is true,
  // prevent getting cookies by exiting early
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID));
  if (prefs) {
    PRBool disableCookieGet = PR_FALSE;
    prefs->GetBoolPref("dom.disable_cookie_get", &disableCookieGet);

    if (disableCookieGet && !nsContentUtils::IsCallerChrome()) {
      return NS_OK;
    }
  }

  nsresult result = NS_OK;
  nsAutoString str;

  nsCOMPtr<nsICookieService> service = do_GetService(kCookieServiceCID, &result);
  if (NS_SUCCEEDED(result) && service) {

    // Get a URI from the document principal
    // We use the original codebase in case the codebase was changed by SetDomain
    nsCOMPtr<nsIAggregatePrincipal> agg(do_QueryInterface(mPrincipal, &result));
    // Document principal should always be an aggregate
    NS_ENSURE_SUCCESS(result, result);

    nsCOMPtr<nsIPrincipal> originalPrincipal;
    result = agg->GetOriginalCodebase(getter_AddRefs(originalPrincipal));
    nsCOMPtr<nsICodebasePrincipal> originalCodebase(
        do_QueryInterface(originalPrincipal, &result));
    if (NS_FAILED(result)) {
      // Document's principal is not a codebase, so can't get cookies
      return NS_OK; 
    }

    nsCOMPtr<nsIURI> codebaseURI;
    result = originalCodebase->GetURI(getter_AddRefs(codebaseURI));
    NS_ENSURE_SUCCESS(result, result);

    nsXPIDLCString cookie;
      result = service->GetCookieString(codebaseURI, getter_Copies(cookie));
    if (NS_SUCCEEDED(result) && cookie)
      CopyASCIItoUCS2(nsDependentCString(cookie), aCookie);
  }
  return result;
}

NS_IMETHODIMP    
nsHTMLDocument::SetCookie(const nsAString& aCookie)
{
  // If caller is not chrome and dom.disable_cookie_get is true,
  // prevent setting cookies by exiting early
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID));
  if (prefs) {
    PRBool disableCookieSet = PR_FALSE;
    prefs->GetBoolPref("dom.disable_cookie_set", &disableCookieSet);
    if (disableCookieSet && !nsContentUtils::IsCallerChrome()) {
      return NS_OK;
    }
  }

  nsresult result = NS_OK;
  nsCOMPtr<nsICookieService> service = do_GetService(kCookieServiceCID, &result);
  if (NS_SUCCEEDED(result) && service && mDocumentURL) {
    nsCOMPtr<nsIScriptGlobalObject> globalObj;
    nsCOMPtr<nsIPrompt> prompt;
    this->GetScriptGlobalObject(getter_AddRefs(globalObj));
    if (globalObj) {
      nsCOMPtr<nsIDOMWindowInternal> window (do_QueryInterface(globalObj));
      if (window) {
        window->GetPrompter(getter_AddRefs(prompt));
      }
    }

    // Get a URI from the document principal
    // We use the original codebase in case the codebase was changed by SetDomain
    nsCOMPtr<nsIAggregatePrincipal> agg(do_QueryInterface(mPrincipal, &result));
    // Document principal should always be an aggregate
    NS_ENSURE_SUCCESS(result, result);

    nsCOMPtr<nsIPrincipal> originalPrincipal;
    result = agg->GetOriginalCodebase(getter_AddRefs(originalPrincipal));
    nsCOMPtr<nsICodebasePrincipal> originalCodebase(
        do_QueryInterface(originalPrincipal, &result));
    if (NS_FAILED(result)) {
      // Document's principal is not a codebase, so can't set cookies
      return NS_OK; 
    }

    nsCOMPtr<nsIURI> codebaseURI;
    result = originalCodebase->GetURI(getter_AddRefs(codebaseURI));
    NS_ENSURE_SUCCESS(result, result);

    result = NS_ERROR_OUT_OF_MEMORY;
    char* cookie = ToNewCString(aCookie);
    if (cookie) {
      result = service->SetCookieString(codebaseURI, prompt, cookie, mHttpChannel);
      nsCRT::free(cookie);
    }
  }
  return result;
}

// static
nsresult
nsHTMLDocument::GetSourceDocumentURL(nsIURI** sourceURL)
{
  // XXX Tom said this reminded him of the "Six Degrees of
  // Kevin Bacon" game. We try to get from here to there using
  // whatever connections possible. The problem is that this
  // could break if any of the connections along the way change.
  // I wish there were a better way.
  *sourceURL = nsnull;

  // XXX This will fail on non-DOM contexts :(
  nsCOMPtr<nsIDOMDocument> domDoc;
  nsContentUtils::GetDocumentFromCaller(getter_AddRefs(domDoc));

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  if (!doc) {
    return NS_OK; // No document in the window
  }

  doc->GetDocumentURL(sourceURL); 

  return sourceURL ? NS_OK : NS_ERROR_FAILURE; 
}


// XXX TBI: accepting arguments to the open method.
nsresult
nsHTMLDocument::OpenCommon(nsIURI* aSourceURL)
{
  nsCOMPtr<nsIDocShell> docshell;

  // If we already have a parser we ignore the document.open call.
  if (mParser)
    return NS_OK;

  // Stop current loads targeted at the window this document is in.
  if (mScriptGlobalObject) {
    mScriptGlobalObject->GetDocShell(getter_AddRefs(docshell));

    if (docshell) {
      nsCOMPtr<nsIWebNavigation> webnav(do_QueryInterface(docshell));
      webnav->Stop(nsIWebNavigation::STOP_NETWORK);
    }
  }

  nsresult result = NS_OK;

  // The open occurred after the document finished loading.
  // So we reset the document and create a new one.
  nsCOMPtr<nsIChannel> channel;
  nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);

  result = NS_NewChannel(getter_AddRefs(channel), aSourceURL, nsnull, group);

  if (NS_FAILED(result)) return result;

  //Before we reset the doc notify the globalwindow of the change.

  if (mScriptGlobalObject) {
    //Hold onto ourselves on the offchance that we're down to one ref

    nsCOMPtr<nsIDOMDocument> kungFuDeathGrip =
      do_QueryInterface((nsIHTMLDocument*)this);

    result = mScriptGlobalObject->SetNewDocument(kungFuDeathGrip, PR_FALSE,
                                                 PR_FALSE);

    if (NS_FAILED(result))
      return result;
  }

  // XXX This is a nasty workaround for a scrollbar code bug
  // (http://bugzilla.mozilla.org/show_bug.cgi?id=55334).

  // Hold on to our root element
  nsCOMPtr<nsIContent> root(mRootContent);

  if (root) {
    PRInt32 count;
    root->ChildCount(count);

    // Remove all the children from the root.
    while (--count >= 0) {
      root->RemoveChildAt(count, PR_TRUE);
    }

    count = 0;

    mRootContent->GetAttrCount(count);

    // Remove all attributes from the root element
    while (--count >= 0) {
      nsCOMPtr<nsIAtom> name, prefix;
      PRInt32 nsid;

      root->GetAttrNameAt(count, nsid, *getter_AddRefs(name),
                          *getter_AddRefs(prefix));

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

  result = Reset(channel, group);
  if (NS_FAILED(result))
    return result;

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

  result = nsComponentManager::CreateInstance(kCParserCID, nsnull, 
                                              NS_GET_IID(nsIParser), 
                                              (void **)&mParser);
  mIsWriting = 1;

  if (NS_SUCCEEDED(result)) { 
    nsCOMPtr<nsIHTMLContentSink> sink;
    nsCOMPtr<nsIWebShell> webShell;

    // Get the webshell of our primary presentation shell
    nsCOMPtr<nsIPresShell> shell = (nsIPresShell*)mPresShells.SafeElementAt(0);
    if (shell) {
      nsCOMPtr<nsIPresContext> cx;
      shell->GetPresContext(getter_AddRefs(cx));
      nsCOMPtr<nsISupports> container;
      if (NS_OK == cx->GetContainer(getter_AddRefs(container))) {
        if (container) {
          webShell = do_QueryInterface(container);
        }
      }
    }

    result = NS_NewHTMLContentSink(getter_AddRefs(sink), this, aSourceURL,
                                   webShell, channel);

    if (NS_OK == result) {
      static NS_DEFINE_CID(kNavDTDCID, NS_CNAVDTD_CID);
      nsCOMPtr<nsIDTD> theDTD(do_CreateInstance(kNavDTDCID, &result));
      if(NS_SUCCEEDED(result)) {
        mParser->RegisterDTD(theDTD);
      }
      mParser->SetContentSink(sink); 
    }
  }

  // Prepare the docshell and the document viewer for the impending
  // out of band document.write()
  if (docshell) {
    docshell->PrepareForNewContentModel();
    nsCOMPtr<nsIContentViewer> cv;
    docshell->GetContentViewer(getter_AddRefs(cv));
    nsCOMPtr<nsIDocumentViewer> docViewer = do_QueryInterface(cv);
    if (docViewer) {
      docViewer->LoadStart(NS_STATIC_CAST(nsIHTMLDocument *, this));
    }
  }

  // Add a wyciwyg channel request into the document load group
  NS_ASSERTION(mWyciwygChannel == nsnull, "nsHTMLDocument::OpenCommon(): wyciwyg channel already exists!");
  CreateAndAddWyciwygChannel();
  return result;
}

NS_IMETHODIMP    
nsHTMLDocument::Open()
{
  nsCOMPtr<nsIDOMDocument> doc;
  return Open(getter_AddRefs(doc));
}

NS_IMETHODIMP    
nsHTMLDocument::Open(nsIDOMDocument** aReturn)
{
  // XXX The URL of the newly created document will match
  // that of the source document. Is this right?

  // XXX This will fail on non-DOM contexts :(
  nsCOMPtr<nsIURI> sourceURL;
  nsresult result = GetSourceDocumentURL(getter_AddRefs(sourceURL));

  // Recover if we had a problem obtaining the source URL
  if (!sourceURL) {
    result = NS_NewURI(getter_AddRefs(sourceURL),
                       NS_LITERAL_CSTRING("about:blank"));
  }

  if (NS_SUCCEEDED(result)) {
    result = OpenCommon(sourceURL);
  }

  CallQueryInterface(this, aReturn);

  return result;
}

#define NS_GENERATE_PARSER_KEY() (void*)((mIsWriting << 31) | (mWriteLevel & 0x7fffffff))

NS_IMETHODIMP    
nsHTMLDocument::Clear()
{
  // This method has been deprecated
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::Close()
{
  nsresult result = NS_OK;

  if (mParser && mIsWriting) {
    mWriteLevel++;
    result = mParser->Parse(NS_LITERAL_STRING("</HTML>"),
                            NS_GENERATE_PARSER_KEY(), 
                            NS_LITERAL_CSTRING("text/html"), PR_FALSE,
                            PR_TRUE);
    mWriteLevel--;
    mIsWriting = 0;
    NS_IF_RELEASE(mParser);

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
    FlushPendingNotifications();

    // Remove the wyciwyg channel request from the document load group
    // that we added in OpenCommon().  If all other requests between
    // document.open() and document.close() have completed, then this
    // method should cause the firing of an onload event.
    NS_ASSERTION(mWyciwygChannel, "nsHTMLDocument::Close(): Trying to remove non-existent wyciwyg channel!");  
    RemoveWyciwygChannel();
    NS_ASSERTION(mWyciwygChannel == nsnull, "nsHTMLDocument::Close(): nsIWyciwyg Channel could not be removed!");  
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
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  mWriteLevel++;

  static NS_NAMED_LITERAL_STRING(new_line, "\n");
  static NS_NAMED_LITERAL_STRING(empty, "");
  const nsAString  *term = aNewlineTerminate ? &new_line : &empty;

  const nsAString& text = aText + *term;

  // Save the data in cache
  if (mWyciwygChannel) {
    mWyciwygChannel->WriteToCacheEntry(NS_ConvertUCS2toUTF8(text));
  } 

  rv = mParser->Parse(text ,
                      NS_GENERATE_PARSER_KEY(),
                      NS_LITERAL_CSTRING("text/html"), PR_FALSE,
                      (!mIsWriting || (mWriteLevel > 1)));

  mWriteLevel--;

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

  if (mDocumentURL) {
    rv = mDocumentURL->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!mDocumentURL || nsCRT::strcasecmp(spec.get(), "about:blank") == 0) {
    // The current document's URL and principal are empty or "about:blank".
    // By writing to this document, the script acquires responsibility for the
    // document for security purposes. Thus a document.write of a script tag
    // ends up producing a script with the same principals as the script
    // that performed the write.
    nsCOMPtr<nsIScriptSecurityManager> secMan =
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrincipal> subject;
    rv = secMan->GetSubjectPrincipal(getter_AddRefs(subject));
    NS_ENSURE_SUCCESS(rv, rv);

    // why is the above code duplicated below???
    rv = secMan->GetSubjectPrincipal(getter_AddRefs(subject));
    NS_ENSURE_SUCCESS(rv, rv);

    if (subject) {
      nsCOMPtr<nsICodebasePrincipal> codebase = do_QueryInterface(subject);
      if (codebase) {
        nsCOMPtr<nsIURI> subjectURI;
        rv = codebase->GetURI(getter_AddRefs(subjectURI));
        NS_ENSURE_SUCCESS(rv, rv);

        NS_IF_RELEASE(mDocumentURL);
        mDocumentURL = subjectURI;
        NS_ADDREF(mDocumentURL);

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

      for (i = 0; i < argc; i++) {
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
  return WriteCommon(nsString(), aNewlineTerminate);
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


nsIContent *
nsHTMLDocument::MatchId(nsIContent *aContent, const nsAString& aId)
{
  // Most elements don't have an id, so lets call the faster HasAttr()
  // method before we create a string object and call GetAttr().

  if (aContent->HasAttr(kNameSpaceID_None, nsHTMLAtoms::id)) {
    nsAutoString value;

    nsresult rv = aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, value);

    if (rv == NS_CONTENT_ATTR_HAS_VALUE && aId.Equals(value)) {
      return aContent;
    }
  }

  nsIContent *result = nsnull;
  PRInt32 i, count;

  aContent->ChildCount(count);
  for (i = 0; i < count && !result; i++) {
    nsIContent *child;
    aContent->ChildAt(i, child);
    result = MatchId(child, aId);
    NS_RELEASE(child);
  }  

  return result;
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
      e = MatchId(mRootContent, aElementId);
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
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLDocument::GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                                       const nsAString& aLocalName,
                                       nsIDOMNodeList** aReturn)
{
  nsAutoString tmp(aLocalName);
  ToLowerCase(tmp); // HTML elements are lower case internally.
  return nsDocument::GetElementsByTagNameNS(aNamespaceURI, tmp,
                                            aReturn); 
}

PRBool
nsHTMLDocument::MatchNameAttribute(nsIContent* aContent, nsString* aData)
{
  // Most elements don't have a name attribute, so lets call the
  // faster HasAttr() method before we create a string object and call
  // GetAttr().

  if (!aContent->HasAttr(kNameSpaceID_None, nsHTMLAtoms::name) || !aData) {
    return PR_FALSE;
  }

  nsAutoString name;

  nsresult rv = aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::name, name);

  if (NS_SUCCEEDED(rv) && name.Equals(*aData)) {
    return PR_TRUE;
  }

  return PR_FALSE;
}

NS_IMETHODIMP    
nsHTMLDocument::GetElementsByName(const nsAString& aElementName, 
                                  nsIDOMNodeList** aReturn)
{
  nsContentList* elements = new nsContentList(this, MatchNameAttribute,
                                              aElementName);
  NS_ENSURE_TRUE(elements, NS_ERROR_OUT_OF_MEMORY);

  *aReturn = elements;
  NS_ADDREF(*aReturn);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::AddedForm()
{
  mNumForms++;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::RemovedForm()
{
  mNumForms--;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetNumFormsSynchronous(PRInt32* aNumForms)
{
  *aNumForms = mNumForms;
  return NS_OK;
}

PRBool
nsHTMLDocument::MatchFormControls(nsIContent* aContent, nsString* aData)
{
  return aContent->IsContentOfType(nsIContent::eHTML_FORM_CONTROL);
}

NS_IMETHODIMP
nsHTMLDocument::GetFormControlElements(nsIDOMNodeList** aReturn)
{
  nsContentList* elements = nsnull;
  elements = new nsContentList(this, MatchFormControls, nsString());
  NS_ENSURE_TRUE(elements, NS_ERROR_OUT_OF_MEMORY);

  *aReturn = elements;
  NS_ADDREF(*aReturn);

  return NS_OK;
}

nsresult
nsHTMLDocument::GetPixelDimensions(nsIPresShell* aShell,
                                   PRInt32* aWidth,
                                   PRInt32* aHeight)
{
  *aWidth = *aHeight = 0;

  nsresult result = FlushPendingNotifications();
  NS_ENSURE_SUCCESS(result, result);

  // Find the <body> element: this is what we'll want to use for the
  // document's width and height values.
  if (!mBodyContent && PR_FALSE == GetBodyContent()) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> body = do_QueryInterface(mBodyContent);

  // Now grab its frame
  nsIFrame* frame;
  result = aShell->GetPrimaryFrameFor(body, &frame);
  if (NS_SUCCEEDED(result) && frame) {
    nsSize                    size;
    nsIView*                  view;
    nsCOMPtr<nsIPresContext>  presContext;

    aShell->GetPresContext(getter_AddRefs(presContext));
    result = frame->GetView(presContext, &view);
    if (NS_SUCCEEDED(result)) {
      // If we have a view check if it's scrollable. If not,
      // just use the view size itself
      if (view) {
        nsIScrollableView* scrollableView = nsnull;

        view->QueryInterface(NS_GET_IID(nsIScrollableView),
                             (void**)&scrollableView);

        if (scrollableView) {
          scrollableView->GetScrolledView(view);
        }

        nsRect r;
        result = view->GetBounds(r);
        if (NS_SUCCEEDED(result)) {
          size.height = r.height;
          size.width = r.width;
        }
      }
      // If we don't have a view, use the frame size
      else {
        result = frame->GetSize(size);
      }
    }

    // Convert from twips to pixels
    if (NS_SUCCEEDED(result)) {
      nsCOMPtr<nsIPresContext> context;
      
      result = aShell->GetPresContext(getter_AddRefs(context));
      
      if (NS_SUCCEEDED(result)) {
        float scale;
        context->GetTwipsToPixels(&scale);
        
        *aWidth = NSTwipsToIntPixels(size.width, scale);
        *aHeight = NSTwipsToIntPixels(size.height, scale);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::GetWidth(PRInt32* aWidth)
{
  NS_ENSURE_ARG_POINTER(aWidth);
  *aWidth = 0;

  nsCOMPtr<nsIPresShell> shell;

  // We make the assumption that the first presentation shell
  // is the one for which we need information.
  GetShellAt(0, getter_AddRefs(shell));
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

  nsCOMPtr<nsIPresShell> shell;

  // We make the assumption that the first presentation shell
  // is the one for which we need information.
  GetShellAt(0, getter_AddRefs(shell));
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
  nsresult result = NS_OK;
  nsIDOMHTMLBodyElement* body;

  aAlinkColor.Truncate();
  result = GetBodyElement(&body);
  if (NS_OK == result) {
    result = body->GetALink(aAlinkColor);
    NS_RELEASE(body);
  }
  else if (mAttrStyleSheet) {
    nscolor color;
    result = mAttrStyleSheet->GetActiveLinkColor(color);
    if (NS_OK == result) {
      nsHTMLValue value(color);
      nsGenericHTMLElement::ColorToString(value, aAlinkColor);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetAlinkColor(const nsAString& aAlinkColor)
{
  nsresult result = NS_OK;
  nsIDOMHTMLBodyElement* body;

  result = GetBodyElement(&body);
  if (NS_OK == result) {
    result = body->SetALink(aAlinkColor);
    NS_RELEASE(body);
  }
  else if (mAttrStyleSheet) {
    nsHTMLValue value;
  
    if (nsGenericHTMLElement::ParseColor(aAlinkColor, this, value)) {
      mAttrStyleSheet->SetActiveLinkColor(value.GetColorValue());
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::GetLinkColor(nsAString& aLinkColor)
{
  nsresult result = NS_OK;
  nsIDOMHTMLBodyElement* body;

  aLinkColor.Truncate();
  result = GetBodyElement(&body);
  if (NS_OK == result) {
    result = body->GetLink(aLinkColor);
    NS_RELEASE(body);
  }
  else if (mAttrStyleSheet) {
    nscolor color;
    result = mAttrStyleSheet->GetLinkColor(color);
    if (NS_OK == result) {
      nsHTMLValue value(color);
      nsGenericHTMLElement::ColorToString(value, aLinkColor);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::SetLinkColor(const nsAString& aLinkColor)
{
  nsresult result = NS_OK;
  nsIDOMHTMLBodyElement* body;

  result = GetBodyElement(&body);
  if (NS_OK == result) {
    result = body->SetLink(aLinkColor);
    NS_RELEASE(body);
  }
  else if (mAttrStyleSheet) {
    nsHTMLValue value;
    if (nsGenericHTMLElement::ParseColor(aLinkColor, this, value)) {
      mAttrStyleSheet->SetLinkColor(value.GetColorValue());
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::GetVlinkColor(nsAString& aVlinkColor)
{
  nsresult result = NS_OK;
  nsIDOMHTMLBodyElement* body;

  aVlinkColor.Truncate();
  result = GetBodyElement(&body);
  if (NS_OK == result) {
    result = body->GetVLink(aVlinkColor);
    NS_RELEASE(body);
  }
  else if (mAttrStyleSheet) {
    nscolor color;
    result = mAttrStyleSheet->GetVisitedLinkColor(color);
    if (NS_OK == result) {
      nsHTMLValue value(color);
      nsGenericHTMLElement::ColorToString(value, aVlinkColor);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::SetVlinkColor(const nsAString& aVlinkColor)
{
  nsresult result = NS_OK;
  nsIDOMHTMLBodyElement* body;

  result = GetBodyElement(&body);
  if (NS_OK == result) {
    result = body->SetVLink(aVlinkColor);
    NS_RELEASE(body);
  }
  else if (mAttrStyleSheet) {
    nsHTMLValue value;
    if (nsGenericHTMLElement::ParseColor(aVlinkColor, this, value)) {
      mAttrStyleSheet->SetVisitedLinkColor(value.GetColorValue());
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::GetBgColor(nsAString& aBgColor)
{
  nsresult result = NS_OK;
  nsIDOMHTMLBodyElement* body;

  aBgColor.Truncate();
  result = GetBodyElement(&body);
  if (NS_SUCCEEDED(result)) {
    result = body->GetBgColor(aBgColor);
    NS_RELEASE(body);
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::SetBgColor(const nsAString& aBgColor)
{
  nsresult result = NS_OK;
  nsIDOMHTMLBodyElement* body;

  result = GetBodyElement(&body);
  if (NS_SUCCEEDED(result)) {
    result = body->SetBgColor(aBgColor);
    NS_RELEASE(body);
  }
  // XXXldb And otherwise?
  
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::GetFgColor(nsAString& aFgColor)
{
  nsresult result = NS_OK;
  nsIDOMHTMLBodyElement* body;

  aFgColor.Truncate();
  result = GetBodyElement(&body);
  if (NS_SUCCEEDED(result)) {
    result = body->GetText(aFgColor);
    NS_RELEASE(body);
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::SetFgColor(const nsAString& aFgColor)
{
  nsresult result = NS_OK;
  nsIDOMHTMLBodyElement* body;

  result = GetBodyElement(&body);
  if (NS_SUCCEEDED(result)) {
    result = body->SetText(aFgColor);
    NS_RELEASE(body);
  }
  // XXXldb And otherwise?
  
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::GetLastModified(nsAString& aLastModified)
{
  if (nsnull != mLastModified) {
    aLastModified.Assign(*mLastModified);
  }
  else {
    aLastModified.Assign(NS_LITERAL_STRING("January 1, 1970 GMT"));
  }

  return NS_OK;
}


NS_IMETHODIMP    
nsHTMLDocument::GetEmbeds(nsIDOMHTMLCollection** aEmbeds)
{
  if (!mEmbeds) {
    mEmbeds = new nsContentList(this, nsHTMLAtoms::embed,
                                kNameSpaceID_Unknown);
    NS_ENSURE_TRUE(mEmbeds, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(mEmbeds);
  }

  *aEmbeds = (nsIDOMHTMLCollection *)mEmbeds;
  NS_ADDREF(mEmbeds);

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

  nsCOMPtr<nsISupports> container;

  cx->GetContainer(getter_AddRefs(container));
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
    aCompatMode.Assign(NS_LITERAL_STRING("BackCompat"));
  } else {
    aCompatMode.Assign(NS_LITERAL_STRING("CSS1Compat"));
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
AddEmptyListToHash(const nsAString& aName, PLDHashTable *aHash)
{
  nsBaseContentList *list = new nsBaseContentList();
  NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(list);

  IdAndNameMapEntry *entry =
    NS_STATIC_CAST(IdAndNameMapEntry *,
                   PL_DHashTableOperate(aHash, &aName, PL_DHASH_ADD));

  if (!entry) {
    NS_RELEASE(list);

    return NS_ERROR_OUT_OF_MEMORY;
  }

  entry->mContentList = list;

  return NS_OK;
}

// Pre-fill the name hash with names that are likely to be resolved in
// this document to avoid walking the tree looking for elements with
// these names.

nsresult
nsHTMLDocument::PrePopulateHashTables()
{
  nsresult rv = NS_OK;

  rv = AddEmptyListToHash(NS_LITERAL_STRING("write"), &mIdAndNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddEmptyListToHash(NS_LITERAL_STRING("writeln"), &mIdAndNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddEmptyListToHash(NS_LITERAL_STRING("open"), &mIdAndNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddEmptyListToHash(NS_LITERAL_STRING("close"), &mIdAndNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddEmptyListToHash(NS_LITERAL_STRING("forms"), &mIdAndNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddEmptyListToHash(NS_LITERAL_STRING("elements"), &mIdAndNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddEmptyListToHash(NS_LITERAL_STRING("characterSet"),
                          &mIdAndNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddEmptyListToHash(NS_LITERAL_STRING("nodeType"), &mIdAndNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddEmptyListToHash(NS_LITERAL_STRING("parentNode"),
                          &mIdAndNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddEmptyListToHash(NS_LITERAL_STRING("cookie"), &mIdAndNameHashTable);
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
  IdAndNameMapEntry *entry =
    NS_STATIC_CAST(IdAndNameMapEntry *,
                   PL_DHashTableOperate(&mIdAndNameHashTable, &aName,
                                        PL_DHASH_LOOKUP));

  if (!PL_DHASH_ENTRY_IS_LIVE(entry)) {
    return NS_OK;
  }

  nsBaseContentList *list = entry->mContentList;

  if (!list) {
    return NS_OK;
  }

  PRInt32 i;

  list->IndexOf(aContent, i);

  if (i < 0) {
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

  if (PL_DHASH_ENTRY_IS_LIVE(entry)) {
    entry->mIdContent = aContent;
  }

  return NS_OK;
}

nsresult
nsHTMLDocument::RemoveFromNameTable(const nsAString& aName,
                                    nsIContent *aContent)
{
  IdAndNameMapEntry *entry =
    NS_STATIC_CAST(IdAndNameMapEntry *,
                   PL_DHashTableOperate(&mIdAndNameHashTable, &aName,
                                        PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_LIVE(entry) && entry->mContentList) {
    entry->mContentList->RemoveElement(aContent);
  }

  return NS_OK;
}

nsresult
nsHTMLDocument::RemoveFromIdTable(nsIContent *aContent)
{
  if (!aContent->HasAttr(kNameSpaceID_None, nsHTMLAtoms::id)) {
    return NS_OK;
  }

  nsAutoString value;
  aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, value);

  if (value.IsEmpty()) {
    return NS_OK;
  }

  IdAndNameMapEntry *entry =
    NS_STATIC_CAST(IdAndNameMapEntry *,
                   PL_DHashTableOperate(&mIdAndNameHashTable,
                                        NS_STATIC_CAST(const nsAString *,
                                                       &value),
                                        PL_DHASH_LOOKUP));

  if (!PL_DHASH_ENTRY_IS_LIVE(entry) || entry->mIdContent != aContent) {
    return NS_OK;
  }

  PL_DHashTableRawRemove(&mIdAndNameHashTable, entry);

  return NS_OK;
}

nsresult
nsHTMLDocument::UnregisterNamedItems(nsIContent *aContent)
{
  nsCOMPtr<nsIAtom> tag;

  aContent->GetTag(*getter_AddRefs(tag));

  if (tag == nsLayoutAtoms::textTagName) {
    // Text nodes are not named items nor can they have children.

    return NS_OK;
  }

  nsAutoString value;
  nsresult rv = NS_OK;

  if (IsNamedItem(aContent, tag, value)) {
    rv = RemoveFromNameTable(value, aContent);

    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  rv = RemoveFromIdTable(aContent);

  if (NS_FAILED(rv)) {
    return rv;
  }

  PRInt32 i, count;

  aContent->ChildCount(count);

  for (i = 0; i < count; i++) {
    nsIContent *child;

    aContent->ChildAt(i, child);

    UnregisterNamedItems(child);

    NS_RELEASE(child);
  }

  return NS_OK;
}

nsresult
nsHTMLDocument::RegisterNamedItems(nsIContent *aContent)
{
  nsCOMPtr<nsIAtom> tag;

  aContent->GetTag(*getter_AddRefs(tag));

  if (tag == nsLayoutAtoms::textTagName) {
    // Text nodes are not named items nor can they have children.

    return NS_OK;
  }

  nsAutoString value;

  if (IsNamedItem(aContent, tag, value)) {
    UpdateNameTableEntry(value, aContent);
  }

  aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, value);

  if (!value.IsEmpty()) {
    nsresult rv = UpdateIdTableEntry(value, aContent);

    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  PRInt32 i, count;

  aContent->ChildCount(count);

  for (i = 0; i < count; i++) {
    nsIContent *child;

    aContent->ChildAt(i, child);

    RegisterNamedItems(child);

    NS_RELEASE(child);
  }

  return NS_OK;
}

static void
FindNamedItems(const nsAString& aName, nsIContent *aContent,
               IdAndNameMapEntry& aEntry)
{
  NS_ASSERTION(aEntry.mContentList,
               "Entry w/o content list passed to FindNamedItems()!");

  nsCOMPtr<nsIAtom> tag;
  aContent->GetTag(*getter_AddRefs(tag));

  if (tag == nsLayoutAtoms::textTagName) {
    // Text nodes are not named items nor can they have children.

    return;
  }

  nsAutoString value;

  if (IsNamedItem(aContent, tag, value) && value.Equals(aName)) {
    aEntry.mContentList->AppendElement(aContent);
  }

  if (!aEntry.mIdContent) {
    aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, value);

    if (value.Equals(aName)) {
      aEntry.mIdContent = aContent;
    }
  }

  PRInt32 i, count;

  aContent->ChildCount(count);

  nsCOMPtr<nsIContent> child;

  for (i = 0; i < count; i++) {
    aContent->ChildAt(i, *getter_AddRefs(child));

    FindNamedItems(aName, child, aEntry);
  }
}

NS_IMETHODIMP
nsHTMLDocument::ResolveName(const nsAString& aName,
                            nsIDOMHTMLFormElement *aForm,
                            nsISupports **aResult)
{
  *aResult = nsnull;

  // Bug 69826 - Make sure to flush the content model if the document
  // is still loading.

  // This is a perf killer while the document is loading!
  FlushPendingNotifications(PR_FALSE);

  // We have built a table and cache the named items. The table will
  // be updated as content is added and removed.

  IdAndNameMapEntry *entry =
    NS_STATIC_CAST(IdAndNameMapEntry *,
                   PL_DHashTableOperate(&mIdAndNameHashTable, &aName,
                                        PL_DHASH_ADD));
  NS_ENSURE_TRUE(entry, NS_ERROR_OUT_OF_MEMORY);

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

    if(mRootContent && !aName.IsEmpty()) {
      FindNamedItems(aName, mRootContent, *entry);
    }
  }

  PRUint32 length;
  list->GetLength(&length);

  if (length) {
    if (length == 1) {
      // Onle one element in the list, return the list in stead of
      // returning the list

      nsCOMPtr<nsIDOMNode> node;

      list->Item(0, getter_AddRefs(node));

      if (aForm && node) {
        // document.forms["foo"].bar should not map to <form
        // name="bar"> so we check here to see if we found a form and
        // if we did we ignore what we found in the document. This
        // doesn't deal with the case where more than one element in
        // found in the document (i.e. there are two named items in
        // the document that have the name we're looking for), that
        // case is dealt with in nsFormContentList

        nsCOMPtr<nsIDOMHTMLFormElement> f(do_QueryInterface(node));

        if (f) {
          node = nsnull;
        }
      }

      *aResult = node;
      NS_IF_ADDREF(*aResult);

      return NS_OK;
    }

    if (length > 1) {
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
          // After t nsFormContentList is done filtering there's zero
          // or one element in the list, return that element, or null
          // if there's no element in the list.

          nsCOMPtr<nsIDOMNode> node;

          fc_list->Item(0, getter_AddRefs(node));

          *aResult = node;
          NS_IF_ADDREF(*aResult);

          delete fc_list;

          return NS_OK;
        }

        list = fc_list;
      }

      return list->QueryInterface(NS_GET_IID(nsISupports), (void **)aResult);
    }
  }

  // No named items were found, see if there's one registerd by id for
  // aName. If we get this far, FindNamedItems() will have been called
  // for aName, so we're guaranteed that if there is an element with
  // the id aName, it'll be in entry->mIdContent.

  nsIContent *e = entry->mIdContent;

  if (e && e != ID_NOT_IN_DOCUMENT) {
    nsCOMPtr<nsIAtom> tag;
    e->GetTag(*getter_AddRefs(tag));

    if (tag == nsHTMLAtoms::embed  ||
        tag == nsHTMLAtoms::img    ||
        tag == nsHTMLAtoms::object ||
        tag == nsHTMLAtoms::applet) {
      *aResult = e;
      NS_ADDREF(*aResult);
    }
  }

  return NS_OK;
}

//----------------------------

PRBool
nsHTMLDocument::GetBodyContent()
{
  nsCOMPtr<nsIContent> root;

  GetRootContent(getter_AddRefs(root));

  if (!root) {  
    return PR_FALSE;
  }

  PRInt32 i, child_count;
  root->ChildCount(child_count);

  for (i = 0; i < child_count; i++) {
    nsCOMPtr<nsIContent> child;

    root->ChildAt(i, *getter_AddRefs(child));
    NS_ENSURE_TRUE(child, NS_ERROR_UNEXPECTED);

    if (child->IsContentOfType(nsIContent::eHTML)) {
      nsCOMPtr<nsINodeInfo> ni;
      child->GetNodeInfo(*getter_AddRefs(ni));

      if (ni->Equals(nsHTMLAtoms::body)) {
        mBodyContent = do_QueryInterface(child);

        return PR_TRUE;
      }
    }
  }

  return PR_FALSE;
}

NS_IMETHODIMP
nsHTMLDocument::GetBodyElement(nsIDOMHTMLBodyElement** aBody)
{
  if (!mBodyContent && PR_FALSE == GetBodyContent()) {
    return NS_ERROR_FAILURE;
  }
  
  return mBodyContent->QueryInterface(NS_GET_IID(nsIDOMHTMLBodyElement), 
                                      (void**)aBody);
}

// forms related stuff

NS_IMETHODIMP    
nsHTMLDocument::GetForms(nsIDOMHTMLCollection** aForms)
{
  if (!mForms) {
    mForms = new nsContentList(this, nsHTMLAtoms::form, kNameSpaceID_Unknown);
    NS_ENSURE_TRUE(mForms, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(mForms);
  }

  *aForms = (nsIDOMHTMLCollection *)mForms;
  NS_ADDREF(mForms);

  return NS_OK;
}


nsresult
nsHTMLDocument::CreateAndAddWyciwygChannel(void)
{ 
  nsresult rv = NS_OK;
  nsCAutoString url, originalSpec;

  mDocumentURL->GetSpec(originalSpec);
  
  // Generate the wyciwyg url
  url = NS_LITERAL_CSTRING("wyciwyg://")
      + nsPrintfCString("%d", mWyciwygSessionCnt++)
      + NS_LITERAL_CSTRING("/")
      + originalSpec;

  nsCOMPtr<nsIURI> wcwgURI;
  NS_NewURI(getter_AddRefs(wcwgURI), url);

  // Create the nsIWyciwygChannel to store
  // out-of-band document.write() script to cache
  nsCOMPtr<nsIChannel> channel;
  // Create a wyciwyg Channel
  rv = NS_NewChannel(getter_AddRefs(channel), wcwgURI);
  if (NS_SUCCEEDED(rv) && channel) {
    mWyciwygChannel = do_QueryInterface(channel);
    // inherit load flags from the original document's channel
    channel->SetLoadFlags(mLoadFlags);
  }

  nsCOMPtr<nsILoadGroup> loadGroup;

  rv = GetDocumentLoadGroup(getter_AddRefs(loadGroup));
  if (NS_FAILED(rv)) return rv;

  // Use the Parent document's loadgroup to trigger load notifications
  if (loadGroup && channel) {
    rv = channel->SetLoadGroup(loadGroup);
    if (NS_FAILED(rv)) return rv;

    nsLoadFlags loadFlags = 0;
    channel->GetLoadFlags(&loadFlags);
    loadFlags |= nsIChannel::LOAD_DOCUMENT_URI;
    channel->SetLoadFlags(loadFlags);
    
    channel->SetOriginalURI(wcwgURI);

    rv = loadGroup->AddRequest(mWyciwygChannel, nsnull);
    if (NS_FAILED(rv)) return rv;
  }

  return rv;
}

nsresult
nsHTMLDocument::RemoveWyciwygChannel(void)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsILoadGroup> loadGroup;
  rv = GetDocumentLoadGroup(getter_AddRefs(loadGroup));
  if (NS_FAILED(rv)) return rv;

  // note there can be a write request without a load group if
  // this is a synchronously constructed about:blank document
  if (loadGroup && mWyciwygChannel) {
    mWyciwygChannel->CloseCacheEntry();
    rv = loadGroup->RemoveRequest(mWyciwygChannel, nsnull, NS_OK);
    if (NS_FAILED(rv)) return rv;
  }
  mWyciwygChannel = nsnull;

  return rv;
}

/* attribute DOMString designMode; */
NS_IMETHODIMP
nsHTMLDocument::GetDesignMode(nsAString & aDesignMode)
{
  if (mEditingIsOn) {
    aDesignMode.Assign(NS_LITERAL_STRING("on"));
  }
  else {
    aDesignMode.Assign(NS_LITERAL_STRING("off"));
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetDesignMode(const nsAString & aDesignMode)
{
  // get editing session
  if (!mScriptGlobalObject)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocShell> docshell;
  mScriptGlobalObject->GetDocShell(getter_AddRefs(docshell));
  if (!docshell)
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCAutoString url;
  mDocumentURL->GetSpec(url);
  // test if the above works if document.domain is set for Midas document
  // (www.netscape.com --> netscape.com)
  if (!url.Equals("about:blank")) {
    // If we're 'about:blank' then we don't care who can edit us.
    // If we're not about:blank, then we need to check sameOrigin.
    nsCOMPtr<nsIScriptSecurityManager> secMan = 
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = secMan->CheckSameOrigin(nsnull, mDocumentURL);
    if (NS_FAILED(rv))
      return rv;
  }

  nsCOMPtr<nsIEditingSession> editSession = do_GetInterface(docshell);
  if (!editSession) 
    return NS_ERROR_FAILURE;

  if (aDesignMode.Equals(NS_LITERAL_STRING("on"), 
                         nsCaseInsensitiveStringComparator())) {
    // go through hoops to get dom window (see nsHTMLDocument::GetSelection)
    nsCOMPtr<nsIPresShell> shell = (nsIPresShell*)mPresShells.SafeElementAt(0);
    NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);

    nsCOMPtr<nsIPresContext> cx;
    shell->GetPresContext(getter_AddRefs(cx));
    NS_ENSURE_TRUE(cx, NS_ERROR_FAILURE);

    nsCOMPtr<nsISupports> container;
    cx->GetContainer(getter_AddRefs(container));
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

  nsCOMPtr<nsIDocShell> docshell;
  mScriptGlobalObject->GetDocShell(getter_AddRefs(docshell));
  if (!docshell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsICommandManager> mMidasCommandManager = do_GetInterface(docshell);
  if (!mMidasCommandManager)
    return NS_ERROR_FAILURE;

  NS_ADDREF(*aCmdMgr = mMidasCommandManager);
  return NS_OK;
}


struct MidasCommand {
  const char*  incomingCommandString;
  const char*  internalCommandString;
  const char*  internalParamString;
  PRBool       useNewParam;
};

static struct MidasCommand gMidasCommandTable[] = {
  { "bold",          "cmd_bold",            "", PR_TRUE },
  { "italic",        "cmd_italic",          "", PR_TRUE },
  { "underline",     "cmd_underline",       "", PR_TRUE },
  { "strikethrough", "cmd_strikethrough",   "", PR_TRUE },
  { "subscript",     "cmd_subscript",       "", PR_TRUE },
  { "superscript",   "cmd_superscript",     "", PR_TRUE },
  { "cut",           "cmd_cut",             "", PR_TRUE },
  { "copy",          "cmd_copy",            "", PR_TRUE },
  { "delete",        "cmd_delete",          "", PR_TRUE },
  { "selectall",     "cmd_selectall",       "", PR_TRUE },
  { "undo",          "cmd_undo",            "", PR_TRUE },
  { "redo",          "cmd_redo",            "", PR_TRUE },
  { "indent",        "cmd_indent",          "", PR_TRUE },
  { "outdent",       "cmd_outdent",         "", PR_TRUE },
  { "backcolor",     "cmd_backgroundColor", "", PR_FALSE },
  { "forecolor",     "cmd_fontColor",       "", PR_FALSE },
  { "fontname",      "cmd_fontFace",        "", PR_FALSE },
  { "fontsize",      "cmd_fontSize",        "", PR_FALSE },
  { "increasefontsize", "cmd_increaseFont", "", PR_FALSE },
  { "decreasefontsize", "cmd_decreaseFont", "", PR_FALSE },
  { "fontsize",      "cmd_fontSize",        "", PR_FALSE },
  { "inserthorizontalrule", "cmd_hline",    "", PR_TRUE },
  { "createlink",    "cmd_insertLinkNoUI",  "", PR_FALSE },
  { "insertimage",   "cmd_insertImageNoUI", "", PR_FALSE },
  { "justifyleft",   "cmd_align",       "left", PR_TRUE },
  { "justifyright",  "cmd_align",      "right", PR_TRUE },
  { "justifycenter", "cmd_align",     "center", PR_TRUE },
  { "justifyfull",   "cmd_align",    "justify", PR_TRUE },
  { "removeformat",  "cmd_removeStyles",    "", PR_TRUE },
  { "unlink",        "cmd_removeLinks",     "", PR_TRUE },
  { "insertorderedlist",   "cmd_ol",        "", PR_TRUE },
  { "insertunorderedlist", "cmd_ul",        "", PR_TRUE },
  { "insertparagraph", "cmd_paragraphState", "p", PR_TRUE },
  { "formatblock",   "cmd_paragraphState",  "", PR_FALSE },
  { "heading",       "cmd_paragraphState",  "", PR_FALSE },
#if 0
// no editor support to remove alignments right now
  { "justifynone",   "cmd_align",           "", PR_TRUE },

// the following will need special review before being turned on
  { "saveas",        "cmd_saveAs",          "", PR_TRUE },
  { "print",         "cmd_print",           "", PR_TRUE },
  { "paste",         "cmd_paste",           "", PR_TRUE },
#endif
  { NULL, NULL, NULL, PR_FALSE }
};

#define MidasCommandCount ((sizeof(gMidasCommandTable) / sizeof(struct MidasCommand)) - 1)


// this function will return false if the command is not recognized
// inCommandID will be converted as necessary for internal operations
// inParam will be converted as necessary for internal operations
// outParam will be Empty if no parameter is needed
PRBool
nsHTMLDocument::ConvertToMidasInternalCommand(const nsAString & inCommandID, 
                                              const nsAString & inParam,
                                              nsACString& outCommandID,
                                              nsACString& outParam)
{
  nsCAutoString convertedCommandID = NS_ConvertUCS2toUTF8(inCommandID);
  PRUint32 i;
  for (i = 0; i < MidasCommandCount; ++i) {
    if (convertedCommandID.Equals(gMidasCommandTable[i].incomingCommandString,
                                  nsCaseInsensitiveCStringComparator())) {
      // set outCommandID (what we use internally)
      outCommandID.Assign(gMidasCommandTable[i].internalCommandString);

      // set outParam based on the flag from the table
      if (gMidasCommandTable[i].useNewParam) {
        outParam.Assign(gMidasCommandTable[i].internalParamString);
      }
      else {
        // pass through the parameter that was passed to us
        outParam.Assign(NS_ConvertUCS2toUTF8(inParam));
      }

      return PR_TRUE;
    }
  }

  // reset results if the command is not found in our table
  outCommandID.SetLength(0);
  outParam.SetLength(0);
  return PR_FALSE;  
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

  // get command manager and dispatch command to our window if it's acceptable
  nsCOMPtr<nsICommandManager> cmdMgr;
  GetMidasCommandManager(getter_AddRefs(cmdMgr));
  if (!cmdMgr)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(mScriptGlobalObject);
  if (!window)
    return NS_ERROR_FAILURE;

  nsCAutoString cmdToDispatch, paramStr;
  if (!ConvertToMidasInternalCommand(commandID, value, cmdToDispatch, paramStr))
    return NS_ERROR_NOT_IMPLEMENTED;

  nsresult rv;
  if (paramStr.IsEmpty()) {
    rv = cmdMgr->DoCommand(cmdToDispatch.get(), nsnull, window);
  } else {
    // we have a command that requires a parameter, create params
    nsCOMPtr<nsICommandParams> cmdParams = do_CreateInstance(
                                            NS_COMMAND_PARAMS_CONTRACTID, &rv);
    if (!cmdParams)
      return NS_ERROR_OUT_OF_MEMORY;
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
  if (!ConvertToMidasInternalCommand(commandID, commandID,
                                     cmdToDispatch, paramStr))
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

  return NS_ERROR_NOT_IMPLEMENTED;
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
  if (!ConvertToMidasInternalCommand(commandID, commandID, 
                                     cmdToDispatch, paramToCheck))
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
  // Alignment is special because the external api is individual commands
  // but internally we use cmd_align with different parameters.  When getting
  // the state of this command, we need to return the boolean for this 
  // particular alignment rather than the string of 'which alignment is this?'
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
  if (!ConvertToMidasInternalCommand(commandID, commandID,
                                     cmdToDispatch, paramStr))
    return NS_ERROR_NOT_IMPLEMENTED;

  // create params
  nsresult rv;
  nsCOMPtr<nsICommandParams> cmdParams = do_CreateInstance(
                                           NS_COMMAND_PARAMS_CONTRACTID, &rv);
  if (!cmdParams)
    return NS_ERROR_OUT_OF_MEMORY;
  rv = cmdParams->SetCStringValue("state_attribute", paramStr.get());
  if (NS_FAILED(rv))
    return rv;

  rv = cmdMgr->GetCommandState(cmdToDispatch.get(), window, cmdParams);
  if (NS_FAILED(rv))
    return rv;

  char *cStringResult = nsnull;
  rv = cmdParams->GetCStringValue("state_attribute", &cStringResult);
  if (NS_SUCCEEDED(rv) && cStringResult && cStringResult[0]) {
    _retval.Assign(NS_ConvertUTF8toUCS2(cStringResult));
  }
  if (cStringResult) {
    nsMemory::Free(cStringResult);
  }

  return rv;
}
