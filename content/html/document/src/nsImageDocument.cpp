/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Morten Nilsen <morten@nilsen.com>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Jan Varga <varga@nixcorp.com>
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

#include "nsRect.h"
#include "nsHTMLDocument.h"
#include "nsIImageDocument.h"
#include "nsIImageLoadingContent.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMEventListener.h"
#include "nsHTMLAtoms.h"
#include "imgIRequest.h"
#include "imgILoader.h"
#include "imgIContainer.h"
#include "imgIDecoderObserver.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsAutoPtr.h"
#include "nsMediaDocument.h"
#include "nsStyleSet.h"
#include "nsIChannel.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMElement.h"

#define AUTOMATIC_IMAGE_RESIZING_PREF "browser.enable_automatic_image_resizing"

class nsImageDocument;

class ImageListener: public nsMediaDocumentStreamListener
{
public:
  ImageListener(nsImageDocument* aDocument);
  virtual ~ImageListener();

  NS_DECL_ISUPPORTS

  NS_DECL_NSIREQUESTOBSERVER
};

class nsImageDocument : public nsMediaDocument,
                        public nsIImageDocument,
                        public imgIDecoderObserver,
                        public nsIDOMEventListener
{
public:
  nsImageDocument();
  virtual ~nsImageDocument();

  NS_DECL_ISUPPORTS

  virtual nsresult Init();

  virtual nsresult StartDocumentLoad(const char*         aCommand,
                                     nsIChannel*         aChannel,
                                     nsILoadGroup*       aLoadGroup,
                                     nsISupports*        aContainer,
                                     nsIStreamListener** aDocListener,
                                     PRBool              aReset = PR_TRUE,
                                     nsIContentSink*     aSink = nsnull);

  virtual void SetScriptGlobalObject(nsIScriptGlobalObject* aScriptGlobalObject);

  NS_DECL_NSIIMAGEDOCUMENT

  NS_DECL_IMGIDECODEROBSERVER

  NS_DECL_IMGICONTAINEROBSERVER

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

  friend class ImageListener;
protected:
  nsresult CreateSyntheticDocument();

  nsresult CheckOverflowing();

  void UpdateTitleAndCharset();

  float GetRatio() {
    return PR_MIN((float)mVisibleWidth / mImageWidth,
                  (float)mVisibleHeight / mImageHeight);
  }

  nsCOMPtr<nsIDOMElement>       mImageElement;

  PRInt32                       mVisibleWidth;
  PRInt32                       mVisibleHeight;
  PRInt32                       mImageWidth;
  PRInt32                       mImageHeight;

  PRPackedBool                  mImageResizingEnabled;
  PRPackedBool                  mImageIsOverflowing;
  PRPackedBool                  mImageIsResized;
};

NS_IMPL_ADDREF_INHERITED(ImageListener, nsMediaDocumentStreamListener)
NS_IMPL_RELEASE_INHERITED(ImageListener, nsMediaDocumentStreamListener)

NS_INTERFACE_MAP_BEGIN(ImageListener)
NS_INTERFACE_MAP_END_INHERITING(nsMediaDocumentStreamListener)

ImageListener::ImageListener(nsImageDocument* aDocument)
  : nsMediaDocumentStreamListener(aDocument)
{
}


ImageListener::~ImageListener()
{
}

NS_IMETHODIMP
ImageListener::OnStartRequest(nsIRequest* request, nsISupports *ctxt)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);

  nsImageDocument *imgDoc = (nsImageDocument*)mDocument.get();
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  if (!channel) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsPIDOMWindow> domWindow =
    do_QueryInterface(imgDoc->GetScriptGlobalObject());
  NS_ENSURE_TRUE(domWindow, NS_ERROR_UNEXPECTED);

  // Do a ShouldProcess check to see whether to keep loading the image.
  nsCOMPtr<nsIURI> channelURI;
  channel->GetURI(getter_AddRefs(channelURI));

  nsCAutoString mimeType;
  channel->GetContentType(mimeType);
    
  PRInt16 decision = nsIContentPolicy::ACCEPT;
  nsresult rv = NS_CheckContentProcessPolicy(nsIContentPolicy::TYPE_IMAGE,
                                             channelURI,
                                             nsnull,
                                             domWindow->GetFrameElementInternal(),
                                             mimeType,
                                             nsnull,
                                             &decision);
                                               
  if (NS_FAILED(rv) || NS_CP_REJECTED(decision)) {
    request->Cancel(NS_ERROR_CONTENT_BLOCKED);
    return NS_OK;
  }

  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(imgDoc->mImageElement);
  NS_ENSURE_TRUE(imageLoader, NS_ERROR_UNEXPECTED);

  imageLoader->AddObserver(imgDoc);
  imageLoader->LoadImageWithChannel(channel, getter_AddRefs(mNextStream));

  return nsMediaDocumentStreamListener::OnStartRequest(request, ctxt);
}

