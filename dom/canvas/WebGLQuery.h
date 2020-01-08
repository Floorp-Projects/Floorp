/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_QUERY_H_
#define WEBGL_QUERY_H_

#include "mozilla/LinkedList.h"
#include "nsWrapperCache.h"

#include "WebGLObjectModel.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace webgl {
class AvailabilityRunnable;
}  // namespace webgl

class WebGLQuery final : public WebGLRefCountedObject<WebGLQuery>,
                         public LinkedListElement<WebGLQuery> {
  friend class webgl::AvailabilityRunnable;
  friend class WebGLRefCountedObject<WebGLQuery>;

 public:
  const GLuint mGLName;

 private:
  GLenum mTarget;
  WebGLRefPtr<WebGLQuery>* mActiveSlot;

  bool mCanBeAvailable = false;  // Track whether the event loop has spun

  ////
 public:
  GLenum Target() const { return mTarget; }
  bool IsActive() const { return bool(mActiveSlot); }

  ////

  NS_INLINE_DECL_REFCOUNTING(WebGLQuery)

  explicit WebGLQuery(WebGLContext* webgl);

 private:
  ~WebGLQuery() { DeleteOnce(); };

  // WebGLRefCountedObject
  void Delete();

 public:
  ////

  void BeginQuery(GLenum target, WebGLRefPtr<WebGLQuery>& slot);
  void DeleteQuery();
  void EndQuery();
  MaybeWebGLVariant GetQueryParameter(GLenum pname) const;
  bool IsQuery() const;
  void QueryCounter(GLenum target);
};

}  // namespace mozilla

#endif  // WEBGL_QUERY_H_
