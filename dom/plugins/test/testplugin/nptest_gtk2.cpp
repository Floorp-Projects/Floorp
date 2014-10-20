/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * 
 * Copyright (c) 2008, Mozilla Corporation
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of the Mozilla Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * Contributor(s):
 *   Josh Aas <josh@mozilla.com>
 *   Michael Ventnor <mventnor@mozilla.com>
 * 
 * ***** END LICENSE BLOCK ***** */

#include "nptest_platform.h"
#include "npapi.h"
#include <pthread.h>
#include <gdk/gdk.h>
#ifdef MOZ_X11
#include <gdk/gdkx.h>
#include <X11/extensions/shape.h>
#endif
#include <glib.h>
#include <gtk/gtk.h>
#include <unistd.h>

#include "mozilla/NullPtr.h"
#include "mozilla/IntentionalCrash.h"

 using namespace std;

struct _PlatformData {
#ifdef MOZ_X11
  Display* display;
  Visual* visual;
  Colormap colormap;
#endif
  GtkWidget* plug;
};

bool
pluginSupportsWindowMode()
{
  return true;
}

bool
pluginSupportsWindowlessMode()
{
  return true;
}

NPError
pluginInstanceInit(InstanceData* instanceData)
{
#ifdef MOZ_X11
  instanceData->platformData = static_cast<PlatformData*>
    (NPN_MemAlloc(sizeof(PlatformData)));
  if (!instanceData->platformData)
    return NPERR_OUT_OF_MEMORY_ERROR;

  instanceData->platformData->display = nullptr;
  instanceData->platformData->visual = nullptr;
  instanceData->platformData->colormap = None;  
  instanceData->platformData->plug = nullptr;

  return NPERR_NO_ERROR;
#else
  // we only support X11 here, since thats what the plugin system uses
  return NPERR_INCOMPATIBLE_VERSION_ERROR;
#endif
}

void
pluginInstanceShutdown(InstanceData* instanceData)
{
  if (instanceData->hasWidget) {
    Window window = reinterpret_cast<XID>(instanceData->window.window);

    if (window != None) {
      // This window XID should still be valid.
      // See bug 429604 and bug 454756.
      XWindowAttributes attributes;
      if (!XGetWindowAttributes(instanceData->platformData->display, window,
                                &attributes))
        g_error("XGetWindowAttributes failed at plugin instance shutdown");
    }
  }

  GtkWidget* plug = instanceData->platformData->plug;
  if (plug) {
    instanceData->platformData->plug = 0;
    if (instanceData->cleanupWidget) {
      // Default/tidy behavior
      gtk_widget_destroy(plug);
    } else {
      // Flash Player style: let the GtkPlug destroy itself on disconnect.
      g_signal_handlers_disconnect_matched(plug, G_SIGNAL_MATCH_DATA, 0, 0,
                                           nullptr, nullptr, instanceData);
    }
  }

  NPN_MemFree(instanceData->platformData);
  instanceData->platformData = 0;
}

static void 
SetCairoRGBA(cairo_t* cairoWindow, uint32_t rgba)
{
  float b = (rgba & 0xFF) / 255.0;
  float g = ((rgba & 0xFF00) >> 8) / 255.0;
  float r = ((rgba & 0xFF0000) >> 16) / 255.0;
  float a = ((rgba & 0xFF000000) >> 24) / 255.0;

  cairo_set_source_rgba(cairoWindow, r, g, b, a);
}

static void
pluginDrawSolid(InstanceData* instanceData, GdkDrawable* gdkWindow,
                int x, int y, int width, int height)
{
  cairo_t* cairoWindow = gdk_cairo_create(gdkWindow);

  if (!instanceData->hasWidget) {
    NPRect* clip = &instanceData->window.clipRect;
    cairo_rectangle(cairoWindow, clip->left, clip->top,
                    clip->right - clip->left, clip->bottom - clip->top);
    cairo_clip(cairoWindow);
  }

  GdkRectangle windowRect = { x, y, width, height };
  gdk_cairo_rectangle(cairoWindow, &windowRect);
  SetCairoRGBA(cairoWindow, instanceData->scriptableObject->drawColor);

  cairo_fill(cairoWindow);
  cairo_destroy(cairoWindow);
}

