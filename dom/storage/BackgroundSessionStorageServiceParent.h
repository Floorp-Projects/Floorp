/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_STORAGE_BACKGROUNDSESSIONSTORAGESERVICEPARENT_H_
#define DOM_STORAGE_BACKGROUNDSESSIONSTORAGESERVICEPARENT_H_

#include "mozilla/dom/PBackgroundSessionStorageServiceParent.h"

namespace mozilla::dom {

class BackgroundSessionStorageServiceParent final
    : public PBackgroundSessionStorageServiceParent {
 public:
  NS_INLINE_DECL_REFCOUNTING(BackgroundSessionStorageServiceParent, override)

  mozilla::ipc::IPCResult RecvClearStoragesForOrigin(
      const nsACString& aOriginAttrs, const nsACString& aOriginKey);

 private:
  ~BackgroundSessionStorageServiceParent() = default;
};

}  // namespace mozilla::dom

#endif  // DOM_STORAGE_BACKGROUNDSESSIONSTORAGESERVICEPARENT_H_
