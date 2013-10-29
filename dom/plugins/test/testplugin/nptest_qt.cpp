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
 * 
 * ***** END LICENSE BLOCK ***** */
#include <QWidget>
#include <QPainter>

#include "nptest_platform.h"
#include "npapi.h"

struct _PlatformData {
#ifdef MOZ_X11
  Display* display;
  Visual* visual;
  Colormap colormap;
#endif
};
 using namespace std;

bool
pluginSupportsWindowMode()
{
  return false;
}

bool
pluginSupportsWindowlessMode()
{
  return true;
}

bool
pluginSupportsAsyncBitmapDrawing()
{
  return false;
}

NPError
pluginInstanceInit(InstanceData* instanceData)
{
#ifdef MOZ_X11
  instanceData->platformData = static_cast<PlatformData*>
    (NPN_MemAlloc(sizeof(PlatformData)));
  if (!instanceData->platformData){
    //printf("NPERR_OUT_OF_MEMORY_ERROR\n");
    return NPERR_OUT_OF_MEMORY_ERROR;
  }

  instanceData->platformData->display = nullptr;
  instanceData->platformData->visual = nullptr;
  instanceData->platformData->colormap = None;

  return NPERR_NO_ERROR;
#else
  printf("NPERR_INCOMPATIBLE_VERSION_ERROR\n");
  return NPERR_INCOMPATIBLE_VERSION_ERROR;
#endif
  return NPERR_NO_ERROR;
}

void
pluginInstanceShutdown(InstanceData* instanceData)
{
  NPN_MemFree(instanceData->platformData);
  instanceData->platformData = 0;
}

void
pluginDoSetWindow(InstanceData* instanceData, NPWindow* newWindow)
{
  instanceData->window = *newWindow;
}

void
pluginWidgetInit(InstanceData* instanceData, void* oldWindow)
{
  // XXX nothing here yet since we don't support windowed plugins
}

static void
pluginDrawWindow(InstanceData* instanceData, void* event)
{
  NPWindow& window = instanceData->window;
  // When we have a widget, window.x/y are meaningless since our
  // widget is always positioned correctly and we just draw into it at 0,0
  int x = instanceData->hasWidget ? 0 : window.x;
  int y = instanceData->hasWidget ? 0 : window.y;
  int width = window.width;
  int height = window.height;

#ifdef MOZ_X11
  XEvent* nsEvent = (XEvent*)event;
  const XGraphicsExposeEvent& expose = nsEvent->xgraphicsexpose;

  QColor drawColor((QColor)instanceData->scriptableObject->drawColor);//QRgb qRgba ( int r, int g, int b, int a )
#ifdef Q_WS_X11
  QPixmap pixmap = QPixmap::fromX11Pixmap(expose.drawable, QPixmap::ExplicitlyShared);

  QRect exposeRect(expose.x, expose.y, expose.width, expose.height);
  if (instanceData->scriptableObject->drawMode == DM_SOLID_COLOR) {
    //printf("Drawing Solid\n");
    // drawing a solid color for reftests
    QPainter painter(&pixmap);
    painter.fillRect(exposeRect, drawColor);
    notifyDidPaint(instanceData);
    return;

  }
#endif
#endif

  NPP npp = instanceData->npp;
  if (!npp)
    return;

  QString text (NPN_UserAgent(npp));
  if (text.isEmpty())
    return;

#ifdef MOZ_X11
#ifdef Q_WS_X11
  //printf("Drawing Default\n");
  // drawing a solid color for reftests
  QColor color;
  QPainter painter(&pixmap);
  QRect theRect(x, y, width, height);
  QRect clipRect(QPoint(window.clipRect.left, window.clipRect.top),
                 QPoint(window.clipRect.right, window.clipRect.bottom));
  painter.setClipRect(clipRect);
  painter.fillRect(theRect, QColor(128,128,128,255));
  painter.drawRect(theRect);
  painter.drawText(QRect(theRect), Qt::AlignCenter, text);
  notifyDidPaint(instanceData);
#endif
#endif
  return;
}

int16_t
pluginHandleEvent(InstanceData* instanceData, void* event)
{
#ifdef MOZ_X11
  XEvent* nsEvent = (XEvent*)event;
  //printf("\nEvent Type %d\n", nsEvent->type);
  switch (nsEvent->type) {
  case GraphicsExpose: {
    //printf("GraphicsExpose\n");

    pluginDrawWindow(instanceData, event);
    break;
  }
  case MotionNotify: {
    //printf("MotionNotify\n");
    XMotionEvent* motion = &nsEvent->xmotion;
    instanceData->lastMouseX = motion->x;
    instanceData->lastMouseY = motion->y;
    break;
  }
  case ButtonPress:{
    ////printf("ButtonPress\n");
    break;
  }
  case ButtonRelease: {
    //printf("ButtonRelease\n");
    XButtonEvent* button = &nsEvent->xbutton;
    instanceData->lastMouseX = button->x;
    instanceData->lastMouseY = button->y;
    instanceData->mouseUpEventCount++;
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
  // XXX nothing here yet since we don't support windowed plugins
  return NPTEST_INT32_ERROR;
}

int32_t pluginGetClipRegionRectCount(InstanceData* instanceData)
{
  // XXX nothing here yet since we don't support windowed plugins
  return NPTEST_INT32_ERROR;
}

int32_t pluginGetClipRegionRectEdge(InstanceData* instanceData, 
    int32_t rectIndex, RectEdge edge)
{
  // XXX nothing here yet since we don't support windowed plugins
  return NPTEST_INT32_ERROR;
}

void pluginDoInternalConsistencyCheck(InstanceData* instanceData, string& error)
{
}
