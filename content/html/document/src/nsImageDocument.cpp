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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Morten Nilsen <morten@nilsen.com>
 *    Christian Biesinger <cbiesinger@web.de>
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

#include "nsCOMPtr.h"
#include "nsHTMLDocument.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLContent.h"
#include "imgIRequest.h"
#include "imgILoader.h"
#include "imgIContainer.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsHTMLValue.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIViewManager.h"
#include "nsIChannel.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
// Needed for Localization
#include "nsXPIDLString.h"
#include "nsIStringBundle.h"
// Needed to fetch scrollbar prefs for image documents that are iframes/frameset frames
#include "nsIScrollable.h"
#include "nsWeakReference.h"
#include "nsRect.h"

#define NSIMAGEDOCUMENT_PROPERTIES_URI "chrome://communicator/locale/layout/ImageDocument.properties"
static NS_DEFINE_CID(kStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);

// done L10N

// XXX TODO:

// 1. hookup the original stream to the image library directly so that we
//    don't have to load the image twice.

// 2. have a synthetic document url that we load that has patterns in it
//    that we replace with the image url (so that we can customize the
//    the presentation of the image): we could add script code to set the
//    width/height to provide scale buttons, we could show the image
//    attributes; etc.; should we have a seperate style sheet?

// 3. override the save-as methods so that we don't write out our synthetic
//    html


class nsImageDocument : public nsHTMLDocument {
public:
  nsImageDocument();
  virtual ~nsImageDocument();

  NS_IMETHOD StartDocumentLoad(const char* aCommand,
                               nsIChannel* aChannel,
                               nsILoadGroup* aLoadGroup,
                               nsISupports* aContainer,
                               nsIStreamListener **aDocListener,
                               PRBool aReset = PR_TRUE,
                               nsIContentSink* aSink = nsnull);

  NS_IMETHOD SetScriptGlobalObject(nsIScriptGlobalObject* aScriptGlobalObject);

  nsresult CreateSyntheticDocument();

  nsresult EndLayout(nsISupports *ctxt, 
                     nsresult status);
  nsresult UpdateTitle( void );

  void StartLayout();
  nsCOMPtr<imgIRequest> mImageRequest;
  nscolor           mBlack;
  nsWeakPtr         mContainer;
};

//----------------------------------------------------------------------

class ImageListener : public nsIStreamListener {
public:
  ImageListener(nsImageDocument* aDoc);
  virtual ~ImageListener();

  NS_DECL_ISUPPORTS
  // nsIRequestObserver methods:
  NS_DECL_NSIREQUESTOBSERVER

  // nsIStreamListener methods:
  NS_DECL_NSISTREAMLISTENER

  nsImageDocument* mDocument;
  nsCOMPtr<nsIStreamListener> mNextStream;
};

ImageListener::ImageListener(nsImageDocument* aDoc)
{
  NS_INIT_ISUPPORTS();
  mDocument = aDoc;
  NS_ADDREF(aDoc);
}

ImageListener::~ImageListener()
{
  NS_RELEASE(mDocument);
}

NS_IMPL_THREADSAFE_ISUPPORTS1(ImageListener, nsIStreamListener)

NS_IMETHODIMP
ImageListener::OnStartRequest(nsIRequest* request, nsISupports *ctxt)
{
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  if (!channel) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIPresShell> shell;
  nsCOMPtr<nsIPresContext> context;
  mDocument->GetShellAt(0, getter_AddRefs(shell));
  if (shell) {
    shell->GetPresContext(getter_AddRefs(context));
  }

  nsCOMPtr<nsIStreamListener> kungFuDeathGrip(this);
  nsCOMPtr<imgILoader> il(do_GetService("@mozilla.org/image/loader;1"));
  il->LoadImageWithChannel(channel, nsnull, context, getter_AddRefs(mNextStream), 
                           getter_AddRefs(mDocument->mImageRequest));

  // XXX
  // if we get a cache hit, and we cancel the channel in the above function,
  // do we call OnStopRequest before we call StartLayout?
  // if so, should LoadImageWithChannel() not call channel->Cancel() ?

  mDocument->StartLayout();

  if (nsnull == mNextStream) {
    return NS_ERROR_FAILURE;
  }
  return mNextStream->OnStartRequest(request, ctxt);
}

