/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ServiceWorkerIPCUtils_h
#define _mozilla_dom_ServiceWorkerIPCUtils_h

#include "ipc/EnumSerializer.h"

// Undo X11/X.h's definition of None
#undef None

#include "mozilla/dom/ServiceWorkerBinding.h"
#include "mozilla/dom/ServiceWorkerRegistrationBinding.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::ServiceWorkerState>
    : public ContiguousEnumSerializer<
          mozilla::dom::ServiceWorkerState,
          mozilla::dom::ServiceWorkerState::Parsed,
          mozilla::dom::ServiceWorkerState::EndGuard_> {};

template <>
struct ParamTraits<mozilla::dom::ServiceWorkerUpdateViaCache>
    : public ContiguousEnumSerializer<
          mozilla::dom::ServiceWorkerUpdateViaCache,
          mozilla::dom::ServiceWorkerUpdateViaCache::Imports,
          mozilla::dom::ServiceWorkerUpdateViaCache::EndGuard_> {};

}  // namespace IPC

#endif  // _mozilla_dom_ServiceWorkerIPCUtils_h
