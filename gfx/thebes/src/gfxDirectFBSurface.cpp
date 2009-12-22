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

#include "gfxDirectFBSurface.h"

#include "cairo-directfb.h"

gfxDirectFBSurface::gfxDirectFBSurface(IDirectFB *dfb, IDirectFBSurface *dfbs)
    : mDFB(nsnull), mDFBSurface(nsnull)
{
    dfb->AddRef( dfb );
    dfbs->AddRef( dfbs );

    cairo_surface_t *surf = cairo_directfb_surface_create(dfb, dfbs);

    mDFB = dfb;
    mDFBSurface = dfbs;

    Init(surf);
}

gfxDirectFBSurface::gfxDirectFBSurface(IDirectFBSurface *dfbs)
    : mDFB(nsnull), mDFBSurface(nsnull)
{
    DFBResult ret;

    dfbs->AddRef( dfbs );

    /* Lightweight, getting singleton */
    ret = DirectFBCreate( &mDFB );
    if (ret) {
         D_DERROR( (DirectResult) ret, "gfxDirectFBSurface: DirectFBCreate() failed!\n" );
         return;
    }

    cairo_surface_t *surf = cairo_directfb_surface_create(mDFB, dfbs);

    mDFBSurface = dfbs;

    Init(surf);
}

gfxDirectFBSurface::gfxDirectFBSurface(cairo_surface_t *csurf)
{
    mDFB = nsnull;
    mDFBSurface = nsnull;

    Init(csurf, PR_TRUE);
}

gfxDirectFBSurface::gfxDirectFBSurface(const gfxIntSize& size, gfxImageFormat format) :
    mDFB(nsnull), mDFBSurface(nsnull)
{
     DFBResult             ret;
     DFBSurfaceDescription desc;

     if (!CheckSurfaceSize(size) || size.width <= 0 || size.height <= 0)
          return;

     /* Lightweight, getting singleton */
     ret = DirectFBCreate( &mDFB );
     if (ret) {
          D_DERROR( (DirectResult) ret, "gfxDirectFBSurface: DirectFBCreate() failed!\n" );
          return;
     }

     desc.flags  = (DFBSurfaceDescriptionFlags)( DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT );
     desc.width  = size.width;
     desc.height = size.height;

     switch (format) {
          case gfxASurface::ImageFormatARGB32:
               desc.pixelformat = DSPF_ARGB;
               break;

          case gfxASurface::ImageFormatRGB24:
               desc.pixelformat = DSPF_RGB32;
               break;

          case gfxASurface::ImageFormatA8:
               desc.pixelformat = DSPF_A8;
               break;

          case gfxASurface::ImageFormatA1:
               desc.pixelformat = DSPF_A1;
               break;

          default:
               D_BUG( "unknown format" );
               return;
     }

     ret = mDFB->CreateSurface( mDFB, &desc, &mDFBSurface );
     if (ret) {
          D_DERROR( (DirectResult) ret, "gfxDirectFBSurface: "
                                        "IDirectFB::CreateSurface( %dx%d ) failed!\n", desc.width, desc.height );
          return;
     }

     cairo_surface_t *surface = cairo_directfb_surface_create(mDFB, mDFBSurface);

     Init(surface);
}

gfxDirectFBSurface::~gfxDirectFBSurface()
{
     if (mDFBSurface)
          mDFBSurface->Release( mDFBSurface );

     if (mDFB)
          mDFB->Release( mDFB );
}

