/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLCHILD_H_
#define WEBGLCHILD_H_

#include <string>

#include "mozilla/dom/PWebGLChild.h"

namespace mozilla {

class ClientWebGLContext;

namespace dom {

class WebGLChild final : public PWebGLChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebGLChild, override);

  ClientWebGLContext& mContext;

  explicit WebGLChild(ClientWebGLContext&);

 private:
  virtual ~WebGLChild();

 public:
  mozilla::ipc::IPCResult RecvJsWarning(const std::string&) const;
  mozilla::ipc::IPCResult RecvOnContextLoss(webgl::ContextLossReason) const;
};

}  // namespace dom
}  // namespace mozilla

#endif  // WEBGLCHILD_H_
