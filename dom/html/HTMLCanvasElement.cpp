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
#include "MediaTrackGraph.h"
#include "mozilla/Assertions.h"
#include "mozilla/Base64.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/CanvasCaptureMediaStream.h"
#include "mozilla/dom/CanvasRenderingContext2D.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/GeneratePlaceholderCanvasData.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/HTMLCanvasElementBinding.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "mozilla/dom/MouseEvent.h"
#include "mozilla/dom/OffscreenCanvas.h"
#include "mozilla/dom/OffscreenCanvasDisplayHelper.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/layers/CanvasRenderer.h"
#include "mozilla/layers/WebRenderCanvasRenderer.h"
#include "mozilla/layers/WebRenderUserData.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerLabels.h"
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
#include "WindowRenderer.h"

using namespace mozilla::layers;
using namespace mozilla::gfx;

NS_IMPL_NS_NEW_HTML_ELEMENT(Canvas)

namespace mozilla::dom {

class RequestedFrameRefreshObserver : public nsARefreshObserver {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RequestedFrameRefreshObserver, override)

 public:
  RequestedFrameRefreshObserver(HTMLCanvasElement* const aOwningElement,
                                nsRefreshDriver* aRefreshDriver,
                                bool aReturnPlaceholderData)
      : mRegistered(false),
        mReturnPlaceholderData(aReturnPlaceholderData),
        mOwningElement(aOwningElement),
        mRefreshDriver(aRefreshDriver),
        mWatchManager(this, AbstractThread::MainThread()),
        mPendingThrottledCapture(false) {
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
      auto size = write.GetStride() * copy->GetSize().height;
      auto* data = write.GetData();
      GeneratePlaceholderCanvasData(size, data);
    } else {
      memcpy(write.GetData(), read.GetData(),
             write.GetStride() * copy->GetSize().height);
    }

