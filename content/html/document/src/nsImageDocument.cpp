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

#include "nsCOMPtr.h"
#include "nsHTMLDocument.h"
#include "nsHTMLParts.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLContent.h"
#ifdef USE_IMG2
#include "imgIRequest.h"
#include "imgILoader.h"
#include "imgIContainer.h"
#else
#include "nsIImageGroup.h"
#include "nsIImageRequest.h"
#endif
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
                               PRBool aReset = PR_TRUE);

  nsresult CreateSyntheticDocument();

#ifndef USE_IMG2
  nsresult StartImageLoad(nsIURI* aURL, nsIStreamListener*& aListener);
#endif
  nsresult EndLayout(nsISupports *ctxt, 
                     nsresult status);
  nsresult UpdateTitle( void );

  void StartLayout();
#ifdef USE_IMG2
  nsCOMPtr<imgIRequest> mImageRequest;
#else
  nsIImageRequest*  mImageRequest;
#endif
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
#ifdef USE_IMG2
  nsCOMPtr<nsIStreamListener> mNextStream;
#else
  nsIStreamListener *mNextStream;
#endif
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
#ifndef USE_IMG2
  NS_IF_RELEASE(mNextStream);
#endif
}

NS_IMPL_THREADSAFE_ISUPPORTS1(ImageListener, nsIStreamListener)

NS_IMETHODIMP
ImageListener::OnStartRequest(nsIRequest* request, nsISupports *ctxt)
{
  nsresult rv;
  nsIURI* uri;

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  if (!channel) return NS_ERROR_NULL_POINTER;

  rv = channel->GetURI(&uri);
  if (NS_FAILED(rv)) return rv;
  
#ifdef USE_IMG2
  nsCOMPtr<nsIStreamListener> kungFuDeathGrip(this);
  nsCOMPtr<imgILoader> il(do_GetService("@mozilla.org/image/loader;1"));
  il->LoadImageWithChannel(channel, nsnull, nsnull, getter_AddRefs(mNextStream), 
                           getter_AddRefs(mDocument->mImageRequest));

  // XXX
  // if we get a cache hit, and we cancel the channel in the above function,
  // do we call OnStopRequest before we call StartLayout?
  // if so, should LoadImageWithChannel() not call channel->Cancel() ?

  mDocument->StartLayout();

#else
  mDocument->StartImageLoad(uri, mNextStream);
#endif
  NS_RELEASE(uri);
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

NS_LAYOUT nsresult
NS_NewImageDocument(nsIDocument** aInstancePtrResult)
{
  nsImageDocument* doc = new nsImageDocument();
  if(doc)
    return doc->QueryInterface(NS_GET_IID(nsIDocument), (void**) aInstancePtrResult);
  return NS_ERROR_OUT_OF_MEMORY;
}

nsImageDocument::nsImageDocument()
{
#ifndef USE_IMG2
  mImageRequest = nsnull;
#endif
  mBlack = NS_RGB(0, 0, 0);
}

nsImageDocument::~nsImageDocument()
{
#ifndef USE_IMG2
  NS_IF_RELEASE(mImageRequest);
#endif
}

NS_IMETHODIMP
nsImageDocument::StartDocumentLoad(const char* aCommand,
                                   nsIChannel* aChannel,
                                   nsILoadGroup* aLoadGroup,
                                   nsISupports* aContainer,
                                   nsIStreamListener **aDocListener,
                                   PRBool aReset)
{
  NS_ASSERTION(aDocListener, "null aDocListener");
  NS_ENSURE_ARG_POINTER(aContainer);
  mContainer = dont_AddRef(NS_GetWeakReference(aContainer));

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
  if (!*aDocListener)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aDocListener);

  return NS_OK;
}

#ifndef USE_IMG2
nsresult
nsImageDocument::StartImageLoad(nsIURI* aURL, nsIStreamListener*& aListener)
{
  nsresult rv = NS_OK;
  aListener = nsnull;

  // Tell image group to load the stream now. This will get the image
  // hooked up to the open stream and return the underlying listener
  // so that we can pass it back upwards.


  nsCOMPtr<nsIPresShell> shell;
  GetShellAt(0, getter_AddRefs(shell));
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
  }
  
  // Finally, start the layout going
  StartLayout();

  return NS_OK;
}
#endif

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

  nsXPIDLCString src;
  mDocumentURL->GetSpec(getter_Copies(src));

  nsString src_string; src_string.AssignWithConversion(src);
  nsHTMLValue val(src_string);

  image->SetHTMLAttribute(nsHTMLAtoms::src, val, PR_FALSE);
  image->SetHTMLAttribute(nsHTMLAtoms::alt, val, PR_FALSE);

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
  nsIURL *pURL=nsnull;
  if(NS_SUCCEEDED(mDocumentURL->QueryInterface(NS_GET_IID(nsIURL),(void **)&pURL))){
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
 
  nsCOMPtr<nsIStringBundle> bundle;
  nsresult rv; 
  // Create a bundle for the localization
  nsCOMPtr<nsIStringBundleService> stringService = 
           do_GetService(kStringBundleServiceCID, &rv);
  if (NS_SUCCEEDED(rv) && stringService) {
    rv = stringService->CreateBundle(NSIMAGEDOCUMENT_PROPERTIES_URI, getter_AddRefs(bundle));
  }
  if (NS_SUCCEEDED(rv) && bundle) {
    nsAutoString key;
    nsXPIDLString valUni;
    if (mImageRequest) {
      PRUint32 width, height;
#ifdef USE_IMG2
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
#else
      mImageRequest->GetNaturalImageSize(&width, &height);
#endif
      // if we got a valid size (sometimes we do not) then display it
      if (width != 0 && height != 0){
        key.AssignWithConversion("ImageTitleWithDimensions");
        nsAutoString widthStr; widthStr.AppendInt(width);
        nsAutoString heightStr; heightStr.AppendInt(height);
        const PRUnichar *formatStrings[2]  = {widthStr.get(), heightStr.get()};
        rv = bundle->FormatStringFromName(key.get(), formatStrings, 2, getter_Copies(valUni));
      }
    }
    if (!valUni || !valUni[0]) {
      key.AssignWithConversion("ImageTitleWithoutDimensions");
      rv = bundle->GetStringFromName(key.get(), getter_Copies(valUni));
    }
    if (NS_SUCCEEDED(rv) && valUni) {
      // set it on the document
      SetTitle(nsDependentString(valUni));
    }
  }
  return NS_OK;
}
