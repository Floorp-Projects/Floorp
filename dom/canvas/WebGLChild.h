/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLCHILD_H_
#define WEBGLCHILD_H_

#include "mozilla/dom/PWebGLChild.h"
#include "nsWeakReference.h"

namespace mozilla {

class ClientWebGLContext;

namespace dom {

class WebGLChild : public PWebGLChild {
 public:
  mozilla::ipc::IPCResult RecvQueueFailed();
  mozilla::ClientWebGLContext* GetContext();

 protected:
  friend mozilla::ClientWebGLContext;
  void SetContext(mozilla::ClientWebGLContext* aContext);

  nsWeakPtr mContext;
};

}  // namespace dom
}  // namespace mozilla

#endif  // WEBGLCHILD_H_