    return copy.forget();
  }

  void SetReturnPlaceholderData(bool aReturnPlaceholderData) {
    mReturnPlaceholderData = aReturnPlaceholderData;
  }

  void NotifyCaptureStateChange() {
    if (mPendingThrottledCapture) {
      return;
    }

    if (!mOwningElement) {
      return;
    }

    Watchable<FrameCaptureState>* captureState =
        mOwningElement->GetFrameCaptureState();
    if (!captureState) {
      PROFILER_MARKER_TEXT("Canvas CaptureStream", MEDIA_RT, {},
                           "Abort: No capture state"_ns);
      return;
    }

    if (captureState->Ref() == FrameCaptureState::CLEAN) {
      PROFILER_MARKER_TEXT("Canvas CaptureStream", MEDIA_RT, {},
                           "Abort: CLEAN"_ns);
      return;
    }

    if (!mRefreshDriver) {
      PROFILER_MARKER_TEXT("Canvas CaptureStream", MEDIA_RT, {},
                           "Abort: no refresh driver"_ns);
      return;
    }

    if (!mRefreshDriver->IsThrottled()) {
      PROFILER_MARKER_TEXT("Canvas CaptureStream", MEDIA_RT, {},
                           "Abort: not throttled"_ns);
      return;
    }

    TimeStamp now = TimeStamp::Now();
    TimeStamp next =
        mLastCaptureTime.IsNull()
            ? now
            : mLastCaptureTime + TimeDuration::FromMilliseconds(
                                     nsRefreshDriver::DefaultInterval());
    if (mLastCaptureTime.IsNull() || next <= now) {
      AUTO_PROFILER_MARKER_TEXT("Canvas CaptureStream", MEDIA_RT, {},
                                "CaptureFrame direct while throttled"_ns);
      CaptureFrame(now);
      return;
    }

    nsCString str;
    if (profiler_thread_is_being_profiled_for_markers()) {
      str.AppendPrintf("Delaying CaptureFrame by %.2fms",
                       (next - now).ToMilliseconds());
    }
    AUTO_PROFILER_MARKER_TEXT("Canvas CaptureStream", MEDIA_RT, {}, str);

    mPendingThrottledCapture = true;
    AbstractThread::MainThread()->DelayedDispatch(
        NS_NewRunnableFunction(
            __func__,
            [this, self = RefPtr<RequestedFrameRefreshObserver>(this), next] {
              mPendingThrottledCapture = false;
              AUTO_PROFILER_MARKER_TEXT(
                  "Canvas CaptureStream", MEDIA_RT, {},
                  "CaptureFrame after delay while throttled"_ns);
              CaptureFrame(next);
            }),
        // next >= now, so this is a guard for (next - now) flooring to 0.
        std::max<uint32_t>(
            1, static_cast<uint32_t>((next - now).ToMilliseconds())));
  }

  void WillRefresh(TimeStamp aTime) override {
    AUTO_PROFILER_MARKER_TEXT("Canvas CaptureStream", MEDIA_RT, {},
                              "CaptureFrame by refresh driver"_ns);

    CaptureFrame(aTime);
  }

  void CaptureFrame(TimeStamp aTime) {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mOwningElement) {
      PROFILER_MARKER_TEXT("Canvas CaptureStream", MEDIA_RT, {},
                           "Abort: no owning element"_ns);
      return;
    }

    if (mOwningElement->IsWriteOnly()) {
      PROFILER_MARKER_TEXT("Canvas CaptureStream", MEDIA_RT, {},
                           "Abort: write only"_ns);
      return;
    }

    if (auto* captureStateWatchable = mOwningElement->GetFrameCaptureState();
        captureStateWatchable &&
        *captureStateWatchable == FrameCaptureState::CLEAN) {
      PROFILER_MARKER_TEXT("Canvas CaptureStream", MEDIA_RT, {},
                           "Abort: CLEAN"_ns);
      return;
    }

    // Mark the context already now, since if the frame capture state is DIRTY
    // and we catch an early return below (not marking it CLEAN), the next draw
    // will not trigger a capture state change from the
    // Watchable<FrameCaptureState>.
    mOwningElement->MarkContextCleanForFrameCapture();

    mOwningElement->ProcessDestroyedFrameListeners();

    if (!mOwningElement->IsFrameCaptureRequested(aTime)) {
      PROFILER_MARKER_TEXT("Canvas CaptureStream", MEDIA_RT, {},
                           "Abort: no capture requested"_ns);
      return;
    }

    RefPtr<SourceSurface> snapshot;
    {
      AUTO_PROFILER_MARKER_TEXT("Canvas CaptureStream", MEDIA_RT, {},
                                "GetSnapshot"_ns);
      snapshot = mOwningElement->GetSurfaceSnapshot(nullptr);
      if (!snapshot) {
        PROFILER_MARKER_TEXT("Canvas CaptureStream", MEDIA_RT, {},
                             "Abort: snapshot failed"_ns);
        return;
      }
    }

    RefPtr<DataSourceSurface> copy;
    {
      AUTO_PROFILER_MARKER_TEXT("Canvas CaptureStream", MEDIA_RT, {},
                                "CopySurface"_ns);
      copy = CopySurface(snapshot, mReturnPlaceholderData);
      if (!copy) {
        PROFILER_MARKER_TEXT("Canvas CaptureStream", MEDIA_RT, {},
                             "Abort: copy failed"_ns);
        return;
      }
    }

    nsCString str;
    if (profiler_thread_is_being_profiled_for_markers()) {
      TimeDuration sinceLast =
          aTime - (mLastCaptureTime.IsNull() ? aTime : mLastCaptureTime);
      str.AppendPrintf("Forwarding captured frame %.2fms after last",
                       sinceLast.ToMilliseconds());
    }
    AUTO_PROFILER_MARKER_TEXT("Canvas CaptureStream", MEDIA_RT, {}, str);

    if (!mLastCaptureTime.IsNull() && aTime <= mLastCaptureTime) {
      aTime = mLastCaptureTime + TimeDuration::FromMilliseconds(1);
    }
    mLastCaptureTime = aTime;

    mOwningElement->SetFrameCapture(copy.forget(), aTime);
  }

  void DetachFromRefreshDriver() {
    MOZ_ASSERT(mOwningElement);
    MOZ_ASSERT(mRefreshDriver);

    Unregister();
    mRefreshDriver = nullptr;
    mWatchManager.Shutdown();
  }

  void Register() {
    if (mRegistered) {
      return;
    }

    MOZ_ASSERT(mRefreshDriver);
    if (mRefreshDriver) {
      mRefreshDriver->AddRefreshObserver(this, FlushType::Display,
                                         "Canvas frame capture listeners");
      mRegistered = true;
    }

    if (!mOwningElement) {
      return;
    }

    if (Watchable<FrameCaptureState>* captureState =
            mOwningElement->GetFrameCaptureState()) {
      mWatchManager.Watch(
          *captureState,
          &RequestedFrameRefreshObserver::NotifyCaptureStateChange);
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

    if (!mOwningElement) {
      return;
    }

    if (Watchable<FrameCaptureState>* captureState =
            mOwningElement->GetFrameCaptureState()) {
      mWatchManager.Unwatch(
          *captureState,
          &RequestedFrameRefreshObserver::NotifyCaptureStateChange);
    }
  }

 private:
  virtual ~RequestedFrameRefreshObserver() {
    MOZ_ASSERT(!mRefreshDriver);
    MOZ_ASSERT(!mRegistered);
  }

  bool mRegistered;
  bool mReturnPlaceholderData;
  const WeakPtr<HTMLCanvasElement> mOwningElement;
  RefPtr<nsRefreshDriver> mRefreshDriver;
  WatchManager<RequestedFrameRefreshObserver> mWatchManager;
  TimeStamp mLastCaptureTime;
  bool mPendingThrottledCapture;
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
  RegisterObserverEvents();
}

