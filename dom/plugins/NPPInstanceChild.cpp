/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
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
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   Chris Jones <jones.chris.g@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "NPPInstanceChild.h"

#ifdef OS_LINUX
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include "gtk2xtbin.h"
#endif

namespace mozilla {
namespace plugins {
//-----------------------------------------------------------------------------

static const char*
NPNVariableToString(NPNVariable aVar)
{
#define VARSTR(v_)  case v_: return #v_

    switch(aVar) {
        VARSTR(NPNVxDisplay);
        VARSTR(NPNVxtAppContext);
        VARSTR(NPNVnetscapeWindow);
        VARSTR(NPNVjavascriptEnabledBool);
        VARSTR(NPNVasdEnabledBool);
        VARSTR(NPNVisOfflineBool);

        VARSTR(NPNVserviceManager);
        VARSTR(NPNVDOMElement);
        VARSTR(NPNVDOMWindow);
        VARSTR(NPNVToolkit);
        VARSTR(NPNVSupportsXEmbedBool);

        VARSTR(NPNVWindowNPObject);

        VARSTR(NPNVPluginElementNPObject);

        VARSTR(NPNVSupportsWindowless);

        VARSTR(NPNVprivateModeBool);

    default: return "???";
    }
#undef VARSTR
}

NPError
NPPInstanceChild::NPN_GetValue(NPNVariable aVar, void* aValue)
{
    printf ("[NPPInstanceChild] NPN_GetValue(%s)\n",
            NPNVariableToString(aVar));

    switch(aVar) {

    case NPNVSupportsWindowless:
        // FIXME/cjones report PR_TRUE here and use XComposite + child
        // surface to implement windowless plugins
        *((NPBool*)aValue) = PR_FALSE;
        return NPERR_NO_ERROR;

#if defined(OS_LINUX)
    case NPNVSupportsXEmbedBool:
        *((NPBool*)aValue) = PR_TRUE;
        return NPERR_NO_ERROR;

    case NPNVToolkit:
        *((NPNToolkitType*)aValue) = NPNVGtk2;
        return NPERR_NO_ERROR;

#elif defined(OS_WIN)
    case NPNVToolkit:
        return NPERR_GENERIC_ERROR;
#endif
    default:
        printf("  unhandled var %s\n", NPNVariableToString(aVar));
        return NPERR_GENERIC_ERROR;   
    }

}

nsresult
NPPInstanceChild::AnswerNPP_GetValue(const String& key, String* value)
{
    return NPERR_GENERIC_ERROR;
}

nsresult
NPPInstanceChild::AnswerNPP_SetWindow(const NPWindow& aWindow, NPError* rv)
{
    printf("[NPPInstanceChild] NPP_SetWindow(%lx, %d, %d)\n",
           reinterpret_cast<unsigned long>(aWindow.window),
           aWindow.width, aWindow.height);

#if defined(OS_LINUX)
    // XXX/cjones: the minimum info is sent over IPC to allow this
    // code to determine the rest.  this code is possibly wrong on
    // some systems, in some conditions

    GdkNativeWindow handle = (GdkNativeWindow) aWindow.window;
    GdkWindow* gdkWindow = gdk_window_lookup(handle);

    mWindow.window = (void*) handle;
    mWindow.width = aWindow.width;
    mWindow.height = aWindow.height;
    mWindow.type = NPWindowTypeWindow;

    mWsInfo.display = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());

    // FIXME/cjones: the code below is correct, but apparently to get
    // a valid GdkWindow*, one needs to create the gtk_plug.  but if we
    // do that, then the plugin can't plug in to plug.  a hacky solution
    // may be to create the plug, then create another socket within that
    // plug.  yuck.
#if 0
    mWsInfo.display = GDK_WINDOW_XDISPLAY(gdkWindow);
    mWsInfo.colormap =
        GDK_COLORMAP_XCOLORMAP(gdk_drawable_get_colormap(gdkWindow));
    GdkVisual* gdkVisual = gdk_drawable_get_visual(gdkWindow);
    mWsInfo.visual = GDK_VISUAL_XVISUAL(gdkVisual);
    mWsInfo.depth = gdkVisual->depth;
#endif

    mWindow.ws_info = (void*) &mWsInfo;

#elif defined(OS_WIN)
    mWindow.window = aWindow.window;
    mWindow.width = aWindow.width;
    mWindow.height = aWindow.height;
    mWindow.type = NPWindowTypeWindow;

#else
#  error Implement me for your OS
#endif

    *rv = mPluginIface->setwindow(&mData, &mWindow);
    return NS_OK;
}


} // namespace plugins
} // namespace mozilla
