/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Matt Woodrow <mwoodrow@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "ImageLayerOGL.h"
#include "MacIOSurfaceImageOGL.h"
#include <AppKit/NSOpenGL.h>
#include "OpenGL/OpenGL.h"

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

MacIOSurfaceImageOGL::MacIOSurfaceImageOGL(LayerManagerOGL *aManager)
  : MacIOSurfaceImage(nsnull), mSize(0, 0)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread to create a cairo image");

  if (aManager) {
    // Allocate texture now to grab a reference to the GLContext
    GLContext *gl = aManager->glForResources();
    mTexture.Allocate(gl);
    gl->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, mTexture.GetTextureID());
    gl->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB, 
                       LOCAL_GL_TEXTURE_MIN_FILTER, 
                       LOCAL_GL_NEAREST);
    gl->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB, 
                       LOCAL_GL_TEXTURE_MAG_FILTER, 
                       LOCAL_GL_NEAREST);
    gl->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, 0);
  }
}

void
MacIOSurfaceImageOGL::SetData(const MacIOSurfaceImage::Data &aData)
{
  mIOSurface = nsIOSurface::LookupSurface(aData.mIOSurface->GetIOSurfaceID());
  mSize = gfxIntSize(mIOSurface->GetWidth(), mIOSurface->GetHeight());
  
  GLContext *gl = mTexture.GetGLContext();
  gl->MakeCurrent();
    
  gl->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, mTexture.GetTextureID());

  void *nativeCtx = gl->GetNativeData(GLContext::NativeGLContext);
  NSOpenGLContext* nsCtx = (NSOpenGLContext*)nativeCtx;

  mIOSurface->CGLTexImageIOSurface2D((CGLContextObj)[nsCtx CGLContextObj],
                                     LOCAL_GL_RGBA, LOCAL_GL_BGRA,
                                     LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV, 0);

  gl->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, 0);
}

} /* layers */
} /* mozilla */