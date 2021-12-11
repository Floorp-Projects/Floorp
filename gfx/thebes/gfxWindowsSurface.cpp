/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxWindowsSurface.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/HelpersCairo.h"
#include "mozilla/gfx/Logging.h"

#include "cairo.h"
#include "cairo-win32.h"

#include "nsString.h"

gfxWindowsSurface::gfxWindowsSurface(HDC dc, uint32_t flags)
    : mOwnsDC(false), mDC(dc), mWnd(nullptr) {
  InitWithDC(flags);
}

void gfxWindowsSurface::MakeInvalid(mozilla::gfx::IntSize& size) {
  size = mozilla::gfx::IntSize(-1, -1);
}

gfxWindowsSurface::gfxWindowsSurface(const mozilla::gfx::IntSize& realSize,
                                     gfxImageFormat imageFormat)
    : mOwnsDC(false), mWnd(nullptr) {
  mozilla::gfx::IntSize size(realSize);
  if (!mozilla::gfx::Factory::CheckSurfaceSize(size)) MakeInvalid(size);

  cairo_format_t cformat = GfxFormatToCairoFormat(imageFormat);
  cairo_surface_t* surf =
      cairo_win32_surface_create_with_dib(cformat, size.width, size.height);

  Init(surf);

  if (CairoStatus() == CAIRO_STATUS_SUCCESS) {
    mDC = cairo_win32_surface_get_dc(CairoSurface());
    RecordMemoryUsed(size.width * size.height * 4 + sizeof(gfxWindowsSurface));
  } else {
    mDC = nullptr;
  }
}

gfxWindowsSurface::gfxWindowsSurface(cairo_surface_t* csurf)
    : mOwnsDC(false), mWnd(nullptr) {
  if (cairo_surface_status(csurf) == 0)
    mDC = cairo_win32_surface_get_dc(csurf);
  else
    mDC = nullptr;

  MOZ_ASSERT(cairo_surface_get_type(csurf) !=
             CAIRO_SURFACE_TYPE_WIN32_PRINTING);

  Init(csurf, true);
}

void gfxWindowsSurface::InitWithDC(uint32_t flags) {
  if (flags & FLAG_IS_TRANSPARENT) {
    Init(cairo_win32_surface_create_with_format(mDC, CAIRO_FORMAT_ARGB32));
  } else {
    Init(cairo_win32_surface_create(mDC));
  }
}

gfxWindowsSurface::~gfxWindowsSurface() {
  if (mOwnsDC) {
    if (mWnd)
      ::ReleaseDC(mWnd, mDC);
    else
      ::DeleteDC(mDC);
  }
}

HDC gfxWindowsSurface::GetDC() {
  return cairo_win32_surface_get_dc(CairoSurface());
}

already_AddRefed<gfxImageSurface> gfxWindowsSurface::GetAsImageSurface() {
  if (!mSurfaceValid) {
    NS_WARNING(
        "GetImageSurface on an invalid (null) surface; who's calling this "
        "without checking for surface errors?");
    return nullptr;
  }

  NS_ASSERTION(
      CairoSurface() != nullptr,
      "CairoSurface() shouldn't be nullptr when mSurfaceValid is TRUE!");

  cairo_surface_t* isurf = cairo_win32_surface_get_image(CairoSurface());
  if (!isurf) return nullptr;

  RefPtr<gfxImageSurface> result =
      gfxASurface::Wrap(isurf).downcast<gfxImageSurface>();
  result->SetOpaqueRect(GetOpaqueRect());

  return result.forget();
}

const mozilla::gfx::IntSize gfxWindowsSurface::GetSize() const {
  if (!mSurfaceValid) {
    NS_WARNING(
        "GetImageSurface on an invalid (null) surface; who's calling this "
        "without checking for surface errors?");
    return mozilla::gfx::IntSize(-1, -1);
  }

  NS_ASSERTION(
      mSurface != nullptr,
      "CairoSurface() shouldn't be nullptr when mSurfaceValid is TRUE!");

  int width, height;
  if (cairo_win32_surface_get_size(mSurface, &width, &height) !=
      CAIRO_STATUS_SUCCESS) {
    return mozilla::gfx::IntSize(-1, -1);
  }

  return mozilla::gfx::IntSize(width, height);
}
