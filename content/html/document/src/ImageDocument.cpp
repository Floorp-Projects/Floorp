/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageDocument.h"
#include "mozilla/dom/ImageDocumentBinding.h"
#include "nsRect.h"
#include "nsIImageLoadingContent.h"
#include "nsGenericHTMLElement.h"
#include "nsDocShell.h"
#include "nsIDocumentInlines.h"
#include "nsDOMTokenList.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMEventListener.h"
#include "nsIFrame.h"
#include "nsGkAtoms.h"
#include "imgIRequest.h"
#include "imgILoader.h"
#include "imgIContainer.h"
#include "imgINotificationObserver.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsAutoPtr.h"
#include "nsStyleSet.h"
#include "nsIChannel.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLElement.h"
#include "nsError.h"
#include "nsURILoader.h"
#include "nsIDocShell.h"
#include "nsIContentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsThreadUtils.h"
#include "nsIScrollableFrame.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/Preferences.h"
#include <algorithm>

#define AUTOMATIC_IMAGE_RESIZING_PREF "browser.enable_automatic_image_resizing"
#define CLICK_IMAGE_RESIZING_PREF "browser.enable_click_image_resizing"
//XXX A hack needed for Firefox's site specific zoom.
#define SITE_SPECIFIC_ZOOM "browser.zoom.siteSpecific"

namespace mozilla {
namespace dom {
 
class ImageListener : public MediaDocumentStreamListener
{
public:
  NS_DECL_NSIREQUESTOBSERVER

  ImageListener(ImageDocument* aDocument);
  virtual ~ImageListener();
};

ImageListener::ImageListener(ImageDocument* aDocument)
  : MediaDocumentStreamListener(aDocument)
{
}

ImageListener::~ImageListener()
{
}

NS_IMETHODIMP
ImageListener::OnStartRequest(nsIRequest* request, nsISupports *ctxt)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);

  ImageDocument *imgDoc = static_cast<ImageDocument*>(mDocument.get());
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  if (!channel) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsPIDOMWindow> domWindow = imgDoc->GetWindow();
  NS_ENSURE_TRUE(domWindow, NS_ERROR_UNEXPECTED);

  // Do a ShouldProcess check to see whether to keep loading the image.
  nsCOMPtr<nsIURI> channelURI;
  channel->GetURI(getter_AddRefs(channelURI));

  nsAutoCString mimeType;
  channel->GetContentType(mimeType);

  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  nsCOMPtr<nsIPrincipal> channelPrincipal;
  if (secMan) {
    secMan->GetChannelPrincipal(channel, getter_AddRefs(channelPrincipal));
  }

  int16_t decision = nsIContentPolicy::ACCEPT;
  nsresult rv = NS_CheckContentProcessPolicy(nsIContentPolicy::TYPE_IMAGE,
                                             channelURI,
                                             channelPrincipal,
                                             domWindow->GetFrameElementInternal(),
                                             mimeType,
                                             nullptr,
                                             &decision,
                                             nsContentUtils::GetContentPolicy(),
                                             secMan);

  if (NS_FAILED(rv) || NS_CP_REJECTED(decision)) {
    request->Cancel(NS_ERROR_CONTENT_BLOCKED);
    return NS_OK;
  }

  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(imgDoc->mImageContent);
  NS_ENSURE_TRUE(imageLoader, NS_ERROR_UNEXPECTED);

  imageLoader->AddObserver(imgDoc);
  imgDoc->mObservingImageLoader = true;
  imageLoader->LoadImageWithChannel(channel, getter_AddRefs(mNextStream));

  return MediaDocumentStreamListener::OnStartRequest(request, ctxt);
}

NS_IMETHODIMP
ImageListener::OnStopRequest(nsIRequest* aRequest, nsISupports* aCtxt, nsresult aStatus)
{
  ImageDocument* imgDoc = static_cast<ImageDocument*>(mDocument.get());
  nsContentUtils::DispatchChromeEvent(imgDoc, static_cast<nsIDocument*>(imgDoc),
                                      NS_LITERAL_STRING("ImageContentLoaded"),
                                      true, true);
  return MediaDocumentStreamListener::OnStopRequest(aRequest, aCtxt, aStatus);
}

