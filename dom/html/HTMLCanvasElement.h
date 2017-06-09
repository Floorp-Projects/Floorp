/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(mozilla_dom_HTMLCanvasElement_h)
#define mozilla_dom_HTMLCanvasElement_h

#include "mozilla/Attributes.h"
#include "mozilla/WeakPtr.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMHTMLCanvasElement.h"
#include "nsIObserver.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsSize.h"
#include "nsError.h"

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/CanvasRenderingContextHelper.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/layers/LayersTypes.h"

class nsICanvasRenderingContextInternal;
class nsITimerCallback;

namespace mozilla {

class WebGLContext;

namespace layers {
class AsyncCanvasRenderer;
class CanvasLayer;
class Image;
class Layer;
class LayerManager;
class SharedSurfaceTextureClient;
} // namespace layers
namespace gfx {
class SourceSurface;
class VRLayerChild;
} // namespace gfx

namespace dom {
class BlobCallback;
class CanvasCaptureMediaStream;
class File;
class HTMLCanvasPrintState;
class OffscreenCanvas;
class PrintCallback;
class RequestedFrameRefreshObserver;

// Listen visibilitychange and memory-pressure event and inform
// context when event is fired.
class HTMLCanvasElementObserver final : public nsIObserver
                                      , public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMEVENTLISTENER

  explicit HTMLCanvasElementObserver(HTMLCanvasElement* aElement);
  void Destroy();

  void RegisterVisibilityChangeEvent();
  void UnregisterVisibilityChangeEvent();

  void RegisterMemoryPressureEvent();
  void UnregisterMemoryPressureEvent();

private:
  ~HTMLCanvasElementObserver();

  HTMLCanvasElement* mElement;
};

/*
 * FrameCaptureListener is used by captureStream() as a way of getting video
 * frames from the canvas. On a refresh driver tick after something has been
 * drawn to the canvas since the last such tick, all registered
 * FrameCaptureListeners whose `mFrameCaptureRequested` equals `true`,
 * will be given a copy of the just-painted canvas.
 * All FrameCaptureListeners get the same copy.
 */
class FrameCaptureListener : public SupportsWeakPtr<FrameCaptureListener>
{
public:
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(FrameCaptureListener)

  FrameCaptureListener()
    : mFrameCaptureRequested(false) {}

  /*
   * Called when a frame capture is desired on next paint.
   */
  void RequestFrameCapture() { mFrameCaptureRequested = true; }

  /*
   * Indicates to the canvas whether or not this listener has requested a frame.
   */
  bool FrameCaptureRequested() const { return mFrameCaptureRequested; }

  /*
   * Interface through which new video frames will be provided while
   * `mFrameCaptureRequested` is `true`.
   */
  virtual void NewFrame(already_AddRefed<layers::Image> aImage,
                        const TimeStamp& aTime) = 0;

protected:
  virtual ~FrameCaptureListener() {}

  bool mFrameCaptureRequested;
};

class HTMLCanvasElement final : public nsGenericHTMLElement,
                                public nsIDOMHTMLCanvasElement,
                                public CanvasRenderingContextHelper
{
  enum {
    DEFAULT_CANVAS_WIDTH = 300,
    DEFAULT_CANVAS_HEIGHT = 150
  };

  typedef layers::AsyncCanvasRenderer AsyncCanvasRenderer;
  typedef layers::CanvasLayer CanvasLayer;
  typedef layers::Layer Layer;
  typedef layers::LayerManager LayerManager;

public:
  explicit HTMLCanvasElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLCanvasElement, canvas)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMHTMLCanvasElement
  NS_DECL_NSIDOMHTMLCANVASELEMENT

