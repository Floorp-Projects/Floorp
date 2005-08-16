/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#import "FindDlgController.h"
#import "Find.h"

@interface FindDlgController(Private)

- (void)loadNewFindStringFromPasteboard;
- (void)putFindStringOnPasteboard;

- (NSString*)getSearchText:(BOOL*)outIsNew;
- (BOOL)find:(BOOL)searchBack;

@end

@implementation FindDlgController

- (void)dealloc
{
  [mLastFindString release];
  [super dealloc];
}

- (void)loadNewFindStringFromPasteboard
{
  BOOL pasteboardChanged;
  NSString* curPasteboard = [self getSearchText:&pasteboardChanged];
  if (pasteboardChanged)
    [mSearchField setStringValue:curPasteboard];

  [mSearchField selectText:nil];

  if ([[mSearchField stringValue] length] > 0)
  {
    [mFindNextButton setEnabled:YES];
    [mFindPrevButton setEnabled:YES];
  }
}

- (void)putFindStringOnPasteboard
{
  NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:NSFindPboard];
  [pasteboard declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
  [pasteboard setString:[mSearchField stringValue] forType:NSStringPboardType];

  [mLastFindString release];
  mLastFindString = [[NSString stringWithString:[mSearchField stringValue]] retain];
}

//
// -find
//
// Performs the actual search.  returns YES on success.
//

-(BOOL) find:(BOOL)searchBack
{
  NSWindowController* controller = [[NSApp mainWindow] windowController];
  if ( [controller conformsToProtocol:@protocol(Find)] ) {
    id<Find> browserController = controller;
    BOOL ignoreCase = [mIgnoreCaseBox state];
    BOOL wrapSearch = [mWrapAroundBox state];
    return [browserController findInPageWithPattern:[mSearchField stringValue] caseSensitive:!ignoreCase wrap:wrapSearch backwards:searchBack];
  } 
  else
    return NO;
}

- (IBAction) findNextButton: (id)aSender
{
  [self putFindStringOnPasteboard];
  if (![self find:NO])
    NSBeep();  
}

- (IBAction) findPreviousButton: (id)aSender
{
  [self putFindStringOnPasteboard];
  if (![self find:YES])
    NSBeep();
}

- (IBAction) findNextAndOrderOut: (id)aSender
{
  [self putFindStringOnPasteboard];
  if (![self find:NO]) {
    NSBeep();
    [[self window] makeFirstResponder:mSearchField];
  }
  else
    [self close];
}


//
// -controlTextDidChange:
//
// Check if there is anything in the text field, and if not, disable the find button
//
- (void)controlTextDidChange:(NSNotification *)aNotification
{
  if ( [[mSearchField stringValue] length] ) {
    [mFindNextButton setEnabled:YES];
    [mFindPrevButton setEnabled:YES];
  } 
  else {
    [mFindNextButton setEnabled:NO];
    [mFindPrevButton setEnabled:NO];
  }
}

//
// -getSearchText
//
// Retrieve the most recent search string
//
- (NSString*)getSearchText:(BOOL*)outIsNew
{
  NSString* searchText;
  
  NSPasteboard *findPboard = [NSPasteboard pasteboardWithName:NSFindPboard];
  if ([[findPboard types] indexOfObject:NSStringPboardType] != NSNotFound) 
    searchText = [findPboard stringForType:NSStringPboardType];
  else
    searchText = @"";
  
  if (outIsNew)
    *outIsNew = !mLastFindString || (mLastFindString && ![mLastFindString isEqualToString:searchText]);
  
  // remember the last pasteboard string that we saw
  [mLastFindString release];
  mLastFindString = [[NSString stringWithString:searchText] retain];
  
  return searchText;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
  [self loadNewFindStringFromPasteboard];
}

- (void)applicationWasActivated
{
  [self loadNewFindStringFromPasteboard];
}

@end
