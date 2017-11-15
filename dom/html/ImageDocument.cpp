/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageDocument.h"
#include "mozilla/dom/ImageDocumentBinding.h"
#include "mozilla/dom/HTMLImageElement.h"
#include "nsRect.h"
#include "nsIImageLoadingContent.h"
#include "nsGenericHTMLElement.h"
#include "nsDocShell.h"
#include "nsIDocumentInlines.h"
#include "nsDOMTokenList.h"
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
#include "nsThreadUtils.h"
#include "nsIScrollableFrame.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/Preferences.h"
#include <algorithm>

#define AUTOMATIC_IMAGE_RESIZING_PREF "browser.enable_automatic_image_resizing"
#define CLICK_IMAGE_RESIZING_PREF "browser.enable_click_image_resizing"

//XXX A hack needed for Firefox's site specific zoom.
static bool IsSiteSpecific()
{
  return !mozilla::Preferences::GetBool("privacy.resistFingerprinting", false) &&
         mozilla::Preferences::GetBool("browser.zoom.siteSpecific", false);
}

namespace mozilla {
namespace dom {

class ImageListener : public MediaDocumentStreamListener
{
public:
  NS_DECL_NSIREQUESTOBSERVER

  explicit ImageListener(ImageDocument* aDocument);
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

  nsCOMPtr<nsPIDOMWindowOuter> domWindow = imgDoc->GetWindow();
  NS_ENSURE_TRUE(domWindow, NS_ERROR_UNEXPECTED);

  // Do a ShouldProcess check to see whether to keep loading the image.
  nsCOMPtr<nsIURI> channelURI;
  channel->GetURI(getter_AddRefs(channelURI));

  nsAutoCString mimeType;
  channel->GetContentType(mimeType);

  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  nsCOMPtr<nsIPrincipal> channelPrincipal;
  if (secMan) {
    secMan->GetChannelResultPrincipal(channel, getter_AddRefs(channelPrincipal));
  }

  nsCOMPtr<nsILoadInfo> loadInfo = channel->GetLoadInfo();

  int16_t decision = nsIContentPolicy::ACCEPT;
  nsresult rv = NS_CheckContentProcessPolicy(nsIContentPolicy::TYPE_INTERNAL_IMAGE,
                                             channelURI,
                                             channelPrincipal,
                                             loadInfo ? loadInfo->TriggeringPrincipal() : nullptr,
                                             domWindow->GetFrameElementInternal(),
                                             mimeType,
                                             nullptr,
                                             &decision,
                                             nsContentUtils::GetContentPolicy());

  if (NS_FAILED(rv) || NS_CP_REJECTED(decision)) {
    request->Cancel(NS_ERROR_CONTENT_BLOCKED);
    return NS_OK;
  }

  if (!imgDoc->mObservingImageLoader) {
    nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(imgDoc->mImageContent);
    NS_ENSURE_TRUE(imageLoader, NS_ERROR_UNEXPECTED);

    imageLoader->AddNativeObserver(imgDoc);
    imgDoc->mObservingImageLoader = true;
    imageLoader->LoadImageWithChannel(channel, getter_AddRefs(mNextStream));
  }

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
  : MediaDocument()
  , mVisibleWidth(0.0)
  , mVisibleHeight(0.0)
  , mImageWidth(0)
  , mImageHeight(0)
  , mResizeImageByDefault(false)
  , mClickResizingEnabled(false)
  , mImageIsOverflowingHorizontally(false)
  , mImageIsOverflowingVertically(false)
  , mImageIsResized(false)
  , mShouldResize(false)
  , mFirstResize(false)
  , mObservingImageLoader(false)
  , mOriginalZoomLevel(1.0)
{
}

ImageDocument::~ImageDocument()
{
}


NS_IMPL_CYCLE_COLLECTION_INHERITED(ImageDocument, MediaDocument,
                                   mImageContent)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(ImageDocument,
                                             MediaDocument,
                                             nsIImageDocument,
                                             imgINotificationObserver,
                                             nsIDOMEventListener)


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
ImageDocument::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ImageDocumentBinding::Wrap(aCx, this, aGivenProto);
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

  mOriginalZoomLevel = IsSiteSpecific() ? 1.0 : GetZoomLevel();

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
        imageLoader->RemoveNativeObserver(this);
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
      LinkStylesheet(NS_LITERAL_STRING("resource://content-accessible/ImageDocument.css"));
      if (!nsContentUtils::IsChildOfSameType(this)) {
        LinkStylesheet(NS_LITERAL_STRING("resource://content-accessible/TopLevelImageDocument.css"));
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
    mOriginalZoomLevel = IsSiteSpecific() ? 1.0 : GetZoomLevel();
  }
  RefPtr<ImageDocument> kungFuDeathGrip(this);
  UpdateSizeFromLayout();

