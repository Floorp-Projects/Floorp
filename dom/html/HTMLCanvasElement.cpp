   /* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLCanvasElement.h"

#include "ImageEncoder.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "Layers.h"
#include "mozilla/Assertions.h"
#include "mozilla/Base64.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/CanvasRenderingContext2D.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/HTMLCanvasElementBinding.h"
#include "mozilla/dom/MouseEvent.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/gfx/Rect.h"
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
#include "nsStreamUtils.h"
#include "ActiveLayerTracker.h"
#include "WebGL1Context.h"
#include "WebGL2Context.h"

using namespace mozilla::layers;
using namespace mozilla::gfx;

NS_IMPL_NS_NEW_HTML_ELEMENT(Canvas)

namespace {

typedef mozilla::dom::HTMLImageElementOrHTMLCanvasElementOrHTMLVideoElement
HTMLImageOrCanvasOrVideoElement;

} // anonymous namespace

namespace mozilla {
namespace dom {

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
    nsRefPtr<nsRunnableMethod<HTMLCanvasPrintState> > doneEvent =
      NS_NewRunnableMethod(this, &HTMLCanvasPrintState::NotifyDone);
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

HTMLCanvasElement::HTMLCanvasElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo),
    mWriteOnly(false)
{
}

HTMLCanvasElement::~HTMLCanvasElement()
{
  ResetPrintCallback();
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLCanvasElement, nsGenericHTMLElement,
                                   mCurrentContext, mPrintCallback,
                                   mPrintState, mOriginalCanvas)

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
    rv = UpdateContext(nullptr, JS::NullHandleValue);
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
    rv = UpdateContext(nullptr, JS::NullHandleValue);
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

  nsRefPtr<nsRunnableMethod<HTMLCanvasElement> > renderEvent =
        NS_NewRunnableMethod(this, &HTMLCanvasElement::CallPrintCallback);
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
    nsRefPtr<CanvasRenderingContext2D> context2d =
      static_cast<CanvasRenderingContext2D*>(cxt.get());
    if (context2d && !mPrintCallback) {
      HTMLImageOrCanvasOrVideoElement element;
      element.SetAsHTMLCanvasElement() = this;
      ErrorResult err;
      context2d->DrawImage(element,
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

// HTMLCanvasElement::mozFetchAsStream

NS_IMETHODIMP
HTMLCanvasElement::MozFetchAsStream(nsIInputStreamCallback *aCallback,
                                    const nsAString& aType)
{
  if (!nsContentUtils::IsCallerChrome())
    return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIInputStream> inputData;

  nsAutoString type(aType);
  rv = ExtractData(type, EmptyString(), getter_AddRefs(inputData));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAsyncInputStream> asyncData = do_QueryInterface(inputData, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIThread> mainThread;
  rv = NS_GetMainThread(getter_AddRefs(mainThread));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStreamCallback> asyncCallback =
    NS_NewInputStreamReadyEvent(aCallback, mainThread);

  return asyncCallback->OnInputStreamReady(asyncData);
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

nsresult
HTMLCanvasElement::ExtractData(nsAString& aType,
                               const nsAString& aOptions,
                               nsIInputStream** aStream)
{
  return ImageEncoder::ExtractData(aType,
                                   aOptions,
                                   GetSize(),
                                   mCurrentContext,
                                   aStream);
}

nsresult
HTMLCanvasElement::ParseParams(JSContext* aCx,
                               const nsAString& aType,
                               const JS::Value& aEncoderOptions,
                               nsAString& aParams,
                               bool* usingCustomParseOptions)
{
  // Quality parameter is only valid for the image/jpeg MIME type
  if (aType.EqualsLiteral("image/jpeg")) {
    if (aEncoderOptions.isNumber()) {
      double quality = aEncoderOptions.toNumber();
      // Quality must be between 0.0 and 1.0, inclusive
      if (quality >= 0.0 && quality <= 1.0) {
        aParams.AppendLiteral("quality=");
        aParams.AppendInt(NS_lround(quality * 100.0));
      }
    }
  }

  // If we haven't parsed the aParams check for proprietary options.
  // The proprietary option -moz-parse-options will take a image lib encoder
  // parse options string as is and pass it to the encoder.
  *usingCustomParseOptions = false;
  if (aParams.Length() == 0 && aEncoderOptions.isString()) {
    NS_NAMED_LITERAL_STRING(mozParseOptions, "-moz-parse-options:");
    nsAutoJSString paramString;
    if (!paramString.init(aCx, aEncoderOptions.toString())) {
      return NS_ERROR_FAILURE;
    }
    if (StringBeginsWith(paramString, mozParseOptions)) {
      nsDependentSubstring parseOptions = Substring(paramString,
                                                    mozParseOptions.Length(),
                                                    paramString.Length() -
                                                    mozParseOptions.Length());
      aParams.Append(parseOptions);
      *usingCustomParseOptions = true;
    }
  }

  return NS_OK;
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

  nsAutoString type;
  nsContentUtils::ASCIIToLower(aType, type);

  nsAutoString params;
  bool usingCustomParseOptions;
  aRv = ParseParams(aCx, type, aParams, params, &usingCustomParseOptions);
  if (aRv.Failed()) {
    return;
  }

#ifdef DEBUG
  if (mCurrentContext) {
    // We disallow canvases of width or height zero, and set them to 1, so
    // we will have a discrepancy with the sizes of the canvas and the context.
    // That discrepancy is OK, the rest are not.
    nsIntSize elementSize = GetWidthHeight();
    MOZ_ASSERT(elementSize.width == mCurrentContext->GetWidth() ||
               (elementSize.width == 0 && mCurrentContext->GetWidth() == 1));
    MOZ_ASSERT(elementSize.height == mCurrentContext->GetHeight() ||
               (elementSize.height == 0 && mCurrentContext->GetHeight() == 1));
  }
#endif

  uint8_t* imageBuffer = nullptr;
  int32_t format = 0;
  if (mCurrentContext) {
    mCurrentContext->GetImageBuffer(&imageBuffer, &format);
  }

  // Encoder callback when encoding is complete.
  class EncodeCallback : public EncodeCompleteCallback
  {
  public:
    EncodeCallback(nsIGlobalObject* aGlobal, FileCallback* aCallback)
      : mGlobal(aGlobal)
      , mFileCallback(aCallback) {}

    // This is called on main thread.
    nsresult ReceiveBlob(already_AddRefed<File> aBlob)
    {
      nsRefPtr<File> blob = aBlob;
      uint64_t size;
      nsresult rv = blob->GetSize(&size);
      if (NS_SUCCEEDED(rv)) {
        AutoJSAPI jsapi;
        if (jsapi.Init(mGlobal)) {
          JS_updateMallocCounter(jsapi.cx(), size);
        }
      }

      nsRefPtr<File> newBlob = new File(mGlobal, blob->Impl());

      mozilla::ErrorResult error;
      mFileCallback->Call(*newBlob, error);

      mGlobal = nullptr;
      mFileCallback = nullptr;

      return error.StealNSResult();
    }

    nsCOMPtr<nsIGlobalObject> mGlobal;
    nsRefPtr<FileCallback> mFileCallback;
  };

  nsCOMPtr<nsIGlobalObject> global = OwnerDoc()->GetScopeObject();
  MOZ_ASSERT(global);
  nsRefPtr<EncodeCompleteCallback> callback = new EncodeCallback(global, &aCallback);
  aRv = ImageEncoder::ExtractDataAsync(type,
                                       params,
                                       usingCustomParseOptions,
                                       imageBuffer,
                                       format,
                                       GetSize(),
                                       callback);
}

already_AddRefed<File>
HTMLCanvasElement::MozGetAsFile(const nsAString& aName,
                                const nsAString& aType,
                                ErrorResult& aRv)
{
  nsCOMPtr<nsIDOMFile> file;
  aRv = MozGetAsFile(aName, aType, getter_AddRefs(file));
  nsRefPtr<File> tmp = static_cast<File*>(file.get());
  return tmp.forget();
}

NS_IMETHODIMP
HTMLCanvasElement::MozGetAsFile(const nsAString& aName,
                                const nsAString& aType,
                                nsIDOMFile** aResult)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eMozGetAsFile);

  // do a trust check if this is a write-only canvas
  if ((mWriteOnly) &&
      !nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  return MozGetAsFileImpl(aName, aType, aResult);
}

nsresult
HTMLCanvasElement::MozGetAsFileImpl(const nsAString& aName,
                                    const nsAString& aType,
                                    nsIDOMFile** aResult)
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

  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(OwnerDoc()->GetScopeObject());

  // The File takes ownership of the buffer
  nsRefPtr<File> file =
    File::CreateMemoryFile(win, imgData, (uint32_t)imgSize, aName, type,
                           PR_Now());

  file.forget(aResult);
  return NS_OK;
}

static bool
GetCanvasContextType(const nsAString& str, CanvasContextType* const out_type)
{
  if (str.EqualsLiteral("2d")) {
    *out_type = CanvasContextType::Canvas2D;
    return true;
  }

  if (str.EqualsLiteral("experimental-webgl")) {
    *out_type = CanvasContextType::WebGL1;
    return true;
  }

#ifdef MOZ_WEBGL_CONFORMANT
  if (str.EqualsLiteral("webgl")) {
    /* WebGL 1.0, $2.1 "Context Creation":
     *   If the user agent supports both the webgl and experimental-webgl
     *   canvas context types, they shall be treated as aliases.
     */
    *out_type = CanvasContextType::WebGL1;
    return true;
  }
