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
 *   Jan Varga <varga@ku.sk>
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
#include "nsIDOMMouseEvent.h"
#include "nsIDOMEventListener.h"
#include "nsGkAtoms.h"
#include "imgIRequest.h"
#include "imgILoader.h"
#include "imgIContainer.h"
#include "nsStubImageDecoderObserver.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIScrollableView.h"
#include "nsStyleContext.h"
#include "nsAutoPtr.h"
#include "nsMediaDocument.h"
#include "nsStyleSet.h"
#include "nsIChannel.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMElement.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsContentErrors.h"
#include "ImageErrors.h"

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
                        public nsStubImageDecoderObserver,
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
  virtual void Destroy();

  NS_DECL_NSIIMAGEDOCUMENT

  // imgIDecoderObserver (override nsStubImageDecoderObserver)
  NS_IMETHOD OnStartContainer(imgIRequest* aRequest, imgIContainer* aImage);

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

  friend class ImageListener;
protected:
  nsresult CreateSyntheticDocument();

  nsresult CheckOverflowing(PRBool changeState);

  void UpdateTitleAndCharset();

  float GetRatio() {
    return PR_MIN((float)mVisibleWidth / mImageWidth,
                  (float)mVisibleHeight / mImageHeight);
  }

  nsCOMPtr<nsIContent>          mImageContent;

  PRInt32                       mVisibleWidth;
  PRInt32                       mVisibleHeight;
  PRInt32                       mImageWidth;
  PRInt32                       mImageHeight;

  PRPackedBool                  mResizeImageByDefault;
  PRPackedBool                  mImageIsOverflowing;
  // mImageIsResized is true if the image is currently resized
  PRPackedBool                  mImageIsResized;
  // mShouldResize is true if the image should be resized when it doesn't fit
  // mImageIsResized cannot be true when this is false, but mImageIsResized
  // can be false when this is true
  PRPackedBool                  mShouldResize;
  PRPackedBool                  mFirstResize;
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
                                             &decision,
                                             nsContentUtils::GetContentPolicy());
                                               
  if (NS_FAILED(rv) || NS_CP_REJECTED(decision)) {
    request->Cancel(NS_ERROR_CONTENT_BLOCKED);
    return NS_OK;
  }

  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(imgDoc->mImageContent);
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
  
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(imgDoc->mImageContent);
  if (imageLoader) {
    imageLoader->RemoveObserver(imgDoc);
  }

  // |status| is NS_IMAGELIB_ERROR_LOAD_ABORTED if the image was found in
  // the cache (bug 177747 comment 51).
  if (status == NS_IMAGELIB_ERROR_LOAD_ABORTED) {
    status = NS_OK;
  }

  // mImageContent can be null if the document is already destroyed
  if (NS_FAILED(status) && imgDoc->mStringBundle && imgDoc->mImageContent) {
    nsCAutoString src;
    imgDoc->mDocumentURI->GetSpec(src);
    NS_ConvertUTF8toUTF16 srcString(src);
    const PRUnichar* formatString[] = { srcString.get() };
    nsXPIDLString errorMsg;
    NS_NAMED_LITERAL_STRING(str, "InvalidImage");
    imgDoc->mStringBundle->FormatStringFromName(str.get(), formatString, 1,
                                                getter_Copies(errorMsg));
    
    imgDoc->mImageContent->SetAttr(kNameSpaceID_None, nsGkAtoms::alt, errorMsg, PR_FALSE);
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

  mResizeImageByDefault =
    nsContentUtils::GetBoolPref(AUTOMATIC_IMAGE_RESIZING_PREF);
  mShouldResize = mResizeImageByDefault;
  mFirstResize = PR_TRUE;

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
nsImageDocument::Destroy()
{
  if (mImageContent) {
    // Remove our event listener from the image content.
    nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(mImageContent);
    target->RemoveEventListener(NS_LITERAL_STRING("click"), this, PR_FALSE);

    // Break reference cycle with mImageContent, if we have one
    nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mImageContent);
    if (imageLoader) {
      imageLoader->RemoveObserver(this);
    }

    mImageContent = nsnull;
  }

  nsMediaDocument::Destroy();
}