ImageDocument::ImageDocument()
  : MediaDocument(),
    mOriginalZoomLevel(1.0)
{
  // NOTE! nsDocument::operator new() zeroes out all members, so don't
  // bother initializing members to 0.
}

ImageDocument::~ImageDocument()
{
}


NS_IMPL_CYCLE_COLLECTION_INHERITED_1(ImageDocument, MediaDocument,
                                     mImageContent)

NS_IMPL_ADDREF_INHERITED(ImageDocument, MediaDocument)
NS_IMPL_RELEASE_INHERITED(ImageDocument, MediaDocument)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(ImageDocument)
  NS_INTERFACE_TABLE_INHERITED3(ImageDocument, nsIImageDocument,
                                imgINotificationObserver, nsIDOMEventListener)
NS_INTERFACE_TABLE_TAIL_INHERITING(MediaDocument)


nsresult
ImageDocument::Init()
{
  nsresult rv = MediaDocument::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  mResizeImageByDefault = Preferences::GetBool(AUTOMATIC_IMAGE_RESIZING_PREF);
  mClickResizingEnabled = Preferences::GetBool(CLICK_IMAGE_RESIZING_PREF);
  mShouldResize = mResizeImageByDefault;
  mFirstResize = true;

  return NS_OK;
}

JSObject*
ImageDocument::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return ImageDocumentBinding::Wrap(aCx, aScope, this);
}

nsresult
ImageDocument::StartDocumentLoad(const char*         aCommand,
                                 nsIChannel*         aChannel,
                                 nsILoadGroup*       aLoadGroup,
                                 nsISupports*        aContainer,
                                 nsIStreamListener** aDocListener,
                                 bool                aReset,
                                 nsIContentSink*     aSink)
{
  nsresult rv =
    MediaDocument::StartDocumentLoad(aCommand, aChannel, aLoadGroup, aContainer,
                                     aDocListener, aReset, aSink);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mOriginalZoomLevel =
    Preferences::GetBool(SITE_SPECIFIC_ZOOM, false) ? 1.0 : GetZoomLevel();

  NS_ASSERTION(aDocListener, "null aDocListener");
  *aDocListener = new ImageListener(this);
  NS_ADDREF(*aDocListener);

  return NS_OK;
}

void
ImageDocument::Destroy()
{
  if (mImageContent) {
    // Remove our event listener from the image content.
    nsCOMPtr<EventTarget> target = do_QueryInterface(mImageContent);
    target->RemoveEventListener(NS_LITERAL_STRING("load"), this, false);
    target->RemoveEventListener(NS_LITERAL_STRING("click"), this, false);

    // Break reference cycle with mImageContent, if we have one
    if (mObservingImageLoader) {
      nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mImageContent);
      if (imageLoader) {
        imageLoader->RemoveObserver(this);
      }
    }

    mImageContent = nullptr;
  }

  MediaDocument::Destroy();
}

void
ImageDocument::SetScriptGlobalObject(nsIScriptGlobalObject* aScriptGlobalObject)
{
  // If the script global object is changing, we need to unhook our event
  // listeners on the window.
  nsCOMPtr<EventTarget> target;
  if (mScriptGlobalObject &&
      aScriptGlobalObject != mScriptGlobalObject) {
    target = do_QueryInterface(mScriptGlobalObject);
    target->RemoveEventListener(NS_LITERAL_STRING("resize"), this, false);
    target->RemoveEventListener(NS_LITERAL_STRING("keypress"), this,
                                false);
  }

  // Set the script global object on the superclass before doing
  // anything that might require it....
  MediaDocument::SetScriptGlobalObject(aScriptGlobalObject);

  if (aScriptGlobalObject) {
    if (!GetRootElement()) {
      // Create synthetic document
#ifdef DEBUG
      nsresult rv =
#endif
        CreateSyntheticDocument();
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to create synthetic document");

      target = do_QueryInterface(mImageContent);
      target->AddEventListener(NS_LITERAL_STRING("load"), this, false);
      target->AddEventListener(NS_LITERAL_STRING("click"), this, false);
    }

    target = do_QueryInterface(aScriptGlobalObject);
    target->AddEventListener(NS_LITERAL_STRING("resize"), this, false);
    target->AddEventListener(NS_LITERAL_STRING("keypress"), this, false);

    if (GetReadyStateEnum() != nsIDocument::READYSTATE_COMPLETE) {
      LinkStylesheet(NS_LITERAL_STRING("resource://gre/res/ImageDocument.css"));
      if (!nsContentUtils::IsChildOfSameType(this)) {
        LinkStylesheet(NS_LITERAL_STRING("resource://gre/res/TopLevelImageDocument.css"));
        LinkStylesheet(NS_LITERAL_STRING("chrome://global/skin/media/TopLevelImageDocument.css"));
      }
    }
    BecomeInteractive();
  }
}

