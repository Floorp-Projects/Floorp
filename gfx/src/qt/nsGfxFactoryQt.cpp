/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Christopher Blizzard.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
 *   John C. Griggs <johng@corel.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsCOMPtr.h"
#include "nsGfxCIID.h"

#include "nsBlender.h"
#include "nsFontMetricsQt.h"
#include "nsRenderingContextQt.h"
#include "nsDeviceContextSpecQt.h"
#include "nsDeviceContextSpecFactoryQt.h"
#include "nsScreenManagerQt.h"
#include "nsScriptableRegion.h"
#include "nsDeviceContextQt.h"
#include "nsImageQt.h"
#include "nsFontList.h"
#include "nsPrintSession.h"
#include "nsNativeThemeQt.h"
#include "gfxImageFrame.h"

#include "qtlog.h"

// Initialize qt logging
PRLogModuleInfo *gQtLogModule = PR_NewLogModule("QtGfx");

// objects that just require generic constructors

NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontMetricsQt)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextQt)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRenderingContextQt)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImageQt)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBlender)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRegionQt)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecQt)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecFactoryQt)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontEnumeratorQt)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontList)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScreenManagerQt)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintSession, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(gfxImageFrame)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNativeThemeQt)

// our custom constructors
static nsresult nsScriptableRegionConstructor(nsISupports *aOuter,REFNSIID aIID,void **aResult)
{
    nsresult rv;

    nsIScriptableRegion *inst = 0;

    if (NULL == aResult) {
        rv = NS_ERROR_NULL_POINTER;
        return rv;
    }
    *aResult = NULL;
    if (NULL != aOuter) {
        rv = NS_ERROR_NO_AGGREGATION;
        return rv;
    }
    // create an nsRegionQt and get the scriptable region from it
    nsCOMPtr <nsIRegion> rgn;
    NS_NEWXPCOM(rgn, nsRegionQt);
    if (rgn != nsnull) {
        nsCOMPtr<nsIScriptableRegion> scriptableRgn = new nsScriptableRegion(rgn);
        inst = scriptableRgn;
    }
    if (NULL == inst) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        return rv;
    }
    NS_ADDREF(inst);
    rv = inst->QueryInterface(aIID, aResult);
    NS_RELEASE(inst);

    return rv;
}

static const nsModuleComponentInfo components[] =
{
    { "Qt Font Metrics",
      NS_FONT_METRICS_CID,
      "@mozilla.org/gfx/fontmetrics;1",
      nsFontMetricsQtConstructor },
    { "Qt Device Context",
      NS_DEVICE_CONTEXT_CID,
      "@mozilla.org/gfx/devicecontext;1",
      nsDeviceContextQtConstructor },
    { "Qt Rendering Context",
      NS_RENDERING_CONTEXT_CID,
      "@mozilla.org/gfx/renderingcontext;1",
      nsRenderingContextQtConstructor },
    { "Qt Image",
      NS_IMAGE_CID,
      "@mozilla.org/gfx/image;1",
      nsImageQtConstructor },
    { "Qt Region",
      NS_REGION_CID,
      "@mozilla.org/gfx/region/qt;1",
      nsRegionQtConstructor },
    { "Scriptable Region",
      NS_SCRIPTABLE_REGION_CID,
      "@mozilla.org/gfx/region;1",
      nsScriptableRegionConstructor },
    { "Blender",
      NS_BLENDER_CID,
      "@mozilla.org/gfx/blender;1",
      nsBlenderConstructor },
    { "Qt Device Context Spec",
      NS_DEVICE_CONTEXT_SPEC_CID,
      "@mozilla.org/gfx/devicecontextspec;1",
      nsDeviceContextSpecQtConstructor },
    { "Qt Device Context Spec Factory",
      NS_DEVICE_CONTEXT_SPEC_FACTORY_CID,
      "@mozilla.org/gfx/devicecontextspecfactory;1",
      nsDeviceContextSpecFactoryQtConstructor },
    { "Qt Font Enumerator",
      NS_FONT_ENUMERATOR_CID,
      "@mozilla.org/gfx/fontenumerator;1",
      nsFontEnumeratorQtConstructor },
    { "Font List",
      NS_FONTLIST_CID,
      //    "@mozilla.org/gfx/fontlist;1"
      NS_FONTLIST_CONTRACTID,
      nsFontListConstructor },
    { "Qt Screen Manager",
      NS_SCREENMANAGER_CID,
      "@mozilla.org/gfx/screenmanager;1",
      nsScreenManagerQtConstructor },
    { "shared image frame",
      GFX_IMAGEFRAME_CID,
      "@mozilla.org/gfx/image/frame;2",
      gfxImageFrameConstructor, },
    { "Native Theme Renderer",
      NS_THEMERENDERER_CID,
      "@mozilla.org/chrome/chrome-native-theme;1",
      nsNativeThemeQtConstructor },
    { "Print Session",
      NS_PRINTSESSION_CID,
      "@mozilla.org/gfx/printsession;1",
      nsPrintSessionConstructor }
};

NS_IMPL_NSGETMODULE(nsGfxQtModule, components)

