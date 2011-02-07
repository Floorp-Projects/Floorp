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
 * The Original Code is Thebes code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <robert@ocallahan.org>
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

#include "gfxTeeSurface.h"

/* Once cairo in tree is update ensure we remove the ifdef
    and just include cairo-tee.h
*/
#ifdef MOZ_TREE_CAIRO
#include "cairo.h"
#else
#include "cairo-tee.h"
#endif

gfxTeeSurface::gfxTeeSurface(cairo_surface_t *csurf)
{
    Init(csurf, PR_TRUE);
}

gfxTeeSurface::gfxTeeSurface(gfxASurface **aSurfaces, PRInt32 aSurfaceCount)
{
    NS_ASSERTION(aSurfaceCount > 0, "Must have a least one surface");
    cairo_surface_t *csurf = cairo_tee_surface_create(aSurfaces[0]->CairoSurface());
    Init(csurf, PR_FALSE);

    for (PRInt32 i = 1; i < aSurfaceCount; ++i) {
        cairo_tee_surface_add(csurf, aSurfaces[i]->CairoSurface());
    }
}

const gfxIntSize
gfxTeeSurface::GetSize() const
{
    nsRefPtr<gfxASurface> master = Wrap(cairo_tee_surface_index(mSurface, 0));
    return master->GetSize();
}

void
gfxTeeSurface::GetSurfaces(nsTArray<nsRefPtr<gfxASurface> >* aSurfaces)
{
    for (PRInt32 i = 0; ; ++i) {
        cairo_surface_t *csurf = cairo_tee_surface_index(mSurface, i);
        if (cairo_surface_status(csurf))
            break;
        nsRefPtr<gfxASurface> *elem = aSurfaces->AppendElement();
        if (!elem)
            return;
        *elem = Wrap(csurf);
    }
}
