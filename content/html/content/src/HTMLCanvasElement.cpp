/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLCanvasElement.h"
#include "nsAttrValueInlines.h"

#include "mozilla/Base64.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "nsNetUtil.h"
#include "nsDOMFile.h"

#include "mozilla/dom/CanvasRenderingContext2D.h"
#include "nsIScriptSecurityManager.h"
#include "nsIXPConnect.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "nsContentUtils.h"
#include "nsJSUtils.h"
#include "nsMathUtils.h"
#include "nsStreamUtils.h"

#include "nsFrameManager.h"
#include "nsDisplayList.h"
#include "BasicLayers.h"
#include "imgIEncoder.h"
#include "nsITimer.h"
#include "nsAsyncDOMEvent.h"

#include "nsIWritablePropertyBag2.h"

#define DEFAULT_CANVAS_WIDTH 300
#define DEFAULT_CANVAS_HEIGHT 150

using namespace mozilla::layers;

nsGenericHTMLElement*
NS_NewHTMLCanvasElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                        mozilla::dom::FromParser aFromParser)
{
  return new mozilla::dom::HTMLCanvasElement(aNodeInfo);
}

namespace {

typedef mozilla::dom::HTMLImageElementOrHTMLCanvasElementOrHTMLVideoElement
HTMLImageOrCanvasOrVideoElement;

class ToBlobRunnable : public nsRunnable
{
public:
  ToBlobRunnable(nsIFileCallback* aCallback,
                 nsIDOMBlob* aBlob)
    : mCallback(aCallback),
      mBlob(aBlob)
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  }

  NS_IMETHOD Run()
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    mCallback->Receive(mBlob);
    return NS_OK;
  }
private:
  nsCOMPtr<nsIFileCallback> mCallback;
  nsCOMPtr<nsIDOMBlob> mBlob;
};

} // anonymous namespace

namespace mozilla {
namespace dom {

class HTMLCanvasPrintState : public nsIDOMMozCanvasPrintState
{
public:
  HTMLCanvasPrintState(HTMLCanvasElement* aCanvas,
                       nsICanvasRenderingContextInternal* aContext,
                       nsITimerCallback* aCallback)
    : mIsDone(false), mPendingNotify(false), mCanvas(aCanvas),
      mContext(aContext), mCallback(aCallback)
  {
  }

  NS_IMETHOD GetContext(nsISupports** aContext)
  {
    NS_ADDREF(*aContext = mContext);
    return NS_OK;
  }

  NS_IMETHOD Done()
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
    return NS_OK;
  }

  void NotifyDone()
  {
    mIsDone = true;
    mPendingNotify = false;
    if (mCallback) {
      mCallback->Notify(nullptr);
    }
  }

  bool mIsDone;

  // CC
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(HTMLCanvasPrintState)
private:
  virtual ~HTMLCanvasPrintState()
  {
  }
  bool mPendingNotify;

protected:
  nsRefPtr<HTMLCanvasElement> mCanvas;
  nsCOMPtr<nsICanvasRenderingContextInternal> mContext;
  nsCOMPtr<nsITimerCallback> mCallback;
};

NS_IMPL_CYCLE_COLLECTING_ADDREF(HTMLCanvasPrintState)
NS_IMPL_CYCLE_COLLECTING_RELEASE(HTMLCanvasPrintState)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(HTMLCanvasPrintState)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozCanvasPrintState)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozCanvasPrintState)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLCanvasPrintState)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(HTMLCanvasPrintState)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCanvas)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(HTMLCanvasPrintState)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCanvas)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallback)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
// ---------------------------------------------------------------------------

HTMLCanvasElement::HTMLCanvasElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo), 
    mWriteOnly(false)
{
}

