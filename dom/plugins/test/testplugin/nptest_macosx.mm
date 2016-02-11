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

#include "nptest_platform.h"
#include "nsAlgorithm.h"
#include <CoreServices/CoreServices.h>
#include <algorithm>

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

NPError
pluginInstanceInit(InstanceData* instanceData)
{
  NPP npp = instanceData->npp;

  NPBool supportsCoreGraphics = false;
  if ((NPN_GetValue(npp, NPNVsupportsCoreGraphicsBool, &supportsCoreGraphics) == NPERR_NO_ERROR) &&
      supportsCoreGraphics) {
    NPN_SetValue(npp, NPPVpluginDrawingModel, (void*)NPDrawingModelCoreGraphics);
  } else {
    printf("CoreGraphics drawing model not supported, can't create a plugin instance.\n");
    return NPERR_INCOMPATIBLE_VERSION_ERROR;
  }

  NPBool supportsCocoaEvents = false;
  if ((NPN_GetValue(npp, NPNVsupportsCocoaBool, &supportsCocoaEvents) == NPERR_NO_ERROR) &&
      supportsCocoaEvents) {
    NPN_SetValue(npp, NPPVpluginEventModel, (void*)NPEventModelCocoa);
    instanceData->eventModel = NPEventModelCocoa;
  } else {
    printf("Cocoa event model not supported, can't create a plugin instance.\n");
    return NPERR_INCOMPATIBLE_VERSION_ERROR;
  }

  return NPERR_NO_ERROR;
}

void
pluginInstanceShutdown(InstanceData* instanceData)
{
}

static bool
RectEquals(const NPRect& r1, const NPRect& r2)
{
  return r1.left == r2.left && r1.top == r2.top &&
         r1.right == r2.right && r1.bottom == r2.bottom;
}

void
pluginDoSetWindow(InstanceData* instanceData, NPWindow* newWindow)
{
  // Ugh. Due to a terrible Gecko bug, we have to ignore position changes
  // when the clip rect doesn't change; the position can be wrong
  // when set by a path other than nsPluginFrame::FixUpPluginWindow.
  int32_t oldX = instanceData->window.x;
  int32_t oldY = instanceData->window.y;
  bool clipChanged =
    !RectEquals(instanceData->window.clipRect, newWindow->clipRect);
  instanceData->window = *newWindow;
  if (!clipChanged) {
    instanceData->window.x = oldX;
    instanceData->window.y = oldY;
  }
}

void
pluginWidgetInit(InstanceData* instanceData, void* oldWindow)
{
  // Should never be called since we don't support window mode
}

static void 
GetColorsFromRGBA(uint32_t rgba, float* r, float* g, float* b, float* a)
{
  *b = (rgba & 0xFF) / 255.0;
  *g = ((rgba & 0xFF00) >> 8) / 255.0;
  *r = ((rgba & 0xFF0000) >> 16) / 255.0;
  *a = ((rgba & 0xFF000000) >> 24) / 255.0;
}

static void
pluginDraw(InstanceData* instanceData, NPCocoaEvent* event)
{
  if (!instanceData)
    return;

  notifyDidPaint(instanceData);

  NPP npp = instanceData->npp;
  if (!npp)
    return;
  
  const char* uaString = NPN_UserAgent(npp);
  if (!uaString)
    return;

  NPWindow window = instanceData->window;

  CGContextRef cgContext = event->data.draw.context;

  float windowWidth = window.width;
  float windowHeight = window.height;

  switch(instanceData->scriptableObject->drawMode) {
  case DM_DEFAULT: {
    CFStringRef uaCFString = CFStringCreateWithCString(kCFAllocatorDefault, uaString, kCFStringEncodingASCII);
    // save the cgcontext gstate
    CGContextSaveGState(cgContext);

    // we get a flipped context
    CGContextTranslateCTM(cgContext, 0.0, windowHeight);
    CGContextScaleCTM(cgContext, 1.0, -1.0);

    // draw a gray background for the plugin
    CGContextAddRect(cgContext, CGRectMake(0, 0, windowWidth, windowHeight));
    CGContextSetGrayFillColor(cgContext, 0.5, 1.0);
    CGContextDrawPath(cgContext, kCGPathFill);

    // draw a black frame around the plugin
    CGContextAddRect(cgContext, CGRectMake(0, 0, windowWidth, windowHeight));
    CGContextSetGrayStrokeColor(cgContext, 0.0, 1.0);
    CGContextSetLineWidth(cgContext, 6.0);
    CGContextStrokePath(cgContext);

    // draw the UA string using Core Text
    CGContextSetTextMatrix(cgContext, CGAffineTransformIdentity);

    // Initialize a rectangular path.
    CGMutablePathRef path = CGPathCreateMutable();
    CGRect bounds = CGRectMake(10.0, 10.0, std::max(0.0, windowWidth - 20.0),
                               std::max(0.0, windowHeight - 20.0));
    CGPathAddRect(path, NULL, bounds);

    // Initialize an attributed string.
    CFMutableAttributedStringRef attrString = CFAttributedStringCreateMutable(kCFAllocatorDefault, 0);
    CFAttributedStringReplaceString(attrString, CFRangeMake(0, 0), uaCFString);

    // Create a color and add it as an attribute to the string.
    CGColorSpaceRef rgbColorSpace = CGColorSpaceCreateDeviceRGB();
    CGFloat components[] = { 0.0, 0.0, 0.0, 1.0 };
    CGColorRef red = CGColorCreate(rgbColorSpace, components);    
    CGColorSpaceRelease(rgbColorSpace);
    CFAttributedStringSetAttribute(attrString, CFRangeMake(0, 50), kCTForegroundColorAttributeName, red);

    // Create the framesetter with the attributed string.
    CTFramesetterRef framesetter = CTFramesetterCreateWithAttributedString(attrString);
    CFRelease(attrString);

    // Create the frame and draw it into the graphics context
    CTFrameRef frame = CTFramesetterCreateFrame(framesetter, CFRangeMake(0, 0), path, NULL);
    CFRelease(framesetter);
    if (frame) {
      CTFrameDraw(frame, cgContext);
      CFRelease(frame);
    }

    // restore the cgcontext gstate
    CGContextRestoreGState(cgContext);
    break;
  }
  case DM_SOLID_COLOR: {
    // save the cgcontext gstate
    CGContextSaveGState(cgContext);

    // we get a flipped context
    CGContextTranslateCTM(cgContext, 0.0, windowHeight);
    CGContextScaleCTM(cgContext, 1.0, -1.0);

    // draw a solid background for the plugin
    CGContextAddRect(cgContext, CGRectMake(0, 0, windowWidth, windowHeight));

    float r,g,b,a;
    GetColorsFromRGBA(instanceData->scriptableObject->drawColor, &r, &g, &b, &a);
    CGContextSetRGBFillColor(cgContext, r, g, b, a);
    CGContextDrawPath(cgContext, kCGPathFill);

    // restore the cgcontext gstate
    CGContextRestoreGState(cgContext);
    break;
  }
  }
}

