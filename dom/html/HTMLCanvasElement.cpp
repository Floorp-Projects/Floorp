/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLCanvasElement.h"

#include "ImageEncoder.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "Layers.h"
#include "MediaSegment.h"
#include "mozilla/Assertions.h"
#include "mozilla/Base64.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/CanvasCaptureMediaStream.h"
#include "mozilla/dom/CanvasRenderingContext2D.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/HTMLCanvasElementBinding.h"
#include "mozilla/dom/MediaStreamTrack.h"
#include "mozilla/dom/MouseEvent.h"
#include "mozilla/dom/OffscreenCanvas.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/layers/AsyncCanvasRenderer.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "nsAttrValueInlines.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"
#include "nsDOMJSUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsITimer.h"
#include "nsIWritablePropertyBag2.h"
#include "nsIXPConnect.h"
#include "nsJSUtils.h"
#include "nsLayoutUtils.h"
#include "nsMathUtils.h"
#include "nsNetUtil.h"
#include "nsRefreshDriver.h"
#include "nsStreamUtils.h"
#include "ActiveLayerTracker.h"
#include "WebGL1Context.h"
#include "WebGL2Context.h"

using namespace mozilla::layers;
using namespace mozilla::gfx;

NS_IMPL_NS_NEW_HTML_ELEMENT(Canvas)

namespace mozilla {
namespace dom {

class RequestedFrameRefreshObserver : public nsARefreshObserver
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RequestedFrameRefreshObserver, override)

public:
  RequestedFrameRefreshObserver(HTMLCanvasElement* const aOwningElement,
                                nsRefreshDriver* aRefreshDriver)
    : mRegistered(false),
      mOwningElement(aOwningElement),
      mRefreshDriver(aRefreshDriver)
  {
    MOZ_ASSERT(mOwningElement);
  }

  static already_AddRefed<DataSourceSurface>
  CopySurface(const RefPtr<SourceSurface>& aSurface)
  {
    RefPtr<DataSourceSurface> data = aSurface->GetDataSurface();
    if (!data) {
      return nullptr;
    }

    DataSourceSurface::ScopedMap read(data, DataSourceSurface::READ);
    if (!read.IsMapped()) {
      return nullptr;
    }

    RefPtr<DataSourceSurface> copy =
      Factory::CreateDataSourceSurfaceWithStride(data->GetSize(),
                                                 data->GetFormat(),
                                                 read.GetStride());
    if (!copy) {
      return nullptr;
    }

    DataSourceSurface::ScopedMap write(copy, DataSourceSurface::WRITE);
    if (!write.IsMapped()) {
      return nullptr;
    }

    MOZ_ASSERT(read.GetStride() == write.GetStride());
    MOZ_ASSERT(data->GetSize() == copy->GetSize());
    MOZ_ASSERT(data->GetFormat() == copy->GetFormat());

    memcpy(write.GetData(), read.GetData(),
           write.GetStride() * copy->GetSize().height);

    return copy.forget();
  }

  void WillRefresh(TimeStamp aTime) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mOwningElement) {
      return;
    }

    if (mOwningElement->IsWriteOnly()) {
      return;
    }

    if (mOwningElement->IsContextCleanForFrameCapture()) {
      return;
    }

    if (!mOwningElement->IsFrameCaptureRequested()) {
      return;
    }

    RefPtr<SourceSurface> snapshot = mOwningElement->GetSurfaceSnapshot(nullptr);
    if (!snapshot) {
      return;
    }

    RefPtr<DataSourceSurface> copy = CopySurface(snapshot);

    mOwningElement->SetFrameCapture(copy.forget());
    mOwningElement->MarkContextCleanForFrameCapture();
  }

  void DetachFromRefreshDriver()
  {
    MOZ_ASSERT(mOwningElement);
    MOZ_ASSERT(mRefreshDriver);

    Unregister();
    mRefreshDriver = nullptr;
  }

  void Register()
  {
    if (mRegistered) {
      return;
    }

    MOZ_ASSERT(mRefreshDriver);
    if (mRefreshDriver) {
      mRefreshDriver->AddRefreshObserver(this, Flush_Display);
      mRegistered = true;
    }
  }

  void Unregister()
  {
    if (!mRegistered) {
      return;
    }

    MOZ_ASSERT(mRefreshDriver);
    if (mRefreshDriver) {
      mRefreshDriver->RemoveRefreshObserver(this, Flush_Display);
      mRegistered = false;
    }
  }

private:
  virtual ~RequestedFrameRefreshObserver()
  {
    MOZ_ASSERT(!mRefreshDriver);
    MOZ_ASSERT(!mRegistered);
  }

  bool mRegistered;
  HTMLCanvasElement* const mOwningElement;
  RefPtr<nsRefreshDriver> mRefreshDriver;
};

