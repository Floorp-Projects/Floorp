/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#include "nsCOMPtr.h"
#include "nsContentDLF.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIContentViewer.h"
#include "nsICategoryManager.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIDocument.h"
#include "nsIURL.h"
#include "nsNodeInfo.h"
#include "nsNodeInfoManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsString.h"
#include "nsContentCID.h"
#include "prprf.h"
#include "nsNetUtil.h"
#include "nsCRT.h"
#include "nsIViewSourceChannel.h"
#ifdef MOZ_MEDIA
#include "nsHTMLMediaElement.h"
#endif
#include "nsContentUtils.h"
#include "imgILoader.h"
#include "nsCharsetSource.h"
#include "nsMimeTypes.h"

#include "mozilla/FunctionTimer.h"

// plugins
#include "nsIPluginHost.h"
#include "nsPluginHost.h"
static NS_DEFINE_CID(kPluginDocumentCID, NS_PLUGINDOCUMENT_CID);

// Factory code for creating variations on html documents

#undef NOISY_REGISTRY

static NS_DEFINE_IID(kHTMLDocumentCID, NS_HTMLDOCUMENT_CID);
static NS_DEFINE_IID(kXMLDocumentCID, NS_XMLDOCUMENT_CID);
static NS_DEFINE_IID(kSVGDocumentCID, NS_SVGDOCUMENT_CID);
#ifdef MOZ_MEDIA
static NS_DEFINE_IID(kVideoDocumentCID, NS_VIDEODOCUMENT_CID);
#endif
static NS_DEFINE_IID(kImageDocumentCID, NS_IMAGEDOCUMENT_CID);
static NS_DEFINE_IID(kXULDocumentCID, NS_XULDOCUMENT_CID);

nsresult
NS_NewContentViewer(nsIContentViewer** aResult);

// XXXbz if you change the MIME types here, be sure to update
// nsIParser.h and DetermineParseMode in nsParser.cpp and
// nsHTMLDocument::StartDocumentLoad accordingly.
static const char* const gHTMLTypes[] = {
  TEXT_HTML,
  TEXT_PLAIN,
  TEXT_CSS,
  TEXT_JAVASCRIPT,
  TEXT_ECMASCRIPT,
  APPLICATION_JAVASCRIPT,
  APPLICATION_ECMASCRIPT,
  APPLICATION_XJAVASCRIPT,
  APPLICATION_JSON,
  VIEWSOURCE_CONTENT_TYPE,
  APPLICATION_XHTML_XML,
  0
};
  
static const char* const gXMLTypes[] = {
  TEXT_XML,
  APPLICATION_XML,
  APPLICATION_MATHML_XML,
  APPLICATION_RDF_XML,
  TEXT_RDF,
  0
};

static const char* const gSVGTypes[] = {
  IMAGE_SVG_XML,
  0
};

static const char* const gXULTypes[] = {
  TEXT_XUL,
  APPLICATION_CACHED_XUL,
  0
};

nsresult
NS_NewContentDocumentLoaderFactory(nsIDocumentLoaderFactory** aResult)
{
  NS_PRECONDITION(aResult, "null OUT ptr");
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsContentDLF* it = new nsContentDLF();
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(it, aResult);
}

nsContentDLF::nsContentDLF()
{
}

nsContentDLF::~nsContentDLF()
{
}

NS_IMPL_ISUPPORTS1(nsContentDLF,
                   nsIDocumentLoaderFactory)

bool
MayUseXULXBL(nsIChannel* aChannel)
{
  nsIScriptSecurityManager *securityManager =
    nsContentUtils::GetSecurityManager();
  if (!securityManager) {
    return false;
  }

  nsCOMPtr<nsIPrincipal> principal;
  securityManager->GetChannelPrincipal(aChannel, getter_AddRefs(principal));
  NS_ENSURE_TRUE(principal, false);

  return nsContentUtils::AllowXULXBLForPrincipal(principal);
}

