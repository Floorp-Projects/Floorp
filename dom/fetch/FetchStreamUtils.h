/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _mozilla_dom_fetch_FetchStreamUtils_h
#define _mozilla_dom_fetch_FetchStreamUtils_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/NotNull.h"
#include "mozilla/dom/FetchTypes.h"

#include "nsIInputStream.h"

#include <cstdint>

namespace mozilla {

namespace ipc {
class PBackgroundParent;
}

namespace dom {

// Convert a ParentToParentStream received over IPC to an nsIInputStream. Can
// only be called in the parent process.
NotNull<nsCOMPtr<nsIInputStream>> ToInputStream(
    const ParentToParentStream& aStream);

// Convert a ParentToChildStream received over IPC to an nsIInputStream. Can
// only be called in a content process.
NotNull<nsCOMPtr<nsIInputStream>> ToInputStream(
    const ParentToChildStream& aStream);

// Serialize an nsIInputStream for IPC inside the parent process. Can only be
// called in the parent process.
ParentToParentStream ToParentToParentStream(
    const NotNull<nsCOMPtr<nsIInputStream>>& aStream, int64_t aStreamSize);

// Serialize an nsIInputStream for IPC from the parent process to a content
// process. Can only be called in the parent process.
ParentToChildStream ToParentToChildStream(
    const NotNull<nsCOMPtr<nsIInputStream>>& aStream, int64_t aStreamSize,
    NotNull<mozilla::ipc::PBackgroundParent*> aBackgroundParent,
    bool aSerializeAsLazy = true);

// Convert a ParentToParentStream to a ParentToChildStream. Can only be called
// in the parent process.
ParentToChildStream ToParentToChildStream(
    const ParentToParentStream& aStream, int64_t aStreamSize,
    NotNull<mozilla::ipc::PBackgroundParent*> aBackgroundParent);

}  // namespace dom

}  // namespace mozilla

#endif  // _mozilla_dom_fetch_FetchStreamUtils_h
