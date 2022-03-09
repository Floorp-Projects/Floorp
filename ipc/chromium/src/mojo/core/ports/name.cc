// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ports/name.h"
#include "chrome/common/ipc_message_utils.h"

namespace mojo {
namespace core {
namespace ports {

std::ostream& operator<<(std::ostream& stream, const Name& name) {
  std::ios::fmtflags flags(stream.flags());
  stream << std::hex << std::uppercase << name.v1;
  if (name.v2 != 0) {
    stream << '.' << name.v2;
  }
  stream.flags(flags);
  return stream;
}

mozilla::Logger& operator<<(mozilla::Logger& log, const Name& name) {
  log.printf("%" PRIX64, name.v1);
  if (name.v2 != 0) {
    log.printf(".%" PRIX64, name.v2);
  }
  return log;
}

}  // namespace ports
}  // namespace core
}  // namespace mojo

void IPC::ParamTraits<mojo::core::ports::PortName>::Write(
    MessageWriter* aWriter, const paramType& aParam) {
  WriteParam(aWriter, aParam.v1);
  WriteParam(aWriter, aParam.v2);
}

bool IPC::ParamTraits<mojo::core::ports::PortName>::Read(MessageReader* aReader,
                                                         paramType* aResult) {
  return ReadParam(aReader, &aResult->v1) && ReadParam(aReader, &aResult->v2);
}

void IPC::ParamTraits<mojo::core::ports::NodeName>::Write(
    MessageWriter* aWriter, const paramType& aParam) {
  WriteParam(aWriter, aParam.v1);
  WriteParam(aWriter, aParam.v2);
}

bool IPC::ParamTraits<mojo::core::ports::NodeName>::Read(MessageReader* aReader,
                                                         paramType* aResult) {
  return ReadParam(aReader, &aResult->v1) && ReadParam(aReader, &aResult->v2);
}