void
nsImageDocument::SetScriptGlobalObject(nsIScriptGlobalObject* aScriptGlobalObject)
{
  // If the script global object is changing, we need to unhook our event
  // listeners on the window.
  nsCOMPtr<nsIDOMEventTarget> target;
  if (mScriptGlobalObject &&
      aScriptGlobalObject != mScriptGlobalObject) {
    target = do_QueryInterface(mScriptGlobalObject);
    target->RemoveEventListener(NS_LITERAL_STRING("resize"), this, PR_FALSE);
    target->RemoveEventListener(NS_LITERAL_STRING("keypress"), this,
                                PR_FALSE);
  }

  // Set the script global object on the superclass before doing
  // anything that might require it....
  nsHTMLDocument::SetScriptGlobalObject(aScriptGlobalObject);

  if (aScriptGlobalObject) {
    if (!mRootContent) {
      // Create synthetic document
      nsresult rv = CreateSyntheticDocument();
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to create synthetic document");

      target = do_QueryInterface(mImageContent);
      target->AddEventListener(NS_LITERAL_STRING("click"), this, PR_FALSE);
    }

    target = do_QueryInterface(aScriptGlobalObject);
    target->AddEventListener(NS_LITERAL_STRING("resize"), this, PR_FALSE);
    target->AddEventListener(NS_LITERAL_STRING("keypress"), this, PR_FALSE);
  }
}


