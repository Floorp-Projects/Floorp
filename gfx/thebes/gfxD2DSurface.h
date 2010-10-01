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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
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

#ifndef GFX_D2DSURFACE_H
#define GFX_D2DSURFACE_H

#include "gfxASurface.h"
#include "nsPoint.h"
#include "nsRect.h"

#include <windows.h>

struct ID3D10Texture2D;

class THEBES_API gfxD2DSurface : public gfxASurface {
public:

    gfxD2DSurface(HWND wnd,
                  gfxContentType aContent);

    gfxD2DSurface(const gfxIntSize& size,
                  gfxImageFormat imageFormat = ImageFormatRGB24);

    gfxD2DSurface(HANDLE handle, gfxContentType aContent);

    gfxD2DSurface(ID3D10Texture2D *texture, gfxContentType aContent);

    gfxD2DSurface(cairo_surface_t *csurf);

    virtual ~gfxD2DSurface();

    virtual TextQuality GetTextQualityInTransparentSurfaces()
    {
      // D2D always draws text in transparent surfaces with grayscale-AA,
      // even if the text is over opaque pixels.
      return TEXT_QUALITY_BAD;
    }

    void Present();
    void Scroll(const nsIntPoint &aDelta, const nsIntRect &aClip);

    HDC GetDC(PRBool aRetainContents);
    void ReleaseDC(const nsIntRect *aUpdatedRect);
};

#endif /* GFX_D2DSURFACE_H */
