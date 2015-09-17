/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(mozilla_dom_HTMLCanvasElement_h)
#define mozilla_dom_HTMLCanvasElement_h

#include "mozilla/Attributes.h"
#include "mozilla/WeakPtr.h"
#include "nsIDOMHTMLCanvasElement.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsSize.h"
#include "nsError.h"

#include "mozilla/gfx/Rect.h"

class nsICanvasRenderingContextInternal;
class nsITimerCallback;

namespace mozilla {

namespace layers {
class CanvasLayer;
class Image;
class LayerManager;
} // namespace layers
namespace gfx {
class SourceSurface;
} // namespace gfx

namespace dom {
class CanvasCaptureMediaStream;
class File;
class FileCallback;
class HTMLCanvasPrintState;
class PrintCallback;
class RequestedFrameRefreshObserver;

enum class CanvasContextType : uint8_t {
  NoContext,
  Canvas2D,
  WebGL1,
  WebGL2
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
  virtual void NewFrame(already_AddRefed<layers::Image> aImage) = 0;

protected:
  virtual ~FrameCaptureListener() {}

  bool mFrameCaptureRequested;
};

class HTMLCanvasElement final : public nsGenericHTMLElement,
                                public nsIDOMHTMLCanvasElement
{
  enum {
    DEFAULT_CANVAS_WIDTH = 300,
    DEFAULT_CANVAS_HEIGHT = 150
  };

  typedef layers::CanvasLayer CanvasLayer;
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
    SetUnsignedIntAttr(nsGkAtoms::height, aHeight, aRv);
  }
  uint32_t Width()
  {
    return GetUnsignedIntAttr(nsGkAtoms::width, DEFAULT_CANVAS_WIDTH);
  }
  void SetWidth(uint32_t aWidth, ErrorResult& aRv)
  {
    SetUnsignedIntAttr(nsGkAtoms::width, aWidth, aRv);
  }
  already_AddRefed<nsISupports>
  GetContext(JSContext* aCx, const nsAString& aContextId,
             JS::Handle<JS::Value> aContextOptions,
             ErrorResult& aRv);
  void ToDataURL(JSContext* aCx, const nsAString& aType,
                 JS::Handle<JS::Value> aParams,
                 nsAString& aDataURL, ErrorResult& aRv)
  {
    aRv = ToDataURL(aType, aParams, aCx, aDataURL);
  }
  void ToBlob(JSContext* aCx,
              FileCallback& aCallback,
              const nsAString& aType,
              JS::Handle<JS::Value> aParams,
              ErrorResult& aRv);

  bool MozOpaque() const
  {
    return GetBoolAttr(nsGkAtoms::moz_opaque);
  }
  void SetMozOpaque(bool aValue, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::moz_opaque, aValue, aRv);
  }
  already_AddRefed<File> MozGetAsFile(const nsAString& aName,
                                      const nsAString& aType,
                                      ErrorResult& aRv);
  already_AddRefed<nsISupports> MozGetIPCContext(const nsAString& aContextId,
                                                 ErrorResult& aRv)
  {
    nsCOMPtr<nsISupports> context;
    aRv = MozGetIPCContext(aContextId, getter_AddRefs(context));
    return context.forget();
  }
  void MozFetchAsStream(nsIInputStreamCallback* aCallback,
                        const nsAString& aType, ErrorResult& aRv)
  {
    aRv = MozFetchAsStream(aCallback, aType);
  }
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

  virtual already_AddRefed<gfx::SourceSurface> GetSurfaceSnapshot(bool* aPremultAlpha = nullptr);

  /*
   * Register a FrameCaptureListener with this canvas.
   * The canvas hooks into the RefreshDriver while there are
   * FrameCaptureListeners registered.
   * The registered FrameCaptureListeners are stored as WeakPtrs, thus it's the
   * caller's responsibility to keep them alive. Once a registered
   * FrameCaptureListener is destroyed it will be automatically deregistered.
   */
  void RegisterFrameCaptureListener(FrameCaptureListener* aListener);

  /*
   * Returns true when there is at least one registered FrameCaptureListener
   * that has requested a frame capture.
   */
  bool IsFrameCaptureRequested() const;

  /*
   * Called by the RefreshDriver hook when a frame has been captured.
   * Makes a copy of the provided surface and hands it to all
   * FrameCaptureListeners having requested frame capture.
   */
  void SetFrameCapture(already_AddRefed<gfx::SourceSurface> aSurface);

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult) override;
  nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute, int32_t aModType) const override;

  // SetAttr override.  C++ is stupid, so have to override both
  // overloaded methods.
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) override;

  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                             bool aNotify) override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;
  nsresult CopyInnerTo(mozilla::dom::Element* aDest);

  virtual nsresult PreHandleEvent(mozilla::EventChainPreVisitor& aVisitor) override;

  /*
   * Helpers called by various users of Canvas
   */

  already_AddRefed<CanvasLayer> GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                               CanvasLayer *aOldLayer,
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

protected:
  virtual ~HTMLCanvasElement();

  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsIntSize GetWidthHeight();

  nsresult UpdateContext(JSContext* aCx, JS::Handle<JS::Value> options);
  nsresult ParseParams(JSContext* aCx,
                       const nsAString& aType,
                       const JS::Value& aEncoderOptions,
                       nsAString& aParams,
                       bool* usingCustomParseOptions);
  nsresult ExtractData(nsAString& aType,
                       const nsAString& aOptions,
                       nsIInputStream** aStream);
  nsresult ToDataURLImpl(JSContext* aCx,
                         const nsAString& aMimeType,
                         const JS::Value& aEncoderOptions,
                         nsAString& aDataURL);
  nsresult MozGetAsBlobImpl(const nsAString& aName,
                            const nsAString& aType,
                            nsISupports** aResult);
  void CallPrintCallback();

  CanvasContextType mCurrentContextType;
  nsRefPtr<HTMLCanvasElement> mOriginalCanvas;
  nsRefPtr<PrintCallback> mPrintCallback;
  nsCOMPtr<nsICanvasRenderingContextInternal> mCurrentContext;
  nsRefPtr<HTMLCanvasPrintState> mPrintState;
  nsTArray<WeakPtr<FrameCaptureListener>> mRequestedFrameListeners;
  nsRefPtr<RequestedFrameRefreshObserver> mRequestedFrameRefreshObserver;

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
  nsRefPtr<HTMLCanvasElement> mCanvas;
  nsCOMPtr<nsICanvasRenderingContextInternal> mContext;
  nsCOMPtr<nsITimerCallback> mCallback;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLCanvasElement_h */
