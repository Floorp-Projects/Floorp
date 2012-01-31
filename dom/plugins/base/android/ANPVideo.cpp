/* The Original Code is Android NPAPI support code
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   James Willcox <jwillcox@mozilla.com>
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

#include <android/log.h>
#include "ANPBase.h"
#include "AndroidMediaLayer.h"
#include "nsIPluginInstanceOwner.h"
#include "nsPluginInstanceOwner.h"
#include "nsNPAPIPluginInstance.h"
#include "gfxRect.h"

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#define ASSIGN(obj, name)   (obj)->name = anp_video_##name

using namespace mozilla;

static AndroidMediaLayer* GetLayerForInstance(NPP instance) {
  nsNPAPIPluginInstance* pinst = static_cast<nsNPAPIPluginInstance*>(instance->ndata);

  nsPluginInstanceOwner* owner;
  if (NS_FAILED(pinst->GetOwner((nsIPluginInstanceOwner**)&owner))) {
    return NULL;
  }

  return owner->Layer();
}

static void Invalidate(NPP instance) {
  nsNPAPIPluginInstance* pinst = static_cast<nsNPAPIPluginInstance*>(instance->ndata);

  nsPluginInstanceOwner* owner;
  if (NS_FAILED(pinst->GetOwner((nsIPluginInstanceOwner**)&owner)))
    return;

  owner->Invalidate();
}

static ANPNativeWindow anp_video_acquireNativeWindow(NPP instance) {
  AndroidMediaLayer* layer = GetLayerForInstance(instance);
  if (!layer)
    return NULL;

  return layer->RequestNativeWindowForVideo();
}

static void anp_video_setWindowDimensions(NPP instance, const ANPNativeWindow window,
        const ANPRectF* dimensions) {
  AndroidMediaLayer* layer = GetLayerForInstance(instance);
  if (!layer)
    return;

  gfxRect rect(dimensions->left, dimensions->top,
               dimensions->right - dimensions->left,
               dimensions->bottom - dimensions->top);

  layer->SetNativeWindowDimensions(window, rect);
  Invalidate(instance);
}


static void anp_video_releaseNativeWindow(NPP instance, ANPNativeWindow window) {
  AndroidMediaLayer* layer = GetLayerForInstance(instance);
  if (!layer)
    return;

  layer->ReleaseNativeWindowForVideo(window);
  Invalidate(instance);
}

static void anp_video_setFramerateCallback(NPP instance, const ANPNativeWindow window, ANPVideoFrameCallbackProc callback) {
  // Bug 722682
  NOT_IMPLEMENTED();
}

///////////////////////////////////////////////////////////////////////////////

void InitVideoInterfaceV0(ANPVideoInterfaceV0* i) {
    ASSIGN(i, acquireNativeWindow);
    ASSIGN(i, setWindowDimensions);
    ASSIGN(i, releaseNativeWindow);
}

void InitVideoInterfaceV1(ANPVideoInterfaceV1* i) {
    ASSIGN(i, acquireNativeWindow);
    ASSIGN(i, setWindowDimensions);
    ASSIGN(i, releaseNativeWindow);
    ASSIGN(i, setFramerateCallback);
}
