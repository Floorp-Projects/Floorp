/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOM_CANVASRENDERINGCONTEXTHELPER_H_
#define MOZILLA_DOM_CANVASRENDERINGCONTEXTHELPER_H_

#include "mozilla/UniquePtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/layers/LayersTypes.h"
#include "nsSize.h"

class nsICanvasRenderingContextInternal;
class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace dom {

class BlobCallback;
class EncodeCompleteCallback;

enum class CanvasContextType : uint8_t {
  NoContext,
  Canvas2D,
  OffscreenCanvas2D,
  WebGL1,
  WebGL2,
  WebGPU,
  ImageBitmap
};

/**
 * Povides common RenderingContext functionality used by both OffscreenCanvas
 * and HTMLCanvasElement.
 */
class CanvasRenderingContextHelper {
 public:
  CanvasRenderingContextHelper();

  virtual bool GetOpaqueAttr() = 0;

 protected:
  virtual nsresult UpdateContext(JSContext* aCx,
                                 JS::Handle<JS::Value> aNewContextOptions,
                                 ErrorResult& aRvForDictionaryInit);

  virtual nsresult ParseParams(JSContext* aCx, const nsAString& aType,
                               const JS::Value& aEncoderOptions,
                               nsAString& outParams,
                               bool* const outCustomParseOptions);

  void ToBlob(JSContext* aCx, nsIGlobalObject* global, BlobCallback& aCallback,
              const nsAString& aType, JS::Handle<JS::Value> aParams,
              bool aUsePlaceholder, ErrorResult& aRv);

  void ToBlob(JSContext* aCx, EncodeCompleteCallback* aCallback,
              const nsAString& aType, JS::Handle<JS::Value> aParams,
              bool aUsePlaceholder, ErrorResult& aRv);

  void ToBlob(EncodeCompleteCallback* aCallback, nsAString& aType,
              const nsAString& aEncodeOptions, bool aUsingCustomOptions,
              bool aUsePlaceholder, ErrorResult& aRv);

  virtual UniquePtr<uint8_t[]> GetImageBuffer(int32_t* aOutFormat,
                                              gfx::IntSize* aOutImageSize);

  already_AddRefed<nsISupports> GetOrCreateContext(
      JSContext* aCx, const nsAString& aContextId,
      JS::Handle<JS::Value> aContextOptions, ErrorResult& aRv);

  already_AddRefed<nsISupports> GetOrCreateContext(
      JSContext* aCx, CanvasContextType aContextType,
      JS::Handle<JS::Value> aContextOptions, ErrorResult& aRv);

  virtual already_AddRefed<nsICanvasRenderingContextInternal> CreateContext(
      CanvasContextType aContextType);

  already_AddRefed<nsICanvasRenderingContextInternal> CreateContextHelper(
      CanvasContextType aContextType, layers::LayersBackend aCompositorBackend);

  virtual nsIntSize GetWidthHeight() = 0;

  CanvasContextType mCurrentContextType;
  nsCOMPtr<nsICanvasRenderingContextInternal> mCurrentContext;
};

}  // namespace dom
namespace CanvasUtils {
bool GetCanvasContextType(const nsAString&, dom::CanvasContextType* const);
}  // namespace CanvasUtils
}  // namespace mozilla

#endif  // MOZILLA_DOM_CANVASRENDERINGCONTEXTHELPER_H_
