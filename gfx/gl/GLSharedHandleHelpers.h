/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLSHAREDHANDLEHELPERS_H_
#define GLSHAREDHANDLEHELPERS_H_

#include "GLContextTypes.h"
#include "mozilla/gfx/Types.h"

namespace mozilla {
namespace gl {

/**
  * Create a new shared GLContext content handle, using the passed buffer as a source.
  * Must be released by ReleaseSharedHandle.
  */
SharedTextureHandle CreateSharedHandle(GLContext* gl,
                                       SharedTextureShareType shareType,
                                       void* buffer,
                                       SharedTextureBufferType bufferType);

/**
  * - It is better to call ReleaseSharedHandle before original GLContext destroyed,
  *     otherwise warning will be thrown on attempt to destroy Texture associated with SharedHandle, depends on backend implementation.
  * - It does not require to be called on context where it was created,
  *     because SharedHandle suppose to keep Context reference internally,
  *     or don't require specific context at all, for example IPC SharedHandle.
  * - Not recommended to call this between AttachSharedHandle and Draw Target call.
  *      if it is really required for some special backend, then DetachSharedHandle API must be added with related implementation.
  * - It is recommended to stop any possible access to SharedHandle (Attachments, pending GL calls) before calling Release,
  *      otherwise some artifacts might appear or even crash if API backend implementation does not expect that.
  * SharedHandle (currently EGLImage) does not require GLContext because it is EGL call, and can be destroyed
  *   at any time, unless EGLImage have siblings (which are not expected with current API).
  */
void ReleaseSharedHandle(GLContext* gl,
                         SharedTextureShareType shareType,
                         SharedTextureHandle sharedHandle);


typedef struct {
    GLenum mTarget;
    gfx::SurfaceFormat mTextureFormat;
    gfx3DMatrix mTextureTransform;
} SharedHandleDetails;

/**
  * Returns information necessary for rendering a shared handle.
  * These values change depending on what sharing mechanism is in use
  */
bool GetSharedHandleDetails(GLContext* gl,
                            SharedTextureShareType shareType,
                            SharedTextureHandle sharedHandle,
                            SharedHandleDetails& details);
/**
  * Attach Shared GL Handle to GL_TEXTURE_2D target
  * GLContext must be current before this call
  */
bool AttachSharedHandle(GLContext* gl,
                        SharedTextureShareType shareType,
                        SharedTextureHandle sharedHandle);

/**
  * Detach Shared GL Handle from GL_TEXTURE_2D target
  */
void DetachSharedHandle(GLContext* gl,
                        SharedTextureShareType shareType,
                        SharedTextureHandle sharedHandle);

}
}

#endif