// ---------------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(HTMLCanvasPrintState, mCanvas,
                                      mContext, mCallback)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(HTMLCanvasPrintState, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(HTMLCanvasPrintState, Release)

HTMLCanvasPrintState::HTMLCanvasPrintState(HTMLCanvasElement* aCanvas,
                                           nsICanvasRenderingContextInternal* aContext,
                                           nsITimerCallback* aCallback)
  : mIsDone(false), mPendingNotify(false), mCanvas(aCanvas),
    mContext(aContext), mCallback(aCallback)
{
}

HTMLCanvasPrintState::~HTMLCanvasPrintState()
{
}

/* virtual */ JSObject*
HTMLCanvasPrintState::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MozCanvasPrintStateBinding::Wrap(aCx, this, aGivenProto);
}

nsISupports*
HTMLCanvasPrintState::Context() const
{
  return mContext;
}

void
HTMLCanvasPrintState::Done()
{
  if (!mPendingNotify && !mIsDone) {
    // The canvas needs to be invalidated for printing reftests on linux to
    // work.
    if (mCanvas) {
      mCanvas->InvalidateCanvas();
    }
    RefPtr<nsRunnableMethod<HTMLCanvasPrintState> > doneEvent =
      NewRunnableMethod(this, &HTMLCanvasPrintState::NotifyDone);
    if (NS_SUCCEEDED(NS_DispatchToCurrentThread(doneEvent))) {
      mPendingNotify = true;
    }
  }
}

void
HTMLCanvasPrintState::NotifyDone()
{
  mIsDone = true;
  mPendingNotify = false;
  if (mCallback) {
    mCallback->Notify(nullptr);
  }
}

// ---------------------------------------------------------------------------

HTMLCanvasElementObserver::HTMLCanvasElementObserver(HTMLCanvasElement* aElement)
    : mElement(aElement)
{
  RegisterVisibilityChangeEvent();
  RegisterMemoryPressureEvent();
}

HTMLCanvasElementObserver::~HTMLCanvasElementObserver()
{
  Destroy();
}

void
HTMLCanvasElementObserver::Destroy()
{
  UnregisterMemoryPressureEvent();
  UnregisterVisibilityChangeEvent();
  mElement = nullptr;
}

void
HTMLCanvasElementObserver::RegisterVisibilityChangeEvent()
{
  if (!mElement) {
    return;
  }

  nsIDocument* document = mElement->OwnerDoc();
  document->AddSystemEventListener(NS_LITERAL_STRING("visibilitychange"),
                                   this, true, false);
}

void
HTMLCanvasElementObserver::UnregisterVisibilityChangeEvent()
{
  if (!mElement) {
    return;
  }

  nsIDocument* document = mElement->OwnerDoc();
  document->RemoveSystemEventListener(NS_LITERAL_STRING("visibilitychange"),
                                      this, true);
}

void
HTMLCanvasElementObserver::RegisterMemoryPressureEvent()
{
  if (!mElement) {
    return;
  }

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();

  MOZ_ASSERT(observerService);

  if (observerService)
    observerService->AddObserver(this, "memory-pressure", false);
}

void
HTMLCanvasElementObserver::UnregisterMemoryPressureEvent()
{
  if (!mElement) {
    return;
  }

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();

  // Do not assert on observerService here. This might be triggered by
  // the cycle collector at a late enough time, that XPCOM services are
  // no longer available. See bug 1029504.
  if (observerService)
    observerService->RemoveObserver(this, "memory-pressure");
}

NS_IMETHODIMP
HTMLCanvasElementObserver::Observe(nsISupports*, const char* aTopic, const char16_t*)
{
  if (!mElement || strcmp(aTopic, "memory-pressure")) {
    return NS_OK;
  }

  mElement->OnMemoryPressure();

  return NS_OK;
}

NS_IMETHODIMP
HTMLCanvasElementObserver::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString type;
  aEvent->GetType(type);
  if (!mElement || !type.EqualsLiteral("visibilitychange")) {
    return NS_OK;
  }

  mElement->OnVisibilityChange();

  return NS_OK;
}

NS_IMPL_ISUPPORTS(HTMLCanvasElementObserver, nsIObserver)

// ---------------------------------------------------------------------------

HTMLCanvasElement::HTMLCanvasElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo),
    mResetLayer(true) ,
    mWriteOnly(false)
{
}

HTMLCanvasElement::~HTMLCanvasElement()
{
  if (mContextObserver) {
    mContextObserver->Destroy();
    mContextObserver = nullptr;
  }

  ResetPrintCallback();
  if (mRequestedFrameRefreshObserver) {
    mRequestedFrameRefreshObserver->DetachFromRefreshDriver();
  }

  if (mAsyncCanvasRenderer) {
    mAsyncCanvasRenderer->mHTMLCanvasElement = nullptr;
  }
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLCanvasElement, nsGenericHTMLElement,
                                   mCurrentContext, mPrintCallback,
                                   mPrintState, mOriginalCanvas,
                                   mOffscreenCanvas)

