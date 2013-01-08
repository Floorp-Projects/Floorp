/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  This file is the Gtk2 implementation of plugin native window.
 */

#include "nsDebug.h"
#include "nsPluginNativeWindow.h"
#include "nsNPAPIPlugin.h"
#include "npapi.h"
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>

#include "gtk2xtbin.h"
#include "mozilla/X11Util.h"

class nsPluginNativeWindowGtk2 : public nsPluginNativeWindow {
public: 
  nsPluginNativeWindowGtk2();
  virtual ~nsPluginNativeWindowGtk2();

  virtual nsresult CallSetWindow(nsRefPtr<nsNPAPIPluginInstance> &aPluginInstance);
private:
  void SetWindow(XID aWindow)
  {
    window = reinterpret_cast<void*>(static_cast<uintptr_t>(aWindow));
  }
  XID GetWindow() const
  {
    return static_cast<XID>(reinterpret_cast<uintptr_t>(window));
  }

  NPSetWindowCallbackStruct mWsInfo;
  /**
   * Either a GtkSocket or a special GtkXtBin widget (derived from GtkSocket)
   * that encapsulates the Xt toolkit within a Gtk Application.
   */
  GtkWidget* mSocketWidget;
  nsresult  CreateXEmbedWindow(bool aEnableXtFocus);
  nsresult  CreateXtWindow();
  void      SetAllocation();
};

static gboolean plug_removed_cb   (GtkWidget *widget, gpointer data);
static void socket_unrealize_cb   (GtkWidget *widget, gpointer data);

nsPluginNativeWindowGtk2::nsPluginNativeWindowGtk2() : nsPluginNativeWindow()
{
  // initialize the struct fields
  window = nullptr; 
  x = 0; 
  y = 0; 
  width = 0; 
  height = 0; 
  memset(&clipRect, 0, sizeof(clipRect));
  ws_info = &mWsInfo;
  type = NPWindowTypeWindow;
  mSocketWidget = 0;
  mWsInfo.type = 0;
  mWsInfo.display = nullptr;
  mWsInfo.visual = nullptr;
  mWsInfo.colormap = 0;
  mWsInfo.depth = 0;
}

nsPluginNativeWindowGtk2::~nsPluginNativeWindowGtk2() 
{
  if(mSocketWidget) {
    gtk_widget_destroy(mSocketWidget);
  }
}

nsresult PLUG_NewPluginNativeWindow(nsPluginNativeWindow ** aPluginNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aPluginNativeWindow);
  *aPluginNativeWindow = new nsPluginNativeWindowGtk2();
  return *aPluginNativeWindow ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult PLUG_DeletePluginNativeWindow(nsPluginNativeWindow * aPluginNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aPluginNativeWindow);
  nsPluginNativeWindowGtk2 *p = (nsPluginNativeWindowGtk2 *)aPluginNativeWindow;
  delete p;
  return NS_OK;
}

