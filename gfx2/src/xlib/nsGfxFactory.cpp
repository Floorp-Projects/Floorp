/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsIGenericFactory.h"
#include "nsIModule.h"

#include "nsCursor.h"
#include "nsPixmap.h"
#include "nsImage.h"
#include "nsRegion.h"

#include "nsChildWindow.h"
#include "nsPopupWindow.h"
#include "nsTopLevelWindow.h"


#include "nsRunAppRun.h"

// objects that just require generic constructors

NS_GENERIC_FACTORY_CONSTRUCTOR(nsCursor)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPixmap)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImage)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRegion)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsChildWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPopupWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTopLevelWindow)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsRunAppRun)

static nsModuleComponentInfo components[] =
{
  { "xlib cursor",
    NS_CURSOR_CID,
    "@mozilla.org/gfx/cursor;2",
    nsCursorConstructor, },
  { "xlib pixmap",
    NS_PIXMAP_CID,
    "@mozilla.org/gfx/pixmap;2",
    nsPixmapConstructor, },
  { "xlib image",
    NS_IMAGE_CID,
    "@mozilla.org/gfx/image;2",
    nsImageConstructor, },
  { "xlib region",
    NS_REGION_CID,
    "@mozilla.org/gfx/region;2",
    nsRegionConstructor, },
  { "xlib window",
    NS_CHILDWINDOW_CID,
    "@mozilla.org/gfx/window/child;2",
    nsChildWindowConstructor, },
  { "xlib window",
    NS_POPUPWINDOW_CID,
    "@mozilla.org/gfx/window/popup;2",
    nsPopupWindowConstructor, },
  { "xlib window",
    NS_TOPLEVELWINDOW_CID,
    "@mozilla.org/gfx/window/toplevel;2",
    nsTopLevelWindowConstructor, },
  { "xlib run app run",
    NS_RUNAPPRUN_CID,
    "@mozilla.org/gfx/run;2",
    nsRunAppRunConstructor, }
};


// @mozilla/gfx/systemlook;2

NS_IMPL_NSGETMODULE("nsGfx2Module", components)