HTMLCanvasElement::~HTMLCanvasElement()
{
  ResetPrintCallback();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLCanvasElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLCanvasElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCurrentContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPrintCallback)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPrintState)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOriginalCanvas)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLCanvasElement,
                                                nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCurrentContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPrintCallback)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPrintState)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOriginalCanvas)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(HTMLCanvasElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLCanvasElement, Element)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLCanvasElement)
  NS_HTML_CONTENT_INTERFACE_TABLE2(HTMLCanvasElement,
                                   nsIDOMHTMLCanvasElement,
                                   nsICanvasElementExternal)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(HTMLCanvasElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLCanvasElement)

NS_IMPL_ELEMENT_CLONE(HTMLCanvasElement)

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
      (aName == nsGkAtoms::width || aName == nsGkAtoms::height || aName == nsGkAtoms::moz_opaque))
  {
    rv = UpdateContext();
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
  nsCOMPtr<nsIPrintCallback> printCallback;
  if ((aType == nsPresContext::eContext_PageLayout ||
       aType == nsPresContext::eContext_PrintPreview) &&
      !mPrintState &&
      NS_SUCCEEDED(GetMozPrintCallback(getter_AddRefs(printCallback))) && printCallback) {
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
    rv = GetContext(NS_LITERAL_STRING("2d"), JSVAL_VOID,
                    getter_AddRefs(context));
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
  nsCOMPtr<nsIPrintCallback> printCallback;
  GetMozPrintCallback(getter_AddRefs(printCallback));
  printCallback->Render(mPrintState);
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
    HTMLCanvasElement* self = const_cast<HTMLCanvasElement*>(this);
    dest->mOriginalCanvas = self;

    nsCOMPtr<nsISupports> cxt;
    dest->GetContext(NS_LITERAL_STRING("2d"), JSVAL_VOID, getter_AddRefs(cxt));
    nsRefPtr<CanvasRenderingContext2D> context2d =
      static_cast<CanvasRenderingContext2D*>(cxt.get());
    if (context2d && !self->mPrintCallback) {
      HTMLImageOrCanvasOrVideoElement element;
      element.SetAsHTMLCanvasElement() = this;
      ErrorResult err;
      context2d->DrawImage(element,
                           0.0, 0.0, err);
      rv = err.ErrorCode();
    }
  }
  return rv;
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
HTMLCanvasElement::ToDataURL(const nsAString& aType, nsIVariant* aParams,
                             uint8_t optional_argc, nsAString& aDataURL)
{
  // do a trust check if this is a write-only canvas
  if (mWriteOnly && !nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  return ToDataURLImpl(aType, aParams, aDataURL);
}

// HTMLCanvasElement::mozFetchAsStream

NS_IMETHODIMP
HTMLCanvasElement::MozFetchAsStream(nsIInputStreamCallback *aCallback,
                                    const nsAString& aType)
{
  if (!nsContentUtils::IsCallerChrome())
    return NS_ERROR_FAILURE;

  nsresult rv;
  bool fellBackToPNG = false;
  nsCOMPtr<nsIInputStream> inputData;

  rv = ExtractData(aType, EmptyString(), getter_AddRefs(inputData), fellBackToPNG);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAsyncInputStream> asyncData = do_QueryInterface(inputData, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIThread> mainThread;
  rv = NS_GetMainThread(getter_AddRefs(mainThread));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStreamCallback> asyncCallback;
  rv = NS_NewInputStreamReadyEvent(getter_AddRefs(asyncCallback), aCallback, mainThread);
  NS_ENSURE_SUCCESS(rv, rv);

  return asyncCallback->OnInputStreamReady(asyncData);
}

NS_IMETHODIMP
HTMLCanvasElement::SetMozPrintCallback(nsIPrintCallback *aCallback)
{
  mPrintCallback = aCallback;
  return NS_OK;
}

NS_IMETHODIMP
HTMLCanvasElement::GetMozPrintCallback(nsIPrintCallback** aCallback)
{
  if (mOriginalCanvas) {
    mOriginalCanvas->GetMozPrintCallback(aCallback);
    return NS_OK;
  }
  NS_IF_ADDREF(*aCallback = mPrintCallback);
  return NS_OK;
}

nsresult
HTMLCanvasElement::ExtractData(const nsAString& aType,
                               const nsAString& aOptions,
                               nsIInputStream** aStream,
                               bool& aFellBackToPNG)
{
  // note that if we don't have a current context, the spec says we're
  // supposed to just return transparent black pixels of the canvas
  // dimensions.
  nsRefPtr<gfxImageSurface> emptyCanvas;
  nsIntSize size = GetWidthHeight();
  if (!mCurrentContext) {
    emptyCanvas = new gfxImageSurface(gfxIntSize(size.width, size.height), gfxASurface::ImageFormatARGB32);
    if (emptyCanvas->CairoStatus()) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  nsresult rv;

  // get image bytes
  nsCOMPtr<nsIInputStream> imgStream;
  NS_ConvertUTF16toUTF8 encoderType(aType);

 try_again:
  if (mCurrentContext) {
    rv = mCurrentContext->GetInputStream(encoderType.get(),
                                         nsPromiseFlatString(aOptions).get(),
                                         getter_AddRefs(imgStream));
  } else {
    // no context, so we have to encode the empty image we created above
    nsCString enccid("@mozilla.org/image/encoder;2?type=");
    enccid += encoderType;

    nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(enccid.get(), &rv);
    if (NS_SUCCEEDED(rv) && encoder) {
      rv = encoder->InitFromData(emptyCanvas->Data(),
                                 size.width * size.height * 4,
                                 size.width,
                                 size.height,
                                 size.width * 4,
                                 imgIEncoder::INPUT_FORMAT_HOSTARGB,
                                 aOptions);
      if (NS_SUCCEEDED(rv)) {
        imgStream = do_QueryInterface(encoder);
      }
    } else {
      rv = NS_ERROR_FAILURE;
    }
  }

  if (NS_FAILED(rv) && !aFellBackToPNG) {
    // Try image/png instead.
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    aFellBackToPNG = true;
    encoderType.AssignLiteral("image/png");
    goto try_again;
  }

  NS_ENSURE_SUCCESS(rv, rv);

  imgStream.forget(aStream);
  return NS_OK;
}

nsresult
HTMLCanvasElement::ToDataURLImpl(const nsAString& aMimeType,
                                 nsIVariant* aEncoderOptions,
                                 nsAString& aDataURL)
{
  bool fallbackToPNG = false;

  nsIntSize size = GetWidthHeight();
  if (size.height == 0 || size.width == 0) {
    aDataURL = NS_LITERAL_STRING("data:,");
    return NS_OK;
  }

  nsAutoString type;
  nsresult rv = nsContentUtils::ASCIIToLower(aMimeType, type);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoString params;

  // Quality parameter is only valid for the image/jpeg MIME type
  if (type.EqualsLiteral("image/jpeg")) {
    uint16_t vartype;

    if (aEncoderOptions &&
        NS_SUCCEEDED(aEncoderOptions->GetDataType(&vartype)) &&
        vartype <= nsIDataType::VTYPE_DOUBLE) {

      double quality;
      // Quality must be between 0.0 and 1.0, inclusive
      if (NS_SUCCEEDED(aEncoderOptions->GetAsDouble(&quality)) &&
          quality >= 0.0 && quality <= 1.0) {
        params.AppendLiteral("quality=");
        params.AppendInt(NS_lround(quality * 100.0));
      }
    }
  }

  // If we haven't parsed the params check for proprietary options.
  // The proprietary option -moz-parse-options will take a image lib encoder
  // parse options string as is and pass it to the encoder.
  bool usingCustomParseOptions = false;
  if (params.Length() == 0) {
    NS_NAMED_LITERAL_STRING(mozParseOptions, "-moz-parse-options:");
    nsAutoString paramString;
    if (NS_SUCCEEDED(aEncoderOptions->GetAsAString(paramString)) && 
        StringBeginsWith(paramString, mozParseOptions)) {
      nsDependentSubstring parseOptions = Substring(paramString, 
                                                    mozParseOptions.Length(), 
                                                    paramString.Length() - 
                                                    mozParseOptions.Length());
      params.Append(parseOptions);
      usingCustomParseOptions = true;
    }
  }

  nsCOMPtr<nsIInputStream> stream;
  rv = ExtractData(type, params, getter_AddRefs(stream), fallbackToPNG);

  // If there are unrecognized custom parse options, we should fall back to 
  // the default values for the encoder without any options at all.
  if (rv == NS_ERROR_INVALID_ARG && usingCustomParseOptions) {
    fallbackToPNG = false;
    rv = ExtractData(type, EmptyString(), getter_AddRefs(stream), fallbackToPNG);
  }

  NS_ENSURE_SUCCESS(rv, rv);

  // build data URL string
  if (fallbackToPNG)
    aDataURL = NS_LITERAL_STRING("data:image/png;base64,");
  else
    aDataURL = NS_LITERAL_STRING("data:") + type +
      NS_LITERAL_STRING(";base64,");

  uint64_t count;
  rv = stream->Available(&count);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(count <= UINT32_MAX, NS_ERROR_FILE_TOO_BIG);

  return Base64EncodeInputStream(stream, aDataURL, (uint32_t)count, aDataURL.Length());
}

// XXXkhuey the encoding should be off the main thread, but we're lazy.
NS_IMETHODIMP
HTMLCanvasElement::ToBlob(nsIFileCallback* aCallback,
                          const nsAString& aType,
                          nsIVariant* aParams,
                          uint8_t optional_argc)
{
  // do a trust check if this is a write-only canvas
  if (mWriteOnly && !nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  if (!aCallback) {
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoString type;
  nsresult rv = nsContentUtils::ASCIIToLower(aType, type);
  if (NS_FAILED(rv)) {
    return rv;
  }

  bool fallbackToPNG = false;

  nsCOMPtr<nsIInputStream> stream;
  rv = ExtractData(type, EmptyString(), getter_AddRefs(stream), fallbackToPNG);
  NS_ENSURE_SUCCESS(rv, rv);

  if (fallbackToPNG) {
    type.AssignLiteral("image/png");
  }

  uint64_t imgSize;
  rv = stream->Available(&imgSize);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(imgSize <= UINT32_MAX, NS_ERROR_FILE_TOO_BIG);

  void* imgData = nullptr;
  rv = NS_ReadInputStreamToBuffer(stream, &imgData, imgSize);
  NS_ENSURE_SUCCESS(rv, rv);

  // The DOMFile takes ownership of the buffer
  nsRefPtr<nsDOMMemoryFile> blob =
    new nsDOMMemoryFile(imgData, imgSize, type);

  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  if (cx) {
    JS_updateMallocCounter(cx, imgSize);
  }

  nsRefPtr<ToBlobRunnable> runnable = new ToBlobRunnable(aCallback, blob);
  return NS_DispatchToCurrentThread(runnable);
}

NS_IMETHODIMP
HTMLCanvasElement::MozGetAsFile(const nsAString& aName,
                                const nsAString& aType,
                                uint8_t optional_argc,
                                nsIDOMFile** aResult)
{
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
  bool fallbackToPNG = false;

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = ExtractData(aType, EmptyString(), getter_AddRefs(stream),
                            fallbackToPNG);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString type(aType);
  if (fallbackToPNG) {
    type.AssignLiteral("image/png");
  }

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

  // The DOMFile takes ownership of the buffer
  nsRefPtr<nsDOMMemoryFile> file =
    new nsDOMMemoryFile(imgData, (uint32_t)imgSize, aName, type);

  file.forget(aResult);
  return NS_OK;
}

nsresult
HTMLCanvasElement::GetContextHelper(const nsAString& aContextId,
                                    nsICanvasRenderingContextInternal **aContext)
{
  NS_ENSURE_ARG(aContext);

  if (aContextId.EqualsLiteral("2d")) {
    Telemetry::Accumulate(Telemetry::CANVAS_2D_USED, 1);
    nsRefPtr<CanvasRenderingContext2D> ctx =
      new CanvasRenderingContext2D();

    ctx->SetCanvasElement(this);
    ctx.forget(aContext);
    return NS_OK;
  }

  NS_ConvertUTF16toUTF8 ctxId(aContextId);

  // check that ctxId is clamped to A-Za-z0-9_-
  for (uint32_t i = 0; i < ctxId.Length(); i++) {
    if ((ctxId[i] < 'A' || ctxId[i] > 'Z') &&
        (ctxId[i] < 'a' || ctxId[i] > 'z') &&
        (ctxId[i] < '0' || ctxId[i] > '9') &&
        (ctxId[i] != '-') &&
        (ctxId[i] != '_'))
    {
      // XXX ERRMSG we need to report an error to developers here! (bug 329026)
      return NS_OK;
    }
  }

  nsCString ctxString("@mozilla.org/content/canvas-rendering-context;1?id=");
  ctxString.Append(ctxId);

  nsresult rv;
  nsCOMPtr<nsICanvasRenderingContextInternal> ctx =
    do_CreateInstance(ctxString.get(), &rv);
  if (rv == NS_ERROR_OUT_OF_MEMORY) {
    *aContext = nullptr;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (NS_FAILED(rv)) {
    *aContext = nullptr;
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    return NS_OK;
  }

  ctx->SetCanvasElement(this);
  ctx.forget(aContext);
  return NS_OK;
}

NS_IMETHODIMP
HTMLCanvasElement::GetContext(const nsAString& aContextId,
                              const JS::Value& aContextOptions,
                              nsISupports **aContext)
{
  nsresult rv;

  if (mCurrentContextId.IsEmpty()) {
    rv = GetContextHelper(aContextId, getter_AddRefs(mCurrentContext));
    NS_ENSURE_SUCCESS(rv, rv);
    if (!mCurrentContext) {
      return NS_OK;
    }

    // Ensure that the context participates in CC.  Note that returning a
    // CC participant from QI doesn't addref.
    nsXPCOMCycleCollectionParticipant *cp = nullptr;
    CallQueryInterface(mCurrentContext, &cp);
    if (!cp) {
      mCurrentContext = nullptr;
      return NS_ERROR_FAILURE;
    }

    // note: if any contexts end up supporting something other
    // than objects, e.g. plain strings, then we'll need to expand
    // this to know how to create nsISupportsStrings etc.

    nsCOMPtr<nsIWritablePropertyBag2> contextProps;
    if (aContextOptions.isObject()) {
      JSContext* cx = nsContentUtils::GetCurrentJSContext();

      contextProps = do_CreateInstance("@mozilla.org/hash-property-bag;1");

      JSObject& opts = aContextOptions.toObject();
      JS::AutoIdArray props(cx, JS_Enumerate(cx, &opts));
      for (size_t i = 0; !!props && i < props.length(); ++i) {
        jsid propid = props[i];
        jsval propname, propval;
        if (!JS_IdToValue(cx, propid, &propname) ||
            !JS_GetPropertyById(cx, &opts, propid, &propval)) {
          return NS_ERROR_FAILURE;
        }

        JSString *propnameString = JS_ValueToString(cx, propname);
        nsDependentJSString pstr;
        if (!propnameString || !pstr.init(cx, propnameString)) {
          mCurrentContext = nullptr;
          return NS_ERROR_FAILURE;
        }

        if (JSVAL_IS_BOOLEAN(propval)) {
          contextProps->SetPropertyAsBool(pstr, JSVAL_TO_BOOLEAN(propval));
        } else if (JSVAL_IS_INT(propval)) {
          contextProps->SetPropertyAsInt32(pstr, JSVAL_TO_INT(propval));
        } else if (JSVAL_IS_DOUBLE(propval)) {
          contextProps->SetPropertyAsDouble(pstr, JSVAL_TO_DOUBLE(propval));
        } else if (JSVAL_IS_STRING(propval)) {
          JSString *propvalString = JS_ValueToString(cx, propval);
          nsDependentJSString vstr;
          if (!propvalString || !vstr.init(cx, propvalString)) {
            mCurrentContext = nullptr;
            return NS_ERROR_FAILURE;
          }

          contextProps->SetPropertyAsAString(pstr, vstr);
        }

      }
    }

    rv = UpdateContext(contextProps);
    if (NS_FAILED(rv)) {
      return rv;
    }

    mCurrentContextId.Assign(aContextId);
  }

  if (!mCurrentContextId.Equals(aContextId)) {
    //XXX eventually allow for more than one active context on a given canvas
    return NS_OK;
  }

  NS_ADDREF (*aContext = mCurrentContext);
  return NS_OK;
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
  if (!aContextId.Equals(NS_LITERAL_STRING("2d")))
    return NS_ERROR_INVALID_ARG;

  if (mCurrentContextId.IsEmpty()) {
    nsresult rv = GetContextHelper(aContextId, getter_AddRefs(mCurrentContext));
    NS_ENSURE_SUCCESS(rv, rv);
    if (!mCurrentContext) {
      return NS_OK;
    }

    mCurrentContext->SetIsIPC(true);

    rv = UpdateContext();
    NS_ENSURE_SUCCESS(rv, rv);

    mCurrentContextId.Assign(aContextId);
  } else if (!mCurrentContextId.Equals(aContextId)) {
    //XXX eventually allow for more than one active context on a given canvas
    return NS_ERROR_INVALID_ARG;
  }

  NS_ADDREF (*aContext = mCurrentContext);
  return NS_OK;
}

nsresult
HTMLCanvasElement::UpdateContext(nsIPropertyBag *aNewContextOptions)
{
  if (!mCurrentContext)
    return NS_OK;

  nsIntSize sz = GetWidthHeight();

  nsresult rv = mCurrentContext->SetIsOpaque(GetIsOpaque());
  if (NS_FAILED(rv)) {
    mCurrentContext = nullptr;
    mCurrentContextId.Truncate();
    return rv;
  }

  rv = mCurrentContext->SetContextOptions(aNewContextOptions);
  if (NS_FAILED(rv)) {
    mCurrentContext = nullptr;
    mCurrentContextId.Truncate();
    return rv;
  }

  rv = mCurrentContext->SetDimensions(sz.width, sz.height);
  if (NS_FAILED(rv)) {
    mCurrentContext = nullptr;
    mCurrentContextId.Truncate();
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

  frame->MarkLayersActive(nsChangeHint(0));

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
  nsIScriptGlobalObject *scope = OwnerDoc()->GetScriptGlobalObject();
  if (scope) {
    JSObject *obj = scope->GetGlobalJSObject();
    if (obj) {
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

  return NULL;
}

bool
HTMLCanvasElement::GetIsOpaque()
{
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

NS_IMETHODIMP_(nsIntSize)
HTMLCanvasElement::GetSizeExternal()
{
  return GetWidthHeight();
}

NS_IMETHODIMP
HTMLCanvasElement::RenderContextsExternal(gfxContext *aContext, gfxPattern::GraphicsFilter aFilter, uint32_t aFlags)
{
  if (!mCurrentContext)
    return NS_OK;

  return mCurrentContext->Render(aContext, aFilter, aFlags);
}

} // namespace dom
} // namespace mozilla

DOMCI_NODE_DATA(HTMLCanvasElement, mozilla::dom::HTMLCanvasElement)
DOMCI_DATA(MozCanvasPrintState, mozilla::dom::HTMLCanvasPrintState)