static void
pluginDrawWindow(InstanceData* instanceData, GdkDrawable* gdkWindow,
                 const GdkRectangle& invalidRect)
{
  NPWindow& window = instanceData->window;
  // When we have a widget, window.x/y are meaningless since our
  // widget is always positioned correctly and we just draw into it at 0,0
  int x = instanceData->hasWidget ? 0 : window.x;
  int y = instanceData->hasWidget ? 0 : window.y;
  int width = window.width;
  int height = window.height;
  
  notifyDidPaint(instanceData);

  if (instanceData->scriptableObject->drawMode == DM_SOLID_COLOR) {
    // drawing a solid color for reftests
    pluginDrawSolid(instanceData, gdkWindow,
                    invalidRect.x, invalidRect.y,
                    invalidRect.width, invalidRect.height);
    return;
  }

  NPP npp = instanceData->npp;
  if (!npp)
    return;

  const char* uaString = NPN_UserAgent(npp);
  if (!uaString)
    return;

  GdkGC* gdkContext = gdk_gc_new(gdkWindow);
  if (!gdkContext)
    return;

  if (!instanceData->hasWidget) {
    NPRect* clip = &window.clipRect;
    GdkRectangle gdkClip = { clip->left, clip->top, clip->right - clip->left,
                             clip->bottom - clip->top };
    gdk_gc_set_clip_rectangle(gdkContext, &gdkClip);
  }

  // draw a grey background for the plugin frame
  GdkColor grey;
  grey.red = grey.blue = grey.green = 32767;
  gdk_gc_set_rgb_fg_color(gdkContext, &grey);
  gdk_draw_rectangle(gdkWindow, gdkContext, TRUE, x, y, width, height);

  // draw a 3-pixel-thick black frame around the plugin
  GdkColor black;
  black.red = black.green = black.blue = 0;
  gdk_gc_set_rgb_fg_color(gdkContext, &black);
  gdk_gc_set_line_attributes(gdkContext, 3, GDK_LINE_SOLID, GDK_CAP_NOT_LAST, GDK_JOIN_MITER);
  gdk_draw_rectangle(gdkWindow, gdkContext, FALSE, x + 1, y + 1,
                     width - 3, height - 3);

  // paint the UA string
  PangoContext* pangoContext = gdk_pango_context_get();
  PangoLayout* pangoTextLayout = pango_layout_new(pangoContext);
  pango_layout_set_width(pangoTextLayout, (width - 10) * PANGO_SCALE);
  pango_layout_set_text(pangoTextLayout, uaString, -1);
  gdk_draw_layout(gdkWindow, gdkContext, x + 5, y + 5, pangoTextLayout);
  g_object_unref(pangoTextLayout);

  g_object_unref(gdkContext);
}

static gboolean
ExposeWidget(GtkWidget* widget, GdkEventExpose* event,
             gpointer user_data)
{
  InstanceData* instanceData = static_cast<InstanceData*>(user_data);
  pluginDrawWindow(instanceData, event->window, event->area);
  return TRUE;
}

static gboolean
MotionEvent(GtkWidget* widget, GdkEventMotion* event,
            gpointer user_data)
{
  InstanceData* instanceData = static_cast<InstanceData*>(user_data);
  instanceData->lastMouseX = event->x;
  instanceData->lastMouseY = event->y;
  return TRUE;
}

static gboolean
ButtonEvent(GtkWidget* widget, GdkEventButton* event,
            gpointer user_data)
{
  InstanceData* instanceData = static_cast<InstanceData*>(user_data);
  instanceData->lastMouseX = event->x;
  instanceData->lastMouseY = event->y;
  if (event->type == GDK_BUTTON_RELEASE) {
    instanceData->mouseUpEventCount++;
  }
  return TRUE;
}

static gboolean
DeleteWidget(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
  InstanceData* instanceData = static_cast<InstanceData*>(user_data);
  // Some plugins do not expect the plug to be removed from the socket before
  // the plugin instance is destroyed.  e.g. bug 485125
  if (instanceData->platformData->plug)
    g_error("plug removed"); // this aborts

  return FALSE;
}