NS_IMPL_ADDREF_INHERITED(HTMLCanvasElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLCanvasElement, Element)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLCanvasElement)
  NS_INTERFACE_TABLE_INHERITED(HTMLCanvasElement, nsIDOMHTMLCanvasElement)
NS_INTERFACE_TABLE_TAIL_INHERITING(nsGenericHTMLElement)

NS_IMPL_ELEMENT_CLONE(HTMLCanvasElement)

/* virtual */ JSObject*
HTMLCanvasElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLCanvasElementBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<nsICanvasRenderingContextInternal>
HTMLCanvasElement::CreateContext(CanvasContextType aContextType)
{
  RefPtr<nsICanvasRenderingContextInternal> ret =
    CanvasRenderingContextHelper::CreateContext(aContextType);

  // Add Observer for webgl canvas.
  if (aContextType == CanvasContextType::WebGL1 ||
      aContextType == CanvasContextType::WebGL2) {
    if (!mContextObserver) {
      mContextObserver = new HTMLCanvasElementObserver(this);
    }
  }

  ret->SetCanvasElement(this);
  return ret.forget();
}

nsIntSize
HTMLCanvasElement::GetWidthHeight()
{
  nsIntSize size(DEFAULT_CANVAS_WIDTH, DEFAULT_CANVAS_HEIGHT);
  const nsAttrValue* value;

  if ((value = GetParsedAttr(nsGkAtoms::width)) &&
      value->Type() == nsAttrValue::eInteger)
  {
      size.width = value->GetIntegerValue();
  }

  if ((value = GetParsedAttr(nsGkAtoms::height)) &&
      value->Type() == nsAttrValue::eInteger)
  {
      size.height = value->GetIntegerValue();
  }

  MOZ_ASSERT(size.width >= 0 && size.height >= 0,
             "we should've required <canvas> width/height attrs to be "
             "unsigned (non-negative) values");

  return size;
}

NS_IMPL_UINT_ATTR_DEFAULT_VALUE(HTMLCanvasElement, Width, width, DEFAULT_CANVAS_WIDTH)
NS_IMPL_UINT_ATTR_DEFAULT_VALUE(HTMLCanvasElement, Height, height, DEFAULT_CANVAS_HEIGHT)
NS_IMPL_BOOL_ATTR(HTMLCanvasElement, MozOpaque, moz_opaque)

nsresult
HTMLCanvasElement::SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify)
{
  nsresult rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue,
                                              aNotify);
  if (NS_SUCCEEDED(rv) && mCurrentContext &&
      aNameSpaceID == kNameSpaceID_None &&
      (aName == nsGkAtoms::width || aName == nsGkAtoms::height || aName == nsGkAtoms::moz_opaque))
  {
    ErrorResult dummy;
    rv = UpdateContext(nullptr, JS::NullHandleValue, dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return rv;
}

nsresult
HTMLCanvasElement::UnsetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                             bool aNotify)
{
  nsresult rv = nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aName, aNotify);
  if (NS_SUCCEEDED(rv) && mCurrentContext &&
      aNameSpaceID == kNameSpaceID_None &&
      (aName == nsGkAtoms::width || aName == nsGkAtoms::height || aName == nsGkAtoms::moz_opaque))
  {
    ErrorResult dummy;
    rv = UpdateContext(nullptr, JS::NullHandleValue, dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return rv;
}

void
HTMLCanvasElement::HandlePrintCallback(nsPresContext::nsPresContextType aType)
{
  // Only call the print callback here if 1) we're in a print testing mode or
  // print preview mode, 2) the canvas has a print callback and 3) the callback
  // hasn't already been called. For real printing the callback is handled in
  // nsSimplePageSequenceFrame::PrePrintNextPage.
  if ((aType == nsPresContext::eContext_PageLayout ||
       aType == nsPresContext::eContext_PrintPreview) &&
      !mPrintState && GetMozPrintCallback()) {
    DispatchPrintCallback(nullptr);
  }
}

nsresult
HTMLCanvasElement::DispatchPrintCallback(nsITimerCallback* aCallback)
{
  // For print reftests the context may not be initialized yet, so get a context
  // so mCurrentContext is set.
  if (!mCurrentContext) {
    nsresult rv;
    nsCOMPtr<nsISupports> context;
    rv = GetContext(NS_LITERAL_STRING("2d"), getter_AddRefs(context));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  mPrintState = new HTMLCanvasPrintState(this, mCurrentContext, aCallback);

  RefPtr<nsRunnableMethod<HTMLCanvasElement> > renderEvent =
        NewRunnableMethod(this, &HTMLCanvasElement::CallPrintCallback);
  return NS_DispatchToCurrentThread(renderEvent);
}

void
HTMLCanvasElement::CallPrintCallback()
{
  ErrorResult rv;
  GetMozPrintCallback()->Call(*mPrintState, rv);
}

void
HTMLCanvasElement::ResetPrintCallback()
{
  if (mPrintState) {
    mPrintState = nullptr;
  }
}

bool
HTMLCanvasElement::IsPrintCallbackDone()
{
  if (mPrintState == nullptr) {
    return true;
  }

  return mPrintState->mIsDone;
}

HTMLCanvasElement*
HTMLCanvasElement::GetOriginalCanvas()
{
  return mOriginalCanvas ? mOriginalCanvas.get() : this;
}

nsresult
HTMLCanvasElement::CopyInnerTo(Element* aDest)
{
  nsresult rv = nsGenericHTMLElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aDest->OwnerDoc()->IsStaticDocument()) {
    HTMLCanvasElement* dest = static_cast<HTMLCanvasElement*>(aDest);
    dest->mOriginalCanvas = this;

    nsCOMPtr<nsISupports> cxt;
    dest->GetContext(NS_LITERAL_STRING("2d"), getter_AddRefs(cxt));
    RefPtr<CanvasRenderingContext2D> context2d =
      static_cast<CanvasRenderingContext2D*>(cxt.get());
    if (context2d && !mPrintCallback) {
      CanvasImageSource source;
      source.SetAsHTMLCanvasElement() = this;
      ErrorResult err;
      context2d->DrawImage(source,
                           0.0, 0.0, err);
      rv = err.StealNSResult();
    }
  }
  return rv;
}

nsresult HTMLCanvasElement::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  if (aVisitor.mEvent->mClass == eMouseEventClass) {
    WidgetMouseEventBase* evt = (WidgetMouseEventBase*)aVisitor.mEvent;
    if (mCurrentContext) {
      nsIFrame *frame = GetPrimaryFrame();
      if (!frame)
        return NS_OK;
      nsPoint ptInRoot = nsLayoutUtils::GetEventCoordinatesRelativeTo(evt, frame);
      nsRect paddingRect = frame->GetContentRectRelativeToSelf();
      Point hitpoint;
      hitpoint.x = (ptInRoot.x - paddingRect.x) / AppUnitsPerCSSPixel();
      hitpoint.y = (ptInRoot.y - paddingRect.y) / AppUnitsPerCSSPixel();

      evt->region = mCurrentContext->GetHitRegion(hitpoint);
      aVisitor.mCanHandle = true;
    }
  }
  return nsGenericHTMLElement::PreHandleEvent(aVisitor);
}

