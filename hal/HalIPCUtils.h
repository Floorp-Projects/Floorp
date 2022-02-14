/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HalIPCUtils_h
#define mozilla_HalIPCUtils_h

#include "HalScreenConfiguration.h"

#include "ipc/EnumSerializer.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::hal::ScreenOrientation>
    : public BitFlagsEnumSerializer<mozilla::hal::ScreenOrientation,
                                    mozilla::hal::kAllScreenOrientationBits> {};

}  // namespace IPC

#endif