void
pluginDoSetWindow(InstanceData* instanceData, NPWindow* newWindow)
{
  instanceData->window = *newWindow;

#ifdef MOZ_X11
  NPSetWindowCallbackStruct *ws_info =
    static_cast<NPSetWindowCallbackStruct*>(newWindow->ws_info);
  instanceData->platformData->display = ws_info->display;
  instanceData->platformData->visual = ws_info->visual;
  instanceData->platformData->colormap = ws_info->colormap;
#endif
}

void
pluginWidgetInit(InstanceData* instanceData, void* oldWindow)
{
#ifdef MOZ_X11
  GtkWidget* oldPlug = instanceData->platformData->plug;
  if (oldPlug) {
    instanceData->platformData->plug = 0;
    gtk_widget_destroy(oldPlug);
  }

  GdkNativeWindow nativeWinId =
    reinterpret_cast<XID>(instanceData->window.window);

  /* create a GtkPlug container */
  GtkWidget* plug = gtk_plug_new(nativeWinId);

  // Test for bugs 539138 and 561308
  if (!plug->window)
    g_error("Plug has no window"); // aborts

  /* make sure the widget is capable of receiving focus */
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET(plug), GTK_CAN_FOCUS);

  /* all the events that our widget wants to receive */
  gtk_widget_add_events(plug, GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK |
                              GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
  g_signal_connect(plug, "expose-event", G_CALLBACK(ExposeWidget),
                   instanceData);
  g_signal_connect(plug, "motion_notify_event", G_CALLBACK(MotionEvent),
                   instanceData);
  g_signal_connect(plug, "button_press_event", G_CALLBACK(ButtonEvent),
                   instanceData);
  g_signal_connect(plug, "button_release_event", G_CALLBACK(ButtonEvent),
                   instanceData);
  g_signal_connect(plug, "delete-event", G_CALLBACK(DeleteWidget),
                   instanceData);
  gtk_widget_show(plug);

  instanceData->platformData->plug = plug;
#endif
}

int16_t
pluginHandleEvent(InstanceData* instanceData, void* event)
{
#ifdef MOZ_X11
  XEvent* nsEvent = (XEvent*)event;

  switch (nsEvent->type) {
  case GraphicsExpose: {
    const XGraphicsExposeEvent& expose = nsEvent->xgraphicsexpose;
    NPWindow& window = instanceData->window;
    window.window = (void*)(expose.drawable);

    GdkNativeWindow nativeWinId = reinterpret_cast<XID>(window.window);

    GdkDisplay* gdkDisplay = gdk_x11_lookup_xdisplay(expose.display);
    if (!gdkDisplay) {
      g_warning("Display not opened by GDK");
      return 0;
    }
    // gdk_pixmap_foreign_new() doesn't check whether a GdkPixmap already
    // exists, so check first.
    // https://bugzilla.gnome.org/show_bug.cgi?id=590690
    GdkPixmap* gdkDrawable =
      GDK_DRAWABLE(gdk_pixmap_lookup_for_display(gdkDisplay, nativeWinId));
    // If there is no existing GdkPixmap or it doesn't have a colormap then
    // create our own.
    if (gdkDrawable) {
      GdkColormap* gdkColormap = gdk_drawable_get_colormap(gdkDrawable);
      if (!gdkColormap) {
        g_warning("No GdkColormap on GdkPixmap");
        return 0;
      }
      if (gdk_x11_colormap_get_xcolormap(gdkColormap)
          != instanceData->platformData->colormap) {
        g_warning("wrong Colormap");
        return 0;
      }
      if (gdk_x11_visual_get_xvisual(gdk_colormap_get_visual(gdkColormap))
          != instanceData->platformData->visual) {
        g_warning("wrong Visual");
        return 0;
      }
      g_object_ref(gdkDrawable);
    } else {
      gdkDrawable =
        GDK_DRAWABLE(gdk_pixmap_foreign_new_for_display(gdkDisplay,
                                                        nativeWinId));
      VisualID visualID = instanceData->platformData->visual->visualid;
      GdkVisual* gdkVisual =
        gdk_x11_screen_lookup_visual(gdk_drawable_get_screen(gdkDrawable),
                                     visualID);
      GdkColormap* gdkColormap =
        gdk_x11_colormap_foreign_new(gdkVisual,
                                     instanceData->platformData->colormap);
      gdk_drawable_set_colormap(gdkDrawable, gdkColormap);
      g_object_unref(gdkColormap);
    }

    const NPRect& clip = window.clipRect;
    if (expose.x < clip.left || expose.y < clip.top ||
        expose.x + expose.width > clip.right ||
        expose.y + expose.height > clip.bottom) {
      g_warning("expose rectangle (x=%d,y=%d,w=%d,h=%d) not in clip rectangle (l=%d,t=%d,r=%d,b=%d)",
                expose.x, expose.y, expose.width, expose.height,
                clip.left, clip.top, clip.right, clip.bottom);
      return 0;
    }
    if (expose.x < window.x || expose.y < window.y ||
        expose.x + expose.width > window.x + int32_t(window.width) ||
        expose.y + expose.height > window.y + int32_t(window.height)) {
      g_warning("expose rectangle (x=%d,y=%d,w=%d,h=%d) not in plugin rectangle (x=%d,y=%d,w=%d,h=%d)",
                expose.x, expose.y, expose.width, expose.height,
                window.x, window.y, window.width, window.height);
      return 0;
    }      

    GdkRectangle invalidRect =
      { expose.x, expose.y, expose.width, expose.height };
    pluginDrawWindow(instanceData, gdkDrawable, invalidRect);
    g_object_unref(gdkDrawable);
    break;
  }
  case MotionNotify: {
    XMotionEvent* motion = &nsEvent->xmotion;
    instanceData->lastMouseX = motion->x;
    instanceData->lastMouseY = motion->y;
    break;
  }
  case ButtonPress:
  case ButtonRelease: {
    XButtonEvent* button = &nsEvent->xbutton;
    instanceData->lastMouseX = button->x;
    instanceData->lastMouseY = button->y;
    if (nsEvent->type == ButtonRelease) {
      instanceData->mouseUpEventCount++;
    }
    break;
  }
  default:
    break;
  }
#endif

  return 0;
}

