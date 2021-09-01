/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IPC_ShadowLayerUtils_h
#define IPC_ShadowLayerUtils_h

#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtils.h"
#include "GLContextTypes.h"
#include "SurfaceDescriptor.h"
#include "SurfaceTypes.h"
#include "mozilla/WidgetUtils.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::ScreenRotation>
    : public ContiguousEnumSerializer<mozilla::ScreenRotation,
                                      mozilla::ROTATION_0,
                                      mozilla::ROTATION_COUNT> {};

}  // namespace IPC

#endif  // IPC_ShadowLayerUtils_h
