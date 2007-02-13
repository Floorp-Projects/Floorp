/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (Original Author)
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

#import "NSString+Utils.h"
#import "NSPasteboard+Utils.h"
#import "BrowserWindowController.h"
#import "PageProxyIcon.h"

#import "MainController.h"

#include "nsCRT.h"
#include "nsNetUtil.h"
#include "nsString.h"

@implementation PageProxyIcon

- (void)awakeFromNib
{
}

- (void)dealloc
{
  [super dealloc];
}

- (BOOL)acceptsFirstResponder
{
  return NO;
}

- (void) resetCursorRects
{
  // XXX provide image for drag-hand cursor
  NSCursor* cursor = [NSCursor arrowCursor];
  [self addCursorRect:NSMakeRect(0,0,[self frame].size.width,[self frame].size.height) cursor:cursor];
  [cursor setOnMouseEntered:YES];
}

- (unsigned int)draggingSourceOperationMaskForLocal:(BOOL)isLocal
{
  if (isLocal)
    return NSDragOperationGeneric;

  return (NSDragOperationLink | NSDragOperationCopy);
}

- (void)mouseDown:(NSEvent *)theEvent
{
  // need to implement this or else mouseDragged isn't called
}

- (void)mouseUp:(NSEvent *)theEvent
{
  // select the url bar text
  [[[self window] windowController] focusURLBar];
}

- (void) mouseDragged: (NSEvent*) event
{
  NSString*		urlString = nil;
  NSString*		titleString = nil;

  id parentWindowController = [[self window] windowController];
  if ([parentWindowController isKindOfClass:[BrowserWindowController class]]) {
    BrowserWrapper* browserWrapper = [(BrowserWindowController*)parentWindowController getBrowserWrapper];
    urlString = [browserWrapper currentURI];
    titleString = [browserWrapper pageTitle];
  }

  // don't allow dragging of proxy icon for empty pages
  if ((!urlString) || [MainController isBlankURL:urlString])
    return;

  NSString     *cleanedTitle = [titleString stringByReplacingCharactersInSet:[NSCharacterSet controlCharacterSet] withString:@" "];

  NSPasteboard *pboard = [NSPasteboard pasteboardWithName:NSDragPboard];

  [pboard declareURLPasteboardWithAdditionalTypes:[NSArray array] owner:self];
  [pboard setDataForURL:urlString title:cleanedTitle];

  [self dragImage: [MainController createImageForDragging:[self image] title:titleString]
                    at: NSMakePoint(0,0) offset: NSMakeSize(0,0)
                    event: event pasteboard: pboard source: self slideBack: YES];
}

@end
