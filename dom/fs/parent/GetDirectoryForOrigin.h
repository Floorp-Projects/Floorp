/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_GETDIRECTORYFORORIGIN_H_
#define DOM_FS_PARENT_GETDIRECTORYFORORIGIN_H_

#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/quota/ForwardDecls.h"

template <class T>
class nsCOMPtr;

class nsIFile;

namespace mozilla {
template <typename V, typename E>
class Result;
}

namespace mozilla::dom::quota {
class QuotaManager;
}

namespace mozilla::dom::fs {

// XXX This function can be removed once the integration with quota manager is
// finished.
Result<nsCOMPtr<nsIFile>, QMResult> GetDirectoryForOrigin(
    const quota::QuotaManager& aQuotaManager, const Origin& aOrigin);

}  // namespace mozilla::dom::fs

#endif  // DOM_FS_PARENT_GETDIRECTORYFORORIGIN_H_