  // CC
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLCanvasElement,
                                           nsGenericHTMLElement)

  // WebIDL
  uint32_t Height()
  {
    return GetUnsignedIntAttr(nsGkAtoms::height, DEFAULT_CANVAS_HEIGHT);
  }
  void SetHeight(uint32_t aHeight, ErrorResult& aRv)
  {
    if (mOffscreenCanvas) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    SetUnsignedIntAttr(nsGkAtoms::height, aHeight, DEFAULT_CANVAS_HEIGHT, aRv);
  }
  uint32_t Width()
  {
    return GetUnsignedIntAttr(nsGkAtoms::width, DEFAULT_CANVAS_WIDTH);
  }
  void SetWidth(uint32_t aWidth, ErrorResult& aRv)
  {
    if (mOffscreenCanvas) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    SetUnsignedIntAttr(nsGkAtoms::width, aWidth, DEFAULT_CANVAS_WIDTH, aRv);
  }

  virtual already_AddRefed<nsISupports>
  GetContext(JSContext* aCx, const nsAString& aContextId,
             JS::Handle<JS::Value> aContextOptions,
             ErrorResult& aRv) override;

  void ToDataURL(JSContext* aCx, const nsAString& aType,
                 JS::Handle<JS::Value> aParams,
                 nsAString& aDataURL,
                 ErrorResult& aRv);

  void ToBlob(JSContext* aCx,
              BlobCallback& aCallback,
              const nsAString& aType,
              JS::Handle<JS::Value> aParams,
              ErrorResult& aRv);

  OffscreenCanvas* TransferControlToOffscreen(ErrorResult& aRv);

  bool MozOpaque() const
  {
    return GetBoolAttr(nsGkAtoms::moz_opaque);
  }
  void SetMozOpaque(bool aValue, ErrorResult& aRv)
  {
    if (mOffscreenCanvas) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    SetHTMLBoolAttr(nsGkAtoms::moz_opaque, aValue, aRv);
  }
  already_AddRefed<File> MozGetAsFile(const nsAString& aName,
                                      const nsAString& aType,
                                      CallerType aCallerType,
                                      ErrorResult& aRv);
  already_AddRefed<nsISupports> MozGetIPCContext(const nsAString& aContextId,
                                                 ErrorResult& aRv);
  PrintCallback* GetMozPrintCallback() const;
  void SetMozPrintCallback(PrintCallback* aCallback);

  already_AddRefed<CanvasCaptureMediaStream>
  CaptureStream(const Optional<double>& aFrameRate, ErrorResult& aRv);

  /**
   * Get the size in pixels of this canvas element
   */
  nsIntSize GetSize();

  /**
   * Determine whether the canvas is write-only.
   */
  bool IsWriteOnly();

  /**
   * Force the canvas to be write-only.
   */
  void SetWriteOnly();

  /**
   * Notify that some canvas content has changed and the window may
   * need to be updated. aDamageRect is in canvas coordinates.
   */
  void InvalidateCanvasContent(const mozilla::gfx::Rect* aDamageRect);
  /*
   * Notify that we need to repaint the entire canvas, including updating of
   * the layer tree.
   */
  void InvalidateCanvas();

  /*
   * Get the number of contexts in this canvas, and request a context at
   * an index.
   */
  int32_t CountContexts ();
  nsICanvasRenderingContextInternal *GetContextAtIndex (int32_t index);

  /*
   * Returns true if the canvas context content is guaranteed to be opaque
   * across its entire area.
   */
  bool GetIsOpaque();
  virtual bool GetOpaqueAttr() override;

  virtual already_AddRefed<gfx::SourceSurface>
  GetSurfaceSnapshot(gfxAlphaType* aOutAlphaType = nullptr);

  /*
   * Register a FrameCaptureListener with this canvas.
   * The canvas hooks into the RefreshDriver while there are
   * FrameCaptureListeners registered.
   * The registered FrameCaptureListeners are stored as WeakPtrs, thus it's the
   * caller's responsibility to keep them alive. Once a registered
   * FrameCaptureListener is destroyed it will be automatically deregistered.
   */
  nsresult RegisterFrameCaptureListener(FrameCaptureListener* aListener);

  /*
   * Returns true when there is at least one registered FrameCaptureListener
   * that has requested a frame capture.
   */
  bool IsFrameCaptureRequested() const;

  /*
   * Processes destroyed FrameCaptureListeners and removes them if necessary.
   * Should there be none left, the FrameRefreshObserver will be unregistered.
   */
  void ProcessDestroyedFrameListeners();

  /*
   * Called by the RefreshDriver hook when a frame has been captured.
   * Makes a copy of the provided surface and hands it to all
   * FrameCaptureListeners having requested frame capture.
   */
  void SetFrameCapture(already_AddRefed<gfx::SourceSurface> aSurface,
                       const TimeStamp& aTime);

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult) override;
  nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute, int32_t aModType) const override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;
  nsresult CopyInnerTo(mozilla::dom::Element* aDest,
                       bool aPreallocateChildren);

  virtual nsresult GetEventTargetParent(
                     mozilla::EventChainPreVisitor& aVisitor) override;

  /*
   * Helpers called by various users of Canvas
   */

  already_AddRefed<Layer> GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                         Layer *aOldLayer,
                                         LayerManager *aManager);
  // Should return true if the canvas layer should always be marked inactive.
  // We should return true here if we can't do accelerated compositing with
  // a non-BasicCanvasLayer.
  bool ShouldForceInactiveLayer(LayerManager *aManager);

  // Call this whenever we need future changes to the canvas
  // to trigger fresh invalidation requests. This needs to be called
  // whenever we render the canvas contents to the screen, or whenever we
  // take a snapshot of the canvas that needs to be "live" (e.g. -moz-element).
  void MarkContextClean();

  // Call this after capturing a frame, so we can avoid unnecessary surface
  // copies for future frames when no drawing has occurred.
  void MarkContextCleanForFrameCapture();

  // Starts returning false when something is drawn.
  bool IsContextCleanForFrameCapture();

  nsresult GetContext(const nsAString& aContextId, nsISupports** aContext);

  layers::LayersBackend GetCompositorBackendType() const;

  void OnVisibilityChange();

  void OnMemoryPressure();

  static void SetAttrFromAsyncCanvasRenderer(AsyncCanvasRenderer *aRenderer);
  static void InvalidateFromAsyncCanvasRenderer(AsyncCanvasRenderer *aRenderer);

  void StartVRPresentation();
  void StopVRPresentation();
  already_AddRefed<layers::SharedSurfaceTextureClient> GetVRFrame();

