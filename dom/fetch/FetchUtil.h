/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FetchUtil_h
#define mozilla_dom_FetchUtil_h

#include "nsString.h"
#include "nsError.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FormData.h"

#define WASM_CONTENT_TYPE "application/wasm"
#define WASM_ALT_DATA_TYPE_V1 "wasm/machine-code/1"

class nsIPrincipal;
class nsIHttpChannel;

namespace mozilla {
namespace dom {

class Document;
class InternalRequest;
class WorkerPrivate;

class FetchUtil final {
 private:
  FetchUtil() = delete;

 public:
  /**
   * Sets outMethod to a valid HTTP request method string based on an input
   * method. Implements checks and normalization as specified by the Fetch
   * specification. Returns NS_ERROR_DOM_SECURITY_ERR if the method is invalid.
   * Otherwise returns NS_OK and the normalized method via outMethod.
   */
  static nsresult GetValidRequestMethod(const nsACString& aMethod,
                                        nsCString& outMethod);

  /**
   * Extracts an HTTP header from a substring range.
   */
  static bool ExtractHeader(nsACString::const_iterator& aStart,
                            nsACString::const_iterator& aEnd,
                            nsCString& aHeaderName, nsCString& aHeaderValue,
                            bool* aWasEmptyHeader);

  static nsresult SetRequestReferrer(nsIPrincipal* aPrincipal, Document* aDoc,
                                     nsIHttpChannel* aChannel,
                                     InternalRequest& aRequest);

  /**
   * Check that the given object is a Response and, if so, stream to the given
   * JS consumer. On any failure, this function will report an error on the
   * given JSContext before returning false. If executing in a worker, the
   * WorkerPrivate must be given.
   */
  static bool StreamResponseToJS(JSContext* aCx, JS::HandleObject aObj,
                                 JS::MimeType aMimeType,
                                 JS::StreamConsumer* aConsumer,
                                 WorkerPrivate* aMaybeWorker);

  /**
   * Called by JS to report (i.e., throw) an error that was passed to the
   * JS::StreamConsumer::streamError() method on a random stream thread.
   * This method is passed by function pointer to the JS engine hence the
   * untyped 'size_t' instead of Gecko 'nsresult'.
   */
  static void ReportJSStreamError(JSContext* aCx, size_t aErrorCode);
};

}  // namespace dom
}  // namespace mozilla
#endif