nsresult nsPluginNativeWindowGtk2::CallSetWindow(nsRefPtr<nsNPAPIPluginInstance> &aPluginInstance)
{
  if (aPluginInstance) {
    if (type == NPWindowTypeWindow) {
      if (!mSocketWidget) {
        nsresult rv;

        // The documentation on the types for many variables in NP(N|P)_GetValue
        // is vague.  Often boolean values are NPBool (1 byte), but
        // https://developer.mozilla.org/en/XEmbed_Extension_for_Mozilla_Plugins
        // treats NPPVpluginNeedsXEmbed as PRBool (int), and
        // on x86/32-bit, flash stores to this using |movl 0x1,&needsXEmbed|.
        // thus we can't use NPBool for needsXEmbed, or the three bytes above
        // it on the stack would get clobbered. so protect with the larger bool.
        int needsXEmbed = 0;
        rv = aPluginInstance->GetValueFromPlugin(NPPVpluginNeedsXEmbed, &needsXEmbed);
        // If the call returned an error code make sure we still use our default value.
        if (NS_FAILED(rv)) {
          needsXEmbed = 0;
        }
#ifdef DEBUG
        printf("nsPluginNativeWindowGtk2: NPPVpluginNeedsXEmbed=%d\n", needsXEmbed);
#endif

        bool isOOPPlugin = aPluginInstance->GetPlugin()->GetLibrary()->IsOOP();
        if (needsXEmbed || isOOPPlugin) {        
          bool enableXtFocus = !needsXEmbed;
          rv = CreateXEmbedWindow(enableXtFocus);
        }
        else {
          rv = CreateXtWindow();
        }

        if (NS_FAILED(rv)) {
          return NS_ERROR_FAILURE;
        }
      }

      if (!mSocketWidget) {
        return NS_ERROR_FAILURE;
      }

      // Make sure to resize and re-place the window if required.
      SetAllocation();
      // Need to reset "window" each time as nsObjectFrame::DidReflow sets it
      // to the ancestor window.
      if (GTK_IS_XTBIN(mSocketWidget)) {
        // Point the NPWindow structures window to the actual X window
        SetWindow(GTK_XTBIN(mSocketWidget)->xtwindow);
      }
      else { // XEmbed or OOP&Xt
        SetWindow(gtk_socket_get_id(GTK_SOCKET(mSocketWidget)));
      }
#ifdef DEBUG
      printf("nsPluginNativeWindowGtk2: call SetWindow with xid=%p\n", (void *)window);
#endif
    } // NPWindowTypeWindow
    aPluginInstance->SetWindow(this);
  }
  else if (mPluginInstance)
    mPluginInstance->SetWindow(nullptr);

  SetPluginInstance(aPluginInstance);
  return NS_OK;
}

nsresult nsPluginNativeWindowGtk2::CreateXEmbedWindow(bool aEnableXtFocus) {
  NS_ASSERTION(!mSocketWidget,"Already created a socket widget!");
  GdkDisplay *display = gdk_display_get_default();
  GdkWindow *parent_win = gdk_x11_window_lookup_for_display(display, GetWindow());
  mSocketWidget = gtk_socket_new();

  //attach the socket to the container widget
  gtk_widget_set_parent_window(mSocketWidget, parent_win);

  // enable/disable focus event handlers,
  // see plugin_window_filter_func() for details
  g_object_set_data(G_OBJECT(mSocketWidget), "enable-xt-focus", (void *)aEnableXtFocus);

  // Make sure to handle the plug_removed signal.  If we don't the
  // socket will automatically be destroyed when the plug is
  // removed, which means we're destroying it more than once.
  // SYNTAX ERROR.
  g_signal_connect(mSocketWidget, "plug_removed",
                   G_CALLBACK(plug_removed_cb), NULL);

  g_signal_connect(mSocketWidget, "unrealize",
                   G_CALLBACK(socket_unrealize_cb), NULL);

  g_signal_connect(mSocketWidget, "destroy",
                   G_CALLBACK(gtk_widget_destroyed), &mSocketWidget);

  gpointer user_data = NULL;
  gdk_window_get_user_data(parent_win, &user_data);

  GtkContainer *container = GTK_CONTAINER(user_data);
  gtk_container_add(container, mSocketWidget);
  gtk_widget_realize(mSocketWidget);

  // The GtkSocket has a visible window, but the plugin's XEmbed plug will
  // cover this window.  Normally GtkSockets let the X server paint their
  // background and this would happen immediately (before the plug is
  // created).  Setting the background to None prevents the server from
  // painting this window, avoiding flicker.
  // TODO GTK3
#if (MOZ_WIDGET_GTK == 2)
  gdk_window_set_back_pixmap(gtk_widget_get_window(mSocketWidget), NULL, FALSE);
#endif

  // Resize before we show
  SetAllocation();

  gtk_widget_show(mSocketWidget);

  gdk_flush();
  SetWindow(gtk_socket_get_id(GTK_SOCKET(mSocketWidget)));

  // Fill out the ws_info structure.
  // (The windowless case is done in nsObjectFrame.cpp.)
  GdkWindow *gdkWindow = gdk_window_lookup(GetWindow());
  if(!gdkWindow)
    return NS_ERROR_FAILURE;

  mWsInfo.display = GDK_WINDOW_XDISPLAY(gdkWindow);
#if (MOZ_WIDGET_GTK == 2)
  mWsInfo.colormap = GDK_COLORMAP_XCOLORMAP(gdk_drawable_get_colormap(gdkWindow));
  GdkVisual* gdkVisual = gdk_drawable_get_visual(gdkWindow);
  mWsInfo.depth = gdkVisual->depth;
#else
  mWsInfo.colormap = None;
  GdkVisual* gdkVisual = gdk_window_get_visual(gdkWindow);
  mWsInfo.depth = gdk_visual_get_depth(gdkVisual);
#endif
  mWsInfo.visual = GDK_VISUAL_XVISUAL(gdkVisual);
    
  return NS_OK;
}

