/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is mozilla.org.
 * Portions created by the Initial Developer are
 * Copyright (C) 2004 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Stuart Parmenter <pavlov@pavlov.net>
 *    Vladimir Vukicevic <vladimir@pobox.com>
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
 */

#include "nsIWidget.h"

#include "nsCairoSurfaceManager.h"
#include "nsCairoDrawingSurface.h"

nsCairoSurfaceManager::nsCairoSurfaceManager()
{
}

nsCairoSurfaceManager::~nsCairoSurfaceManager()
{
    FlushCachedSurfaces();
}

NS_IMETHODIMP
nsCairoSurfaceManager::FlushCachedSurfaces()
{
    for (WidgetToSurfaceMap::iterator iter = mSurfaceMap.begin();
         iter != mSurfaceMap.end();
         iter++)
    {
        nsCairoDrawingSurface *surf = (iter->second);
        NS_RELEASE(surf);
    }

    mSurfaceMap.clear();

    return NS_OK;
}

NS_IMETHODIMP
nsCairoSurfaceManager::GetDrawingSurfaceForWidget (nsCairoDeviceContext *aDC,
                                                   nsIWidget *aWidget,
                                                   nsIDrawingSurface **aSurface)
{
    nsNativeWidget nativeWidget = aWidget->GetNativeData(NS_NATIVE_WIDGET);

    return GetDrawingSurfaceForNativeWidget (aDC, nativeWidget, aSurface);
}

NS_IMETHODIMP
nsCairoSurfaceManager::GetDrawingSurfaceForNativeWidget (nsCairoDeviceContext *aDC,
                                                         nsNativeWidget aWidget,
                                                         nsIDrawingSurface **aSurface)
{
    nsCairoDrawingSurface *surf = nsnull;
    WidgetToSurfaceMap::iterator iter = mSurfaceMap.find(aWidget);

    /* cached? */
    if (iter == mSurfaceMap.end()) {
        /* nope. */
        surf = new nsCairoDrawingSurface();
        NS_ADDREF(surf);         // our own reference for map
        surf->Init (aDC, aWidget);

        mSurfaceMap.insert(WidgetToSurfaceMap::value_type (aWidget, surf));
    } else {
        surf = (iter->second);
    }

    *aSurface = surf;
    NS_ADDREF(*aSurface);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoSurfaceManager::ReleaseSurface (nsIDrawingSurface *aSurface)
{
    nsCairoDrawingSurface *cds = NS_STATIC_CAST(nsCairoDrawingSurface*, aSurface);
    nsNativeWidget nw = cds->GetNativeWidget();

    WidgetToSurfaceMap::iterator iter = mSurfaceMap.find(nw);
    if (iter == mSurfaceMap.end()) {
        // ?!
        return NS_OK;
    }

    PRUint32 refcnt;
    NS_RELEASE2(cds, refcnt);

    if (refcnt == 0) {
        mSurfaceMap.erase (iter);
    }
}
