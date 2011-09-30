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

// We can use Carbon in this header but not Cocoa. Cocoa pointers must be void.

#ifndef __LP64__
#import <Carbon/Carbon.h>
#endif

#include "nsRect.h"
#include "nsIWidget.h"
#include "npapi.h"

// We use void pointers to avoid exporting native event types to cross-platform code.

#ifndef __LP64__
// Get the rect for an entire top-level Carbon window in screen coords.
void NS_NPAPI_CarbonWindowFrame(WindowRef aWindow, nsRect& outRect);
#endif

// Get the rect for an entire top-level Cocoa window in screen coords.
void NS_NPAPI_CocoaWindowFrame(void* aWindow, nsRect& outRect);

// Returns whether or not a Cocoa NSWindow has main status.
bool NS_NPAPI_CocoaWindowIsMain(void* aWindow);

// Puts up a Cocoa context menu (NSMenu) for a particular NPCocoaEvent.
NPError NS_NPAPI_ShowCocoaContextMenu(void* menu, nsIWidget* widget, NPCocoaEvent* event);

NPBool NS_NPAPI_ConvertPointCocoa(void* inView,
                                  double sourceX, double sourceY, NPCoordinateSpace sourceSpace,
                                  double *destX, double *destY, NPCoordinateSpace destSpace);