NS_IMETHODIMP
ImageListener::OnStopRequest(nsIRequest* request, nsISupports *ctxt,
                             nsresult status)
{
  if(mDocument){
    mDocument->EndLayout(ctxt, status);
  }

  if (nsnull == mNextStream) {
    return NS_ERROR_FAILURE;
  }
  return mNextStream->OnStopRequest(request, ctxt, status);
}

NS_IMETHODIMP
ImageListener::OnDataAvailable(nsIRequest* request, nsISupports *ctxt,
                               nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
{
  if (nsnull == mNextStream) {
    return NS_ERROR_FAILURE;
  }
  return mNextStream->OnDataAvailable(request, ctxt, inStr, sourceOffset, count);
}

//----------------------------------------------------------------------

NS_EXPORT nsresult
NS_NewImageDocument(nsIDocument** aInstancePtrResult)
{
  nsImageDocument* doc = new nsImageDocument();
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

nsImageDocument::nsImageDocument()
{
  mBlack = NS_RGB(0, 0, 0);
}

nsImageDocument::~nsImageDocument()
{
}

NS_IMETHODIMP
nsImageDocument::StartDocumentLoad(const char* aCommand,
                                   nsIChannel* aChannel,
                                   nsILoadGroup* aLoadGroup,
                                   nsISupports* aContainer,
                                   nsIStreamListener **aDocListener,
                                   PRBool aReset,
                                   nsIContentSink* aSink)
{
  NS_ASSERTION(aDocListener, "null aDocListener");
  NS_ENSURE_ARG_POINTER(aContainer);
  mContainer = dont_AddRef(NS_GetWeakReference(aContainer));

  nsresult rv = nsDocument::StartDocumentLoad(aCommand, aChannel, aLoadGroup,
                                              aContainer, aDocListener, aReset,
                                              aSink);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  if (httpChannel) {
    // The misspelled key 'referer' is as per the HTTP spec
    nsCAutoString referrerHeader;
    rv = httpChannel->GetRequestHeader(NS_LITERAL_CSTRING("referer"),
                                       referrerHeader);
    
    if (NS_SUCCEEDED(rv)) {
      SetReferrer(NS_ConvertASCIItoUCS2(referrerHeader));
    }
  }

  // Create synthetic document
  rv = CreateSyntheticDocument();
  if (NS_OK != rv) {
    return rv;
  }

  *aDocListener = new ImageListener(this);
  if (!*aDocListener)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aDocListener);

  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::SetScriptGlobalObject(nsIScriptGlobalObject* aScriptGlobalObject)
{
  if (!aScriptGlobalObject) {
    // If the global object is being set to null, then it means we are
    // going away soon. Drop our ref to imgRequest so that we don't end
    // up leaking due to cycles through imgLib
    mImageRequest = nsnull;
  }

  return nsHTMLDocument::SetScriptGlobalObject(aScriptGlobalObject);
}

nsresult
nsImageDocument::CreateSyntheticDocument()
{
  // Synthesize an html document that refers to the image
  nsresult rv;

  nsCOMPtr<nsINodeInfo> nodeInfo;
  rv = mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::html, nsnull,
                                     kNameSpaceID_None,
                                     *getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsIHTMLContent* root;
  rv = NS_NewHTMLHtmlElement(&root, nodeInfo);
  if (NS_OK != rv) {
    return rv;
  }
  root->SetDocument(this, PR_FALSE, PR_TRUE);
  SetRootContent(root);

  rv = mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::body, nsnull,
                                     kNameSpaceID_None,
                                     *getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsIHTMLContent* body;
  rv = NS_NewHTMLBodyElement(&body, nodeInfo);
  if (NS_OK != rv) {
    return rv;
  }
  body->SetDocument(this, PR_FALSE, PR_TRUE);

  rv = mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::p, nsnull, kNameSpaceID_None,
                                     *getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsIHTMLContent* center;
  rv = NS_NewHTMLParagraphElement(&center, nodeInfo);
  if (NS_OK != rv) {
    return rv;
  }
  center->SetDocument(this, PR_FALSE, PR_TRUE);

  rv = mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::img, nsnull,
                                     kNameSpaceID_None,
                                     *getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsIHTMLContent* image;
  rv = NS_NewHTMLImageElement(&image, nodeInfo);
  if (NS_OK != rv) {
    return rv;
  }
  image->SetDocument(this, PR_FALSE, PR_TRUE);

  nsCAutoString src;
  mDocumentURL->GetSpec(src);

  NS_ConvertUTF8toUCS2 src_string(src);
  nsHTMLValue val(src_string);

  image->SetHTMLAttribute(nsHTMLAtoms::src, val, PR_FALSE);

  // Create a stringbundle for localized error message
  nsCOMPtr<nsIStringBundle> bundle;
  nsCOMPtr<nsIStringBundleService> stringService =
           do_GetService(kStringBundleServiceCID, &rv);
  if (NS_SUCCEEDED(rv) && stringService)
    rv = stringService->CreateBundle(NSIMAGEDOCUMENT_PROPERTIES_URI, getter_AddRefs(bundle));
  if (NS_SUCCEEDED(rv) && bundle) {
    const PRUnichar* formatString[1] = { src_string.get() };
    nsXPIDLString errorMsg;
    rv = bundle->FormatStringFromName(NS_LITERAL_STRING("InvalidImage").get(), formatString, 1, getter_Copies(errorMsg));

    nsHTMLValue errorText(errorMsg);
    image->SetHTMLAttribute(nsHTMLAtoms::alt, errorText, PR_FALSE);
  }

  root->AppendChildTo(body, PR_FALSE, PR_FALSE);
  center->AppendChildTo(image, PR_FALSE, PR_FALSE);
  body->AppendChildTo(center, PR_FALSE, PR_FALSE);

  NS_RELEASE(image);
  NS_RELEASE(center);
  NS_RELEASE(body);
  NS_RELEASE(root);

  return NS_OK;
}

void
nsImageDocument::StartLayout()
{

  // Reset scrolling to default settings for this shell.
  // This must happen before the initial reflow, when we create the root frame
  nsCOMPtr<nsIScrollable> scrollableContainer(do_QueryReferent(mContainer));
  if (scrollableContainer) {
    scrollableContainer->ResetScrollbarPreferences();
  }


  PRInt32 i, ns = GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsCOMPtr<nsIPresShell> shell;
    GetShellAt(i, getter_AddRefs(shell));
    if (nsnull != shell) {
      // Make shell an observer for next time
      shell->BeginObservingDocument();

      // Resize-reflow this time
      nsCOMPtr<nsIPresContext> cx;
      shell->GetPresContext(getter_AddRefs(cx));
      nsRect r;
      cx->GetVisibleArea(r);
      shell->InitialReflow(r.width, r.height);

      // Now trigger a refresh
      // XXX It's unfortunate that this has to be here
      nsCOMPtr<nsIViewManager> vm;
      shell->GetViewManager(getter_AddRefs(vm));
      if (vm) {
        vm->EnableRefresh(NS_VMREFRESH_IMMEDIATE);
      }

    }
  }
}

nsresult
nsImageDocument::EndLayout(nsISupports *ctxt, 
                           nsresult status)
{
  // Layout has completed: now update the title
  UpdateTitle();

  return NS_OK;
}


// NOTE: call this AFTER the shell has been installed as an observer of the 
//       document so the update notification gets processed by the shell 
//       and it updates the titlebar
nsresult nsImageDocument::UpdateTitle( void )
{
#ifdef USE_EXTENSION_FOR_TYPE
  // XXX TEMPORARY XXX
  // We want to display the image type, however there is no way to right now
  // so instead we just get the image-extension
  //  - get the URL interface, get the extension, convert to upper-case
  //  Unless the Imagerequest or Image can tell us the type this is the best we can do.
  nsCOMPtr<nsIURL> url = do_QueryInterface(mDocumentURL);
  if (url) {
    nsXPIDLCString extension;
    url->GetFileExtension(getter_Copies(extension));
    ToUpperCase(extension);
    titleStr.AppendWithConversion(extension);
  }
#endif
 
  nsCOMPtr<nsIStringBundle> bundle;
  nsresult rv; 
  // Create a bundle for the localization
  nsCOMPtr<nsIStringBundleService> stringService = 
           do_GetService(kStringBundleServiceCID, &rv);
  if (NS_SUCCEEDED(rv) && stringService) {
    rv = stringService->CreateBundle(NSIMAGEDOCUMENT_PROPERTIES_URI, getter_AddRefs(bundle));
  }
  if (NS_SUCCEEDED(rv) && bundle) {
    nsXPIDLString valUni;
    nsAutoString widthStr;
    nsAutoString heightStr;
    nsXPIDLString fileStr;
    nsAutoString typeStr;
    PRUint32 width = 0, height = 0;

    nsCOMPtr<nsIURL> url = do_QueryInterface(mDocumentURL);
    if (url) {
      nsCAutoString pName;
      url->GetFileName(pName);
      fileStr.Assign(NS_ConvertUTF8toUCS2(pName));
    }

    if (mImageRequest) {
      imgIContainer* imgContainer;
      rv = mImageRequest->GetImage(&imgContainer);
      if (NS_SUCCEEDED(rv) && imgContainer) {
        nscoord w = 0, h = 0;
        imgContainer->GetWidth(&w);
        imgContainer->GetHeight(&h);
        width = w;
        height = h;
        NS_RELEASE(imgContainer);
      }

      widthStr.AppendInt(width);
      heightStr.AppendInt(height);

      nsXPIDLCString mimeType;
      mImageRequest->GetMimeType(getter_Copies(mimeType));
      ToUpperCase(mimeType);
      nsXPIDLCString::const_iterator start, end;
      mimeType.BeginReading(start);
      mimeType.EndReading(end);
      nsXPIDLCString::const_iterator iter = end;
      if (FindInReadable(NS_LITERAL_CSTRING("IMAGE/"), start, iter) && iter != end) {
        // strip out "X-" if any
        if (*iter == 'X') {
          ++iter;
          if (iter != end && *iter == '-') {
            ++iter;
            if (iter == end) {
              // looks like "IMAGE/X-" is the type??  Bail out of here.
              mimeType.BeginReading(iter);
            }
          } else {
            --iter;
          }
        }
        CopyASCIItoUCS2(Substring(iter, end), typeStr);
      } else {
        CopyASCIItoUCS2(mimeType, typeStr);
      }
    }
    // If we got a filename, display it
    if (!fileStr.IsEmpty()) {
      // if we got a valid size (sometimes we do not) then display it
      if (width != 0 && height != 0){
        const PRUnichar *formatStrings[4]  = {fileStr.get(), typeStr.get(), widthStr.get(), heightStr.get()};
        rv = bundle->FormatStringFromName(NS_LITERAL_STRING("ImageTitleWithDimensionsAndFile").get(), formatStrings, 4, getter_Copies(valUni));
      } else {
        const PRUnichar *formatStrings[2] = {fileStr.get(), typeStr.get()};
        rv = bundle->FormatStringFromName(NS_LITERAL_STRING("ImageTitleWithoutDimensions").get(), formatStrings, 2, getter_Copies(valUni));
      }
    } else {
      // if we got a valid size (sometimes we do not) then display it
      if (width != 0 && height != 0){
        const PRUnichar *formatStrings[3]  = {typeStr.get(), widthStr.get(), heightStr.get()};
        rv = bundle->FormatStringFromName(NS_LITERAL_STRING("ImageTitleWithDimensions").get(), formatStrings, 3, getter_Copies(valUni));
      } else {
        const PRUnichar *formatStrings[1]  = {typeStr.get()};
        rv = bundle->FormatStringFromName(NS_LITERAL_STRING("ImageTitleWithNeitherDimensionsNorFile").get(), formatStrings, 1, getter_Copies(valUni));
      }
    }

    if (NS_SUCCEEDED(rv) && valUni) {
      // set it on the document
      SetTitle(valUni);
    }
  }
  return NS_OK;
}
