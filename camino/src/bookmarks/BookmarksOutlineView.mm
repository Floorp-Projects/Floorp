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

#import "BookmarksOutlineView.h"
#import "BookmarksService.h"
#import "BookmarksDataSource.h"

#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"

@implementation BookmarksOutlineView

- (void)awakeFromNib
{
  [self registerForDraggedTypes:[NSArray arrayWithObjects:@"MozURLType", @"MozBookmarkType", NSStringPboardType, NSURLPboardType, nil]];
}

- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
{
  if (operation == NSDragOperationDelete)
  {
    NSArray* contentIds = nil;
    NSPasteboard* pboard = [NSPasteboard pasteboardWithName:NSDragPboard];
    contentIds = [pboard propertyListForType: @"MozBookmarkType"];
    if (contentIds) {
      for (unsigned int i = 0; i < [contentIds count]; ++i)
      {
        BookmarkItem* item = BookmarksService::GetWrapperFor([[contentIds objectAtIndex:i] unsignedIntValue]);
        [item remove];
      }
    }
  }
}

- (unsigned int)draggingSourceOperationMaskForLocal:(BOOL)localFlag
{
  if (localFlag)
    return (NSDragOperationCopy | NSDragOperationGeneric | NSDragOperationMove);
  
	return (NSDragOperationDelete | NSDragOperationGeneric);
}

@end
