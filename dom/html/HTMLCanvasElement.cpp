/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLCanvasElement.h"

#include "ImageEncoder.h"
#include "ImageLayers.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "Layers.h"
#include "MediaTrackGraph.h"
#include "mozilla/Assertions.h"
#include "mozilla/Base64.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/CanvasCaptureMediaStream.h"
#include "mozilla/dom/CanvasRenderingContext2D.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/HTMLCanvasElementBinding.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "mozilla/dom/MouseEvent.h"
#include "mozilla/dom/OffscreenCanvas.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/layers/AsyncCanvasRenderer.h"
#include "mozilla/layers/OOPCanvasRenderer.h"
#include "mozilla/layers/WebRenderCanvasRenderer.h"
#include "mozilla/layers/WebRenderUserData.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "mozilla/webgpu/CanvasContext.h"
#include "nsAttrValueInlines.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"
#include "nsDOMJSUtils.h"
#include "nsITimer.h"
#include "nsJSUtils.h"
#include "nsLayoutUtils.h"
#include "nsMathUtils.h"
#include "nsNetUtil.h"
#include "nsRefreshDriver.h"
#include "nsStreamUtils.h"
#include "ActiveLayerTracker.h"
#include "CanvasUtils.h"
#include "VRManagerChild.h"
#include "ClientWebGLContext.h"

using namespace mozilla::layers;
using namespace mozilla::gfx;

NS_IMPL_NS_NEW_HTML_ELEMENT(Canvas)

namespace mozilla {
namespace dom {

class RequestedFrameRefreshObserver : public nsARefreshObserver {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RequestedFrameRefreshObserver, override)

 public:
  RequestedFrameRefreshObserver(HTMLCanvasElement* const aOwningElement,
                                nsRefreshDriver* aRefreshDriver,
                                bool aReturnPlaceholderData)
      : mRegistered(false),
        mReturnPlaceholderData(aReturnPlaceholderData),
        mOwningElement(aOwningElement),
        mRefreshDriver(aRefreshDriver) {
    MOZ_ASSERT(mOwningElement);
  }

  static already_AddRefed<DataSourceSurface> CopySurface(
      const RefPtr<SourceSurface>& aSurface, bool aReturnPlaceholderData) {
    RefPtr<DataSourceSurface> data = aSurface->GetDataSurface();
    if (!data) {
      return nullptr;
    }

    DataSourceSurface::ScopedMap read(data, DataSourceSurface::READ);
    if (!read.IsMapped()) {
      return nullptr;
    }

    RefPtr<DataSourceSurface> copy = Factory::CreateDataSourceSurfaceWithStride(
        data->GetSize(), data->GetFormat(), read.GetStride());
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

    if (aReturnPlaceholderData) {
      // If returning placeholder data, fill the frame copy with white pixels.
      memset(write.GetData(), 0xFF, write.GetStride() * copy->GetSize().height);
    } else {
      memcpy(write.GetData(), read.GetData(),
             write.GetStride() * copy->GetSize().height);
    }

    return copy.forget();
  }

  void SetReturnPlaceholderData(bool aReturnPlaceholderData) {
    mReturnPlaceholderData = aReturnPlaceholderData;
  }

  void WillRefresh(TimeStamp aTime) override {
    MOZ_ASSERT(NS_IsMainThread());

    AUTO_PROFILER_LABEL("RequestedFrameRefreshObserver::WillRefresh", OTHER);

    if (!mOwningElement) {
      return;
    }

    if (mOwningElement->IsWriteOnly()) {
      return;
    }

    if (mOwningElement->IsContextCleanForFrameCapture()) {
      return;
    }

    mOwningElement->ProcessDestroyedFrameListeners();

    if (!mOwningElement->IsFrameCaptureRequested()) {
      return;
    }

    RefPtr<SourceSurface> snapshot;
    {
      AUTO_PROFILER_LABEL(
          "RequestedFrameRefreshObserver::WillRefresh:GetSnapshot", OTHER);
      snapshot = mOwningElement->GetSurfaceSnapshot(nullptr);
      if (!snapshot) {
        return;
      }
    }

    RefPtr<DataSourceSurface> copy;
    {
      AUTO_PROFILER_LABEL(
          "RequestedFrameRefreshObserver::WillRefresh:CopySurface", OTHER);
      copy = CopySurface(snapshot, mReturnPlaceholderData);
      if (!copy) {
        return;
      }
    }

    {
      AUTO_PROFILER_LABEL("RequestedFrameRefreshObserver::WillRefresh:SetFrame",
                          OTHER);
      mOwningElement->SetFrameCapture(copy.forget(), aTime);
      mOwningElement->MarkContextCleanForFrameCapture();
    }
  }

  void DetachFromRefreshDriver() {
    MOZ_ASSERT(mOwningElement);
    MOZ_ASSERT(mRefreshDriver);

    Unregister();
    mRefreshDriver = nullptr;
  }

  void Register() {
    if (mRegistered) {
      return;
    }

    MOZ_ASSERT(mRefreshDriver);
    if (mRefreshDriver) {
      mRefreshDriver->AddRefreshObserver(this, FlushType::Display);
      mRegistered = true;
    }
  }

  void Unregister() {
    if (!mRegistered) {
      return;
    }

    MOZ_ASSERT(mRefreshDriver);
    if (mRefreshDriver) {
      mRefreshDriver->RemoveRefreshObserver(this, FlushType::Display);
      mRegistered = false;
    }
  }

 private:
  virtual ~RequestedFrameRefreshObserver() {
    MOZ_ASSERT(!mRefreshDriver);
    MOZ_ASSERT(!mRegistered);
  }

