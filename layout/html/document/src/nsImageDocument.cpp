/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
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
#ifdef NECKO
#include "nsIChannel.h"
#endif

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
#ifdef NECKO
                               nsIChannel* aChannel,
                               nsILoadGroup* aLoadGroup,
#else
                               nsIURI *aUrl, 
#endif
                               nsIContentViewerContainer* aContainer,
                               nsIStreamListener **aDocListener);

  nsresult CreateSyntheticDocument();

  nsresult StartImageLoad(nsIURI* aURL, nsIStreamListener*& aListener);
#ifdef NECKO
  nsresult EndLayout(nsISupports *ctxt, 
                        nsresult status, 
                        const PRUnichar *errorMsg);
#else
  nsresult EndLayout(nsIURI* aURL, 
                        nsresult aStatus,
                        const PRUnichar* aMsg);
#endif

  void StartLayout();

  nsIImageRequest* mImageRequest;
  nscolor mBlack;
};

//----------------------------------------------------------------------

class ImageListener : public nsIStreamListener {
public:
  ImageListener(nsImageDocument* aDoc);
  virtual ~ImageListener();

  NS_DECL_ISUPPORTS
#ifdef NECKO
  // nsIStreamObserver methods:
  NS_DECL_NSISTREAMOBSERVER

  // nsIStreamListener methods:
  NS_DECL_NSISTREAMLISTENER
#else
  NS_IMETHOD OnStartRequest(nsIURI* aURL, const char *aContentType);
  NS_IMETHOD OnProgress(nsIURI* aURL, PRUint32 aProgress, PRUint32 aProgressMax);
  NS_IMETHOD OnStatus(nsIURI* aURL, const PRUnichar* aMsg);
  NS_IMETHOD OnStopRequest(nsIURI* aURL, nsresult aStatus,
                           const PRUnichar* aMsg);
  NS_IMETHOD GetBindInfo(nsIURI* aURL, nsStreamBindingInfo* aInfo);
  NS_IMETHOD OnDataAvailable(nsIURI* aURL, nsIInputStream* aStream,
                             PRUint32 aCount);
#endif

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

NS_IMPL_ISUPPORTS(ImageListener, kIStreamListenerIID)

NS_IMETHODIMP
#ifdef NECKO
ImageListener::OnStartRequest(nsIChannel* channel, nsISupports *ctxt)
#else
ImageListener::OnStartRequest(nsIURI* aURL, const char *aContentType)
#endif
{
#ifdef NECKO
  nsresult rv;
  nsIURI* uri;
  rv = channel->GetURI(&uri);
  if (NS_FAILED(rv)) return rv;
  
  mDocument->StartImageLoad(uri, mNextStream);
  NS_RELEASE(uri);
  NS_RELEASE(channel);
#else
  mDocument->StartImageLoad(aURL, mNextStream);
#endif
  if (nsnull == mNextStream) {
    return NS_ERROR_FAILURE;
  }
#ifdef NECKO
  return mNextStream->OnStartRequest(channel, ctxt);
#else
  return mNextStream->OnStartRequest(aURL, aContentType);
#endif
}

#ifndef NECKO
NS_IMETHODIMP
ImageListener::OnProgress(nsIURI* aURL, PRUint32 aProgress,
                          PRUint32 aProgressMax)
{
  if (nsnull == mNextStream) {
    return NS_ERROR_FAILURE;
  }
  return mNextStream->OnProgress(aURL, aProgress, aProgressMax);
}

NS_IMETHODIMP
ImageListener::OnStatus(nsIURI* aURL, const PRUnichar* aMsg)
{
  if (nsnull == mNextStream) {
    return NS_ERROR_FAILURE;
  }
  return mNextStream->OnStatus(aURL, aMsg);
}
#endif

NS_IMETHODIMP
#ifdef NECKO
ImageListener::OnStopRequest(nsIChannel* channel, nsISupports *ctxt,
                             nsresult status, const PRUnichar *errorMsg)
#else
ImageListener::OnStopRequest(nsIURI* aURL, nsresult aStatus,
                             const PRUnichar* aMsg)
#endif
{
  if (nsnull == mNextStream) {
    return NS_ERROR_FAILURE;
  }
#ifdef NECKO
  mDocument->EndLayout(ctxt, status, errorMsg);
  return mNextStream->OnStopRequest(channel, ctxt, status, errorMsg);
#else
  mDocument->EndLayout(aURL, aStatus, aMsg);
  return mNextStream->OnStopRequest(aURL, aStatus, aMsg);
#endif
}

#ifndef NECKO
NS_IMETHODIMP
ImageListener::GetBindInfo(nsIURI* aURL, nsStreamBindingInfo* aInfo)
{
  if (nsnull == mNextStream) {
    return NS_ERROR_FAILURE;
  }
  return mNextStream->GetBindInfo(aURL, aInfo);
}
#endif

NS_IMETHODIMP
#ifdef NECKO
ImageListener::OnDataAvailable(nsIChannel* channel, nsISupports *ctxt,
                               nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
#else
ImageListener::OnDataAvailable(nsIURI* aURL, nsIInputStream* aStream,
                               PRUint32 aCount)
#endif
{
  if (nsnull == mNextStream) {
    return NS_ERROR_FAILURE;
  }
#ifdef NECKO
  return mNextStream->OnDataAvailable(channel, ctxt, inStr, sourceOffset, count);
#else
  return mNextStream->OnDataAvailable(aURL, aStream, aCount);
#endif
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
#ifdef NECKO
                                   nsIChannel* aChannel,
                                   nsILoadGroup* aLoadGroup,
#else
                                   nsIURI *aURL, 
#endif
                                   nsIContentViewerContainer* aContainer,
                                   nsIStreamListener **aDocListener)
{
  nsresult rv = nsDocument::StartDocumentLoad(aCommand,
#ifdef NECKO
                                              aChannel, aLoadGroup,
#else
                                              aURL, 
#endif
                                              aContainer, 
                                              aDocListener);
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
#ifdef NECKO
        char* spec;
#else
        const char* spec;
#endif
        (void)aURL->GetSpec(&spec);
        nsIStreamListener* listener = nsnull;
        rv = group->GetImageFromStream(spec, nsnull, nsnull,
                                       0, 0, 0,
                                       mImageRequest, listener);
#ifdef NECKO
        nsCRT::free(spec);
#endif
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
  // XXX Set title to "GIF image widthxheight pixels - Netscape"

  // Wire up an image load request to the document
//  mImageRequest = aGroup->GetImage(cp, this, aBackgroundColor, 0, 0, 0);

  // Synthesize an html document that refers to the image
  nsresult rv;
  nsIHTMLContent* root;
  rv = NS_NewHTMLHtmlElement(&root, nsHTMLAtoms::html);
  if (NS_OK != rv) {
    return rv;
  }
  root->SetDocument(this, PR_FALSE);
  SetRootContent(root);

  nsIHTMLContent* body;
  rv = NS_NewHTMLBodyElement(&body, nsHTMLAtoms::body);
  if (NS_OK != rv) {
    return rv;
  }
  body->SetDocument(this, PR_FALSE);

  nsIHTMLContent* center;
  nsIAtom* centerAtom = NS_NewAtom("p");
  rv = NS_NewHTMLParagraphElement(&center, centerAtom);
  NS_RELEASE(centerAtom);
  if (NS_OK != rv) {
    return rv;
  }
  center->SetDocument(this, PR_FALSE);

  nsIHTMLContent* image;
  nsIAtom* imgAtom = NS_NewAtom("img");
  rv = NS_NewHTMLImageElement(&image, imgAtom);
  NS_RELEASE(imgAtom);
  if (NS_OK != rv) {
    return rv;
  }
  image->SetDocument(this, PR_FALSE);

#ifdef NECKO
  char* src;
  mDocumentURL->GetSpec(&src);
#else
  PRUnichar* src;
  mDocumentURL->ToString(&src);
#endif
  nsHTMLValue val(src);
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
        vm->EnableRefresh();
      }

      NS_RELEASE(shell);
    }
  }
}

nsresult
#ifdef NECKO
nsImageDocument::EndLayout(nsISupports *ctxt, 
                           nsresult status, 
                           const PRUnichar *errorMsg)
#else
nsImageDocument::EndLayout(nsIURI* aURL, 
                           nsresult aStatus, 
                           const PRUnichar* aMsg)
#endif
{
  nsString titleStr = "Image ";
  if (mImageRequest) {
    PRUint32 width, height;
    mImageRequest->GetNaturalDimensions(&width, &height);
    titleStr.Append((PRInt32)width);
    titleStr.Append("x");
    titleStr.Append((PRInt32)height);
    titleStr.Append(" pixels");
  } 
  SetTitle(titleStr);
  return NS_OK;
}
