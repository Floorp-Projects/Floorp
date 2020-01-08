/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLErrorQueue.h"
#include "ClientWebGLContext.h"

namespace mozilla {

ClientWebGLErrorSink::ClientWebGLErrorSink(UniquePtr<Consumer>&& aConsumer)
    : CommandSink(std::move(aConsumer)) {}

void ClientWebGLErrorSink::SetClientWebGLContext(
    RefPtr<ClientWebGLContext>& aClientContext) {
  mClientContext = aClientContext;
}

bool ClientWebGLErrorSink::DispatchCommand(WebGLErrorCommand command) {
  if (!mClientContext) {
    return false;
  }

#define WEBGL_ERROR_HANDLER(_cmd, _method)            \
  case _cmd:                                          \
    return DispatchAsyncMethod(*mClientContext.get(), \
                               &ClientWebGLContext::_method);

  switch (command) {
    WEBGL_ERROR_HANDLER(OnLostContext, OnLostContext)
    WEBGL_ERROR_HANDLER(OnRestoredContext, OnRestoredContext)
    WEBGL_ERROR_HANDLER(CreationError, PostContextCreationError)
    WEBGL_ERROR_HANDLER(Warning, PostWarning)
    default:
      return false;
  }
}

}  // namespace mozilla
