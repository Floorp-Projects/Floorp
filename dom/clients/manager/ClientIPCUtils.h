/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientIPCUtils_h
#define _mozilla_dom_ClientIPCUtils_h

#include "ipc/IPCMessageUtils.h"

#include "mozilla/dom/ClientBinding.h"
#include "mozilla/dom/ClientsBinding.h"
#include "mozilla/dom/DocumentBinding.h"
#include "nsContentUtils.h"

namespace IPC {
  template<>
  struct ParamTraits<mozilla::dom::ClientType> :
    public ContiguousEnumSerializer<mozilla::dom::ClientType,
                                    mozilla::dom::ClientType::Window,
                                    mozilla::dom::ClientType::EndGuard_>
  {};

  template<>
  struct ParamTraits<mozilla::dom::FrameType> :
    public ContiguousEnumSerializer<mozilla::dom::FrameType,
                                    mozilla::dom::FrameType::Auxiliary,
                                    mozilla::dom::FrameType::EndGuard_>
  {};

  template<>
  struct ParamTraits<mozilla::dom::VisibilityState> :
    public ContiguousEnumSerializer<mozilla::dom::VisibilityState,
                                    mozilla::dom::VisibilityState::Hidden,
                                    mozilla::dom::VisibilityState::EndGuard_>
  {};

  template<>
  struct ParamTraits<nsContentUtils::StorageAccess> :
    public ContiguousEnumSerializer<nsContentUtils::StorageAccess,
                                    nsContentUtils::StorageAccess::eDeny,
                                    nsContentUtils::StorageAccess::eNumValues>
  {};
} // namespace IPC

#endif // _mozilla_dom_ClientIPCUtils_h
