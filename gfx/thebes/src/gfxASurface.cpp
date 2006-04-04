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
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
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

#include <stdio.h>

#include "gfxASurface.h"

#include "gfxImageSurface.h"

#ifdef CAIRO_HAS_WIN32_SURFACE
#include "gfxWindowsSurface.h"
#endif

#ifdef CAIRO_HAS_XLIB_SURFACE
#include "gfxXlibSurface.h"
#endif

#ifdef CAIRO_HAS_QUARTZGL_SURFACE
#include "gfxQuartzSurface.h"
#endif

static cairo_user_data_key_t gfxasurface_pointer_key;

gfxASurface*
gfxASurface::GetSurfaceWrapper(cairo_surface_t *csurf)
{
    return (gfxASurface*) cairo_surface_get_user_data(csurf, &gfxasurface_pointer_key);
}

void
gfxASurface::SetSurfaceWrapper(cairo_surface_t *csurf, gfxASurface *asurf)
{
    cairo_surface_set_user_data(csurf, &gfxasurface_pointer_key, asurf, NULL);
}

already_AddRefed<gfxASurface>
gfxASurface::Wrap (cairo_surface_t *csurf)
{
    gfxASurface *result;

    /* Do we already have a wrapper for this surface? */
    result = GetSurfaceWrapper(csurf);
    if (result) {
        // fprintf(stderr, "Using existing surface for %p\n", result);
        NS_ADDREF(result);
        return result;
    }

    /* No wrapper; figure out the surface type and create it */
    cairo_surface_type_t stype = cairo_surface_get_type(csurf);

    if (stype == CAIRO_SURFACE_TYPE_IMAGE) {
        result = new gfxImageSurface(csurf);
    }
#ifdef CAIRO_HAS_WIN32_SURFACE
    else if (stype == CAIRO_SURFACE_TYPE_WIN32) {
        result = new gfxWindowsSurface(csurf);
    }
#endif
#ifdef CAIRO_HAS_XLIB_SURFACE
    else if (stype == CAIRO_SURFACE_TYPE_XLIB) {
        result = new gfxXlibSurface(csurf);
    }
#endif
#ifdef CAIRO_HAS_QUARTZGL_SURFACE
    else if (stype == CAIRO_SURFACE_TYPE_QUARTZ2) {
        result = new gfxQuartzSurface(csurf);
    }
#endif
    else {
        result = new gfxUnknownSurface(csurf);
    }

    //fprintf(stderr, "New surface for %p\n", result);

    if (result) {
        NS_ADDREF(result);
        SetSurfaceWrapper(csurf, result);
    }

    return result;
}

void
gfxASurface::Init(cairo_surface_t* surface, PRBool existingSurface)
{
    if (existingSurface) {
        // old surface, ::Wrap will stuff the pointer in, but we need to retain mSurface
        cairo_surface_reference(surface);
    } else {
        // we're initializing from a new surface, so stuff our pointer here
        SetSurfaceWrapper(surface, this);
    }

    mDestroyed = PR_FALSE;
    mSurface = surface;
}

void
gfxASurface::Destroy()
{
    if (mDestroyed) {
        NS_WARNING("Calling Destroy on an already-destroyed surface!");
        return;
    }

    cairo_surface_set_user_data(mSurface, &gfxasurface_pointer_key, NULL, NULL);
    cairo_surface_destroy(mSurface);
    mDestroyed = PR_TRUE;
}