NS_IMETHODIMP
nsContentDLF::CreateInstance(const char* aCommand,
                             nsIChannel* aChannel,
                             nsILoadGroup* aLoadGroup,
                             const char* aContentType, 
                             nsISupports* aContainer,
                             nsISupports* aExtraInfo,
                             nsIStreamListener** aDocListener,
                             nsIContentViewer** aDocViewer)
{
#ifdef NS_FUNCTION_TIMER
  nsCAutoString channelURL__("N/A");
  nsCOMPtr<nsIURI> url__;
  if (aChannel && NS_SUCCEEDED(aChannel->GetURI(getter_AddRefs(url__)))) {
    url__->GetSpec(channelURL__);
  }
  NS_TIME_FUNCTION_FMT("%s (line %d) (url: %s)", MOZ_FUNCTION_NAME,
                       __LINE__, channelURL__.get());
#endif

  // Declare "type" here.  This is because although the variable itself only
  // needs limited scope, we need to use the raw string memory -- as returned
  // by "type.get()" farther down in the function.
  nsCAutoString type;

  // Are we viewing source?
  nsCOMPtr<nsIViewSourceChannel> viewSourceChannel = do_QueryInterface(aChannel);
  if (viewSourceChannel)
  {
    aCommand = "view-source";

    // The parser freaks out when it sees the content-type that a
    // view-source channel normally returns.  Get the actual content
    // type of the data.  If it's known, use it; otherwise use
    // text/plain.
    viewSourceChannel->GetOriginalContentType(type);
    bool knownType = false;
    PRInt32 typeIndex;
    for (typeIndex = 0; gHTMLTypes[typeIndex] && !knownType; ++typeIndex) {
      if (type.Equals(gHTMLTypes[typeIndex]) &&
          !type.EqualsLiteral(VIEWSOURCE_CONTENT_TYPE)) {
        knownType = true;
      }
    }

    for (typeIndex = 0; gXMLTypes[typeIndex] && !knownType; ++typeIndex) {
      if (type.Equals(gXMLTypes[typeIndex])) {
        knownType = true;
      }
    }

    for (typeIndex = 0; gSVGTypes[typeIndex] && !knownType; ++typeIndex) {
      if (type.Equals(gSVGTypes[typeIndex])) {
        knownType = true;
      }
    }

    for (typeIndex = 0; gXULTypes[typeIndex] && !knownType; ++typeIndex) {
      if (type.Equals(gXULTypes[typeIndex])) {
        knownType = true;
      }
    }

    if (knownType) {
      viewSourceChannel->SetContentType(type);
    } else if (IsImageContentType(type.get())) {
      // If it's an image, we want to display it the same way we normally would.
      // Also note the lifetime of "type" allows us to safely use "get()" here.
      aContentType = type.get();
    } else {
      viewSourceChannel->SetContentType(NS_LITERAL_CSTRING(TEXT_PLAIN));
    }
  } else if (0 == PL_strcmp(VIEWSOURCE_CONTENT_TYPE, aContentType)) {
    aChannel->SetContentType(NS_LITERAL_CSTRING(TEXT_PLAIN));
    aContentType = TEXT_PLAIN;
  }
  // Try html
  int typeIndex=0;
  while(gHTMLTypes[typeIndex]) {
    if (0 == PL_strcmp(gHTMLTypes[typeIndex++], aContentType)) {
      return CreateDocument(aCommand, 
                            aChannel, aLoadGroup,
                            aContainer, kHTMLDocumentCID,
                            aDocListener, aDocViewer);
    }
  }

  // Try XML
  typeIndex = 0;
  while(gXMLTypes[typeIndex]) {
    if (0== PL_strcmp(gXMLTypes[typeIndex++], aContentType)) {
      return CreateDocument(aCommand, 
                            aChannel, aLoadGroup,
                            aContainer, kXMLDocumentCID,
                            aDocListener, aDocViewer);
    }
  }

  // Try SVG
  typeIndex = 0;
  while(gSVGTypes[typeIndex]) {
    if (!PL_strcmp(gSVGTypes[typeIndex++], aContentType)) {
      return CreateDocument(aCommand,
                            aChannel, aLoadGroup,
                            aContainer, kSVGDocumentCID,
                            aDocListener, aDocViewer);
    }
  }

  // Try XUL
  typeIndex = 0;
  while (gXULTypes[typeIndex]) {
    if (0 == PL_strcmp(gXULTypes[typeIndex++], aContentType)) {
      if (!MayUseXULXBL(aChannel)) {
        return NS_ERROR_REMOTE_XUL;
      }

      return CreateXULDocument(aCommand,
                               aChannel, aLoadGroup,
                               aContentType, aContainer,
                               aExtraInfo, aDocListener, aDocViewer);
    }
  }

#ifdef MOZ_MEDIA
  if (nsHTMLMediaElement::ShouldHandleMediaType(aContentType)) {
    return CreateDocument(aCommand, 
                          aChannel, aLoadGroup,
                          aContainer, kVideoDocumentCID,
                          aDocListener, aDocViewer);
  }  
#endif

  // Try image types
  if (IsImageContentType(aContentType)) {
    return CreateDocument(aCommand, 
                          aChannel, aLoadGroup,
                          aContainer, kImageDocumentCID,
                          aDocListener, aDocViewer);
  }

  nsCOMPtr<nsIPluginHost> pluginHostCOM(do_GetService(MOZ_PLUGIN_HOST_CONTRACTID));
  nsPluginHost *pluginHost = static_cast<nsPluginHost*>(pluginHostCOM.get());
  if(pluginHost &&
     NS_SUCCEEDED(pluginHost->IsPluginEnabledForType(aContentType))) {
    return CreateDocument(aCommand,
                          aChannel, aLoadGroup,
                          aContainer, kPluginDocumentCID,
                          aDocListener, aDocViewer);
  }

  // If we get here, then we weren't able to create anything. Sorry!
  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsContentDLF::CreateInstanceForDocument(nsISupports* aContainer,
                                        nsIDocument* aDocument,
                                        const char *aCommand,
                                        nsIContentViewer** aContentViewer)
{
  NS_TIME_FUNCTION;

  nsCOMPtr<nsIContentViewer> contentViewer;
  nsresult rv = NS_NewContentViewer(getter_AddRefs(contentViewer));
  NS_ENSURE_SUCCESS(rv, rv);

  // Bind the document to the Content Viewer
  rv = contentViewer->LoadStart(aDocument);
  contentViewer.forget(aContentViewer);
  return rv;
}

NS_IMETHODIMP
nsContentDLF::CreateBlankDocument(nsILoadGroup *aLoadGroup,
                                  nsIPrincipal* aPrincipal,
                                  nsIDocument **aDocument)
{
  NS_TIME_FUNCTION;

  *aDocument = nsnull;

  nsresult rv = NS_ERROR_FAILURE;

  // create a new blank HTML document
  nsCOMPtr<nsIDocument> blankDoc(do_CreateInstance(kHTMLDocumentCID));

  if (blankDoc) {
    // initialize
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), NS_LITERAL_CSTRING("about:blank"));
    if (uri) {
      blankDoc->ResetToURI(uri, aLoadGroup, aPrincipal);
      rv = NS_OK;
    }
  }

  // add some simple content structure
  if (NS_SUCCEEDED(rv)) {
    rv = NS_ERROR_FAILURE;

    nsNodeInfoManager *nim = blankDoc->NodeInfoManager();

    nsCOMPtr<nsINodeInfo> htmlNodeInfo;

    // generate an html html element
    htmlNodeInfo = nim->GetNodeInfo(nsGkAtoms::html, 0, kNameSpaceID_XHTML,
                                    nsIDOMNode::ELEMENT_NODE);
    nsCOMPtr<nsIContent> htmlElement =
      NS_NewHTMLHtmlElement(htmlNodeInfo.forget());

    // generate an html head element
    htmlNodeInfo = nim->GetNodeInfo(nsGkAtoms::head, 0, kNameSpaceID_XHTML,
                                    nsIDOMNode::ELEMENT_NODE);
    nsCOMPtr<nsIContent> headElement =
      NS_NewHTMLHeadElement(htmlNodeInfo.forget());

    // generate an html body elemment
    htmlNodeInfo = nim->GetNodeInfo(nsGkAtoms::body, 0, kNameSpaceID_XHTML,
                                    nsIDOMNode::ELEMENT_NODE);
    nsCOMPtr<nsIContent> bodyElement =
      NS_NewHTMLBodyElement(htmlNodeInfo.forget());

    // blat in the structure
    if (htmlElement && headElement && bodyElement) {
      NS_ASSERTION(blankDoc->GetChildCount() == 0,
                   "Shouldn't have children");
      rv = blankDoc->AppendChildTo(htmlElement, false);
      if (NS_SUCCEEDED(rv)) {
        rv = htmlElement->AppendChildTo(headElement, false);

        if (NS_SUCCEEDED(rv)) {
          // XXXbz Why not notifying here?
          htmlElement->AppendChildTo(bodyElement, false);
        }
      }
    }
  }

  // add a nice bow
  if (NS_SUCCEEDED(rv)) {
    blankDoc->SetDocumentCharacterSetSource(kCharsetFromDocTypeDefault);
    blankDoc->SetDocumentCharacterSet(NS_LITERAL_CSTRING("UTF-8"));
    
    *aDocument = blankDoc;
    NS_ADDREF(*aDocument);
  }
  return rv;
}