nsChangeHint
HTMLCanvasElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                          int32_t aModType) const
{
  nsChangeHint retval =
    nsGenericHTMLElement::GetAttributeChangeHint(aAttribute, aModType);
  if (aAttribute == nsGkAtoms::width ||
      aAttribute == nsGkAtoms::height)
  {
    NS_UpdateHint(retval, NS_STYLE_HINT_REFLOW);
  } else if (aAttribute == nsGkAtoms::moz_opaque)
  {
    NS_UpdateHint(retval, NS_STYLE_HINT_VISUAL);
  }
  return retval;
}

bool
HTMLCanvasElement::ParseAttribute(int32_t aNamespaceID,
                                  nsIAtom* aAttribute,
                                  const nsAString& aValue,
                                  nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::width || aAttribute == nsGkAtoms::height)) {
    return aResult.ParseNonNegativeIntValue(aValue);
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}


// HTMLCanvasElement::toDataURL

NS_IMETHODIMP
HTMLCanvasElement::ToDataURL(const nsAString& aType, JS::Handle<JS::Value> aParams,
                             JSContext* aCx, nsAString& aDataURL)
{
  // do a trust check if this is a write-only canvas
  if (mWriteOnly && !nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  return ToDataURLImpl(aCx, aType, aParams, aDataURL);
}

void
HTMLCanvasElement::SetMozPrintCallback(PrintCallback* aCallback)
{
  mPrintCallback = aCallback;
}

PrintCallback*
HTMLCanvasElement::GetMozPrintCallback() const
{
  if (mOriginalCanvas) {
    return mOriginalCanvas->GetMozPrintCallback();
  }
  return mPrintCallback;
}

already_AddRefed<CanvasCaptureMediaStream>
HTMLCanvasElement::CaptureStream(const Optional<double>& aFrameRate,
                                 ErrorResult& aRv)
{
  if (IsWriteOnly()) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  nsPIDOMWindowInner* window = OwnerDoc()->GetInnerWindow();
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (!mCurrentContext) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return nullptr;
  }

  RefPtr<CanvasCaptureMediaStream> stream =
    CanvasCaptureMediaStream::CreateSourceStream(window, this);
  if (!stream) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  TrackID videoTrackId = 1;
  nsCOMPtr<nsIPrincipal> principal = NodePrincipal();
  nsresult rv =
    stream->Init(aFrameRate, videoTrackId, principal);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  stream->CreateDOMTrack(videoTrackId, MediaSegment::VIDEO,
                         new BasicUnstoppableTrackSource(principal));

  rv = RegisterFrameCaptureListener(stream->FrameCaptureListener());
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return stream.forget();
}

