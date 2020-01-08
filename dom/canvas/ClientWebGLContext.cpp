/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientWebGLContext.h"

#include "mozilla/ipc/Shmem.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/layers/OOPCanvasRenderer.h"
#include "mozilla/layers/TextureClientSharedSurface.h"
#include "TexUnpackBlob.h"
#include "WebGLContextEndpoint.h"
#include "mozilla/dom/WebGLContextEvent.h"
#include "HostWebGLContext.h"
#include "WebGLMethodDispatcher.h"
#include "WebGLChild.h"
#include "nsIGfxInfo.h"

namespace mozilla {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(ClientWebGLRefCount)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(ClientWebGLRefCount, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(ClientWebGLRefCount, Release)

static bool GetJSScalarFromGLType(GLenum type,
                                  js::Scalar::Type* const out_scalarType) {
  switch (type) {
    case LOCAL_GL_BYTE:
      *out_scalarType = js::Scalar::Int8;
      return true;

    case LOCAL_GL_UNSIGNED_BYTE:
      *out_scalarType = js::Scalar::Uint8;
      return true;

    case LOCAL_GL_SHORT:
      *out_scalarType = js::Scalar::Int16;
      return true;

    case LOCAL_GL_HALF_FLOAT:
    case LOCAL_GL_HALF_FLOAT_OES:
    case LOCAL_GL_UNSIGNED_SHORT:
    case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
    case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
    case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
      *out_scalarType = js::Scalar::Uint16;
      return true;

    case LOCAL_GL_UNSIGNED_INT:
    case LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV:
    case LOCAL_GL_UNSIGNED_INT_5_9_9_9_REV:
    case LOCAL_GL_UNSIGNED_INT_10F_11F_11F_REV:
    case LOCAL_GL_UNSIGNED_INT_24_8:
      *out_scalarType = js::Scalar::Uint32;
      return true;
    case LOCAL_GL_INT:
      *out_scalarType = js::Scalar::Int32;
      return true;

    case LOCAL_GL_FLOAT:
      *out_scalarType = js::Scalar::Float32;
      return true;

    default:
      return false;
  }
}

/* static */ RefPtr<ClientWebGLContext>
ClientWebGLContext::MakeSingleProcessWebGLContext(WebGLVersion aVersion) {
  UniquePtr<HostWebGLContext> host = HostWebGLContext::Create(aVersion);
  if (!host) {
    return nullptr;
  }

  return new ClientWebGLContext(std::move(host));
}

/* static */ RefPtr<ClientWebGLContext>
ClientWebGLContext::MakeCrossProcessWebGLContext(WebGLVersion aVersion) {
  CompositorBridgeChild* cbc = CompositorBridgeChild::Get();
  MOZ_ASSERT(cbc);
  if (!cbc) {
    return nullptr;
  }

  // Construct the WebGL command queue, used to send commands from the client
  // process to the host for execution.  It takes a response queue that is used
  // to return responses to synchronous messages.
  // TODO: Be smarter in choosing these.
  static const size_t CommandQueueSize = 256 * 1024;  // 256K
  static const size_t ResponseQueueSize = 8 * 1024;   // 8K

  UniquePtr<ProducerConsumerQueue> commandPcq =
      ProducerConsumerQueue::Create(cbc, CommandQueueSize);
  UniquePtr<ProducerConsumerQueue> responsePcq =
      ProducerConsumerQueue::Create(cbc, ResponseQueueSize);
  if ((!commandPcq) || (!responsePcq)) {
    WEBGL_BRIDGE_LOGE("Failed to create command/response PCQ");
    return nullptr;
  }

  UniquePtr<WebGLCrossProcessCommandQueue> commandQueue =
      WebGLCrossProcessCommandQueue::Create(std::move(commandPcq), responsePcq);
  if (!commandQueue) {
    WEBGL_BRIDGE_LOGE("Failed to create WebGLCrossProcessCommandQueue");
    return nullptr;
  }

  // Construct the error and warning queue, used to asynchronously send errors
  // and warnings from the WebGLContext in the host to the DOM in the client.
  // TODO: Be smarter in choosing this.
  static const size_t ErrorQueueSize = 4 * 1024;  // 4K

  UniquePtr<ProducerConsumerQueue> errorPcq =
      ProducerConsumerQueue::Create(cbc, ErrorQueueSize);
  if (!errorPcq) {
    WEBGL_BRIDGE_LOGE("Failed to create error and warning PCQ");
    return nullptr;
  }

  UniquePtr<WebGLErrorQueue> errorQueue =
      WebGLErrorQueue::Create(std::move(errorPcq));
  if (!errorQueue) {
    WEBGL_BRIDGE_LOGE("Failed to create WebGLErrorQueue");
    return nullptr;
  }

  // Use the error/warning and command queues to construct a
  // ClientWebGLContext in this process and a HostWebGLContext
  // in the host process.
  dom::WebGLChild* webGLChild = new dom::WebGLChild();
  webGLChild = static_cast<dom::WebGLChild*>(cbc->SendPWebGLConstructor(
      webGLChild, aVersion, std::move(commandQueue->mSink),
      std::move(errorQueue->mSource)));
  if (!webGLChild) {
    WEBGL_BRIDGE_LOGE("SendPWebGLConstructor failed");
    return nullptr;
  }

  RefPtr<ClientWebGLContext> client = new ClientWebGLContext(
      webGLChild, aVersion, std::move(commandQueue->mSource),
      std::move(errorQueue->mSink));

  // Start the error and warning drain task
  client->DrainErrorQueue(true);
  return client;
}

/* static */ RefPtr<ClientWebGLContext> ClientWebGLContext::Create(
    WebGLVersion aVersion) {
  bool shouldRemoteWebGL = gfxPrefs::WebGLIsRemoted();
  DebugOnly<bool> isHostProcess = XRE_IsGPUProcess() || XRE_IsParentProcess();
  MOZ_ASSERT(!isHostProcess);

  if (shouldRemoteWebGL) {
    return MakeCrossProcessWebGLContext(aVersion);
  }
  return MakeSingleProcessWebGLContext(aVersion);
}

ClientWebGLContext::ClientWebGLContext(UniquePtr<HostWebGLContext>&& aHost)
    : WebGLContextEndpoint(aHost->GetVersion()),
      mWebGLChild(nullptr),
      mHostContext(std::move(aHost)) {
  MOZ_ASSERT(mHostContext);
  mHostContext->SetClientContext(this);
}

ClientWebGLContext::~ClientWebGLContext() {
  RemovePostRefreshObserver();
  if (mWebGLChild) {
    Unused << mWebGLChild->Send__delete__(mWebGLChild);
  }
}

ClientWebGLContext::ClientWebGLContext(
    dom::WebGLChild* aWebGLChild, WebGLVersion aVersion,
    UniquePtr<ClientWebGLCommandSource>&& aCommandSource,
    UniquePtr<ClientWebGLErrorSink>&& aErrorSink)
    : WebGLContextEndpoint(aVersion),
      mCommandSource(std::move(aCommandSource)),
      mErrorSink(std::move(aErrorSink)),
      mWebGLChild(aWebGLChild) {
  MOZ_ASSERT(mCommandSource && mErrorSink && mWebGLChild);
  aWebGLChild->SetContext(this);
}

void DrainWebGLError(nsWeakPtr aWeakContext) {
  nsCOMPtr<nsICanvasRenderingContextInternal> baseContext =
      do_QueryReferent(aWeakContext);
  if (!baseContext) {
    // Do not re-issue the task.
    WEBGL_BRIDGE_LOGI(
        "DrainWebGLError: ClientWebGLContext "
        "has been destroyed.  Stopping.");
    return;
  }

  WEBGL_BRIDGE_LOGV("Draining WebGL Error Queue");
  RefPtr<ClientWebGLContext> context =
      static_cast<ClientWebGLContext*>(baseContext.get());
  context->DrainErrorQueue(true);
}

void ClientWebGLContext::DrainErrorQueue(bool toReissue) {
  RefPtr<ClientWebGLContext> refThis = this;
  mErrorSink->SetClientWebGLContext(refThis);
  bool success = mErrorSink->ProcessAll() == CommandResult::QueueEmpty;
  refThis = nullptr;
  mErrorSink->SetClientWebGLContext(refThis);

  if (!toReissue) {
    return;
  }

  // Re-issue the task if successful.
  nsWeakPtr weakThis = do_GetWeakReference(this);
  auto drainErrorRunnable =
      NewRunnableFunction("DrainWebGLError", &DrainWebGLError, weakThis);
  if ((!success) ||
      NS_FAILED(NS_DelayedDispatchToCurrentThread(
          drainErrorRunnable.downcast<nsIRunnable>(), 10 /* ms */))) {
    MOZ_ASSERT_UNREACHABLE(
        "DrainErrorQueue failed to reissue.  The "
        "error/warning queue will no longer be drained.");
  }
}

bool ClientWebGLContext::UpdateCompositableHandle(
    LayerTransactionChild* aLayerTransaction, CompositableHandle aHandle) {
  // When running OOP WebGL (i.e. when we have a WebGLChild actor), tell the
  // host about the new compositable.  When running in-process, we don't need to
  // care.
  if (mWebGLChild) {
    WEBGL_BRIDGE_LOGI("[%p] Setting CompositableHandle to %" PRIx64, this,
                      aHandle.Value());
    return mWebGLChild->SendUpdateCompositableHandle(aLayerTransaction,
                                                     aHandle);
  }
  return true;
}

void ClientWebGLContext::PostWarning(const nsCString& aWarning) {
  if (!mCanvasElement) {
    return;
  }
  dom::AutoJSAPI api;
  if (!api.Init(mCanvasElement->OwnerDoc()->GetScopeObject())) {
    return;
  }
  JSContext* cx = api.cx();
  // no need to print to stderr, as JS::WarnASCII takes care of this for us.
  WEBGL_BRIDGE_LOGD("[%p] Posting message to console: '%s'", this,
                    aWarning.Data());
  JS::WarnASCII(cx, aWarning.Data());
}

void ClientWebGLContext::OnLostContext() {
  const auto kEventName = NS_LITERAL_STRING("webglcontextlost");
  const auto kCanBubble = CanBubble::eYes;
  const auto kIsCancelable = Cancelable::eYes;
  bool useDefaultHandler;

  WEBGL_BRIDGE_LOGD("[%p] Posting webglcontextlost event", this);
  if (mCanvasElement) {
    nsContentUtils::DispatchTrustedEvent(
        mCanvasElement->OwnerDoc(), static_cast<nsIContent*>(mCanvasElement),
        kEventName, kCanBubble, kIsCancelable, &useDefaultHandler);
  } else {
    // OffscreenCanvas case
    RefPtr<Event> event = new Event(mOffscreenCanvas, nullptr, nullptr);
    event->InitEvent(kEventName, kCanBubble, kIsCancelable);
    event->SetTrusted(true);
    useDefaultHandler = mOffscreenCanvas->DispatchEvent(
        *event, CallerType::System, IgnoreErrors());
  }

  if (!useDefaultHandler) {
    // If we're told to use the default handler, it means the script
    // didn't bother to handle the event. In this case, we shouldn't
    // auto-restore the context.
    AllowContextRestore();
  }
}

void ClientWebGLContext::OnRestoredContext() {
  WEBGL_BRIDGE_LOGD("[%p] Posting webglcontextrestored event", this);
  if (mCanvasElement) {
    nsContentUtils::DispatchTrustedEvent(
        mCanvasElement->OwnerDoc(), static_cast<nsIContent*>(mCanvasElement),
        NS_LITERAL_STRING("webglcontextrestored"), CanBubble::eYes,
        Cancelable::eYes);
  } else {
    RefPtr<Event> event = new Event(mOffscreenCanvas, nullptr, nullptr);
    event->InitEvent(NS_LITERAL_STRING("webglcontextrestored"), CanBubble::eYes,
                     Cancelable::eYes);
    event->SetTrusted(true);
    mOffscreenCanvas->DispatchEvent(*event);
  }
}

void ClientWebGLContext::PostContextCreationError(const nsCString& text) {
  RefPtr<EventTarget> target = mCanvasElement;
  if (!target && mOffscreenCanvas) {
    target = mOffscreenCanvas;
  } else if (!target) {
    nsCString msg;
    msg.AppendPrintf("Failed to create WebGL context: %s", text.BeginReading());
    PostWarning(msg);
    return;
  }

  WEBGL_BRIDGE_LOGD("[%p] Posting webglcontextcreationerror event", this);
  const auto kEventName = NS_LITERAL_STRING("webglcontextcreationerror");

  dom::WebGLContextEventInit eventInit;
  // eventInit.mCancelable = true; // The spec says this, but it's silly.
  eventInit.mStatusMessage = NS_ConvertASCIItoUTF16(text);

  const RefPtr<WebGLContextEvent> event =
      WebGLContextEvent::Constructor(target, kEventName, eventInit);
  event->SetTrusted(true);

  target->DispatchEvent(*event);

  //////

  nsCString msg;
  msg.AppendPrintf("Failed to create WebGL context: %s", text.BeginReading());
  PostWarning(msg);
}

// ---

// Dispatch a command to the host, using data in WebGLMethodDispatcher for
// information: e.g. to choose the right synchronization protocol.
template <typename ReturnType>
struct WebGLClientDispatcher {
  // non-const method
  template <size_t Id, typename... MethodArgs, typename... GivenArgs>
  static ReturnType Run(const ClientWebGLContext& c,
                        ReturnType (HostWebGLContext::*method)(MethodArgs...),
                        GivenArgs&&... aArgs) {
    // Non-void calls must be sync, otherwise what would we return?
    MOZ_ASSERT(WebGLMethodDispatcher::SyncType<Id>() == CommandSyncType::SYNC);
    return c.DispatchSync<Id, ReturnType>(
        static_cast<const MethodArgs&>(aArgs)...);
  }

  // const method
  template <size_t Id, typename... MethodArgs, typename... GivenArgs>
  static ReturnType Run(const ClientWebGLContext& c,
                        ReturnType (HostWebGLContext::*method)(MethodArgs...)
                            const,
                        GivenArgs&&... aArgs) {
    // Non-void calls must be sync, otherwise what would we return?
    MOZ_ASSERT(WebGLMethodDispatcher::SyncType<Id>() == CommandSyncType::SYNC);
    return c.DispatchSync<Id, ReturnType>(
        static_cast<const MethodArgs&>(aArgs)...);
  }
};

template <>
struct WebGLClientDispatcher<void> {
  // non-const method
  template <size_t Id, typename... MethodArgs, typename... GivenArgs>
  static void Run(const ClientWebGLContext& c,
                  void (HostWebGLContext::*method)(MethodArgs...),
                  GivenArgs&&... aArgs) {
    if (WebGLMethodDispatcher::SyncType<Id>() == CommandSyncType::SYNC) {
      c.DispatchVoidSync<Id>(static_cast<const MethodArgs&>(aArgs)...);
    } else {
      c.DispatchAsync<Id>(static_cast<const MethodArgs&>(aArgs)...);
    }
  }

  // const method
  template <size_t Id, typename... MethodArgs, typename... GivenArgs>
  static void Run(const ClientWebGLContext& c,
                  void (HostWebGLContext::*method)(MethodArgs...) const,
                  GivenArgs&&... aArgs) {
    if (WebGLMethodDispatcher::SyncType<Id>() == CommandSyncType::SYNC) {
      c.DispatchVoidSync<Id>(static_cast<const MethodArgs&>(aArgs)...);
    } else {
      c.DispatchAsync<Id>(static_cast<const MethodArgs&>(aArgs)...);
    }
  }
};

// If we are running WebGL in this process then call the HostWebGLContext
// method directly.  Otherwise, dispatch over IPC.
template <
    typename MethodType, MethodType method,
    typename ReturnType = typename FunctionTypeTraits<MethodType>::ReturnType,
    size_t Id = WebGLMethodDispatcher::Id<MethodType, method>(),
    typename... Args>
ReturnType ClientWebGLContext::Run(Args&&... aArgs) const {
  if (!IsHostOOP()) {
    MOZ_ASSERT(mHostContext);
    return ((mHostContext.get())->*method)(std::forward<Args>(aArgs)...);
  }
  return WebGLClientDispatcher<ReturnType>::template Run<Id>(*this, method,
                                                             aArgs...);
}

// -------------------------------------------------------------------------
// Client-side helper methods.  Dispatch to a Host method.
// -------------------------------------------------------------------------

#define RPROC(_METHOD) \
  decltype(&HostWebGLContext::_METHOD), &HostWebGLContext::_METHOD

// ------------------------- Composition, etc -------------------------

void ClientWebGLContext::UpdateLastUseIndex() {
  static CheckedInt<uint64_t> sIndex = 0;

  sIndex++;

  // should never happen with 64-bit; trying to handle this would be riskier
  // than not handling it as the handler code would never get exercised.
  if (!sIndex.isValid())
    MOZ_CRASH("Can't believe it's been 2^64 transactions already!");
  mLastUseIndex = sIndex.value();
}

static uint8_t gWebGLLayerUserData;

class WebGLContextUserData : public LayerUserData {
 public:
  explicit WebGLContextUserData(HTMLCanvasElement* canvas) : mCanvas(canvas) {}

  /* PreTransactionCallback gets called by the Layers code every time the
   * WebGL canvas is going to be composited.
   */
  static void PreTransactionCallback(void* data) {
    ClientWebGLContext* webgl = static_cast<ClientWebGLContext*>(data);

    // Prepare the context for composition
    webgl->BeginComposition();
  }

  /** DidTransactionCallback gets called by the Layers code everytime the WebGL
   * canvas gets composite, so it really is the right place to put actions that
   * have to be performed upon compositing
   */
  static void DidTransactionCallback(void* data) {
    ClientWebGLContext* webgl = static_cast<ClientWebGLContext*>(data);

    // Clean up the context after composition
    webgl->EndComposition();
  }

 private:
  RefPtr<HTMLCanvasElement> mCanvas;
};

void ClientWebGLContext::BeginComposition() {
  // When running single-process WebGL, Present needs to be called in
  // BeginComposition so that it is done _before_ the CanvasRenderer to
  // Update attaches it for composition.
  // When running cross-process WebGL, Present needs to be called in
  // EndComposition so that it happens _after_ the OOPCanvasRenderer's
  // Update tells it what CompositableHost to use,
  if (!IsHostOOP()) {
    MOZ_ASSERT(mHostContext);
    WEBGL_BRIDGE_LOGI("[%p] Presenting", this);
    mHostContext->Present();
  }
}

void ClientWebGLContext::EndComposition() {
  if (IsHostOOP()) {
    WEBGL_BRIDGE_LOGI("[%p] Presenting", this);
    Run<RPROC(Present)>();
  }

  // Mark ourselves as no longer invalidated.
  MarkContextClean();
  UpdateLastUseIndex();
}

already_AddRefed<layers::Layer> ClientWebGLContext::GetCanvasLayer(
    nsDisplayListBuilder* builder, Layer* oldLayer, LayerManager* manager) {
  if (!mResetLayer && oldLayer && oldLayer->HasUserData(&gWebGLLayerUserData)) {
    RefPtr<layers::Layer> ret = oldLayer;
    return ret.forget();
  }

  WEBGL_BRIDGE_LOGI("[%p] Creating WebGL CanvasLayer/Renderer", this);

  RefPtr<CanvasLayer> canvasLayer = manager->CreateCanvasLayer();
  if (!canvasLayer) {
    NS_WARNING("CreateCanvasLayer returned null!");
    return nullptr;
  }

  WebGLContextUserData* userData = nullptr;
  if (builder->IsPaintingToWindow() && mCanvasElement) {
    userData = new WebGLContextUserData(mCanvasElement);
  }

  canvasLayer->SetUserData(&gWebGLLayerUserData, userData);

  CanvasRenderer* canvasRenderer = canvasLayer->CreateOrGetCanvasRenderer();
  if (!InitializeCanvasRenderer(builder, canvasRenderer)) return nullptr;

  uint32_t flags = HasAlphaSupport() ? 0 : Layer::CONTENT_OPAQUE;
  canvasLayer->SetContentFlags(flags);

  mResetLayer = false;

  return canvasLayer.forget();
}

bool ClientWebGLContext::UpdateWebRenderCanvasData(
    nsDisplayListBuilder* aBuilder, WebRenderCanvasData* aCanvasData) {
  CanvasRenderer* renderer = aCanvasData->GetCanvasRenderer();

  if (!mResetLayer && renderer) {
    return true;
  }

  WEBGL_BRIDGE_LOGI("[%p] Creating WebGL WR CanvasLayer/Renderer", this);
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

mozilla::dom::PWebGLChild* ClientWebGLContext::GetWebGLChild() {
  return mWebGLChild;
}

bool ClientWebGLContext::InitializeCanvasRenderer(
    nsDisplayListBuilder* aBuilder, CanvasRenderer* aRenderer) {
  const FuncScope funcScope(this, "<InitializeCanvasRenderer>");
  if (IsContextLost()) return false;

  Maybe<ICRData> icrData =
      Run<RPROC(InitializeCanvasRenderer)>(GetCompositorBackendType());

  if (!icrData) {
    return false;
  }

  mSurfaceInfo = *icrData;

  CanvasInitializeData data;
  if (aBuilder->IsPaintingToWindow() && mCanvasElement) {
    // Make the layer tell us whenever a transaction finishes (including
    // the current transaction), so we can clear our invalidation state and
    // start invalidating again. We need to do this for the layer that is
    // being painted to a window (there shouldn't be more than one at a time,
    // and if there is, flushing the invalidation state more often than
    // necessary is harmless).

    // The layer will be destroyed when we tear down the presentation
    // (at the latest), at which time this userData will be destroyed,
    // releasing the reference to the element.
    // The userData will receive DidTransactionCallbacks, which flush the
    // the invalidation state to indicate that the canvas is up to date.
    data.mPreTransCallback = WebGLContextUserData::PreTransactionCallback;
    data.mPreTransCallbackData = this;
    data.mDidTransCallback = WebGLContextUserData::DidTransactionCallback;
    data.mDidTransCallbackData = this;
  }

  MOZ_ASSERT(mCanvasElement);  // TODO: What to do here?  Is this about
                               // OffscreenCanvas?

  if (IsHostOOP()) {
    data.mOOPRenderer = mCanvasElement->GetOOPCanvasRenderer();
    MOZ_ASSERT(data.mOOPRenderer);
    MOZ_ASSERT((!data.mOOPRenderer->mContext) ||
               (data.mOOPRenderer->mContext == this));
    data.mOOPRenderer->mContext = this;
  } else {
    MOZ_ASSERT(mHostContext);
    data.mGLContext = mHostContext->GetWebGLContext()->gl;
  }

  data.mHasAlpha = mSurfaceInfo.hasAlpha;
  data.mIsGLAlphaPremult = mSurfaceInfo.isPremultAlpha || !data.mHasAlpha;

  // TODO: Do I really need this?  Can't use mSurfaceInfo.size?
  data.mSize = DrawingBufferSize();

  aRenderer->Initialize(data);
  aRenderer->SetDirty();
  return true;
}

layers::LayersBackend ClientWebGLContext::GetCompositorBackendType() const {
  if (mCanvasElement) {
    return mCanvasElement->GetCompositorBackendType();
  } else if (mOffscreenCanvas) {
    return mOffscreenCanvas->GetCompositorBackendType();
  }

  return LayersBackend::LAYERS_NONE;
}

mozilla::dom::Document* ClientWebGLContext::GetOwnerDoc() const {
  MOZ_ASSERT(mCanvasElement);
  if (!mCanvasElement) {
    return nullptr;
  }
  return mCanvasElement->OwnerDoc();
}

void ClientWebGLContext::Commit() {
  if (mOffscreenCanvas) {
    mOffscreenCanvas->CommitFrameToCompositor();
  }
}

void ClientWebGLContext::GetCanvas(
    Nullable<dom::OwningHTMLCanvasElementOrOffscreenCanvas>& retval) {
  if (mCanvasElement) {
    MOZ_RELEASE_ASSERT(!mOffscreenCanvas, "GFX: Canvas is offscreen.");

    if (mCanvasElement->IsInNativeAnonymousSubtree()) {
      retval.SetNull();
    } else {
      retval.SetValue().SetAsHTMLCanvasElement() = mCanvasElement;
    }
  } else if (mOffscreenCanvas) {
    retval.SetValue().SetAsOffscreenCanvas() = mOffscreenCanvas;
  } else {
    retval.SetNull();
  }
}

void ClientWebGLContext::GetContextAttributes(
    dom::Nullable<dom::WebGLContextAttributes>& retval) {
  retval.SetNull();
  const FuncScope funcScope(this, "getContextAttributes");
  if (IsContextLost()) return;

  dom::WebGLContextAttributes& result = retval.SetValue();

  result.mAlpha.Construct(mOptions.alpha);
  result.mDepth = mOptions.depth;
  result.mStencil = mOptions.stencil;
  result.mAntialias = mOptions.antialias;
  result.mPremultipliedAlpha = mOptions.premultipliedAlpha;
  result.mPreserveDrawingBuffer = mOptions.preserveDrawingBuffer;
  result.mFailIfMajorPerformanceCaveat = mOptions.failIfMajorPerformanceCaveat;
  result.mPowerPreference = mOptions.powerPreference;
}

NS_IMETHODIMP
ClientWebGLContext::SetDimensions(int32_t signedWidth, int32_t signedHeight) {
  const FuncScope funcScope(this, "<SetDimensions>");

  WEBGL_BRIDGE_LOGI("[%p] SetDimensions: (%d, %d)", this, signedWidth,
                    signedHeight);

  // May have a OffscreenCanvas instead of an HTMLCanvasElement
  if (GetCanvas()) GetCanvas()->InvalidateCanvas();

  SetDimensionsData data = Run<RPROC(SetDimensions)>(signedWidth, signedHeight);

  // May have a OffscreenCanvas instead of an HTMLCanvasElement
  if (GetCanvas()) GetCanvas()->InvalidateCanvas();

  // if we exceeded either the global or the per-principal limit for WebGL
  // contexts, lose the oldest-used context now to free resources. Note that we
  // can't do that in the ClientWebGLContext constructor as we don't have a
  // canvas element yet there. Here is the right place to do so, as we are about
  // to create the OpenGL context and that is what can fail if we already have
  // too many.
  if (data.mMaybeLostOldContext) {
    LoseOldestWebGLContextIfLimitExceeded();
  }

  mOptions = data.mOptions;
  mOptionsFrozen = data.mOptionsFrozen;
  mResetLayer = data.mResetLayer;
  mPixelStore = data.mPixelStore;
  return data.mResult;
}

gfx::IntSize ClientWebGLContext::DrawingBufferSize() {
  // TODO: This is absurd.
  return Run<RPROC(DrawingBufferSize)>();
}

void ClientWebGLContext::OnMemoryPressure() {
  WEBGL_BRIDGE_LOGI("[%p] OnMemoryPressure", this);
  return Run<RPROC(OnMemoryPressure)>();
}

static bool IsFeatureInBlacklist(const nsCOMPtr<nsIGfxInfo>& gfxInfo,
                                 int32_t feature,
                                 nsCString* const out_blacklistId) {
  int32_t status;
  if (!NS_SUCCEEDED(gfxUtils::ThreadSafeGetFeatureStatus(
          gfxInfo, feature, *out_blacklistId, &status))) {
    return false;
  }

  return status != nsIGfxInfo::FEATURE_STATUS_OK;
}

NS_IMETHODIMP
ClientWebGLContext::SetContextOptions(JSContext* cx,
                                      JS::Handle<JS::Value> options,
                                      ErrorResult& aRvForDictionaryInit) {
  const FuncScope funcScope(this, "getContext");
  (void)IsContextLost();  // Ignore this.

  if (options.isNullOrUndefined() && mOptionsFrozen) return NS_OK;

  WEBGL_BRIDGE_LOGI("[%p] SetContextOptions", this);

  WebGLContextAttributes attributes;
  if (!attributes.Init(cx, options)) {
    aRvForDictionaryInit.Throw(NS_ERROR_UNEXPECTED);
    return NS_ERROR_UNEXPECTED;
  }

  WebGLContextOptions newOpts;

  newOpts.stencil = attributes.mStencil;
  newOpts.depth = attributes.mDepth;
  newOpts.premultipliedAlpha = attributes.mPremultipliedAlpha;
  newOpts.antialias = attributes.mAntialias;
  newOpts.preserveDrawingBuffer = attributes.mPreserveDrawingBuffer;
  newOpts.failIfMajorPerformanceCaveat =
      attributes.mFailIfMajorPerformanceCaveat;
  newOpts.powerPreference = attributes.mPowerPreference;
  newOpts.enableDebugRendererInfo =
      Preferences::GetBool("webgl.enable-debug-renderer-info", false);
  MOZ_ASSERT(mCanvasElement || mOffscreenCanvas);
  newOpts.shouldResistFingerprinting =
      mCanvasElement ?
                     // If we're constructed from a canvas element
          nsContentUtils::ShouldResistFingerprinting(GetOwnerDoc())
                     :
                     // If we're constructed from an offscreen canvas
          nsContentUtils::ShouldResistFingerprinting(
              mOffscreenCanvas->GetOwnerGlobal()->PrincipalOrNull());

  if (attributes.mAlpha.WasPassed()) {
    newOpts.alpha = attributes.mAlpha.Value();
  }

  // Don't do antialiasing if we've disabled MSAA.
  if (!gfxPrefs::MSAALevel()) {
    newOpts.antialias = false;
  }

  if (!gfxPrefs::WebGLForceMSAA()) {
    const nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();

    nsCString blocklistId;
    if (IsFeatureInBlacklist(gfxInfo, nsIGfxInfo::FEATURE_WEBGL_MSAA,
                             &blocklistId)) {
      EnqueueWarning(nsCString(
          "Disallowing antialiased backbuffers due to blacklisting."));
      newOpts.antialias = false;
    }
  }

#if 0
    EnqueueWarning("aaHint: %d stencil: %d depth: %d alpha: %d premult: %d preserve: %d\n",
               newOpts.antialias ? 1 : 0,
               newOpts.stencil ? 1 : 0,
               newOpts.depth ? 1 : 0,
               newOpts.alpha ? 1 : 0,
               newOpts.premultipliedAlpha ? 1 : 0,
               newOpts.preserveDrawingBuffer ? 1 : 0);
#endif

  if (mOptionsFrozen && !(newOpts == mOptions)) {
    // Error if the options are already frozen, and the ones that were asked for
    // aren't the same as what they were originally.
    return NS_ERROR_FAILURE;
  }

  mOptions = newOpts;

  // Send new options to the host
  Run<RPROC(SetContextOptions)>(newOpts);

  return NS_OK;
}

void ClientWebGLContext::AllowContextRestore() {
  Run<RPROC(AllowContextRestore)>();
}

void ClientWebGLContext::DidRefresh() { Run<RPROC(DidRefresh)>(); }

already_AddRefed<mozilla::gfx::SourceSurface>
ClientWebGLContext::GetSurfaceSnapshot(gfxAlphaType* out_alphaType) {
  MOZ_ASSERT_UNREACHABLE("TODO: ClientWebGLContext::GetSurfaceSnapshot");
  return nullptr;
}

UniquePtr<uint8_t[]> ClientWebGLContext::GetImageBuffer(int32_t* out_format) {
  *out_format = 0;

  // Use GetSurfaceSnapshot() to make sure that appropriate y-flip gets applied
  gfxAlphaType any;
  RefPtr<SourceSurface> snapshot = GetSurfaceSnapshot(&any);
  if (!snapshot) return nullptr;

  RefPtr<DataSourceSurface> dataSurface = snapshot->GetDataSurface();

  return gfxUtils::GetImageBuffer(dataSurface, mOptions.premultipliedAlpha,
                                  out_format);
}

NS_IMETHODIMP
ClientWebGLContext::GetInputStream(const char* mimeType,
                                   const nsAString& encoderOptions,
                                   nsIInputStream** out_stream) {
  // Use GetSurfaceSnapshot() to make sure that appropriate y-flip gets applied
  gfxAlphaType any;
  RefPtr<SourceSurface> snapshot = GetSurfaceSnapshot(&any);
  if (!snapshot) return NS_ERROR_FAILURE;

  RefPtr<DataSourceSurface> dataSurface = snapshot->GetDataSurface();
  return gfxUtils::GetInputStream(dataSurface, mOptions.premultipliedAlpha,
                                  mimeType, encoderOptions, out_stream);
}

// ------------------------- Client WebGL Objects -------------------------

// ReleaseWebGLObject must call Remove last since it may delete id.
// NB: MakeWebGLObject adds the newly created object to its strong-reference
// map _and_ increments its ref count again, so the already_AddRefed return
// value has a ref-count of 2.
#define DEFINE_WEBGL_METHODS(_TYPE)                                         \
  void ClientWebGLContext::ReleaseWebGLObject(                              \
      const WebGLId<WebGL##_TYPE>* id) {                                    \
    Run<RPROC(ReleaseWebGLObject<WebGL##_TYPE>)>(*id);                      \
    Remove(*id);                                                            \
  }                                                                         \
  inline already_AddRefed<ClientWebGLObject<WebGL##_TYPE>> MakeWebGLObject( \
      const WebGLId<WebGL##_TYPE>& id, ClientWebGLContext* aContext) {      \
    RefPtr<ClientWebGLObject<WebGL##_TYPE>> ret;                            \
    if (id) {                                                               \
      ret = new ClientWebGL##_TYPE(id.Id(), aContext);                      \
      DebugOnly<bool> ok = aContext->Insert(ret);                           \
      MOZ_ASSERT(ok);                                                       \
    }                                                                       \
    WEBGL_BRIDGE_LOGV("MakeWebGLObject<WebGL" #_TYPE "> id: %" PRIu64,      \
                      id.Id());                                             \
    return ret.forget();                                                    \
  }

DEFINE_WEBGL_METHODS(Buffer)
DEFINE_WEBGL_METHODS(Framebuffer)
DEFINE_WEBGL_METHODS(Program)
DEFINE_WEBGL_METHODS(Query)
DEFINE_WEBGL_METHODS(Renderbuffer)
DEFINE_WEBGL_METHODS(Sampler)
DEFINE_WEBGL_METHODS(Shader)
DEFINE_WEBGL_METHODS(Sync)
DEFINE_WEBGL_METHODS(Texture)
DEFINE_WEBGL_METHODS(TransformFeedback)
DEFINE_WEBGL_METHODS(UniformLocation)
DEFINE_WEBGL_METHODS(VertexArray)

#undef DEFINE_WEBGL_METHODS

template <typename WebGLType>
RefPtr<ClientWebGLObject<WebGLType>> ClientWebGLContext::Make(
    const WebGLId<WebGLType>& aId) {
  RefPtr<ClientWebGLObject<WebGLType>> ret;
  if (!aId) {
    return std::move(ret);
  }

  ret = MakeWebGLObject(aId, this);
  return std::move(ret);
}

// Implement all Create... methods where the client generates the ID.
// (These are WebGL methods, exposed to JS)
#define DEFINE_ASYNC_CREATE_METHOD(_TYPE)                                    \
  already_AddRefed<ClientWebGL##_TYPE> ClientWebGLContext::Create##_TYPE() { \
    auto id = GenerateId<WebGL##_TYPE>();                                    \
    Run<RPROC(Create##_TYPE)>(id);                                           \
    return Make(id).forget().downcast<ClientWebGL##_TYPE>();                 \
  }

#define DEFINE_ASYNC_CREATE_METHOD_EXT(_TYPE)                             \
  already_AddRefed<ClientWebGL##_TYPE> ClientWebGLContext::Create##_TYPE( \
      bool aExt) {                                                        \
    auto id = GenerateId<WebGL##_TYPE>();                                 \
    Run<RPROC(Create##_TYPE)>(id, aExt);                                  \
    return Make(id).forget().downcast<ClientWebGL##_TYPE>();              \
  }

// All but Buffer, Texture and UniformLocation
DEFINE_ASYNC_CREATE_METHOD(Framebuffer)
DEFINE_ASYNC_CREATE_METHOD(Program)
DEFINE_ASYNC_CREATE_METHOD(Renderbuffer)
DEFINE_ASYNC_CREATE_METHOD(Sampler)
DEFINE_ASYNC_CREATE_METHOD(TransformFeedback)
DEFINE_ASYNC_CREATE_METHOD_EXT(Query)
DEFINE_ASYNC_CREATE_METHOD_EXT(VertexArray)

#undef DEFINE_ASYNC_CREATE_METHOD

already_AddRefed<ClientWebGLShader> ClientWebGLContext::CreateShader(
    GLenum type) {
  auto id = GenerateId<WebGLShader>();
  Run<RPROC(CreateShader)>(type, id);
  return Make(id).forget().downcast<ClientWebGLShader>();
}

already_AddRefed<ClientWebGLSync> ClientWebGLContext::FenceSync(
    GLenum condition, GLbitfield flags) {
  auto id = GenerateId<WebGLSync>();
  Run<RPROC(FenceSync)>(id, condition, flags);
  return Make(id).forget().downcast<ClientWebGLSync>();
}

// Implement all Create... methods where the host generates the ID.
// (These are WebGL methods, exposed to JS)

already_AddRefed<ClientWebGLBuffer> ClientWebGLContext::CreateBuffer() {
  return Make<WebGLBuffer>(Run<RPROC(CreateBuffer)>())
      .forget()
      .downcast<ClientWebGLBuffer>();
}

already_AddRefed<ClientWebGLTexture> ClientWebGLContext::CreateTexture() {
  return Make<WebGLTexture>(Run<RPROC(CreateTexture)>())
      .forget()
      .downcast<ClientWebGLTexture>();
}

template <typename WebGLType>
RefPtr<ClientWebGLObject<WebGLType>> ClientWebGLContext::EnsureWebGLObject(
    const WebGLId<WebGLType>& aId) {
  RefPtr<ClientWebGLObject<WebGLType>> ret;
  if (!aId) {
    return std::move(ret);
  }

  ret = Find(aId);
  if (ret) {
    return std::move(ret);
  }

  return Make<WebGLType>(aId);
};

struct MaybeWebGLVariantMatcher {
  MaybeWebGLVariantMatcher(ClientWebGLContext* cxt, JSContext* cx,
                           ErrorResult* rv)
      : mCxt(cxt), mCx(cx), mRv(rv) {}

  JS::Value match(int32_t x) { return JS::NumberValue(x); }
  JS::Value match(int64_t x) { return JS::NumberValue(x); }
  JS::Value match(uint32_t x) { return JS::NumberValue(x); }
  JS::Value match(uint64_t x) { return JS::NumberValue(x); }
  JS::Value match(float x) { return JS::Float32Value(x); }
  JS::Value match(double x) { return JS::DoubleValue(x); }
  JS::Value match(bool x) { return JS::BooleanValue(x); }
  JS::Value match(const nsCString& x) {
    return StringValue(mCx, x.BeginReading(), *mRv);
  }
  JS::Value match(const nsString& x) { return StringValue(mCx, x, *mRv); }
  JS::Value match(const WebGLShaderPrecisionFormat& x) {
    MOZ_ASSERT_UNREACHABLE(
        "Should not convert WebGLShaderPrecisionFormat to JS::Value");
    return JS::NullValue();
  }

  template <size_t Length>
  JS::Value match(const Array<int32_t, Length>& x) {
    JSObject* obj = dom::Int32Array::Create(
        mCx, mCxt, static_cast<const int32_t(&)[Length]>(x));
    if (!obj) {
      *mRv = NS_ERROR_OUT_OF_MEMORY;
      mCxt->EnqueueErrorOutOfMemory("ToJSValue: Out of memory.");
    }
    return JS::ObjectOrNullValue(obj);
  }

  template <size_t Length>
  JS::Value match(const Array<uint32_t, Length>& x) {
    JSObject* obj = dom::Uint32Array::Create(
        mCx, mCxt, static_cast<const uint32_t(&)[Length]>(x));
    if (!obj) {
      *mRv = NS_ERROR_OUT_OF_MEMORY;
      mCxt->EnqueueErrorOutOfMemory("ToJSValue: Out of memory.");
    }
    return JS::ObjectOrNullValue(obj);
  }

  template <size_t Length>
  JS::Value match(const Array<float, Length>& x) {
    JSObject* obj = dom::Float32Array::Create(
        mCx, mCxt, static_cast<const float(&)[Length]>(x));
    if (!obj) {
      *mRv = NS_ERROR_OUT_OF_MEMORY;
      mCxt->EnqueueErrorOutOfMemory("ToJSValue: Out of memory.");
    }
    return JS::ObjectOrNullValue(obj);
  }

  template <size_t Length>
  JS::Value match(const Array<bool, Length>& x) {
    JS::Rooted<JS::Value> obj(mCx);
    if (!dom::ToJSValue(mCx, static_cast<const bool(&)[Length]>(x), &obj)) {
      *mRv = NS_ERROR_OUT_OF_MEMORY;
      mCxt->EnqueueErrorOutOfMemory("ToJSValue: Out of memory.");
    }
    return obj;
  }

  JS::Value match(const nsTArray<uint32_t>& x) {
    JSObject* obj = dom::Uint32Array::Create(mCx, mCxt, x.Length(), &x[0]);
    if (!obj) {
      *mRv = NS_ERROR_OUT_OF_MEMORY;
      mCxt->EnqueueErrorOutOfMemory("ToJSValue: Out of memory.");
    }
    return JS::ObjectOrNullValue(obj);
  }

  JS::Value match(const nsTArray<int32_t>& x) {
    JSObject* obj = dom::Int32Array::Create(mCx, mCxt, x.Length(), &x[0]);
    if (!obj) {
      *mRv = NS_ERROR_OUT_OF_MEMORY;
      mCxt->EnqueueErrorOutOfMemory("ToJSValue: Out of memory.");
    }
    return JS::ObjectOrNullValue(obj);
  }

  JS::Value match(const nsTArray<float>& x) {
    JSObject* obj = dom::Float32Array::Create(mCx, mCxt, x.Length(), &x[0]);
    if (!obj) {
      *mRv = NS_ERROR_OUT_OF_MEMORY;
      mCxt->EnqueueErrorOutOfMemory("ToJSValue: Out of memory.");
    }
    return JS::ObjectOrNullValue(obj);
  }

  JS::Value match(const nsTArray<bool>& x) {
    JS::Rooted<JS::Value> obj(mCx);
    if (!dom::ToJSValue(mCx, &x[0], x.Length(), &obj)) {
      *mRv = NS_ERROR_OUT_OF_MEMORY;
      mCxt->EnqueueErrorOutOfMemory("ToJSValue: Out of memory.");
    }
    return obj;
  }

  template <typename WebGLClass>
  JS::Value match(const WebGLId<WebGLClass>& x) {
    return mCxt->WebGLObjectAsJSValue(mCx, mCxt->EnsureWebGLObject(x), *mRv);
  }

 private:
  // Create a JS::Value from a C string
  JS::Value StringValue(JSContext* cx, const char* chars, ErrorResult& rv) {
    JSString* str = JS_NewStringCopyZ(cx, chars);
    if (!str) {
      rv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return JS::NullValue();
    }

    return JS::StringValue(str);
  }

  // Create a JS::Value from an nsAString
  JS::Value StringValue(JSContext* cx, const nsAString& str, ErrorResult& er) {
    JSString* jsStr = JS_NewUCStringCopyN(cx, str.BeginReading(), str.Length());
    if (!jsStr) {
      er.Throw(NS_ERROR_OUT_OF_MEMORY);
      return JS::NullValue();
    }

    return JS::StringValue(jsStr);
  }

  ClientWebGLContext* mCxt;
  JSContext* mCx;
  ErrorResult* mRv;
};

JS::Value ClientWebGLContext::ToJSValue(JSContext* cx,
                                        const MaybeWebGLVariant& aVariant,
                                        ErrorResult& rv) const {
  if (!aVariant) {
    return JS::NullValue();
  }
  return aVariant.ref().match(
      MaybeWebGLVariantMatcher(const_cast<ClientWebGLContext*>(this), cx, &rv));
}

// ------------------------- GL State -------------------------
bool ClientWebGLContext::IsContextLost() const {
  return Run<RPROC(IsContextLost)>();
}

void ClientWebGLContext::Disable(GLenum cap) { Run<RPROC(Disable)>(cap); }

void ClientWebGLContext::Enable(GLenum cap) { Run<RPROC(Enable)>(cap); }

bool ClientWebGLContext::IsEnabled(GLenum cap) {
  return Run<RPROC(IsEnabled)>(cap);
}

void ClientWebGLContext::GetProgramInfoLog(const WebGLId<WebGLProgram>& prog,
                                           nsAString& retval) {
  retval = Run<RPROC(GetProgramInfoLog)>(prog);
}

void ClientWebGLContext::GetShaderInfoLog(const WebGLId<WebGLShader>& shader,
                                          nsAString& retval) {
  retval = Run<RPROC(GetShaderInfoLog)>(shader);
}

void ClientWebGLContext::GetShaderSource(const WebGLId<WebGLShader>& shader,
                                         nsAString& retval) {
  retval = Run<RPROC(GetShaderSource)>(shader);
}

void ClientWebGLContext::GetParameter(JSContext* cx, GLenum pname,
                                      JS::MutableHandle<JS::Value> retval,
                                      ErrorResult& rv) {
  retval.set(ToJSValue(cx, Run<RPROC(GetParameter)>(pname), rv));
}

void ClientWebGLContext::GetBufferParameter(
    JSContext* cx, GLenum target, GLenum pname,
    JS::MutableHandle<JS::Value> retval) {
  ErrorResult unused;
  retval.set(
      ToJSValue(cx, Run<RPROC(GetBufferParameter)>(target, pname), unused));
}

void ClientWebGLContext::GetFramebufferAttachmentParameter(
    JSContext* cx, GLenum target, GLenum attachment, GLenum pname,
    JS::MutableHandle<JS::Value> retval, ErrorResult& rv) {
  retval.set(ToJSValue(
      cx,
      Run<RPROC(GetFramebufferAttachmentParameter)>(target, attachment, pname),
      rv));
}

void ClientWebGLContext::GetProgramParameter(
    JSContext* cx, const WebGLId<WebGLProgram>& prog, GLenum pname,
    JS::MutableHandle<JS::Value> retval) {
  ErrorResult unused;
  retval.set(
      ToJSValue(cx, Run<RPROC(GetProgramParameter)>(prog, pname), unused));
}

void ClientWebGLContext::GetRenderbufferParameter(
    JSContext* cx, GLenum target, GLenum pname,
    JS::MutableHandle<JS::Value> retval) {
  ErrorResult unused;
  retval.set(ToJSValue(cx, Run<RPROC(GetRenderbufferParameter)>(target, pname),
                       unused));
}

void ClientWebGLContext::GetShaderParameter(
    JSContext* cx, const WebGLId<WebGLShader>& shader, GLenum pname,
    JS::MutableHandle<JS::Value> retval) {
  ErrorResult unused;
  retval.set(
      ToJSValue(cx, Run<RPROC(GetShaderParameter)>(shader, pname), unused));
}

void ClientWebGLContext::GetIndexedParameter(JSContext* cx, GLenum target,
                                             GLuint index,
                                             JS::MutableHandleValue retval,
                                             ErrorResult& rv) {
  ErrorResult unused;
  retval.set(
      ToJSValue(cx, Run<RPROC(GetIndexedParameter)>(target, index), unused));
}

void ClientWebGLContext::GetUniform(JSContext* cx,
                                    const WebGLId<WebGLProgram>& prog,
                                    const WebGLId<WebGLUniformLocation>& loc,
                                    JS::MutableHandle<JS::Value> retval) {
  ErrorResult ignored;
  retval.set(ToJSValue(cx, Run<RPROC(GetUniform)>(prog, loc), ignored));
}

already_AddRefed<ClientWebGLUniformLocation>
ClientWebGLContext::GetUniformLocation(const WebGLId<WebGLProgram>& prog,
                                       const nsAString& name) {
  return EnsureWebGLObject(Run<RPROC(GetUniformLocation)>(prog, nsString(name)))
      .forget()
      .downcast<ClientWebGLUniformLocation>();
}

already_AddRefed<ClientWebGLShaderPrecisionFormat>
ClientWebGLContext::GetShaderPrecisionFormat(GLenum shadertype,
                                             GLenum precisiontype) {
  MaybeWebGLVariant response =
      Run<RPROC(GetShaderPrecisionFormat)>(shadertype, precisiontype);
  if ((!response) || (!response.ref().is<WebGLShaderPrecisionFormat>())) {
    MOZ_ASSERT(!response, "Expected response to be WebGLShaderPrecisionFormat");
    return nullptr;
  }

  return MakeAndAddRef<ClientWebGLShaderPrecisionFormat>(
      response.ref().as<WebGLShaderPrecisionFormat>());
}

void ClientWebGLContext::BindAttribLocation(const WebGLId<WebGLProgram>& prog,
                                            GLuint location,
                                            const nsAString& name) {
  return Run<RPROC(BindAttribLocation)>(prog, location, nsString(name));
}

GLint ClientWebGLContext::GetAttribLocation(const WebGLId<WebGLProgram>& prog,
                                            const nsAString& name) {
  return Run<RPROC(GetAttribLocation)>(prog, nsString(name));
}

void ClientWebGLContext::AttachShader(const WebGLId<WebGLProgram>& progId,
                                      const WebGLId<WebGLShader>& shaderId) {
  Run<RPROC(AttachShader)>(progId, shaderId);
}

void ClientWebGLContext::ShaderSource(const WebGLId<WebGLShader>& shader,
                                      const nsAString& source) {
  Run<RPROC(ShaderSource)>(shader, nsString(source));
}

void ClientWebGLContext::BindRenderbuffer(
    GLenum target, const WebGLId<WebGLRenderbuffer>& fb) {
  Run<RPROC(BindRenderbuffer)>(target, fb);
}

void ClientWebGLContext::BlendColor(GLclampf r, GLclampf g, GLclampf b,
                                    GLclampf a) {
  Run<RPROC(BlendColor)>(r, g, b, a);
}

void ClientWebGLContext::BlendEquation(GLenum mode) {
  Run<RPROC(BlendEquation)>(mode);
}

void ClientWebGLContext::BlendEquationSeparate(GLenum modeRGB,
                                               GLenum modeAlpha) {
  Run<RPROC(BlendEquationSeparate)>(modeRGB, modeAlpha);
}

void ClientWebGLContext::BlendFunc(GLenum sfactor, GLenum dfactor) {
  Run<RPROC(BlendEquationSeparate)>(sfactor, dfactor);
}

void ClientWebGLContext::BlendFuncSeparate(GLenum srcRGB, GLenum dstRGB,
                                           GLenum srcAlpha, GLenum dstAlpha) {
  Run<RPROC(BlendFuncSeparate)>(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

GLenum ClientWebGLContext::CheckFramebufferStatus(GLenum target) {
  return Run<RPROC(CheckFramebufferStatus)>(target);
}

void ClientWebGLContext::Clear(GLbitfield mask) {
  Run<RPROC(Clear)>(mask);
  // TODO: We need to invalidate if the target was the screen buffer.
  // As of right now we don't know so I'm being conservative.
  Invalidate();
}

void ClientWebGLContext::ClearColor(GLclampf r, GLclampf g, GLclampf b,
                                    GLclampf a) {
  Run<RPROC(ClearColor)>(r, g, b, a);
}

void ClientWebGLContext::ClearDepth(GLclampf v) { Run<RPROC(ClearDepth)>(v); }

void ClientWebGLContext::ClearStencil(GLint v) { Run<RPROC(ClearStencil)>(v); }

void ClientWebGLContext::ColorMask(WebGLboolean r, WebGLboolean g,
                                   WebGLboolean b, WebGLboolean a) {
  Run<RPROC(ColorMask)>(r, g, b, a);
}

void ClientWebGLContext::CompileShader(const WebGLId<WebGLShader>& shaderId) {
  Run<RPROC(CompileShader)>(shaderId);
}

void ClientWebGLContext::CullFace(GLenum face) { Run<RPROC(CullFace)>(face); }

void ClientWebGLContext::DepthFunc(GLenum func) { Run<RPROC(DepthFunc)>(func); }

void ClientWebGLContext::DepthMask(WebGLboolean b) { Run<RPROC(DepthMask)>(b); }

void ClientWebGLContext::DepthRange(GLclampf zNear, GLclampf zFar) {
  Run<RPROC(DepthRange)>(zNear, zFar);
}

void ClientWebGLContext::DetachShader(const WebGLId<WebGLProgram>& progId,
                                      const WebGLId<WebGLShader>& shaderId) {
  Run<RPROC(DetachShader)>(progId, shaderId);
}

void ClientWebGLContext::Flush() { Run<RPROC(Flush)>(); }

void ClientWebGLContext::Finish() { Run<RPROC(Finish)>(); }

void ClientWebGLContext::FrontFace(GLenum mode) { Run<RPROC(FrontFace)>(mode); }

GLenum ClientWebGLContext::GetError() { return Run<RPROC(GetError)>(); }

void ClientWebGLContext::Hint(GLenum target, GLenum mode) {
  Run<RPROC(Hint)>(target, mode);
}

void ClientWebGLContext::LineWidth(GLfloat width) {
  Run<RPROC(LineWidth)>(width);
}

void ClientWebGLContext::LinkProgram(const WebGLId<WebGLProgram>& progId) {
  Run<RPROC(LinkProgram)>(progId);
}

void ClientWebGLContext::PixelStorei(GLenum pname, GLint param) {
  mPixelStore = Run<RPROC(PixelStorei)>(pname, param);
}

void ClientWebGLContext::PolygonOffset(GLfloat factor, GLfloat units) {
  Run<RPROC(PolygonOffset)>(factor, units);
}

void ClientWebGLContext::SampleCoverage(GLclampf value, WebGLboolean invert) {
  Run<RPROC(SampleCoverage)>(value, invert);
}

void ClientWebGLContext::Scissor(GLint x, GLint y, GLsizei width,
                                 GLsizei height) {
  Run<RPROC(Scissor)>(x, y, width, height);
}

void ClientWebGLContext::StencilFunc(GLenum func, GLint ref, GLuint mask) {
  Run<RPROC(StencilFunc)>(func, ref, mask);
}

void ClientWebGLContext::StencilFuncSeparate(GLenum face, GLenum func,
                                             GLint ref, GLuint mask) {
  Run<RPROC(StencilFuncSeparate)>(face, func, ref, mask);
}

void ClientWebGLContext::StencilMask(GLuint mask) {
  Run<RPROC(StencilMask)>(mask);
}

void ClientWebGLContext::StencilMaskSeparate(GLenum face, GLuint mask) {
  Run<RPROC(StencilMaskSeparate)>(face, mask);
}

void ClientWebGLContext::StencilOp(GLenum sfail, GLenum dpfail, GLenum dppass) {
  Run<RPROC(StencilOp)>(sfail, dpfail, dppass);
}

void ClientWebGLContext::StencilOpSeparate(GLenum face, GLenum sfail,
                                           GLenum dpfail, GLenum dppass) {
  Run<RPROC(StencilOpSeparate)>(face, sfail, dpfail, dppass);
}

void ClientWebGLContext::Viewport(GLint x, GLint y, GLsizei width,
                                  GLsizei height) {
  Run<RPROC(Viewport)>(x, y, width, height);
}

// ------------------------- Buffer Objects -------------------------
void ClientWebGLContext::BindBuffer(GLenum target,
                                    const WebGLId<WebGLBuffer>& buffer) {
  Run<RPROC(BindBuffer)>(target, buffer);
}

void ClientWebGLContext::BindBufferBase(GLenum target, GLuint index,
                                        const WebGLId<WebGLBuffer>& buffer) {
  Run<RPROC(BindBufferBase)>(target, index, buffer);
}

void ClientWebGLContext::BindBufferRange(GLenum target, GLuint index,
                                         const WebGLId<WebGLBuffer>& buffer,
                                         WebGLintptr offset,
                                         WebGLsizeiptr size) {
  Run<RPROC(BindBufferRange)>(target, index, buffer, offset, size);
}

void ClientWebGLContext::GetBufferSubData(GLenum target, GLintptr srcByteOffset,
                                          const dom::ArrayBufferView& dstData,
                                          GLuint dstElemOffset,
                                          GLuint dstElemCountOverride) {
  if (!ValidateNonNegative("srcByteOffset", srcByteOffset)) return;

  uint8_t* bytes;
  size_t byteLen;
  if (!ValidateArrayBufferView(dstData, dstElemOffset, dstElemCountOverride,
                               LOCAL_GL_INVALID_VALUE, &bytes, &byteLen)) {
    return;
  }

  Maybe<UniquePtr<RawBuffer<>>> result =
      Run<RPROC(GetBufferSubData)>(target, srcByteOffset, byteLen);
  if (!result) {
    return;
  }
  MOZ_ASSERT(result.ref()->Length() == byteLen);
  memcpy(bytes, result.ref()->Data(), byteLen);
}

////

void ClientWebGLContext::BufferData(GLenum target, WebGLsizeiptr size,
                                    GLenum usage) {
  const FuncScope funcScope(this, "bufferData");
  if (!ValidateNonNegative("size", size)) return;

  UniqueBuffer zeroBuffer(calloc(size, 1));
  if (!zeroBuffer) return EnqueueErrorOutOfMemory("Failed to allocate zeros.");

  Run<RPROC(BufferData)>(
      target, RawBuffer<>(size_t(size), (uint8_t*)zeroBuffer.get()), usage);
}

void ClientWebGLContext::BufferData(
    GLenum target, const dom::Nullable<dom::ArrayBuffer>& maybeSrc,
    GLenum usage) {
  const FuncScope funcScope(this, "bufferData");
  if (!ValidateNonNull("src", maybeSrc)) return;
  const auto& src = maybeSrc.Value();

  src.ComputeLengthAndData();
  Run<RPROC(BufferData)>(
      target, RawBuffer<>(src.LengthAllowShared(), src.DataAllowShared()),
      usage);
}

void ClientWebGLContext::BufferData(GLenum target,
                                    const dom::ArrayBufferView& src,
                                    GLenum usage, GLuint srcElemOffset,
                                    GLuint srcElemCountOverride) {
  const FuncScope funcScope(this, "bufferData");
  uint8_t* bytes;
  size_t byteLen;
  if (!ValidateArrayBufferView(src, srcElemOffset, srcElemCountOverride,
                               LOCAL_GL_INVALID_VALUE, &bytes, &byteLen)) {
    return;
  }

  Run<RPROC(BufferData)>(target, RawBuffer<>(byteLen, bytes), usage);
}

////

void ClientWebGLContext::BufferSubData(GLenum target,
                                       WebGLsizeiptr dstByteOffset,
                                       const dom::ArrayBuffer& src) {
  const FuncScope funcScope(this, "bufferSubData");
  src.ComputeLengthAndData();
  Run<RPROC(BufferSubData)>(
      target, dstByteOffset,
      RawBuffer<>(src.LengthAllowShared(), src.DataAllowShared()));
}

void ClientWebGLContext::BufferSubData(GLenum target,
                                       WebGLsizeiptr dstByteOffset,
                                       const dom::ArrayBufferView& src,
                                       GLuint srcElemOffset,
                                       GLuint srcElemCountOverride) {
  const FuncScope funcScope(this, "bufferSubData");
  uint8_t* bytes;
  size_t byteLen;
  if (!ValidateArrayBufferView(src, srcElemOffset, srcElemCountOverride,
                               LOCAL_GL_INVALID_VALUE, &bytes, &byteLen)) {
    return;
  }

  Run<RPROC(BufferSubData)>(target, dstByteOffset, RawBuffer<>(byteLen, bytes));
}

void ClientWebGLContext::CopyBufferSubData(GLenum readTarget,
                                           GLenum writeTarget,
                                           GLintptr readOffset,
                                           GLintptr writeOffset,
                                           GLsizeiptr size) {
  Run<RPROC(CopyBufferSubData)>(readTarget, writeTarget, readOffset,
                                writeOffset, size);
}

// -------------------------- Framebuffer Objects --------------------------

void ClientWebGLContext::DeleteFramebuffer(
    const WebGLId<WebGLFramebuffer>& aFb) {
  Run<RPROC(DeleteFramebuffer)>(aFb);
}

void ClientWebGLContext::BindFramebuffer(GLenum target,
                                         const WebGLId<WebGLFramebuffer>& fb) {
  Run<RPROC(BindFramebuffer)>(target, fb);
}

void ClientWebGLContext::FramebufferRenderbuffer(
    GLenum target, GLenum attachment, GLenum rbTarget,
    const WebGLId<WebGLRenderbuffer>& rb) {
  Run<RPROC(FramebufferRenderbuffer)>(target, attachment, rbTarget, rb);
}

void ClientWebGLContext::FramebufferTexture2D(GLenum target, GLenum attachment,
                                              GLenum texImageTarget,
                                              const WebGLId<WebGLTexture>& tex,
                                              GLint level) {
  Run<RPROC(FramebufferTexture2D)>(target, attachment, texImageTarget, tex,
                                   level);
}

void ClientWebGLContext::BlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1,
                                         GLint srcY1, GLint dstX0, GLint dstY0,
                                         GLint dstX1, GLint dstY1,
                                         GLbitfield mask, GLenum filter) {
  Run<RPROC(BlitFramebuffer)>(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1,
                              dstY1, mask, filter);

  // TODO: We need to invalidate if the target was the screen buffer.
  // As of right now we don't know so I'm being conservative.
  Invalidate();
}

void ClientWebGLContext::FramebufferTextureLayer(
    GLenum target, GLenum attachment, const WebGLId<WebGLTexture>& textureId,
    GLint level, GLint layer) {
  Run<RPROC(FramebufferTextureLayer)>(target, attachment, textureId, level,
                                      layer);
}

void ClientWebGLContext::InvalidateFramebuffer(
    GLenum target, const dom::Sequence<GLenum>& attachments,
    ErrorResult& unused) {
  Run<RPROC(InvalidateFramebuffer)>(target, nsTArray<GLenum>(attachments));
}

void ClientWebGLContext::InvalidateSubFramebuffer(
    GLenum target, const dom::Sequence<GLenum>& attachments, GLint x, GLint y,
    GLsizei width, GLsizei height, ErrorResult& unused) {
  Run<RPROC(InvalidateSubFramebuffer)>(target, nsTArray<GLenum>(attachments), x,
                                       y, width, height);
}

void ClientWebGLContext::ReadBuffer(GLenum mode) {
  Run<RPROC(ReadBuffer)>(mode);
}

// ----------------------- Renderbuffer objects -----------------------
void ClientWebGLContext::DeleteRenderbuffer(
    const WebGLId<WebGLRenderbuffer>& aRb) {
  Run<RPROC(DeleteRenderbuffer)>(aRb);
}

void ClientWebGLContext::GetInternalformatParameter(
    JSContext* cx, GLenum target, GLenum internalformat, GLenum pname,
    JS::MutableHandleValue retval, ErrorResult& rv) {
  Maybe<nsTArray<int32_t>> maybeArr =
      Run<RPROC(GetInternalformatParameter)>(target, internalformat, pname);
  if (!maybeArr) {
    retval.setObjectOrNull(nullptr);
    return;
  }

  nsTArray<int32_t>& arr = maybeArr.ref();
  // zero-length array indicates out-of-memory
  JSObject* obj = arr.Length()
                      ? dom::Int32Array::Create(cx, this, arr.Length(), &arr[0])
                      : nullptr;
  if (!obj) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  retval.setObjectOrNull(obj);
}

void ClientWebGLContext::RenderbufferStorage(GLenum target,
                                             GLenum internalFormat,
                                             GLsizei width, GLsizei height) {
  const FuncScope scope(this, FuncScopeId::renderbufferStorage);
  Run<RPROC(RenderbufferStorage_base)>(target, 0, internalFormat, width, height,
                                       GetFuncScopeId());
}

void ClientWebGLContext::RenderbufferStorageMultisample(GLenum target,
                                                        GLsizei samples,
                                                        GLenum internalFormat,
                                                        GLsizei width,
                                                        GLsizei height) {
  const FuncScope scope(this, FuncScopeId::renderbufferStorageMultisample);
  Run<RPROC(RenderbufferStorage_base)>(target, samples, internalFormat, width,
                                       height, GetFuncScopeId());
}

// --------------------------- Texture objects ---------------------------
void ClientWebGLContext::DeleteTexture(const WebGLId<WebGLTexture>& aTex) {
  Run<RPROC(DeleteTexture)>(aTex);
}

void ClientWebGLContext::ActiveTexture(GLenum texUnit) {
  Run<RPROC(ActiveTexture)>(texUnit);
}

void ClientWebGLContext::BindTexture(GLenum texTarget,
                                     const WebGLId<WebGLTexture>& tex) {
  Run<RPROC(BindTexture)>(texTarget, tex);
}

void ClientWebGLContext::GenerateMipmap(GLenum texTarget) {
  Run<RPROC(GenerateMipmap)>(texTarget);
}

void ClientWebGLContext::CopyTexImage2D(GLenum target, GLint level,
                                        GLenum internalFormat, GLint x, GLint y,
                                        GLsizei rawWidth, GLsizei rawHeight,
                                        GLint border) {
  uint32_t width, height, depth;
  if (!ValidateExtents(rawWidth, rawHeight, 1, border, &width, &height,
                       &depth)) {
    return;
  }

  Run<RPROC(CopyTexImage2D)>(target, level, internalFormat, x, y, width, height,
                             depth);
}

void ClientWebGLContext::GetTexParameter(JSContext* cx, GLenum texTarget,
                                         GLenum pname,
                                         JS::MutableHandle<JS::Value> retval) {
  ErrorResult ignored;
  retval.set(
      ToJSValue(cx, Run<RPROC(GetTexParameter)>(texTarget, pname), ignored));
}

void ClientWebGLContext::TexParameterf(GLenum texTarget, GLenum pname,
                                       GLfloat param) {
  Run<RPROC(TexParameter_base)>(texTarget, pname, FloatOrInt(param));
}

void ClientWebGLContext::TexParameteri(GLenum texTarget, GLenum pname,
                                       GLint param) {
  Run<RPROC(TexParameter_base)>(texTarget, pname, FloatOrInt(param));
}

void ClientWebGLContext::TexStorage2D(GLenum target, GLsizei levels,
                                      GLenum internalFormat, GLsizei width,
                                      GLsizei height) {
  const FuncScope scope(this, FuncScopeId::TexStorage2D);
  const uint8_t funcDims = 2;
  const GLsizei depth = 1;
  Run<RPROC(TexStorage)>(funcDims, target, levels, internalFormat, width,
                         height, depth, GetFuncScopeId());
}

void ClientWebGLContext::TexStorage3D(GLenum target, GLsizei levels,
                                      GLenum internalFormat, GLsizei width,
                                      GLsizei height, GLsizei depth) {
  const FuncScope scope(this, FuncScopeId::TexStorage3D);
  const uint8_t funcDims = 3;
  Run<RPROC(TexStorage)>(funcDims, target, levels, internalFormat, width,
                         height, depth, GetFuncScopeId());
}

////////////////////////////////////

static inline bool DoesJSTypeMatchUnpackType(GLenum unpackType,
                                             js::Scalar::Type jsType) {
  switch (unpackType) {
    case LOCAL_GL_BYTE:
      return jsType == js::Scalar::Type::Int8;

    case LOCAL_GL_UNSIGNED_BYTE:
      return jsType == js::Scalar::Type::Uint8 ||
             jsType == js::Scalar::Type::Uint8Clamped;

    case LOCAL_GL_SHORT:
      return jsType == js::Scalar::Type::Int16;

    case LOCAL_GL_UNSIGNED_SHORT:
    case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
    case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
    case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
    case LOCAL_GL_HALF_FLOAT:
    case LOCAL_GL_HALF_FLOAT_OES:
      return jsType == js::Scalar::Type::Uint16;

    case LOCAL_GL_INT:
      return jsType == js::Scalar::Type::Int32;

    case LOCAL_GL_UNSIGNED_INT:
    case LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV:
    case LOCAL_GL_UNSIGNED_INT_10F_11F_11F_REV:
    case LOCAL_GL_UNSIGNED_INT_5_9_9_9_REV:
    case LOCAL_GL_UNSIGNED_INT_24_8:
      return jsType == js::Scalar::Type::Uint32;

    case LOCAL_GL_FLOAT:
      return jsType == js::Scalar::Type::Float32;

    default:
      return false;
  }
}

////////////////////////////////////

bool ClientWebGLContext::ValidateViewType(GLenum unpackType,
                                          const TexImageSource& src) {
  if (!src.mView) return true;
  const auto& view = *(src.mView);

  const auto& jsType = view.Type();
  if (!DoesJSTypeMatchUnpackType(unpackType, jsType)) {
    EnqueueErrorInvalidOperation(
        "ArrayBufferView type not compatible with `type`.");
    return false;
  }

  return true;
}

////////////////////////////////////

void ClientWebGLContext::TexImage2D(GLenum target, GLint level,
                                    GLenum internalFormat, GLsizei width,
                                    GLsizei height, GLint border,
                                    GLenum unpackFormat, GLenum unpackType,
                                    const TexImageSource& src) {
  const FuncScope scope(this, FuncScopeId::texImage2D);
  const uint8_t funcDims = 2;
  const GLsizei depth = 1;

  if (!ValidateViewType(unpackType, src)) {
    return;
  }

  MaybeWebGLTexUnpackVariant blob =
      From(target, width, height, depth, border, src);
  if (!blob) {
    return;
  }

  Run<RPROC(TexImage)>(funcDims, target, level, internalFormat, width, height,
                       depth, border, unpackFormat, unpackType, std::move(blob),
                       GetFuncScopeId());
}

////////////////////////////////////

void ClientWebGLContext::TexSubImage2D(GLenum target, GLint level,
                                       GLint xOffset, GLint yOffset,
                                       GLsizei width, GLsizei height,
                                       GLenum unpackFormat, GLenum unpackType,
                                       const TexImageSource& src) {
  const FuncScope scope(this, FuncScopeId::texSubImage2D);
  const uint8_t funcDims = 2;
  const GLint zOffset = 0;
  const GLsizei depth = 1;

  if (!ValidateViewType(unpackType, src)) {
    return;
  }

  MaybeWebGLTexUnpackVariant blob = From(target, width, height, depth, 0, src);
  if (!blob) {
    return;
  }

  Run<RPROC(TexSubImage)>(funcDims, target, level, xOffset, yOffset, zOffset,
                          width, height, depth, unpackFormat, unpackType,
                          std::move(blob), GetFuncScopeId());
}

////////////////////////////////////

void ClientWebGLContext::TexImage3D(GLenum target, GLint level,
                                    GLenum internalFormat, GLsizei width,
                                    GLsizei height, GLsizei depth, GLint border,
                                    GLenum unpackFormat, GLenum unpackType,
                                    const TexImageSource& src) {
  const FuncScope scope(this, FuncScopeId::texImage3D);
  const uint8_t funcDims = 3;

  MaybeWebGLTexUnpackVariant blob =
      From(target, width, height, depth, border, src);
  if (!blob) {
    return;
  }
  Run<RPROC(TexImage)>(funcDims, target, level, internalFormat, width, height,
                       depth, border, unpackFormat, unpackType, std::move(blob),
                       GetFuncScopeId());
}

////////////////////////////////////

void ClientWebGLContext::TexSubImage3D(GLenum target, GLint level,
                                       GLint xOffset, GLint yOffset,
                                       GLint zOffset, GLsizei width,
                                       GLsizei height, GLsizei depth,
                                       GLenum unpackFormat, GLenum unpackType,
                                       const TexImageSource& src) {
  const FuncScope scope(this, FuncScopeId::texSubImage3D);
  const uint8_t funcDims = 3;

  MaybeWebGLTexUnpackVariant blob = From(target, width, height, depth, 0, src);
  if (!blob) {
    return;
  }
  Run<RPROC(TexSubImage)>(funcDims, target, level, xOffset, yOffset, zOffset,
                          width, height, depth, unpackFormat, unpackType,
                          std::move(blob), GetFuncScopeId());
}

////////////////////////////////////

void ClientWebGLContext::CopyTexSubImage2D(GLenum target, GLint level,
                                           GLint xOffset, GLint yOffset,
                                           GLint x, GLint y, GLsizei rawWidth,
                                           GLsizei rawHeight) {
  const FuncScope scope(this, FuncScopeId::copyTexSubImage2D);
  const uint8_t funcDims = 2;
  const GLint zOffset = 0;

  uint32_t width, height, depth;
  if (!ValidateExtents(rawWidth, rawHeight, 1, 0, &width, &height, &depth)) {
    return;
  }

  Run<RPROC(CopyTexSubImage)>(funcDims, target, level, xOffset, yOffset,
                              zOffset, x, y, width, height, depth,
                              GetFuncScopeId());
}

////////////////////////////////////

void ClientWebGLContext::CopyTexSubImage3D(GLenum target, GLint level,
                                           GLint xOffset, GLint yOffset,
                                           GLint zOffset, GLint x, GLint y,
                                           GLsizei rawWidth,
                                           GLsizei rawHeight) {
  const FuncScope scope(this, FuncScopeId::copyTexSubImage3D);
  const uint8_t funcDims = 3;

  uint32_t width, height, depth;
  if (!ValidateExtents(rawWidth, rawHeight, 1, 0, &width, &height, &depth)) {
    return;
  }

  Run<RPROC(CopyTexSubImage)>(funcDims, target, level, xOffset, yOffset,
                              zOffset, x, y, width, height, depth,
                              GetFuncScopeId());
}

void ClientWebGLContext::TexImage(uint8_t funcDims, GLenum target, GLint level,
                                  GLenum internalFormat, GLsizei width,
                                  GLsizei height, GLsizei depth, GLint border,
                                  GLenum unpackFormat, GLenum unpackType,
                                  const TexImageSource& src,
                                  FuncScopeId aFuncId) {
  const FuncScope scope(this, FuncScopeId::texImage2D);
  MaybeWebGLTexUnpackVariant blob =
      From(target, width, height, depth, border, src);
  if (!blob) {
    return;
  }
  Run<RPROC(TexImage)>(funcDims, target, level, internalFormat, width, height,
                       depth, border, unpackFormat, unpackType, std::move(blob),
                       aFuncId);
}

void ClientWebGLContext::TexSubImage(uint8_t funcDims, GLenum target,
                                     GLint level, GLint xOffset, GLint yOffset,
                                     GLint zOffset, GLsizei width,
                                     GLsizei height, GLsizei depth,
                                     GLenum unpackFormat, GLenum unpackType,
                                     const TexImageSource& src,
                                     FuncScopeId aFuncId) {
  MaybeWebGLTexUnpackVariant blob = From(target, width, height, depth, 0, src);
  if (!blob) {
    return;
  }
  Run<RPROC(TexSubImage)>(funcDims, target, level, xOffset, yOffset, zOffset,
                          width, height, depth, unpackFormat, unpackType,
                          std::move(blob), aFuncId);
}

void ClientWebGLContext::CompressedTexImage(
    uint8_t funcDims, GLenum target, GLint level, GLenum internalFormat,
    GLsizei width, GLsizei height, GLsizei depth, GLint border,
    const TexImageSource& src, const Maybe<GLsizei>& expectedImageSize,
    FuncScopeId aFuncId) {
  MaybeWebGLTexUnpackVariant blob = FromCompressed(
      target, width, height, depth, border, src, expectedImageSize);
  if (!blob) {
    return;
  }
  Run<RPROC(CompressedTexImage)>(funcDims, target, level, internalFormat, width,
                                 height, depth, border, std::move(blob),
                                 expectedImageSize, aFuncId);
}

void ClientWebGLContext::CompressedTexSubImage(
    uint8_t funcDims, GLenum target, GLint level, GLint xOffset, GLint yOffset,
    GLint zOffset, GLsizei width, GLsizei height, GLsizei depth,
    GLenum unpackFormat, const TexImageSource& src,
    const Maybe<GLsizei>& expectedImageSize, FuncScopeId aFuncId) {
  MaybeWebGLTexUnpackVariant blob =
      FromCompressed(target, width, height, depth, 0, src, expectedImageSize);
  if (!blob) {
    return;
  }
  Run<RPROC(CompressedTexSubImage)>(
      funcDims, target, level, xOffset, yOffset, zOffset, width, height, depth,
      unpackFormat, std::move(blob), expectedImageSize, aFuncId);
}

// ------------------- Programs and shaders --------------------------------
void ClientWebGLContext::DeleteProgram(const WebGLId<WebGLProgram>& aProg) {
  Run<RPROC(DeleteProgram)>(aProg);
}

void ClientWebGLContext::DeleteShader(const WebGLId<WebGLShader>& aShader) {
  Run<RPROC(DeleteShader)>(aShader);
}

void ClientWebGLContext::UseProgram(const WebGLId<WebGLProgram>& prog) {
  Run<RPROC(UseProgram)>(prog);
}

void ClientWebGLContext::GetAttachedShaders(
    const WebGLId<WebGLProgram>& prog,
    dom::Nullable<nsTArray<RefPtr<ClientWebGLShader>>>& retval) {
  MaybeAttachedShaders shaders = Run<RPROC(GetAttachedShaders)>(prog);
  if (!shaders) {
    retval.SetNull();
    return;
  }

  nsTArray<RefPtr<ClientWebGLShader>>& shaderArray = retval.SetValue();
  for (size_t i = 0; i < shaders.ref().Length; ++i) {
    if (!shaders.ref()[i]) {
      continue;
    }
    auto shader = Find(shaders.ref()[i]);
    if (!shader) {
      MOZ_ASSERT_UNREACHABLE("Returned a missing shader?");
      continue;
    }

    shaderArray.AppendElement(downcast(std::move(shader)));
  }
}

void ClientWebGLContext::ValidateProgram(const WebGLId<WebGLProgram>& prog) {
  Run<RPROC(ValidateProgram)>(prog);
}

GLint ClientWebGLContext::GetFragDataLocation(const WebGLId<WebGLProgram>& prog,
                                              const nsAString& name) {
  return Run<RPROC(GetFragDataLocation)>(prog, nsString(name));
}

// ------------------------ Uniforms and attributes ------------------------
already_AddRefed<ClientWebGLActiveInfo> ClientWebGLContext::GetActiveAttrib(
    const WebGLId<WebGLProgram>& prog, GLuint index) {
  Maybe<WebGLActiveInfo> response = Run<RPROC(GetActiveAttrib)>(prog, index);
  return response ? MakeAndAddRef<ClientWebGLActiveInfo>(this, response.ref())
                  : nullptr;
}

already_AddRefed<ClientWebGLActiveInfo> ClientWebGLContext::GetActiveUniform(
    const WebGLId<WebGLProgram>& prog, GLuint index) {
  Maybe<WebGLActiveInfo> response = Run<RPROC(GetActiveUniform)>(prog, index);
  return response ? MakeAndAddRef<ClientWebGLActiveInfo>(this, response.ref())
                  : nullptr;
}

void ClientWebGLContext::GetActiveUniforms(
    JSContext* cx, const WebGLId<WebGLProgram>& prog,
    const dom::Sequence<GLuint>& uniformIndices, GLenum pname,
    JS::MutableHandleValue retval) {
  ErrorResult unused;
  retval.set(ToJSValue(cx,
                       Run<RPROC(GetActiveUniforms)>(
                           prog, nsTArray<uint32_t>(uniformIndices), pname),
                       unused));
}

void ClientWebGLContext::GetUniformIndices(
    const WebGLId<WebGLProgram>& prog,
    const dom::Sequence<nsString>& uniformNames,
    dom::Nullable<nsTArray<GLuint>>& retval) {
  MaybeWebGLVariant response =
      Run<RPROC(GetUniformIndices)>(prog, nsTArray<nsString>(uniformNames));
  if ((!response) || !(response.ref().template is<nsTArray<uint32_t>>())) {
    MOZ_ASSERT(!response, "response has wrong type");
    retval.SetNull();
    return;
  }
  retval.SetValue() = response.ref().template as<nsTArray<uint32_t>>();
}

void ClientWebGLContext::GetActiveUniformBlockParameter(
    JSContext* cx, const WebGLId<WebGLProgram>& prog, GLuint uniformBlockIndex,
    GLenum pname, JS::MutableHandleValue retval, ErrorResult& rv) {
  retval.set(ToJSValue(cx,
                       Run<RPROC(GetActiveUniformBlockParameter)>(
                           prog, uniformBlockIndex, pname),
                       rv));
}

void ClientWebGLContext::GetActiveUniformBlockName(
    const WebGLId<WebGLProgram>& prog, GLuint uniformBlockIndex,
    nsAString& retval) {
  retval = Run<RPROC(GetActiveUniformBlockName)>(prog, uniformBlockIndex);
}

GLuint ClientWebGLContext::GetUniformBlockIndex(
    const WebGLId<WebGLProgram>& prog, const nsAString& uniformBlockName) {
  return Run<RPROC(GetUniformBlockIndex)>(prog, nsString(uniformBlockName));
}

void ClientWebGLContext::GetVertexAttrib(JSContext* cx, GLuint index,
                                         GLenum pname,
                                         JS::MutableHandle<JS::Value> retval,
                                         ErrorResult& rv) {
  retval.set(ToJSValue(cx, Run<RPROC(GetVertexAttrib)>(index, pname), rv));
}

void ClientWebGLContext::Uniform1f(const WebGLId<WebGLUniformLocation>& aLoc,
                                   GLfloat x) {
  Run<RPROC(UniformFVec)>(aLoc, nsTArray<GLfloat>({x}));
}

void ClientWebGLContext::Uniform2f(const WebGLId<WebGLUniformLocation>& aLoc,
                                   GLfloat x, GLfloat y) {
  Run<RPROC(UniformFVec)>(aLoc, nsTArray<GLfloat>({x, y}));
}

void ClientWebGLContext::Uniform3f(const WebGLId<WebGLUniformLocation>& aLoc,
                                   GLfloat x, GLfloat y, GLfloat z) {
  Run<RPROC(UniformFVec)>(aLoc, nsTArray<GLfloat>({x, y, z}));
}

void ClientWebGLContext::Uniform4f(const WebGLId<WebGLUniformLocation>& aLoc,
                                   GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  Run<RPROC(UniformFVec)>(aLoc, nsTArray<GLfloat>({x, y, z, w}));
}

void ClientWebGLContext::Uniform1i(const WebGLId<WebGLUniformLocation>& aLoc,
                                   GLint x) {
  Run<RPROC(UniformIVec)>(aLoc, nsTArray<GLint>({x}));
}

void ClientWebGLContext::Uniform2i(const WebGLId<WebGLUniformLocation>& aLoc,
                                   GLint x, GLint y) {
  Run<RPROC(UniformIVec)>(aLoc, nsTArray<GLint>({x, y}));
}

void ClientWebGLContext::Uniform3i(const WebGLId<WebGLUniformLocation>& aLoc,
                                   GLint x, GLint y, GLint z) {
  Run<RPROC(UniformIVec)>(aLoc, nsTArray<GLint>({x, y, z}));
}

void ClientWebGLContext::Uniform4i(const WebGLId<WebGLUniformLocation>& aLoc,
                                   GLint x, GLint y, GLint z, GLint w) {
  Run<RPROC(UniformIVec)>(aLoc, nsTArray<GLint>({x, y, z, w}));
}

void ClientWebGLContext::Uniform1ui(const WebGLId<WebGLUniformLocation>& aLoc,
                                    GLuint x) {
  Run<RPROC(UniformUIVec)>(aLoc, nsTArray<GLuint>({x}));
}

void ClientWebGLContext::Uniform2ui(const WebGLId<WebGLUniformLocation>& aLoc,
                                    GLuint x, GLuint y) {
  Run<RPROC(UniformUIVec)>(aLoc, nsTArray<GLuint>({x, y}));
}

void ClientWebGLContext::Uniform3ui(const WebGLId<WebGLUniformLocation>& aLoc,
                                    GLuint x, GLuint y, GLuint z) {
  Run<RPROC(UniformUIVec)>(aLoc, nsTArray<GLuint>({x, y, z}));
}

void ClientWebGLContext::Uniform4ui(const WebGLId<WebGLUniformLocation>& aLoc,
                                    GLuint x, GLuint y, GLuint z, GLuint w) {
  Run<RPROC(UniformUIVec)>(aLoc, nsTArray<GLuint>({x, y, z, w}));
}

#define FOO(DIM)                                                          \
  void ClientWebGLContext::Uniform##DIM##fv(                              \
      WebGLId<WebGLUniformLocation> loc, const Float32ListU& list,        \
      GLuint elemOffset, GLuint elemCountOverride) {                      \
    const auto& arr = Float32Arr::From(list);                             \
    Run<RPROC(UniformNfv)>(                                               \
        nsCString("uniform" #DIM "fv"), (uint8_t)DIM, loc,                \
        RawBuffer<const float>(arr.elemCount, arr.elemBytes), elemOffset, \
        elemCountOverride);                                               \
  }

FOO(1)
FOO(2)
FOO(3)
FOO(4)

#undef FOO

//////

#define FOO(DIM)                                                            \
  void ClientWebGLContext::Uniform##DIM##iv(                                \
      WebGLId<WebGLUniformLocation> loc, const Int32ListU& list,            \
      GLuint elemOffset, GLuint elemCountOverride) {                        \
    const auto& arr = Int32Arr::From(list);                                 \
    Run<RPROC(UniformNiv)>(                                                 \
        nsCString("uniform" #DIM "iv"), (uint8_t)DIM, loc,                  \
        RawBuffer<const int32_t>(arr.elemCount, arr.elemBytes), elemOffset, \
        elemCountOverride);                                                 \
  }

FOO(1)
FOO(2)
FOO(3)
FOO(4)

#undef FOO

//////

#define FOO(DIM)                                                             \
  void ClientWebGLContext::Uniform##DIM##uiv(                                \
      WebGLId<WebGLUniformLocation> loc, const Uint32ListU& list,            \
      GLuint elemOffset, GLuint elemCountOverride) {                         \
    const auto& arr = Uint32Arr::From(list);                                 \
    Run<RPROC(UniformNuiv)>(                                                 \
        nsCString("uniform" #DIM "uiv"), (uint8_t)DIM, loc,                  \
        RawBuffer<const uint32_t>(arr.elemCount, arr.elemBytes), elemOffset, \
        elemCountOverride);                                                  \
  }

FOO(1)
FOO(2)
FOO(3)
FOO(4)

#undef FOO

//////

#define FOO(DIM, ROW, COL)                                                     \
  void ClientWebGLContext::UniformMatrix##DIM##fv(                             \
      WebGLId<WebGLUniformLocation> loc, bool transpose,                       \
      const Float32ListU& list, GLuint elemOffset, GLuint elemCountOverride) { \
    const auto& arr = Float32Arr::From(list);                                  \
    Run<RPROC(UniformMatrixAxBfv)>(                                            \
        nsCString("uniformMatrix" #DIM "fv"), (uint8_t)ROW, (uint8_t)COL, loc, \
        transpose, RawBuffer<const float>(arr.elemCount, arr.elemBytes),       \
        elemOffset, elemCountOverride);                                        \
  }

FOO(2, 2, 2)
FOO(2x3, 2, 3)
FOO(2x4, 2, 4)

FOO(3x2, 3, 2)
FOO(3, 3, 3)
FOO(3x4, 3, 4)

FOO(4x2, 4, 2)
FOO(4x3, 4, 3)
FOO(4, 4, 4)

#undef FOO

void ClientWebGLContext::UniformBlockBinding(
    const WebGLId<WebGLProgram>& progId, GLuint uniformBlockIndex,
    GLuint uniformBlockBinding) {
  Run<RPROC(UniformBlockBinding)>(progId, uniformBlockIndex,
                                  uniformBlockBinding);
}

void ClientWebGLContext::EnableVertexAttribArray(GLuint index) {
  Run<RPROC(EnableVertexAttribArray)>(index);
}

void ClientWebGLContext::DisableVertexAttribArray(GLuint index) {
  Run<RPROC(DisableVertexAttribArray)>(index);
}

WebGLsizeiptr ClientWebGLContext::GetVertexAttribOffset(GLuint index,
                                                        GLenum pname) {
  return Run<RPROC(GetVertexAttribOffset)>(index, pname);
}

void ClientWebGLContext::VertexAttrib1f(GLuint index, GLfloat x) {
  Run<RPROC(VertexAttrib4f)>(index, x, 0, 0, 1, FuncScopeId::vertexAttrib1f);
}

void ClientWebGLContext::VertexAttrib2f(GLuint index, GLfloat x, GLfloat y) {
  Run<RPROC(VertexAttrib4f)>(index, x, y, 0, 1, FuncScopeId::vertexAttrib2f);
}

void ClientWebGLContext::VertexAttrib3f(GLuint index, GLfloat x, GLfloat y,
                                        GLfloat z) {
  Run<RPROC(VertexAttrib4f)>(index, x, y, z, 1, FuncScopeId::vertexAttrib3f);
}

void ClientWebGLContext::VertexAttrib1fv(GLuint index,
                                         const Float32ListU& list) {
  const FuncScope funcScope(this, FuncScopeId::vertexAttrib1fv);
  const auto& arr = Float32Arr::From(list);
  if (!ValidateAttribArraySetter(1, arr.elemCount)) return;

  Run<RPROC(VertexAttrib4f)>(index, arr.elemBytes[0], 0, 0, 1,
                             GetFuncScopeId());
}

void ClientWebGLContext::VertexAttrib2fv(GLuint index,
                                         const Float32ListU& list) {
  const FuncScope funcScope(this, FuncScopeId::vertexAttrib2fv);
  const auto& arr = Float32Arr::From(list);
  if (!ValidateAttribArraySetter(2, arr.elemCount)) return;

  Run<RPROC(VertexAttrib4f)>(index, arr.elemBytes[0], arr.elemBytes[1], 0, 1,
                             GetFuncScopeId());
}

void ClientWebGLContext::VertexAttrib3fv(GLuint index,
                                         const Float32ListU& list) {
  const FuncScope funcScope(this, FuncScopeId::vertexAttrib3fv);
  const auto& arr = Float32Arr::From(list);
  if (!ValidateAttribArraySetter(3, arr.elemCount)) return;

  Run<RPROC(VertexAttrib4f)>(index, arr.elemBytes[0], arr.elemBytes[1],
                             arr.elemBytes[2], 1, GetFuncScopeId());
}

void ClientWebGLContext::VertexAttrib4fv(GLuint index,
                                         const Float32ListU& list) {
  const FuncScope funcScope(this, FuncScopeId::vertexAttrib4fv);
  const auto& arr = Float32Arr::From(list);
  if (!ValidateAttribArraySetter(4, arr.elemCount)) return;

  Run<RPROC(VertexAttrib4f)>(index, arr.elemBytes[0], arr.elemBytes[1],
                             arr.elemBytes[2], arr.elemBytes[3],
                             GetFuncScopeId());
}

void ClientWebGLContext::VertexAttribIPointer(GLuint index, GLint size,
                                              GLenum type, GLsizei stride,
                                              WebGLintptr byteOffset) {
  const bool isFuncInt = true;
  const bool normalized = false;
  Run<RPROC(VertexAttribAnyPointer)>(isFuncInt, index, size, type, normalized,
                                     stride, byteOffset,
                                     FuncScopeId::vertexAttribIPointer);
}

void ClientWebGLContext::VertexAttrib4f(GLuint index, GLfloat x, GLfloat y,
                                        GLfloat z, GLfloat w,
                                        FuncScopeId aFuncId) {
  Run<RPROC(VertexAttrib4f)>(index, x, y, z, w, aFuncId);
}

void ClientWebGLContext::VertexAttribI4i(GLuint index, GLint x, GLint y,
                                         GLint z, GLint w,
                                         FuncScopeId aFuncId) {
  Run<RPROC(VertexAttribI4i)>(index, x, y, z, w, aFuncId);
}

void ClientWebGLContext::VertexAttribI4ui(GLuint index, GLuint x, GLuint y,
                                          GLuint z, GLuint w,
                                          FuncScopeId aFuncId) {
  Run<RPROC(VertexAttribI4ui)>(index, x, y, z, w, aFuncId);
}

void ClientWebGLContext::VertexAttribI4iv(GLuint index,
                                          const Int32ListU& list) {
  FuncScope scope(this, FuncScopeId::vertexAttribI4iv);

  const auto& arr = Int32Arr::From(list);
  if (!ValidateAttribArraySetter(4, arr.elemCount)) return;

  const auto& itr = arr.elemBytes;
  Run<RPROC(VertexAttribI4i)>(index, itr[0], itr[1], itr[2], itr[3],
                              FuncScopeId::vertexAttribI4iv);
}

void ClientWebGLContext::VertexAttribI4uiv(GLuint index,
                                           const Uint32ListU& list) {
  FuncScope scope(this, FuncScopeId::vertexAttribI4uiv);

  const auto& arr = Uint32Arr::From(list);
  if (!ValidateAttribArraySetter(4, arr.elemCount)) return;

  const auto& itr = arr.elemBytes;
  Run<RPROC(VertexAttribI4ui)>(index, itr[0], itr[1], itr[2], itr[3],
                               FuncScopeId::vertexAttribI4uiv);
}

void ClientWebGLContext::VertexAttribPointer(GLuint index, GLint size,
                                             GLenum type,
                                             WebGLboolean normalized,
                                             GLsizei stride,
                                             WebGLintptr byteOffset) {
  const bool isFuncInt = false;
  Run<RPROC(VertexAttribAnyPointer)>(isFuncInt, index, size, type, normalized,
                                     stride, byteOffset,
                                     FuncScopeId::vertexAttribPointer);
}

// -------------------------------- Drawing -------------------------------
void ClientWebGLContext::DrawArrays(GLenum mode, GLint first, GLsizei count) {
  Run<RPROC(DrawArraysInstanced)>(mode, first, count, 1, false);
  // TODO: We need to invalidate if the target was the screen buffer.
  // As of right now we don't know so I'm being conservative.
  Invalidate();
}

void ClientWebGLContext::DrawElements(GLenum mode, GLsizei count, GLenum type,
                                      WebGLintptr byteOffset) {
  Run<RPROC(DrawElementsInstanced)>(mode, count, type, byteOffset, 1,
                                    FuncScopeId::drawElements, false);
  // TODO: We need to invalidate if the target was the screen buffer.
  // As of right now we don't know so I'm being conservative.
  Invalidate();
}

void ClientWebGLContext::DrawRangeElements(GLenum mode, GLuint start,
                                           GLuint end, GLsizei count,
                                           GLenum type,
                                           WebGLintptr byteOffset) {
  Run<RPROC(DrawRangeElements)>(mode, start, end, count, type, byteOffset);
  // TODO: We need to invalidate if the target was the screen buffer.
  // As of right now we don't know so I'm being conservative.
  Invalidate();
}

// ------------------------------ Readback -------------------------------
void ClientWebGLContext::ReadPixels(GLint x, GLint y, GLsizei width,
                                    GLsizei height, GLenum format, GLenum type,
                                    WebGLsizeiptr offset,
                                    dom::CallerType aCallerType,
                                    ErrorResult& out_error) {
  const FuncScope funcScope(this, "readPixels");
  if (!ReadPixels_SharedPrecheck(aCallerType, out_error)) return;
  Run<RPROC(ReadPixels1)>(x, y, width, height, format, type, offset);
}

void ClientWebGLContext::ReadPixels(GLint x, GLint y, GLsizei width,
                                    GLsizei height, GLenum format, GLenum type,
                                    const dom::ArrayBufferView& dstData,
                                    GLuint dstElemOffset,
                                    dom::CallerType aCallerType,
                                    ErrorResult& out_error) {
  const FuncScope funcScope(this, "readPixels");
  if (!ReadPixels_SharedPrecheck(aCallerType, out_error)) return;

  ////

  js::Scalar::Type reqScalarType;
  if (!GetJSScalarFromGLType(type, &reqScalarType)) {
    nsCString name;
    WebGLContext::EnumName(type, &name);
    EnqueueErrorInvalidEnumInfo("type: invalid enum value %s",
                                name.BeginReading());
    return;
  }

  const auto& viewElemType = dstData.Type();
  if (viewElemType != reqScalarType) {
    EnqueueErrorInvalidOperation("`pixels` type does not match `type`.");
    return;
  }

  uint8_t* bytes;
  size_t byteLen;
  if (!ValidateArrayBufferView(dstData, dstElemOffset, 0,
                               LOCAL_GL_INVALID_VALUE, &bytes, &byteLen)) {
    return;
  }

  Maybe<UniquePtr<RawBuffer<>>> result =
      Run<RPROC(ReadPixels2)>(x, y, width, height, format, type, byteLen);
  if (!result) {
    return;
  }
  MOZ_ASSERT(result.ref()->Length() == byteLen);
  memcpy(bytes, result.ref()->Data(), byteLen);
}

bool ClientWebGLContext::ReadPixels_SharedPrecheck(CallerType aCallerType,
                                                   ErrorResult& out_error) {
  if (mCanvasElement && mCanvasElement->IsWriteOnly() &&
      aCallerType != CallerType::System) {
    EnqueueWarning("readPixels: Not allowed");
    out_error.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return false;
  }

  return true;
}

// ------------------------------ Vertex Array ------------------------------
bool ClientWebGLContext::IsVertexArray(const ClientWebGLVertexArray* obj,
                                       bool aFromExtension) {
  return obj && obj->IsValidForContext(this) &&
         (!aFromExtension ||
          UseExtension(WebGLExtensionID::OES_vertex_array_object));
}

void ClientWebGLContext::DeleteVertexArray(
    const WebGLId<WebGLVertexArray>& array, bool aFromExtension) {
  Run<RPROC(DeleteVertexArray)>(array, aFromExtension);
}

void ClientWebGLContext::BindVertexArray(const WebGLId<WebGLVertexArray>& array,
                                         bool aFromExtension) {
  Run<RPROC(BindVertexArray)>(array, aFromExtension);
}

void ClientWebGLContext::DrawArraysInstanced(GLenum mode, GLint first,
                                             GLsizei count, GLsizei primcount,
                                             bool aFromExtension) {
  Run<RPROC(DrawArraysInstanced)>(mode, first, count, primcount,
                                  aFromExtension);
  // TODO: We need to invalidate if the target was the screen buffer.
  // As of right now we don't know so I'm being conservative.
  Invalidate();
}

void ClientWebGLContext::DrawElementsInstanced(GLenum mode, GLsizei count,
                                               GLenum type, WebGLintptr offset,
                                               GLsizei primcount,
                                               FuncScopeId aFuncId,
                                               bool aFromExtension) {
  Run<RPROC(DrawElementsInstanced)>(mode, count, type, offset, primcount,
                                    aFuncId, aFromExtension);
  // TODO: We need to invalidate if the target was the screen buffer.
  // As of right now we don't know so I'm being conservative.
  Invalidate();
}

void ClientWebGLContext::VertexAttribDivisor(GLuint index, GLuint divisor,
                                             bool aFromExtension) {
  Run<RPROC(VertexAttribDivisor)>(index, divisor, aFromExtension);
}

// --------------------------------- GL Query ---------------------------------
void ClientWebGLContext::GetQuery(JSContext* cx, GLenum target, GLenum pname,
                                  JS::MutableHandleValue retval,
                                  bool aFromExtension) const {
  ErrorResult ignored;
  retval.set(ToJSValue(cx, Run<RPROC(GetQuery)>(target, pname, aFromExtension),
                       ignored));
}

void ClientWebGLContext::GetQueryParameter(JSContext* cx,
                                           const WebGLId<WebGLQuery>& query,
                                           GLenum pname,
                                           JS::MutableHandleValue retval,
                                           bool aFromExtension) const {
  ErrorResult ignored;
  retval.set(
      ToJSValue(cx, Run<RPROC(GetQueryParameter)>(query, pname, aFromExtension),
                ignored));
}

void ClientWebGLContext::DeleteQuery(const WebGLId<WebGLQuery>& query,
                                     bool aFromExtension) const {
  Run<RPROC(DeleteQuery)>(query, aFromExtension);
}

void ClientWebGLContext::BeginQuery(GLenum target,
                                    const WebGLId<WebGLQuery>& query,
                                    bool aFromExtension) const {
  Run<RPROC(BeginQuery)>(target, query, aFromExtension);
}

void ClientWebGLContext::EndQuery(GLenum target, bool aFromExtension) const {
  Run<RPROC(EndQuery)>(target, aFromExtension);
}

void ClientWebGLContext::QueryCounter(const WebGLId<WebGLQuery>& query,
                                      GLenum target) const {
  Run<RPROC(QueryCounter)>(query, target);
}

// --------------------------- Buffer Operations --------------------------
void ClientWebGLContext::DeleteBuffer(const WebGLId<WebGLBuffer>& aBuf) {
  Run<RPROC(DeleteBuffer)>(aBuf);
}

void ClientWebGLContext::ClearBufferfv(GLenum buffer, GLint drawBuffer,
                                       const Float32ListU& list,
                                       GLuint srcElemOffset) {
  const auto& arr = Float32Arr::From(list);
  Run<RPROC(ClearBufferfv)>(
      buffer, drawBuffer, RawBuffer<const float>(arr.elemCount, arr.elemBytes),
      srcElemOffset);
  // TODO: We need to invalidate if the target was the screen buffer.
  // As of right now we don't know so I'm being conservative.
  Invalidate();
}

void ClientWebGLContext::ClearBufferiv(GLenum buffer, GLint drawBuffer,
                                       const Int32ListU& list,
                                       GLuint srcElemOffset) {
  const auto& arr = Int32Arr::From(list);
  Run<RPROC(ClearBufferiv)>(
      buffer, drawBuffer,
      RawBuffer<const int32_t>(arr.elemCount, arr.elemBytes), srcElemOffset);
  // TODO: We need to invalidate if the target was the screen buffer.
  // As of right now we don't know so I'm being conservative.
  Invalidate();
}

void ClientWebGLContext::ClearBufferuiv(GLenum buffer, GLint drawBuffer,
                                        const Uint32ListU& list,
                                        GLuint srcElemOffset) {
  const auto& arr = Uint32Arr::From(list);
  Run<RPROC(ClearBufferuiv)>(
      buffer, drawBuffer,
      RawBuffer<const uint32_t>(arr.elemCount, arr.elemBytes), srcElemOffset);
  // TODO: We need to invalidate if the target was the screen buffer.
  // As of right now we don't know so I'm being conservative.
  Invalidate();
}

void ClientWebGLContext::ClearBufferfi(GLenum buffer, GLint drawBuffer,
                                       GLfloat depth, GLint stencil) {
  Run<RPROC(ClearBufferfi)>(buffer, drawBuffer, depth, stencil);
  // TODO: We need to invalidate if the target was the screen buffer.
  // As of right now we don't know so I'm being conservative.
  Invalidate();
}

// -------------------------------- Sampler -------------------------------
void ClientWebGLContext::GetSamplerParameter(
    JSContext* cx, const WebGLId<WebGLSampler>& sampler, GLenum pname,
    JS::MutableHandleValue retval) {
  ErrorResult ignored;
  retval.set(
      ToJSValue(cx, Run<RPROC(GetSamplerParameter)>(sampler, pname), ignored));
}

void ClientWebGLContext::DeleteSampler(const WebGLId<WebGLSampler>& aId) {
  Run<RPROC(DeleteSampler)>(aId);
}

void ClientWebGLContext::BindSampler(GLuint unit,
                                     const WebGLId<WebGLSampler>& sampler) {
  Run<RPROC(BindSampler)>(unit, sampler);
}

void ClientWebGLContext::SamplerParameteri(
    const WebGLId<WebGLSampler>& samplerId, GLenum pname, GLint param) {
  Run<RPROC(SamplerParameteri)>(samplerId, pname, param);
}

void ClientWebGLContext::SamplerParameterf(
    const WebGLId<WebGLSampler>& samplerId, GLenum pname, GLfloat param) {
  Run<RPROC(SamplerParameterf)>(samplerId, pname, param);
}

// ------------------------------- GL Sync ---------------------------------
void ClientWebGLContext::GetSyncParameter(JSContext* cx,
                                          const WebGLId<WebGLSync>& sync,
                                          GLenum pname,
                                          JS::MutableHandleValue retval) {
  ErrorResult ignored;
  retval.set(ToJSValue(cx, Run<RPROC(GetSyncParameter)>(sync, pname), ignored));
}

GLenum ClientWebGLContext::ClientWaitSync(const WebGLId<WebGLSync>& sync,
                                          GLbitfield flags, GLuint64 timeout) {
  return Run<RPROC(ClientWaitSync)>(sync, flags, timeout);
}

void ClientWebGLContext::WaitSync(const WebGLId<WebGLSync>& sync,
                                  GLbitfield flags, GLint64 timeout) {
  Run<RPROC(WaitSync)>(sync, flags, timeout);
}

void ClientWebGLContext::DeleteSync(const WebGLId<WebGLSync>& aId) {
  Run<RPROC(DeleteSync)>(aId);
}

// -------------------------- Transform Feedback ---------------------------
void ClientWebGLContext::DeleteTransformFeedback(
    const WebGLId<WebGLTransformFeedback>& tf) {
  Run<RPROC(DeleteTransformFeedback)>(tf);
}

void ClientWebGLContext::BindTransformFeedback(
    GLenum target, const WebGLId<WebGLTransformFeedback>& tf) {
  Run<RPROC(BindTransformFeedback)>(target, tf);
}

void ClientWebGLContext::BeginTransformFeedback(GLenum primitiveMode) {
  Run<RPROC(BeginTransformFeedback)>(primitiveMode);
}

void ClientWebGLContext::EndTransformFeedback() {
  Run<RPROC(EndTransformFeedback)>();
}

void ClientWebGLContext::PauseTransformFeedback() {
  Run<RPROC(PauseTransformFeedback)>();
}

void ClientWebGLContext::ResumeTransformFeedback() {
  Run<RPROC(ResumeTransformFeedback)>();
}

already_AddRefed<ClientWebGLActiveInfo>
ClientWebGLContext::GetTransformFeedbackVarying(
    const WebGLId<WebGLProgram>& prog, GLuint index) {
  Maybe<WebGLActiveInfo> response =
      Run<RPROC(GetTransformFeedbackVarying)>(prog, index);
  return response ? MakeAndAddRef<ClientWebGLActiveInfo>(this, response.ref())
                  : nullptr;
}
void ClientWebGLContext::TransformFeedbackVaryings(
    const WebGLId<WebGLProgram>& program,
    const dom::Sequence<nsString>& varyings, GLenum bufferMode) {
  Run<RPROC(TransformFeedbackVaryings)>(program, nsTArray<nsString>(varyings),
                                        bufferMode);
}

// ------------------------------ Extensions ------------------------------

const Maybe<ExtensionSets>& ClientWebGLContext::GetCachedExtensions() {
  if (!mExtensions) {
    mExtensions = Run<RPROC(GetSupportedExtensions)>();
    if (mExtensions) {
      mExtensions.ref().mNonSystem.Sort();
      mExtensions.ref().mSystem.Sort();
    }
  }
  return mExtensions;
}

ClientWebGLExtensionBase* ClientWebGLContext::GetExtension(
    dom::CallerType callerType, WebGLExtensionID ext, bool toEnable) {
  if (toEnable) {
    EnableExtension(callerType, ext);
  }
  return UseExtension(ext);
}

void ClientWebGLContext::EnableExtension(dom::CallerType callerType,
                                         WebGLExtensionID ext) {
  const Maybe<ExtensionSets>& exts = GetCachedExtensions();
  if (!exts) {
    return;
  }
  if (exts.ref().mNonSystem.ContainsSorted(ext) ||
      ((callerType == dom::CallerType::System) &&
       exts.ref().mSystem.ContainsSorted(ext))) {
    mEnabledExtension[static_cast<uint8_t>(ext)] = true;
    Run<RPROC(EnableExtension)>(callerType, ext);
  }
}

// ---------------------------- Misc Extensions ----------------------------
void ClientWebGLContext::DrawBuffers(const dom::Sequence<GLenum>& buffers,
                                     bool aFromExtension) {
  Run<RPROC(DrawBuffers)>(nsTArray<uint32_t>(buffers), aFromExtension);
}

void ClientWebGLContext::GetASTCExtensionSupportedProfiles(
    dom::Nullable<nsTArray<nsString>>& retval) const {
  Maybe<nsTArray<nsString>> response =
      Run<RPROC(GetASTCExtensionSupportedProfiles)>();
  if (!response) {
    retval.SetNull();
    return;
  }
  retval.SetValue() = response.ref();
}

void ClientWebGLContext::GetTranslatedShaderSource(
    const WebGLId<WebGLShader>& shader, nsAString& retval) const {
  retval = Run<RPROC(GetTranslatedShaderSource)>(shader);
}

void ClientWebGLContext::LoseContext(bool isSimulated) {
  Run<RPROC(LoseContext)>(isSimulated);
}

void ClientWebGLContext::RestoreContext() { Run<RPROC(RestoreContext)>(); }

void ClientWebGLContext::ForceLoseContext() { Run<RPROC(LoseContext)>(false); }

void ClientWebGLContext::MOZDebugGetParameter(
    JSContext* cx, GLenum pname, JS::MutableHandle<JS::Value> retval,
    ErrorResult& rv) const {
  retval.set(ToJSValue(cx, Run<RPROC(MOZDebugGetParameter)>(pname), rv));
}

void ClientWebGLContext::EnqueueErrorPrintfHelper(GLenum aGLError,
                                                  const nsCString& msg) {
  Run<RPROC(EnqueueError)>(aGLError, msg);
}

void ClientWebGLContext::EnqueueWarning(const nsCString& msg) {
  Run<RPROC(EnqueueWarning)>(msg);
}

#undef RPROC

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ClientWebGLContext)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports,
                                   nsICanvasRenderingContextInternal)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ClientWebGLContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ClientWebGLContext)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(ClientWebGLContext)

}  // namespace mozilla
