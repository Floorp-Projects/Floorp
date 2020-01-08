/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLERRORQUEUE_H_
#define WEBGLERRORQUEUE_H_

#include "WebGLCommandQueue.h"

namespace mozilla {

class ClientWebGLContext;

using mozilla::webgl::PcqStatus;

enum WebGLErrorCommand {
  CreationError,
  OnLostContext,
  OnRestoredContext,
  Warning
};

/**
 * This is the source for the WebGL error and warning queue.  When an
 * asynchronous error or warning occurs in the host, such as the GL context
 * being lost or a diagnostic message being printed, it is inserted into this
 * queue to be asynchronously retrieved by the client in the content process,
 * where it is communicated to the relevant DOM objects.
 */
using HostWebGLErrorSource = CommandSource<WebGLErrorCommand>;

/**
 * This is the sink for the WebGL error and warning queue.
 * Before this sink can handle events, it needs to have its ClientWebGLContext
 * set.
 */
class ClientWebGLErrorSink : public CommandSink<WebGLErrorCommand> {
 public:
  ClientWebGLErrorSink(UniquePtr<Consumer>&& aConsumer);

  void SetClientWebGLContext(RefPtr<ClientWebGLContext>& aClientContext);

 protected:
  bool DispatchCommand(WebGLErrorCommand command) override;

  RefPtr<ClientWebGLContext> mClientContext;
};

/**
 * Factory for the WebGL error and warning queue.
 */
using WebGLErrorQueue =
    CommandQueue<HostWebGLErrorSource, ClientWebGLErrorSink>;

}  // namespace mozilla

#endif  // WEBGLERRORQUEUE_H_
