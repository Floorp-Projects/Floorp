/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_IPCUtils_h
#define mozilla_dom_cache_IPCUtils_h

#include "ipc/EnumSerializer.h"

#include "mozilla/dom/cache/Types.h"

namespace IPC {
template <>
struct ParamTraits<mozilla::dom::cache::Namespace>
    : public ContiguousEnumSerializer<
          mozilla::dom::cache::Namespace,
          mozilla::dom::cache::DEFAULT_NAMESPACE,
          mozilla::dom::cache::NUMBER_OF_NAMESPACES> {};

template <>
struct ParamTraits<mozilla::dom::cache::OpenMode>
    : public ContiguousEnumSerializer<mozilla::dom::cache::OpenMode,
                                      mozilla::dom::cache::OpenMode::Eager,
                                      mozilla::dom::cache::OpenMode::NumTypes> {
};
}  // namespace IPC

#endif  // mozilla_dom_cache_IPCUtils_h
