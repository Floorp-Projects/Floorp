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
#include "nsWellFormedDTD.h"
#include "nsParserCIID.h"
#include "nsIParser.h"
#include "nsIXMLContent.h"
#include "nsIXMLContentSink.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h" 
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
#include "nsIDOMCDATASection.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsExpatDTD.h"
#include "nsINameSpaceManager.h"
#include "nsICSSLoader.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsXPIDLString.h"
#include "nsIHTTPChannel.h"
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


// XXX The XML world depends on the html atoms
#include "nsHTMLAtoms.h"
#ifdef INCLUDE_XUL
#include "nsXULAtoms.h"
#endif

static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIXMLDocumentIID, NS_IXMLDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMCommentIID, NS_IDOMCOMMENT_IID);
static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);
static NS_DEFINE_IID(kIHTMLContentContainerIID, NS_IHTMLCONTENTCONTAINER_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMNodeListIID, NS_IDOMNODELIST_IID);
static NS_DEFINE_IID(kIDOMProcessingInstructionIID, NS_IDOMPROCESSINGINSTRUCTION_IID);
static NS_DEFINE_IID(kIDOMCDATASectionIID, NS_IDOMCDATASECTION_IID);
static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);

#define DETECTOR_PROGID_MAX 127
static char g_detector_progid[DETECTOR_PROGID_MAX + 1];
static PRBool gInitDetector = PR_FALSE;
static PRBool gPlugDetector = PR_FALSE;
static NS_DEFINE_IID(kIParserFilterIID, NS_IPARSERFILTER_IID);


// ==================================================================
// =
// ==================================================================


static int
MyPrefChangedCallback(const char*aPrefName, void* instance_data)
{
        nsresult rv;
        NS_WITH_SERVICE(nsIPref, prefs, "component://netscape/preferences", &rv);
        char* detector_name = nsnull;
        if(NS_SUCCEEDED(rv) && NS_SUCCEEDED(
             rv = prefs->CopyCharPref("intl.charset.detector",
                                     &detector_name)))
        {
			if(nsCRT::strlen(detector_name) > 0) {
				PL_strncpy(g_detector_progid, NS_CHARSET_DETECTOR_PROGID_BASE,DETECTOR_PROGID_MAX);
				PL_strncat(g_detector_progid, detector_name,DETECTOR_PROGID_MAX);
				gPlugDetector = PR_TRUE;
			} else {
				g_detector_progid[0]=0;
				gPlugDetector = PR_FALSE;
			}
            PR_FREEIF(detector_name);
        }
	return 0;
}


NS_LAYOUT nsresult
NS_NewXMLDocument(nsIDocument** aInstancePtrResult)
{
  nsXMLDocument* doc = new nsXMLDocument();
  if (doc == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  return doc->QueryInterface(kIDocumentIID, (void**) aInstancePtrResult);
}

nsXMLDocument::nsXMLDocument()
{
  mParser = nsnull;
  mAttrStyleSheet = nsnull;
  mInlineStyleSheet = nsnull;
  mCSSLoader = nsnull;
  
#ifdef XSL
  mTransformMediator = nsnull;
#endif
}

nsXMLDocument::~nsXMLDocument()
{
  NS_IF_RELEASE(mParser);  
  if (nsnull != mAttrStyleSheet) {
    mAttrStyleSheet->SetOwningDocument(nsnull);
    NS_RELEASE(mAttrStyleSheet);
  }
  if (nsnull != mInlineStyleSheet) {
    mInlineStyleSheet->SetOwningDocument(nsnull);
    NS_RELEASE(mInlineStyleSheet);
  }
  if (mCSSLoader) {
    mCSSLoader->DropDocumentReference();
    NS_RELEASE(mCSSLoader);
  }
#ifdef XSL
  NS_IF_RELEASE(mTransformMediator);
#endif
}

NS_IMETHODIMP 
nsXMLDocument::QueryInterface(REFNSIID aIID,
                              void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIXMLDocumentIID)) {
    NS_ADDREF_THIS();
    *aInstancePtr = (void**) (nsIXMLDocument *)this;
    return NS_OK;
  }
  if (aIID.Equals(kIHTMLContentContainerIID)) {
    NS_ADDREF_THIS();
    *aInstancePtr = (void**) (nsIHTMLContentContainer *)this;
    return NS_OK;
  }
  return nsDocument::QueryInterface(aIID, aInstancePtr);
}

nsrefcnt nsXMLDocument::AddRef()
{
  return nsDocument::AddRef();
}

nsrefcnt nsXMLDocument::Release()
{
  return nsDocument::Release();
}