  MediaDocument::OnPageShow(aPersisted, aDispatchStartTarget);
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
  return rv.StealNSResult();
}

void
ImageDocument::ShrinkToFit()
{
  if (!mImageContent) {
    return;
  }
  if (GetZoomLevel() != mOriginalZoomLevel && mImageIsResized &&
      !nsContentUtils::IsChildOfSameType(this)) {
    // If we're zoomed, so that we don't maintain the invariant that
    // mImageIsResized if and only if its displayed width/height fit in
    // mVisibleWidth/mVisibleHeight, then we may need to switch to/from the
    // overflowingVertical class here, because our viewport size may have
    // changed and we don't plan to adjust the image size to compensate.  Since
    // mImageIsResized it has a "height" attribute set, and we can just get the
    // displayed image height by getting .height on the HTMLImageElement.
    //
    // Hold strong ref, because Height() can run script.
    RefPtr<HTMLImageElement> img = HTMLImageElement::FromContent(mImageContent);
    uint32_t imageHeight = img->Height();
    nsDOMTokenList* classList = img->ClassList();
    ErrorResult ignored;
    if (imageHeight > mVisibleHeight) {
      classList->Add(NS_LITERAL_STRING("overflowingVertical"), ignored);
    } else {
      classList->Remove(NS_LITERAL_STRING("overflowingVertical"), ignored);
    }
    ignored.SuppressException();
    return;
  }

  // Keep image content alive while changing the attributes.
  RefPtr<HTMLImageElement> image = HTMLImageElement::FromContent(mImageContent);
  {
    IgnoredErrorResult ignored;
    image->SetWidth(std::max(1, NSToCoordFloor(GetRatio() * mImageWidth)),
                    ignored);
  }
  {
    IgnoredErrorResult ignored;
    image->SetHeight(std::max(1, NSToCoordFloor(GetRatio() * mImageHeight)),
                     ignored);
  }

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
  if (restoreImage) {
    RestoreImage();
    FlushPendingNotifications(FlushType::Layout);
  }

  nsCOMPtr<nsIPresShell> shell = GetShell();
  if (!shell) {
    return;
  }

  nsIScrollableFrame* sf = shell->GetRootScrollFrameAsScrollable();
  if (!sf) {
    return;
  }

  float ratio = GetRatio();
  // Don't try to scroll image if the document is not visible (mVisibleWidth or
  // mVisibleHeight is zero).
  if (ratio <= 0.0) {
    return;
  }
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
  nsCOMPtr<Element> imageContent = mImageContent;
  imageContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::width, true);
  imageContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::height, true);

  if (ImageIsOverflowing()) {
    if (!mImageIsOverflowingVertically) {
      SetModeClass(eOverflowingHorizontalOnly);
    } else {
      SetModeClass(eOverflowingVertical);
    }
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
  else if (ImageIsOverflowing()) {
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
    return OnSizeAvailable(aRequest, image);
  }

  // Run this using a script runner because HAS_TRANSPARENCY notifications can
  // come during painting and this will trigger invalidation.
  if (aType == imgINotificationObserver::HAS_TRANSPARENCY) {
    nsCOMPtr<nsIRunnable> runnable =
      NewRunnableMethod("dom::ImageDocument::OnHasTransparency",
                        this,
                        &ImageDocument::OnHasTransparency);
    nsContentUtils::AddScriptRunner(runnable);
  }

  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    uint32_t reqStatus;
    aRequest->GetImageStatus(&reqStatus);
    nsresult status =
        reqStatus & imgIRequest::STATUS_ERROR ? NS_ERROR_FAILURE : NS_OK;
    return OnLoadComplete(aRequest, status);
  }

  return NS_OK;
}

void
ImageDocument::OnHasTransparency()
{
  if (!mImageContent || nsContentUtils::IsChildOfSameType(this)) {
    return;
  }

  nsDOMTokenList* classList = mImageContent->ClassList();
  mozilla::ErrorResult rv;
  classList->Add(NS_LITERAL_STRING("transparent"), rv);
}

void
ImageDocument::SetModeClass(eModeClasses mode)
{
  nsDOMTokenList* classList = mImageContent->ClassList();
  ErrorResult rv;

  if (mode == eShrinkToFit) {
    classList->Add(NS_LITERAL_STRING("shrinkToFit"), rv);
  } else {
    classList->Remove(NS_LITERAL_STRING("shrinkToFit"), rv);
  }

  if (mode == eOverflowingVertical) {
    classList->Add(NS_LITERAL_STRING("overflowingVertical"), rv);
  } else {
    classList->Remove(NS_LITERAL_STRING("overflowingVertical"), rv);
  }

  if (mode == eOverflowingHorizontalOnly) {
    classList->Add(NS_LITERAL_STRING("overflowingHorizontalOnly"), rv);
  } else {
    classList->Remove(NS_LITERAL_STRING("overflowingHorizontalOnly"), rv);
  }

  rv.SuppressException();
}

