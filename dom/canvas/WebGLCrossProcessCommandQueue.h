/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLCROSSPROCESSCOMMANDQUEUE_H_
#define WEBGLCROSSPROCESSCOMMANDQUEUE_H_

#include "mozilla/dom/WebGLCommandQueue.h"

namespace mozilla {

namespace layers {
class PCompositorBridgeParent;
}

class HostWebGLContext;

using mozilla::webgl::ProducerConsumerQueue;

/**
 * The source for the WebGL Command Queue.
 */
using ClientWebGLCommandSource = SyncCommandSource<size_t>;

/**
 * The sink for the WebGL Command Queue.  This object is created in the client
 * and sent to the host, where it needs to be given a HostWebGLContext that it
 * then uses for executing methods.  Add new commands to DispatchCommand using
 * the WEBGL_SYNC_COMMAND and WEBGL_ASYNC_COMMAND macros.
 */
class HostWebGLCommandSink : public SyncCommandSink<size_t> {
 public:
  HostWebGLCommandSink(UniquePtr<Consumer>&& aConsumer,
                       UniquePtr<Producer>&& aResponseProducer)
      : SyncCommandSink(std::move(aConsumer), std::move(aResponseProducer)) {}

  HostWebGLContext* mHostContext = nullptr;

 protected:
  // For IPDL:
  friend struct mozilla::ipc::IPDLParamTraits<HostWebGLCommandSink>;
  friend class mozilla::layers::PCompositorBridgeParent;
  HostWebGLCommandSink() {}

  bool DispatchCommand(size_t command) override;
};

namespace ipc {

template <>
struct IPDLParamTraits<mozilla::HostWebGLCommandSink>
    : public IPDLParamTraits<mozilla::SyncCommandSink<size_t>> {
 public:
  typedef mozilla::HostWebGLCommandSink paramType;
};

}  // namespace ipc

}  // namespace mozilla

#endif  // WEBGLCROSSPROCESSCOMMANDQUEUE_H_