  bool mRegistered;
  bool mReturnPlaceholderData;
  HTMLCanvasElement* const mOwningElement;
  RefPtr<nsRefreshDriver> mRefreshDriver;
};

// ---------------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(HTMLCanvasPrintState, mCanvas, mContext,
                                      mCallback)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(HTMLCanvasPrintState, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(HTMLCanvasPrintState, Release)

HTMLCanvasPrintState::HTMLCanvasPrintState(
    HTMLCanvasElement* aCanvas, nsICanvasRenderingContextInternal* aContext,
    nsITimerCallback* aCallback)
    : mIsDone(false),
      mPendingNotify(false),
      mCanvas(aCanvas),
      mContext(aContext),
      mCallback(aCallback) {}

HTMLCanvasPrintState::~HTMLCanvasPrintState() = default;

/* virtual */
JSObject* HTMLCanvasPrintState::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return MozCanvasPrintState_Binding::Wrap(aCx, this, aGivenProto);
}

nsISupports* HTMLCanvasPrintState::Context() const { return mContext; }

void HTMLCanvasPrintState::Done() {
  if (!mPendingNotify && !mIsDone) {
    // The canvas needs to be invalidated for printing reftests on linux to
    // work.
    if (mCanvas) {
      mCanvas->InvalidateCanvas();
    }
    RefPtr<nsRunnableMethod<HTMLCanvasPrintState>> doneEvent =
        NewRunnableMethod("dom::HTMLCanvasPrintState::NotifyDone", this,
                          &HTMLCanvasPrintState::NotifyDone);
    if (NS_SUCCEEDED(NS_DispatchToCurrentThread(doneEvent))) {
      mPendingNotify = true;
    }
  }
}

void HTMLCanvasPrintState::NotifyDone() {
  mIsDone = true;
  mPendingNotify = false;
  if (mCallback) {
    mCallback->Notify(nullptr);
  }
}

// ---------------------------------------------------------------------------

HTMLCanvasElementObserver::HTMLCanvasElementObserver(
    HTMLCanvasElement* aElement)
    : mElement(aElement) {
  RegisterVisibilityChangeEvent();
  RegisterObserverEvents();
}

HTMLCanvasElementObserver::~HTMLCanvasElementObserver() { Destroy(); }

void HTMLCanvasElementObserver::Destroy() {
  UnregisterObserverEvents();
  UnregisterVisibilityChangeEvent();
  mElement = nullptr;
}

void HTMLCanvasElementObserver::RegisterVisibilityChangeEvent() {
  if (!mElement) {
    return;
  }

  Document* document = mElement->OwnerDoc();
  document->AddSystemEventListener(NS_LITERAL_STRING("visibilitychange"), this,
                                   true, false);
}

void HTMLCanvasElementObserver::UnregisterVisibilityChangeEvent() {
  if (!mElement) {
    return;
  }

  Document* document = mElement->OwnerDoc();
  document->RemoveSystemEventListener(NS_LITERAL_STRING("visibilitychange"),
                                      this, true);
}

void HTMLCanvasElementObserver::RegisterObserverEvents() {
  if (!mElement) {
    return;
  }

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();

  MOZ_ASSERT(observerService);

  if (observerService) {
    observerService->AddObserver(this, "memory-pressure", false);
    observerService->AddObserver(this, "canvas-device-reset", false);
  }
}

void HTMLCanvasElementObserver::UnregisterObserverEvents() {
  if (!mElement) {
    return;
  }

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();

  // Do not assert on observerService here. This might be triggered by
  // the cycle collector at a late enough time, that XPCOM services are
  // no longer available. See bug 1029504.
  if (observerService) {
    observerService->RemoveObserver(this, "memory-pressure");
    observerService->RemoveObserver(this, "canvas-device-reset");
  }
}

