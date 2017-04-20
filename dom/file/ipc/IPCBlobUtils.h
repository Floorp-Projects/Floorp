/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IPCBlobUtils_h
#define mozilla_dom_IPCBlobUtils_h

#include "mozilla/dom/File.h"

namespace mozilla {

namespace ipc {
class PBackgroundChild;
class PBackgroundParent;
}

namespace dom {

class nsIContentChild;
class nsIContentParent;

namespace IPCBlobUtils {

already_AddRefed<BlobImpl>
Deserialize(const IPCBlob& aIPCBlob);

// These 4 methods serialize aBlobImpl into aIPCBlob using the right manager.

nsresult
Serialize(BlobImpl* aBlobImpl, nsIContentChild* aManager, IPCBlob& aIPCBlob);

nsresult
Serialize(BlobImpl* aBlobImpl, PBackgroundChild* aManager, IPCBlob& aIPCBlob);

nsresult
Serialize(BlobImpl* aBlobImpl, nsIContentParent* aManager, IPCBlob& aIPCBlob);

nsresult
Serialize(BlobImpl* aBlobImpl, PBackgroundParent* aManager, IPCBlob& aIPCBlob);

} // IPCBlobUtils
} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_IPCBlobUtils_h
