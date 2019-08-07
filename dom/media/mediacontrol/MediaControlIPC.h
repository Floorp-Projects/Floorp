/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ipc_MediaControlIPC_h
#define ipc_MediaControlIPC_h

#include "ipc/IPCMessageUtils.h"

#include "mozilla/dom/MediaController.h"

namespace IPC {
template <>
struct ParamTraits<mozilla::dom::MediaControlActions>
    : public ContiguousEnumSerializer<
          mozilla::dom::MediaControlActions,
          mozilla::dom::MediaControlActions::ePlay,
          mozilla::dom::MediaControlActions(
              mozilla::dom::MediaControlActions::eActionsNum)> {};
}  // namespace IPC

#endif  // mozilla_MediaControlIPC_hh