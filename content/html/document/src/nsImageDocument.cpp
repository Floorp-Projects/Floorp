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
 */
#include "nsCOMPtr.h"
#include "nsHTMLDocument.h"
#include "nsHTMLParts.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLContent.h"
#include "nsIImageGroup.h"
#include "nsIImageRequest.h"
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

static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOCUMENT_IID);

class nsImageDocument : public nsHTMLDocument {
public:
  nsImageDocument();
  virtual ~nsImageDocument();

  NS_IMETHOD StartDocumentLoad(const char* aCommand,
                               nsIChannel* aChannel,
                               nsILoadGroup* aLoadGroup,
                               nsISupports* aContainer,
                               nsIStreamListener **aDocListener,
                               PRBool aReset = PR_TRUE);

  nsresult CreateSyntheticDocument();

  nsresult StartImageLoad(nsIURI* aURL, nsIStreamListener*& aListener);
  nsresult EndLayout(nsISupports *ctxt, 
                        nsresult status, 
                        const PRUnichar *errorMsg);
  nsresult UpdateTitle( void );

  void StartLayout();

  nsIImageRequest*  mImageRequest;
  nscolor           mBlack;
};

//----------------------------------------------------------------------

class ImageListener : public nsIStreamListener {
public:
  ImageListener(nsImageDocument* aDoc);
  virtual ~ImageListener();

  NS_DECL_ISUPPORTS
  // nsIStreamObserver methods:
  NS_DECL_NSISTREAMOBSERVER

  // nsIStreamListener methods:
  NS_DECL_NSISTREAMLISTENER

  nsImageDocument* mDocument;
  nsIStreamListener* mNextStream;
};

ImageListener::ImageListener(nsImageDocument* aDoc)
{
  mDocument = aDoc;
  NS_ADDREF(aDoc);
  mRefCnt = 1;
}

ImageListener::~ImageListener()
{
  NS_RELEASE(mDocument);
  NS_IF_RELEASE(mNextStream);
}

NS_IMPL_THREADSAFE_ISUPPORTS(ImageListener, kIStreamListenerIID)

NS_IMETHODIMP
ImageListener::OnStartRequest(nsIChannel* channel, nsISupports *ctxt)
{
  nsresult rv;
  nsIURI* uri;
  rv = channel->GetURI(&uri);
  if (NS_FAILED(rv)) return rv;
  
  mDocument->StartImageLoad(uri, mNextStream);
  NS_RELEASE(uri);
  if (nsnull == mNextStream) {
    return NS_ERROR_FAILURE;
  }
  return mNextStream->OnStartRequest(channel, ctxt);
}

NS_IMETHODIMP
ImageListener::OnStopRequest(nsIChannel* channel, nsISupports *ctxt,
                             nsresult status, const PRUnichar *errorMsg)
{
  if(mDocument){
    mDocument->EndLayout(ctxt, status, errorMsg);
  }

  if (nsnull == mNextStream) {
    return NS_ERROR_FAILURE;
  }
  return mNextStream->OnStopRequest(channel, ctxt, status, errorMsg);
}

