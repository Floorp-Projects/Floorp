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
using ClientWebGLCommandSourceP =
    SyncCommandSource<size_t, mozilla::webgl::PcqProducer,
                      mozilla::webgl::ProducerConsumerQueue>;

/**
 * The sink for the WebGL Command Queue.  This object is created in the client
 * and sent to the host, where it needs to be given a HostWebGLContext that it
 * then uses for executing methods.  Add new commands to DispatchCommand using
 * the WEBGL_SYNC_COMMAND and WEBGL_ASYNC_COMMAND macros.
 */
template <typename Consumer, typename Queue>
class HostWebGLCommandSink final
    : public SyncCommandSink<size_t, Consumer, Queue> {
 public:
  HostWebGLCommandSink(UniquePtr<Consumer>&& aConsumer,
                       UniquePtr<typename Queue::Producer>&& aResponseProducer)
      : SyncCommandSink<size_t, Consumer, Queue>(
            std::move(aConsumer), std::move(aResponseProducer)) {}

  HostWebGLContext* mHostContext = nullptr;

 protected:
  // For IPDL:
  friend struct mozilla::ipc::IPDLParamTraits<HostWebGLCommandSink>;
  friend class mozilla::layers::PCompositorBridgeParent;
  HostWebGLCommandSink() = default;

  bool DispatchCommand(size_t command) override;
};

using HostWebGLCommandSinkP =
    HostWebGLCommandSink<mozilla::webgl::PcqConsumer,
                         mozilla::webgl::ProducerConsumerQueue>;

namespace ipc {

template <typename Consumer, typename Queue>
struct IPDLParamTraits<mozilla::HostWebGLCommandSink<Consumer, Queue>>
    : public IPDLParamTraits<
          mozilla::SyncCommandSink<size_t, Consumer, Queue>> {
 public:
  typedef mozilla::HostWebGLCommandSink<Consumer, Queue> paramType;
};

}  // namespace ipc

}  // namespace mozilla

#endif  // WEBGLCROSSPROCESSCOMMANDQUEUE_H_
