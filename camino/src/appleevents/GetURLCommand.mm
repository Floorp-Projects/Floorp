/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
*
* The contents of this file are subject to the Mozilla Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS
* IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
* implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is the Mozilla browser.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation. Portions created by Netscape are
* Copyright (C) 1999 Netscape Communications Corporation. All
* Rights Reserved.
*
* Contributor(s):
*   David Hyatt <hyatt@netscape.com> (Original Author)
*/

#import "CHGetURLCommand.h"
#import <AppKit/AppKit.h>

#import "BrowserWindowController.h"

#include "nsIPref.h"
#include "nsCOMPtr.h"
#include "nsIServiceManagerUtils.h"

@implementation CHGetURLCommand

- (id)performDefaultImplementation 
{
  // get the pref that specifies if we want to re-use the existing window or
  // open a new one. There's really no point caching this pref.
  PRBool reuseWindow = PR_FALSE;
  nsCOMPtr<nsIPref> prefService ( do_GetService(NS_PREF_CONTRACTID) );
  if ( prefService )
    prefService->GetBoolPref("browser.always_reuse_window", &reuseWindow);
  
  // reuse the main window if there is one. The user may have closed all of 
  // them or we may get this event at startup before we've had time to load
  // our window.
  BrowserWindowController* controller = nsnull;
  if ( reuseWindow && [NSApp mainWindow] ) {
    controller = [[NSApp mainWindow] windowController];
    [controller loadURLString:[self directParameter]];
  }
  else
    controller = [[NSApp delegate] openBrowserWindowWithURL: [self directParameter]];

  [[[controller getBrowserWrapper] getBrowserView] setActive: YES];
  return nil;
}

@end
