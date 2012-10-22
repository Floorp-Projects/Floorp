/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"

using namespace mozilla;

JSObject*
WebGLFramebuffer::WrapObject(JSContext *cx, JSObject *scope, bool *triedToWrap) {
    return dom::WebGLFramebufferBinding::Wrap(cx, scope, this, triedToWrap);
}
NS_IMPL_CYCLE_COLLECTION_CLASS(WebGLFramebuffer)
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(WebGLFramebuffer)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WebGLFramebuffer)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mColorAttachment.mTexturePtr)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mColorAttachment.mRenderbufferPtr)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDepthAttachment.mTexturePtr)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDepthAttachment.mRenderbufferPtr)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mStencilAttachment.mTexturePtr)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mStencilAttachment.mRenderbufferPtr)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDepthStencilAttachment.mTexturePtr)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDepthStencilAttachment.mRenderbufferPtr)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WebGLFramebuffer)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mColorAttachment.mTexturePtr)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mColorAttachment.mRenderbufferPtr)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDepthAttachment.mTexturePtr)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDepthAttachment.mRenderbufferPtr)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mStencilAttachment.mTexturePtr)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mStencilAttachment.mRenderbufferPtr)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDepthStencilAttachment.mTexturePtr)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDepthStencilAttachment.mRenderbufferPtr)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebGLFramebuffer)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebGLFramebuffer)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebGLFramebuffer)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