HTMLCanvasElementObserver::~HTMLCanvasElementObserver() { Destroy(); }

void HTMLCanvasElementObserver::Destroy() {
  UnregisterObserverEvents();
  mElement = nullptr;
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

NS_IMPL_ISUPPORTS(HTMLCanvasElementObserver, nsIObserver)

// ---------------------------------------------------------------------------

HTMLCanvasElement::HTMLCanvasElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsGenericHTMLElement(std::move(aNodeInfo)),
      mResetLayer(true),
      mMaybeModified(false),
      mWriteOnly(false) {}

HTMLCanvasElement::~HTMLCanvasElement() { Destroy(); }

void HTMLCanvasElement::Destroy() {
  if (mOffscreenDisplay) {
    mOffscreenDisplay->Destroy();
    mOffscreenDisplay = nullptr;
    mImageContainer = nullptr;
  }

  if (mContextObserver) {
    mContextObserver->Destroy();
    mContextObserver = nullptr;
  }

  ResetPrintCallback();
  if (mRequestedFrameRefreshObserver) {
    mRequestedFrameRefreshObserver->DetachFromRefreshDriver();
    mRequestedFrameRefreshObserver = nullptr;
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLCanvasElement)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLCanvasElement,
                                                nsGenericHTMLElement)
  tmp->Destroy();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCurrentContext, mPrintCallback, mPrintState,
                                  mOriginalCanvas, mOffscreenCanvas)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLCanvasElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCurrentContext, mPrintCallback,
                                    mPrintState, mOriginalCanvas,
                                    mOffscreenCanvas)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

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
  // nsPageSequenceFrame::PrePrintNextSheet.
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
    rv = GetContext(u"2d"_ns, getter_AddRefs(context));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  mPrintState = new HTMLCanvasPrintState(this, mCurrentContext, aCallback);

  RefPtr<nsRunnableMethod<HTMLCanvasElement>> renderEvent =
      NewRunnableMethod("dom::HTMLCanvasElement::CallPrintCallback", this,
                        &HTMLCanvasElement::CallPrintCallback);
  return OwnerDoc()->Dispatch(TaskCategory::Other, renderEvent.forget());
}