#endif

  if (WebGL2Context::IsSupported()) {
    if (str.EqualsLiteral("experimental-webgl2")) {
      *out_type = CanvasContextType::WebGL2;
      return true;
    }
  }

  return false;
}

static already_AddRefed<nsICanvasRenderingContextInternal>
CreateContextForCanvas(CanvasContextType contextType, HTMLCanvasElement* canvas)
{
  nsRefPtr<nsICanvasRenderingContextInternal> ret;

  switch (contextType) {
  case CanvasContextType::Canvas2D:
    Telemetry::Accumulate(Telemetry::CANVAS_2D_USED, 1);
    ret = new CanvasRenderingContext2D();
    break;

  case CanvasContextType::WebGL1:
    Telemetry::Accumulate(Telemetry::CANVAS_WEBGL_USED, 1);

    ret = WebGL1Context::Create();
    if (!ret)
      return nullptr;
    break;

  case CanvasContextType::WebGL2:
    Telemetry::Accumulate(Telemetry::CANVAS_WEBGL_USED, 1);

    ret = WebGL2Context::Create();
    if (!ret)
      return nullptr;
    break;
  }
  MOZ_ASSERT(ret);

  ret->SetCanvasElement(canvas);
  return ret.forget();
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
                              ErrorResult& rv)
{
  CanvasContextType contextType;
  if (!GetCanvasContextType(aContextId, &contextType))
    return nullptr;

  if (!mCurrentContext) {
    // This canvas doesn't have a context yet.

    nsRefPtr<nsICanvasRenderingContextInternal> context;
    context = CreateContextForCanvas(contextType, this);
    if (!context)
      return nullptr;

    // Ensure that the context participates in CC.  Note that returning a
    // CC participant from QI doesn't addref.
    nsXPCOMCycleCollectionParticipant* cp = nullptr;
    CallQueryInterface(context, &cp);
    if (!cp) {
      rv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    mCurrentContext = context.forget();
    mCurrentContextType = contextType;

    rv = UpdateContext(aCx, aContextOptions);
    if (rv.Failed()) {
      rv = NS_OK; // See bug 645792
      return nullptr;
    }
  } else {
    // We already have a context of some type.
    if (contextType != mCurrentContextType)
      return nullptr;
  }

  nsCOMPtr<nsICanvasRenderingContextInternal> context = mCurrentContext;
  return context.forget();
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

    nsRefPtr<nsICanvasRenderingContextInternal> context;
    context = CreateContextForCanvas(contextType, this);
    if (!context) {
      *aContext = nullptr;
      return NS_OK;
    }

    mCurrentContext = context;
    mCurrentContext->SetIsIPC(true);
    mCurrentContextType = contextType;

    nsresult rv = UpdateContext(nullptr, JS::NullHandleValue);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // We already have a context of some type.
    if (contextType != mCurrentContextType)
      return NS_ERROR_INVALID_ARG;
  }

  NS_ADDREF (*aContext = mCurrentContext);
  return NS_OK;
}