NS_IMETHODIMP
nsImageDocument::GetImageResizingEnabled(PRBool* aImageResizingEnabled)
{
  *aImageResizingEnabled = PR_TRUE;
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
nsImageDocument::GetImageRequest(imgIRequest** aImageRequest)
{
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mImageContent);
  if (imageLoader) {
    return imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                                   aImageRequest);
  }

  *aImageRequest = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::ShrinkToFit()
{
  nsCOMPtr<nsIDOMHTMLImageElement> image = do_QueryInterface(mImageContent);
  image->SetWidth(PR_MAX(1, NSToCoordFloor(GetRatio() * mImageWidth)));
  image->SetHeight(PR_MAX(1, NSToCoordFloor(GetRatio() * mImageHeight)));
  
  mImageContent->SetAttr(kNameSpaceID_None, nsGkAtoms::style,
                         NS_LITERAL_STRING("cursor: -moz-zoom-in"), PR_TRUE);
  
  mImageIsResized = PR_TRUE;
  
  UpdateTitleAndCharset();

  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::RestoreImageTo(PRInt32 aX, PRInt32 aY)
{
  float ratio = GetRatio();

  RestoreImage();
  FlushPendingNotifications(Flush_Layout);

  nsIPresShell *shell = GetShellAt(0);
  if (!shell)
    return NS_OK;

  nsPresContext* context = shell->GetPresContext();
  if (!context)
    return NS_OK;

  nsIViewManager* vm = context->GetViewManager();
  if (!vm)
    return NS_OK;

  nsIScrollableView* view;
  vm->GetRootScrollableView(&view);
  if (!view)
    return NS_OK;

  nsSize scrolledSize;
  if (NS_FAILED(view->GetContainerSize(&scrolledSize.width, &scrolledSize.height)))
    return NS_OK;

  nsRect portRect = view->View()->GetBounds();
  view->ScrollTo(nsPresContext::CSSPixelsToAppUnits(aX/ratio) - portRect.width/2,
                 nsPresContext::CSSPixelsToAppUnits(aY/ratio) - portRect.height/2,
                 NS_VMREFRESH_IMMEDIATE);
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::RestoreImage()
{
  mImageContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::width, PR_TRUE);
  mImageContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::height, PR_TRUE);
  
  if (mImageIsOverflowing) {
    mImageContent->SetAttr(kNameSpaceID_None, nsGkAtoms::style,
                           NS_LITERAL_STRING("cursor: -moz-zoom-out"), PR_TRUE);
  }
  else {
    mImageContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::style, PR_TRUE);
  }
  
  mImageIsResized = PR_FALSE;
  
  UpdateTitleAndCharset();

  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::ToggleImageSize()
{
  mShouldResize = PR_TRUE;
  if (mImageIsResized) {
    mShouldResize = PR_FALSE;
    RestoreImage();
  }
  else if (mImageIsOverflowing) {
    ShrinkToFit();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::OnStartContainer(imgIRequest* aRequest, imgIContainer* aImage)
{
  aImage->GetWidth(&mImageWidth);
  aImage->GetHeight(&mImageHeight);
  CheckOverflowing(mResizeImageByDefault);
  UpdateTitleAndCharset();

  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("resize")) {
    CheckOverflowing(PR_FALSE);
  }
  else if (eventType.EqualsLiteral("click")) {
    mShouldResize = PR_TRUE;
    if (mImageIsResized) {
      PRInt32 x = 0, y = 0;
      nsCOMPtr<nsIDOMMouseEvent> event(do_QueryInterface(aEvent));
      if (event) {
        event->GetClientX(&x);
        event->GetClientY(&y);
        PRInt32 left = 0, top = 0;
        nsCOMPtr<nsIDOMNSHTMLElement> nsElement(do_QueryInterface(mImageContent));
        nsElement->GetOffsetLeft(&left);
        nsElement->GetOffsetTop(&top);
        x -= left;
        y -= top;
      }
      mShouldResize = PR_FALSE;
      RestoreImageTo(x, y);
    }
    else if (mImageIsOverflowing) {
      ShrinkToFit();
    }
  }
  else if (eventType.EqualsLiteral("keypress")) {
    nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aEvent);
    PRUint32 charCode;
    keyEvent->GetCharCode(&charCode);
    // plus key
    if (charCode == 0x2B) {
      mShouldResize = PR_FALSE;
      if (mImageIsResized) {
        RestoreImage();
      }
    }
    // minus key
    else if (charCode == 0x2D) {
      mShouldResize = PR_TRUE;
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

  nsCOMPtr<nsIContent> body = do_QueryInterface(mBodyContent);
  if (!body) {
    NS_WARNING("no body on image document!");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsINodeInfo> nodeInfo;
  rv = mNodeInfoManager->GetNodeInfo(nsGkAtoms::img, nsnull,
                                     kNameSpaceID_None,
                                     getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  mImageContent = NS_NewHTMLImageElement(nodeInfo);
  if (!mImageContent) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mImageContent);
  NS_ENSURE_TRUE(imageLoader, NS_ERROR_UNEXPECTED);

  nsCAutoString src;
  mDocumentURI->GetSpec(src);

  NS_ConvertUTF8toUTF16 srcString(src);
  // Make sure not to start the image load from here...
  imageLoader->SetLoadingEnabled(PR_FALSE);
  mImageContent->SetAttr(kNameSpaceID_None, nsGkAtoms::src, srcString, PR_FALSE);
  mImageContent->SetAttr(kNameSpaceID_None, nsGkAtoms::alt, srcString, PR_FALSE);

  body->AppendChildTo(mImageContent, PR_FALSE);
  imageLoader->SetLoadingEnabled(PR_TRUE);

  return NS_OK;
}

nsresult
nsImageDocument::CheckOverflowing(PRBool changeState)
{
  nsIPresShell *shell = GetShellAt(0);
  if (!shell) {
    return NS_OK;
  }

  nsPresContext *context = shell->GetPresContext();
  nsRect visibleArea = context->GetVisibleArea();

  nsCOMPtr<nsIContent> content = do_QueryInterface(mBodyContent);
  if (!content) {
    NS_WARNING("no body on image document!");
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<nsStyleContext> styleContext =
    context->StyleSet()->ResolveStyleFor(content, nsnull);

  nsMargin m;
  if (styleContext->GetStyleMargin()->GetMargin(m))
    visibleArea.Deflate(m);
  m = styleContext->GetStyleBorder()->GetBorder();
  visibleArea.Deflate(m);
  if (styleContext->GetStylePadding()->GetPadding(m))
    visibleArea.Deflate(m);

  mVisibleWidth = nsPresContext::AppUnitsToIntCSSPixels(visibleArea.width);
  mVisibleHeight = nsPresContext::AppUnitsToIntCSSPixels(visibleArea.height);

  PRBool imageWasOverflowing = mImageIsOverflowing;
  mImageIsOverflowing =
    mImageWidth > mVisibleWidth || mImageHeight > mVisibleHeight;
  PRBool windowBecameBigEnough = imageWasOverflowing && !mImageIsOverflowing;

  if (changeState || mShouldResize || mFirstResize ||
      windowBecameBigEnough) {
    if (mImageIsOverflowing && (changeState || mShouldResize)) {
      ShrinkToFit();
    }
    else if (mImageIsResized || mFirstResize || windowBecameBigEnough) {
      RestoreImage();
    }
  }
  mFirstResize = PR_FALSE;

  return NS_OK;
}

void 
nsImageDocument::UpdateTitleAndCharset()
{
  nsCAutoString typeStr;
  nsCOMPtr<imgIRequest> imageRequest;
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mImageContent);
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
    "ImageTitleWithDimensions",
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

  NS_ADDREF(doc);
  nsresult rv = doc->Init();

  if (NS_FAILED(rv)) {
    NS_RELEASE(doc);
  }

  *aResult = doc;

  return rv;
}
