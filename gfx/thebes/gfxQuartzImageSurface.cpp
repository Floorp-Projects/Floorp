/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include "gfxQuartzImageSurface.h"

#include "cairo-quartz.h"
#include "cairo-quartz-image.h"

gfxQuartzImageSurface::gfxQuartzImageSurface(gfxImageSurface *imageSurface)
{
    if (imageSurface->CairoSurface() == NULL)
        return;

    cairo_surface_t *surf = cairo_quartz_image_surface_create (imageSurface->CairoSurface());
    Init (surf);
}

gfxQuartzImageSurface::gfxQuartzImageSurface(cairo_surface_t *csurf)
{
    Init (csurf, PR_TRUE);
}

gfxQuartzImageSurface::~gfxQuartzImageSurface()
{
}

PRInt32
gfxQuartzImageSurface::KnownMemoryUsed()
{
  // This surface doesn't own any memory itself, but we want to report here the
  // amount of memory that the surface it wraps uses.
  nsRefPtr<gfxImageSurface> imgSurface = GetAsImageSurface();
  if (imgSurface)
    return imgSurface->KnownMemoryUsed();
  return 0;
}

already_AddRefed<gfxImageSurface>
gfxQuartzImageSurface::GetAsImageSurface()
{
    if (!mSurfaceValid)
        return nsnull;

    cairo_surface_t *isurf = cairo_quartz_image_surface_get_image (CairoSurface());
    if (!isurf) {
        NS_WARNING ("Couldn't obtain an image surface from a QuartzImageSurface?!");
        return nsnull;
    }

    nsRefPtr<gfxASurface> asurf = gfxASurface::Wrap(isurf);
    gfxImageSurface *imgsurf = (gfxImageSurface*) asurf.get();
    NS_ADDREF(imgsurf);
    return imgsurf;
}