void
ImageDocument::OnPageShow(bool aPersisted,
                          EventTarget* aDispatchStartTarget)
{
  if (aPersisted) {
    mOriginalZoomLevel =
      Preferences::GetBool(SITE_SPECIFIC_ZOOM, false) ? 1.0 : GetZoomLevel();
  }
  MediaDocument::OnPageShow(aPersisted, aDispatchStartTarget);
}

NS_IMETHODIMP
ImageDocument::GetImageResizingEnabled(bool* aImageResizingEnabled)
{
  *aImageResizingEnabled = ImageResizingEnabled();
  return NS_OK;
}

NS_IMETHODIMP
ImageDocument::GetImageIsOverflowing(bool* aImageIsOverflowing)
{
  *aImageIsOverflowing = ImageIsOverflowing();
  return NS_OK;
}

NS_IMETHODIMP
ImageDocument::GetImageIsResized(bool* aImageIsResized)
{
  *aImageIsResized = ImageIsResized();
  return NS_OK;
}

already_AddRefed<imgIRequest>
ImageDocument::GetImageRequest(ErrorResult& aRv)
{
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mImageContent);
  nsCOMPtr<imgIRequest> imageRequest;
  if (imageLoader) {
    aRv = imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                                  getter_AddRefs(imageRequest));
  }
  return imageRequest.forget();
}

NS_IMETHODIMP
ImageDocument::GetImageRequest(imgIRequest** aImageRequest)
{
  ErrorResult rv;
  *aImageRequest = GetImageRequest(rv).take();
  return rv.ErrorCode();
}

void
ImageDocument::ShrinkToFit()
{
  if (!mImageContent) {
    return;
  }
  if (GetZoomLevel() != mOriginalZoomLevel && mImageIsResized &&
      !nsContentUtils::IsChildOfSameType(this)) {
    return;
  }

  // Keep image content alive while changing the attributes.
  nsCOMPtr<nsIContent> imageContent = mImageContent;
  nsCOMPtr<nsIDOMHTMLImageElement> image = do_QueryInterface(mImageContent);
  image->SetWidth(std::max(1, NSToCoordFloor(GetRatio() * mImageWidth)));
  image->SetHeight(std::max(1, NSToCoordFloor(GetRatio() * mImageHeight)));
  
  // The view might have been scrolled when zooming in, scroll back to the
  // origin now that we're showing a shrunk-to-window version.
  ScrollImageTo(0, 0, false);

  if (!mImageContent) {
    // ScrollImageTo flush destroyed our content.
    return;
  }

  SetModeClass(eShrinkToFit);
  
  mImageIsResized = true;
  
  UpdateTitleAndCharset();
}

NS_IMETHODIMP
ImageDocument::DOMShrinkToFit()
{
  ShrinkToFit();
  return NS_OK;
}

NS_IMETHODIMP
ImageDocument::DOMRestoreImageTo(int32_t aX, int32_t aY)
{
  RestoreImageTo(aX, aY);
  return NS_OK;
}

