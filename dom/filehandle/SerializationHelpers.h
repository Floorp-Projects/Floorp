/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_filehandle_SerializationHelpers_h
#define mozilla_dom_filehandle_SerializationHelpers_h

#include "ipc/EnumSerializer.h"

#include "mozilla/dom/FileModeBinding.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::FileMode>
    : public ContiguousEnumSerializer<mozilla::dom::FileMode,
                                      mozilla::dom::FileMode::Readonly,
                                      mozilla::dom::FileMode::EndGuard_> {};

}  // namespace IPC

#endif  // mozilla_dom_filehandle_SerializationHelpers_h