int32_t pluginGetEdge(InstanceData* instanceData, RectEdge edge)
{
  if (!instanceData->hasWidget)
    return NPTEST_INT32_ERROR;
  GtkWidget* plug = instanceData->platformData->plug;
  if (!plug)
    return NPTEST_INT32_ERROR;
  GdkWindow* plugWnd = plug->window;
  if (!plugWnd)
    return NPTEST_INT32_ERROR;

  GdkWindow* toplevelGdk = 0;
#ifdef MOZ_X11
  Window toplevel = 0;
  NPN_GetValue(instanceData->npp, NPNVnetscapeWindow, &toplevel);
  if (!toplevel)
    return NPTEST_INT32_ERROR;
  toplevelGdk = gdk_window_foreign_new(toplevel);
#endif
  if (!toplevelGdk)
    return NPTEST_INT32_ERROR;

  GdkRectangle toplevelFrameExtents;
  gdk_window_get_frame_extents(toplevelGdk, &toplevelFrameExtents);
  g_object_unref(toplevelGdk);

  gint pluginWidth, pluginHeight;
  gdk_drawable_get_size(GDK_DRAWABLE(plugWnd), &pluginWidth, &pluginHeight);
  gint pluginOriginX, pluginOriginY;
  gdk_window_get_origin(plugWnd, &pluginOriginX, &pluginOriginY);
  gint pluginX = pluginOriginX - toplevelFrameExtents.x;
  gint pluginY = pluginOriginY - toplevelFrameExtents.y;

  switch (edge) {
  case EDGE_LEFT:
    return pluginX;
  case EDGE_TOP:
    return pluginY;
  case EDGE_RIGHT:
    return pluginX + pluginWidth;
  case EDGE_BOTTOM:
    return pluginY + pluginHeight;
  }

  return NPTEST_INT32_ERROR;
}

#ifdef MOZ_X11
static void intersectWithShapeRects(Display* display, Window window,
                                    int kind, GdkRegion* region)
{
  int count = -1, order;
  XRectangle* shapeRects =
    XShapeGetRectangles(display, window, kind, &count, &order);
  // The documentation says that shapeRects will be nullptr when the
  // extension is not supported. Unfortunately XShapeGetRectangles
  // also returns nullptr when the region is empty, so we can't treat
  // nullptr as failure. I hope this way is OK.
  if (count < 0)
    return;

  GdkRegion* shapeRegion = gdk_region_new();
  if (!shapeRegion) {
    XFree(shapeRects);
    return;
  }

  for (int i = 0; i < count; ++i) {
    XRectangle* r = &shapeRects[i];
    GdkRectangle rect = { r->x, r->y, r->width, r->height };
    gdk_region_union_with_rect(shapeRegion, &rect);
  }
  XFree(shapeRects);

  gdk_region_intersect(region, shapeRegion);
  gdk_region_destroy(shapeRegion);
}
#endif

