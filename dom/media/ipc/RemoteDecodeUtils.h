/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_IPC_REMOTEDECODEUTILS_H_
#define DOM_MEDIA_IPC_REMOTEDECODEUTILS_H_

#include "mozilla/Logging.h"
#include "mozilla/RemoteDecoderManagerChild.h"
#include "mozilla/ipc/UtilityProcessSandboxing.h"

namespace mozilla {

inline LazyLogModule gRemoteDecodeLog{"RemoteDecode"};

// Return the sandboxing kind of the current utility process. Should only be
// called on the utility process.
ipc::SandboxingKind GetCurrentSandboxingKind();

ipc::SandboxingKind GetSandboxingKindFromLocation(RemoteDecodeIn aLocation);

RemoteDecodeIn GetRemoteDecodeInFromKind(ipc::SandboxingKind aKind);

RemoteDecodeIn GetRemoteDecodeInFromVideoBridgeSource(
    layers::VideoBridgeSource aSource);

const char* RemoteDecodeInToStr(RemoteDecodeIn aLocation);

}  // namespace mozilla

#endif  // DOM_MEDIA_IPC_REMOTEDECODEUTILS_H_