nsresult
nsXMLDocument::Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup)
{
  nsresult result = nsDocument::Reset(aChannel, aLoadGroup);
  nsCOMPtr<nsIURI> aURL;
  result = aChannel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(result)) return result;
  if (NS_FAILED(result)) {
    return result;
  }

  if (nsnull != mAttrStyleSheet) {
    mAttrStyleSheet->SetOwningDocument(nsnull);
    NS_RELEASE(mAttrStyleSheet);
  }
  if (nsnull != mInlineStyleSheet) {
    mInlineStyleSheet->SetOwningDocument(nsnull);
    NS_RELEASE(mInlineStyleSheet);
  }

  result = NS_NewHTMLStyleSheet(&mAttrStyleSheet, aURL, this);
  if (NS_OK == result) {
    AddStyleSheet(mAttrStyleSheet); // tell the world about our new style sheet
    
    result = NS_NewHTMLCSSStyleSheet(&mInlineStyleSheet, aURL, this);
    if (NS_OK == result) {
      AddStyleSheet(mInlineStyleSheet); // tell the world about our new style sheet
    }
  }

  return result;
}

NS_IMETHODIMP 
nsXMLDocument::GetContentType(nsString& aContentType) const
{
  // XXX Should get document type from incoming stream
  aContentType.Assign("text/xml");
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLDocument::StartDocumentLoad(const char* aCommand,
                               nsIChannel* aChannel,
                               nsILoadGroup* aLoadGroup,
                               nsISupports* aContainer,
                               nsIStreamListener **aDocListener)
{
  nsresult rv = nsDocument::StartDocumentLoad(aCommand,
                                              aChannel, aLoadGroup,
                                              aContainer, 
                                              aDocListener);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoString charset("UTF-8");
  PRBool bIsHTML = PR_FALSE; 
  char* aContentType;
  nsCharsetSource charsetSource = kCharsetFromDocTypeDefault;

  nsCOMPtr<nsIURI> aUrl;
  rv = aChannel->GetURI(getter_AddRefs(aUrl));
  if (NS_FAILED(rv)) return rv;

  rv = aChannel->GetContentType(&aContentType);
  
  if (NS_SUCCEEDED(rv)) { 
  	 if ( 0 == PL_strcmp(aContentType, "text/html")) {
		 bIsHTML = PR_TRUE;
	 }
  }

  nsCOMPtr<nsIHTTPChannel> httpChannel = do_QueryInterface(aChannel);
  if(httpChannel) {
     nsIAtom* contentTypeKey = NS_NewAtom("content-type");
     nsXPIDLCString contenttypeheader;
     rv = httpChannel->GetResponseHeader(contentTypeKey, getter_Copies(contenttypeheader));


     NS_RELEASE(contentTypeKey);
     if (NS_SUCCEEDED(rv)) {
	nsAutoString contentType;
	contentType.Assign(contenttypeheader);
	PRInt32 start = contentType.RFind("charset=", PR_TRUE ) ;
	if(kNotFound != start)
	{
		 start += 8; // 8 = "charset=".length
		 PRInt32 end = contentType.FindCharInSet(";\n\r ", start  );
		 if(kNotFound == end )
			 end = contentType.Length();
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
  }

  static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
  static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

  rv = nsComponentManager::CreateInstance(kCParserCID, 
                                    nsnull, 
                                    kCParserIID, 
                                    (void **)&mParser);
  if (NS_OK == rv) {
    nsIXMLContentSink* sink;
    
    nsCOMPtr<nsIDocShell> docShell;
    if(aContainer)
      docShell = do_QueryInterface(aContainer, &rv);
    else rv = NS_NewXMLContentSink(&sink, this, aUrl, nsnull);

    if(NS_SUCCEEDED(rv) && (docShell)) {
      nsCOMPtr<nsIContentViewer> cv;
      docShell->GetContentViewer(getter_AddRefs(cv));
      if (cv) {
        nsCOMPtr<nsIMarkupDocumentViewer> muCV = do_QueryInterface(cv);            
        if (muCV) {
          if(bIsHTML &&(0 == nsCRT::strcmp("view-source", aCommand))) { // only do this for view-source

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
					  }//hint Charset


					// get user default charset
	    			if(kCharsetFromUserDefault > charsetSource) 
					{
						PRUnichar* defaultCharsetFromDocShell = NULL;
						if (muCV) {
  							rv = muCV->GetDefaultCharacterSet(&defaultCharsetFromDocShell);
							if(NS_SUCCEEDED(rv)) {
#ifdef DEBUG_charset
   									nsAutoString d(defaultCharsetFromDocShell);
 									char* cCharset = d.ToNewCString();
 									printf("From default charset, charset = %s\n", cCharset);
 									Recycle(cCharset);
#endif
							charset = defaultCharsetFromDocShell;
							Recycle(defaultCharsetFromDocShell);
							charsetSource = kCharsetFromUserDefault;
						}
					}//user default

					//user requested charset
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

					//charset from previous loading
					if(kCharsetFromPreviousLoading > charsetSource)
					{
						PRUnichar* forceCharsetFromDocShell = NULL;
						if (muCV) {
						rv = muCV->GetForceCharacterSet(&forceCharsetFromDocShell);
						}
						if(NS_SUCCEEDED(rv) && (nsnull != forceCharsetFromDocShell)) 
						{
#ifdef DEBUG_charset
								nsAutoString d(forceCharsetFromDocShell);
								char* cCharset = d.ToNewCString();
								printf("From force, charset = %s \n", cCharset);
								Recycle(cCharset);
#endif
							charset = forceCharsetFromDocShell;
							Recycle(forceCharsetFromDocShell);
							//TODO: we should define appropriate constant for force charset
							charsetSource = kCharsetFromPreviousLoading;  
						}
					} //previous loading
    
					//auto-detector charset (needed here?)
					nsresult rv_detect = NS_OK;
					if(! gInitDetector)
					{
                  nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_PROGID));
						if(pref)
						{
							char* detector_name = nsnull;
							if(NS_SUCCEEDED(
								rv_detect = pref->CopyCharPref("intl.charset.detector",
										 &detector_name)))
									{
										PL_strncpy(g_detector_progid, NS_CHARSET_DETECTOR_PROGID_BASE,DETECTOR_PROGID_MAX);
										PL_strncat(g_detector_progid, detector_name,DETECTOR_PROGID_MAX);
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
							nsComponentManager::CreateInstance(g_detector_progid, nsnull,
									NS_GET_IID(nsICharsetDetector), (void**)&cdet)))
						{
							if(NS_SUCCEEDED( rv_detect = 
								nsComponentManager::CreateInstance(
									NS_CHARSET_DETECTION_ADAPTOR_PROGID, nsnull,
									kIParserFilterIID, (void**)&cdetflt)))
								{
									if(cdetflt && 
											NS_SUCCEEDED( rv_detect=
											cdetflt->QueryInterface(
											NS_GET_IID(nsICharsetDetectionAdaptor),(void**) &adp)))
												{
                                       wss = do_QueryInterface(docShell, 
                                          &rv_detect);
                                          
													if( NS_SUCCEEDED(rv_detect))
																{
																	rv_detect = adp->Init(wss, cdet, (nsIDocument*)this, 
																						   mParser, charset.GetUnicode(),aCommand);													
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
					} 
				  }

			} //charset selection for view source only
          }
        }
      }
      if(NS_SUCCEEDED(rv))
        {
        nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(docShell));
        rv = NS_NewXMLContentSink(&sink, this, aUrl, webShell);
        }
    }

    if (NS_OK == rv) {      
      // Set the parser as the stream listener for the document loader...
      static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
      rv = mParser->QueryInterface(kIStreamListenerIID, (void**)aDocListener);

      if (NS_OK == rv) {

        mParser->SetDocumentCharset(charset, charsetSource);
        mParser->SetCommand(aCommand);
        mParser->SetContentSink(sink);
        mParser->Parse(aUrl, nsnull, PR_FALSE, (void *)this);
      }
      NS_RELEASE(sink); 
    }
  } 

  return rv;
}

NS_IMETHODIMP 
nsXMLDocument::EndLoad()
{
  NS_IF_RELEASE(mParser);
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
  // XXX TBI
  *aDocumentType = nsnull;
  return NS_OK;
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateCDATASection(const nsString& aData, nsIDOMCDATASection** aReturn)
{
  nsIContent* content;
  nsresult rv = NS_NewXMLCDATASection(&content);

  if (NS_OK == rv) {
    rv = content->QueryInterface(kIDOMCDATASectionIID, (void**)aReturn);
    (*aReturn)->AppendData(aData);
    NS_RELEASE(content);
  }

  return rv;
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateEntityReference(const nsString& aName, nsIDOMEntityReference** aReturn)
{
  *aReturn = nsnull;
  return NS_OK;
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateProcessingInstruction(const nsString& aTarget, 
                                           const nsString& aData, 
                                           nsIDOMProcessingInstruction** aReturn)
{
  nsIContent* content;
  nsresult rv = NS_NewXMLProcessingInstruction(&content, aTarget, aData);

  if (NS_OK != rv) {
    return rv;
  }

  rv = content->QueryInterface(kIDOMProcessingInstructionIID, (void**)aReturn);
  NS_RELEASE(content);
  
  return rv;
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateElement(const nsString& aTagName, 
                              nsIDOMElement** aReturn)
{
  nsIXMLContent* content;
  nsIAtom* tag = NS_NewAtom(aTagName);
  nsresult rv = NS_NewXMLElement(&content, tag);
  NS_RELEASE(tag);
   
  if (NS_OK != rv) {
    return rv;
  }
  rv = content->QueryInterface(kIDOMElementIID, (void**)aReturn);
  NS_RELEASE(content);
 
  return rv;
}

NS_IMETHODIMP    
nsXMLDocument::CreateElementWithNameSpace(const nsString& aTagName, 
                                            const nsString& aNameSpace, 
                                            nsIDOMElement** aReturn)
{
  PRInt32 namespaceID = kNameSpaceID_None;
  nsresult rv = NS_OK;

  if ((0 < aNameSpace.Length() && (nsnull != mNameSpaceManager))) {
    mNameSpaceManager->GetNameSpaceID(aNameSpace, namespaceID);
  }

  nsIContent* content;
  if (namespaceID == kNameSpaceID_HTML) {
    nsIHTMLContent* htmlContent;
    
    rv = NS_CreateHTMLElement(&htmlContent, aTagName);
    content = (nsIContent*)htmlContent;
  }
  else {
    nsIXMLContent* xmlContent;
    nsIAtom* tag;

    tag = NS_NewAtom(aTagName);
    rv = NS_NewXMLElement(&xmlContent, tag);
    NS_RELEASE(tag);
    if (NS_OK == rv) {
      xmlContent->SetNameSpaceID(namespaceID);
    }
    content = (nsIXMLContent*)xmlContent;
  }

  if (NS_OK != rv) {
    return rv;
  }
  content->SetContentID(mNextContentID++);
  rv = content->QueryInterface(kIDOMElementIID, (void**)aReturn);
  
  return rv;
}
 
NS_IMETHODIMP
nsXMLDocument::ImportNode(nsIDOMNode* aImportedNode,
                          PRBool aDeep,
                          nsIDOMNode** aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXMLDocument::CreateElementNS(const nsString& aNamespaceURI,
                               const nsString& aQualifiedName,
                               nsIDOMElement** aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXMLDocument::CreateAttributeNS(const nsString& aNamespaceURI,
                                 const nsString& aQualifiedName,
                                 nsIDOMAttr** aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXMLDocument::GetElementById(const nsString& aElementId,
                              nsIDOMElement** aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXMLDocument::GetElementsByTagNameNS(const nsString& aNamespaceURI, 
                                      const nsString& aLocalName, 
                                      nsIDOMNodeList** aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}


// nsIXMLDocument interface
static nsIContent *
MatchName(nsIContent *aContent, const nsString& aName)
{
  nsAutoString value;
  nsIContent *result = nsnull;
  PRInt32 ns;

  aContent->GetNameSpaceID(ns);
  if (kNameSpaceID_HTML == ns) {
    if ((NS_CONTENT_ATTR_HAS_VALUE == aContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::id, value)) &&
        aName.Equals(value)) {
      return aContent;
    }
    else if ((NS_CONTENT_ATTR_HAS_VALUE == aContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::name, value)) &&
             aName.Equals(value)) {
      return aContent;
    }
  }
  
  PRInt32 i, count;
  aContent->ChildCount(count);
  for (i = 0; i < count && result == nsnull; i++) {
    nsIContent *child;
    aContent->ChildAt(i, child);
    result = MatchName(child, aName);
    NS_RELEASE(child);
  }  

  return result;
}

NS_IMETHODIMP 
nsXMLDocument::GetContentById(const nsString& aName, nsIContent** aContent)
{
  // XXX Since we don't have a validating parser, the only content
  // that this applies to is HTML content.
  nsIContent *content;

  content = MatchName(mRootContent, aName);

  NS_IF_ADDREF(content);
  *aContent = content;

  return NS_OK;
}

#ifdef XSL
NS_IMETHODIMP 
nsXMLDocument::SetTransformMediator(nsITransformMediator* aMediator)
{
  NS_ASSERTION(nsnull == mTransformMediator, "nsXMLDocument::SetTransformMediator(): \
    Cannot set a second transform mediator\n");
  mTransformMediator = aMediator;
  NS_IF_ADDREF(mTransformMediator);
  return NS_OK;
}
#endif

NS_IMETHODIMP
nsXMLDocument::GetCSSLoader(nsICSSLoader*& aLoader)
{
  nsresult result = NS_OK;
  if (! mCSSLoader) {
    result = NS_NewCSSLoader(this, &mCSSLoader);
    if (mCSSLoader) {
      mCSSLoader->SetCaseSensitive(PR_TRUE);
      mCSSLoader->SetQuirkMode(PR_FALSE); // No quirks in XML
    }
  }
  aLoader = mCSSLoader;
  NS_IF_ADDREF(aLoader);
  return result;
}

