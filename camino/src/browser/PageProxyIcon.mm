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
* Copyright (C) 2002 Netscape Communications Corporation. All
* Rights Reserved.
*
* Contributor(s):
*   Joe Hewitt <hewitt@netscape.com> (Original Author)
*/

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

- (unsigned int)draggingSourceOperationMaskForLocal:(BOOL)flag
{
  return NSDragOperationGeneric;
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
  [[[[self window] windowController] getBrowserWrapper] getTitle:&titleString andHref:&urlString];

  NSString     *cleanedTitle = [titleString stringByReplacingCharactersInSet:[NSCharacterSet controlCharacterSet] withString:@" "];

  NSPasteboard *pboard = [NSPasteboard pasteboardWithName:NSDragPboard];

  [pboard declareURLPasteboardWithAdditionalTypes:[NSArray array] owner:self];
  [pboard setDataForURL:urlString title:cleanedTitle];
  
  [self dragImage: [MainController createImageForDragging:[self image] title:titleString]
                    at: NSMakePoint(0,0) offset: NSMakeSize(0,0)
                    event: event pasteboard: pboard source: self slideBack: YES];
}


@end
