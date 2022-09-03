/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IPC_VideoBridgeUtils_h
#define IPC_VideoBridgeUtils_h

#include "ipc/EnumSerializer.h"

namespace mozilla {
namespace layers {

enum class VideoBridgeSource : uint8_t {
  RddProcess,
  GpuProcess,
  MFMediaEngineCDMProcess,
  _Count,
};

typedef Maybe<VideoBridgeSource> MaybeVideoBridgeSource;

}  // namespace layers
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::layers::VideoBridgeSource>
    : public ContiguousEnumSerializer<
          mozilla::layers::VideoBridgeSource,
          mozilla::layers::VideoBridgeSource::RddProcess,
          mozilla::layers::VideoBridgeSource::_Count> {};

}  // namespace IPC

#endif  // IPC_VideoBridgeUtils_h
