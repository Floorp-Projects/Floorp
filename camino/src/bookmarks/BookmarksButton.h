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
*   David Hyatt <hyatt@netscape.com> (Original Author)
*/

#import <Foundation/Foundation.h>
#import <Appkit/Appkit.h>

class nsIDOMElement;
class BookmarksService;

@class BookmarkItem;

@interface BookmarksButton : NSButton
{
  nsIDOMElement*    mElement;
  BookmarkItem*     mBookmarkItem;
  BOOL              mIsFolder;
}

-(id)initWithFrame:(NSRect)frame element:(nsIDOMElement*)element;

-(void)setElement: (nsIDOMElement*)aElt;
-(nsIDOMElement*)element;

-(IBAction)openBookmark:(id)aSender;
-(IBAction)openBookmarkInNewTab:(id)aSender;
-(IBAction)openBookmarkInNewWindow:(id)aSender;
-(IBAction)showBookmarkInfo:(id)aSender;
-(IBAction)deleteBookmarks: (id)aSender;
-(IBAction)addFolder:(id)aSender;


@end
