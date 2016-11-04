/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasRenderingContextHelper.h"
#include "ImageBitmapRenderingContext.h"
#include "ImageEncoder.h"
#include "mozilla/dom/CanvasRenderingContext2D.h"
#include "mozilla/Telemetry.h"
#include "mozilla/UniquePtr.h"
#include "nsContentUtils.h"
#include "nsDOMJSUtils.h"
#include "nsIScriptContext.h"
#include "nsJSUtils.h"
#include "WebGL1Context.h"
#include "WebGL2Context.h"

namespace mozilla {
namespace dom {

void
CanvasRenderingContextHelper::ToBlob(JSContext* aCx,
                                     nsIGlobalObject* aGlobal,
                                     BlobCallback& aCallback,
                                     const nsAString& aType,
                                     JS::Handle<JS::Value> aParams,
                                     ErrorResult& aRv)
{
  // Encoder callback when encoding is complete.
  class EncodeCallback : public EncodeCompleteCallback
  {
  public:
    EncodeCallback(nsIGlobalObject* aGlobal, BlobCallback* aCallback)
      : mGlobal(aGlobal)
      , mBlobCallback(aCallback) {}

    // This is called on main thread.
    nsresult ReceiveBlob(already_AddRefed<Blob> aBlob)
    {
      RefPtr<Blob> blob = aBlob;

      ErrorResult rv;
      uint64_t size = blob->GetSize(rv);
      if (rv.Failed()) {
        rv.SuppressException();
      } else {
        AutoJSAPI jsapi;
        if (jsapi.Init(mGlobal)) {
          JS_updateMallocCounter(jsapi.cx(), size);
        }
      }

      RefPtr<Blob> newBlob = Blob::Create(mGlobal, blob->Impl());

      mBlobCallback->Call(*newBlob, rv);

      mGlobal = nullptr;
      mBlobCallback = nullptr;

      return rv.StealNSResult();
    }

    nsCOMPtr<nsIGlobalObject> mGlobal;
    RefPtr<BlobCallback> mBlobCallback;
  };

  RefPtr<EncodeCompleteCallback> callback =
    new EncodeCallback(aGlobal, &aCallback);

  ToBlob(aCx, aGlobal, callback, aType, aParams, aRv);
}

void
CanvasRenderingContextHelper::ToBlob(JSContext* aCx,
                                     nsIGlobalObject* aGlobal,
                                     EncodeCompleteCallback* aCallback,
                                     const nsAString& aType,
                                     JS::Handle<JS::Value> aParams,
                                     ErrorResult& aRv)
{
  nsAutoString type;
  nsContentUtils::ASCIIToLower(aType, type);

  nsAutoString params;
  bool usingCustomParseOptions;
  aRv = ParseParams(aCx, type, aParams, params, &usingCustomParseOptions);
  if (aRv.Failed()) {
    return;
  }

  if (mCurrentContext) {
    // We disallow canvases of width or height zero, and set them to 1, so
    // we will have a discrepancy with the sizes of the canvas and the context.
    // That discrepancy is OK, the rest are not.
    nsIntSize elementSize = GetWidthHeight();
    if ((elementSize.width != mCurrentContext->GetWidth() &&
         (elementSize.width != 0 || mCurrentContext->GetWidth() != 1)) ||
        (elementSize.height != mCurrentContext->GetHeight() &&
         (elementSize.height != 0 || mCurrentContext->GetHeight() != 1))) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
  }

  UniquePtr<uint8_t[]> imageBuffer;
  int32_t format = 0;
  if (mCurrentContext) {
    imageBuffer = mCurrentContext->GetImageBuffer(&format);
  }

  RefPtr<EncodeCompleteCallback> callback = aCallback;

  aRv = ImageEncoder::ExtractDataAsync(type,
                                       params,
                                       usingCustomParseOptions,
                                       Move(imageBuffer),
                                       format,
                                       GetWidthHeight(),
                                       callback);
}

already_AddRefed<nsICanvasRenderingContextInternal>
CanvasRenderingContextHelper::CreateContext(CanvasContextType aContextType)
{
  return CreateContextHelper(aContextType, layers::LayersBackend::LAYERS_NONE);
}

already_AddRefed<nsICanvasRenderingContextInternal>
CanvasRenderingContextHelper::CreateContextHelper(CanvasContextType aContextType,
                                                  layers::LayersBackend aCompositorBackend)
{
  MOZ_ASSERT(aContextType != CanvasContextType::NoContext);
  RefPtr<nsICanvasRenderingContextInternal> ret;

  switch (aContextType) {
  case CanvasContextType::NoContext:
    break;

  case CanvasContextType::Canvas2D:
    Telemetry::Accumulate(Telemetry::CANVAS_2D_USED, 1);
    ret = new CanvasRenderingContext2D(aCompositorBackend);
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

  case CanvasContextType::ImageBitmap:
    ret = new ImageBitmapRenderingContext();

    break;
  }
  MOZ_ASSERT(ret);

  return ret.forget();
}

already_AddRefed<nsISupports>
CanvasRenderingContextHelper::GetContext(JSContext* aCx,
                                         const nsAString& aContextId,
                                         JS::Handle<JS::Value> aContextOptions,
                                         ErrorResult& aRv)
{
  CanvasContextType contextType;
  if (!CanvasUtils::GetCanvasContextType(aContextId, &contextType))
    return nullptr;

  if (!mCurrentContext) {
    // This canvas doesn't have a context yet.
    RefPtr<nsICanvasRenderingContextInternal> context;
    context = CreateContext(contextType);
    if (!context) {
      return nullptr;
    }

    // Ensure that the context participates in CC.  Note that returning a
    // CC participant from QI doesn't addref.
    nsXPCOMCycleCollectionParticipant* cp = nullptr;
    CallQueryInterface(context, &cp);
    if (!cp) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    mCurrentContext = context.forget();
    mCurrentContextType = contextType;

    nsresult rv = UpdateContext(aCx, aContextOptions, aRv);
    if (NS_FAILED(rv)) {
      // See bug 645792 and bug 1215072.
      // We want to throw only if dictionary initialization fails,
      // so only in case aRv has been set to some error value.
      if (contextType == CanvasContextType::WebGL1)
        Telemetry::Accumulate(Telemetry::CANVAS_WEBGL_SUCCESS, 0);
      else if (contextType == CanvasContextType::WebGL2)
        Telemetry::Accumulate(Telemetry::CANVAS_WEBGL2_SUCCESS, 0);
      return nullptr;
    }
    if (contextType == CanvasContextType::WebGL1)
      Telemetry::Accumulate(Telemetry::CANVAS_WEBGL_SUCCESS, 1);
    else if (contextType == CanvasContextType::WebGL2)
      Telemetry::Accumulate(Telemetry::CANVAS_WEBGL2_SUCCESS, 1);
  } else {
    // We already have a context of some type.
    if (contextType != mCurrentContextType)
      return nullptr;
  }

  nsCOMPtr<nsICanvasRenderingContextInternal> context = mCurrentContext;
  return context.forget();
}

nsresult
CanvasRenderingContextHelper::UpdateContext(JSContext* aCx,
                                            JS::Handle<JS::Value> aNewContextOptions,
                                            ErrorResult& aRvForDictionaryInit)
{
  if (!mCurrentContext)
    return NS_OK;

  nsIntSize sz = GetWidthHeight();

  nsCOMPtr<nsICanvasRenderingContextInternal> currentContext = mCurrentContext;

  nsresult rv = currentContext->SetIsOpaque(GetOpaqueAttr());
  if (NS_FAILED(rv)) {
    mCurrentContext = nullptr;
    return rv;
  }

  rv = currentContext->SetContextOptions(aCx, aNewContextOptions,
                                         aRvForDictionaryInit);
  if (NS_FAILED(rv)) {
    mCurrentContext = nullptr;
    return rv;
  }

  rv = currentContext->SetDimensions(sz.width, sz.height);
  if (NS_FAILED(rv)) {
    mCurrentContext = nullptr;
  }

  return rv;
}

nsresult
CanvasRenderingContextHelper::ParseParams(JSContext* aCx,
                                          const nsAString& aType,
                                          const JS::Value& aEncoderOptions,
                                          nsAString& outParams,
                                          bool* const outUsingCustomParseOptions)
{
  // Quality parameter is only valid for the image/jpeg MIME type
  if (aType.EqualsLiteral("image/jpeg")) {
    if (aEncoderOptions.isNumber()) {
      double quality = aEncoderOptions.toNumber();
      // Quality must be between 0.0 and 1.0, inclusive
      if (quality >= 0.0 && quality <= 1.0) {
        outParams.AppendLiteral("quality=");
        outParams.AppendInt(NS_lround(quality * 100.0));
      }
    }
  }

  // If we haven't parsed the aParams check for proprietary options.
  // The proprietary option -moz-parse-options will take a image lib encoder
  // parse options string as is and pass it to the encoder.
  *outUsingCustomParseOptions = false;
  if (outParams.Length() == 0 && aEncoderOptions.isString()) {
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
      outParams.Append(parseOptions);
      *outUsingCustomParseOptions = true;
    }
  }

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
