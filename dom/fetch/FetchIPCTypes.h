/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fetch_IPCUtils_h
#define mozilla_dom_fetch_IPCUtils_h

#include "ipc/EnumSerializer.h"

#include "mozilla/dom/BindingIPCUtils.h"
#include "mozilla/dom/HeadersBinding.h"
#include "mozilla/dom/RequestBinding.h"
#include "mozilla/dom/ResponseBinding.h"
#include "mozilla/dom/FetchDriver.h"

namespace IPC {
template <>
struct ParamTraits<mozilla::dom::HeadersGuardEnum>
    : public mozilla::dom::WebIDLEnumSerializer<
          mozilla::dom::HeadersGuardEnum> {};
template <>
struct ParamTraits<mozilla::dom::ReferrerPolicy>
    : public mozilla::dom::WebIDLEnumSerializer<mozilla::dom::ReferrerPolicy> {
};
template <>
struct ParamTraits<mozilla::dom::RequestMode>
    : public mozilla::dom::WebIDLEnumSerializer<mozilla::dom::RequestMode> {};
template <>
struct ParamTraits<mozilla::dom::RequestCredentials>
    : public mozilla::dom::WebIDLEnumSerializer<
          mozilla::dom::RequestCredentials> {};
template <>
struct ParamTraits<mozilla::dom::RequestCache>
    : public mozilla::dom::WebIDLEnumSerializer<mozilla::dom::RequestCache> {};
template <>
struct ParamTraits<mozilla::dom::RequestRedirect>
    : public mozilla::dom::WebIDLEnumSerializer<mozilla::dom::RequestRedirect> {
};
template <>
struct ParamTraits<mozilla::dom::ResponseType>
    : public mozilla::dom::WebIDLEnumSerializer<mozilla::dom::ResponseType> {};

template <>
struct ParamTraits<mozilla::dom::FetchDriverObserver::EndReason>
    : public ContiguousEnumSerializerInclusive<
          mozilla::dom::FetchDriverObserver::EndReason,
          mozilla::dom::FetchDriverObserver::eAborted,
          mozilla::dom::FetchDriverObserver::eByNetworking> {};
}  // namespace IPC

#endif  // mozilla_dom_fetch_IPCUtils_h
