/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fetch_IPCUtils_h
#define mozilla_dom_fetch_IPCUtils_h

#include "ipc/EnumSerializer.h"

#include "mozilla/dom/HeadersBinding.h"
#include "mozilla/dom/RequestBinding.h"
#include "mozilla/dom/ResponseBinding.h"

namespace IPC {
template <>
struct ParamTraits<mozilla::dom::HeadersGuardEnum>
    : public ContiguousEnumSerializer<
          mozilla::dom::HeadersGuardEnum, mozilla::dom::HeadersGuardEnum::None,
          mozilla::dom::HeadersGuardEnum::EndGuard_> {};
template <>
struct ParamTraits<mozilla::dom::ReferrerPolicy>
    : public ContiguousEnumSerializer<mozilla::dom::ReferrerPolicy,
                                      mozilla::dom::ReferrerPolicy::_empty,
                                      mozilla::dom::ReferrerPolicy::EndGuard_> {
};
template <>
struct ParamTraits<mozilla::dom::RequestMode>
    : public ContiguousEnumSerializer<mozilla::dom::RequestMode,
                                      mozilla::dom::RequestMode::Same_origin,
                                      mozilla::dom::RequestMode::EndGuard_> {};
template <>
struct ParamTraits<mozilla::dom::RequestCredentials>
    : public ContiguousEnumSerializer<
          mozilla::dom::RequestCredentials,
          mozilla::dom::RequestCredentials::Omit,
          mozilla::dom::RequestCredentials::EndGuard_> {};
template <>
struct ParamTraits<mozilla::dom::RequestCache>
    : public ContiguousEnumSerializer<mozilla::dom::RequestCache,
                                      mozilla::dom::RequestCache::Default,
                                      mozilla::dom::RequestCache::EndGuard_> {};
template <>
struct ParamTraits<mozilla::dom::RequestRedirect>
    : public ContiguousEnumSerializer<
          mozilla::dom::RequestRedirect, mozilla::dom::RequestRedirect::Follow,
          mozilla::dom::RequestRedirect::EndGuard_> {};
template <>
struct ParamTraits<mozilla::dom::ResponseType>
    : public ContiguousEnumSerializer<mozilla::dom::ResponseType,
                                      mozilla::dom::ResponseType::Basic,
                                      mozilla::dom::ResponseType::EndGuard_> {};
}  // namespace IPC

#endif  // mozilla_dom_fetch_IPCUtils_h