int16_t
pluginHandleEvent(InstanceData* instanceData, void* event)
{
  NPCocoaEvent* cocoaEvent = (NPCocoaEvent*)event;
  if (!cocoaEvent)
    return kNPEventNotHandled;

  switch (cocoaEvent->type) {
    case NPCocoaEventDrawRect:
      pluginDraw(instanceData, cocoaEvent);
      break;
    case NPCocoaEventMouseDown:
    case NPCocoaEventMouseUp:
    case NPCocoaEventMouseMoved:
    case NPCocoaEventMouseDragged:
      instanceData->lastMouseX = (int32_t)cocoaEvent->data.mouse.pluginX;
      instanceData->lastMouseY = (int32_t)cocoaEvent->data.mouse.pluginY;
      if (cocoaEvent->type == NPCocoaEventMouseUp) {
        instanceData->mouseUpEventCount++;
      }
      break;
    case NPCocoaEventWindowFocusChanged:
      instanceData->topLevelWindowActivationState = cocoaEvent->data.focus.hasFocus ?
        ACTIVATION_STATE_ACTIVATED : ACTIVATION_STATE_DEACTIVATED;
      instanceData->topLevelWindowActivationEventCount = instanceData->topLevelWindowActivationEventCount + 1;
      break;
    case NPCocoaEventFocusChanged:
      instanceData->focusState = cocoaEvent->data.focus.hasFocus ?
      ACTIVATION_STATE_ACTIVATED : ACTIVATION_STATE_DEACTIVATED;
      instanceData->focusEventCount = instanceData->focusEventCount + 1;
      break;
    default:
      return kNPEventNotHandled;
  }

  return kNPEventHandled;
}

int32_t pluginGetEdge(InstanceData* instanceData, RectEdge edge)
{
  NPWindow* w = &instanceData->window;
  switch (edge) {
  case EDGE_LEFT:
    return w->x;
  case EDGE_TOP:
    return w->y;
  case EDGE_RIGHT:
    return w->x + w->width;
  case EDGE_BOTTOM:
    return w->y + w->height;
  }
  MOZ_CRASH("Unexpected RectEdge?!");
}

int32_t pluginGetClipRegionRectCount(InstanceData* instanceData)
{
  return 1;
}

int32_t pluginGetClipRegionRectEdge(InstanceData* instanceData, 
    int32_t rectIndex, RectEdge edge)
{
  if (rectIndex != 0)
    return NPTEST_INT32_ERROR;

  // We have to add the Cocoa titlebar height here since the clip rect
  // is being returned relative to that
  static const int COCOA_TITLEBAR_HEIGHT = 22;

  NPWindow* w = &instanceData->window;
  switch (edge) {
  case EDGE_LEFT:
    return w->clipRect.left;
  case EDGE_TOP:
    return w->clipRect.top + COCOA_TITLEBAR_HEIGHT;
  case EDGE_RIGHT:
    return w->clipRect.right;
  case EDGE_BOTTOM:
    return w->clipRect.bottom + COCOA_TITLEBAR_HEIGHT;
  }
  MOZ_CRASH("Unexpected RectEdge?!");
}

void pluginDoInternalConsistencyCheck(InstanceData* instanceData, string& error)
{
}
