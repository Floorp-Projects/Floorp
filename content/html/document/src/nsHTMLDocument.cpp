/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   IBM Corp.
 */ 
#define NS_IMPL_IDS
#include "nsICharsetAlias.h"
#undef NS_IMPL_IDS

#include "nsCOMPtr.h"
#include "nsIFileChannel.h"
#include "nsXPIDLString.h"
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
#include "nsContentList.h"
#include "nsDOMError.h"
#include "nsICodebasePrincipal.h"
#include "nsIAggregatePrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsJSUtils.h"
#include "nsDOMPropEnums.h"
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
#include "nsIHTTPChannel.h"
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
#include "nsINetDataCacheManager.h"
#include "nsICachedNetData.h"
#include "nsIXMLContent.h" //for createelementNS
#include "nsHTMLParts.h" //for createelementNS
#include "nsLayoutCID.h"
#include "nsContentCID.h"

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
        NS_WITH_SERVICE(nsIPref, prefs, "@mozilla.org/preferences;1", &rv);
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
NS_LAYOUT nsresult
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

  mBodyContent = nsnull;
  mForms = nsnull;
  mIsWriting = 0;
  mWriteLevel = 0;
  
  if (gRefCntRDFService++ == 0)
  {
    nsresult rv;    
		rv = nsServiceManager::GetService(kRDFServiceCID,
						  NS_GET_IID(nsIRDFService),
						  (nsISupports**) &gRDF);

    //NS_WITH_SERVICE(nsIRDFService, gRDF, kRDFServiceCID, &rv);
  }

  mDomainWasSet = PR_FALSE; // Bug 13871: Frameset spoofing

  PrePopulateHashTables();
}

nsHTMLDocument::~nsHTMLDocument()
{
  PRInt32 i;

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
  for (i = 0; i < mImageMaps.Count(); i++) {
    nsIDOMHTMLMapElement* map = (nsIDOMHTMLMapElement*)mImageMaps.ElementAt(i);
    NS_RELEASE(map);
  }
  NS_IF_RELEASE(mForms);
  if (mCSSLoader) {
    mCSSLoader->DropDocumentReference();  // release weak ref
    NS_RELEASE(mCSSLoader);
  }

  NS_IF_RELEASE(mBodyContent);

  if (--gRefCntRDFService == 0)
  {     
     nsServiceManager::ReleaseService("@mozilla.org/rdf/rdf-service;1", gRDF);
  }

  InvalidateHashTables();
}

NS_IMETHODIMP
nsHTMLDocument::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_PRECONDITION(nsnull != aInstancePtr, "null ptr");
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsIHTMLDocument))) {
    NS_ADDREF_THIS();
    *aInstancePtr = (void**) (nsIHTMLDocument *)this;
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMHTMLDocument))) {
    NS_ADDREF_THIS();
    *aInstancePtr = (void**) (nsIDOMHTMLDocument *)this;
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMNSHTMLDocument))) {
    NS_ADDREF_THIS();
    *aInstancePtr = (void**) (nsIDOMNSHTMLDocument *)this;
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIHTMLContentContainer))) {
    NS_ADDREF_THIS();
    *aInstancePtr = (void**) (nsIHTMLContentContainer *)this;
    return NS_OK;
  }
  return nsDocument::QueryInterface(aIID, aInstancePtr);
}

nsrefcnt
nsHTMLDocument::AddRef()
{
  return nsDocument::AddRef();
}

nsrefcnt
nsHTMLDocument::Release()
{
  return nsDocument::Release();
}

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

  PRInt32 i;

  InvalidateHashTables();
  PrePopulateHashTables();

  NS_IF_RELEASE(mImages);
  NS_IF_RELEASE(mApplets);
  NS_IF_RELEASE(mEmbeds);
  NS_IF_RELEASE(mLinks);
  NS_IF_RELEASE(mAnchors);
  NS_IF_RELEASE(mLayers);

  for (i = 0; i < mImageMaps.Count(); i++) {
    nsIDOMHTMLMapElement* map = (nsIDOMHTMLMapElement*)mImageMaps.ElementAt(i);
    NS_RELEASE(map);
  }
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

  return result;
}

