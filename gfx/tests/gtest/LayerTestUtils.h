/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#ifndef TEST_GFX_TEXTURES
#define TEST_GFX_TEXTURES

#include "CompositorTypes.h"

class gfxImageSurface;
class nsIWidget;

namespace mozilla {
namespace gfx {
  class DataSourceSurface;
}

namespace layers {

class PlanarYCbCrData;
class Compositor;

Compositor* CreateCompositor(LayersBackend aBakend, nsIWidget* aWidget);

/// Fills the surface with values between 0 and 100.
void SetupSurface(gfxImageSurface* surface);

/// Fills the surface with values between 0 and 100.
void SetupSurface(gfx::DataSourceSurface* surface);

/// Returns true if two surfaces contain the same data
void AssertSurfacesEqual(gfx::DataSourceSurface* surface1,
                         gfx::DataSourceSurface* surface2);

/// Returns true if two surfaces contain the same data
void AssertSurfacesEqual(gfxImageSurface* surface1,
                         gfxImageSurface* surface2);

/// Same as above, for YCbCr surfaces
void AssertYCbCrSurfacesEqual(PlanarYCbCrData* surface1,
                              PlanarYCbCrData* surface2);


} // layers
} // mozilla

#endif
