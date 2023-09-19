/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasRenderingContextHelper.h"
#include "GLContext.h"
#include "ImageBitmapRenderingContext.h"
#include "ImageEncoder.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/CanvasRenderingContext2D.h"
#include "mozilla/dom/OffscreenCanvasRenderingContext2D.h"
#include "mozilla/GfxMessageUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/webgpu/CanvasContext.h"
#include "MozFramebuffer.h"
#include "nsContentUtils.h"
#include "nsDOMJSUtils.h"
#include "nsIScriptContext.h"
#include "nsJSUtils.h"
#include "ClientWebGLContext.h"

namespace mozilla::dom {

CanvasRenderingContextHelper::CanvasRenderingContextHelper()
    : mCurrentContextType(CanvasContextType::NoContext) {}

void CanvasRenderingContextHelper::ToBlob(
    JSContext* aCx, nsIGlobalObject* aGlobal, BlobCallback& aCallback,
    const nsAString& aType, JS::Handle<JS::Value> aParams, bool aUsePlaceholder,
    ErrorResult& aRv) {
  // Encoder callback when encoding is complete.
  class EncodeCallback : public EncodeCompleteCallback {
   public:
    EncodeCallback(nsIGlobalObject* aGlobal, BlobCallback* aCallback)
        : mGlobal(aGlobal), mBlobCallback(aCallback) {}

    // This is called on main thread.
    MOZ_CAN_RUN_SCRIPT
    nsresult ReceiveBlobImpl(already_AddRefed<BlobImpl> aBlobImpl) override {
      MOZ_ASSERT(NS_IsMainThread());

      RefPtr<BlobImpl> blobImpl = aBlobImpl;

      RefPtr<Blob> blob;

      if (blobImpl) {
        blob = Blob::Create(mGlobal, blobImpl);
      }

      RefPtr<BlobCallback> callback(std::move(mBlobCallback));
      ErrorResult rv;

      callback->Call(blob, rv);

      mGlobal = nullptr;
      MOZ_ASSERT(!mBlobCallback);

      return rv.StealNSResult();
    }

    bool CanBeDeletedOnAnyThread() override {
      // EncodeCallback is used from the main thread only.
      return false;
    }

    nsCOMPtr<nsIGlobalObject> mGlobal;
    RefPtr<BlobCallback> mBlobCallback;
  };

  RefPtr<EncodeCompleteCallback> callback =
      new EncodeCallback(aGlobal, &aCallback);

  ToBlob(aCx, callback, aType, aParams, aUsePlaceholder, aRv);
}

void CanvasRenderingContextHelper::ToBlob(
    JSContext* aCx, EncodeCompleteCallback* aCallback, const nsAString& aType,
    JS::Handle<JS::Value> aParams, bool aUsePlaceholder, ErrorResult& aRv) {
  nsAutoString type;
  nsContentUtils::ASCIIToLower(aType, type);

  nsAutoString params;
  bool usingCustomParseOptions;
  aRv = ParseParams(aCx, type, aParams, params, &usingCustomParseOptions);
  if (aRv.Failed()) {
    return;
  }

  ToBlob(aCallback, type, params, usingCustomParseOptions, aUsePlaceholder,
         aRv);
}

void CanvasRenderingContextHelper::ToBlob(EncodeCompleteCallback* aCallback,
                                          nsAString& aType,
                                          const nsAString& aEncodeOptions,
                                          bool aUsingCustomOptions,
                                          bool aUsePlaceholder,
                                          ErrorResult& aRv) {
  const nsIntSize elementSize = GetWidthHeight();
  if (mCurrentContext) {
    // We disallow canvases of width or height zero, and set them to 1, so
    // we will have a discrepancy with the sizes of the canvas and the context.
    // That discrepancy is OK, the rest are not.
    if ((elementSize.width != mCurrentContext->GetWidth() &&
         (elementSize.width != 0 || mCurrentContext->GetWidth() != 1)) ||
        (elementSize.height != mCurrentContext->GetHeight() &&
         (elementSize.height != 0 || mCurrentContext->GetHeight() != 1))) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
  }

  int32_t format = 0;
  auto imageSize = gfx::IntSize{elementSize.width, elementSize.height};
  UniquePtr<uint8_t[]> imageBuffer = GetImageBuffer(&format, &imageSize);
  RefPtr<EncodeCompleteCallback> callback = aCallback;

  aRv = ImageEncoder::ExtractDataAsync(
      aType, aEncodeOptions, aUsingCustomOptions, std::move(imageBuffer),
      format, {imageSize.width, imageSize.height}, aUsePlaceholder, callback);
}

UniquePtr<uint8_t[]> CanvasRenderingContextHelper::GetImageBuffer(
    int32_t* aOutFormat, gfx::IntSize* aOutImageSize) {
  if (mCurrentContext) {
    return mCurrentContext->GetImageBuffer(aOutFormat, aOutImageSize);
  }
  return nullptr;
}

already_AddRefed<nsICanvasRenderingContextInternal>
CanvasRenderingContextHelper::CreateContext(CanvasContextType aContextType) {
  return CreateContextHelper(aContextType, layers::LayersBackend::LAYERS_NONE);
}

already_AddRefed<nsICanvasRenderingContextInternal>
CanvasRenderingContextHelper::CreateContextHelper(
    CanvasContextType aContextType, layers::LayersBackend aCompositorBackend) {
  MOZ_ASSERT(aContextType != CanvasContextType::NoContext);
  RefPtr<nsICanvasRenderingContextInternal> ret;

  switch (aContextType) {
    case CanvasContextType::NoContext:
      break;

    case CanvasContextType::Canvas2D:
      Telemetry::Accumulate(Telemetry::CANVAS_2D_USED, 1);
      ret = new CanvasRenderingContext2D(aCompositorBackend);
      break;

    case CanvasContextType::OffscreenCanvas2D:
      Telemetry::Accumulate(Telemetry::CANVAS_2D_USED, 1);
      ret = new OffscreenCanvasRenderingContext2D(aCompositorBackend);
      break;

    case CanvasContextType::WebGL1:
      Telemetry::Accumulate(Telemetry::CANVAS_WEBGL_USED, 1);

      ret = new ClientWebGLContext(/*webgl2:*/ false);

      break;

    case CanvasContextType::WebGL2:
      Telemetry::Accumulate(Telemetry::CANVAS_WEBGL_USED, 1);

      ret = new ClientWebGLContext(/*webgl2:*/ true);

      break;

    case CanvasContextType::WebGPU:
      // TODO
      // Telemetry::Accumulate(Telemetry::CANVAS_WEBGPU_USED, 1);

      ret = new webgpu::CanvasContext();

      break;

    case CanvasContextType::ImageBitmap:
      ret = new ImageBitmapRenderingContext();

      break;
  }
  MOZ_ASSERT(ret);

  ret->Initialize();
  return ret.forget();
}

already_AddRefed<nsISupports> CanvasRenderingContextHelper::GetOrCreateContext(
    JSContext* aCx, const nsAString& aContextId,
    JS::Handle<JS::Value> aContextOptions, ErrorResult& aRv) {
  CanvasContextType contextType;
  if (!CanvasUtils::GetCanvasContextType(aContextId, &contextType))
    return nullptr;

  return GetOrCreateContext(aCx, contextType, aContextOptions, aRv);
}

already_AddRefed<nsISupports> CanvasRenderingContextHelper::GetOrCreateContext(
    JSContext* aCx, CanvasContextType aContextType,
    JS::Handle<JS::Value> aContextOptions, ErrorResult& aRv) {
  if (!mCurrentContext) {
    // This canvas doesn't have a context yet.
    RefPtr<nsICanvasRenderingContextInternal> context;
    context = CreateContext(aContextType);
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

    mCurrentContext = std::move(context);
    mCurrentContextType = aContextType;

    // https://html.spec.whatwg.org/multipage/canvas.html#dom-canvas-getcontext-dev
    // Step 1. If options is not an object, then set options to null.
    JS::Rooted<JS::Value> options(RootingCx(), aContextOptions);
    if (!options.isObject()) {
      options.setNull();
    }

    nsresult rv = UpdateContext(aCx, options, aRv);
    if (NS_FAILED(rv)) {
      // See bug 645792 and bug 1215072.
      // We want to throw only if dictionary initialization fails,
      // so only in case aRv has been set to some error value.
      if (aContextType == CanvasContextType::WebGL1) {
        Telemetry::Accumulate(Telemetry::CANVAS_WEBGL_SUCCESS, 0);
      } else if (aContextType == CanvasContextType::WebGL2) {
        Telemetry::Accumulate(Telemetry::CANVAS_WEBGL2_SUCCESS, 0);
      } else if (aContextType == CanvasContextType::WebGPU) {
        // Telemetry::Accumulate(Telemetry::CANVAS_WEBGPU_SUCCESS, 0);
      }
      return nullptr;
    }
    if (aContextType == CanvasContextType::WebGL1) {
      Telemetry::Accumulate(Telemetry::CANVAS_WEBGL_SUCCESS, 1);
    } else if (aContextType == CanvasContextType::WebGL2) {
      Telemetry::Accumulate(Telemetry::CANVAS_WEBGL2_SUCCESS, 1);
    } else if (aContextType == CanvasContextType::WebGPU) {
      // Telemetry::Accumulate(Telemetry::CANVAS_WEBGPU_SUCCESS, 1);
    }
  } else {
    // We already have a context of some type.
    if (aContextType != mCurrentContextType) return nullptr;
  }

  nsCOMPtr<nsICanvasRenderingContextInternal> context = mCurrentContext;
  return context.forget();
}

nsresult CanvasRenderingContextHelper::UpdateContext(
    JSContext* aCx, JS::Handle<JS::Value> aNewContextOptions,
    ErrorResult& aRvForDictionaryInit) {
  if (!mCurrentContext) return NS_OK;

  nsIntSize sz = GetWidthHeight();

  nsCOMPtr<nsICanvasRenderingContextInternal> currentContext = mCurrentContext;

  currentContext->SetOpaqueValueFromOpaqueAttr(GetOpaqueAttr());

  nsresult rv = currentContext->SetContextOptions(aCx, aNewContextOptions,
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

nsresult CanvasRenderingContextHelper::ParseParams(
    JSContext* aCx, const nsAString& aType, const JS::Value& aEncoderOptions,
    nsAString& outParams, bool* const outUsingCustomParseOptions) {
  // Quality parameter is only valid for the image/jpeg and image/webp MIME
  // types.
  if (aType.EqualsLiteral("image/jpeg") || aType.EqualsLiteral("image/webp")) {
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
    constexpr auto mozParseOptions = u"-moz-parse-options:"_ns;
    nsAutoJSString paramString;
    if (!paramString.init(aCx, aEncoderOptions.toString())) {
      return NS_ERROR_FAILURE;
    }
    if (StringBeginsWith(paramString, mozParseOptions)) {
      nsDependentSubstring parseOptions =
          Substring(paramString, mozParseOptions.Length(),
                    paramString.Length() - mozParseOptions.Length());
      outParams.Append(parseOptions);
      *outUsingCustomParseOptions = true;
    }
  }

  return NS_OK;
}

}  // namespace mozilla::dom
