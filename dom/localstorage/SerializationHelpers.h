/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_SerializationHelpers_h
#define mozilla_dom_localstorage_SerializationHelpers_h

#include "ipc/IPCMessageUtils.h"

#include "mozilla/dom/LSSnapshot.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::LSSnapshot::LoadState> :
  public ContiguousEnumSerializer<mozilla::dom::LSSnapshot::LoadState,
                                  mozilla::dom::LSSnapshot::LoadState::Initial,
                                  mozilla::dom::LSSnapshot::LoadState::EndGuard>
{ };

} // namespace IPC

#endif // mozilla_dom_localstorage_SerializationHelpers_h
