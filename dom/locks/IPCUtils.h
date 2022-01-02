/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_LOCKS_IPCUTILS_H_
#define DOM_LOCKS_IPCUTILS_H_

#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "mozilla/dom/LockManagerBinding.h"

namespace IPC {
using LockMode = mozilla::dom::LockMode;
template <>
struct ParamTraits<LockMode>
    : public ContiguousEnumSerializerInclusive<LockMode, LockMode::Shared,
                                               LockMode::Exclusive> {};

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::LockInfo, mName, mMode,
                                  mClientId);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::LockManagerSnapshot, mHeld,
                                  mPending);
}  // namespace IPC

#endif  // DOM_LOCKS_IPCUTILS_H_