static GdkRegion* computeClipRegion(InstanceData* instanceData)
{
  if (!instanceData->hasWidget)
    return 0;

  GtkWidget* plug = instanceData->platformData->plug;
  if (!plug)
    return 0;
  GdkWindow* plugWnd = plug->window;
  if (!plugWnd)
    return 0;

  gint plugWidth, plugHeight;
  gdk_drawable_get_size(GDK_DRAWABLE(plugWnd), &plugWidth, &plugHeight);
  GdkRectangle pluginRect = { 0, 0, plugWidth, plugHeight };
  GdkRegion* region = gdk_region_rectangle(&pluginRect);
  if (!region)
    return 0;

  int pluginX = 0, pluginY = 0;

#ifdef MOZ_X11
  Display* display = GDK_WINDOW_XDISPLAY(plugWnd);
  Window window = GDK_WINDOW_XWINDOW(plugWnd);

  Window toplevel = 0;
  NPN_GetValue(instanceData->npp, NPNVnetscapeWindow, &toplevel);
  if (!toplevel)
    return 0;

  for (;;) {
    Window root;
    int x, y;
    unsigned int width, height, border_width, depth;
    if (!XGetGeometry(display, window, &root, &x, &y, &width, &height,
                      &border_width, &depth)) {
      gdk_region_destroy(region);
      return 0;
    }

    GdkRectangle windowRect = { 0, 0, static_cast<gint>(width),
                                static_cast<gint>(height) };
    GdkRegion* windowRgn = gdk_region_rectangle(&windowRect);
    if (!windowRgn) {
      gdk_region_destroy(region);
      return 0;
    }
    intersectWithShapeRects(display, window, ShapeBounding, windowRgn);
    intersectWithShapeRects(display, window, ShapeClip, windowRgn);
    gdk_region_offset(windowRgn, -pluginX, -pluginY);
    gdk_region_intersect(region, windowRgn);
    gdk_region_destroy(windowRgn);

    // Stop now if we've reached the toplevel. Stopping here means
    // clipping performed by the toplevel window is taken into account.
    if (window == toplevel)
      break;

    Window parent;
    Window* children;
    unsigned int nchildren;
    if (!XQueryTree(display, window, &root, &parent, &children, &nchildren)) {
      gdk_region_destroy(region);
      return 0;
    }
    XFree(children);

    pluginX += x;
    pluginY += y;

    window = parent;
  }
#endif
  // pluginX and pluginY are now relative to the toplevel. Make them
  // relative to the window frame top-left.
  GdkWindow* toplevelGdk = gdk_window_foreign_new(window);
  if (!toplevelGdk)
    return 0;
  GdkRectangle toplevelFrameExtents;
  gdk_window_get_frame_extents(toplevelGdk, &toplevelFrameExtents);
  gint toplevelOriginX, toplevelOriginY;
  gdk_window_get_origin(toplevelGdk, &toplevelOriginX, &toplevelOriginY);
  g_object_unref(toplevelGdk);

  pluginX += toplevelOriginX - toplevelFrameExtents.x;
  pluginY += toplevelOriginY - toplevelFrameExtents.y;

  gdk_region_offset(region, pluginX, pluginY);
  return region;
}

int32_t pluginGetClipRegionRectCount(InstanceData* instanceData)
{
  GdkRegion* region = computeClipRegion(instanceData);
  if (!region)
    return NPTEST_INT32_ERROR;

  GdkRectangle* rects;
  gint nrects;
  gdk_region_get_rectangles(region, &rects, &nrects);
  gdk_region_destroy(region);
  g_free(rects);
  return nrects;
}

