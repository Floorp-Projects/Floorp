/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BodyUtil_h
#define mozilla_dom_BodyUtil_h

#include "nsString.h"
#include "nsError.h"

#include "mozilla/dom/File.h"
#include "mozilla/dom/FormData.h"

#include "js/Utility.h"  // JS::FreePolicy

namespace mozilla {
class ErrorResult;

namespace dom {

class BodyUtil final {
 private:
  BodyUtil() = delete;

 public:
  /**
   * Creates an array buffer from an array, assigning the result to |aValue|.
   * The array buffer takes ownership of |aInput|, which must be allocated
   * by |malloc|.
   */
  static void ConsumeArrayBuffer(JSContext* aCx,
                                 JS::MutableHandle<JSObject*> aValue,
                                 uint32_t aInputLength,
                                 UniquePtr<uint8_t[], JS::FreePolicy> aInput,
                                 ErrorResult& aRv);

  /**
   * Creates an in-memory blob from an array. The blob takes ownership of
   * |aInput|, which must be allocated by |malloc|.
   */
  static already_AddRefed<Blob> ConsumeBlob(nsIGlobalObject* aParent,
                                            const nsString& aMimeType,
                                            uint32_t aInputLength,
                                            uint8_t* aInput, ErrorResult& aRv);

  /**
   * Creates a form data object from a UTF-8 encoded |aStr|. Returns |nullptr|
   * and sets |aRv| to MSG_BAD_FORMDATA if |aStr| contains invalid data.
   */
  static already_AddRefed<FormData> ConsumeFormData(
      nsIGlobalObject* aParent, const nsCString& aMimeType,
      const nsACString& aMixedCaseMimeType, const nsCString& aStr,
      ErrorResult& aRv);

  /**
   * UTF-8 decodes |aInput| into |aText|. The caller may free |aInput|
   * once this method returns.
   */
  static nsresult ConsumeText(uint32_t aInputLength, uint8_t* aInput,
                              nsString& aText);

  /**
   * Parses a UTF-8 encoded |aStr| as JSON, assigning the result to |aValue|.
   * Sets |aRv| to a syntax error if |aStr| contains invalid data.
   */
  static void ConsumeJson(JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
                          const nsString& aStr, ErrorResult& aRv);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_BodyUtil_h