NS_IMETHODIMP 
nsHTMLDocument::GetContentType(nsAWritableString& aContentType) const
{
  aContentType.Assign(NS_LITERAL_STRING("text/html"));
  return NS_OK;
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

  nsCOMPtr<nsICachedNetData> cachedData;
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
  nsCOMPtr<nsIHTTPChannel> httpChannel = do_QueryInterface(aChannel);

  PRBool bTryCache = PR_FALSE;
  PRUint32 cacheFlags = 0;

  if (httpChannel) {
    nsXPIDLCString lastModHeader;
    nsIAtom* lastModKey = NS_NewAtom("last-modified");

    rv = httpChannel->GetResponseHeader(lastModKey, 
                                        getter_Copies(lastModHeader));

    NS_RELEASE(lastModKey);
    if (NS_SUCCEEDED(rv)) {
      lastModified.AssignWithConversion(NS_STATIC_CAST(const char*,
                                                       lastModHeader));
      SetLastModified(lastModified);
    }

    nsXPIDLCString referrerHeader;
    nsAutoString referrer;
    // The misspelled key 'referer' is as per the HTTP spec
    nsCOMPtr<nsIAtom> referrerKey(dont_AddRef(NS_NewAtom("referer")));

    rv = httpChannel->GetRequestHeader(referrerKey, 
                                       getter_Copies(referrerHeader));

    if (NS_SUCCEEDED(rv)) {
      referrer.AssignWithConversion(NS_STATIC_CAST(const char*,
                                                   referrerHeader));
      SetReferrer(referrer);
    }

    if(kCharsetFromHTTPHeader > charsetSource) {
      nsCOMPtr<nsIAtom>
        contentTypeKey(dont_AddRef(NS_NewAtom("content-type")));

      nsXPIDLCString contenttypeheader;
      rv = httpChannel->GetResponseHeader(contentTypeKey,
                                          getter_Copies(contenttypeheader));

      if (NS_SUCCEEDED(rv)) {
        nsAutoString contentType;
        contentType.AssignWithConversion(NS_STATIC_CAST(const char*,
                                                        contenttypeheader));

        PRInt32 start = contentType.RFind("charset=", PR_TRUE ) ;
        if(start != kNotFound) {
          start += 8; // 8 = "charset=".length
          PRInt32 end = 0;
          if(PRUnichar('"') == contentType.CharAt(start)) {
          	start++;
          	end = contentType.FindCharInSet("\"", start  );
	          if(kNotFound == end )
	            end = contentType.Length();
          } else {
          	end = contentType.FindCharInSet(";\n\r ", start  );
	          if(kNotFound == end )
	            end = contentType.Length();
          }
          nsAutoString theCharset;
          contentType.Mid(theCharset, start, end - start);
          nsCOMPtr<nsICharsetAlias> calias(do_CreateInstance(kCharsetAliasCID,
                                                             &rv));

          if(calias) {
            nsAutoString preferred;
            rv = calias->GetPreferred(theCharset, preferred);

            if(NS_SUCCEEDED(rv)) {
#ifdef DEBUG_charset
 							char* cCharset = charset.ToNewCString();
							printf("From HTTP Header, charset = %s\n", cCharset);
 							Recycle(cCharset);
 #endif
              charset = preferred;
              charsetSource = kCharsetFromHTTPHeader;
            }
          }
        }
      }
    }

    PRUint32 loadAttr = 0;
    rv = httpChannel->GetLoadAttributes(&loadAttr);
    NS_ASSERTION(NS_SUCCEEDED(rv),"cannot get load attribute");
    if(NS_SUCCEEDED(rv)) {
      // copy from nsHTTPChannel.cpp
      if(loadAttr & nsIChannel::CACHE_AS_FILE)
        cacheFlags = nsINetDataCacheManager::CACHE_AS_FILE;
      else if(loadAttr & nsIChannel::INHIBIT_PERSISTENT_CACHING)
        cacheFlags = nsINetDataCacheManager::BYPASS_PERSISTENT_CACHE;
      bTryCache = PR_TRUE;
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
    if (NS_SUCCEEDED(rv)) 
    { 
        // if we failed to get a last modification date, then we don't want to necessarily
        // fail to create a document for this file. Just don't set the last modified date on it...
        rv = file->GetLastModificationDate(&modDate);
        if (NS_SUCCEEDED(rv))
        {
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

  if(mParser) {
    nsCOMPtr<nsISupportsParserBundle> parserBundle;
    nsresult result;
    
    parserBundle = do_QueryInterface(mParser, &result);
    
    if(NS_SUCCEEDED(result)) {
      // We do this to help consumers who don't have access to the webshell.
      nsAutoString theDocShell,theChannel;
      theDocShell.AssignWithConversion("docshell");
      theChannel.AssignWithConversion("channel");
      parserBundle->SetDataIntoBundle(theDocShell,docShell);
      parserBundle->SetDataIntoBundle(theChannel,aChannel);
    }
  }

  nsCOMPtr<nsIDocumentCharsetInfo> dcInfo;
  docShell->GetDocumentCharsetInfo(getter_AddRefs(dcInfo));  

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
#ifdef DEBUG_charset
   		nsAutoString d(defaultCharsetFromWebShell);
 			char* cCharset = d.ToNewCString();
 			printf("From default charset, charset = %s\n", cCharset);
 			Recycle(cCharset);
 #endif
        charset = defaultCharsetFromWebShell;
        Recycle(defaultCharsetFromWebShell);
        charsetSource = kCharsetFromUserDefault;
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
			  char* cCharset = d.ToNewCString();
			  printf("From request charset, charset = %s req=%d->%d\n", 
			  		cCharset, charsetSource, requestCharsetSource);
			  Recycle(cCharset);
#endif
        charsetSource = requestCharsetSource;
        charset = requestCharset;
        Recycle(requestCharset);
      }
    }
	  if(kCharsetFromPreviousLoading > charsetSource)
    {
      PRUnichar* forceCharsetFromWebShell = NULL;
      if (muCV) {
		    rv = muCV->GetForceCharacterSet(&forceCharsetFromWebShell);
      }
		  if(NS_SUCCEEDED(rv) && (nsnull != forceCharsetFromWebShell)) 
      {
#ifdef DEBUG_charset
				nsAutoString d(forceCharsetFromWebShell);
			  char* cCharset = d.ToNewCString();
			  printf("From force, charset = %s \n", cCharset);
			  Recycle(cCharset);
#endif
				charset = forceCharsetFromWebShell;
        Recycle(forceCharsetFromWebShell);
				//TODO: we should define appropriate constant for force charset
				charsetSource = kCharsetFromPreviousLoading;  
          } else if (dcInfo) {
            nsCOMPtr<nsIAtom> csAtom;
            dcInfo->GetForcedCharset(getter_AddRefs(csAtom));
            if (csAtom.get() != NULL) {
              csAtom->ToString(charset);
              charsetSource = kCharsetFromPreviousLoading;  
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

    if(bTryCache && urlSpec)
    {
       nsCOMPtr<nsINetDataCacheManager> cacheMgr;
       cacheMgr = do_GetService(NS_NETWORK_CACHE_MANAGER_CONTRACTID, &rv);       
       if(NS_SUCCEEDED(rv))
       {
          rv = cacheMgr->GetCachedNetData(urlSpec, nsnull, 0, cacheFlags, getter_AddRefs(cachedData));
          if(NS_SUCCEEDED(rv)) {
              if(kCharsetFromCache > charsetSource) 
              {
                nsXPIDLCString cachedCharset;
                PRUint32 cachedCharsetLen = 0;
                rv = cachedData->GetAnnotation( "charset", &cachedCharsetLen, 
                      getter_Copies(cachedCharset));
                if(NS_SUCCEEDED(rv) && (cachedCharsetLen > 0)) 
                {
                   charset.AssignWithConversion(cachedCharset);
                   charsetSource = kCharsetFromCache;
                }
              }    
          }
       }
       rv=NS_OK;
    }

    if (kCharsetFromParentFrame > charsetSource) {
      if (dcInfo) {
        nsCOMPtr<nsIAtom> csAtom;
        dcInfo->GetParentCharset(getter_AddRefs(csAtom));
        if (csAtom) {
          csAtom->ToString(charset);
          charsetSource = kCharsetFromParentFrame;  

          // printf("### 0 >>> Having parent CS = %s\n", charset.ToNewCString());
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
                     mParser, charset.GetUnicode(),aCommand);
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
  }
#endif

  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = this->SetDocumentCharacterSet(charset);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if(cachedData) {
       rv=cachedData->SetAnnotation("charset",charset.Length()+1,
                                    NS_ConvertUCS2toUTF8(charset).get());
       NS_ASSERTION(NS_SUCCEEDED(rv),"cannot SetAnnotation");
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
	  char* cCharset = charset.ToNewCString();
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
  if (nsnull == mDocumentTitle) {
    mDocumentTitle = new nsString(aTitle);
  }
  else {
    mDocumentTitle->Assign(aTitle);
  }

  // Pass on to any interested containers
  PRInt32 i, n = mPresShells.Count();
  for (i = 0; i < n; i++) {
    nsIPresShell* shell = (nsIPresShell*) mPresShells.ElementAt(i);
    nsCOMPtr<nsIPresContext> cx;
    shell->GetPresContext(getter_AddRefs(cx));
    nsCOMPtr<nsISupports> container;
    if (NS_OK == cx->GetContainer(getter_AddRefs(container))) {
      if (container) {
        nsCOMPtr<nsIBaseWindow> docShell(do_QueryInterface(container));
        if(docShell) {
          docShell->SetTitle(mDocumentTitle->GetUnicode());
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::AddImageMap(nsIDOMHTMLMapElement* aMap)
{
  NS_PRECONDITION(nsnull != aMap, "null ptr");
  if (nsnull == aMap) {
    return NS_ERROR_NULL_POINTER;
  }
  if (mImageMaps.AppendElement(aMap)) {
    NS_ADDREF(aMap);
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
  if (mImageMaps.RemoveElement((void*)aMap)) {
    NS_RELEASE(aMap);
  }
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
  PRInt32 i, n = mImageMaps.Count();
  for (i = 0; i < n; i++) {
    nsIDOMHTMLMapElement* map = (nsIDOMHTMLMapElement*)mImageMaps.ElementAt(i);
    if (NS_OK == map->GetName(name)) {
      if (name.EqualsIgnoreCase(aMapName)) {
        *aResult = map;
        NS_ADDREF(map);
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
  if (nsnull != mBaseURL) {
    NS_ADDREF(mBaseURL);
    aURL = mBaseURL;
  }
  else {
    aURL = GetDocumentURL();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDocument::SetBaseURL(const nsAReadableString& aURLSpec)
{
  nsresult result = NS_OK;

  NS_IF_RELEASE(mBaseURL);
  if (0 < aURLSpec.Length()) {
    {
        result = NS_NewURI(&mBaseURL, aURLSpec, mDocumentURL);
    }
  }
  return result;
}

NS_IMETHODIMP
nsHTMLDocument::GetBaseTarget(nsAWritableString& aTarget) const
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
    result = NS_NewCSSLoader(this, &mCSSLoader);
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
nsHTMLDocument::SetHeaderData(nsIAtom* aHeaderField,
                              const nsAReadableString& aData)
{
  nsresult result = nsMarkupDocument::SetHeaderData(aHeaderField, aData);

  if (NS_SUCCEEDED(result)) {
    if (aHeaderField == nsHTMLAtoms::headerDefaultStyle) {
      // switch alternate style sheets based on default
      nsAutoString type;
      nsAutoString title;
      nsAutoString textHtml; textHtml.AssignWithConversion("text/html");
      PRInt32 index;
      PRInt32 count = mStyleSheets.Count();
      for (index = 0; index < count; index++) {
        nsIStyleSheet* sheet = (nsIStyleSheet*)mStyleSheets.ElementAt(index);
        sheet->GetType(type);
        if (PR_FALSE == type.Equals(textHtml)) {
          sheet->GetTitle(title);
          if (0 < title.Length()) {  // if sheet has title
            nsAutoString data(aData);
            PRBool disabled = ((0 == aData.Length()) || 
                               (PR_FALSE == title.EqualsIgnoreCase(data)));
            SetStyleSheetDisabledState(sheet, disabled);
          }
        }
      }
    }
  }
  return result;
}

NS_IMETHODIMP 
nsHTMLDocument::ContentAppended(nsIContent* aContainer,
                                PRInt32 aNewIndexInContainer)
{
  nsresult rv = RegisterNamedItems(aContainer);

  if (NS_FAILED(rv)) {
    return rv;
  }

  return nsDocument::ContentAppended(aContainer, aNewIndexInContainer);
}

NS_IMETHODIMP 
nsHTMLDocument::ContentInserted(nsIContent* aContainer, nsIContent* aContent,
                                PRInt32 aIndexInContainer)
{
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
                                 nsIAtom* aAttribute, PRInt32 aHint)
{
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

    aContent->GetAttribute(aNameSpaceID, nsHTMLAtoms::id, value);

    if (!value.IsEmpty()) {
      nsresult rv = AddToIdTable(value, aContent, PR_TRUE);

      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  return nsDocument::AttributeChanged(aContent, aNameSpaceID, aAttribute,
                                      aHint);
}

NS_IMETHODIMP 
nsHTMLDocument::FlushPendingNotifications(PRBool aFlushReflows)
{
  // Determine if it is safe to flush the sink
  // by determining if it safe to flush all the presshells.
  PRBool isSafeToFlush = PR_TRUE;
  PRInt32 i = 0, n = mPresShells.Count();
  while ((i < n) && (isSafeToFlush)) {
    nsIPresShell* shell = NS_STATIC_CAST(nsIPresShell*, mPresShells[i]);
    if (shell) {
      shell->IsSafeToFlush(isSafeToFlush);
    }
    i++;
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

    rv = NS_CreateHTMLElement(getter_AddRefs(htmlContent), nodeInfo);
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
  nsresult rv = NS_CreateHTMLElement(getter_AddRefs(content), nodeInfo);
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


//
// nsIDOMHTMLDocument interface implementation
//
// see http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-html.html#ID-1006298752
// for full specification.
//
NS_IMETHODIMP
nsHTMLDocument::GetTitle(nsAWritableString& aTitle)
{
  if (nsnull != mDocumentTitle) {
    aTitle.Assign(*mDocumentTitle);
  } else {
    aTitle.Truncate();
  }
  return NS_OK;
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
  NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager, 
                 NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
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
      (NS_CONTENT_ATTR_HAS_VALUE == aContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::href, attr))) {
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
      (NS_CONTENT_ATTR_HAS_VALUE == aContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::name, attr))) {
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
  NS_WITH_SERVICE(nsICookieService, service, kCookieServiceCID, &result);
  if ((NS_OK == result) && (nsnull != service) && (nsnull != mDocumentURL)) {
    result = service->GetCookieString(mDocumentURL, str);
  }
  aCookie.Assign(str);
  return result;
}

NS_IMETHODIMP    
nsHTMLDocument::SetCookie(const nsAReadableString& aCookie)
{
  nsresult result = NS_OK;
  NS_WITH_SERVICE(nsICookieService, service, kCookieServiceCID, &result);
  if ((NS_OK == result) && (nsnull != service) && (nsnull != mDocumentURL)) {
    result = service->SetCookieString(mDocumentURL, this, nsAutoString(aCookie));
  }
  return result;
}

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

  // XXX Question, why does this return NS_OK on failure?
  nsresult result = NS_OK;
      
  // We need to use the dynamically scoped global and assume that the 
  // current JSContext is a DOM context with a nsIScriptGlobalObject so
  // that we can get the url of the caller.
  // XXX This will fail on non-DOM contexts :(

  nsCOMPtr<nsIScriptGlobalObject> global;
  nsContentUtils::GetDynamicScriptGlobal(cx, getter_AddRefs(global));
  if (global) {
    nsCOMPtr<nsIDOMWindowInternal> window(do_QueryInterface(global, &result));

    if (window) {
      nsCOMPtr<nsIDOMDocument> document;
      
      result = window->GetDocument(getter_AddRefs(document));
      if (NS_SUCCEEDED(result)) {
        nsCOMPtr<nsIDocument> doc(do_QueryInterface(document, &result));
        if (doc) { 
          *sourceURL = doc->GetDocumentURL(); 
          result = sourceURL ? NS_OK : NS_ERROR_FAILURE; 
        } 
      }
    }
  }
  return result;
}


// XXX TBI: accepting arguments to the open method.
nsresult
nsHTMLDocument::OpenCommon(nsIURI* aSourceURL)
{
  // If we already have a parser we ignore the document.open call.
  if (mParser)
    return NS_OK;

  // Stop current loads targetted at the window this document is in.
  if (mScriptGlobalObject) {
    nsCOMPtr<nsIDocShell> docshell;
    mScriptGlobalObject->GetDocShell(getter_AddRefs(docshell));

    if (docshell) {
      docshell->StopLoad();
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

    result = mScriptGlobalObject->SetNewDocument(kungFuDeathGrip);

    if (NS_FAILED(result))
      return result;
  }

  result = Reset(channel, group);
  if (NS_FAILED(result))
    return result;

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

  return result;
}

NS_IMETHODIMP    
nsHTMLDocument::Open()
{
  nsresult result = NS_OK;
  nsIURI* sourceURL;

  // XXX For the non-script Open case, we have to make
  // up a URL.
  result = NS_NewURI(&sourceURL, "about:blank");

  if (NS_SUCCEEDED(result)) {
    result = OpenCommon(sourceURL);
    NS_RELEASE(sourceURL);
  }

  return result;
}

NS_IMETHODIMP    
nsHTMLDocument::Open(JSContext *cx, jsval *argv, PRUint32 argc,
                     nsIDOMDocument** aReturn)
{
  nsresult result = NS_OK;
  nsIURI* sourceURL;

  // XXX The URL of the newly created document will match
  // that of the source document. Is this right?
  result = GetSourceDocumentURL(cx, &sourceURL);
  // Recover if we had a problem obtaining the source URL
  if (nsnull == sourceURL) {
      result = NS_NewURI(&sourceURL, "about:blank");
  }

  if (NS_SUCCEEDED(result)) {
    result = OpenCommon(sourceURL);
    NS_RELEASE(sourceURL);
  }

  QueryInterface(NS_GET_IID(nsIDOMDocument), (void **)aReturn);

  return result;
}

#define NS_GENERATE_PARSER_KEY() (void*)((mIsWriting << 31) | (mWriteLevel & 0x7fffffff))

NS_IMETHODIMP    
nsHTMLDocument::Clear(JSContext* cx, jsval* argv, PRUint32 argc)
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
  }

  return NS_OK;
}

nsresult
nsHTMLDocument::WriteCommon(const nsAReadableString& aText,
                            PRBool aNewlineTerminate)
{
  nsresult result = NS_OK;

  if (nsnull == mParser) {
    result = Open();
    if (NS_FAILED(result)) {
      return result;
    }
  }
  
  nsAutoString str(aText);

  if (aNewlineTerminate) {
    str.AppendWithConversion('\n');
  }

  mWriteLevel++;
  result = mParser->Parse(str, NS_GENERATE_PARSER_KEY(), 
                          NS_ConvertASCIItoUCS2("text/html"), PR_FALSE, 
                          (!mIsWriting || (mWriteLevel > 1)));
  mWriteLevel--;
  
  return result;
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
nsHTMLDocument::ScriptWriteCommon(JSContext *cx, 
                                  jsval *argv, 
                                  PRUint32 argc,
                                  PRBool aNewlineTerminate)
{
  nsresult result = NS_OK;

  nsXPIDLCString spec;
  if (!mDocumentURL ||
      (NS_SUCCEEDED(mDocumentURL->GetSpec(getter_Copies(spec))) &&
       nsCRT::strcasecmp(spec, "about:blank") == 0)) 
  {
    // The current document's URL and principal are empty or "about:blank".
    // By writing to this document, the script acquires responsibility for the
    // document for security purposes. Thus a document.write of a script tag
    // ends up producing a script with the same principals as the script
    // that performed the write.
    nsIScriptContext *context = (nsIScriptContext*)JS_GetContextPrivate(cx);
    JSObject* obj;
    if (NS_FAILED(GetScriptObject(context, (void**)&obj))) 
      return NS_ERROR_FAILURE;
    nsIScriptSecurityManager *sm = nsJSUtils::nsGetSecurityManager(cx, nsnull);
    if (!sm)
      return NS_ERROR_FAILURE;
    nsCOMPtr<nsIPrincipal> subject;
    if (NS_FAILED(sm->GetSubjectPrincipal(getter_AddRefs(subject))))
      return NS_ERROR_FAILURE;
    if (subject) {
      nsCOMPtr<nsICodebasePrincipal> codebase = do_QueryInterface(subject);
      if (codebase) {
        nsCOMPtr<nsIURI> subjectURI;
        if (NS_FAILED(codebase->GetURI(getter_AddRefs(subjectURI))))
          return NS_ERROR_FAILURE;

        NS_IF_RELEASE(mDocumentURL);
        mDocumentURL = subjectURI;
        NS_ADDREF(mDocumentURL);

        NS_IF_RELEASE(mPrincipal);
        mPrincipal = subject;
        NS_ADDREF(mPrincipal);
      }
    }
  }

  if (!mParser) {
    nsCOMPtr<nsIDOMDocument> doc;

    result = Open(cx, argv, argc, getter_AddRefs(doc));
    if (NS_FAILED(result)) {
      return result;
    }
  }

  if (argc > 0) {
    PRUint32 index;
    nsAutoString str;

    for (index = 0; index < argc; index++) {
      JSString *jsstring = JS_ValueToString(cx, argv[index]);
      
      if (jsstring) {
        str.Append(NS_REINTERPRET_CAST(const PRUnichar*,
                                       JS_GetStringChars(jsstring)),
                   JS_GetStringLength(jsstring));
      }
    }

    if (aNewlineTerminate) {
      str.AppendWithConversion('\n');
    }

    mWriteLevel++;
    result = mParser->Parse(str, NS_GENERATE_PARSER_KEY(), 
                            NS_ConvertASCIItoUCS2("text/html"), PR_FALSE, 
                            (!mIsWriting || (mWriteLevel > 1)));
    mWriteLevel--;
  }
  
  return result;
}

NS_IMETHODIMP    
nsHTMLDocument::Write(JSContext *cx, jsval *argv, PRUint32 argc)
{
  return ScriptWriteCommon(cx, argv, argc, PR_FALSE);
}

NS_IMETHODIMP    
nsHTMLDocument::Writeln(JSContext *cx, jsval *argv, PRUint32 argc)
{
  return ScriptWriteCommon(cx, argv, argc, PR_TRUE);
}

nsIContent *
nsHTMLDocument::MatchId(nsIContent *aContent, const nsAReadableString& aId)
{
  nsAutoString value;

  nsresult rv = aContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::id,
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

  nsresult rv = aContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::name,
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
  shell = getter_AddRefs(GetShellAt(0));
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
  shell = getter_AddRefs(GetShellAt(0));
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
    aContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::name, aName);

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

  aContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::id, value);

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

  aContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::id, value);

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
    aContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::id, value);

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

    FindNamedItems(aName, mRootContent, *list);

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

  if (e) {
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

NS_IMETHODIMP    
nsHTMLDocument::NamedItem(JSContext* cx, jsval* argv, PRUint32 argc, 
                          jsval* aReturn)
{
  nsresult rv = NS_OK;

  if (argc < 1) 
    return NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR;

  JSString *jsstr = ::JS_ValueToString(cx, argv[0]);
  PRUnichar *ustr = NS_REINTERPRET_CAST(PRUnichar *, JS_GetStringChars(jsstr));

  nsCOMPtr<nsISupports> item;

  rv = ResolveName(nsLiteralString(ustr, ::JS_GetStringLength(jsstr)),
                   nsnull, getter_AddRefs(item));

  nsCOMPtr<nsIScriptObjectOwner> owner(do_QueryInterface(item));

  nsIScriptContext *context = (nsIScriptContext*)::JS_GetContextPrivate(cx);
  JSObject *scriptObject;
  rv = GetScriptObject(context, (void **)&scriptObject);
  if (NS_FAILED(rv))
    return rv;

  if (owner) {
    nsIScriptSecurityManager *sm =
      nsJSUtils::nsGetSecurityManager(cx, scriptObject);

    rv = sm->CheckScriptAccess(cx, scriptObject, 
                               NS_DOM_PROP_NSHTMLFORMELEMENT_NAMEDITEM,
                               PR_FALSE);
    if (NS_SUCCEEDED(rv)) {
      JSObject* obj;

      rv = owner->GetScriptObject(context, (void**)&obj);
      if (NS_FAILED(rv)) {
        return rv;
      }

      *aReturn = OBJECT_TO_JSVAL(obj);
    }

    return rv;
  }

  nsCOMPtr<nsISupports> supports;
  rv = this->QueryInterface(NS_GET_IID(nsISupports), getter_AddRefs(supports));

  if (NS_SUCCEEDED(rv)) {
    rv = nsJSUtils::nsCallJSScriptObjectGetProperty(supports, cx, scriptObject,
                                                    argv[0], aReturn);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLDocument::GetScriptObject(nsIScriptContext *aContext,
                                void** aScriptObject)
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIScriptGlobalObject> global;

  if (nsnull == mScriptObject) {
    // XXX We make the (possibly erroneous) assumption that the first
    // presentation shell represents the "primary view" of the document
    // and that the JS parent chain should incorporate just that view.
    // This is done for lack of a better model when we have multiple
    // views.
    nsIPresShell* shell = (nsIPresShell*) mPresShells.ElementAt(0);
    if (shell) {
      nsCOMPtr<nsIPresContext> cx;
      shell->GetPresContext(getter_AddRefs(cx));
      nsCOMPtr<nsISupports> container;
      
      res = cx->GetContainer(getter_AddRefs(container));
      if (NS_SUCCEEDED(res) && container) {
        global = do_GetInterface(container);
      }
    }
    // XXX If we can't find a view, parent to the calling context's
    // global object. This may not be right either, but we need
    // something.
    else {
      global = getter_AddRefs(aContext->GetGlobalObject());
    }
    
    if (NS_SUCCEEDED(res)) {
      res = NS_NewScriptHTMLDocument(aContext, 
                                     (nsISupports *)(nsIDOMHTMLDocument *)this,
                                     (nsISupports *)global, 
                                     (void**)&mScriptObject);
    }
  }
  
  *aScriptObject = mScriptObject;

  return res;
}

PRBool    
nsHTMLDocument::Resolve(JSContext *aContext, JSObject *aObj, jsval aID,
                        PRBool *aDidDefineProperty)
{
  *aDidDefineProperty = PR_FALSE;

  if (!JSVAL_IS_STRING(aID)) {
    return PR_TRUE;
  }

  nsresult result;
  PRBool ret = PR_TRUE;
  jsval val = 0;

  result = NamedItem(aContext, &aID, 1, &val);
  if (NS_SUCCEEDED(result) && val) {
    JSString *str = JSVAL_TO_STRING(aID);
    ret = ::JS_DefineUCProperty(aContext, aObj,JS_GetStringChars(str),
                                JS_GetStringLength(str), val, nsnull,
                                nsnull, 0);

    *aDidDefineProperty = PR_TRUE;
  }
  if (NS_FAILED(result)) {
    ret = PR_FALSE;
  }

  return ret;
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

nsresult
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
nsHTMLDocument::AddForm(nsIDOMHTMLFormElement *aForm)
{
#if 0
  // Not necessary anymore since forms are real content now
  NS_PRECONDITION(nsnull != aForm, "null ptr");
  if (nsnull == aForm) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIContent* iContent = nsnull;
  nsresult result = aForm->QueryInterface(NS_GET_IID(nsIContent), (void**)&iContent);
  if ((NS_OK == result) && iContent) {
    nsIDOMHTMLCollection* forms = nsnull;
    
    // Initialize mForms if necessary...
    if (nsnull == mForms) {
      mForms = new nsContentList(this, nsHTMLAtoms::form, kNameSpaceID_Unknown);
      NS_ADDREF(mForms);
    }

    mForms->Add(iContent);
    NS_RELEASE(iContent);
  }
  return result;
#endif
  return NS_OK;
}

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

