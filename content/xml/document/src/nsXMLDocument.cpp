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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */


#include "nsXMLDocument.h"
#include "nsParserCIID.h"
#include "nsIParser.h"
#include "nsIXMLContent.h"
#include "nsIXMLContentSink.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h" 
#include "nsIContent.h"
#include "nsIContentViewerContainer.h"
#include "nsIContentViewer.h"
#include "nsIWebShell.h"
#include "nsIDocShell.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIDocumentLoader.h"
#include "nsIHTMLContent.h"
#include "nsHTMLParts.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsIStyleSet.h"
#include "nsIComponentManager.h"
#include "nsIDOMComment.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"
#include "nsIBaseWindow.h"
#include "nsIDOMCDATASection.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsIDOMDocumentType.h"
#include "nsINameSpaceManager.h"
#include "nsICSSLoader.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIHttpChannel.h"
#include "nsIURI.h"
#include "nsIServiceManager.h"
#include "nsICharsetAlias.h"
#include "nsIPref.h"
#include "nsICharsetDetector.h"
#include "prmem.h"
#include "prtime.h"
#include "nsIWebShellServices.h"
#include "nsICharsetDetectionAdaptor.h"
#include "nsCharsetDetectionAdaptorCID.h"
#include "nsICharsetAlias.h"
#include "nsIParserFilter.h"
#include "nsNetUtil.h"
#include "nsDOMError.h"
#include "nsScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsIAggregatePrincipal.h"
#include "nsLayoutCID.h"
#include "nsContentCID.h"
#include "nsDOMAttribute.h"
#include "nsGUIEvent.h"

#include "nsCExternalHandlerService.h"
#include "nsIMIMEService.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"

#include "nsContentUtils.h"

static NS_DEFINE_CID(kHTMLStyleSheetCID,NS_HTMLSTYLESHEET_CID);

// XXX The XML world depends on the html atoms
#include "nsHTMLAtoms.h"

#define DETECTOR_CONTRACTID_MAX 127
static char g_detector_contractid[DETECTOR_CONTRACTID_MAX + 1];
static PRBool gInitDetector = PR_FALSE;
static PRBool gPlugDetector = PR_FALSE;

static const char* kLoadAsData = "loadAsData";

// ==================================================================
// =
// ==================================================================


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