void
ImageDocument::ScrollImageTo(int32_t aX, int32_t aY, bool restoreImage)
{
  float ratio = GetRatio();

  if (restoreImage) {
    RestoreImage();
    FlushPendingNotifications(Flush_Layout);
  }

  nsCOMPtr<nsIPresShell> shell = GetShell();
  if (!shell)
    return;

  nsIScrollableFrame* sf = shell->GetRootScrollFrameAsScrollable();
  if (!sf)
    return;

  nsRect portRect = sf->GetScrollPortRect();
  sf->ScrollTo(nsPoint(nsPresContext::CSSPixelsToAppUnits(aX/ratio) - portRect.width/2,
                       nsPresContext::CSSPixelsToAppUnits(aY/ratio) - portRect.height/2),
               nsIScrollableFrame::INSTANT);
}

void
ImageDocument::RestoreImage()
{
  if (!mImageContent) {
    return;
  }
  // Keep image content alive while changing the attributes.
  nsCOMPtr<nsIContent> imageContent = mImageContent;
  imageContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::width, true);
  imageContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::height, true);
  
  if (mImageIsOverflowing) {
    SetModeClass(eOverflowing);
  }
  else {
    SetModeClass(eNone);
  }
  
  mImageIsResized = false;
  
  UpdateTitleAndCharset();
}

NS_IMETHODIMP
ImageDocument::DOMRestoreImage()
{
  RestoreImage();
  return NS_OK;
}

void
ImageDocument::ToggleImageSize()
{
  mShouldResize = true;
  if (mImageIsResized) {
    mShouldResize = false;
    ResetZoomLevel();
    RestoreImage();
  }
  else if (mImageIsOverflowing) {
    ResetZoomLevel();
    ShrinkToFit();
  }
}

NS_IMETHODIMP
ImageDocument::DOMToggleImageSize()
{
  ToggleImageSize();
  return NS_OK;
}

NS_IMETHODIMP
ImageDocument::Notify(imgIRequest* aRequest, int32_t aType, const nsIntRect* aData)
{
  if (aType == imgINotificationObserver::SIZE_AVAILABLE) {
    nsCOMPtr<imgIContainer> image;
    aRequest->GetImage(getter_AddRefs(image));
    return OnStartContainer(aRequest, image);
  }

  nsDOMTokenList* classList = mImageContent->AsElement()->GetClassList();
  mozilla::ErrorResult rv;
  if (aType == imgINotificationObserver::DECODE_COMPLETE) {
    if (mImageContent && !nsContentUtils::IsChildOfSameType(this)) {
      // Update the background-color of the image only after the
      // image has been decoded to prevent flashes of just the
      // background-color.
      classList->Add(NS_LITERAL_STRING("decoded"), rv);
      NS_ENSURE_SUCCESS(rv.ErrorCode(), rv.ErrorCode());
    }
  }

  if (aType == imgINotificationObserver::DISCARD) {
    // mImageContent can be null if the document is already destroyed
    if (mImageContent && !nsContentUtils::IsChildOfSameType(this)) {
      // Remove any decoded-related styling when the image is unloaded.
      classList->Remove(NS_LITERAL_STRING("decoded"), rv);
      NS_ENSURE_SUCCESS(rv.ErrorCode(), rv.ErrorCode());
    }
  }

  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    uint32_t reqStatus;
    aRequest->GetImageStatus(&reqStatus);
    nsresult status =
        reqStatus & imgIRequest::STATUS_ERROR ? NS_ERROR_FAILURE : NS_OK;
    return OnStopRequest(aRequest, status);
  }

  return NS_OK;
}

void
ImageDocument::SetModeClass(eModeClasses mode)
{
  nsDOMTokenList* classList = mImageContent->AsElement()->GetClassList();
  mozilla::ErrorResult rv;

  if (mode == eShrinkToFit) {
    classList->Add(NS_LITERAL_STRING("shrinkToFit"), rv);
  } else {
    classList->Remove(NS_LITERAL_STRING("shrinkToFit"), rv);
  }

  if (mode == eOverflowing) {
    classList->Add(NS_LITERAL_STRING("overflowing"), rv);
  } else {
    classList->Remove(NS_LITERAL_STRING("overflowing"), rv);
  }
}

nsresult
ImageDocument::OnStartContainer(imgIRequest* aRequest, imgIContainer* aImage)
{
  // Styles have not yet been applied, so we don't know the final size. For now,
  // default to the image's intrinsic size.
  aImage->GetWidth(&mImageWidth);
  aImage->GetHeight(&mImageHeight);

  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &ImageDocument::DefaultCheckOverflowing);
  nsContentUtils::AddScriptRunner(runnable);
  UpdateTitleAndCharset();

  return NS_OK;
}

