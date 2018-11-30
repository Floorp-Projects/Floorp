/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_SYNC_H_
#define WEBGL_SYNC_H_

#include "mozilla/LinkedList.h"
#include "nsWrapperCache.h"
#include "WebGLObjectModel.h"

namespace mozilla {
namespace webgl {
class AvailabilityRunnable;
}  // namespace webgl

class WebGLSync final : public nsWrapperCache,
                        public WebGLRefCountedObject<WebGLSync>,
                        public LinkedListElement<WebGLSync> {
  friend class WebGL2Context;
  friend class webgl::AvailabilityRunnable;

  const GLsync mGLName;
  const uint64_t mFenceId;
  bool mCanBeAvailable = false;

 public:
  WebGLSync(WebGLContext* webgl, GLenum condition, GLbitfield flags);

  void Delete();
  WebGLContext* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> givenProto) override;

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLSync)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLSync)

  void MarkSignaled() const;

 private:
  ~WebGLSync();
};

}  // namespace mozilla

#endif  // WEBGL_SYNC_H_