nsresult
HTMLCanvasElement::ExtractData(nsAString& aType,
                               const nsAString& aOptions,
                               nsIInputStream** aStream)
{
  return ImageEncoder::ExtractData(aType,
                                   aOptions,
                                   GetSize(),
                                   mCurrentContext,
                                   mAsyncCanvasRenderer,
                                   aStream);
}

nsresult
HTMLCanvasElement::ToDataURLImpl(JSContext* aCx,
                                 const nsAString& aMimeType,
                                 const JS::Value& aEncoderOptions,
                                 nsAString& aDataURL)
{
  nsIntSize size = GetWidthHeight();
  if (size.height == 0 || size.width == 0) {
    aDataURL = NS_LITERAL_STRING("data:,");
    return NS_OK;
  }

  nsAutoString type;
  nsContentUtils::ASCIIToLower(aMimeType, type);

  nsAutoString params;
  bool usingCustomParseOptions;
  nsresult rv =
    ParseParams(aCx, type, aEncoderOptions, params, &usingCustomParseOptions);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIInputStream> stream;
  rv = ExtractData(type, params, getter_AddRefs(stream));

  // If there are unrecognized custom parse options, we should fall back to
  // the default values for the encoder without any options at all.
  if (rv == NS_ERROR_INVALID_ARG && usingCustomParseOptions) {
    rv = ExtractData(type, EmptyString(), getter_AddRefs(stream));
  }

  NS_ENSURE_SUCCESS(rv, rv);

  // build data URL string
  aDataURL = NS_LITERAL_STRING("data:") + type + NS_LITERAL_STRING(";base64,");

  uint64_t count;
  rv = stream->Available(&count);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(count <= UINT32_MAX, NS_ERROR_FILE_TOO_BIG);

  return Base64EncodeInputStream(stream, aDataURL, (uint32_t)count, aDataURL.Length());
}

void
HTMLCanvasElement::ToBlob(JSContext* aCx,
                          FileCallback& aCallback,
                          const nsAString& aType,
                          JS::Handle<JS::Value> aParams,
                          ErrorResult& aRv)
{
  // do a trust check if this is a write-only canvas
  if (mWriteOnly && !nsContentUtils::IsCallerChrome()) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsCOMPtr<nsIGlobalObject> global = OwnerDoc()->GetScopeObject();
  MOZ_ASSERT(global);

  CanvasRenderingContextHelper::ToBlob(aCx, global, aCallback, aType,
                                       aParams, aRv);

}

OffscreenCanvas*
HTMLCanvasElement::TransferControlToOffscreen(ErrorResult& aRv)
{
  if (mCurrentContext) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  if (!mOffscreenCanvas) {
    nsIntSize sz = GetWidthHeight();
    RefPtr<AsyncCanvasRenderer> renderer = GetAsyncCanvasRenderer();
    renderer->SetWidth(sz.width);
    renderer->SetHeight(sz.height);

    nsCOMPtr<nsIGlobalObject> global =
      do_QueryInterface(OwnerDoc()->GetInnerWindow());
    mOffscreenCanvas = new OffscreenCanvas(global,
                                           sz.width,
                                           sz.height,
                                           GetCompositorBackendType(),
                                           renderer);
    if (mWriteOnly) {
      mOffscreenCanvas->SetWriteOnly();
    }

    if (!mContextObserver) {
      mContextObserver = new HTMLCanvasElementObserver(this);
    }
  } else {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }

  return mOffscreenCanvas;
}

already_AddRefed<File>
HTMLCanvasElement::MozGetAsFile(const nsAString& aName,
                                const nsAString& aType,
                                ErrorResult& aRv)
{
  nsCOMPtr<nsISupports> file;
  aRv = MozGetAsFile(aName, aType, getter_AddRefs(file));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsCOMPtr<nsIDOMBlob> blob = do_QueryInterface(file);
  RefPtr<Blob> domBlob = static_cast<Blob*>(blob.get());
  MOZ_ASSERT(domBlob->IsFile());
  return domBlob->ToFile();
}

NS_IMETHODIMP
HTMLCanvasElement::MozGetAsFile(const nsAString& aName,
                                const nsAString& aType,
                                nsISupports** aResult)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eMozGetAsFile);

  // do a trust check if this is a write-only canvas
  if ((mWriteOnly) &&
      !nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  return MozGetAsBlobImpl(aName, aType, aResult);
}