nsresult
ImageDocument::OnStopRequest(imgIRequest *aRequest,
                             nsresult aStatus)
{
  UpdateTitleAndCharset();

  // mImageContent can be null if the document is already destroyed
  if (NS_FAILED(aStatus) && mStringBundle && mImageContent) {
    nsAutoCString src;
    mDocumentURI->GetSpec(src);
    NS_ConvertUTF8toUTF16 srcString(src);
    const char16_t* formatString[] = { srcString.get() };
    nsXPIDLString errorMsg;
    NS_NAMED_LITERAL_STRING(str, "InvalidImage");
    mStringBundle->FormatStringFromName(str.get(), formatString, 1,
                                        getter_Copies(errorMsg));

    mImageContent->SetAttr(kNameSpaceID_None, nsGkAtoms::alt, errorMsg, false);
  }

  return NS_OK;
}

NS_IMETHODIMP
ImageDocument::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("resize")) {
    CheckOverflowing(false);
  }
  else if (eventType.EqualsLiteral("click") && mClickResizingEnabled) {
    ResetZoomLevel();
    mShouldResize = true;
    if (mImageIsResized) {
      int32_t x = 0, y = 0;
      nsCOMPtr<nsIDOMMouseEvent> event(do_QueryInterface(aEvent));
      if (event) {
        event->GetClientX(&x);
        event->GetClientY(&y);
        int32_t left = 0, top = 0;
        nsCOMPtr<nsIDOMHTMLElement> htmlElement =
          do_QueryInterface(mImageContent);
        htmlElement->GetOffsetLeft(&left);
        htmlElement->GetOffsetTop(&top);
        x -= left;
        y -= top;
      }
      mShouldResize = false;
      RestoreImageTo(x, y);
    }
    else if (mImageIsOverflowing) {
      ShrinkToFit();
    }
  } else if (eventType.EqualsLiteral("load")) {
    UpdateSizeFromLayout();
  }

  return NS_OK;
}

void
ImageDocument::UpdateSizeFromLayout()
{
  // Pull an updated size from the content frame to account for any size
  // change due to CSS properties like |image-orientation|.
  Element* contentElement = mImageContent->AsElement();
  if (!contentElement) {
    return;
  }

  nsIFrame* contentFrame = contentElement->GetPrimaryFrame(Flush_Frames);
  if (!contentFrame) {
    return;
  }

  nsIntSize oldSize(mImageWidth, mImageHeight);
  IntrinsicSize newSize = contentFrame->GetIntrinsicSize();

  if (newSize.width.GetUnit() == eStyleUnit_Coord) {
    mImageWidth = nsPresContext::AppUnitsToFloatCSSPixels(newSize.width.GetCoordValue());
  }
  if (newSize.height.GetUnit() == eStyleUnit_Coord) {
    mImageHeight = nsPresContext::AppUnitsToFloatCSSPixels(newSize.height.GetCoordValue());
  }

  // Ensure that our information about overflow is up-to-date if needed.
  if (mImageWidth != oldSize.width || mImageHeight != oldSize.height) {
    CheckOverflowing(false);
  }
}

nsresult
ImageDocument::CreateSyntheticDocument()
{
  // Synthesize an html document that refers to the image
  nsresult rv = MediaDocument::CreateSyntheticDocument();
  NS_ENSURE_SUCCESS(rv, rv);

  // Add the image element
  Element* body = GetBodyElement();
  if (!body) {
    NS_WARNING("no body on image document!");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = mNodeInfoManager->GetNodeInfo(nsGkAtoms::img, nullptr,
                                           kNameSpaceID_XHTML,
                                           nsIDOMNode::ELEMENT_NODE);

  mImageContent = NS_NewHTMLImageElement(nodeInfo.forget());
  if (!mImageContent) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mImageContent);
  NS_ENSURE_TRUE(imageLoader, NS_ERROR_UNEXPECTED);

  nsAutoCString src;
  mDocumentURI->GetSpec(src);

  NS_ConvertUTF8toUTF16 srcString(src);
  // Make sure not to start the image load from here...
  imageLoader->SetLoadingEnabled(false);
  mImageContent->SetAttr(kNameSpaceID_None, nsGkAtoms::src, srcString, false);
  mImageContent->SetAttr(kNameSpaceID_None, nsGkAtoms::alt, srcString, false);

  body->AppendChildTo(mImageContent, false);
  imageLoader->SetLoadingEnabled(true);

  return NS_OK;
}

