/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ShadowLayerUtilsX11_h
#define mozilla_layers_ShadowLayerUtilsX11_h

#include "gfxipc/SurfaceDescriptor.h"
#include "ipc/IPCMessageUtils.h"

namespace IPC {
class Message;

template <>
struct ParamTraits<mozilla::layers::SurfaceDescriptorX11> {
  typedef mozilla::layers::SurfaceDescriptorX11 paramType;

  static void Write(Message* aMsg, const paramType& aParam);

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult);
};

}  // namespace IPC

#endif  // mozilla_layers_ShadowLayerUtilsX11_h