NS_LAYOUT nsresult
NS_NewDOMDocument(nsIDOMDocument** aInstancePtrResult,
                  const nsAReadableString& aNamespaceURI, 
                  const nsAReadableString& aQualifiedName, 
                  nsIDOMDocumentType* aDoctype,
                  nsIURI* aBaseURI)
{
  nsresult rv;

  *aInstancePtrResult = nsnull;

  nsXMLDocument* doc = new nsXMLDocument();
  if (doc == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIDOMDocument> kungFuDeathGrip(doc);

  rv = doc->Reset(nsnull, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  doc->SetDocumentURL(aBaseURI);
  doc->SetBaseURL(aBaseURI);

  if (aDoctype) {
    nsCOMPtr<nsIDOMNode> tmpNode;
    rv = doc->AppendChild(aDoctype, getter_AddRefs(tmpNode));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  if (aQualifiedName.Length() > 0) {
    nsCOMPtr<nsIDOMElement> root;
    rv = doc->CreateElementNS(aNamespaceURI, aQualifiedName,
                              getter_AddRefs(root));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> tmpNode;

    rv = doc->AppendChild(root, getter_AddRefs(tmpNode));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aInstancePtrResult = doc;
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}


NS_LAYOUT nsresult
NS_NewXMLDocument(nsIDocument** aInstancePtrResult)
{
  nsXMLDocument* doc = new nsXMLDocument();
  if (doc == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  return doc->QueryInterface(NS_GET_IID(nsIDocument), (void**) aInstancePtrResult);
}

nsXMLDocument::nsXMLDocument() 
  : mAttrStyleSheet(nsnull), mInlineStyleSheet(nsnull), 
    mParser(nsnull)
{
}

nsXMLDocument::~nsXMLDocument()
{
  NS_IF_RELEASE(mParser);  
  if (mAttrStyleSheet) {
    mAttrStyleSheet->SetOwningDocument(nsnull);
    NS_RELEASE(mAttrStyleSheet);
  }
  if (mInlineStyleSheet) {
    mInlineStyleSheet->SetOwningDocument(nsnull);
    NS_RELEASE(mInlineStyleSheet);
  }
  if (mCSSLoader) {
    mCSSLoader->DropDocumentReference();
  }
}


// QueryInterface implementation for nsHTMLAnchorElement
NS_INTERFACE_MAP_BEGIN(nsXMLDocument)
  NS_INTERFACE_MAP_ENTRY(nsIXMLDocument)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLContentContainer)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIHttpEventSink)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(Document)
NS_INTERFACE_MAP_END_INHERITING(nsDocument)


NS_IMPL_ADDREF_INHERITED(nsXMLDocument, nsDocument)
NS_IMPL_RELEASE_INHERITED(nsXMLDocument, nsDocument)


NS_IMETHODIMP 
nsXMLDocument::Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup)
{
  nsresult result = nsDocument::Reset(aChannel, aLoadGroup);
  if (NS_FAILED(result)) return result;
  nsCOMPtr<nsIURI> url;
  if (aChannel) {
    result = aChannel->GetURI(getter_AddRefs(url));
    if (NS_FAILED(result)) return result;
  }

  if (nsnull != mAttrStyleSheet) {
    mAttrStyleSheet->SetOwningDocument(nsnull);
    NS_RELEASE(mAttrStyleSheet);
  }
  if (nsnull != mInlineStyleSheet) {
    mInlineStyleSheet->SetOwningDocument(nsnull);
    NS_RELEASE(mInlineStyleSheet);
  }

  result = SetDefaultStylesheets(url);

  mBaseTarget.Truncate();

  return result;
}

NS_IMETHODIMP 
nsXMLDocument::GetContentType(nsAWritableString& aContentType) const
{
  // XXX Should get document type from incoming stream
  aContentType.Assign(NS_LITERAL_STRING("text/xml"));
  return NS_OK;
}

NS_IMETHODIMP
nsXMLDocument::GetInterface(const nsIID& aIID, void** aSink)
{
  // Since we implement all the interfaces that you can get with
  // GetInterface() we can simply call QueryInterface() here and let
  // it do all the work.

  return QueryInterface(aIID, aSink);
}


// nsIHttpEventSink
NS_IMETHODIMP
nsXMLDocument::OnRedirect(nsIHttpChannel *aHttpChannel, nsIChannel *aNewChannel)
{
  nsresult rv;

  nsCOMPtr<nsIScriptSecurityManager> securityManager = 
           do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);

  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIURI> newLocation;
  rv = aNewChannel->GetURI(getter_AddRefs(newLocation));

  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPrincipal> newCodebase;
  rv = securityManager->GetCodebasePrincipal(newLocation,
                                             getter_AddRefs(newCodebase));
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAggregatePrincipal> agg = do_QueryInterface(mPrincipal, &rv);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Principal not an aggregate.");

  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  rv = agg->SetCodebase(newCodebase);

  return rv;
}

NS_IMETHODIMP
nsXMLDocument::Load(const nsAReadableString& aUrl)
{
  nsCOMPtr<nsIChannel> channel;
  nsCOMPtr<nsIURI> uri;
  nsresult rv;
  
  // Create a new URI
  rv = NS_NewURI(getter_AddRefs(uri), aUrl, mDocumentURL);
  if (NS_FAILED(rv)) return rv;

  // Get security manager, check to see if we're allowed to load this URI
  nsCOMPtr<nsIScriptSecurityManager> secMan = 
           do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
  if (NS_FAILED(secMan->CheckConnect(nsnull, uri, "XMLDocument", "load")))
    return NS_ERROR_FAILURE;

  // Partial Reset
  SetDocumentURL(uri);
  SetBaseURL(uri);
  mBaseTarget.Truncate();

  // Create a channel
  rv = NS_OpenURI(getter_AddRefs(channel), uri, nsnull, nsnull, this);
  if (NS_FAILED(rv)) return rv;

  // Set a principal for this document
  NS_IF_RELEASE(mPrincipal);
  nsCOMPtr<nsISupports> channelOwner;
  rv = channel->GetOwner(getter_AddRefs(channelOwner));
  if (NS_SUCCEEDED(rv) && channelOwner)
      rv = channelOwner->QueryInterface(NS_GET_IID(nsIPrincipal), (void**)&mPrincipal);

  if (NS_FAILED(rv) || !channelOwner)
  {
    rv = secMan->GetCodebasePrincipal(uri, &mPrincipal);
    if (!mPrincipal) return rv;
  }

  // Prepare for loading the XML document "into oneself"
  nsCOMPtr<nsIStreamListener> listener;
  if (NS_FAILED(rv = StartDocumentLoad(kLoadAsData, channel, 
                                       nsnull, nsnull, 
                                       getter_AddRefs(listener),
                                       PR_FALSE))) {
    NS_ERROR("nsXMLDocument::Load: Failed to start the document load.");
    return rv;
  }

  // Start an asynchronous read of the XML document
  rv = channel->AsyncOpen(listener, nsnull);

  return rv;
}

NS_IMETHODIMP 
nsXMLDocument::StartDocumentLoad(const char* aCommand,
                                 nsIChannel* aChannel,
                                 nsILoadGroup* aLoadGroup,
                                 nsISupports* aContainer,
                                 nsIStreamListener **aDocListener,
                                 PRBool aReset)
{
  nsresult rv = nsDocument::StartDocumentLoad(aCommand,
                                              aChannel, aLoadGroup,
                                              aContainer, 
                                              aDocListener, aReset);
  if (NS_FAILED(rv)) return rv;

  nsAutoString charset; charset.AssignWithConversion("UTF-8");
  PRBool bIsHTML = PR_FALSE; 
  char* aContentType;
  nsCharsetSource charsetSource = kCharsetFromDocTypeDefault;

  nsCOMPtr<nsIURI> aUrl;
  rv = aChannel->GetURI(getter_AddRefs(aUrl));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIMIMEService> MIMEService (do_GetService(NS_MIMESERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) return rv;
  rv = MIMEService->GetTypeFromURI(aUrl, &aContentType);
    
  if (NS_SUCCEEDED(rv)) { 
    if ( 0 == PL_strcmp(aContentType, "text/html")) {
      bIsHTML = PR_TRUE;
    }
    Recycle(aContentType);
    aContentType = nsnull;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  if(httpChannel) {
    nsXPIDLCString contenttypeheader;
    rv = httpChannel->GetResponseHeader("content-type", getter_Copies(contenttypeheader));

    if (NS_SUCCEEDED(rv)) {
      nsAutoString contentType;
      contentType.AssignWithConversion( NS_STATIC_CAST(const char*, contenttypeheader) );
      PRInt32 start = contentType.RFind("charset=", PR_TRUE ) ;
      if(kNotFound != start)
      {
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
        nsICharsetAlias* calias = nsnull;
        rv = nsServiceManager::GetService(
                kCharsetAliasCID,
                NS_GET_IID(nsICharsetAlias),
                (nsISupports**) &calias);
        if(NS_SUCCEEDED(rv) && (nsnull != calias) )
        {
          nsAutoString preferred;
          rv = calias->GetPreferred(theCharset, preferred);
          if(NS_SUCCEEDED(rv))
          {
            charset = preferred;
            charsetSource = kCharsetFromHTTPHeader;
          }
          nsServiceManager::ReleaseService(kCharsetAliasCID, calias);
        }
      }
    }
  } //end of checking http channel

  static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

  rv = nsComponentManager::CreateInstance(kCParserCID, nsnull, 
                                          NS_GET_IID(nsIParser), 
                                          (void **)&mParser);
  if (NS_FAILED(rv))  return rv;

  nsIXMLContentSink* sink;
    
  nsCOMPtr<nsIDocShell> docShell;
  if(aContainer)
  {
    docShell = do_QueryInterface(aContainer, &rv);
    if(NS_FAILED(rv) || !(docShell))  return rv; 

    nsCOMPtr<nsISupportsParserBundle> parserBundle;
    nsresult result;
    parserBundle = do_QueryInterface(mParser, &result);
    if(NS_SUCCEEDED(result)) {
        // We do this to help consumers who don't have access to the webshell.
        nsAutoString theID;
        theID.AssignWithConversion("docshell");
        parserBundle->SetDataIntoBundle(theID,docShell);
    }

    nsCOMPtr<nsIContentViewer> cv;
    docShell->GetContentViewer(getter_AddRefs(cv));
    if (cv) {
      nsCOMPtr<nsIMarkupDocumentViewer> muCV = do_QueryInterface(cv);            
      if (muCV) {
        if(bIsHTML &&(0 == nsCRT::strcmp("view-source", aCommand))) { 
          // only do this for view-source
          //view source is now an XML document and we need to make provisions
          //for the usual charset use when displaying those documents, this 
          //code mirrors nsHTMLDocument.cpp
          PRUnichar* requestCharset = nsnull;
          nsIParserFilter *cdetflt = nsnull;
          nsCharsetSource requestCharsetSource = kCharsetUninitialized;

          //need to be able to override doc charset default on user request
          if( kCharsetFromDocTypeDefault == charsetSource ) // it is not from HTTP header
            charsetSource = kCharsetFromWeakDocTypeDefault;

          //check hint Charset (is this needed here?)
          PRUnichar* hintCharset = nsnull;
          nsCharsetSource  hintSource = kCharsetUninitialized;

          rv = muCV->GetHintCharacterSet(&hintCharset); 
          if(NS_SUCCEEDED(rv)) {
            rv = muCV->GetHintCharacterSetSource((PRInt32 *)(&hintSource));
            if(NS_SUCCEEDED(rv)) {
              if(hintSource > charsetSource) {
                charset = hintCharset;
                Recycle(hintCharset);
                charsetSource = hintSource;
              }
              if(kCharsetUninitialized != hintSource) {
                muCV->SetHintCharacterSetSource((PRInt32)(kCharsetUninitialized));
              }
            }
          }//hint Charset

          // get user default charset
          if(kCharsetFromUserDefault > charsetSource) 
          {
            PRUnichar* defaultCharsetFromDocShell = NULL;
            if (muCV) {
              rv = muCV->GetDefaultCharacterSet(&defaultCharsetFromDocShell);
              if(NS_SUCCEEDED(rv)) {
                charset = defaultCharsetFromDocShell;
                Recycle(defaultCharsetFromDocShell);
                charsetSource = kCharsetFromUserDefault;
              }
            }
          }//user default

          //user requested charset
          if(NS_SUCCEEDED(rv)) 
          {
            if(requestCharsetSource > charsetSource) 
            {
              charsetSource = requestCharsetSource;
              charset = requestCharset;
              Recycle(requestCharset);
            }
          }

          //charset from previous loading
          if(kCharsetFromUserForced > charsetSource)
          {
            PRUnichar* forceCharsetFromDocShell = NULL;
            if (muCV) {
              rv = muCV->GetForceCharacterSet(&forceCharsetFromDocShell);
            }
            if(NS_SUCCEEDED(rv) && (nsnull != forceCharsetFromDocShell)) 
            {
              charset = forceCharsetFromDocShell;
              Recycle(forceCharsetFromDocShell);
              //TODO: we should define appropriate constant for force charset
              charsetSource = kCharsetFromUserForced;  
            }
          } //previous loading

          //auto-detector charset (needed here?)
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
	      
          //auto-detector charset (needed here?)
          if((kCharsetFromAutoDetection > charsetSource )  && gPlugDetector)
          {
            // we could do charset detection
            nsICharsetDetector *cdet = nsnull;
            nsCOMPtr<nsIWebShellServices> wss;
            nsICharsetDetectionAdaptor *adp = nsnull;

            if(NS_SUCCEEDED( rv_detect = 
       	    nsComponentManager::CreateInstance(g_detector_contractid, nsnull,
				    NS_GET_IID(nsICharsetDetector), (void**)&cdet)))
            {
              if(NS_SUCCEEDED( rv_detect = 
                  nsComponentManager::CreateInstance(
                  NS_CHARSET_DETECTION_ADAPTOR_CONTRACTID, nsnull,
                  NS_GET_IID(nsIParserFilter), (void**)&cdetflt)))
              {
                if(cdetflt && 
                    NS_SUCCEEDED( rv_detect=
                    cdetflt->QueryInterface(
                    NS_GET_IID(nsICharsetDetectionAdaptor),(void**) &adp)))
                {
                  wss = do_QueryInterface(docShell, &rv_detect);

                  if( NS_SUCCEEDED(rv_detect))
                  {
                    rv_detect = adp->Init(wss, cdet, (nsIDocument*)this, 
                                     mParser, charset.get(),aCommand);													
                    nsIParserFilter *oldFilter = nsnull;
                    if(cdetflt)
                      oldFilter = mParser->SetParserFilter(cdetflt);
                    NS_IF_RELEASE(oldFilter);
                    NS_IF_RELEASE(cdetflt);
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

            NS_IF_RELEASE(cdet);
            NS_IF_RELEASE(adp);
            // NO NS_IF_RELEASE(cdetflt); here, do it after mParser->SetParserFilter
          } //end of autodetection

        } //charset selection for view source only
      } //got document viewer
    } //got content view

    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(docShell));
    rv = NS_NewXMLContentSink(&sink, this, aUrl, webShell);
  }
  else 
    rv = NS_NewXMLContentSink(&sink, this, aUrl, nsnull);
 
  if (NS_OK == rv) {      
    // Set the parser as the stream listener for the document loader...
    rv = mParser->QueryInterface(NS_GET_IID(nsIStreamListener), (void**)aDocListener);

    if (NS_OK == rv) {
      mParser->SetDocumentCharset(charset, charsetSource);
      mParser->SetCommand(aCommand);
      mParser->SetContentSink(sink);
      mParser->Parse(aUrl, nsnull, PR_FALSE, (void *)this);
    }
    NS_RELEASE(sink); 
  }

  return rv;
}

NS_IMETHODIMP 
nsXMLDocument::EndLoad()
{
  nsAutoString cmd;
  if (mParser) mParser->GetCommand(cmd);
  NS_IF_RELEASE(mParser);
  if (cmd.EqualsWithConversion(kLoadAsData)) {
    // Generate a document load event for the case when an XML document was loaded
    // as pure data without any presentation attached to it.
    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event;
    event.eventStructType = NS_EVENT;
    event.message = NS_PAGE_LOAD;
    HandleDOMEvent(nsnull, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
  }    
  return nsDocument::EndLoad();  
}

NS_IMETHODIMP 
nsXMLDocument::GetAttributeStyleSheet(nsIHTMLStyleSheet** aResult)
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
nsXMLDocument::GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mInlineStyleSheet;
  if (nsnull == mInlineStyleSheet) {
    return NS_ERROR_NOT_AVAILABLE;  // probably not the right error...
  }
  else {
    NS_ADDREF(mInlineStyleSheet);
  }
  return NS_OK;
}

void nsXMLDocument::InternalAddStyleSheet(nsIStyleSheet* aSheet)  // subclass hook for sheet ordering
{
  if (aSheet == mAttrStyleSheet) {  // always first
    mStyleSheets.InsertElementAt(aSheet, 0);
  }
  else if (aSheet == mInlineStyleSheet) {  // always last
    mStyleSheets.AppendElement(aSheet);
  }
  else {
    if (mInlineStyleSheet == mStyleSheets.ElementAt(mStyleSheets.Count() - 1)) {
      // keep attr sheet last
      mStyleSheets.InsertElementAt(aSheet, mStyleSheets.Count() - 1);
    }
    else {
      mStyleSheets.AppendElement(aSheet);
    }
  }
}

void
nsXMLDocument::InternalInsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex)
{
  mStyleSheets.InsertElementAt(aSheet, aIndex + 1); // offset one for the attr style sheet
}

// nsIDOMDocument interface
NS_IMETHODIMP    
nsXMLDocument::GetDoctype(nsIDOMDocumentType** aDocumentType)
{
  return nsDocument::GetDoctype(aDocumentType); 
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateCDATASection(const nsAReadableString& aData, nsIDOMCDATASection** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsReadingIterator<PRUnichar> begin;
  nsReadingIterator<PRUnichar> end;
  aData.BeginReading(begin);
  aData.EndReading(end);
  if (FindInReadable(NS_LITERAL_STRING("]]>"),begin,end))
    return NS_ERROR_DOM_INVALID_CHARACTER_ERR;

  nsIContent* content;
  nsresult rv = NS_NewXMLCDATASection(&content);

  if (NS_OK == rv) {
    rv = content->QueryInterface(NS_GET_IID(nsIDOMCDATASection), (void**)aReturn);
    (*aReturn)->AppendData(aData);
    NS_RELEASE(content);
  }

  return rv;
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateEntityReference(const nsAReadableString& aName, nsIDOMEntityReference** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  *aReturn = nsnull;
  return NS_OK;
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateProcessingInstruction(const nsAReadableString& aTarget, 
                                           const nsAReadableString& aData, 
                                           nsIDOMProcessingInstruction** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsIContent* content;
  nsresult rv = NS_NewXMLProcessingInstruction(&content, aTarget, aData);

  if (NS_OK != rv) {
    return rv;
  }

  rv = content->QueryInterface(NS_GET_IID(nsIDOMProcessingInstruction), (void**)aReturn);
  NS_RELEASE(content);
  
  return rv;
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateElement(const nsAReadableString& aTagName, 
                              nsIDOMElement** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;
  NS_ENSURE_TRUE(aTagName.Length(), NS_ERROR_DOM_INVALID_CHARACTER_ERR);

  nsIXMLContent* content;
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsresult rv;

  rv = mNodeInfoManager->GetNodeInfo(aTagName, nsnull, kNameSpaceID_None,
                                     *getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewXMLElement(&content, nodeInfo);
  NS_ENSURE_SUCCESS(rv, rv);
   
  rv = content->QueryInterface(NS_GET_IID(nsIDOMElement), (void**)aReturn);
  NS_RELEASE(content);
 
  return rv;
}

NS_IMETHODIMP    
nsXMLDocument::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsresult rv;
  nsCOMPtr<nsIDOMDocumentType> docType, newDocType;
  nsCOMPtr<nsIDOMDocument> newDoc;

  // Get the doctype prior to new document construction. There's no big 
  // advantage now to dealing with the doctype separately, but maybe one 
  // day we'll do something significant with the doctype on document creation.
  GetDoctype(getter_AddRefs(docType));
  if (docType) {
    nsCOMPtr<nsIDOMNode> newDocTypeNode;
    rv = docType->CloneNode(PR_TRUE, getter_AddRefs(newDocTypeNode));
    if (NS_FAILED(rv)) return rv;
    newDocType = do_QueryInterface(newDocTypeNode);
  }

  // Create an empty document
  nsAutoString emptyStr;
  emptyStr.Truncate();
  rv = NS_NewDOMDocument(getter_AddRefs(newDoc), emptyStr, emptyStr,
                         newDocType, mDocumentURL);
  if (NS_FAILED(rv)) return rv;

  if (aDeep) {
    // If there was a doctype, a new one has already been inserted into the
    // new document. We might have to add nodes before it.
    PRBool beforeDocType = (docType.get() != nsnull);
    nsCOMPtr<nsIDOMNodeList> childNodes;
    
    GetChildNodes(getter_AddRefs(childNodes));
    if (childNodes) {
      PRUint32 index, count;
      childNodes->GetLength(&count);
      for (index=0; index < count; index++) {
        nsCOMPtr<nsIDOMNode> child;
        childNodes->Item(index, getter_AddRefs(child));
        if (child && (child != docType)) {
          nsCOMPtr<nsIDOMNode> newChild;
          rv = child->CloneNode(aDeep, getter_AddRefs(newChild));
          if (NS_FAILED(rv)) return rv;
          
          nsCOMPtr<nsIDOMNode> dummyNode;
          if (beforeDocType) {
            rv = newDoc->InsertBefore(newChild, 
                                      docType, 
                                      getter_AddRefs(dummyNode));
            if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
          }
          else {
            rv = newDoc->AppendChild(newChild, 
                                     getter_AddRefs(dummyNode));
            if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
          }
        }
        else {
          beforeDocType = PR_FALSE;
        }
      }
    }
  }

  return newDoc->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)aReturn);
}
 
NS_IMETHODIMP
nsXMLDocument::ImportNode(nsIDOMNode* aImportedNode,
                          PRBool aDeep,
                          nsIDOMNode** aReturn)
{
  return nsDocument::ImportNode(aImportedNode, aDeep, aReturn);
}

NS_IMETHODIMP
nsXMLDocument::CreateAttributeNS(const nsAReadableString& aNamespaceURI,
                                 const nsAReadableString& aQualifiedName,
                                 nsIDOMAttr** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsresult rv = mNodeInfoManager->GetNodeInfo(aQualifiedName, aNamespaceURI,
                                              *getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString value;
  nsDOMAttribute* attribute;

  attribute = new nsDOMAttribute(nsnull, nodeInfo, value);
  NS_ENSURE_TRUE(attribute, NS_ERROR_OUT_OF_MEMORY); 

  return attribute->QueryInterface(NS_GET_IID(nsIDOMAttr), (void**)aReturn);
}

NS_IMETHODIMP
nsXMLDocument::CreateElementNS(const nsAReadableString& aNamespaceURI,
                               const nsAReadableString& aQualifiedName,
                               nsIDOMElement** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

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

    rv = NS_CreateHTMLElement(getter_AddRefs(htmlContent), nodeInfo, PR_TRUE);
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

static nsIContent *
MatchId(nsIContent *aContent, const nsAReadableString& aName)
{
  nsAutoString value;
  nsIContent *result = nsnull;
  PRInt32 ns;

  aContent->GetNameSpaceID(ns);
  if (kNameSpaceID_HTML == ns) {
    if ((NS_CONTENT_ATTR_HAS_VALUE == aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, value)) &&
        aName.Equals(value)) {
      return aContent;
    }
  }
  else {
    nsCOMPtr<nsIXMLContent> xmlContent = do_QueryInterface(aContent);    
    nsCOMPtr<nsIAtom> IDValue;
    if (xmlContent && NS_SUCCEEDED(xmlContent->GetID(*getter_AddRefs(IDValue))) && IDValue) {
      const PRUnichar* IDValStr = nsnull;
      IDValue->GetUnicode(&IDValStr);       
      if (aName.Equals(IDValStr)) {
        return aContent;
      }
    }
  }
  
  PRInt32 i, count;
  aContent->ChildCount(count);
  for (i = 0; i < count && result == nsnull; i++) {
    nsIContent *child;
    aContent->ChildAt(i, child);
    result = MatchId(child, aName);
    NS_RELEASE(child);
  }  

  return result;
}

NS_IMETHODIMP
nsXMLDocument::GetElementById(const nsAReadableString& aElementId,
                             nsIDOMElement** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  NS_WARN_IF_FALSE(!aElementId.IsEmpty(),"getElementById(\"\"), fix caller?");
  if (aElementId.IsEmpty())
    return NS_OK;

  // If we tried to load a document and something went wrong, we might not have
  // root content. This can happen when you do document.load() and the document
  // to load is not XML, for example.
  if (!mRootContent)
    return NS_OK;

  // XXX For now, we do a brute force search of the content tree.
  // We should come up with a more efficient solution.
  nsCOMPtr<nsIContent> content(do_QueryInterface(MatchId(mRootContent,aElementId)));

  nsresult rv = NS_OK;
  if (content) {
    rv = content->QueryInterface(NS_GET_IID(nsIDOMElement),(void**)aReturn);
  }

  return rv;
}

// nsIXMLDocument
NS_IMETHODIMP 
nsXMLDocument::SetDefaultStylesheets(nsIURI* aUrl)
{
  nsresult result = NS_OK;
  if (aUrl) {
    //result = NS_NewHTMLStyleSheet(&mAttrStyleSheet, aUrl, this);
    result = nsComponentManager::CreateInstance(kHTMLStyleSheetCID,nsnull,NS_GET_IID(nsIHTMLStyleSheet),(void**)&mAttrStyleSheet);
    if (NS_SUCCEEDED(result)) {
      result = mAttrStyleSheet->Init(aUrl,this);
      if (NS_FAILED(result)) {
        NS_RELEASE(mAttrStyleSheet);
      }
    }
    if (NS_OK == result) {
      AddStyleSheet(mAttrStyleSheet); // tell the world about our new style sheet
      
      result = NS_NewHTMLCSSStyleSheet(&mInlineStyleSheet, aUrl, this);
      if (NS_OK == result) {
        AddStyleSheet(mInlineStyleSheet); // tell the world about our new style sheet
      }
    }
  }

  return result;
}

NS_IMETHODIMP 
nsXMLDocument::SetTitle(const PRUnichar *aTitle)
{
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
          docShell->SetTitle(aTitle);
        }
      }
    }
  }

  // Fire a DOM event for the title change.
  nsCOMPtr<nsIDOMEvent> event;
  CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
  event->InitEvent(NS_LITERAL_STRING("DOMTitleChanged"), PR_TRUE, PR_TRUE);
  DispatchEvent(event);

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLDocument::SetBaseTarget(const nsAReadableString &aBaseTarget)
{
  mBaseTarget.Assign(aBaseTarget);
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLDocument::GetBaseTarget(nsAWritableString &aBaseTarget)
{
  aBaseTarget.Assign(mBaseTarget);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLDocument::GetCSSLoader(nsICSSLoader*& aLoader)
{
  nsresult result = NS_OK;
  if (! mCSSLoader) {
    result = NS_NewCSSLoader(this, getter_AddRefs(mCSSLoader));
    if (mCSSLoader) {
      mCSSLoader->SetCaseSensitive(PR_TRUE);
      mCSSLoader->SetQuirkMode(PR_FALSE); // No quirks in XML
    }
  }
  aLoader = mCSSLoader;
  NS_IF_ADDREF(aLoader);
  return result;
}