NS_IMETHODIMP
HTMLCanvasElementObserver::Observe(nsISupports*, const char* aTopic,
                                   const char16_t*) {
  if (!mElement) {
    return NS_OK;
  }

  if (strcmp(aTopic, "memory-pressure") == 0) {
    mElement->OnMemoryPressure();
  } else if (strcmp(aTopic, "canvas-device-reset") == 0) {
    mElement->OnDeviceReset();
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLCanvasElementObserver::HandleEvent(Event* aEvent) {
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

HTMLCanvasElement::HTMLCanvasElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsGenericHTMLElement(std::move(aNodeInfo)),
      mResetLayer(true),
      mMaybeModified(false),
      mWriteOnly(false) {}

HTMLCanvasElement::~HTMLCanvasElement() {
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
                                   mCurrentContext, mPrintCallback, mPrintState,
                                   mOriginalCanvas, mOffscreenCanvas)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(HTMLCanvasElement,
                                               nsGenericHTMLElement)

NS_IMPL_ELEMENT_CLONE(HTMLCanvasElement)

/* virtual */
JSObject* HTMLCanvasElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return HTMLCanvasElement_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<nsICanvasRenderingContextInternal>
HTMLCanvasElement::CreateContext(CanvasContextType aContextType) {
  // Note that the compositor backend will be LAYERS_NONE if there is no widget.
  RefPtr<nsICanvasRenderingContextInternal> ret =
      CreateContextHelper(aContextType, GetCompositorBackendType());

  // Add Observer for webgl canvas.
  if (aContextType == CanvasContextType::WebGL1 ||
      aContextType == CanvasContextType::WebGL2 ||
      aContextType == CanvasContextType::Canvas2D) {
    if (!mContextObserver) {
      mContextObserver = new HTMLCanvasElementObserver(this);
    }
  }

  ret->SetCanvasElement(this);
  return ret.forget();
}

nsIntSize HTMLCanvasElement::GetWidthHeight() {
  nsIntSize size(DEFAULT_CANVAS_WIDTH, DEFAULT_CANVAS_HEIGHT);
  const nsAttrValue* value;

  if ((value = GetParsedAttr(nsGkAtoms::width)) &&
      value->Type() == nsAttrValue::eInteger) {
    size.width = value->GetIntegerValue();
  }

  if ((value = GetParsedAttr(nsGkAtoms::height)) &&
      value->Type() == nsAttrValue::eInteger) {
    size.height = value->GetIntegerValue();
  }

  MOZ_ASSERT(size.width >= 0 && size.height >= 0,
             "we should've required <canvas> width/height attrs to be "
             "unsigned (non-negative) values");

  return size;
}

nsresult HTMLCanvasElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                         const nsAttrValue* aValue,
                                         const nsAttrValue* aOldValue,
                                         nsIPrincipal* aSubjectPrincipal,
                                         bool aNotify) {
  AfterMaybeChangeAttr(aNamespaceID, aName, aNotify);

  return nsGenericHTMLElement::AfterSetAttr(
      aNamespaceID, aName, aValue, aOldValue, aSubjectPrincipal, aNotify);
}

nsresult HTMLCanvasElement::OnAttrSetButNotChanged(
    int32_t aNamespaceID, nsAtom* aName, const nsAttrValueOrString& aValue,
    bool aNotify) {
  AfterMaybeChangeAttr(aNamespaceID, aName, aNotify);

  return nsGenericHTMLElement::OnAttrSetButNotChanged(aNamespaceID, aName,
                                                      aValue, aNotify);
}

void HTMLCanvasElement::AfterMaybeChangeAttr(int32_t aNamespaceID,
                                             nsAtom* aName, bool aNotify) {
  if (mCurrentContext && aNamespaceID == kNameSpaceID_None &&
      (aName == nsGkAtoms::width || aName == nsGkAtoms::height ||
       aName == nsGkAtoms::moz_opaque)) {
    ErrorResult dummy;
    UpdateContext(nullptr, JS::NullHandleValue, dummy);
  }
}

void HTMLCanvasElement::HandlePrintCallback(nsPresContext* aPresContext) {
  // Only call the print callback here if 1) we're in a print testing mode or
  // print preview mode, 2) the canvas has a print callback and 3) the callback
  // hasn't already been called. For real printing the callback is handled in
  // nsSimplePageSequenceFrame::PrePrintNextPage.
  if ((aPresContext->Type() == nsPresContext::eContext_PageLayout ||
       aPresContext->Type() == nsPresContext::eContext_PrintPreview) &&
      !mPrintState && GetMozPrintCallback()) {
    DispatchPrintCallback(nullptr);
  }
}

nsresult HTMLCanvasElement::DispatchPrintCallback(nsITimerCallback* aCallback) {
  // For print reftests the context may not be initialized yet, so get a context
  // so mCurrentContext is set.
  if (!mCurrentContext) {
    nsresult rv;
    nsCOMPtr<nsISupports> context;
    rv = GetContext(NS_LITERAL_STRING("2d"), getter_AddRefs(context));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  mPrintState = new HTMLCanvasPrintState(this, mCurrentContext, aCallback);

  RefPtr<nsRunnableMethod<HTMLCanvasElement>> renderEvent =
      NewRunnableMethod("dom::HTMLCanvasElement::CallPrintCallback", this,
                        &HTMLCanvasElement::CallPrintCallback);
  return OwnerDoc()->Dispatch(TaskCategory::Other, renderEvent.forget());
}

MOZ_CAN_RUN_SCRIPT
void HTMLCanvasElement::CallPrintCallback() {
  RefPtr<PrintCallback> callback = GetMozPrintCallback();
  RefPtr<HTMLCanvasPrintState> state = mPrintState;
  callback->Call(*state);
}

void HTMLCanvasElement::ResetPrintCallback() {
  if (mPrintState) {
    mPrintState = nullptr;
  }
}

bool HTMLCanvasElement::IsPrintCallbackDone() {
  if (mPrintState == nullptr) {
    return true;
  }

  return mPrintState->mIsDone;
}

HTMLCanvasElement* HTMLCanvasElement::GetOriginalCanvas() {
  return mOriginalCanvas ? mOriginalCanvas.get() : this;
}

nsresult HTMLCanvasElement::CopyInnerTo(HTMLCanvasElement* aDest) {
  nsresult rv = nsGenericHTMLElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aDest->OwnerDoc()->IsStaticDocument()) {
    // The Firefox print preview code can create a static clone from an
    // existing static clone, so we may not be the original 'canvas' element.
    aDest->mOriginalCanvas = GetOriginalCanvas();

    // We make sure that the canvas is not zero sized since that would cause
    // the DrawImage call below to return an error, which would cause printing
    // to fail.
    nsIntSize size = GetWidthHeight();
    if (size.height > 0 && size.width > 0) {
      nsCOMPtr<nsISupports> cxt;
      aDest->GetContext(NS_LITERAL_STRING("2d"), getter_AddRefs(cxt));
      RefPtr<CanvasRenderingContext2D> context2d =
          static_cast<CanvasRenderingContext2D*>(cxt.get());
      if (context2d && !mPrintCallback) {
        CanvasImageSource source;
        source.SetAsHTMLCanvasElement() = this;
        ErrorResult err;
        context2d->DrawImage(source, 0.0, 0.0, err);
        rv = err.StealNSResult();
      }
    }
  }
  return rv;
}

void HTMLCanvasElement::GetEventTargetParent(EventChainPreVisitor& aVisitor) {
  if (aVisitor.mEvent->mClass == eMouseEventClass) {
    WidgetMouseEventBase* evt = (WidgetMouseEventBase*)aVisitor.mEvent;
    if (mCurrentContext) {
      nsIFrame* frame = GetPrimaryFrame();
      if (!frame) {
        return;
      }
      nsPoint ptInRoot =
          nsLayoutUtils::GetEventCoordinatesRelativeTo(evt, RelativeTo{frame});
      nsRect paddingRect = frame->GetContentRectRelativeToSelf();
      Point hitpoint;
      hitpoint.x = (ptInRoot.x - paddingRect.x) / AppUnitsPerCSSPixel();
      hitpoint.y = (ptInRoot.y - paddingRect.y) / AppUnitsPerCSSPixel();

      evt->mRegion = mCurrentContext->GetHitRegion(hitpoint);
      aVisitor.mCanHandle = true;
    }
  }
  nsGenericHTMLElement::GetEventTargetParent(aVisitor);
}

nsChangeHint HTMLCanvasElement::GetAttributeChangeHint(const nsAtom* aAttribute,
                                                       int32_t aModType) const {
  nsChangeHint retval =
      nsGenericHTMLElement::GetAttributeChangeHint(aAttribute, aModType);
  if (aAttribute == nsGkAtoms::width || aAttribute == nsGkAtoms::height) {
    retval |= NS_STYLE_HINT_REFLOW;
  } else if (aAttribute == nsGkAtoms::moz_opaque) {
    retval |= NS_STYLE_HINT_VISUAL;
  }
  return retval;
}

bool HTMLCanvasElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                       const nsAString& aValue,
                                       nsIPrincipal* aMaybeScriptedPrincipal,
                                       nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::width || aAttribute == nsGkAtoms::height)) {
    return aResult.ParseNonNegativeIntValue(aValue);
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void HTMLCanvasElement::ToDataURL(JSContext* aCx, const nsAString& aType,
                                  JS::Handle<JS::Value> aParams,
                                  nsAString& aDataURL,
                                  nsIPrincipal& aSubjectPrincipal,
                                  ErrorResult& aRv) {
  // mWriteOnly check is redundant, but optimizes for the common case.
  if (mWriteOnly && !CallerCanRead(aCx)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsresult rv = ToDataURLImpl(aCx, aSubjectPrincipal, aType, aParams, aDataURL);
  if (NS_FAILED(rv)) {
    aDataURL.AssignLiteral("data:,");
  }
}

void HTMLCanvasElement::SetMozPrintCallback(PrintCallback* aCallback) {
  mPrintCallback = aCallback;
}

PrintCallback* HTMLCanvasElement::GetMozPrintCallback() const {
  if (mOriginalCanvas) {
    return mOriginalCanvas->GetMozPrintCallback();
  }
  return mPrintCallback;
}

class CanvasCaptureTrackSource : public MediaStreamTrackSource {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CanvasCaptureTrackSource,
                                           MediaStreamTrackSource)

  CanvasCaptureTrackSource(nsIPrincipal* aPrincipal,
                           CanvasCaptureMediaStream* aCaptureStream)
      : MediaStreamTrackSource(aPrincipal, nsString()),
        mCaptureStream(aCaptureStream) {}

  MediaSourceEnum GetMediaSource() const override {
    return MediaSourceEnum::Other;
  }

  void Stop() override {
    if (!mCaptureStream) {
      NS_ERROR("No stream");
      return;
    }

    mCaptureStream->StopCapture();
  }

  void Disable() override {}

  void Enable() override {}

 private:
  virtual ~CanvasCaptureTrackSource() = default;

  RefPtr<CanvasCaptureMediaStream> mCaptureStream;
};

NS_IMPL_ADDREF_INHERITED(CanvasCaptureTrackSource, MediaStreamTrackSource)
NS_IMPL_RELEASE_INHERITED(CanvasCaptureTrackSource, MediaStreamTrackSource)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CanvasCaptureTrackSource)
NS_INTERFACE_MAP_END_INHERITING(MediaStreamTrackSource)
NS_IMPL_CYCLE_COLLECTION_INHERITED(CanvasCaptureTrackSource,
                                   MediaStreamTrackSource, mCaptureStream)

