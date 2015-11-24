/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLTimerQuery.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "nsContentUtils.h"
#include "WebGLContext.h"

namespace mozilla {

JSObject*
WebGLTimerQuery::WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto)
{
  return dom::WebGLTimerQueryEXTBinding::Wrap(cx, this, givenProto);
}

WebGLTimerQuery::WebGLTimerQuery(WebGLContext* webgl, GLuint name)
  : WebGLContextBoundObject(webgl)
  , mGLName(name)
  , mTarget(LOCAL_GL_NONE)
{
}

WebGLTimerQuery::~WebGLTimerQuery()
{
  DeleteOnce();
}

WebGLTimerQuery*
WebGLTimerQuery::Create(WebGLContext* webgl)
{
  GLuint name = 0;
  webgl->MakeContextCurrent();
  webgl->gl->fGenQueries(1, &name);
  return new WebGLTimerQuery(webgl, name);
}

void
WebGLTimerQuery::Delete()
{
  mContext->MakeContextCurrent();
  mContext->gl->fDeleteQueries(1, &mGLName);
}

WebGLContext*
WebGLTimerQuery::GetParentObject() const
{
  return mContext;
}


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLTimerQuery)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLTimerQuery, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLTimerQuery, Release)

} // namespace mozilla