NS_IMETHODIMP
ImageListener::OnStopRequest(nsIRequest* request, nsISupports *ctxt,
                             nsresult status)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);
  nsImageDocument *imgDoc = (nsImageDocument*)mDocument.get();
  imgDoc->UpdateTitleAndCharset();
  
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(imgDoc->mImageElement);
  if (imageLoader) {
    imageLoader->RemoveObserver(imgDoc);
  }

  return nsMediaDocumentStreamListener::OnStopRequest(request, ctxt, status);
}


  // NOTE! nsDocument::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

nsImageDocument::nsImageDocument()
{

  // NOTE! nsDocument::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

}

nsImageDocument::~nsImageDocument()
{
}

NS_IMPL_ADDREF_INHERITED(nsImageDocument, nsMediaDocument)
NS_IMPL_RELEASE_INHERITED(nsImageDocument, nsMediaDocument)

NS_INTERFACE_MAP_BEGIN(nsImageDocument)
  NS_INTERFACE_MAP_ENTRY(nsIImageDocument)
  NS_INTERFACE_MAP_ENTRY(imgIDecoderObserver)
  NS_INTERFACE_MAP_ENTRY(imgIContainerObserver)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(ImageDocument)
NS_INTERFACE_MAP_END_INHERITING(nsMediaDocument)


nsresult
nsImageDocument::Init()
{
  nsresult rv = nsMediaDocument::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  mImageResizingEnabled =
    nsContentUtils::GetBoolPref(AUTOMATIC_IMAGE_RESIZING_PREF);

  return NS_OK;
}

nsresult
nsImageDocument::StartDocumentLoad(const char*         aCommand,
                                   nsIChannel*         aChannel,
                                   nsILoadGroup*       aLoadGroup,
                                   nsISupports*        aContainer,
                                   nsIStreamListener** aDocListener,
                                   PRBool              aReset,
                                   nsIContentSink*     aSink)
{
  nsresult rv =
    nsMediaDocument::StartDocumentLoad(aCommand, aChannel, aLoadGroup,
                                       aContainer, aDocListener, aReset,
                                       aSink);
  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_ASSERTION(aDocListener, "null aDocListener");
  *aDocListener = new ImageListener(this);
  if (!*aDocListener)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aDocListener);

  return NS_OK;
}

void
nsImageDocument::SetScriptGlobalObject(nsIScriptGlobalObject* aScriptGlobalObject)
{
  if (!aScriptGlobalObject) {
    if (mImageResizingEnabled) {
      nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(mImageElement);
      target->RemoveEventListener(NS_LITERAL_STRING("click"), this, PR_FALSE);

      target = do_QueryInterface(mScriptGlobalObject);
      target->RemoveEventListener(NS_LITERAL_STRING("resize"), this, PR_FALSE);
      target->RemoveEventListener(NS_LITERAL_STRING("keypress"), this,
                                  PR_FALSE);
    }

    // Break reference cycle with mImageElement, if we have one
    nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mImageElement);
    if (imageLoader) {
      imageLoader->RemoveObserver(this);
    }
    
    mImageElement = nsnull;
  }

  // Set the script global object on the superclass before doing
  // anything that might require it....
  nsHTMLDocument::SetScriptGlobalObject(aScriptGlobalObject);

  if (aScriptGlobalObject) {
    // Create synthetic document
    nsresult rv = CreateSyntheticDocument();
    if (NS_FAILED(rv)) {
      return;
    }

    if (mImageResizingEnabled) {
      nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(mImageElement);
      target->AddEventListener(NS_LITERAL_STRING("click"), this, PR_FALSE);

      target = do_QueryInterface(aScriptGlobalObject);
      target->AddEventListener(NS_LITERAL_STRING("resize"), this, PR_FALSE);
      target->AddEventListener(NS_LITERAL_STRING("keypress"), this, PR_FALSE);
    }
  }
}


