/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#import "FindDlgController.h"
#import "BrowserWindowController.h"
#import "CHFind.h"
#include "nsCOMPtr.h"


@implementation FindDlgController

- (id)initWithWindowNibName:(NSString *)windowNibName
{
  if ( (self = [super initWithWindowNibName:windowNibName]) )
    mSearchText = nil;
  return self;
}

- (void)dealloc
{
  [mSearchText release];
}

- (void)windowDidLoad
{
}


//
// -find
//
// User clicked the find button, send the action to the window controller of the
// frontmost browser window. If we found something, hide the window. If not, beep.
//
-(IBAction) find: (id)aSender
{
  NSWindowController* controller = [[NSApp mainWindow] windowController];
  if ( [controller conformsToProtocol:@protocol(CHFind)] ) {
    id<CHFind> browserController = controller;
    BOOL ignoreCase = [mIgnoreCaseBox state];
    BOOL wrapSearch = [mWrapAroundBox state];
    BOOL searchBack = [mSearchBackwardsBox state];

    [self storeSearchText:[mSearchField stringValue]];

    BOOL found = [browserController findInPageWithPattern:mSearchText caseSensitive:!ignoreCase
        wrap:wrapSearch backwards:searchBack];

    if (! found ) 
      NSBeep();
    // we stay open
  }
  else
    NSBeep();
}


//
// -findAgain
//
// Someone hit "find again" in the edit menu, send the action to the window controller of the
// frontmost browser window. Beep if we didn't find something.
//
-(IBAction) findAgain: (id)aSender
{
  NSWindowController* controller = [[NSApp mainWindow] windowController];
  if ( [controller conformsToProtocol:@protocol(CHFind)] ) {
    id<CHFind> browserController = controller;
    BOOL ignoreCase = [mIgnoreCaseBox state];
    BOOL wrapSearch = [mWrapAroundBox state];
    BOOL searchBack = [mSearchBackwardsBox state];
    
    BOOL found = [browserController findInPageWithPattern:mSearchText caseSensitive:!ignoreCase
        wrap:wrapSearch backwards:searchBack];
    if ( !found )
      NSBeep();
  }
  else
    NSBeep();
}

//
// controlTextDidChange
//
// Check if there is anything in the text field, and if not, disable the find button
//
- (void)controlTextDidChange:(NSNotification *)aNotification
{
  if ( [[mSearchField stringValue] length] )
    [mFindButton setEnabled:PR_TRUE];
  else
    [mFindButton setEnabled:PR_FALSE];    
}

- (void)storeSearchText:(NSString*)inText
{
  [mSearchText autorelease];
  mSearchText = inText;
  [mSearchText retain];
}

- (NSString*)getSearchText
{
  return mSearchText;
}

@end
