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
#define NS_IMPL_IDS
#include "nsICharsetAlias.h"
#undef NS_IMPL_IDS

#include "nsCOMPtr.h"
#include "nsIFileChannel.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsHTMLDocument.h"
#include "nsIParser.h"
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
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIURL.h"
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
#include "nsIFormManager.h"
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

#include "nsContentCID.h"
#include "nsIPrompt.h"
//AHMED 12-2 
#ifdef IBMBIDI
#include "nsIUBidiUtils.h"
#endif

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


//#define rickgdebug 1
#ifdef rickgdebug
#include "nsHTMLContentSinkStream.h"
#endif

#define ELEMENT_NOT_IN_TABLE ((nsIContent *)1)

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kCookieServiceCID, NS_COOKIESERVICE_CID);

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

static PRBool
IsNamedItem(nsIContent* aContent, nsIAtom *aTag, nsAWritableString& aName);

static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

static NS_DEFINE_CID(kHTMLStyleSheetCID,NS_HTMLSTYLESHEET_CID);

nsIRDFService* nsHTMLDocument::gRDF;
nsrefcnt       nsHTMLDocument::gRefCntRDFService = 0;

static int PR_CALLBACK
MyPrefChangedCallback(const char*aPrefName, void* instance_data)
{
        nsresult rv;
        nsCOMPtr<nsIPref> prefs = 
                 do_GetService("@mozilla.org/preferences;1", &rv);
        PRUnichar* detector_name = nsnull;
        if(NS_SUCCEEDED(rv) && NS_SUCCEEDED(
             rv = prefs->GetLocalizedUnicharPref("intl.charset.detector",
                                     &detector_name)))
        {
			if(nsCRT::strlen(detector_name) > 0) {
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
  if(doc)
    return doc->QueryInterface(NS_GET_IID(nsIDocument), (void**) aInstancePtrResult);
  return NS_ERROR_OUT_OF_MEMORY;
}

nsHTMLDocument::nsHTMLDocument()
  : nsMarkupDocument(),
    mAttrStyleSheet(nsnull),
    mStyleAttrStyleSheet(nsnull),
    mBaseURL(nsnull),
    mBaseTarget(nsnull),
    mLastModified(nsnull),
    mReferrer(nsnull),
    mIsWriting(0)
{
  mImages = nsnull;
  mApplets = nsnull;
  mEmbeds = nsnull;
  mLinks = nsnull;
  mAnchors = nsnull;
  mLayers = nsnull;
  mParser = nsnull;
  mDTDMode = eDTDMode_quirks;  
  mCSSLoader = nsnull;
  mDocWriteDummyRequest = nsnull;

  mBodyContent = nsnull;
  mForms = nsnull;
  mIsWriting = 0;
  mWriteLevel = 0;

#ifdef IBMBIDI
  mTexttype = IBMBIDI_TEXTTYPE_LOGICAL;
#endif
  
  if (gRefCntRDFService++ == 0)
  {
    nsresult rv;    
		rv = nsServiceManager::GetService(kRDFServiceCID,
						  NS_GET_IID(nsIRDFService),
						  (nsISupports**) &gRDF);

    //nsCOMPtr<nsIRDFService> gRDF(do_GetService(kRDFServiceCID, &rv));
  }

  mDomainWasSet = PR_FALSE; // Bug 13871: Frameset spoofing

  PrePopulateHashTables();
}

nsHTMLDocument::~nsHTMLDocument()
{
  NS_IF_RELEASE(mImages);
  NS_IF_RELEASE(mApplets);
  NS_IF_RELEASE(mEmbeds);
  NS_IF_RELEASE(mLinks);
  NS_IF_RELEASE(mAnchors);
  NS_IF_RELEASE(mLayers);
  if (nsnull != mAttrStyleSheet) {
    mAttrStyleSheet->SetOwningDocument(nsnull);
    NS_RELEASE(mAttrStyleSheet);
  }
  if (nsnull != mStyleAttrStyleSheet) {
    mStyleAttrStyleSheet->SetOwningDocument(nsnull);
    NS_RELEASE(mStyleAttrStyleSheet);
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
  mImageMaps.Clear();
  NS_IF_RELEASE(mForms);
  if (mCSSLoader) {
    mCSSLoader->DropDocumentReference();  // release weak ref
  }

  NS_IF_RELEASE(mBodyContent);

  if (--gRefCntRDFService == 0)
  {     
     nsServiceManager::ReleaseService("@mozilla.org/rdf/rdf-service;1", gRDF);
  }

  InvalidateHashTables();
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


NS_IMETHODIMP
nsHTMLDocument::Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup)
{
  nsresult result = nsDocument::Reset(aChannel, aLoadGroup);
  nsCOMPtr<nsIURI> aURL;

  if (aChannel) {
    result = aChannel->GetURI(getter_AddRefs(aURL));
    if (NS_FAILED(result))
        return result;
  }

  InvalidateHashTables();
  PrePopulateHashTables();

  NS_IF_RELEASE(mImages);
  NS_IF_RELEASE(mApplets);
  NS_IF_RELEASE(mEmbeds);
  NS_IF_RELEASE(mLinks);
  NS_IF_RELEASE(mAnchors);
  NS_IF_RELEASE(mLayers);

  mImageMaps.Clear();
  NS_IF_RELEASE(mForms);

  if (aURL) {
    if (!mAttrStyleSheet) {
      //result = NS_NewHTMLStyleSheet(&mAttrStyleSheet, aURL, this);
      result = nsComponentManager::CreateInstance(kHTMLStyleSheetCID, nsnull,
                                                  NS_GET_IID(nsIHTMLStyleSheet),
                                                  (void**)&mAttrStyleSheet);
      if (NS_SUCCEEDED(result)) {
        result = mAttrStyleSheet->Init(aURL,this);
        if (NS_FAILED(result)) {
          NS_RELEASE(mAttrStyleSheet);
        }
      }
    }
    else {
      result = mAttrStyleSheet->Reset(aURL);
    }
    if (NS_SUCCEEDED(result)) {
      AddStyleSheet(mAttrStyleSheet); // tell the world about our new style sheet

      if (!mStyleAttrStyleSheet) {
        result = NS_NewHTMLCSSStyleSheet(&mStyleAttrStyleSheet, aURL, this);
      }
      else {
        result = mStyleAttrStyleSheet->Reset(aURL);
      }
      if (NS_SUCCEEDED(result)) {
        AddStyleSheet(mStyleAttrStyleSheet); // tell the world about our new style sheet
      }
    }
  }

  NS_ASSERTION(mDocWriteDummyRequest == nsnull, "nsHTMLDocument::Reset() - dummy doc write request still exists!");
  mDocWriteDummyRequest = nsnull;

  return result;
}

NS_IMETHODIMP
nsHTMLDocument::CreateShell(nsIPresContext* aContext,
                            nsIViewManager* aViewManager,
                            nsIStyleSet* aStyleSet,
                            nsIPresShell** aInstancePtrResult)
{
  nsresult result = nsMarkupDocument::CreateShell(aContext,
                                                  aViewManager,
                                                  aStyleSet,
                                                  aInstancePtrResult);

  if (NS_SUCCEEDED(result)) {
    aContext->SetCompatibilityMode(((eDTDMode_strict== mDTDMode) ? 
                                    eCompatibility_Standard : 
                                    eCompatibility_NavQuirks));
  }
  return result;
}

NS_IMETHODIMP
nsHTMLDocument::StartDocumentLoad(const char* aCommand,
                                  nsIChannel* aChannel,
                                  nsILoadGroup* aLoadGroup,
                                  nsISupports* aContainer,
                                  nsIStreamListener **aDocListener,
                                  PRBool aReset)
{
  PRBool needsParser=PR_TRUE;
  if (aCommand)
  {
    nsAutoString command; command.AssignWithConversion(aCommand);
    nsAutoString delayedView; delayedView.AssignWithConversion("view delayedContentLoad");
    if (command.Equals(delayedView)) {
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

  nsAutoString charset; charset.AssignWithConversion("ISO-8859-1"); // fallback value in case webShell return error
  nsCharsetSource charsetSource = kCharsetFromWeakDocTypeDefault;

  nsCOMPtr<nsIURI> aURL;
  rv = aChannel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoString lastModified;
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);

  if (httpChannel) {
    nsXPIDLCString lastModHeader;
    rv = httpChannel->GetResponseHeader("last-modified", 
                                        getter_Copies(lastModHeader));

    if (NS_SUCCEEDED(rv)) {
      lastModified.AssignWithConversion(NS_STATIC_CAST(const char*,
                                                       lastModHeader));
      SetLastModified(lastModified);
    }

    nsXPIDLCString referrerHeader;
    // The misspelled key 'referer' is as per the HTTP spec
    rv = httpChannel->GetRequestHeader("referer", 
                                       getter_Copies(referrerHeader));

    if (NS_SUCCEEDED(rv)) {
      nsAutoString referrer;
      referrer.AssignWithConversion(referrerHeader);

      SetReferrer(referrer);
    }

    if(kCharsetFromHTTPHeader > charsetSource) {
      nsXPIDLCString charsetheader;
      rv = httpChannel->GetCharset(getter_Copies(charsetheader));
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsICharsetAlias> calias(do_CreateInstance(kCharsetAliasCID, &rv));
        if(calias) {
          nsAutoString preferred;
          rv = calias->GetPreferred(NS_ConvertASCIItoUCS2(charsetheader), preferred);
          if(NS_SUCCEEDED(rv)) {
#ifdef DEBUG_charset
            printf("From HTTP Header, charset = %s\n", NS_ConvertUCS2toUTF8(charset).get());
#endif
            charset = preferred;
            charsetSource = kCharsetFromHTTPHeader;
          }
        }  
      }
    } 

    nsCOMPtr<nsICachingChannel> cachingChan = do_QueryInterface(httpChannel);
    if (cachingChan) {
      nsCOMPtr<nsISupports> cacheToken;
      cachingChan->GetCacheToken(getter_AddRefs(cacheToken));
      if (cacheToken)
        cacheDescriptor = do_QueryInterface(cacheToken);
    }

    // Don't propogate the result code beyond here, since it
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
      rv = file->GetLastModificationDate(&modDate);
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

  PRUnichar* requestCharset = nsnull;
  nsCharsetSource requestCharsetSource = kCharsetUninitialized;

  nsCOMPtr <nsIParserFilter> cdetflt;
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
	if(kCharsetFromUserDefault > charsetSource) 
  {
		PRUnichar* defaultCharsetFromWebShell = NULL;
    if (muCV) {
  		rv = muCV->GetDefaultCharacterSet(&defaultCharsetFromWebShell);
      if(NS_SUCCEEDED(rv)) {
        charset = defaultCharsetFromWebShell;
        Recycle(defaultCharsetFromWebShell);
        charsetSource = kCharsetFromUserDefault;
      }
    }
  }

    // for html, we need to find out the Meta tag from the hint.
    if (muCV) {
      rv = muCV->GetHintCharacterSet(&requestCharset);
      if(NS_SUCCEEDED(rv)) {
        rv = muCV->GetHintCharacterSetSource((PRInt32*)(&requestCharsetSource));
 		        if(kCharsetUninitialized != requestCharsetSource) {
 		            muCV->SetHintCharacterSetSource((PRInt32)(kCharsetUninitialized));
 		  			}
      }
    }
    if(NS_SUCCEEDED(rv)) 
    {
      if(requestCharsetSource > charsetSource) 
      {
#ifdef DEBUG_charset
				nsAutoString d(requestCharset);
			  char* cCharset = ToNewCString(d);
			  printf("From request charset, charset = %s req=%d->%d\n", 
			  		cCharset, charsetSource, requestCharsetSource);
			  Recycle(cCharset);
#endif
        charsetSource = requestCharsetSource;
        charset = requestCharset;
        Recycle(requestCharset);
      }
    }
    if(kCharsetFromUserForced > charsetSource) {
      PRUnichar* forceCharsetFromWebShell = NULL;
      if (muCV) {
		    rv = muCV->GetForceCharacterSet(&forceCharsetFromWebShell);
      }
		  if(NS_SUCCEEDED(rv) && (nsnull != forceCharsetFromWebShell)) 
      {
#ifdef DEBUG_charset
				nsAutoString d(forceCharsetFromWebShell);
			  char* cCharset = ToNewCString(d);
			  printf("From force, charset = %s \n", cCharset);
			  Recycle(cCharset);
#endif
				charset = forceCharsetFromWebShell;
        Recycle(forceCharsetFromWebShell);
				//TODO: we should define appropriate constant for force charset
				charsetSource = kCharsetFromUserForced;  
          } else if (dcInfo) {
            nsCOMPtr<nsIAtom> csAtom;
            dcInfo->GetForcedCharset(getter_AddRefs(csAtom));
            if (csAtom.get() != NULL) {
              csAtom->ToString(charset);
              charsetSource = kCharsetFromUserForced;  
              dcInfo->SetForcedCharset(NULL);
            }
          }
    }

    nsresult rv_detect = NS_OK;
    if(! gInitDetector)
    {
      nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_CONTRACTID));
      if(pref)
      {
        PRUnichar* detector_name = nsnull;
        if(NS_SUCCEEDED(
             rv_detect = pref->GetLocalizedUnicharPref("intl.charset.detector",
                                 &detector_name)))
        {
          PL_strncpy(g_detector_contractid, NS_CHARSET_DETECTOR_CONTRACTID_BASE,DETECTOR_CONTRACTID_MAX);
          PL_strncat(g_detector_contractid, NS_ConvertUCS2toUTF8(detector_name).get(),DETECTOR_CONTRACTID_MAX);
          gPlugDetector = PR_TRUE;
          PR_FREEIF(detector_name);
        }
        pref->RegisterCallback("intl.charset.detector", MyPrefChangedCallback, nsnull);
      }
      gInitDetector = PR_TRUE;
    }

    // don't try to access bookmarks if we are loading about:blank...it's not going
    // to give us anything useful and it causes Bug #44397. At the same time, I'm loath to do something
    // like this because I think it's really bogus that layout is depending on bookmarks. This is very evil.
    nsXPIDLCString scheme;
    aURL->GetScheme(getter_Copies(scheme));

    nsXPIDLCString urlSpec;
    aURL->GetSpec(getter_Copies(urlSpec));

    if (cacheDescriptor && urlSpec)
    {
      if (kCharsetFromCache > charsetSource) 
      {
        nsXPIDLCString cachedCharset;
        rv = cacheDescriptor->GetMetaDataElement("charset",
                                                 getter_Copies(cachedCharset));
        if (NS_SUCCEEDED(rv) && PL_strlen(cachedCharset) > 0)
        {
          charset.AssignWithConversion(cachedCharset);
          charsetSource = kCharsetFromCache;
        }
      }    
      rv = NS_OK;
    }

    if (scheme && nsCRT::strcasecmp("about", scheme) && (kCharsetFromBookmarks > charsetSource))
    {
      nsCOMPtr<nsIRDFDataSource>  datasource;
      if (gRDF && NS_SUCCEEDED(rv_detect = gRDF->GetDataSource("rdf:bookmarks", getter_AddRefs(datasource))))
      {
           nsCOMPtr<nsIBookmarksService>   bookmarks = do_QueryInterface(datasource);
           if (bookmarks)
           {

                 if (urlSpec)
                 {
                            nsXPIDLString pBookmarkedCharset;

                            if (NS_SUCCEEDED(rv = bookmarks->GetLastCharset(urlSpec, getter_Copies(pBookmarkedCharset))) && 
                               (rv != NS_RDF_NO_VALUE))
                            {
                              charset = pBookmarkedCharset;
                              charsetSource = kCharsetFromBookmarks;
                            }    

                  } 
           }
       }
    } 

    if (kCharsetFromParentFrame > charsetSource) {
      if (dcInfo) {
        nsCOMPtr<nsIAtom> csAtom;
        dcInfo->GetParentCharset(getter_AddRefs(csAtom));
        if (csAtom) {
          csAtom->ToString(charset);
          charsetSource = kCharsetFromParentFrame;  

          // printf("### 0 >>> Having parent CS = %s\n", NS_LossyConvertUCS2toASCII(charset).get());
        }
      }
    }

    if((kCharsetFromAutoDetection > charsetSource )  && gPlugDetector)
    {
      nsCOMPtr <nsICharsetDetector> cdet = do_CreateInstance(g_detector_contractid, 
                                                             &rv_detect);
      if(NS_SUCCEEDED( rv_detect )) 
      {
        cdetflt = do_CreateInstance(NS_CHARSET_DETECTION_ADAPTOR_CONTRACTID,
			            &rv_detect);
        if(NS_SUCCEEDED( rv_detect )) 
        {
          nsCOMPtr<nsICharsetDetectionAdaptor> adp = do_QueryInterface(cdetflt, 
                                                                       &rv_detect);
          if(cdetflt && NS_SUCCEEDED( rv_detect ))
          {
            nsCOMPtr<nsIWebShellServices> wss = do_QueryInterface(docShell,
                                                                  &rv_detect);
            if( NS_SUCCEEDED( rv_detect )) 
            {
              rv_detect = adp->Init(wss, cdet, (nsIDocument*)this, 
                     mParser, charset.get(),aCommand);
            }
          }
        }
      }
      else 
      {
        // IF we cannot create the detector, don't bother to 
        // create one next time.
        gPlugDetector = PR_FALSE;
      }
  }
#endif

  if (NS_FAILED(rv)) {
    return rv;
  }
//ahmed
#ifdef IBMBIDI
  // Check if 864 but in Implicit mode !
  if( (mTexttype == IBMBIDI_TEXTTYPE_LOGICAL)&&(charset.EqualsIgnoreCase("ibm864")) )
    charset.AssignWithConversion("IBM864i");
#endif // IBMBIDI

  rv = this->SetDocumentCharacterSet(charset);
  if (NS_FAILED(rv)) {
    return rv;
  }

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

    if(cdetflt)
      // The current implementation for SetParserFilter needs to 
      // be changed to be more XPCOM friendly. See bug #40149
      nsCOMPtr<nsIParserFilter> oldFilter = getter_AddRefs(mParser->SetParserFilter(cdetflt));

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
    rv = NS_NewHTMLContentSink(getter_AddRefs(sink), this, aURL, webShell,aChannel);
    if (NS_FAILED(rv)) { return rv; }
    NS_ASSERTION(sink, "null sink with successful result from factory method");
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

NS_IMETHODIMP
nsHTMLDocument::EndLoad()
{
  NS_IF_RELEASE(mParser);
  return nsDocument::EndLoad();
}

NS_IMETHODIMP
nsHTMLDocument::SetTitle(const nsAReadableString& aTitle)
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
  if (mImageMaps.AppendElement(aMap)) {
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
  mImageMaps.RemoveElement(aMap, 0);
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
  mImageMaps.Count(&n);
  for (i = 0; i < n; i++) {
    nsCOMPtr<nsIDOMHTMLMapElement> map;
    mImageMaps.QueryElementAt(i, NS_GET_IID(nsIDOMHTMLMapElement), getter_AddRefs(map));
    if (map && NS_SUCCEEDED(map->GetName(name))) {
      if (name.EqualsIgnoreCase(aMapName)) {
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
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mAttrStyleSheet;
  if (nsnull == mAttrStyleSheet) {
    return NS_ERROR_NOT_AVAILABLE;  // probably not the right error...
  }
  else {
    NS_ADDREF(mAttrStyleSheet);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mStyleAttrStyleSheet;
  if (nsnull == mStyleAttrStyleSheet) {
    return NS_ERROR_NOT_AVAILABLE;  // probably not the right error...
  }
  else {
    NS_ADDREF(mStyleAttrStyleSheet);
  }
  return NS_OK;
}

void
nsHTMLDocument::InternalAddStyleSheet(nsIStyleSheet* aSheet)  // subclass hook for sheet ordering
{
  if (aSheet == mAttrStyleSheet) {  // always first
    mStyleSheets.InsertElementAt(aSheet, 0);
  }
  else if (aSheet == mStyleAttrStyleSheet) {  // always last
    mStyleSheets.AppendElement(aSheet);
  }
  else {
    if (mStyleAttrStyleSheet == mStyleSheets.ElementAt(mStyleSheets.Count() - 1)) {
      // keep attr sheet last
      mStyleSheets.InsertElementAt(aSheet, mStyleSheets.Count() - 1);
    }
    else {
      mStyleSheets.AppendElement(aSheet);
    }
  }
}

void
nsHTMLDocument::InternalInsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex)
{
  mStyleSheets.InsertElementAt(aSheet, aIndex + 1); // offset one for the attr style sheet
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
nsHTMLDocument::GetBaseTarget(nsAWritableString& aTarget)
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
nsHTMLDocument::SetBaseTarget(const nsAReadableString& aTarget)
{
  if (0 < aTarget.Length()) {
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
nsHTMLDocument::SetLastModified(const nsAReadableString& aLastModified)
{
  if (0 < aLastModified.Length()) {
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
nsHTMLDocument::SetReferrer(const nsAReadableString& aReferrer)
{
  if (0 < aReferrer.Length()) {
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
    mCSSLoader->SetQuirkMode(PRBool(eDTDMode_strict!= mDTDMode));
  }
  aLoader = mCSSLoader;
  NS_IF_ADDREF(aLoader);
  return result;
}


NS_IMETHODIMP
nsHTMLDocument::GetDTDMode(nsDTDMode& aMode)
{
  aMode = mDTDMode;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetDTDMode(nsDTDMode aMode)
{
  mDTDMode = aMode;
  if (mCSSLoader) {
    mCSSLoader->SetQuirkMode(PRBool(eDTDMode_strict!= mDTDMode));
  }

  nsIPresShell* shell = (nsIPresShell*) mPresShells.ElementAt(0);
  if (nsnull != shell) {
    nsCOMPtr<nsIPresContext> pc;
    shell->GetPresContext(getter_AddRefs(pc));
     if (pc) {
        pc->SetCompatibilityMode(((eDTDMode_strict== mDTDMode) ? 
                                    eCompatibility_Standard : 
                                    eCompatibility_NavQuirks));
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
                                 nsIAtom* aAttribute, PRInt32 aModType, PRInt32 aHint)
{
  NS_ABORT_IF_FALSE(aContent, "Null content!");

  // XXX: Check namespaces!

  if (aAttribute == nsHTMLAtoms::name) {
    nsCOMPtr<nsIAtom> tag;
    nsAutoString value;

    aContent->GetTag(*getter_AddRefs(tag));

    if (IsNamedItem(aContent, tag, value)) {
      nsresult rv = AddToNameTable(value, aContent);

      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  } else if (aAttribute == nsHTMLAtoms::id) {
    nsAutoString value;

    aContent->GetAttr(aNameSpaceID, nsHTMLAtoms::id, value);

    if (!value.IsEmpty()) {
      nsresult rv = AddToIdTable(value, aContent, PR_TRUE);

      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  return nsDocument::AttributeChanged(aContent, aNameSpaceID, aAttribute, aModType, 
                                      aHint);
}

NS_IMETHODIMP 
nsHTMLDocument::FlushPendingNotifications(PRBool aFlushReflows)
{
  // Determine if it is safe to flush the sink
  // by determining if it safe to flush all the presshells.
  PRBool isSafeToFlush = PR_TRUE;
  if (aFlushReflows) {
    PRInt32 i = 0, n = mPresShells.Count();
    while ((i < n) && (isSafeToFlush)) {
      nsIPresShell* shell = NS_STATIC_CAST(nsIPresShell*, mPresShells[i]);
      if (shell) {
        shell->IsSafeToFlush(isSafeToFlush);
      }
      ++i;
    }
  }

  nsresult result = NS_OK;
  if ((isSafeToFlush) && (mParser)) {
    nsCOMPtr<nsIContentSink> sink;
    
    // XXX Ack! Parser doesn't addref sink before passing it back
    sink = mParser->GetContentSink();
    if (sink) {
      result = sink->FlushPendingNotifications();
    }
  }

  if (NS_SUCCEEDED(result)) {
    result = nsDocument::FlushPendingNotifications(aFlushReflows);
  }

  return result;
}

NS_IMETHODIMP
nsHTMLDocument::CreateElementNS(const nsAReadableString& aNamespaceURI,
                                const nsAReadableString& aQualifiedName,
                                nsIDOMElement** aReturn)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsINodeInfo> nodeInfo;
  rv = mNodeInfoManager->GetNodeInfo(aQualifiedName, aNamespaceURI,
                                     *getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 namespaceID;
  nodeInfo->GetNamespaceID(namespaceID);

  nsCOMPtr<nsIContent> content;
  if (namespaceID == kNameSpaceID_HTML) {
    nsCOMPtr<nsIHTMLContent> htmlContent;

    rv = NS_CreateHTMLElement(getter_AddRefs(htmlContent), nodeInfo, PR_FALSE);
    content = do_QueryInterface(htmlContent);
  }
  else {
    nsCOMPtr<nsIXMLContent> xmlContent;
    rv = NS_NewXMLElement(getter_AddRefs(xmlContent), nodeInfo);
    content = do_QueryInterface(xmlContent);
  }

  NS_ENSURE_SUCCESS(rv, rv);

  content->SetContentID(mNextContentID++);

  return content->QueryInterface(NS_GET_IID(nsIDOMElement), (void**)aReturn);
}


//
// nsIDOMDocument interface implementation
//
NS_IMETHODIMP    
nsHTMLDocument::CreateElement(const nsAReadableString& aTagName, 
                              nsIDOMElement** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  NS_ENSURE_TRUE(aTagName.Length(), NS_ERROR_DOM_INVALID_CHARACTER_ERR);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsAutoString tmp(aTagName);
  tmp.ToLowerCase();

  mNodeInfoManager->GetNodeInfo(tmp, nsnull, kNameSpaceID_None,
                                *getter_AddRefs(nodeInfo));

  nsCOMPtr<nsIHTMLContent> content;
  nsresult rv = NS_CreateHTMLElement(getter_AddRefs(content), nodeInfo, PR_FALSE);
  if (NS_SUCCEEDED(rv)) {
    content->SetContentID(mNextContentID++);
    rv = content->QueryInterface(NS_GET_IID(nsIDOMElement), (void**)aReturn);
  }
  return rv;
}

NS_IMETHODIMP    
nsHTMLDocument::CreateProcessingInstruction(const nsAReadableString& aTarget, 
                                            const nsAReadableString& aData, 
                                            nsIDOMProcessingInstruction** aReturn)
{
  // There are no PIs for HTML
  *aReturn = nsnull;
  
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP    
nsHTMLDocument::CreateCDATASection(const nsAReadableString& aData, 
                                   nsIDOMCDATASection** aReturn)
{
  // There are no CDATASections in HTML
  *aReturn = nsnull;

  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}
 
NS_IMETHODIMP    
nsHTMLDocument::CreateEntityReference(const nsAReadableString& aName, 
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
nsHTMLDocument::CreateComment(const nsAReadableString& aData, nsIDOMComment** aReturn)
{ 
  return nsDocument::CreateComment(aData, aReturn); 
}

NS_IMETHODIMP    
nsHTMLDocument::CreateAttribute(const nsAReadableString& aName, nsIDOMAttr** aReturn)
{ 
  return nsDocument::CreateAttribute(aName, aReturn); 
}

NS_IMETHODIMP    
nsHTMLDocument::CreateTextNode(const nsAReadableString& aData, nsIDOMText** aReturn)
{ 
  return nsDocument::CreateTextNode(aData, aReturn); 
}

NS_IMETHODIMP    
nsHTMLDocument::GetElementsByTagName(const nsAReadableString& aTagname, nsIDOMNodeList** aReturn)
{ 
  nsAutoString tmp(aTagname);
  tmp.ToLowerCase(); // HTML elements are lower case internally.
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
nsHTMLDocument::GetNodeName(nsAWritableString& aNodeName)
{ 
  return nsDocument::GetNodeName(aNodeName); 
}

NS_IMETHODIMP    
nsHTMLDocument::GetNodeValue(nsAWritableString& aNodeValue)
{ 
  return nsDocument::GetNodeValue(aNodeValue); 
}

NS_IMETHODIMP    
nsHTMLDocument::SetNodeValue(const nsAReadableString& aNodeValue)
{ 
  return nsDocument::SetNodeValue(aNodeValue); 
}

NS_IMETHODIMP    
nsHTMLDocument::GetNodeType(PRUint16* aNodeType)
{ 
  return nsDocument::GetNodeType(aNodeType); 
}

NS_IMETHODIMP
nsHTMLDocument::GetNamespaceURI(nsAWritableString& aNamespaceURI)
{ 
  return nsDocument::GetNamespaceURI(aNamespaceURI);
}

NS_IMETHODIMP
nsHTMLDocument::GetPrefix(nsAWritableString& aPrefix)
{
  return nsDocument::GetPrefix(aPrefix);
}

NS_IMETHODIMP
nsHTMLDocument::SetPrefix(const nsAReadableString& aPrefix)
{
  return nsDocument::SetPrefix(aPrefix);
}

NS_IMETHODIMP
nsHTMLDocument::GetLocalName(nsAWritableString& aLocalName)
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
nsHTMLDocument::IsSupported(const nsAReadableString& aFeature,
                            const nsAReadableString& aVersion,
                            PRBool* aReturn)
{
  return nsDocument::IsSupported(aFeature, aVersion, aReturn);
}

NS_IMETHODIMP
nsHTMLDocument::GetBaseURI(nsAWritableString &aURI)
{
  aURI.Truncate();
  nsCOMPtr<nsIURI> uri(do_QueryInterface(mBaseURL ? mBaseURL : mDocumentURL));
  if (uri) {
    nsXPIDLCString spec;
    uri->GetSpec(getter_Copies(spec));
    if (spec) {
      CopyASCIItoUCS2(nsDependentCString(spec), aURI);
    }
  }
  return NS_OK;
}


NS_IMETHODIMP    
nsHTMLDocument::LookupNamespacePrefix(const nsAReadableString& aNamespaceURI,
                                      nsAWritableString& aPrefix) 
{
  aPrefix.Truncate();
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::LookupNamespaceURI(const nsAReadableString& aNamespacePrefix,
                                   nsAWritableString& aNamespaceURI)
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
nsHTMLDocument::GetTitle(nsAWritableString& aTitle)
{
  return nsDocument::GetTitle(aTitle);
}

NS_IMETHODIMP    
nsHTMLDocument::GetReferrer(nsAWritableString& aReferrer)
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
nsHTMLDocument::GetDomain(nsAWritableString& aDomain)
{
  nsCOMPtr<nsIURI> uri;
  if (NS_FAILED(GetDomainURI(getter_AddRefs(uri))))
    return NS_ERROR_FAILURE;

  char *hostName;
  if (NS_FAILED(uri->GetHost(&hostName)))
    return NS_ERROR_FAILURE;
  aDomain.Assign(NS_ConvertASCIItoUCS2(hostName));
  nsCRT::free(hostName);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::SetDomain(const nsAReadableString& aDomain)
{
  // Check new domain
  
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
    if (suffix.EqualsIgnoreCase(nsString(aDomain)) &&
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
  nsXPIDLCString scheme;
  if (NS_FAILED(uri->GetScheme(getter_Copies(scheme))))
    return NS_ERROR_FAILURE;
  nsXPIDLCString path;
  if (NS_FAILED(uri->GetPath(getter_Copies(path))))
    return NS_ERROR_FAILURE;
  nsAutoString newURIString; newURIString.AssignWithConversion( NS_STATIC_CAST(const char*, scheme) );
  newURIString.AppendWithConversion("://");
  newURIString += aDomain;
  newURIString.AppendWithConversion(path);
  nsIURI *newURI;
  if (NS_FAILED(NS_NewURI(&newURI, newURIString)))
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
nsHTMLDocument::GetURL(nsAWritableString& aURL)
{
  if (nsnull != mDocumentURL) {
    char* str;
    mDocumentURL->GetSpec(&str);
    aURL.Assign(NS_ConvertASCIItoUCS2(str));
    nsCRT::free(str);
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

  nsAutoString bodyStr;
  bodyStr.AssignWithConversion("BODY");

  nsCOMPtr<nsIDOMNode> child;
  root->GetFirstChild(getter_AddRefs(child));

  while (child) {
    nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(child));

    if (domElement) {
      nsAutoString tagName;

      domElement->GetTagName(tagName);

      if (bodyStr.EqualsIgnoreCase(tagName)) {
        nsCOMPtr<nsIDOMNode> ret;

        nsresult rv = root->ReplaceChild(aBody, child, getter_AddRefs(ret));

        NS_IF_RELEASE(mBodyContent);

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
  if (nsnull == mImages) {
    mImages = new nsContentList(this, nsHTMLAtoms::img, kNameSpaceID_Unknown);
    if (nsnull == mImages) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mImages);
  }

  *aImages = (nsIDOMHTMLCollection *)mImages;
  NS_ADDREF(mImages);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::GetApplets(nsIDOMHTMLCollection** aApplets)
{
  if (nsnull == mApplets) {
    mApplets = new nsContentList(this, nsHTMLAtoms::applet, kNameSpaceID_Unknown);
    if (nsnull == mApplets) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mApplets);
  }

  *aApplets = (nsIDOMHTMLCollection *)mApplets;
  NS_ADDREF(mApplets);

  return NS_OK;
}

PRBool
nsHTMLDocument::MatchLinks(nsIContent *aContent, nsString* aData)
{
  nsIAtom *name;
  aContent->GetTag(name);
  nsAutoString attr;
  PRBool result = PR_FALSE;
  
  if ((nsnull != name) && 
      ((nsHTMLAtoms::area == name) || (nsHTMLAtoms::a == name)) &&
      (NS_CONTENT_ATTR_HAS_VALUE == aContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::href, attr))) {
      result = PR_TRUE;
  }

  NS_IF_RELEASE(name);
  return result;
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
  nsIAtom *name;
  aContent->GetTag(name);
  nsAutoString attr;
  PRBool result = PR_FALSE;
  
  if ((nsnull != name) && 
      (nsHTMLAtoms::a == name) &&
      (NS_CONTENT_ATTR_HAS_VALUE == aContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::name, attr))) {
      result = PR_TRUE;
  }

  NS_IF_RELEASE(name);
  return result;
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
nsHTMLDocument::GetCookie(nsAWritableString& aCookie)
{
  nsresult result = NS_OK;
  nsAutoString str;

  aCookie.Truncate(); // clear current cookie in case service fails; no cookie isn't an error condition.

  nsCOMPtr<nsICookieService> service = do_GetService(kCookieServiceCID, &result);
  if (NS_SUCCEEDED(result) && service && mDocumentURL) {
    nsXPIDLCString cookie;
    result = service->GetCookieString(mDocumentURL, getter_Copies(cookie));
    if (NS_SUCCEEDED(result) && cookie)
      CopyASCIItoUCS2(nsDependentCString(cookie), aCookie);
  }
  return result;
}

NS_IMETHODIMP    
nsHTMLDocument::SetCookie(const nsAReadableString& aCookie)
{
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
    result = NS_ERROR_OUT_OF_MEMORY;
    char* cookie = ToNewCString(aCookie);
    if (cookie) {
      result = service->SetCookieString(mDocumentURL, prompt, cookie);
      nsCRT::free(cookie);
    }
  }
  return result;
}

// static
nsresult
nsHTMLDocument::GetSourceDocumentURL(JSContext* cx,
                                     nsIURI** sourceURL)
{
  // XXX Tom said this reminded him of the "Six Degrees of
  // Kevin Bacon" game. We try to get from here to there using
  // whatever connections possible. The problem is that this
  // could break if any of the connections along the way change.
  // I wish there were a better way.
  *sourceURL = nsnull;

  if (!cx) {
    return NS_OK;
  }

  // We need to use the dynamically scoped global and assume that the 
  // current JSContext is a DOM context with a nsIScriptGlobalObject so
  // that we can get the url of the caller.
  // XXX This will fail on non-DOM contexts :(

  nsCOMPtr<nsIScriptGlobalObject> global;
  nsContentUtils::GetDynamicScriptGlobal(cx, getter_AddRefs(global));

  nsCOMPtr<nsIDOMWindowInternal> window(do_QueryInterface(global));

  if (!window) {
    return NS_OK; // Can't get the source URI if we can't get the window.
  }

  nsCOMPtr<nsIDOMDocument> dom_doc;
  window->GetDocument(getter_AddRefs(dom_doc));

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(dom_doc));

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

  // Stop current loads targetted at the window this document is in.
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

  result = NS_OpenURI(getter_AddRefs(channel), aSourceURL, nsnull, group);

  if (NS_FAILED(result)) return result;

  //Before we reset the doc notify the globalwindow of the change.

  if (mScriptGlobalObject) {
    //Hold onto ourselves on the offchance that we're down to one ref

    nsCOMPtr<nsIDOMDocument> kungFuDeathGrip =
      do_QueryInterface((nsIHTMLDocument*)this);

    result = mScriptGlobalObject->SetNewDocument(kungFuDeathGrip, PR_FALSE);

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
    if (mChildren) {
      mChildren->RemoveElement(root);
    }

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

    mChildren->AppendElement(root);
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
    nsIPresShell* shell = (nsIPresShell*) mPresShells.ElementAt(0);
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

  // Add a doc write dummy request into the document load group
  NS_ASSERTION(mDocWriteDummyRequest == nsnull, "nsHTMLDocument::OpenCommon(): doc write dummy request exists!");
  AddDocWriteDummyRequest();

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
  nsresult result = NS_OK;
  nsCOMPtr<nsIURI> sourceURL;

  // XXX The URL of the newly created document will match
  // that of the source document. Is this right?

  // XXX: This service should be cached.
  nsCOMPtr<nsIJSContextStack>
    stack(do_GetService("@mozilla.org/js/xpc/ContextStack;1", &result));

  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;

  JSContext *cx;

  if (NS_FAILED(stack->Peek(&cx)))
    return NS_ERROR_FAILURE;

  result = GetSourceDocumentURL(cx, getter_AddRefs(sourceURL));
  // Recover if we had a problem obtaining the source URL
  if (!sourceURL) {
    result = NS_NewURI(getter_AddRefs(sourceURL), "about:blank");
  }

  if (NS_SUCCEEDED(result)) {
    result = OpenCommon(sourceURL);
  }

  QueryInterface(NS_GET_IID(nsIDOMDocument), (void **)aReturn);

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
    nsAutoString emptyStr; emptyStr.AssignWithConversion("</HTML>");
    mWriteLevel++;
    result = mParser->Parse(emptyStr, NS_GENERATE_PARSER_KEY(), 
                            NS_ConvertASCIItoUCS2("text/html"), PR_FALSE,
                            PR_TRUE);
    mWriteLevel--;
    mIsWriting = 0;
    NS_IF_RELEASE(mParser);

    // XXX Make sure that all the document.written content is reflowed.
    // We should remove this call once we change nsHTMLDocument::OpenCommon() so that it
    // completely destroys the earlier document's content and frame hierarchy.  Right now,
    // it re-uses the earlier document's root content object and corresponding frame objects.
    // These re-used frame objects think that they have already been reflowed, so they drop
    // initial reflows.  For certain cases of document.written content, like a frameset document,
    // the dropping of the initial reflow means that we end up in document.close() without
    // appended any reflow commands to the reflow queue and, consequently, without adding the
    // dummy layout request to the load group.  Since the dummy layout request is not added to
    // the load group, the onload handler of the frameset fires before the frames get reflowed
    // and loaded.  That is the long explanation for why we need this one line of code here!
    FlushPendingNotifications();

    // Remove the doc write dummy request from the document load group
    // that we added in OpenCommon().  If all other requests between
    // document.open() and document.close() have completed, then this
    // method should cause the firing of an onload event.
    NS_ASSERTION(mDocWriteDummyRequest, "nsHTMLDocument::Close(): Trying to remove non-existent doc write dummy request!");  
    RemoveDocWriteDummyRequest();
    NS_ASSERTION(mDocWriteDummyRequest == nsnull, "nsHTMLDocument::Close(): Doc write dummy request could not be removed!");  
  }

  return NS_OK;
}

nsresult
nsHTMLDocument::WriteCommon(const nsAReadableString& aText,
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

  if (aNewlineTerminate) {
    rv = mParser->Parse(aText + NS_LITERAL_STRING("\n"),
                        NS_GENERATE_PARSER_KEY(),
                        NS_LITERAL_STRING("text/html"), PR_FALSE,
                        (!mIsWriting || (mWriteLevel > 1)));
  } else {
    rv = mParser->Parse(aText,
                        NS_GENERATE_PARSER_KEY(),
                        NS_LITERAL_STRING("text/html"), PR_FALSE,
                        (!mIsWriting || (mWriteLevel > 1)));
  }
  mWriteLevel--;

  return rv;
}

NS_IMETHODIMP
nsHTMLDocument::Write(const nsAReadableString& aText)
{
  return WriteCommon(aText, PR_FALSE);
}

NS_IMETHODIMP    
nsHTMLDocument::Writeln(const nsAReadableString& aText)
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

  nsXPIDLCString spec;

  if (mDocumentURL) {
    rv = mDocumentURL->GetSpec(getter_Copies(spec));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!mDocumentURL || nsCRT::strcasecmp(spec, "about:blank") == 0) {
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

        NS_IF_RELEASE(mPrincipal);
        mPrincipal = subject;
        NS_ADDREF(mPrincipal);
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
nsHTMLDocument::MatchId(nsIContent *aContent, const nsAReadableString& aId)
{
  nsAutoString value;

  nsresult rv = aContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::id,
                                  value);

  if (rv == NS_CONTENT_ATTR_HAS_VALUE && aId.Equals(value)) {
    return aContent;
  }
  
  nsIContent *result = nsnull;
  PRInt32 i, count;

  aContent->ChildCount(count);
  for (i = 0; i < count && result == nsnull; i++) {
    nsIContent *child;
    aContent->ChildAt(i, child);
    result = MatchId(child, aId);
    NS_RELEASE(child);
  }  

  return result;
}

NS_IMETHODIMP    
nsHTMLDocument::GetElementById(const nsAReadableString& aElementId,
                               nsIDOMElement** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;
  
  if(!mRootContent) {
    return NS_OK;
  }

  NS_WARN_IF_FALSE(!aElementId.IsEmpty(), "getElementById(\"\"), fix caller?");
  if (aElementId.IsEmpty())
    return NS_OK;

  nsStringKey key(aElementId);

  nsIContent *e = NS_STATIC_CAST(nsIContent *, mIdHashTable.Get(&key));

  if (e == ELEMENT_NOT_IN_TABLE) {
    // We're looked for this id before and we didn't find it, so it's
    // not in the document now either

    return NS_OK;
  } else if (!e) {
    e = MatchId(mRootContent, aElementId);

    if (!e) {
      // There is no element with the given id in the document, cache
      // the fact that it's not in the document
      mIdHashTable.Put(&key, ELEMENT_NOT_IN_TABLE);

      return NS_OK;
    }

    // We found an element with a matching id, store that in the hash
    mIdHashTable.Put(&key, e);
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
nsHTMLDocument::CreateAttributeNS(const nsAReadableString& aNamespaceURI,
                                  const nsAReadableString& aQualifiedName,
                                  nsIDOMAttr** aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLDocument::GetElementsByTagNameNS(const nsAReadableString& aNamespaceURI,
                                       const nsAReadableString& aLocalName,
                                       nsIDOMNodeList** aReturn)
{
  nsAutoString tmp(aLocalName);
  tmp.ToLowerCase(); // HTML elements are lower case internally.
  return nsDocument::GetElementsByTagNameNS(aNamespaceURI, tmp,
                                            aReturn); 
}

PRBool
nsHTMLDocument::MatchNameAttribute(nsIContent* aContent, nsString* aData)
{
  nsAutoString name;

  nsresult rv = aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::name,
                                  name);

  if (NS_SUCCEEDED(rv) && aData && name.Equals(*aData)) {
    return PR_TRUE;
  }
  else {
    return PR_FALSE;
  }
}

NS_IMETHODIMP    
nsHTMLDocument::GetElementsByName(const nsAReadableString& aElementName, 
                                  nsIDOMNodeList** aReturn)
{
  nsContentList* elements = new nsContentList(this, MatchNameAttribute,
                                              aElementName);
  NS_ENSURE_TRUE(elements, NS_ERROR_OUT_OF_MEMORY);

  *aReturn = elements;
  NS_ADDREF(*aReturn);

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

  nsresult result;
  result = FlushPendingNotifications();
  if (NS_FAILED(result))
    return NS_OK;

  // Find the <body> element: this is what we'll want to use for the
  // document's width and height values.
  if (mBodyContent == nsnull && PR_FALSE == GetBodyContent()) {
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

        result = view->GetDimensions(&size.width, &size.height);
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

  nsCOMPtr<nsIPresShell> shell;
  nsresult result = NS_OK;

  // We make the assumption that the first presentation shell
  // is the one for which we need information.
  GetShellAt(0, getter_AddRefs(shell));
  if (shell) {
    PRInt32 width, height;

    result = GetPixelDimensions(shell, &width, &height);
    *aWidth = width;
  } else
    *aWidth = 0;

  return result;
}

NS_IMETHODIMP    
nsHTMLDocument::GetHeight(PRInt32* aHeight)
{
  NS_ENSURE_ARG_POINTER(aHeight);

  nsCOMPtr<nsIPresShell> shell;
  nsresult result = NS_OK;

  // We make the assumption that the first presentation shell
  // is the one for which we need information.
  GetShellAt(0, getter_AddRefs(shell));
  if (shell) {
    PRInt32 width, height;

    result = GetPixelDimensions(shell, &width, &height);
    *aHeight = height;
  } else
    *aHeight = 0;

  return result;
}

NS_IMETHODIMP
nsHTMLDocument::GetAlinkColor(nsAWritableString& aAlinkColor)
{
  nsresult result = NS_OK;
  nsIDOMHTMLBodyElement* body;

  aAlinkColor.Truncate();
  result = GetBodyElement(&body);
  if (NS_OK == result) {
    result = body->GetALink(aAlinkColor);
    NS_RELEASE(body);
  }
  else if (nsnull != mAttrStyleSheet) {
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
nsHTMLDocument::SetAlinkColor(const nsAReadableString& aAlinkColor)
{
  nsresult result = NS_OK;
  nsIDOMHTMLBodyElement* body;

  result = GetBodyElement(&body);
  if (NS_OK == result) {
    result = body->SetALink(aAlinkColor);
    NS_RELEASE(body);
  }
  else if (nsnull != mAttrStyleSheet) {
    nsHTMLValue value;
  
    if (nsGenericHTMLElement::ParseColor(aAlinkColor, this, value)) {
      mAttrStyleSheet->SetActiveLinkColor(value.GetColorValue());
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::GetLinkColor(nsAWritableString& aLinkColor)
{
  nsresult result = NS_OK;
  nsIDOMHTMLBodyElement* body;

  aLinkColor.Truncate();
  result = GetBodyElement(&body);
  if (NS_OK == result) {
    result = body->GetLink(aLinkColor);
    NS_RELEASE(body);
  }
  else if (nsnull != mAttrStyleSheet) {
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
nsHTMLDocument::SetLinkColor(const nsAReadableString& aLinkColor)
{
  nsresult result = NS_OK;
  nsIDOMHTMLBodyElement* body;

  result = GetBodyElement(&body);
  if (NS_OK == result) {
    result = body->SetLink(aLinkColor);
    NS_RELEASE(body);
  }
  else if (nsnull != mAttrStyleSheet) {
    nsHTMLValue value;
    if (nsGenericHTMLElement::ParseColor(aLinkColor, this, value)) {
      mAttrStyleSheet->SetLinkColor(value.GetColorValue());
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::GetVlinkColor(nsAWritableString& aVlinkColor)
{
  nsresult result = NS_OK;
  nsIDOMHTMLBodyElement* body;

  aVlinkColor.Truncate();
  result = GetBodyElement(&body);
  if (NS_OK == result) {
    result = body->GetVLink(aVlinkColor);
    NS_RELEASE(body);
  }
  else if (nsnull != mAttrStyleSheet) {
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
nsHTMLDocument::SetVlinkColor(const nsAReadableString& aVlinkColor)
{
  nsresult result = NS_OK;
  nsIDOMHTMLBodyElement* body;

  result = GetBodyElement(&body);
  if (NS_OK == result) {
    result = body->SetVLink(aVlinkColor);
    NS_RELEASE(body);
  }
  else if (nsnull != mAttrStyleSheet) {
    nsHTMLValue value;
    if (nsGenericHTMLElement::ParseColor(aVlinkColor, this, value)) {
      mAttrStyleSheet->SetVisitedLinkColor(value.GetColorValue());
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::GetBgColor(nsAWritableString& aBgColor)
{
  nsresult result = NS_OK;
  nsIDOMHTMLBodyElement* body;

  aBgColor.Truncate();
  result = GetBodyElement(&body);
  if (NS_OK == result) {
    result = body->GetBgColor(aBgColor);
    NS_RELEASE(body);
  }
  else if (nsnull != mAttrStyleSheet) {
    nscolor color;
    result = mAttrStyleSheet->GetDocumentBackgroundColor(color);
    if (NS_OK == result) {
      nsHTMLValue value(color);
      nsGenericHTMLElement::ColorToString(value, aBgColor);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::SetBgColor(const nsAReadableString& aBgColor)
{
  nsresult result = NS_OK;
  nsIDOMHTMLBodyElement* body;

  result = GetBodyElement(&body);
  if (NS_OK == result) {
    result = body->SetBgColor(aBgColor);
    NS_RELEASE(body);
  }
  else if (nsnull != mAttrStyleSheet) {
    nsHTMLValue value;
    if (nsGenericHTMLElement::ParseColor(aBgColor, this, value)) {
      mAttrStyleSheet->SetDocumentBackgroundColor(value.GetColorValue());
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::GetFgColor(nsAWritableString& aFgColor)
{
  nsresult result = NS_OK;
  nsIDOMHTMLBodyElement* body;

  aFgColor.Truncate();
  result = GetBodyElement(&body);
  if (NS_OK == result) {
    result = body->GetText(aFgColor);
    NS_RELEASE(body);
  }
  else if (nsnull != mAttrStyleSheet) {
    nscolor color;
    result = mAttrStyleSheet->GetDocumentForegroundColor(color);
    if (NS_OK == result) {
      nsHTMLValue value(color);
      nsGenericHTMLElement::ColorToString(value, aFgColor);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::SetFgColor(const nsAReadableString& aFgColor)
{
  nsresult result = NS_OK;
  nsIDOMHTMLBodyElement* body;

  result = GetBodyElement(&body);
  if (NS_OK == result) {
    result = body->SetText(aFgColor);
    NS_RELEASE(body);
  }
  else if (nsnull != mAttrStyleSheet) {
    nsHTMLValue value;
  
    if (nsGenericHTMLElement::ParseColor(aFgColor, this, value)) {
      mAttrStyleSheet->SetDocumentForegroundColor(value.GetColorValue());
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::GetLastModified(nsAWritableString& aLastModified)
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
nsHTMLDocument::GetSelection(nsAWritableString& aReturn)
{
  aReturn.Truncate();

  nsCOMPtr<nsIConsoleService> consoleService
    (do_GetService("@mozilla.org/consoleservice;1"));
 
  if (consoleService) {
    consoleService->LogStringMessage(NS_LITERAL_STRING("Deprecated method document.getSelection() called.  Please use window.getSelection() instead.").get());
  }

  nsIPresShell* shell = (nsIPresShell*)mPresShells.ElementAt(0);

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

static PRBool PR_CALLBACK
NameHashCleanupEnumeratorCallback(nsHashKey *aKey, void *aData, void* closure)
{
  nsBaseContentList *list = (nsBaseContentList *)aData;

  // The document this hash is in is most likely going away so we
  // reset the live nodelists to avoid leaving dangling pointers to
  // non-existing content
  list->Reset();

  NS_RELEASE(list);

  return PR_TRUE;
}

void
nsHTMLDocument::InvalidateHashTables()
{
  mNameHashTable.Reset(NameHashCleanupEnumeratorCallback);
  mIdHashTable.Reset();
}

static nsresult
AddEmptyListToHash(const nsAReadableString& aName, nsHashtable& aHash)
{
  nsBaseContentList *list = new nsBaseContentList();
  if (!list) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(list);

  nsStringKey key(aName);

  aHash.Put(&key, list);

  return NS_OK;
}

// Pre-fill the name hash with names that are likely to be resolved in
// this document to avoid walking the tree looking for elements with
// these names.

nsresult
nsHTMLDocument::PrePopulateHashTables()
{
  nsresult rv = NS_OK;

  rv = AddEmptyListToHash(NS_LITERAL_STRING("write"), mNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddEmptyListToHash(NS_LITERAL_STRING("writeln"), mNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddEmptyListToHash(NS_LITERAL_STRING("open"), mNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddEmptyListToHash(NS_LITERAL_STRING("close"), mNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddEmptyListToHash(NS_LITERAL_STRING("forms"), mNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddEmptyListToHash(NS_LITERAL_STRING("elements"), mNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddEmptyListToHash(NS_LITERAL_STRING("characterSet"), mNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddEmptyListToHash(NS_LITERAL_STRING("nodeType"), mNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddEmptyListToHash(NS_LITERAL_STRING("parentNode"), mNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddEmptyListToHash(NS_LITERAL_STRING("cookie"), mNameHashTable);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

static PRBool
IsNamedItem(nsIContent* aContent, nsIAtom *aTag, nsAWritableString& aName)
{
  // Only the content types reflected in Level 0 with a NAME
  // attribute are registered. Images, layers and forms always get 
  // reflected up to the document. Applets and embeds only go
  // to the closest container (which could be a form).
  if ((aTag == nsHTMLAtoms::img) || (aTag == nsHTMLAtoms::form) ||
      (aTag == nsHTMLAtoms::applet) || (aTag == nsHTMLAtoms::embed) ||
      (aTag == nsHTMLAtoms::object)) {
    aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::name, aName);

    if (!aName.IsEmpty()) {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

nsresult
nsHTMLDocument::AddToNameTable(const nsAReadableString& aName,
                               nsIContent *aContent)
{
  nsStringKey key(aName);

  nsBaseContentList* list = NS_STATIC_CAST(nsBaseContentList *,
                                           mNameHashTable.Get(&key));

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
nsHTMLDocument::AddToIdTable(const nsAReadableString& aId,
                             nsIContent *aContent, PRBool aPutInTable)
{
  nsStringKey key(aId);

  nsIContent *e = NS_STATIC_CAST(nsIContent *, mIdHashTable.Get(&key));

  if (e == ELEMENT_NOT_IN_TABLE || (!e && aPutInTable)) {
    mIdHashTable.Put(&key, aContent);
  }

  return NS_OK;
}

nsresult
nsHTMLDocument::RemoveFromNameTable(const nsAReadableString& aName,
                                    nsIContent *aContent)
{
  nsStringKey key(aName);

  nsBaseContentList* list = NS_STATIC_CAST(nsBaseContentList *,
                                           mNameHashTable.Get(&key));

  if (list) {
    list->RemoveElement(aContent);
  }

  return NS_OK;
}

nsresult
nsHTMLDocument::RemoveFromIdTable(nsIContent *aContent)
{
  nsAutoString value;

  aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, value);

  if (value.IsEmpty()) {
    return NS_OK;
  }

  nsStringKey key(value);

  nsIContent *e = NS_STATIC_CAST(nsIContent *, mIdHashTable.Get(&key));

  if (e != aContent)
    return NS_OK;

  mIdHashTable.Remove(&key);

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
    AddToNameTable(value, aContent);
  }

  aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, value);

  if (!value.IsEmpty()) {
    nsresult rv = AddToIdTable(value, aContent, PR_FALSE);

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

void
nsHTMLDocument::FindNamedItems(const nsAReadableString& aName,
                               nsIContent *aContent, nsBaseContentList& aList)
{
  nsCOMPtr<nsIAtom> tag;
  nsAutoString value;

  aContent->GetTag(*getter_AddRefs(tag));

  if (tag == nsLayoutAtoms::textTagName) {
    // Text nodes are not named items nor can they have children.

    return;
  }

  if (IsNamedItem(aContent, tag, value) && value.Equals(aName)) {
    aList.AppendElement(aContent);
  } else {
    aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, value);

    if (value.Equals(aName)) {
      AddToIdTable(value, aContent, PR_TRUE);
    }
  }

  PRInt32 i, count;

  aContent->ChildCount(count);

  nsCOMPtr<nsIContent> child;

  for (i = 0; i < count; i++) {
    aContent->ChildAt(i, *getter_AddRefs(child));

    FindNamedItems(aName, child, aList);
  }
}

NS_IMETHODIMP
nsHTMLDocument::ResolveName(const nsAReadableString& aName,
                            nsIDOMHTMLFormElement *aForm,
                            nsISupports **aResult)
{
  *aResult = nsnull;

  // Bug 69826 - Make sure to flush the content model if the document
  // is still loading.

  // This is a perf killer while the document is loading!
  FlushPendingNotifications(PR_FALSE);

  nsStringKey key(aName);

  // We have built a table and cache the named items. The table will
  // be updated as content is added and removed.

  nsBaseContentList *list = NS_STATIC_CAST(nsContentList *,
                                           mNameHashTable.Get(&key));

  if (!list) {
#ifdef DEBUG_jst
    {
      printf ("nsHTMLDocument name cache miss for name '%s'\n",
              NS_ConvertUCS2toUTF8(aName).get());
    }
#endif

    list = new nsBaseContentList();
    NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(list);

    if(mRootContent) {
      FindNamedItems(aName, mRootContent, *list);
    }

    mNameHashTable.Put(&key, list);
  }

  PRUint32 length;

  list->GetLength(&length);

  if (length == 1) {
    // Onle one element in the list, return the list in stead of
    // returning the list

    nsCOMPtr<nsIDOMNode> node;

    list->Item(0, getter_AddRefs(node));

    if (aForm && node) {
      // document.forms["foo"].bar should not map to <form name="bar">
      // so we check here to see if we found a form and if we did we
      // ignore what we found in the document. This doesn't deal with
      // the case where more than one element in found in the document
      // (i.e. there are two named items in the document that have the
      // name we're looking for), that case is dealt with in
      // nsFormContentList

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
    // The list contains more than one element, return the whole list,
    // unless...

    if (aForm) {
      // ... we're called from a form, in that case we create a
      // nsFormContentList which will filter out the elements in the
      // list that don't belong to aForm

      nsFormContentList *fc_list = new nsFormContentList(aForm, *list);
      NS_ENSURE_TRUE(fc_list, NS_ERROR_OUT_OF_MEMORY);

      PRUint32 len;

      fc_list->GetLength(&len);

      if (len < 2) {
        // After t nsFormContentList is done filtering there's zero or
        // one element in the list, return that element, or null if
        // there's no element in the list.

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

  // No named items were found, look if we'll find the name by id.

  nsIContent *e = NS_STATIC_CAST(nsIContent *, mIdHashTable.Get(&key));

  if (e && e != ELEMENT_NOT_IN_TABLE) {
    nsCOMPtr<nsIAtom> tag;
    e->GetTag(*getter_AddRefs(tag));

    if (tag.get() == nsHTMLAtoms::embed ||
        tag.get() == nsHTMLAtoms::img ||
        tag.get() == nsHTMLAtoms::object ||
        tag.get() == nsHTMLAtoms::applet) {
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
  nsCOMPtr<nsIDOMElement> root;

  GetDocumentElement(getter_AddRefs(root));

  if (!root) {  
    return PR_FALSE;
  }

  nsAutoString bodyStr; bodyStr.AssignWithConversion("BODY");
  nsCOMPtr<nsIDOMNode> child;
  root->GetFirstChild(getter_AddRefs(child));

  while (child) {
    nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(child));

    if (domElement) {
      nsAutoString tagName;
      domElement->GetTagName(tagName);

      if (bodyStr.EqualsIgnoreCase(tagName)) {
        mBodyContent = child;
        NS_ADDREF(mBodyContent);
        return PR_TRUE;
      }
    }
    nsIDOMNode *tmpNode = child;
    tmpNode->GetNextSibling(getter_AddRefs(child));
  }

  return PR_FALSE;
}

NS_IMETHODIMP
nsHTMLDocument::GetBodyElement(nsIDOMHTMLBodyElement** aBody)
{
  if (mBodyContent == nsnull && PR_FALSE == GetBodyContent()) {
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

//----------------------------------------------------------------------
//
// DocWriteDummyRequest
//
//   This is a dummy request implementation that is used to make sure that
//   the onload event fires for document.writes that occur after the document
//   has finished loading.  Since such document.writes() blow away the old document
//   we need some way to generate document load notifications for the content that
//   is document.written.  The addition and removal of the dummy request generates
//   the appropriate load notifications which bubble up through a chain of observers
//   till the document viewer's LoadComplete() method which fires the onLoad event.
//

class DocWriteDummyRequest : public nsIChannel
{
protected:
  DocWriteDummyRequest();
  virtual ~DocWriteDummyRequest();

  static PRInt32 gRefCnt;

  nsCOMPtr<nsIURI> mURI;
  nsLoadFlags mLoadFlags;
  nsCOMPtr<nsILoadGroup> mLoadGroup;

public:
  static nsresult
  Create(nsIRequest** aResult);

  NS_DECL_ISUPPORTS

	// nsIRequest
  NS_IMETHOD GetName(PRUnichar* *result) { 
    *result = ToNewUnicode(NS_LITERAL_STRING("about:dummy-doc-write-request"));
    return NS_OK;
  }
  NS_IMETHOD IsPending(PRBool *_retval) { *_retval = PR_TRUE; return NS_OK; }
  NS_IMETHOD GetStatus(nsresult *status) { *status = NS_OK; return NS_OK; } 
  NS_IMETHOD Cancel(nsresult status);
  NS_IMETHOD Suspend(void) { return NS_OK; }
  NS_IMETHOD Resume(void)  { return NS_OK; }

 	// nsIChannel
  NS_IMETHOD GetOriginalURI(nsIURI* *aOriginalURI) { *aOriginalURI = mURI; NS_ADDREF(*aOriginalURI); return NS_OK; }
  NS_IMETHOD SetOriginalURI(nsIURI* aOriginalURI) { mURI = aOriginalURI; return NS_OK; }
  NS_IMETHOD GetURI(nsIURI* *aURI) { *aURI = mURI; NS_ADDREF(*aURI); return NS_OK; }
  NS_IMETHOD Open(nsIInputStream **_retval) { *_retval = nsnull; return NS_OK; }
  NS_IMETHOD AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt) { return NS_OK; }
  NS_IMETHOD GetLoadFlags(nsLoadFlags *aLoadFlags) { *aLoadFlags = mLoadFlags; return NS_OK; }
  NS_IMETHOD SetLoadFlags(nsLoadFlags aLoadFlags) { mLoadFlags = aLoadFlags; return NS_OK; }
  NS_IMETHOD GetOwner(nsISupports * *aOwner) { *aOwner = nsnull; return NS_OK; }
  NS_IMETHOD SetOwner(nsISupports * aOwner) { return NS_OK; }
  NS_IMETHOD GetLoadGroup(nsILoadGroup * *aLoadGroup) { *aLoadGroup = mLoadGroup; NS_IF_ADDREF(*aLoadGroup); return NS_OK; }
  NS_IMETHOD SetLoadGroup(nsILoadGroup * aLoadGroup) { mLoadGroup = aLoadGroup; return NS_OK; }
  NS_IMETHOD GetNotificationCallbacks(nsIInterfaceRequestor * *aNotificationCallbacks) { *aNotificationCallbacks = nsnull; return NS_OK; }
  NS_IMETHOD SetNotificationCallbacks(nsIInterfaceRequestor * aNotificationCallbacks) { return NS_OK; }
  NS_IMETHOD GetSecurityInfo(nsISupports * *aSecurityInfo) { *aSecurityInfo = nsnull; return NS_OK; } 
  NS_IMETHOD GetContentType(char * *aContentType) { *aContentType = nsnull; return NS_OK; } 
  NS_IMETHOD SetContentType(const char * aContentType) { return NS_OK; } 
  NS_IMETHOD GetContentLength(PRInt32 *aContentLength) { return NS_OK; }
  NS_IMETHOD SetContentLength(PRInt32 aContentLength) { return NS_OK; }

};

PRInt32 DocWriteDummyRequest::gRefCnt;

NS_IMPL_ADDREF(DocWriteDummyRequest);
NS_IMPL_RELEASE(DocWriteDummyRequest);
NS_IMPL_QUERY_INTERFACE2(DocWriteDummyRequest, nsIRequest, nsIChannel);

nsresult
DocWriteDummyRequest::Create(nsIRequest** aResult)
{
  DocWriteDummyRequest* request = new DocWriteDummyRequest();
  if (!request)
      return NS_ERROR_OUT_OF_MEMORY;

  return request->QueryInterface(NS_GET_IID(nsIRequest), (void**) aResult); 
}


DocWriteDummyRequest::DocWriteDummyRequest()
{
  NS_INIT_REFCNT();

  gRefCnt++;
  mLoadGroup = nsnull;
  mLoadFlags = 0;
  mURI = nsnull;
}


DocWriteDummyRequest::~DocWriteDummyRequest()
{
  gRefCnt--;
}

NS_IMETHODIMP
DocWriteDummyRequest::Cancel(nsresult status)
{
  // XXX To be implemented?
  return NS_OK;
}

// ----------------------------------------------------------------------------

nsresult
nsHTMLDocument::AddDocWriteDummyRequest(void)
{ 
  nsresult rv = NS_OK;

  rv = DocWriteDummyRequest::Create(getter_AddRefs(mDocWriteDummyRequest));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsILoadGroup> loadGroup;

  rv = GetDocumentLoadGroup(getter_AddRefs(loadGroup));
  if (NS_FAILED(rv)) return rv;

  if (loadGroup) {
    nsCOMPtr<nsIChannel> channel(do_QueryInterface(mDocWriteDummyRequest));
    rv = channel->SetLoadGroup(loadGroup);
    if (NS_FAILED(rv)) return rv;

    nsLoadFlags loadFlags = 0;
    channel->GetLoadFlags(&loadFlags);
    loadFlags |= nsIChannel::LOAD_DOCUMENT_URI;
    channel->SetLoadFlags(loadFlags);

    channel->SetOriginalURI(mDocumentURL);

    rv = loadGroup->AddRequest(mDocWriteDummyRequest, nsnull);
    if (NS_FAILED(rv)) return rv;
  }

  return rv;
}

nsresult
nsHTMLDocument::RemoveDocWriteDummyRequest(void)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsILoadGroup> loadGroup;
  rv = GetDocumentLoadGroup(getter_AddRefs(loadGroup));
  if (NS_FAILED(rv)) return rv;

  if (loadGroup && mDocWriteDummyRequest) {
    rv = loadGroup->RemoveRequest(mDocWriteDummyRequest, nsnull, NS_OK);
    if (NS_FAILED(rv)) return rv;

    mDocWriteDummyRequest = nsnull;
  }

  return rv;
}
