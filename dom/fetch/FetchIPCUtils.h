/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FetchIPCUtils_h
#define mozilla_dom_FetchIPCUtils_h

#include "ipc/IPCMessageUtils.h"

// Fix X11 header brain damage that conflicts with HeadersGuardEnum::None
#undef None

#include "mozilla/dom/HeadersBinding.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/Response.h"

namespace IPC {
  template<>
  struct ParamTraits<mozilla::dom::HeadersGuardEnum> :
    public ContiguousEnumSerializer<mozilla::dom::HeadersGuardEnum,
                                    mozilla::dom::HeadersGuardEnum::None,
                                    mozilla::dom::HeadersGuardEnum::EndGuard_> {};
  template<>
  struct ParamTraits<mozilla::dom::RequestMode> :
    public ContiguousEnumSerializer<mozilla::dom::RequestMode,
                                    mozilla::dom::RequestMode::Same_origin,
                                    mozilla::dom::RequestMode::EndGuard_> {};
  template<>
  struct ParamTraits<mozilla::dom::RequestCredentials> :
    public ContiguousEnumSerializer<mozilla::dom::RequestCredentials,
                                    mozilla::dom::RequestCredentials::Omit,
                                    mozilla::dom::RequestCredentials::EndGuard_> {};
  template<>
  struct ParamTraits<mozilla::dom::RequestCache> :
    public ContiguousEnumSerializer<mozilla::dom::RequestCache,
                                    mozilla::dom::RequestCache::Default,
                                    mozilla::dom::RequestCache::EndGuard_> {};
  template<>
  struct ParamTraits<mozilla::dom::RequestContext> :
    public ContiguousEnumSerializer<mozilla::dom::RequestContext,
                                    mozilla::dom::RequestContext::Audio,
                                    mozilla::dom::RequestContext::EndGuard_> {};
  template<>
  struct ParamTraits<mozilla::dom::ResponseType> :
    public ContiguousEnumSerializer<mozilla::dom::ResponseType,
                                    mozilla::dom::ResponseType::Basic,
                                    mozilla::dom::ResponseType::EndGuard_> {};
}

#endif // mozilla_dom_FetchIPCUtils_h
