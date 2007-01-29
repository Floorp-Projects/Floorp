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

#include "gfxASurface.h"

#include "gfxImageSurface.h"

#include "cairo.h"

#ifdef CAIRO_HAS_WIN32_SURFACE
#include "gfxWindowsSurface.h"
#endif

#ifdef CAIRO_HAS_XLIB_SURFACE
#include "gfxXlibSurface.h"
#endif

#ifdef CAIRO_HAS_NQUARTZ_SURFACE
#include "gfxQuartzSurface.h"
#endif

#include <stdio.h>

static cairo_user_data_key_t gfxasurface_pointer_key;

// Surfaces use refcounting that's tied to the cairo surface refcnt, to avoid
// refcount mismatch issues.
nsrefcnt
gfxASurface::AddRef(void)
{
    NS_PRECONDITION(mSurface != nsnull, "gfxASurface::AddRef without mSurface");

    if (mHasFloatingRef) {
        // eat the floating ref
        mHasFloatingRef = PR_FALSE;
    } else {
        cairo_surface_reference(mSurface);
    }

    return (nsrefcnt) cairo_surface_get_reference_count(mSurface);
}

nsrefcnt
gfxASurface::Release(void)
{
    NS_PRECONDITION(!mHasFloatingRef, "gfxASurface::Release while floating ref still outstanding!");
    NS_PRECONDITION(mSurface != nsnull, "gfxASurface::Release without mSurface");
    // Note that there is a destructor set on user data for mSurface,
    // which will delete this gfxASurface wrapper when the surface's refcount goes
    // out of scope.
    nsrefcnt refcnt = (nsrefcnt) cairo_surface_get_reference_count(mSurface);
    cairo_surface_destroy(mSurface);

    // |this| may not be valid any more, don't use it!

    return --refcnt;
}

void
gfxASurface::SurfaceDestroyFunc(void *data) {
    gfxASurface *surf = (gfxASurface*) data;
    // fprintf (stderr, "Deleting wrapper for %p (wrapper: %p)\n", surf->mSurface, data);
    delete surf;
}

gfxASurface*
gfxASurface::GetSurfaceWrapper(cairo_surface_t *csurf)
{
    return (gfxASurface*) cairo_surface_get_user_data(csurf, &gfxasurface_pointer_key);
}

void
gfxASurface::SetSurfaceWrapper(cairo_surface_t *csurf, gfxASurface *asurf)
{
    cairo_surface_set_user_data(csurf, &gfxasurface_pointer_key, asurf, SurfaceDestroyFunc);
}

already_AddRefed<gfxASurface>
gfxASurface::Wrap (cairo_surface_t *csurf)
{
    gfxASurface *result;

    /* Do we already have a wrapper for this surface? */
    result = GetSurfaceWrapper(csurf);
    if (result) {
        // fprintf(stderr, "Existing wrapper for %p -> %p\n", csurf, result);
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
#ifdef CAIRO_HAS_NQUARTZ_SURFACE
    else if (stype == CAIRO_SURFACE_TYPE_NQUARTZ) {
        result = new gfxQuartzSurface(csurf);
    }
#endif
    else {
        result = new gfxUnknownSurface(csurf);
    }

    // fprintf(stderr, "New wrapper for %p -> %p\n", csurf, result);

    NS_ADDREF(result);
    return result;
}

void
gfxASurface::Init(cairo_surface_t* surface, PRBool existingSurface)
{
    SetSurfaceWrapper(surface, this);

    mSurface = surface;

    if (existingSurface) {
        mHasFloatingRef = PR_FALSE;
    } else {
        mHasFloatingRef = PR_TRUE;
    }
}

gfxASurface::gfxSurfaceType
gfxASurface::GetType() const
{
    return (gfxSurfaceType)cairo_surface_get_type(mSurface);
}

gfxASurface::gfxContentType
gfxASurface::GetContentType() const
{
    return (gfxContentType)cairo_surface_get_content(mSurface);
}

void
gfxASurface::SetDeviceOffset(gfxPoint offset)
{
    cairo_surface_set_device_offset(mSurface,
                                    offset.x, offset.y);
}

gfxPoint
gfxASurface::GetDeviceOffset() const
{
    gfxPoint pt;
    cairo_surface_get_device_offset(mSurface, &pt.x, &pt.y);
    return pt;
}

void
gfxASurface::Flush()
{
    cairo_surface_flush(mSurface);
}

void
gfxASurface::MarkDirty()
{
    cairo_surface_mark_dirty(mSurface);
}

void
gfxASurface::MarkDirty(const gfxRect& r)
{
    cairo_surface_mark_dirty_rectangle(mSurface,
                                       (int) r.pos.x, (int) r.pos.y,
                                       (int) r.size.width, (int) r.size.height);
}


void
gfxASurface::SetData(const cairo_user_data_key_t *key,
                     void *user_data,
                     thebes_destroy_func_t destroy)
{
    cairo_surface_set_user_data(mSurface, key, user_data, destroy);
}

void *
gfxASurface::GetData(const cairo_user_data_key_t *key)
{
    return cairo_surface_get_user_data(mSurface, key);
}

void
gfxASurface::Finish()
{
    cairo_surface_finish(mSurface);
}