already_AddRefed<CanvasCaptureMediaStream> HTMLCanvasElement::CaptureStream(
    const Optional<double>& aFrameRate, nsIPrincipal& aSubjectPrincipal,
    ErrorResult& aRv) {
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

  auto stream = MakeRefPtr<CanvasCaptureMediaStream>(window, this);

  nsCOMPtr<nsIPrincipal> principal = NodePrincipal();
  nsresult rv = stream->Init(aFrameRate, principal);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  RefPtr<MediaStreamTrack> track =
      new VideoStreamTrack(window, stream->GetSourceStream(),
                           new CanvasCaptureTrackSource(principal, stream));
  stream->AddTrackInternal(track);

  // Check site-specific permission and display prompt if appropriate.
  // If no permission, arrange for the frame capture listener to return
  // all-white, opaque image data.
  bool usePlaceholder = !CanvasUtils::IsImageExtractionAllowed(
      OwnerDoc(), nsContentUtils::GetCurrentJSContext(), aSubjectPrincipal);

  rv = RegisterFrameCaptureListener(stream->FrameCaptureListener(),
                                    usePlaceholder);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return stream.forget();
}

nsresult HTMLCanvasElement::ExtractData(JSContext* aCx,
                                        nsIPrincipal& aSubjectPrincipal,
                                        nsAString& aType,
                                        const nsAString& aOptions,
                                        nsIInputStream** aStream) {
  // Check site-specific permission and display prompt if appropriate.
  // If no permission, return all-white, opaque image data.
  bool usePlaceholder = !CanvasUtils::IsImageExtractionAllowed(
      OwnerDoc(), aCx, aSubjectPrincipal);
  return ImageEncoder::ExtractData(aType, aOptions, GetSize(), usePlaceholder,
                                   mCurrentContext, mAsyncCanvasRenderer,
                                   aStream);
}

nsresult HTMLCanvasElement::ToDataURLImpl(JSContext* aCx,
                                          nsIPrincipal& aSubjectPrincipal,
                                          const nsAString& aMimeType,
                                          const JS::Value& aEncoderOptions,
                                          nsAString& aDataURL) {
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
  rv =
      ExtractData(aCx, aSubjectPrincipal, type, params, getter_AddRefs(stream));

  // If there are unrecognized custom parse options, we should fall back to
  // the default values for the encoder without any options at all.
  if (rv == NS_ERROR_INVALID_ARG && usingCustomParseOptions) {
    rv = ExtractData(aCx, aSubjectPrincipal, type, EmptyString(),
                     getter_AddRefs(stream));
  }

  NS_ENSURE_SUCCESS(rv, rv);

  // build data URL string
  aDataURL = NS_LITERAL_STRING("data:") + type + NS_LITERAL_STRING(";base64,");

  uint64_t count;
  rv = stream->Available(&count);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(count <= UINT32_MAX, NS_ERROR_FILE_TOO_BIG);

  return Base64EncodeInputStream(stream, aDataURL, (uint32_t)count,
                                 aDataURL.Length());
}