void nsPluginNativeWindowGtk2::SetAllocation() {
  if (!mSocketWidget)
    return;

  GtkAllocation new_allocation;
  new_allocation.x = 0;
  new_allocation.y = 0;
  new_allocation.width = width;
  new_allocation.height = height;
  gtk_widget_size_allocate(mSocketWidget, &new_allocation);
}

nsresult nsPluginNativeWindowGtk2::CreateXtWindow() {
  NS_ASSERTION(!mSocketWidget,"Already created a socket widget!");

#ifdef DEBUG      
  printf("About to create new xtbin of %i X %i from %p...\n",
         width, height, (void*)window);
#endif
  GdkDisplay *display = gdk_display_get_default();
  GdkWindow *gdkWindow = gdk_x11_window_lookup_for_display(display, GetWindow());
  mSocketWidget = gtk_xtbin_new(gdkWindow, 0);
  // Check to see if creating the xtbin failed for some reason.
  // if it did, we can't go any further.
  if (!mSocketWidget)
    return NS_ERROR_FAILURE;

  g_signal_connect(mSocketWidget, "destroy",
                   G_CALLBACK(gtk_widget_destroyed), &mSocketWidget);

  gtk_widget_set_size_request(mSocketWidget, width, height);

#ifdef DEBUG
  printf("About to show xtbin(%p)...\n", (void*)mSocketWidget); fflush(NULL);
#endif
  gtk_widget_show(mSocketWidget);
#ifdef DEBUG
  printf("completed gtk_widget_show(%p)\n", (void*)mSocketWidget); fflush(NULL);
#endif

  // Fill out the ws_info structure.
  GtkXtBin* xtbin = GTK_XTBIN(mSocketWidget);
  // The xtbin has its own Display structure.
  mWsInfo.display = xtbin->xtdisplay;
  mWsInfo.colormap = xtbin->xtclient.xtcolormap;
  mWsInfo.visual = xtbin->xtclient.xtvisual;
  mWsInfo.depth = xtbin->xtclient.xtdepth;
  // Leave mWsInfo.type = 0 - Who knows what this is meant to be?

  XFlush(mWsInfo.display);

  return NS_OK;
}

/* static */
gboolean
plug_removed_cb (GtkWidget *widget, gpointer data)
{
  // Gee, thanks for the info!
  return TRUE;
}

static void
socket_unrealize_cb(GtkWidget *widget, gpointer data)
{
  // Unmap and reparent any child windows that GDK does not yet know about.
  // (See bug 540114 comment 10.)
  GdkWindow* socket_window =  gtk_widget_get_window(widget);
  GdkDisplay* gdkDisplay = gdk_display_get_default();
  Display* display = GDK_DISPLAY_XDISPLAY(gdkDisplay);

  // Ignore X errors that may happen if windows get destroyed (possibly
  // requested by the plugin) between XQueryTree and when we operate on them.
  gdk_error_trap_push();

  Window root, parent;
  Window* children;
  unsigned int nchildren;
  if (!XQueryTree(display, gdk_x11_window_get_xid(socket_window),
                  &root, &parent, &children, &nchildren))
    return;

  for (unsigned int i = 0; i < nchildren; ++i) {
    Window child = children[i];
    if (!gdk_x11_window_lookup_for_display(gdkDisplay, child)) {
      // This window is not known to GDK.
      XUnmapWindow(display, child);
      XReparentWindow(display, child, DefaultRootWindow(display), 0, 0);
    }
  }

  if (children) XFree(children);

  mozilla::FinishX(display);
  gdk_error_trap_pop();
}
