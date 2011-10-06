/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* ***** BEGIN LICENSE BLOCK *****
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
  * The Original Code is Mozilla.org code.
  *
  * The Initial Developer of the Original Code is
  * Mozilla Corporation.
  * Portions created by the Initial Developer are Copyright (C) 2008
  * the Initial Developer. All Rights Reserved.
  *
  * Contributor(s):
  *   Josh Aas <josh@mozilla.com>
  *
  * Alternatively, the contents of this file may be used under the terms of
  * either of the GNU General Public License Version 2 or later (the "GPL"),
  * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

PRBool NS_NPAPI_CocoaWindowIsMain(void* aWindow)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (!aWindow)
    return PR_TRUE;

  NSWindow* window = (NSWindow*)aWindow;

  return (PRBool)[window isMainWindow];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(PR_TRUE);
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
  NS_ASSERTION(inView, "Must have a native view to convert coordinates.");

  // Caller has to want a result.
  if (!destX && !destY)
    return PR_FALSE;

  if (sourceSpace == destSpace) {
    if (destX)
      *destX = sourceX;
    if (destY)
      *destY = sourceY;
    return PR_TRUE;
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
      return PR_FALSE;
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
      return PR_FALSE;
  }

  if (destX)
    *destX = destPoint.x;
  if (destY)
    *destY = destPoint.y;

  return PR_TRUE;
}