nsresult
HTMLCanvasElement::UpdateContext(JSContext* aCx, JS::Handle<JS::Value> aNewContextOptions)
{
  if (!mCurrentContext)
    return NS_OK;

  nsIntSize sz = GetWidthHeight();

  nsresult rv = mCurrentContext->SetIsOpaque(HasAttr(kNameSpaceID_None, nsGkAtoms::moz_opaque));
  if (NS_FAILED(rv)) {
    mCurrentContext = nullptr;
    return rv;
  }

  rv = mCurrentContext->SetContextOptions(aCx, aNewContextOptions);
  if (NS_FAILED(rv)) {
    mCurrentContext = nullptr;
    return rv;
  }

  rv = mCurrentContext->SetDimensions(sz.width, sz.height);
  if (NS_FAILED(rv)) {
    mCurrentContext = nullptr;
    return rv;
  }

  return rv;
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

  return HasAttr(kNameSpaceID_None, nsGkAtoms::moz_opaque);
}

already_AddRefed<CanvasLayer>
HTMLCanvasElement::GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                  CanvasLayer *aOldLayer,
                                  LayerManager *aManager)
{
  if (!mCurrentContext)
    return nullptr;

  return mCurrentContext->GetCanvasLayer(aBuilder, aOldLayer, aManager);
}

bool
HTMLCanvasElement::ShouldForceInactiveLayer(LayerManager *aManager)
{
  return !mCurrentContext || mCurrentContext->ShouldForceInactiveLayer(aManager);
}

void
HTMLCanvasElement::MarkContextClean()
{
  if (!mCurrentContext)
    return;

  mCurrentContext->MarkContextClean();
}

TemporaryRef<SourceSurface>
HTMLCanvasElement::GetSurfaceSnapshot(bool* aPremultAlpha)
{
  if (!mCurrentContext)
    return nullptr;

  return mCurrentContext->GetSurfaceSnapshot(aPremultAlpha);
}

} // namespace dom
} // namespace mozilla