void HTMLCanvasElement::CallPrintCallback() {
  if (!mPrintState) {
    // `mPrintState` might have been destroyed by cancelling the previous
    // printing (especially the canvas frame destruction) during processing
    // event loops in the printing.
    return;
  }
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
  Document* destDoc = aDest->OwnerDoc();
  if (destDoc->IsStaticDocument()) {
    // The Firefox print preview code can create a static clone from an
    // existing static clone, so we may not be the original 'canvas' element.
    aDest->mOriginalCanvas = GetOriginalCanvas();

    if (GetMozPrintCallback()) {
      destDoc->SetHasPrintCallbacks();
    }

    // We make sure that the canvas is not zero sized since that would cause
    // the DrawImage call below to return an error, which would cause printing
    // to fail.
    nsIntSize size = GetWidthHeight();
    if (size.height > 0 && size.width > 0) {
      nsCOMPtr<nsISupports> cxt;
      aDest->GetContext(u"2d"_ns, getter_AddRefs(cxt));
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

void HTMLCanvasElement::MapAttributesIntoRule(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  MapAspectRatioInto(aAttributes, aDecls);
  MapCommonAttributesInto(aAttributes, aDecls);
}

nsMapRuleToAttributesFunc HTMLCanvasElement::GetAttributeMappingFunction()
    const {
  return &MapAttributesIntoRule;
}

NS_IMETHODIMP_(bool)
HTMLCanvasElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  static const MappedAttributeEntry attributes[] = {
      {nsGkAtoms::width}, {nsGkAtoms::height}, {nullptr}};
  static const MappedAttributeEntry* const map[] = {attributes,
                                                    sCommonAttributeMap};
  return FindAttributeDependence(aAttribute, map);
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

  bool HasAlpha() const override {
    if (!mCaptureStream || !mCaptureStream->Canvas()) {
      // In cycle-collection
      return false;
    }
    return !mCaptureStream->Canvas()->GetIsOpaque();
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
      OwnerDoc(), nsContentUtils::GetCurrentJSContext(),
      Some(&aSubjectPrincipal));

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
      OwnerDoc(), aCx, Some(&aSubjectPrincipal));
  return ImageEncoder::ExtractData(aType, aOptions, GetSize(), usePlaceholder,
                                   mCurrentContext, mCanvasRenderer, aStream);
}

nsresult HTMLCanvasElement::ToDataURLImpl(JSContext* aCx,
                                          nsIPrincipal& aSubjectPrincipal,
                                          const nsAString& aMimeType,
                                          const JS::Value& aEncoderOptions,
                                          nsAString& aDataURL) {
  nsIntSize size = GetWidthHeight();
  if (size.height == 0 || size.width == 0) {
    aDataURL = u"data:,"_ns;
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
    rv = ExtractData(aCx, aSubjectPrincipal, type, u""_ns,
                     getter_AddRefs(stream));
  }

  NS_ENSURE_SUCCESS(rv, rv);

  // build data URL string
  aDataURL = u"data:"_ns + type + u";base64,"_ns;

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
      OwnerDoc(), aCx, Some(&aSubjectPrincipal));
  CanvasRenderingContextHelper::ToBlob(aCx, global, aCallback, aType, aParams,
                                       usePlaceholder, aRv);
}