void HTMLCanvasElement::ToBlob(JSContext* aCx, BlobCallback& aCallback,
                               const nsAString& aType,
                               JS::Handle<JS::Value> aParams,
                               nsIPrincipal& aSubjectPrincipal,
                               ErrorResult& aRv) {
  // mWriteOnly check is redundant, but optimizes for the common case.
  if (mWriteOnly && !CallerCanRead(aCx)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsCOMPtr<nsIGlobalObject> global = OwnerDoc()->GetScopeObject();
  MOZ_ASSERT(global);

  nsIntSize elemSize = GetWidthHeight();
  if (elemSize.width == 0 || elemSize.height == 0) {
    // According to spec, blob should return null if either its horizontal
    // dimension or its vertical dimension is zero. See link below.
    // https://html.spec.whatwg.org/multipage/scripting.html#dom-canvas-toblob
    OwnerDoc()->Dispatch(
        TaskCategory::Other,
        NewRunnableMethod<Blob*, const char*>(
            "dom::HTMLCanvasElement::ToBlob", &aCallback,
            static_cast<void (BlobCallback::*)(Blob*, const char*)>(
                &BlobCallback::Call),
            nullptr, nullptr));
    return;
  }

  // Check site-specific permission and display prompt if appropriate.
  // If no permission, return all-white, opaque image data.
  bool usePlaceholder = !CanvasUtils::IsImageExtractionAllowed(
      OwnerDoc(), aCx, aSubjectPrincipal);
  CanvasRenderingContextHelper::ToBlob(aCx, global, aCallback, aType, aParams,
                                       usePlaceholder, aRv);
}
#define DISABLE_OFFSCREEN_CANVAS 1
OffscreenCanvas* HTMLCanvasElement::TransferControlToOffscreen(
    ErrorResult& aRv) {
  if (DISABLE_OFFSCREEN_CANVAS) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return nullptr;
  }
  if (mCurrentContext) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  if (!mOffscreenCanvas) {
    nsIntSize sz = GetWidthHeight();
    RefPtr<AsyncCanvasRenderer> renderer = GetAsyncCanvasRenderer();
    renderer->SetWidth(sz.width);
    renderer->SetHeight(sz.height);

    nsPIDOMWindowInner* win = OwnerDoc()->GetInnerWindow();
    if (!win) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return nullptr;
    }

    mOffscreenCanvas =
        new OffscreenCanvas(win->AsGlobal(), sz.width, sz.height,
                            GetCompositorBackendType(), renderer);
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

already_AddRefed<File> HTMLCanvasElement::MozGetAsFile(
    const nsAString& aName, const nsAString& aType,
    nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  // do a trust check if this is a write-only canvas
  if (mWriteOnly && !aSubjectPrincipal.IsSystemPrincipal()) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  RefPtr<File> file;
  aRv = MozGetAsFileImpl(aName, aType, aSubjectPrincipal, getter_AddRefs(file));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  return file.forget();
}

nsresult HTMLCanvasElement::MozGetAsFileImpl(const nsAString& aName,
                                             const nsAString& aType,
                                             nsIPrincipal& aSubjectPrincipal,
                                             File** aResult) {
  nsCOMPtr<nsIInputStream> stream;
  nsAutoString type(aType);
  nsresult rv =
      ExtractData(nsContentUtils::GetCurrentJSContext(), aSubjectPrincipal,
                  type, EmptyString(), getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  uint64_t imgSize;
  void* imgData = nullptr;
  rv = NS_ReadInputStreamToBuffer(stream, &imgData, -1, &imgSize);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsPIDOMWindowInner> win =
      do_QueryInterface(OwnerDoc()->GetScopeObject());

  // The File takes ownership of the buffer
  RefPtr<File> file = File::CreateMemoryFileWithLastModifiedNow(
      win->AsGlobal(), imgData, imgSize, aName, type);
  if (NS_WARN_IF(!file)) {
    return NS_ERROR_FAILURE;
  }

  file.forget(aResult);
  return NS_OK;
}

nsresult HTMLCanvasElement::GetContext(const nsAString& aContextId,
                                       nsISupports** aContext) {
  ErrorResult rv;
  mMaybeModified = true;  // For FirstContentfulPaint
  *aContext = GetContext(nullptr, aContextId, JS::NullHandleValue, rv).take();
  return rv.StealNSResult();
}

already_AddRefed<nsISupports> HTMLCanvasElement::GetContext(
    JSContext* aCx, const nsAString& aContextId,
    JS::Handle<JS::Value> aContextOptions, ErrorResult& aRv) {
  if (mOffscreenCanvas) {
    return nullptr;
  }

  mMaybeModified = true;  // For FirstContentfulPaint
  return CanvasRenderingContextHelper::GetContext(
      aCx, aContextId,
      aContextOptions.isObject() ? aContextOptions : JS::NullHandleValue, aRv);
}

already_AddRefed<nsISupports> HTMLCanvasElement::MozGetIPCContext(
    const nsAString& aContextId, ErrorResult& aRv) {
  // Note that we're a [ChromeOnly] method, so from JS we can only be called by
  // system code.

  // We only support 2d shmem contexts for now.
  if (!aContextId.EqualsLiteral("2d")) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return nullptr;
  }

  CanvasContextType contextType = CanvasContextType::Canvas2D;

  if (!mCurrentContext) {
    // This canvas doesn't have a context yet.

    RefPtr<nsICanvasRenderingContextInternal> context;
    context = CreateContext(contextType);
    if (!context) {
      return nullptr;
    }

    mCurrentContext = context;
    mCurrentContext->SetIsIPC(true);
    mCurrentContextType = contextType;

    ErrorResult dummy;
    nsresult rv = UpdateContext(nullptr, JS::NullHandleValue, dummy);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.Throw(rv);
      return nullptr;
    }
  } else {
    // We already have a context of some type.
    if (contextType != mCurrentContextType) {
      aRv.Throw(NS_ERROR_INVALID_ARG);
      return nullptr;
    }
  }

  nsCOMPtr<nsISupports> context(mCurrentContext);
  return context.forget();
}

nsIntSize HTMLCanvasElement::GetSize() { return GetWidthHeight(); }