int32_t pluginGetClipRegionRectEdge(InstanceData* instanceData, 
    int32_t rectIndex, RectEdge edge)
{
  GdkRegion* region = computeClipRegion(instanceData);
  if (!region)
    return NPTEST_INT32_ERROR;

  GdkRectangle* rects;
  gint nrects;
  gdk_region_get_rectangles(region, &rects, &nrects);
  gdk_region_destroy(region);
  if (rectIndex >= nrects) {
    g_free(rects);
    return NPTEST_INT32_ERROR;
  }

  GdkRectangle rect = rects[rectIndex];
  g_free(rects);

  switch (edge) {
  case EDGE_LEFT:
    return rect.x;
  case EDGE_TOP:
    return rect.y;
  case EDGE_RIGHT:
    return rect.x + rect.width;
  case EDGE_BOTTOM:
    return rect.y + rect.height;
  }
  return NPTEST_INT32_ERROR;
}

void pluginDoInternalConsistencyCheck(InstanceData* instanceData, string& error)
{
}

string
pluginGetClipboardText(InstanceData* instanceData)
{
  GtkClipboard* cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  // XXX this is a BAD WAY to interact with GtkClipboard.  We use this
  // deprecated interface only to test nested event loop handling.
  gchar* text = gtk_clipboard_wait_for_text(cb);
  string retText = text ? text : "";

  g_free(text);

  return retText;
}

//-----------------------------------------------------------------------------
// NB: this test is quite gross in that it's not only
// nondeterministic, but dependent on the guts of the nested glib
// event loop handling code in PluginModule.  We first sleep long
// enough to make sure that the "detection timer" will be pending when
// we enter the nested glib loop, then similarly for the "process browser
// events" timer.  Then we "schedule" the crasher thread to run at about the
// same time we expect that the PluginModule "process browser events" task
// will run.  If all goes well, the plugin process will crash and generate the
// XPCOM "plugin crashed" task, and the browser will run that task while still
// in the "process some events" loop.

static void*
CrasherThread(void* data)
{
  // Give the parent thread a chance to send the message.
  usleep(200);

  // Exit (without running atexit hooks) rather than crashing with a signal
  // so as to make timing more reliable.  The process terminates immediately
  // rather than waiting for a thread in the parent process to attach and
  // generate a minidump.
  _exit(1);

  // not reached
  return(nullptr);
}

bool
pluginCrashInNestedLoop(InstanceData* instanceData)
{
  // wait at least long enough for nested loop detector task to be pending ...
  sleep(1);

  // Run the nested loop detector by processing all events that are waiting.
  bool found_event = false;
  while (g_main_context_iteration(nullptr, FALSE)) {
    found_event = true;
  }
  if (!found_event) {
    g_warning("DetectNestedEventLoop did not fire");
    return true; // trigger a test failure
  }

  // wait at least long enough for the "process browser events" task to be
  // pending ...
  sleep(1);

  // we'll be crashing soon, note that fact now to avoid messing with
  // timing too much
  mozilla::NoteIntentionalCrash("plugin");

  // schedule the crasher thread ...
  pthread_t crasherThread;
  if (0 != pthread_create(&crasherThread, nullptr, CrasherThread, nullptr)) {
    g_warning("Failed to create thread");
    return true; // trigger a test failure
  }

  // .. and hope it crashes at about the same time as the "process browser
  // events" task (that should run in this loop) is being processed in the
  // parent.
  found_event = false;
  while (g_main_context_iteration(nullptr, FALSE)) {
    found_event = true;
  }
  if (found_event) {
    g_warning("Should have crashed in ProcessBrowserEvents");
  } else {
    g_warning("ProcessBrowserEvents did not fire");
  }

  // if we get here without crashing, then we'll trigger a test failure
  return true;
}

static int
SleepThenDie(Display* display)
{
  mozilla::NoteIntentionalCrash("plugin");
  fprintf(stderr, "[testplugin:%d] SleepThenDie: sleeping\n", getpid());
  sleep(1);

  fprintf(stderr, "[testplugin:%d] SleepThenDie: dying\n", getpid());
  _exit(1);
}

bool
pluginDestroySharedGfxStuff(InstanceData* instanceData)
{
  // Closing the X socket results in the gdk error handler being
  // invoked, which exit()s us.  We want to give the parent process a
  // little while to do whatever it wanted to do, so steal the IO
  // handler from gdk and set up our own that delays seppuku.
  XSetIOErrorHandler(SleepThenDie);
  close(ConnectionNumber(GDK_DISPLAY()));
  return true;
}
