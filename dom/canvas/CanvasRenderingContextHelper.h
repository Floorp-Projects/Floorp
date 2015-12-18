/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOM_CANVASRENDERINGCONTEXTHELPER_H_
#define MOZILLA_DOM_CANVASRENDERINGCONTEXTHELPER_H_

#include "mozilla/dom/BindingDeclarations.h"
#include "nsSize.h"

class nsICanvasRenderingContextInternal;
class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace dom {

class EncodeCompleteCallback;
class FileCallback;

enum class CanvasContextType : uint8_t {
  NoContext,
  Canvas2D,
  WebGL1,
  WebGL2
};

/**
 * Povides common RenderingContext functionality used by both OffscreenCanvas
 * and HTMLCanvasElement.
 */
class CanvasRenderingContextHelper
{
public:
  virtual already_AddRefed<nsISupports>
  GetContext(JSContext* aCx,
             const nsAString& aContextId,
             JS::Handle<JS::Value> aContextOptions,
             ErrorResult& aRv);

  virtual bool GetOpaqueAttr() = 0;

protected:
  virtual nsresult UpdateContext(JSContext* aCx,
                                 JS::Handle<JS::Value> aNewContextOptions,
                                 ErrorResult& aRvForDictionaryInit);

  virtual nsresult ParseParams(JSContext* aCx,
                               const nsAString& aType,
                               const JS::Value& aEncoderOptions,
                               nsAString& outParams,
                               bool* const outCustomParseOptions);

  void ToBlob(JSContext* aCx, nsIGlobalObject* global, FileCallback& aCallback,
              const nsAString& aType, JS::Handle<JS::Value> aParams,
              ErrorResult& aRv);

  void ToBlob(JSContext* aCx, nsIGlobalObject* aGlobal, EncodeCompleteCallback* aCallback,
              const nsAString& aType, JS::Handle<JS::Value> aParams,
              ErrorResult& aRv);

  virtual already_AddRefed<nsICanvasRenderingContextInternal>
  CreateContext(CanvasContextType aContextType);

  virtual nsIntSize GetWidthHeight() = 0;

  CanvasContextType mCurrentContextType;
  nsCOMPtr<nsICanvasRenderingContextInternal> mCurrentContext;
};

} // namespace dom
} // namespace mozilla

#endif // MOZILLA_DOM_CANVASRENDERINGCONTEXTHELPER_H_
