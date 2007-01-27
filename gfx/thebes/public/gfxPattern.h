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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
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

#ifndef GFX_PATTERN_H
#define GFX_PATTERN_H

#include "gfxTypes.h"

#include "gfxColor.h"
#include "gfxMatrix.h"

class gfxContext;
class gfxASurface;
typedef struct _cairo_pattern cairo_pattern_t;


class THEBES_API gfxPattern {
    THEBES_INLINE_DECL_REFCOUNTING(gfxPattern)

public:
    gfxPattern(cairo_pattern_t *aPattern);
    gfxPattern(gfxRGBA aColor);
    gfxPattern(gfxASurface *surface); // from another surface
    // linear
    gfxPattern(gfxFloat x0, gfxFloat y0, gfxFloat x1, gfxFloat y1); // linear
    gfxPattern(gfxFloat cx0, gfxFloat cy0, gfxFloat radius0,
               gfxFloat cx1, gfxFloat cy1, gfxFloat radius1); // radial
    virtual ~gfxPattern();

    cairo_pattern_t *CairoPattern();
    void AddColorStop(gfxFloat offset, const gfxRGBA& c);
    void SetMatrix(const gfxMatrix& matrix);
    gfxMatrix CurrentMatrix() const;

    enum GraphicsExtend {
        EXTEND_NONE,
        EXTEND_REPEAT,
        EXTEND_REFLECT,
        EXTEND_PAD
    };

    // none, repeat, reflect
    void SetExtend(GraphicsExtend extend);
    GraphicsExtend Extend() const;

    void SetFilter(int filter);
    int Filter() const;

    /* returns TRUE if it succeeded */
    PRBool GetSolidColor(gfxRGBA& aColor);

protected:
    cairo_pattern_t *mPattern;
};

#endif /* GFX_PATTERN_H */
