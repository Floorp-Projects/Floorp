/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(mozilla_dom_HTMLCanvasElement_h)
#define mozilla_dom_HTMLCanvasElement_h

#include "mozilla/Attributes.h"
#include "nsIDOMHTMLCanvasElement.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsSize.h"
#include "nsError.h"
#include "nsNodeInfoManager.h"

#include "nsICanvasElementExternal.h"
#include "nsLayoutUtils.h"
#include "mozilla/gfx/Rect.h"

class nsICanvasRenderingContextInternal;
class nsIDOMFile;
class nsITimerCallback;
class nsIPropertyBag;

namespace mozilla {

namespace layers {
class CanvasLayer;
class LayerManager;
}

namespace dom {

class HTMLCanvasPrintState;

class HTMLCanvasElement MOZ_FINAL : public nsGenericHTMLElement,
                                    public nsICanvasElementExternal,
                                    public nsIDOMHTMLCanvasElement
{
  enum {
    DEFAULT_CANVAS_WIDTH = 300,
    DEFAULT_CANVAS_HEIGHT = 150
  };

  typedef layers::CanvasLayer CanvasLayer;
  typedef layers::LayerManager LayerManager;

public:
  HTMLCanvasElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~HTMLCanvasElement();

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLCanvasElement, canvas)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

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
             const Optional<JS::Handle<JS::Value> >& aContextOptions,
             ErrorResult& aRv)
  {
    JS::Value contextOptions = aContextOptions.WasPassed()
                             ? aContextOptions.Value()
                             : JS::UndefinedValue();
    nsCOMPtr<nsISupports> context;
    aRv = GetContext(aContextId, contextOptions, aCx, getter_AddRefs(context));
    return context.forget();
  }
  void ToDataURL(JSContext* aCx, const nsAString& aType,
                 const Optional<JS::Handle<JS::Value> >& aParams,
                 nsAString& aDataURL, ErrorResult& aRv)
  {
    JS::Value params = aParams.WasPassed()
                     ? aParams.Value()
                     : JS::UndefinedValue();
    aRv = ToDataURL(aType, params, aCx, aDataURL);
  }
  void ToBlob(nsIFileCallback* aCallback, const nsAString& aType,
              ErrorResult& aRv)
  {
    aRv = ToBlob(aCallback, aType);
  }

  bool MozOpaque() const
  {
    return GetBoolAttr(nsGkAtoms::moz_opaque);
  }
  void SetMozOpaque(bool aValue, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::moz_opaque, aValue, aRv);
  }
  already_AddRefed<nsIDOMFile> MozGetAsFile(const nsAString& aName,
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
  nsIPrintCallback* GetMozPrintCallback() const;
  // Using XPCOM SetMozPrintCallback.

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

  /*
   * nsICanvasElementExternal -- for use outside of content/layout
   */
  NS_IMETHOD_(nsIntSize) GetSizeExternal() MOZ_OVERRIDE;
  NS_IMETHOD RenderContextsExternal(gfxContext *aContext,
                                    gfxPattern::GraphicsFilter aFilter,
                                    uint32_t aFlags = RenderFlagPremultAlpha) MOZ_OVERRIDE;

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult) MOZ_OVERRIDE;
  nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute, int32_t aModType) const MOZ_OVERRIDE;

  // SetAttr override.  C++ is stupid, so have to override both
  // overloaded methods.
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) MOZ_OVERRIDE;
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;
  nsresult CopyInnerTo(mozilla::dom::Element* aDest);

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

  nsresult GetContext(const nsAString& aContextId, nsISupports** aContext);

  virtual nsIDOMNode* AsDOMNode() MOZ_OVERRIDE { return this; }

protected:
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  nsIntSize GetWidthHeight();

  nsresult UpdateContext(nsIPropertyBag *aNewContextOptions = nullptr);
  nsresult ExtractData(const nsAString& aType,
                       const nsAString& aOptions,
                       nsIInputStream** aStream,
                       bool& aFellBackToPNG);
  nsresult ToDataURLImpl(JSContext* aCx,
                         const nsAString& aMimeType,
                         const JS::Value& aEncoderOptions,
                         nsAString& aDataURL);
  nsresult MozGetAsFileImpl(const nsAString& aName,
                            const nsAString& aType,
                            nsIDOMFile** aResult);
  nsresult GetContextHelper(const nsAString& aContextId,
                            nsICanvasRenderingContextInternal **aContext);
  void CallPrintCallback();

  nsString mCurrentContextId;
  nsRefPtr<HTMLCanvasElement> mOriginalCanvas;
  nsCOMPtr<nsIPrintCallback> mPrintCallback;
  nsCOMPtr<nsICanvasRenderingContextInternal> mCurrentContext;
  nsCOMPtr<HTMLCanvasPrintState> mPrintState;

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
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLCanvasElement_h */