bool HTMLCanvasElement::IsWriteOnly() const { return mWriteOnly; }

void HTMLCanvasElement::SetWriteOnly() {
  mExpandedReader = nullptr;
  mWriteOnly = true;
}

void HTMLCanvasElement::SetWriteOnly(nsIPrincipal* aExpandedReader) {
  mExpandedReader = aExpandedReader;
  mWriteOnly = true;
}

bool HTMLCanvasElement::CallerCanRead(JSContext* aCx) {
  if (!mWriteOnly) {
    return true;
  }

  nsIPrincipal* prin = nsContentUtils::SubjectPrincipal(aCx);

  // If mExpandedReader is set, this canvas was tainted only by
  // mExpandedReader's resources. So allow reading if the subject
  // principal subsumes mExpandedReader.
  if (mExpandedReader && prin->Subsumes(mExpandedReader)) {
    return true;
  }

  return nsContentUtils::PrincipalHasPermission(*prin,
                                                nsGkAtoms::all_urlsPermission);
}

void HTMLCanvasElement::InvalidateCanvasContent(const gfx::Rect* damageRect) {
  // We don't need to flush anything here; if there's no frame or if
  // we plan to reframe we don't need to invalidate it anyway.
  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) return;

  ActiveLayerTracker::NotifyContentChange(frame);

  // When using layers-free WebRender, we cannot invalidate the layer (because
  // there isn't one). Instead, we mark the CanvasRenderer dirty and scheduling
  // an empty transaction which is effectively equivalent.
  CanvasRenderer* renderer = nullptr;
  const auto key = static_cast<uint32_t>(DisplayItemType::TYPE_CANVAS);
  RefPtr<WebRenderLocalCanvasData> localData =
      GetWebRenderUserData<WebRenderLocalCanvasData>(frame, key);
  RefPtr<WebRenderCanvasData> data =
      GetWebRenderUserData<WebRenderCanvasData>(frame, key);
  if (data) {
    renderer = data->GetCanvasRenderer();
  }

  if (localData && wr::AsUint64(localData->mImageKey)) {
    localData->mDirty = true;
    frame->SchedulePaint(nsIFrame::PAINT_COMPOSITE_ONLY);
  } else if (renderer) {
    renderer->SetDirty();
    frame->SchedulePaint(nsIFrame::PAINT_COMPOSITE_ONLY);
  } else {
    Layer* layer = nullptr;
    if (damageRect) {
      nsIntSize size = GetWidthHeight();
      if (size.width != 0 && size.height != 0) {
        gfx::IntRect invalRect = gfx::IntRect::Truncate(*damageRect);
        layer =
            frame->InvalidateLayer(DisplayItemType::TYPE_CANVAS, &invalRect);
      }
    } else {
      layer = frame->InvalidateLayer(DisplayItemType::TYPE_CANVAS);
    }

    if (layer) {
      static_cast<CanvasLayer*>(layer)->Updated();
    } else {
      // This path is taken in two situations:
      // 1) WebRender is enabled and has not yet processed a display list.
      // 2) WebRender is disabled and layer invalidation failed.
      // In both cases, schedule a full paint to properly update canvas.
      frame->SchedulePaint(nsIFrame::PAINT_DEFAULT, false);
    }
  }

  /*
   * Treat canvas invalidations as animation activity for JS. Frequently
   * invalidating a canvas will feed into heuristics and cause JIT code to be
   * kept around longer, for smoother animations.
   */
  nsPIDOMWindowInner* win = OwnerDoc()->GetInnerWindow();

  if (win) {
    if (JSObject* obj = win->AsGlobal()->GetGlobalJSObject()) {
      js::NotifyAnimationActivity(obj);
    }
  }
}

void HTMLCanvasElement::InvalidateCanvas() {
  // We don't need to flush anything here; if there's no frame or if
  // we plan to reframe we don't need to invalidate it anyway.
  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) return;

  frame->InvalidateFrame();
}

int32_t HTMLCanvasElement::CountContexts() {
  if (mCurrentContext) return 1;

  return 0;
}

nsICanvasRenderingContextInternal* HTMLCanvasElement::GetContextAtIndex(
    int32_t index) {
  if (mCurrentContext && index == 0) return mCurrentContext;

  return nullptr;
}

bool HTMLCanvasElement::GetIsOpaque() {
  if (mCurrentContext) {
    return mCurrentContext->GetIsOpaque();
  }

  return GetOpaqueAttr();
}

bool HTMLCanvasElement::GetOpaqueAttr() {
  return HasAttr(kNameSpaceID_None, nsGkAtoms::moz_opaque);
}

CanvasContextType HTMLCanvasElement::GetCurrentContextType() {
  if (mAsyncCanvasRenderer) {
    return mAsyncCanvasRenderer->GetContextType();
  }
  return mCurrentContextType;
}

