/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_IPDLParamTraits_h
#define mozilla_ipc_IPDLParamTraits_h

#include "chrome/common/ipc_message_utils.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Variant.h"

#include "nsTArray.h"

#include <type_traits>

namespace mozilla {
namespace ipc {

class IProtocol;

//
// IPDLParamTraits was an extended version of ParamTraits. Unlike ParamTraits,
// IPDLParamTraits passes an additional IProtocol* argument to the
// write and read methods.
//
// Previously this was required for serializing and deserializing types which
// require knowledge of which protocol they're being sent over, however the
// actor is now available through `IPC::Message{Writer,Reader}::GetActor()` so
// the extra argument is no longer necessary. Please use `IPC::ParamTraits` in
// the future.
//
// Types which implement IPDLParamTraits are also supported by ParamTraits.
//
template <typename P>
struct IPDLParamTraits {};

//
// WriteIPDLParam and ReadIPDLParam are like IPC::WriteParam and IPC::ReadParam,
// however, they also accept a redundant extra actor argument.
//
// NOTE: WriteIPDLParam takes a universal reference, so that it can support
// whatever reference type is supported by the underlying ParamTraits::Write
// implementation.
//
template <typename P>
static MOZ_NEVER_INLINE void WriteIPDLParam(IPC::MessageWriter* aWriter,
                                            IProtocol* aActor, P&& aParam) {
  MOZ_ASSERT(aActor == aWriter->GetActor());
  IPC::WriteParam(aWriter, std::forward<P>(aParam));
}

template <typename P>
static MOZ_NEVER_INLINE bool ReadIPDLParam(IPC::MessageReader* aReader,
                                           IProtocol* aActor, P* aResult) {
  MOZ_ASSERT(aActor == aReader->GetActor());
  return IPC::ReadParam(aReader, aResult);
}

}  // namespace ipc
}  // namespace mozilla

#endif  // defined(mozilla_ipc_IPDLParamTraits_h)