protected:
  virtual ~HTMLCanvasElement();

  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual nsIntSize GetWidthHeight() override;

  virtual already_AddRefed<nsICanvasRenderingContextInternal>
  CreateContext(CanvasContextType aContextType) override;

  nsresult ExtractData(nsAString& aType,
                       const nsAString& aOptions,
                       nsIInputStream** aStream);
  nsresult ToDataURLImpl(JSContext* aCx,
                         const nsAString& aMimeType,
                         const JS::Value& aEncoderOptions,
                         nsAString& aDataURL);
  nsresult MozGetAsFileImpl(const nsAString& aName,
                            const nsAString& aType,
                            File** aResult);
  void CallPrintCallback();

  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                bool aNotify) override;
  virtual nsresult OnAttrSetButNotChanged(int32_t aNamespaceID, nsIAtom* aName,
                                          const nsAttrValueOrString& aValue,
                                          bool aNotify) override;

  AsyncCanvasRenderer* GetAsyncCanvasRenderer();

  bool mResetLayer;
  RefPtr<HTMLCanvasElement> mOriginalCanvas;
  RefPtr<PrintCallback> mPrintCallback;
  RefPtr<HTMLCanvasPrintState> mPrintState;
  nsTArray<WeakPtr<FrameCaptureListener>> mRequestedFrameListeners;
  RefPtr<RequestedFrameRefreshObserver> mRequestedFrameRefreshObserver;
  RefPtr<AsyncCanvasRenderer> mAsyncCanvasRenderer;
  RefPtr<OffscreenCanvas> mOffscreenCanvas;
  RefPtr<HTMLCanvasElementObserver> mContextObserver;
  bool mVRPresentationActive;

public:
  // Record whether this canvas should be write-only or not.
  // We set this when script paints an image from a different origin.
  // We also transitively set it when script paints a canvas which
  // is itself write-only.
  bool                     mWriteOnly;

  bool IsPrintCallbackDone();

  void HandlePrintCallback(nsPresContext::nsPresContextType aType);

  nsresult DispatchPrintCallback(nsITimerCallback* aCallback);

  void ResetPrintCallback();

  HTMLCanvasElement* GetOriginalCanvas();

  CanvasContextType GetCurrentContextType() {
    return mCurrentContextType;
  }

private:
  /**
   * This function is called by AfterSetAttr and OnAttrSetButNotChanged.
   * This function will be called by AfterSetAttr whether the attribute is being
   * set or unset.
   *
   * @param aNamespaceID the namespace of the attr being set
   * @param aName the localname of the attribute being set
   * @param aNotify Whether we plan to notify document observers.
   */
  void AfterMaybeChangeAttr(int32_t aNamespaceID, nsIAtom* aName, bool aNotify);
};

class HTMLCanvasPrintState final : public nsWrapperCache
{
public:
  HTMLCanvasPrintState(HTMLCanvasElement* aCanvas,
                       nsICanvasRenderingContextInternal* aContext,
                       nsITimerCallback* aCallback);

  nsISupports* Context() const;

  void Done();

  void NotifyDone();

  bool mIsDone;

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(HTMLCanvasPrintState)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(HTMLCanvasPrintState)

  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

  HTMLCanvasElement* GetParentObject()
  {
    return mCanvas;
  }

private:
  ~HTMLCanvasPrintState();
  bool mPendingNotify;

protected:
  RefPtr<HTMLCanvasElement> mCanvas;
  nsCOMPtr<nsICanvasRenderingContextInternal> mContext;
  nsCOMPtr<nsITimerCallback> mCallback;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLCanvasElement_h */