nsresult
HTMLCanvasElement::MozGetAsBlobImpl(const nsAString& aName,
                                    const nsAString& aType,
                                    nsISupports** aResult)
{
  nsCOMPtr<nsIInputStream> stream;
  nsAutoString type(aType);
  nsresult rv = ExtractData(type, EmptyString(), getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  uint64_t imgSize;
  rv = stream->Available(&imgSize);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(imgSize <= UINT32_MAX, NS_ERROR_FILE_TOO_BIG);

  void* imgData = nullptr;
  rv = NS_ReadInputStreamToBuffer(stream, &imgData, (uint32_t)imgSize);
  NS_ENSURE_SUCCESS(rv, rv);

  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  if (cx) {
    JS_updateMallocCounter(cx, imgSize);
  }

  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(OwnerDoc()->GetScopeObject());

  // The File takes ownership of the buffer
  nsCOMPtr<nsIDOMBlob> file =
    File::CreateMemoryFile(win, imgData, (uint32_t)imgSize, aName, type,
                           PR_Now());

  file.forget(aResult);
  return NS_OK;
}

nsresult
HTMLCanvasElement::GetContext(const nsAString& aContextId,
                              nsISupports** aContext)
{
  ErrorResult rv;
  *aContext = GetContext(nullptr, aContextId, JS::NullHandleValue, rv).take();
  return rv.StealNSResult();
}

already_AddRefed<nsISupports>
HTMLCanvasElement::GetContext(JSContext* aCx,
                              const nsAString& aContextId,
                              JS::Handle<JS::Value> aContextOptions,
                              ErrorResult& aRv)
{
  if (mOffscreenCanvas) {
    return nullptr;
  }

  return CanvasRenderingContextHelper::GetContext(aCx, aContextId,
    aContextOptions.isObject() ? aContextOptions : JS::NullHandleValue,
    aRv);
}

NS_IMETHODIMP
HTMLCanvasElement::MozGetIPCContext(const nsAString& aContextId,
                                    nsISupports **aContext)
{
  if(!nsContentUtils::IsCallerChrome()) {
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // We only support 2d shmem contexts for now.
  if (!aContextId.EqualsLiteral("2d"))
    return NS_ERROR_INVALID_ARG;

  CanvasContextType contextType = CanvasContextType::Canvas2D;

  if (!mCurrentContext) {
    // This canvas doesn't have a context yet.

    RefPtr<nsICanvasRenderingContextInternal> context;
    context = CreateContext(contextType);
    if (!context) {
      *aContext = nullptr;
      return NS_OK;
    }

    mCurrentContext = context;
    mCurrentContext->SetIsIPC(true);
    mCurrentContextType = contextType;

    ErrorResult dummy;
    nsresult rv = UpdateContext(nullptr, JS::NullHandleValue, dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // We already have a context of some type.
    if (contextType != mCurrentContextType)
      return NS_ERROR_INVALID_ARG;
  }

  NS_ADDREF (*aContext = mCurrentContext);
  return NS_OK;
}


nsIntSize
HTMLCanvasElement::GetSize()
{
  return GetWidthHeight();
}

bool
HTMLCanvasElement::IsWriteOnly()
{
  return mWriteOnly;
}

void
HTMLCanvasElement::SetWriteOnly()
{
  mWriteOnly = true;
}

void
HTMLCanvasElement::InvalidateCanvasContent(const gfx::Rect* damageRect)
{
  // We don't need to flush anything here; if there's no frame or if
  // we plan to reframe we don't need to invalidate it anyway.
  nsIFrame *frame = GetPrimaryFrame();
  if (!frame)
    return;

  ActiveLayerTracker::NotifyContentChange(frame);

  Layer* layer = nullptr;
  if (damageRect) {
    nsIntSize size = GetWidthHeight();
    if (size.width != 0 && size.height != 0) {

      gfx::Rect realRect(*damageRect);
      realRect.RoundOut();

      // then make it a nsIntRect
      nsIntRect invalRect(realRect.X(), realRect.Y(),
                          realRect.Width(), realRect.Height());

      layer = frame->InvalidateLayer(nsDisplayItem::TYPE_CANVAS, &invalRect);
    }
  } else {
    layer = frame->InvalidateLayer(nsDisplayItem::TYPE_CANVAS);
  }
  if (layer) {
    static_cast<CanvasLayer*>(layer)->Updated();
  }

  /*
   * Treat canvas invalidations as animation activity for JS. Frequently
   * invalidating a canvas will feed into heuristics and cause JIT code to be
   * kept around longer, for smoother animations.
   */
  nsCOMPtr<nsIGlobalObject> global =
    do_QueryInterface(OwnerDoc()->GetInnerWindow());

  if (global) {
    if (JSObject *obj = global->GetGlobalJSObject()) {
      js::NotifyAnimationActivity(obj);
    }
  }
}

void
HTMLCanvasElement::InvalidateCanvas()
{
  // We don't need to flush anything here; if there's no frame or if
  // we plan to reframe we don't need to invalidate it anyway.
  nsIFrame *frame = GetPrimaryFrame();
  if (!frame)
    return;

  frame->InvalidateFrame();
}

int32_t
HTMLCanvasElement::CountContexts()
{
  if (mCurrentContext)
    return 1;

  return 0;
}

nsICanvasRenderingContextInternal *
HTMLCanvasElement::GetContextAtIndex(int32_t index)
{
  if (mCurrentContext && index == 0)
    return mCurrentContext;

  return nullptr;
}

bool
HTMLCanvasElement::GetIsOpaque()
{
  if (mCurrentContext) {
    return mCurrentContext->GetIsOpaque();
  }

  return GetOpaqueAttr();
}

bool
HTMLCanvasElement::GetOpaqueAttr()
{
  return HasAttr(kNameSpaceID_None, nsGkAtoms::moz_opaque);
}

already_AddRefed<Layer>
HTMLCanvasElement::GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                  Layer *aOldLayer,
                                  LayerManager *aManager)
{
  // The address of sOffscreenCanvasLayerUserDataDummy is used as the user
  // data key for retained LayerManagers managed by FrameLayerBuilder.
  // We don't much care about what value in it, so just assign a dummy
  // value for it.
  static uint8_t sOffscreenCanvasLayerUserDataDummy = 0;

  if (mCurrentContext) {
    return mCurrentContext->GetCanvasLayer(aBuilder, aOldLayer, aManager);
  }

  if (mOffscreenCanvas) {
    if (!mResetLayer &&
        aOldLayer && aOldLayer->HasUserData(&sOffscreenCanvasLayerUserDataDummy)) {
      RefPtr<Layer> ret = aOldLayer;
      return ret.forget();
    }

    RefPtr<CanvasLayer> layer = aManager->CreateCanvasLayer();
    if (!layer) {
      NS_WARNING("CreateCanvasLayer failed!");
      return nullptr;
    }

    LayerUserData* userData = nullptr;
    layer->SetUserData(&sOffscreenCanvasLayerUserDataDummy, userData);

    CanvasLayer::Data data;
    data.mRenderer = GetAsyncCanvasRenderer();
    data.mSize = GetWidthHeight();
    layer->Initialize(data);

    layer->Updated();
    return layer.forget();
  }

  return nullptr;
}

bool
HTMLCanvasElement::ShouldForceInactiveLayer(LayerManager* aManager)
{
  if (mCurrentContext) {
    return mCurrentContext->ShouldForceInactiveLayer(aManager);
  }

  if (mOffscreenCanvas) {
    // TODO: We should handle offscreen canvas case.
    return false;
  }

  return true;
}

void
HTMLCanvasElement::MarkContextClean()
{
  if (!mCurrentContext)
    return;

  mCurrentContext->MarkContextClean();
}

void
HTMLCanvasElement::MarkContextCleanForFrameCapture()
{
  if (!mCurrentContext)
    return;

  mCurrentContext->MarkContextCleanForFrameCapture();
}

bool
HTMLCanvasElement::IsContextCleanForFrameCapture()
{
  return mCurrentContext && mCurrentContext->IsContextCleanForFrameCapture();
}

nsresult
HTMLCanvasElement::RegisterFrameCaptureListener(FrameCaptureListener* aListener)
{
  WeakPtr<FrameCaptureListener> listener = aListener;

  if (mRequestedFrameListeners.Contains(listener)) {
    return NS_OK;
  }

  if (!mRequestedFrameRefreshObserver) {
    nsIDocument* doc = OwnerDoc();
    if (!doc) {
      return NS_ERROR_FAILURE;
    }

    while (doc->GetParentDocument()) {
      doc = doc->GetParentDocument();
    }

    nsIPresShell* shell = doc->GetShell();
    if (!shell) {
      return NS_ERROR_FAILURE;
    }

    nsPresContext* context = shell->GetPresContext();
    if (!context) {
      return NS_ERROR_FAILURE;
    }

    context = context->GetRootPresContext();
    if (!context) {
      return NS_ERROR_FAILURE;
    }

    nsRefreshDriver* driver = context->RefreshDriver();
    if (!driver) {
      return NS_ERROR_FAILURE;
    }

    mRequestedFrameRefreshObserver =
      new RequestedFrameRefreshObserver(this, driver);
  }

  mRequestedFrameListeners.AppendElement(listener);
  mRequestedFrameRefreshObserver->Register();
  return NS_OK;
}

bool
HTMLCanvasElement::IsFrameCaptureRequested() const
{
  for (WeakPtr<FrameCaptureListener> listener : mRequestedFrameListeners) {
    if (!listener) {
      continue;
    }

    if (listener->FrameCaptureRequested()) {
      return true;
    }
  }
  return false;
}

void
HTMLCanvasElement::SetFrameCapture(already_AddRefed<SourceSurface> aSurface)
{
  RefPtr<SourceSurface> surface = aSurface;
  RefPtr<SourceSurfaceImage> image = new SourceSurfaceImage(surface->GetSize(), surface);

  // Loop backwards to allow removing elements in the loop.
  for (int i = mRequestedFrameListeners.Length() - 1; i >= 0; --i) {
    WeakPtr<FrameCaptureListener> listener = mRequestedFrameListeners[i];
    if (!listener) {
      // listener was destroyed. Remove it from the list.
      mRequestedFrameListeners.RemoveElementAt(i);
      continue;
    }

    RefPtr<Image> imageRefCopy = image.get();
    listener->NewFrame(imageRefCopy.forget());
  }

  if (mRequestedFrameListeners.IsEmpty()) {
    mRequestedFrameRefreshObserver->Unregister();
  }
}

already_AddRefed<SourceSurface>
HTMLCanvasElement::GetSurfaceSnapshot(bool* aPremultAlpha)
{
  if (!mCurrentContext)
    return nullptr;

  return mCurrentContext->GetSurfaceSnapshot(aPremultAlpha);
}

AsyncCanvasRenderer*
HTMLCanvasElement::GetAsyncCanvasRenderer()
{
  if (!mAsyncCanvasRenderer) {
    mAsyncCanvasRenderer = new AsyncCanvasRenderer();
    mAsyncCanvasRenderer->mHTMLCanvasElement = this;
  }

  return mAsyncCanvasRenderer;
}

layers::LayersBackend
HTMLCanvasElement::GetCompositorBackendType() const
{
  nsIWidget* docWidget = nsContentUtils::WidgetForDocument(OwnerDoc());
  if (docWidget) {
    layers::LayerManager* layerManager = docWidget->GetLayerManager();
    return layerManager->GetCompositorBackendType();
  }

  return LayersBackend::LAYERS_NONE;
}

void
HTMLCanvasElement::OnVisibilityChange()
{
  if (OwnerDoc()->Hidden()) {
    return;
  }

  if (mOffscreenCanvas) {
    class Runnable final : public CancelableRunnable
    {
    public:
      explicit Runnable(AsyncCanvasRenderer* aRenderer)
        : mRenderer(aRenderer)
      {}

      NS_IMETHOD Run()
      {
        if (mRenderer && mRenderer->mContext) {
          mRenderer->mContext->OnVisibilityChange();
        }

        return NS_OK;
      }

      void Revoke()
      {
        mRenderer = nullptr;
      }

    private:
      RefPtr<AsyncCanvasRenderer> mRenderer;
    };

    RefPtr<nsIRunnable> runnable = new Runnable(mAsyncCanvasRenderer);
    nsCOMPtr<nsIThread> activeThread = mAsyncCanvasRenderer->GetActiveThread();
    if (activeThread) {
      activeThread->Dispatch(runnable, nsIThread::DISPATCH_NORMAL);
    }
    return;
  }

  if (mCurrentContext) {
    mCurrentContext->OnVisibilityChange();
  }
}

void
HTMLCanvasElement::OnMemoryPressure()
{
  if (mOffscreenCanvas) {
    class Runnable final : public CancelableRunnable
    {
    public:
      explicit Runnable(AsyncCanvasRenderer* aRenderer)
        : mRenderer(aRenderer)
      {}

      NS_IMETHOD Run()
      {
        if (mRenderer && mRenderer->mContext) {
          mRenderer->mContext->OnMemoryPressure();
        }

        return NS_OK;
      }

      void Revoke()
      {
        mRenderer = nullptr;
      }

    private:
      RefPtr<AsyncCanvasRenderer> mRenderer;
    };

    RefPtr<nsIRunnable> runnable = new Runnable(mAsyncCanvasRenderer);
    nsCOMPtr<nsIThread> activeThread = mAsyncCanvasRenderer->GetActiveThread();
    if (activeThread) {
      activeThread->Dispatch(runnable, nsIThread::DISPATCH_NORMAL);
    }
    return;
  }

  if (mCurrentContext) {
    mCurrentContext->OnMemoryPressure();
  }
}

/* static */ void
HTMLCanvasElement::SetAttrFromAsyncCanvasRenderer(AsyncCanvasRenderer *aRenderer)
{
  HTMLCanvasElement *element = aRenderer->mHTMLCanvasElement;
  if (!element) {
    return;
  }

  if (element->GetWidthHeight() == aRenderer->GetSize()) {
    return;
  }

  gfx::IntSize asyncCanvasSize = aRenderer->GetSize();

  ErrorResult rv;
  element->SetUnsignedIntAttr(nsGkAtoms::width, asyncCanvasSize.width, rv);
  if (rv.Failed()) {
    NS_WARNING("Failed to set width attribute to a canvas element asynchronously.");
  }

  element->SetUnsignedIntAttr(nsGkAtoms::height, asyncCanvasSize.height, rv);
  if (rv.Failed()) {
    NS_WARNING("Failed to set height attribute to a canvas element asynchronously.");
  }

  element->mResetLayer = true;
}

/* static */ void
HTMLCanvasElement::InvalidateFromAsyncCanvasRenderer(AsyncCanvasRenderer *aRenderer)
{
  HTMLCanvasElement *element = aRenderer->mHTMLCanvasElement;
  if (!element) {
    return;
  }

  element->InvalidateCanvasContent(nullptr);
}

} // namespace dom
} // namespace mozilla