NS_IMETHODIMP
nsImageDocument::GetImageResizingEnabled(PRBool* aImageResizingEnabled)
{
  *aImageResizingEnabled = mImageResizingEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::GetImageIsOverflowing(PRBool* aImageIsOverflowing)
{
  *aImageIsOverflowing = mImageIsOverflowing;
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::GetImageIsResized(PRBool* aImageIsResized)
{
  *aImageIsResized = mImageIsResized;
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::ShrinkToFit()
{
  if (mImageResizingEnabled) {
    nsCOMPtr<nsIDOMHTMLImageElement> image = do_QueryInterface(mImageElement);
    image->SetWidth(NSToCoordFloor(GetRatio() * mImageWidth));

    mImageElement->SetAttribute(NS_LITERAL_STRING("style"),
                                NS_LITERAL_STRING("cursor: -moz-zoom-in"));

    mImageIsResized = PR_TRUE;

    UpdateTitleAndCharset();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::RestoreImage()
{
  if (mImageResizingEnabled) {
    mImageElement->RemoveAttribute(NS_LITERAL_STRING("width"));

    if (mImageIsOverflowing) {
      mImageElement->SetAttribute(NS_LITERAL_STRING("style"),
                                  NS_LITERAL_STRING("cursor: -moz-zoom-out"));
    }
    else {
      mImageElement->RemoveAttribute(NS_LITERAL_STRING("style"));
    }

    mImageIsResized = PR_FALSE;

    UpdateTitleAndCharset();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::ToggleImageSize()
{
  if (mImageResizingEnabled) {
    if (mImageIsResized) {
      RestoreImage();
    }
    else if (mImageIsOverflowing) {
      ShrinkToFit();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::OnStartDecode(imgIRequest* aRequest)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::OnStartContainer(imgIRequest* aRequest, imgIContainer* aImage)
{
  aImage->GetWidth(&mImageWidth);
  aImage->GetHeight(&mImageHeight);
  if (mImageResizingEnabled) {
    CheckOverflowing();
  }
  UpdateTitleAndCharset();

  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::OnStartFrame(imgIRequest* aRequest, gfxIImageFrame* aFrame)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::OnDataAvailable(imgIRequest* aRequest,
                                 gfxIImageFrame* aFrame,
                                 const nsRect* aRect)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::OnStopFrame(imgIRequest* aRequest,
                             gfxIImageFrame* aFrame)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::OnStopContainer(imgIRequest* aRequest,
                                 imgIContainer* aImage)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::OnStopDecode(imgIRequest* aRequest,
                              nsresult status,
                              const PRUnichar* statusArg)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::FrameChanged(imgIContainer* aContainer,
                              gfxIImageFrame* aFrame,
                              nsRect* aDirtyRect)
{
  return NS_OK;
}


NS_IMETHODIMP
nsImageDocument::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("resize")) {
    CheckOverflowing();
  }
  else if (eventType.EqualsLiteral("click")) {
    ToggleImageSize();
  }
  else if (eventType.EqualsLiteral("keypress")) {
    nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aEvent);
    PRUint32 charCode;
    keyEvent->GetCharCode(&charCode);
    // plus key
    if (charCode == 0x2B) {
      if (mImageIsResized) {
        RestoreImage();
      }
    }
    // minus key
    else if (charCode == 0x2D) {
      if (mImageIsOverflowing) {
        ShrinkToFit();
      }
    }
  }

  return NS_OK;
}

nsresult
nsImageDocument::CreateSyntheticDocument()
{
  // Synthesize an html document that refers to the image
  nsresult rv = nsMediaDocument::CreateSyntheticDocument();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHTMLContent> body = do_QueryInterface(mBodyContent);
  if (!body) {
    NS_WARNING("no body on image document!");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsINodeInfo> nodeInfo;
  rv = mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::img, nsnull,
                                     kNameSpaceID_None,
                                     getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHTMLContent> image = NS_NewHTMLImageElement(nodeInfo);
  if (!image) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  image->SetDocument(this, PR_FALSE, PR_TRUE);
  mImageElement = do_QueryInterface(image);
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(image);
  NS_ENSURE_TRUE(imageLoader, NS_ERROR_UNEXPECTED);

  nsCAutoString src;
  mDocumentURI->GetSpec(src);

  NS_ConvertUTF8toUCS2 srcString(src);
  // Make sure not to start the image load from here...
  imageLoader->SetLoadingEnabled(PR_FALSE);
  image->SetAttr(kNameSpaceID_None, nsHTMLAtoms::src, srcString, PR_FALSE);

  if (mStringBundle) {
    const PRUnichar* formatString[1] = { srcString.get() };
    nsXPIDLString errorMsg;
    NS_NAMED_LITERAL_STRING(str, "InvalidImage");
    mStringBundle->FormatStringFromName(str.get(), formatString, 1,
                                        getter_Copies(errorMsg));

    image->SetAttr(kNameSpaceID_None, nsHTMLAtoms::alt, errorMsg, PR_FALSE);
  }

  body->AppendChildTo(image, PR_FALSE, PR_FALSE);
  imageLoader->SetLoadingEnabled(PR_TRUE);

  return NS_OK;
}

nsresult
nsImageDocument::CheckOverflowing()
{
  nsIPresShell *shell = GetShellAt(0);
  if (!shell) {
    return NS_OK;
  }

  nsCOMPtr<nsPresContext> context;
  shell->GetPresContext(getter_AddRefs(context));

  nsRect visibleArea = context->GetVisibleArea();

  nsCOMPtr<nsIContent> content = do_QueryInterface(mBodyContent);
  nsRefPtr<nsStyleContext> styleContext =
    context->StyleSet()->ResolveStyleFor(content, nsnull);

  const nsStyleMargin* marginData = styleContext->GetStyleMargin();
  nsMargin margin;
  marginData->GetMargin(margin);
  visibleArea.Deflate(margin);

  nsStyleBorderPadding bPad;
  styleContext->GetBorderPaddingFor(bPad);
  bPad.GetBorderPadding(margin);
  visibleArea.Deflate(margin);

  float t2p;
  t2p = context->TwipsToPixels();
  mVisibleWidth = NSTwipsToIntPixels(visibleArea.width, t2p);
  mVisibleHeight = NSTwipsToIntPixels(visibleArea.height, t2p);

  mImageIsOverflowing =
    mImageWidth > mVisibleWidth || mImageHeight > mVisibleHeight;

  if (mImageIsOverflowing) {
    ShrinkToFit();
  }
  else if (mImageIsResized) {
    RestoreImage();
  }

  return NS_OK;
}

void 
nsImageDocument::UpdateTitleAndCharset()
{
  nsCAutoString typeStr;
  nsCOMPtr<imgIRequest> imageRequest;
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mImageElement);
  if (imageLoader) {
    imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                            getter_AddRefs(imageRequest));
  }
    
  if (imageRequest) {
    nsXPIDLCString mimeType;
    imageRequest->GetMimeType(getter_Copies(mimeType));
    ToUpperCase(mimeType);
    nsXPIDLCString::const_iterator start, end;
    mimeType.BeginReading(start);
    mimeType.EndReading(end);
    nsXPIDLCString::const_iterator iter = end;
    if (FindInReadable(NS_LITERAL_CSTRING("IMAGE/"), start, iter) && 
        iter != end) {
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
      typeStr = Substring(iter, end);
    } else {
      typeStr = mimeType;
    }
  }

  nsXPIDLString status;
  if (mImageIsResized) {
    nsAutoString ratioStr;
    ratioStr.AppendInt(NSToCoordFloor(GetRatio() * 100));

    const PRUnichar* formatString[1] = { ratioStr.get() };
    mStringBundle->FormatStringFromName(NS_LITERAL_STRING("ScaledImage").get(),
                                        formatString, 1,
                                        getter_Copies(status));
  }

  static const char* const formatNames[4] = 
  {
    "ImageTitleWithNeitherDimensionsNorFile",
    "ImageTitleWithoutDimensions",
    "ImageTitleWithDimesions",
    "ImageTitleWithDimensionsAndFile",
  };

  nsMediaDocument::UpdateTitleAndCharset(typeStr, formatNames,
                                         mImageWidth, mImageHeight, status);
}


nsresult
NS_NewImageDocument(nsIDocument** aResult)
{
  nsImageDocument* doc = new nsImageDocument();
  if (!doc) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = doc->Init();

  if (NS_FAILED(rv)) {
    delete doc;
    return rv;
  }

  NS_ADDREF(*aResult = doc);

  return NS_OK;
}