already_AddRefed<Layer> HTMLCanvasElement::GetCanvasLayer(
    nsDisplayListBuilder* aBuilder, Layer* aOldLayer, LayerManager* aManager) {
  // The address of sOffscreenCanvasLayerUserDataDummy is used as the user
  // data key for retained LayerManagers managed by FrameLayerBuilder.
  // We don't much care about what value in it, so just assign a dummy
  // value for it.
  static uint8_t sOffscreenCanvasLayerUserDataDummy = 0;
  static uint8_t sOffscreenImageLayerUserDataDummy = 0;

  if (mCurrentContext) {
    return mCurrentContext->GetCanvasLayer(aBuilder, aOldLayer, aManager);
  }

  if (mOffscreenCanvas) {
    CanvasContextType contentType = mAsyncCanvasRenderer->GetContextType();
    if (contentType == CanvasContextType::NoContext) {
      // context is not created yet.
      return nullptr;
    }
    if (contentType != CanvasContextType::ImageBitmap) {
      if (!mResetLayer && aOldLayer &&
          aOldLayer->HasUserData(&sOffscreenCanvasLayerUserDataDummy)) {
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

      CanvasRenderer* canvasRenderer = layer->CreateOrGetCanvasRenderer();

      if (!InitializeCanvasRenderer(aBuilder, canvasRenderer)) {
        return nullptr;
      }

      layer->Updated();
      mResetLayer = false;
      return layer.forget();
    }
    if (contentType == CanvasContextType::ImageBitmap) {
      if (!mResetLayer && aOldLayer &&
          aOldLayer->HasUserData(&sOffscreenImageLayerUserDataDummy)) {
        RefPtr<Layer> ret = aOldLayer;
        return ret.forget();
      }

      RefPtr<ImageLayer> layer = aManager->CreateImageLayer();
      if (!layer) {
        NS_WARNING("CreateImageLayer failed!");
        return nullptr;
      }

      LayerUserData* userData = nullptr;
      layer->SetUserData(&sOffscreenImageLayerUserDataDummy, userData);

      RefPtr<ImageContainer> imageContainer =
          mAsyncCanvasRenderer->GetImageContainer();
      MOZ_ASSERT(imageContainer);
      layer->SetContainer(imageContainer);
      mResetLayer = false;
      return layer.forget();
    }
  }

  return nullptr;
}

bool HTMLCanvasElement::UpdateWebRenderCanvasData(
    nsDisplayListBuilder* aBuilder, WebRenderCanvasData* aCanvasData) {
  if (mCurrentContext) {
    return mCurrentContext->UpdateWebRenderCanvasData(aBuilder, aCanvasData);
  }
  if (mOffscreenCanvas) {
    CanvasContextType contentType = mAsyncCanvasRenderer->GetContextType();
    if (contentType == CanvasContextType::NoContext) {
      // context is not created yet.
      return false;
    }
    if (contentType != CanvasContextType::ImageBitmap) {
      CanvasRenderer* renderer = aCanvasData->GetCanvasRenderer();

      if (!mResetLayer && renderer) {
        return true;
      }

      renderer = aCanvasData->CreateCanvasRenderer();
      if (!InitializeCanvasRenderer(aBuilder, renderer)) {
        // Clear CanvasRenderer of WebRenderCanvasData
        aCanvasData->ClearCanvasRenderer();
        return false;
      }

      MOZ_ASSERT(renderer);
      mResetLayer = false;
      return true;
    }
    if (contentType == CanvasContextType::ImageBitmap) {
      RefPtr<ImageContainer> imageContainer =
          mAsyncCanvasRenderer->GetImageContainer();
      MOZ_ASSERT(imageContainer);
      aCanvasData->SetImageContainer(imageContainer);
      mResetLayer = false;
      return true;
    }
  }

  // Clear CanvasRenderer of WebRenderCanvasData
  aCanvasData->ClearCanvasRenderer();
  return false;
}

bool HTMLCanvasElement::InitializeCanvasRenderer(nsDisplayListBuilder* aBuilder,
                                                 CanvasRenderer* aRenderer) {
  if (mCurrentContext) {
    return mCurrentContext->InitializeCanvasRenderer(aBuilder, aRenderer);
  }

  if (mOffscreenCanvas) {
    CanvasInitializeData data;
    data.mRenderer = GetAsyncCanvasRenderer();
    data.mSize = GetWidthHeight();
    aRenderer->Initialize(data);
    return true;
  }

  return false;
}

bool HTMLCanvasElement::ShouldForceInactiveLayer(LayerManager* aManager) {
  if (mCurrentContext) {
    return mCurrentContext->ShouldForceInactiveLayer(aManager);
  }

  if (mOffscreenCanvas) {
    // TODO: We should handle offscreen canvas case.
    return false;
  }

  return true;
}

void HTMLCanvasElement::MarkContextClean() {
  if (!mCurrentContext) return;

  mCurrentContext->MarkContextClean();
}

void HTMLCanvasElement::MarkContextCleanForFrameCapture() {
  if (!mCurrentContext) return;

  mCurrentContext->MarkContextCleanForFrameCapture();
}

bool HTMLCanvasElement::IsContextCleanForFrameCapture() {
  return mCurrentContext && mCurrentContext->IsContextCleanForFrameCapture();
}

nsresult HTMLCanvasElement::RegisterFrameCaptureListener(
    FrameCaptureListener* aListener, bool aReturnPlaceholderData) {
  WeakPtr<FrameCaptureListener> listener = aListener;

  if (mRequestedFrameListeners.Contains(listener)) {
    return NS_OK;
  }

  if (!mRequestedFrameRefreshObserver) {
    Document* doc = OwnerDoc();
    if (!doc) {
      return NS_ERROR_FAILURE;
    }

    while (doc->GetInProcessParentDocument()) {
      doc = doc->GetInProcessParentDocument();
    }

    nsPresContext* context = doc->GetPresContext();
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
        new RequestedFrameRefreshObserver(this, driver, aReturnPlaceholderData);
  } else {
    mRequestedFrameRefreshObserver->SetReturnPlaceholderData(
        aReturnPlaceholderData);
  }

  mRequestedFrameListeners.AppendElement(listener);
  mRequestedFrameRefreshObserver->Register();
  return NS_OK;
}

bool HTMLCanvasElement::IsFrameCaptureRequested() const {
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

void HTMLCanvasElement::ProcessDestroyedFrameListeners() {
  // Loop backwards to allow removing elements in the loop.
  for (int i = mRequestedFrameListeners.Length() - 1; i >= 0; --i) {
    WeakPtr<FrameCaptureListener> listener = mRequestedFrameListeners[i];
    if (!listener) {
      // listener was destroyed. Remove it from the list.
      mRequestedFrameListeners.RemoveElementAt(i);
      continue;
    }
  }

  if (mRequestedFrameListeners.IsEmpty()) {
    mRequestedFrameRefreshObserver->Unregister();
  }
}

void HTMLCanvasElement::SetFrameCapture(
    already_AddRefed<SourceSurface> aSurface, const TimeStamp& aTime) {
  RefPtr<SourceSurface> surface = aSurface;
  RefPtr<SourceSurfaceImage> image =
      new SourceSurfaceImage(surface->GetSize(), surface);

  for (WeakPtr<FrameCaptureListener> listener : mRequestedFrameListeners) {
    if (!listener) {
      continue;
    }

    RefPtr<Image> imageRefCopy = image.get();
    listener->NewFrame(imageRefCopy.forget(), aTime);
  }
}

already_AddRefed<SourceSurface> HTMLCanvasElement::GetSurfaceSnapshot(
    gfxAlphaType* const aOutAlphaType) {
  if (!mCurrentContext) return nullptr;

  return mCurrentContext->GetSurfaceSnapshot(aOutAlphaType);
}

AsyncCanvasRenderer* HTMLCanvasElement::GetAsyncCanvasRenderer() {
  if (!mAsyncCanvasRenderer) {
    mAsyncCanvasRenderer = new AsyncCanvasRenderer();
    mAsyncCanvasRenderer->mHTMLCanvasElement = this;
  }

  return mAsyncCanvasRenderer;
}

layers::OOPCanvasRenderer* HTMLCanvasElement::GetOOPCanvasRenderer() {
  if (!mOOPCanvasRenderer) {
    const auto context = GetWebGLContext();
    MOZ_ASSERT(context);
    mOOPCanvasRenderer = new OOPCanvasRenderer(context);
  }

  return mOOPCanvasRenderer;
}

layers::LayersBackend HTMLCanvasElement::GetCompositorBackendType() const {
  nsIWidget* docWidget = nsContentUtils::WidgetForDocument(OwnerDoc());
  if (docWidget) {
    layers::LayerManager* layerManager = docWidget->GetLayerManager();
    if (layerManager) {
      return layerManager->GetCompositorBackendType();
    }
  }

  return LayersBackend::LAYERS_NONE;
}

void HTMLCanvasElement::OnVisibilityChange() {
  if (OwnerDoc()->Hidden()) {
    return;
  }

  if (mOffscreenCanvas) {
    class Runnable final : public CancelableRunnable {
     public:
      explicit Runnable(AsyncCanvasRenderer* aRenderer)
          : mozilla::CancelableRunnable("Runnable"), mRenderer(aRenderer) {}

      NS_IMETHOD Run() override {
        if (mRenderer && mRenderer->mContext) {
          mRenderer->mContext->OnVisibilityChange();
        }

        return NS_OK;
      }

     private:
      RefPtr<AsyncCanvasRenderer> mRenderer;
    };

    RefPtr<nsIRunnable> runnable = new Runnable(mAsyncCanvasRenderer);
    nsCOMPtr<nsIEventTarget> activeTarget =
        mAsyncCanvasRenderer->GetActiveEventTarget();
    if (activeTarget) {
      activeTarget->Dispatch(runnable, nsIThread::DISPATCH_NORMAL);
    }
    return;
  }

  if (mCurrentContext) {
    mCurrentContext->OnVisibilityChange();
  }
}

void HTMLCanvasElement::OnMemoryPressure() {
  if (mOffscreenCanvas) {
    class Runnable final : public CancelableRunnable {
     public:
      explicit Runnable(AsyncCanvasRenderer* aRenderer)
          : mozilla::CancelableRunnable("Runnable"), mRenderer(aRenderer) {}

      NS_IMETHOD Run() override {
        if (mRenderer && mRenderer->mContext) {
          mRenderer->mContext->OnMemoryPressure();
        }

        return NS_OK;
      }

     private:
      RefPtr<AsyncCanvasRenderer> mRenderer;
    };

    RefPtr<nsIRunnable> runnable = new Runnable(mAsyncCanvasRenderer);
    nsCOMPtr<nsIEventTarget> activeTarget =
        mAsyncCanvasRenderer->GetActiveEventTarget();
    if (activeTarget) {
      activeTarget->Dispatch(runnable, nsIThread::DISPATCH_NORMAL);
    }
    return;
  }

  if (mCurrentContext) {
    mCurrentContext->OnMemoryPressure();
  }
}

void HTMLCanvasElement::OnDeviceReset() {
  if (!mOffscreenCanvas && mCurrentContext) {
    mCurrentContext->Reset();
  }
}

/* static */
void HTMLCanvasElement::SetAttrFromAsyncCanvasRenderer(
    AsyncCanvasRenderer* aRenderer) {
  HTMLCanvasElement* element = aRenderer->mHTMLCanvasElement;
  if (!element) {
    return;
  }

  if (element->GetWidthHeight() == aRenderer->GetSize()) {
    return;
  }

  gfx::IntSize asyncCanvasSize = aRenderer->GetSize();

  ErrorResult rv;
  element->SetUnsignedIntAttr(nsGkAtoms::width, asyncCanvasSize.width,
                              DEFAULT_CANVAS_WIDTH, rv);
  if (rv.Failed()) {
    NS_WARNING(
        "Failed to set width attribute to a canvas element asynchronously.");
  }

  element->SetUnsignedIntAttr(nsGkAtoms::height, asyncCanvasSize.height,
                              DEFAULT_CANVAS_HEIGHT, rv);
  if (rv.Failed()) {
    NS_WARNING(
        "Failed to set height attribute to a canvas element asynchronously.");
  }

  element->mResetLayer = true;
}

/* static */
void HTMLCanvasElement::InvalidateFromAsyncCanvasRenderer(
    AsyncCanvasRenderer* aRenderer) {
  HTMLCanvasElement* element = aRenderer->mHTMLCanvasElement;
  if (!element) {
    return;
  }

  element->InvalidateCanvasContent(nullptr);
}

ClientWebGLContext* HTMLCanvasElement::GetWebGLContext() {
  if (GetCurrentContextType() != CanvasContextType::WebGL1 &&
      GetCurrentContextType() != CanvasContextType::WebGL2) {
    return nullptr;
  }

  return static_cast<ClientWebGLContext*>(GetContextAtIndex(0));
}

webgpu::CanvasContext* HTMLCanvasElement::GetWebGPUContext() {
  if (GetCurrentContextType() != CanvasContextType::WebGPU) {
    return nullptr;
  }

  return static_cast<webgpu::CanvasContext*>(GetContextAtIndex(0));
}

}  // namespace dom
}  // namespace mozilla
