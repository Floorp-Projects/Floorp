/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_IPCForwards_h
#define mozilla_ipc_IPCForwards_h

// A few helpers to avoid having to include lots of stuff in headers.

namespace mozilla {
template <typename T>
class Maybe;

namespace ipc {
class IProtocol;
}
}  // namespace mozilla

namespace IPC {
class Message;
class MessageReader;
class MessageWriter;
template <typename P>
inline mozilla::Maybe<P> ReadParam(MessageReader*);
}  // namespace IPC

// TODO(bug 1812271): Remove users of this macro.
#define ALLOW_DEPRECATED_READPARAM                           \
  enum { kHasDeprecatedReadParamPrivateConstructor = true }; \
  template <typename P>                                      \
  friend mozilla::Maybe<P> IPC::ReadParam(IPC::MessageReader*);

#endif
