/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_useractivation_ipc_utils_h__
#define mozilla_dom_useractivation_ipc_utils_h__

#include "ipc/EnumSerializer.h"

// Undo X11/X.h's definition of None
#undef None

#include "mozilla/dom/UserActivation.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::UserActivation::State>
    : public ContiguousEnumSerializer<
          mozilla::dom::UserActivation::State,
          mozilla::dom::UserActivation::State::None,
          mozilla::dom::UserActivation::State::EndGuard_> {};

}  // namespace IPC

#endif  // mozilla_dom_useractivation_ipc_utils_h__