nsresult
ImageDocument::OnSizeAvailable(imgIRequest* aRequest, imgIContainer* aImage)
{
  int32_t oldWidth = mImageWidth;
  int32_t oldHeight = mImageHeight;

  // Styles have not yet been applied, so we don't know the final size. For now,
  // default to the image's intrinsic size.
  aImage->GetWidth(&mImageWidth);
  aImage->GetHeight(&mImageHeight);

  // Multipart images send size available for each part; ignore them if it
  // doesn't change our size. (We may not even support changing size in
  // multipart images in the future.)
  if (oldWidth == mImageWidth && oldHeight == mImageHeight) {
    return NS_OK;
  }

  nsCOMPtr<nsIRunnable> runnable =
    NewRunnableMethod("dom::ImageDocument::DefaultCheckOverflowing",
                      this,
                      &ImageDocument::DefaultCheckOverflowing);
  nsContentUtils::AddScriptRunner(runnable);
  UpdateTitleAndCharset();

  return NS_OK;
}

nsresult
ImageDocument::OnLoadComplete(imgIRequest* aRequest, nsresult aStatus)
{
  UpdateTitleAndCharset();

  // mImageContent can be null if the document is already destroyed
  if (NS_FAILED(aStatus) && mStringBundle && mImageContent) {
    nsAutoCString src;
    mDocumentURI->GetSpec(src);
    NS_ConvertUTF8toUTF16 srcString(src);
    const char16_t* formatString[] = { srcString.get() };
    nsAutoString errorMsg;
    mStringBundle->FormatStringFromName("InvalidImage", formatString, 1,
                                        errorMsg);

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
        RefPtr<HTMLImageElement> img =
          HTMLImageElement::FromContent(mImageContent);
        x -= img->OffsetLeft();
        y -= img->OffsetTop();
      }
      mShouldResize = false;
      RestoreImageTo(x, y);
    }
    else if (ImageIsOverflowing()) {
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
  if (!mImageContent) {
    return;
  }

  // Need strong ref, because GetPrimaryFrame can run script.
  nsCOMPtr<Element> imageContent = mImageContent;
  nsIFrame* contentFrame = imageContent->GetPrimaryFrame(FlushType::Frames);
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

  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
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

  bool imageWasOverflowing = ImageIsOverflowing();
  bool imageWasOverflowingVertically = mImageIsOverflowingVertically;
  mImageIsOverflowingHorizontally = mImageWidth > mVisibleWidth;
  mImageIsOverflowingVertically = mImageHeight > mVisibleHeight;
  bool windowBecameBigEnough = imageWasOverflowing && !ImageIsOverflowing();
  bool verticalOverflowChanged =
    mImageIsOverflowingVertically != imageWasOverflowingVertically;

  if (changeState || mShouldResize || mFirstResize ||
      windowBecameBigEnough || verticalOverflowChanged) {
    if (ImageIsOverflowing() && (changeState || mShouldResize)) {
      ShrinkToFit();
    }
    else if (mImageIsResized || mFirstResize || windowBecameBigEnough) {
      RestoreImage();
    } else if (!mImageIsResized && verticalOverflowChanged) {
      if (mImageIsOverflowingVertically) {
        SetModeClass(eOverflowingVertical);
      } else {
        SetModeClass(eOverflowingHorizontalOnly);
      }
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
    nsCString mimeType;
    imageRequest->GetMimeType(getter_Copies(mimeType));
    ToUpperCase(mimeType);
    nsCString::const_iterator start, end;
    mimeType.BeginReading(start);
    mimeType.EndReading(end);
    nsCString::const_iterator iter = end;
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

  nsAutoString status;
  if (mImageIsResized) {
    nsAutoString ratioStr;
    ratioStr.AppendInt(NSToCoordFloor(GetRatio() * 100));

    const char16_t* formatString[1] = { ratioStr.get() };
    mStringBundle->FormatStringFromName("ScaledImage", formatString, 1, status);
  }

  static const char* const formatNames[4] =
  {
    "ImageTitleWithNeitherDimensionsNorFile",
    "ImageTitleWithoutDimensions",
    "ImageTitleWithDimensions2",
    "ImageTitleWithDimensions2AndFile",
  };

  MediaDocument::UpdateTitleAndCharset(typeStr, mChannel, formatNames,
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
    if (cv) {
      cv->SetFullZoom(mOriginalZoomLevel);
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
    if (cv) {
      cv->GetFullZoom(&zoomLevel);
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
