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

#import <AppKit/AppKit.h>

#import "BookmarksService.h"

class nsIDOMElement;

@class BookmarksButton;

@interface BookmarksToolbar : NSView<BookmarksClient>
{
  NSMutableArray*    mButtons;
  BookmarksButton*   mDragInsertionButton;
  int                mDragInsertionPosition;
  BOOL               mIsShowing;
  BOOL               mDrawBorder;
}

-(void)initializeToolbar;

// Called to construct & edit the initial set of personal toolbar buttons.
-(void)buildButtonList;
-(void)addButton: (nsIDOMElement*)aElt atIndex: (int)aIndex;
-(void)editButton: (nsIDOMElement*)aElt;
-(void)removeButton: (nsIDOMElement*)aElt;

// Called to lay out the buttons on the toolbar.
-(void)reflowButtons;
-(void)reflowButtonsStartingAtIndex: (int)aIndex;

-(BOOL)isShown;
-(void)setDrawBottomBorder:(BOOL)drawBorder;
-(void)showBookmarksToolbar: (BOOL)aShow;

-(IBAction)addFolder:(id)aSender;

@end