NS_IMETHODIMP
ImageListener::OnDataAvailable(nsIChannel* channel, nsISupports *ctxt,
                               nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
{
  if (nsnull == mNextStream) {
    return NS_ERROR_FAILURE;
  }
  return mNextStream->OnDataAvailable(channel, ctxt, inStr, sourceOffset, count);
}

//----------------------------------------------------------------------

NS_LAYOUT nsresult
NS_NewImageDocument(nsIDocument** aInstancePtrResult)
{
  nsImageDocument* doc = new nsImageDocument();
  if(doc)
    return doc->QueryInterface(kIDocumentIID, (void**) aInstancePtrResult);
  return NS_ERROR_OUT_OF_MEMORY;
}

nsImageDocument::nsImageDocument()
{
  mImageRequest = nsnull;
  mBlack = NS_RGB(0, 0, 0);
}

nsImageDocument::~nsImageDocument()
{
  NS_IF_RELEASE(mImageRequest);
}

NS_IMETHODIMP
nsImageDocument::StartDocumentLoad(const char* aCommand,
                                   nsIChannel* aChannel,
                                   nsILoadGroup* aLoadGroup,
                                   nsISupports* aContainer,
                                   nsIStreamListener **aDocListener,
                                   PRBool aReset)
{
  nsresult rv = Init();

  if (NS_FAILED(rv) && rv != NS_ERROR_ALREADY_INITIALIZED) {
    return rv;
  }

  rv = nsDocument::StartDocumentLoad(aCommand, aChannel, aLoadGroup,
                                     aContainer, aDocListener, aReset);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Create synthetic document
  rv = CreateSyntheticDocument();
  if (NS_OK != rv) {
    return rv;
  }

  *aDocListener = new ImageListener(this);

  return NS_OK;
}

nsresult
nsImageDocument::StartImageLoad(nsIURI* aURL, nsIStreamListener*& aListener)
{
  nsresult rv = NS_OK;
  aListener = nsnull;

  // Tell image group to load the stream now. This will get the image
  // hooked up to the open stream and return the underlying listener
  // so that we can pass it back upwards.
  nsIPresShell* shell = GetShellAt(0);
  if (nsnull != shell) {
    nsCOMPtr<nsIPresContext> cx;
    shell->GetPresContext(getter_AddRefs(cx));
    if (cx) {
      nsIImageGroup* group = nsnull;
      cx->GetImageGroup(&group);
      if (nsnull != group) {

        char* spec;
        (void)aURL->GetSpec(&spec);
        nsIStreamListener* listener = nsnull;
        rv = group->GetImageFromStream(spec, nsnull, nsnull,
                                       0, 0, 0,
                                       mImageRequest, listener);

        //set flag to indicate view-image needs to use imgcache
        group->SetImgLoadAttributes(nsImageLoadFlags_kSticky);

        nsCRT::free(spec);
        aListener = listener;
        NS_RELEASE(group);
      }
    }
    NS_RELEASE(shell);
  }

  
  // Finally, start the layout going
  StartLayout();

  return NS_OK;
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

  char* src;
  mDocumentURL->GetSpec(&src);

  nsString src_string; src_string.AssignWithConversion(src);
  nsHTMLValue val(src_string);
  delete[] src;
  image->SetHTMLAttribute(nsHTMLAtoms::src, val, PR_FALSE);
  image->SetHTMLAttribute(nsHTMLAtoms::alt, val, PR_FALSE);

  root->AppendChildTo(body, PR_FALSE);
  center->AppendChildTo(image, PR_FALSE);
  body->AppendChildTo(center, PR_FALSE);

  NS_RELEASE(image);
  NS_RELEASE(center);
  NS_RELEASE(body);
  NS_RELEASE(root);

  return NS_OK;
}

void
nsImageDocument::StartLayout()
{
  PRInt32 i, ns = GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsIPresShell* shell = GetShellAt(i);
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

      NS_RELEASE(shell);
    }
  }
}

nsresult
nsImageDocument::EndLayout(nsISupports *ctxt, 
                           nsresult status, 
                           const PRUnichar *errorMsg)
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
  nsString titleStr;

#ifdef USE_EXTENSION_FOR_TYPE
  // XXX TEMPORARY XXX
  // We want to display the image type, however there is no way to right now
  // so instead we just get the image-extension
  //  - get the URL interface, get the extension, convert to upper-case
  //  Unless the Imagerequest or Image can tell us the type this is the best we can do.
  nsIURL *pURL=nsnull;
  if(NS_SUCCEEDED(mDocumentURL->QueryInterface(nsIURL::GetIID(),(void **)&pURL))){
    char *pExtension=nsnull;
    pURL->GetFileExtension(&pExtension);
    if(pExtension){
      nsString strExt; strExt.AssignWithConversion(pExtension);
      strExt.ToUpperCase();
      titleStr.Append(strExt);
      nsCRT::free(pExtension);
      pExtension=nsnull;
    }
    NS_IF_RELEASE(pURL);
  }
#endif

  // append the image information...
  titleStr.AppendWithConversion( " Image" );
  if (mImageRequest) {
    PRUint32 width, height;
    mImageRequest->GetNaturalDimensions(&width, &height);
    // if we got a valid size (sometimes we do not) then display it
    if (width != 0 && height != 0){
      titleStr.AppendWithConversion( " " );
      titleStr.AppendInt((PRInt32)width);
      titleStr.AppendWithConversion("x");
      titleStr.AppendInt((PRInt32)height);
      titleStr.AppendWithConversion(" pixels");
    }
  } 
  // set it on the document
  SetTitle(titleStr);
  return NS_OK;
}
