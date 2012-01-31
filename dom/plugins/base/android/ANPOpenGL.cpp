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

#include <dlfcn.h>
#include <android/log.h>
#include "AndroidBridge.h"
#include "ANPBase.h"
#include "GLContextProvider.h"
#include "nsNPAPIPluginInstance.h"

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#define ASSIGN(obj, name)   (obj)->name = anp_opengl_##name

using namespace mozilla;
using namespace mozilla::gl;

static ANPEGLContext anp_opengl_acquireContext(NPP inst) {
    // Bug 687267
    NOT_IMPLEMENTED();
    return NULL;
}

static ANPTextureInfo anp_opengl_lockTexture(NPP instance) {
    ANPTextureInfo info = { 0, 0, 0, 0 };
    NOT_IMPLEMENTED();
    return info;
}

static void anp_opengl_releaseTexture(NPP instance, const ANPTextureInfo* info) {
    NOT_IMPLEMENTED();
}

static void anp_opengl_invertPluginContent(NPP instance, bool isContentInverted) {
    NOT_IMPLEMENTED();
}

///////////////////////////////////////////////////////////////////////////////

void InitOpenGLInterface(ANPOpenGLInterfaceV0* i) {
    ASSIGN(i, acquireContext);
    ASSIGN(i, lockTexture);
    ASSIGN(i, releaseTexture);
    ASSIGN(i, invertPluginContent);
}
