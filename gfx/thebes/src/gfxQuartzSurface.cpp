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
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#include "gfxQuartzSurface.h"

#include "cairo-quartz2.h"

gfxQuartzSurface::gfxQuartzSurface(gfxImageFormat format,
                                   int width, int height,
                                   PRBool y_grows_down)
    : mWidth(width), mHeight(height)
{
    mCGContext = nsnull;

    cairo_surface_t *surf = cairo_quartzgl_surface_create
        ((cairo_format_t) format, width, height, y_grows_down);

    Init(surf);
}

gfxQuartzSurface::gfxQuartzSurface(CGContextRef context,
                                   int width, int height,
                                   PRBool y_grows_down)
    : mCGContext(context), mWidth(width), mHeight(height)
{
    cairo_surface_t *surf = cairo_quartzgl_surface_create_for_cg_context
        (context, width, height, y_grows_down);
    //printf ("+++ gfxQuartzSurface[%p] %p %d %d -> %p\n", this, context, width, height, surf);
    Init(surf);
}

gfxQuartzSurface::gfxQuartzSurface(cairo_surface_t *csurf)
{
    mWidth = -1;
    mHeight = -1;
    mCGContext = nsnull;

    Init(csurf, PR_TRUE);
}

gfxQuartzSurface::~gfxQuartzSurface()
{
    //printf ("--- ~gfxQuartzSurface[%p] %p %p\n", this, CairoSurface(), mCGContext);
    Destroy();
}
