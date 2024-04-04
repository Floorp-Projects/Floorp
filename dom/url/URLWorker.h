/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_URLWorker_h
#define mozilla_dom_URLWorker_h

#include "URL.h"
#include "URLMainThread.h"

namespace mozilla {

namespace net {
class nsStandardURL;
}

namespace dom {

class URLWorker final {
 public:
  static already_AddRefed<URLWorker> Constructor(
      const GlobalObject& aGlobal, const nsACString& aURL,
      const Optional<nsACString>& aBase, ErrorResult& aRv);

  static already_AddRefed<URLWorker> Constructor(const GlobalObject& aGlobal,
                                                 const nsACString& aURL,
                                                 const nsACString& aBase,
                                                 ErrorResult& aRv);

  static void CreateObjectURL(const GlobalObject& aGlobal, Blob& aBlob,
                              nsACString& aResult, mozilla::ErrorResult& aRv);
  static void RevokeObjectURL(const GlobalObject& aGlobal,
                              const nsACString& aUrl, ErrorResult& aRv);
  static bool IsValidObjectURL(const GlobalObject& aGlobal,
                               const nsACString& aUrl, ErrorResult& aRv);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_URLWorker_h
