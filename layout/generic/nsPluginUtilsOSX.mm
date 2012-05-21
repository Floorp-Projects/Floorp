/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPluginUtilsOSX.h"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#include "nsObjCExceptions.h"

#ifndef __LP64__
void NS_NPAPI_CarbonWindowFrame(WindowRef aWindow, nsRect& outRect)
{
  if (!aWindow)
    return;

  Rect windowRect;
  ::GetWindowBounds(aWindow, kWindowStructureRgn, &windowRect);
  outRect.x = windowRect.left;
  outRect.y = windowRect.top;
  outRect.width = windowRect.right - windowRect.left;
  outRect.height = windowRect.bottom - windowRect.top;
}
#endif

void NS_NPAPI_CocoaWindowFrame(void* aWindow, nsRect& outRect)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!aWindow)
    return;

  NSWindow* window = (NSWindow*)aWindow;

  float menubarScreenHeight;
  NSArray* allScreens = [NSScreen screens];
  if ([allScreens count])
    menubarScreenHeight = [[allScreens objectAtIndex:0] frame].size.height;
  else
    return; // If there are no screens, there's not much we can say.

  NSRect frame = [window frame];
  outRect.x = (nscoord)frame.origin.x;
  outRect.y = (nscoord)(menubarScreenHeight - (frame.origin.y + frame.size.height));
  outRect.width = (nscoord)frame.size.width;
  outRect.height = (nscoord)frame.size.height;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

bool NS_NPAPI_CocoaWindowIsMain(void* aWindow)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (!aWindow)
    return true;

  NSWindow* window = (NSWindow*)aWindow;

  return (bool)[window isMainWindow];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(true);
}

NPError NS_NPAPI_ShowCocoaContextMenu(void* menu, nsIWidget* widget, NPCocoaEvent* event)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (!menu || !widget || !event)
    return NPERR_GENERIC_ERROR;

  NSMenu* cocoaMenu = (NSMenu*)menu;
  NSView* cocoaView = (NSView*)widget->GetNativeData(NS_NATIVE_WIDGET);

  NSEventType cocoaEventType = NSRightMouseDown;
  unsigned int cocoaModifierFlags = 0;
  double x = 0.0;   // Coordinates for the context menu in plugin terms, top-left origin.
  double y = 0.0;

  NPCocoaEventType eventType = event->type;
  if (eventType == NPCocoaEventMouseDown ||
      eventType == NPCocoaEventMouseUp ||
      eventType == NPCocoaEventMouseMoved ||
      eventType == NPCocoaEventMouseEntered ||
      eventType == NPCocoaEventMouseExited ||
      eventType == NPCocoaEventMouseDragged) {
    x = event->data.mouse.pluginX;
    y = event->data.mouse.pluginY;
    if ((x < 0.0) || (y < 0.0))
      return NPERR_GENERIC_ERROR;
  }

  // Flip the coords to bottom-left origin.
  NSRect viewFrame = [cocoaView frame];
  double shiftedX = x;
  double shiftedY = viewFrame.size.height - y;
  // Shift to window coords.
  shiftedX += viewFrame.origin.x;
  shiftedY += [cocoaView convertPoint:NSMakePoint(0,0) toView:nil].y - viewFrame.size.height;

  // Create an NSEvent we can use to show the context menu. Only the location
  // is important here so just simulate a right mouse down. The coordinates
  // must be in top-level window terms.
  NSEvent* cocoaEvent = [NSEvent mouseEventWithType:cocoaEventType
                                           location:NSMakePoint(shiftedX, shiftedY)
                                      modifierFlags:cocoaModifierFlags
                                          timestamp:0
                                       windowNumber:[[cocoaView window] windowNumber]
                                            context:nil
                                        eventNumber:0
                                         clickCount:1
                                           pressure:0.0];

  [NSMenu popUpContextMenu:cocoaMenu withEvent:cocoaEvent forView:cocoaView];

  return NPERR_NO_ERROR;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NPERR_GENERIC_ERROR);
}

NPBool NS_NPAPI_ConvertPointCocoa(void* inView,
                                  double sourceX, double sourceY, NPCoordinateSpace sourceSpace,
                                  double *destX, double *destY, NPCoordinateSpace destSpace)
{
  // Plugins don't always have a view/frame. It would be odd to ask for a point conversion
  // without a view, so we'll warn about it, but it's technically OK.
  if (!inView) {
    NS_WARNING("Must have a native view to convert coordinates.");
    return false;
  }

  // Caller has to want a result.
  if (!destX && !destY)
    return false;

  if (sourceSpace == destSpace) {
    if (destX)
      *destX = sourceX;
    if (destY)
      *destY = sourceY;
    return true;
  }

  NSView* view = (NSView*)inView;
  NSWindow* window = [view window];
  NSPoint sourcePoint = NSMakePoint(sourceX, sourceY);

  // Convert to screen space.
  NSPoint screenPoint;
  switch (sourceSpace) {
    case NPCoordinateSpacePlugin:
      screenPoint = [view convertPoint:sourcePoint toView:nil];
      screenPoint = [window convertBaseToScreen:screenPoint];
      break;
    case NPCoordinateSpaceWindow:
      screenPoint = [window convertBaseToScreen:sourcePoint];
      break;
    case NPCoordinateSpaceFlippedWindow:
      sourcePoint.y = [window frame].size.height - sourcePoint.y;
      screenPoint = [window convertBaseToScreen:sourcePoint];
      break;
    case NPCoordinateSpaceScreen:
      screenPoint = sourcePoint;
      break;
    case NPCoordinateSpaceFlippedScreen:
      sourcePoint.y = [[[NSScreen screens] objectAtIndex:0] frame].size.height - sourcePoint.y;
      screenPoint = sourcePoint;
      break;
    default:
      return false;
  }

  // Convert from screen to dest space.
  NSPoint destPoint;
  switch (destSpace) {
    case NPCoordinateSpacePlugin:
      destPoint = [window convertScreenToBase:screenPoint];
      destPoint = [view convertPoint:destPoint fromView:nil];
      break;
    case NPCoordinateSpaceWindow:
      destPoint = [window convertScreenToBase:screenPoint];
      break;
    case NPCoordinateSpaceFlippedWindow:
      destPoint = [window convertScreenToBase:screenPoint];
      destPoint.y = [window frame].size.height - destPoint.y;
      break;
    case NPCoordinateSpaceScreen:
      destPoint = screenPoint;
      break;
    case NPCoordinateSpaceFlippedScreen:
      destPoint = screenPoint;
      destPoint.y = [[[NSScreen screens] objectAtIndex:0] frame].size.height - destPoint.y;
      break;
    default:
      return false;
  }

  if (destX)
    *destX = destPoint.x;
  if (destY)
    *destY = destPoint.y;

  return true;
}

