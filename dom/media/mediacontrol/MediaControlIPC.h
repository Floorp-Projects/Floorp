/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ipc_MediaControlIPC_h
#define ipc_MediaControlIPC_h

#include "ipc/IPCMessageUtils.h"

#include "mozilla/dom/ContentMediaController.h"
#include "mozilla/dom/MediaControlKeysEvent.h"

namespace IPC {
template <>
struct ParamTraits<mozilla::dom::MediaControlKeysEvent>
    : public ContiguousEnumSerializerInclusive<
          mozilla::dom::MediaControlKeysEvent,
          mozilla::dom::MediaControlKeysEvent::ePlay,
          mozilla::dom::MediaControlKeysEvent::eStop> {};

template <>
struct ParamTraits<mozilla::dom::MediaPlaybackState>
    : public ContiguousEnumSerializerInclusive<
          mozilla::dom::MediaPlaybackState,
          mozilla::dom::MediaPlaybackState::eStarted,
          mozilla::dom::MediaPlaybackState::eStopped> {};

template <>
struct ParamTraits<mozilla::dom::MediaAudibleState>
    : public ContiguousEnumSerializerInclusive<
          mozilla::dom::MediaAudibleState,
          mozilla::dom::MediaAudibleState::eInaudible,
          mozilla::dom::MediaAudibleState::eAudible> {};

}  // namespace IPC

#endif  // mozilla_MediaControlIPC_hh