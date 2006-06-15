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

#include "gfxPlatform.h"

#if defined(XP_WIN)
#include "gfxWindowsPlatform.h"
#elif defined(XP_MACOSX)
#include "gfxPlatformMac.h"
#elif defined(MOZ_WIDGET_GTK2)
#include "gfxPlatformGtk.h"
#elif defined(XP_BEOS)
#include "gfxBeOSPlatform.h"
#endif

#include "gfxContext.h"
#include "gfxImageSurface.h"

#ifdef MOZ_ENABLE_GLITZ
#include <stdlib.h>
#endif

gfxPlatform *gPlatform = nsnull;
int gGlitzState = -1;

gfxPlatform*
gfxPlatform::GetPlatform()
{
    if (!gPlatform) {
#if defined(XP_WIN)
        gPlatform = new gfxWindowsPlatform;
#elif defined(XP_MACOSX)
        gPlatform = new gfxPlatformMac;
#elif defined(MOZ_WIDGET_GTK2)
        gPlatform = new gfxPlatformGtk;
#elif defined(XP_BEOS)
        gPlatform = new gfxBeOSPlatform;
#endif
    }

    return gPlatform;
}

PRBool
gfxPlatform::UseGlitz()
{
#ifdef MOZ_ENABLE_GLITZ
    if (gGlitzState == -1) {
        if (getenv("MOZ_GLITZ"))
            gGlitzState = 1;
        else
            gGlitzState = 0;
    }

    if (gGlitzState)
        return PR_TRUE;
#endif

    return PR_FALSE;
}

void
gfxPlatform::SetUseGlitz(PRBool use)
{
    gGlitzState = (use ? 1 : 0);
}

already_AddRefed<gfxASurface>
gfxPlatform::OptimizeImage(gfxImageSurface* aSurface)
{
    nsRefPtr<gfxASurface> optSurface;
    optSurface = CreateOffscreenSurface(aSurface->Width(),
                                        aSurface->Height(),
                                        aSurface->Format());

    if (!optSurface)
        return nsnull;

    nsRefPtr<gfxContext> tmpCtx(new gfxContext(optSurface));
    tmpCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
    tmpCtx->SetSource(aSurface);
    tmpCtx->Paint();

    gfxASurface *ret = optSurface;
    NS_ADDREF(ret);
    return ret;
}

nsresult
gfxPlatform::GetFontList(const nsACString& aLangGroup,
                         const nsACString& aGenericFamily,
                         nsStringArray& aListOfFonts)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
gfxPlatform::UpdateFontList()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
