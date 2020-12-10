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

#if defined(MOZ_X11)
#  include "mozilla/layers/ShadowLayerUtilsX11.h"
#endif

namespace IPC {

#if !defined(MOZ_HAVE_SURFACEDESCRIPTORX11)
template <>
struct ParamTraits<mozilla::layers::SurfaceDescriptorX11> {
  typedef mozilla::layers::SurfaceDescriptorX11 paramType;
  static void Write(Message*, const paramType&) {}
  static bool Read(const Message*, PickleIterator*, paramType*) {
    return false;
  }
};
#endif  // !defined(MOZ_HAVE_XSURFACEDESCRIPTORX11)

template <>
struct ParamTraits<mozilla::ScreenRotation>
    : public ContiguousEnumSerializer<mozilla::ScreenRotation,
                                      mozilla::ROTATION_0,
                                      mozilla::ROTATION_COUNT> {};

}  // namespace IPC

#endif  // IPC_ShadowLayerUtils_h