nsresult
nsContentDLF::CreateDocument(const char* aCommand,
                             nsIChannel* aChannel,
                             nsILoadGroup* aLoadGroup,
                             nsISupports* aContainer,
                             const nsCID& aDocumentCID,
                             nsIStreamListener** aDocListener,
                             nsIContentViewer** aContentViewer)
{
  NS_TIME_FUNCTION;

  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIURI> aURL;
  rv = aChannel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) return rv;

#ifdef NOISY_CREATE_DOC
  if (nsnull != aURL) {
    nsAutoString tmp;
    aURL->ToString(tmp);
    fputs(NS_LossyConvertUTF16toASCII(tmp).get(), stdout);
    printf(": creating document\n");
  }
#endif

  // Create the document
  nsCOMPtr<nsIDocument> doc = do_CreateInstance(aDocumentCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create the content viewer  XXX: could reuse content viewer here!
  nsCOMPtr<nsIContentViewer> contentViewer;
  rv = NS_NewContentViewer(getter_AddRefs(contentViewer));
  NS_ENSURE_SUCCESS(rv, rv);

  doc->SetContainer(aContainer);

  // Initialize the document to begin loading the data.  An
  // nsIStreamListener connected to the parser is returned in
  // aDocListener.
  rv = doc->StartDocumentLoad(aCommand, aChannel, aLoadGroup, aContainer, aDocListener, true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Bind the document to the Content Viewer
  rv = contentViewer->LoadStart(doc);
  contentViewer.forget(aContentViewer);
  return rv;
}

nsresult
nsContentDLF::CreateXULDocument(const char* aCommand,
                                nsIChannel* aChannel,
                                nsILoadGroup* aLoadGroup,
                                const char* aContentType,
                                nsISupports* aContainer,
                                nsISupports* aExtraInfo,
                                nsIStreamListener** aDocListener,
                                nsIContentViewer** aContentViewer)
{
  NS_TIME_FUNCTION;

  nsresult rv;
  nsCOMPtr<nsIDocument> doc = do_CreateInstance(kXULDocumentCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIContentViewer> contentViewer;
  rv = NS_NewContentViewer(getter_AddRefs(contentViewer));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIURI> aURL;
  rv = aChannel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) return rv;

  /* 
   * Initialize the document to begin loading the data...
   *
   * An nsIStreamListener connected to the parser is returned in
   * aDocListener.
   */

  doc->SetContainer(aContainer);

  rv = doc->StartDocumentLoad(aCommand, aChannel, aLoadGroup, aContainer, aDocListener, true);
  if (NS_FAILED(rv)) return rv;

  /*
   * Bind the document to the Content Viewer...
   */
  rv = contentViewer->LoadStart(doc);
  contentViewer.forget(aContentViewer);
  return rv;
}

bool nsContentDLF::IsImageContentType(const char* aContentType) {
  nsCOMPtr<imgILoader> loader(do_GetService("@mozilla.org/image/loader;1"));
  bool isDecoderAvailable = false;
  loader->SupportImageWithMimeType(aContentType, &isDecoderAvailable);
  return isDecoderAvailable;
}