OffscreenCanvas* HTMLCanvasElement::TransferControlToOffscreen(
    ErrorResult& aRv) {
  if (mCurrentContext || mOffscreenCanvas) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  MOZ_ASSERT(!mOffscreenDisplay);

  nsPIDOMWindowInner* win = OwnerDoc()->GetInnerWindow();
  if (!win) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  LayersBackend backend = LayersBackend::LAYERS_NONE;
  TextureType textureType = TextureType::Unknown;
  nsIWidget* docWidget = nsContentUtils::WidgetForDocument(OwnerDoc());
  if (docWidget) {
    WindowRenderer* renderer = docWidget->GetWindowRenderer();
    if (renderer) {
      backend = renderer->GetCompositorBackendType();
      textureType = TexTypeForWebgl(renderer->AsKnowsCompositor());
    }
  }

  nsIntSize sz = GetWidthHeight();
  mOffscreenDisplay =
      MakeRefPtr<OffscreenCanvasDisplayHelper>(this, sz.width, sz.height);
  mOffscreenCanvas =
      new OffscreenCanvas(win->AsGlobal(), sz.width, sz.height, backend,
                          textureType, mOffscreenDisplay);
  if (mWriteOnly) {
    mOffscreenCanvas->SetWriteOnly();
  }

  if (!mContextObserver) {
    mContextObserver = new HTMLCanvasElementObserver(this);
  }

  return mOffscreenCanvas;
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
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  mMaybeModified = true;  // For FirstContentfulPaint
  return CanvasRenderingContextHelper::GetOrCreateContext(
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

  if (mOffscreenCanvas) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
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

void HTMLCanvasElement::SetWriteOnly(
    nsIPrincipal* aExpandedReader /* = nullptr */) {
  mExpandedReader = aExpandedReader;
  mWriteOnly = true;
  if (mOffscreenCanvas) {
    mOffscreenCanvas->SetWriteOnly();
  }
}

bool HTMLCanvasElement::CallerCanRead(JSContext* aCx) const {
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

void HTMLCanvasElement::SetWidth(uint32_t aWidth, ErrorResult& aRv) {
  if (mOffscreenCanvas) {
    aRv.ThrowInvalidStateError(
        "Cannot set width of placeholder canvas transferred to "
        "OffscreenCanvas.");
    return;
  }

  SetUnsignedIntAttr(nsGkAtoms::width, aWidth, DEFAULT_CANVAS_WIDTH, aRv);
}

void HTMLCanvasElement::SetHeight(uint32_t aHeight, ErrorResult& aRv) {
  if (mOffscreenCanvas) {
    aRv.ThrowInvalidStateError(
        "Cannot set height of placeholder canvas transferred to "
        "OffscreenCanvas.");
    return;
  }

  SetUnsignedIntAttr(nsGkAtoms::height, aHeight, DEFAULT_CANVAS_HEIGHT, aRv);
}

void HTMLCanvasElement::InvalidateCanvasPlaceholder(uint32_t aWidth,
                                                    uint32_t aHeight) {
  ErrorResult rv;
  SetUnsignedIntAttr(nsGkAtoms::width, aWidth, DEFAULT_CANVAS_WIDTH, rv);
  MOZ_ASSERT(!rv.Failed());
  SetUnsignedIntAttr(nsGkAtoms::height, aHeight, DEFAULT_CANVAS_HEIGHT, rv);
  MOZ_ASSERT(!rv.Failed());
}

void HTMLCanvasElement::InvalidateCanvasContent(const gfx::Rect* damageRect) {
  // Cache the current ImageContainer to avoid contention on the mutex.
  if (mOffscreenDisplay) {
    mImageContainer = mOffscreenDisplay->GetImageContainer();
  }

  // We don't need to flush anything here; if there's no frame or if
  // we plan to reframe we don't need to invalidate it anyway.
  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) return;

  // When using layers-free WebRender, we cannot invalidate the layer (because
  // there isn't one). Instead, we mark the CanvasRenderer dirty and scheduling
  // an empty transaction which is effectively equivalent.
  CanvasRenderer* renderer = nullptr;
  const auto key = static_cast<uint32_t>(DisplayItemType::TYPE_CANVAS);
  RefPtr<WebRenderCanvasData> data =
      GetWebRenderUserData<WebRenderCanvasData>(frame, key);
  if (data) {
    renderer = data->GetCanvasRenderer();
  }

  if (renderer) {
    renderer->SetDirty();
    frame->SchedulePaint(nsIFrame::PAINT_COMPOSITE_ONLY);
  } else {
    if (damageRect) {
      nsIntSize size = GetWidthHeight();
      if (size.width != 0 && size.height != 0) {
        gfx::IntRect invalRect = gfx::IntRect::Truncate(*damageRect);
        frame->InvalidateLayer(DisplayItemType::TYPE_CANVAS, &invalRect);
      }
    } else {
      frame->InvalidateLayer(DisplayItemType::TYPE_CANVAS);
    }

    // This path is taken in two situations:
    // 1) WebRender is enabled and has not yet processed a display list.
    // 2) WebRender is disabled and layer invalidation failed.
    // In both cases, schedule a full paint to properly update canvas.
    frame->SchedulePaint(nsIFrame::PAINT_DEFAULT, false);
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
  if (mCurrentContextType == CanvasContextType::NoContext &&
      mOffscreenDisplay) {
    mCurrentContextType = mOffscreenDisplay->GetContextType();
  }
  return mCurrentContextType;
}

already_AddRefed<Image> HTMLCanvasElement::GetAsImage() {
  if (mOffscreenDisplay) {
    return mOffscreenDisplay->GetAsImage();
  }

  if (mCurrentContext) {
    return mCurrentContext->GetAsImage();
  }

  return nullptr;
}

bool HTMLCanvasElement::UpdateWebRenderCanvasData(
    nsDisplayListBuilder* aBuilder, WebRenderCanvasData* aCanvasData) {
  MOZ_ASSERT(!mOffscreenDisplay);

  if (mCurrentContext) {
    return mCurrentContext->UpdateWebRenderCanvasData(aBuilder, aCanvasData);
  }

  // Clear CanvasRenderer of WebRenderCanvasData
  aCanvasData->ClearCanvasRenderer();
  return false;
}

bool HTMLCanvasElement::InitializeCanvasRenderer(nsDisplayListBuilder* aBuilder,
                                                 CanvasRenderer* aRenderer) {
  MOZ_ASSERT(!mOffscreenDisplay);

  if (mCurrentContext) {
    return mCurrentContext->InitializeCanvasRenderer(aBuilder, aRenderer);
  }

  return false;
}

void HTMLCanvasElement::MarkContextClean() {
  if (!mCurrentContext) return;

  mCurrentContext->MarkContextClean();
}

void HTMLCanvasElement::MarkContextCleanForFrameCapture() {
  if (!mCurrentContext) return;

  mCurrentContext->MarkContextCleanForFrameCapture();
}

Watchable<FrameCaptureState>* HTMLCanvasElement::GetFrameCaptureState() {
  if (!mCurrentContext) {
    return nullptr;
  }
  return mCurrentContext->GetFrameCaptureState();
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

    PresShell* shell = nsContentUtils::FindPresShellForDocument(doc);
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
        new RequestedFrameRefreshObserver(this, driver, aReturnPlaceholderData);
  } else {
    mRequestedFrameRefreshObserver->SetReturnPlaceholderData(
        aReturnPlaceholderData);
  }

  mRequestedFrameListeners.AppendElement(listener);
  mRequestedFrameRefreshObserver->Register();
  return NS_OK;
}

bool HTMLCanvasElement::IsFrameCaptureRequested(const TimeStamp& aTime) const {
  for (WeakPtr<FrameCaptureListener> listener : mRequestedFrameListeners) {
    if (!listener) {
      continue;
    }

    if (listener->FrameCaptureRequested(aTime)) {
      return true;
    }
  }
  return false;
}

void HTMLCanvasElement::ProcessDestroyedFrameListeners() {
  // Remove destroyed listeners from the list.
  mRequestedFrameListeners.RemoveElementsBy(
      [](const auto& weakListener) { return !weakListener; });

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
  if (mCurrentContext) {
    return mCurrentContext->GetSurfaceSnapshot(aOutAlphaType);
  } else if (mOffscreenDisplay) {
    return mOffscreenDisplay->GetSurfaceSnapshot();
  }
  return nullptr;
}

layers::LayersBackend HTMLCanvasElement::GetCompositorBackendType() const {
  nsIWidget* docWidget = nsContentUtils::WidgetForDocument(OwnerDoc());
  if (docWidget) {
    WindowRenderer* renderer = docWidget->GetWindowRenderer();
    if (renderer) {
      return renderer->GetCompositorBackendType();
    }
  }

  return LayersBackend::LAYERS_NONE;
}

void HTMLCanvasElement::OnMemoryPressure() {
  // FIXME(aosmond): We need to implement memory pressure handling for
  // OffscreenCanvas when it is on worker threads. See bug 1746260.

  if (mCurrentContext) {
    mCurrentContext->OnMemoryPressure();
  }
}

void HTMLCanvasElement::OnDeviceReset() {
  if (!mOffscreenCanvas && mCurrentContext) {
    mCurrentContext->Reset();
  }
}

ClientWebGLContext* HTMLCanvasElement::GetWebGLContext() {
  if (GetCurrentContextType() != CanvasContextType::WebGL1 &&
      GetCurrentContextType() != CanvasContextType::WebGL2) {
    return nullptr;
  }

  return static_cast<ClientWebGLContext*>(GetCurrentContext());
}

webgpu::CanvasContext* HTMLCanvasElement::GetWebGPUContext() {
  if (GetCurrentContextType() != CanvasContextType::WebGPU) {
    return nullptr;
  }

  return static_cast<webgpu::CanvasContext*>(GetCurrentContext());
}

CompositableHandle HTMLCanvasElement::GetCompositableHandle() const {
  if (mOffscreenDisplay) {
    return mOffscreenDisplay->GetCompositableHandle();
  }
  return CompositableHandle();
}

}  // namespace mozilla::dom
