/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_TEXTUREPOOLOGL_H
#define GFX_TEXTUREPOOLOGL_H

#include "GLContextTypes.h"             // for GLContext, GLuint

namespace mozilla {
namespace gl {

// A texture pool for for the on-screen GLContext. The main purpose of this class
// is to provide the ability to easily allocate an on-screen texture from the
// content thread. The unfortunate nature of the SurfaceTexture API (see AndroidSurfaceTexture)
// necessitates this.
class TexturePoolOGL
{
public:
  // Get a new texture from the pool. Will block
  // and wait for one to be created if necessary
  static GLuint AcquireTexture();

  // Called by the active LayerManagerOGL to fill
  // the pool
  static void Fill(GLContext* aContext);

  // Initializes the pool, but does not fill it. Called by gfxPlatform init.
  static void Init();

  // Clears all internal data structures in preparation for shutdown
  static void Shutdown();
};

} // gl
} // mozilla

#endif // GFX_TEXTUREPOOLOGL_H