nsresult
ImageDocument::CheckOverflowing(bool changeState)
{
  /* Create a scope so that the style context gets destroyed before we might
   * call RebuildStyleData.  Also, holding onto pointers to the
   * presentation through style resolution is potentially dangerous.
   */
  {
    nsIPresShell *shell = GetShell();
    if (!shell) {
      return NS_OK;
    }

    nsPresContext *context = shell->GetPresContext();
    nsRect visibleArea = context->GetVisibleArea();

    mVisibleWidth = nsPresContext::AppUnitsToFloatCSSPixels(visibleArea.width);
    mVisibleHeight = nsPresContext::AppUnitsToFloatCSSPixels(visibleArea.height);
  }

  bool imageWasOverflowing = mImageIsOverflowing;
  mImageIsOverflowing =
    mImageWidth > mVisibleWidth || mImageHeight > mVisibleHeight;
  bool windowBecameBigEnough = imageWasOverflowing && !mImageIsOverflowing;

  if (changeState || mShouldResize || mFirstResize ||
      windowBecameBigEnough) {
    if (mImageIsOverflowing && (changeState || mShouldResize)) {
      ShrinkToFit();
    }
    else if (mImageIsResized || mFirstResize || windowBecameBigEnough) {
      RestoreImage();
    }
  }
  mFirstResize = false;

  return NS_OK;
}

void 
ImageDocument::UpdateTitleAndCharset()
{
  nsAutoCString typeStr;
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

    const char16_t* formatString[1] = { ratioStr.get() };
    mStringBundle->FormatStringFromName(MOZ_UTF16("ScaledImage"),
                                        formatString, 1,
                                        getter_Copies(status));
  }

  static const char* const formatNames[4] = 
  {
    "ImageTitleWithNeitherDimensionsNorFile",
    "ImageTitleWithoutDimensions",
    "ImageTitleWithDimensions2",
    "ImageTitleWithDimensions2AndFile",
  };

  MediaDocument::UpdateTitleAndCharset(typeStr, formatNames,
                                       mImageWidth, mImageHeight, status);
}

void
ImageDocument::ResetZoomLevel()
{
  nsCOMPtr<nsIDocShell> docShell(mDocumentContainer);
  if (docShell) {
    if (nsContentUtils::IsChildOfSameType(this)) {
      return;
    }

    nsCOMPtr<nsIContentViewer> cv;
    docShell->GetContentViewer(getter_AddRefs(cv));
    nsCOMPtr<nsIMarkupDocumentViewer> mdv = do_QueryInterface(cv);
    if (mdv) {
      mdv->SetFullZoom(mOriginalZoomLevel);
    }
  }
}

float
ImageDocument::GetZoomLevel()
{
  float zoomLevel = mOriginalZoomLevel;
  nsCOMPtr<nsIDocShell> docShell(mDocumentContainer);
  if (docShell) {
    nsCOMPtr<nsIContentViewer> cv;
    docShell->GetContentViewer(getter_AddRefs(cv));
    nsCOMPtr<nsIMarkupDocumentViewer> mdv = do_QueryInterface(cv);
    if (mdv) {
      mdv->GetFullZoom(&zoomLevel);
    }
  }
  return zoomLevel;
}

} // namespace dom
} // namespace mozilla

nsresult
NS_NewImageDocument(nsIDocument** aResult)
{
  mozilla::dom::ImageDocument* doc = new mozilla::dom::ImageDocument();
  NS_ADDREF(doc);

  nsresult rv = doc->Init();
  if (NS_FAILED(rv)) {
    NS_RELEASE(doc);
  }

  *aResult = doc;

  return rv;
}
