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
*   Ben Goodger <ben@netscape.com> (Original Author)
*   David Haas <haasd@cae.wisc.edu>
*/
#import <AppKit/AppKit.h>
#import "BookmarksService.h"

@interface BookmarkInfoController : NSWindowController<BookmarksClient>
{
    IBOutlet NSTextField* mNameField;
    IBOutlet NSTextField* mLocationField;
    IBOutlet NSTextField* mKeywordField;
    IBOutlet NSTextField* mDescriptionField;
    IBOutlet NSTextField* mNameLabel;
    IBOutlet NSTextField* mLocationLabel;
    IBOutlet NSTextField* mKeywordLabel;
    IBOutlet NSTextField* mDescriptionLabel;
    
    IBOutlet NSView*      mVariableFieldsContainer;

    IBOutlet NSButton*    mDockMenuCheckbox;
    IBOutlet NSButton*    mTabgroupCheckbox;
    
    BookmarkItem*         mBookmarkItem;
    NSTextView*           mFieldEditor;
}

+ (id)sharedBookmarkInfoController;
+ (void)closeBookmarkInfoController;

-(void)setBookmark:(BookmarkItem*)aBookmark;
-(BookmarkItem*)bookmark;

- (IBAction)dockMenuCheckboxClicked:(id)sender;
- (IBAction)tabGroupCheckboxClicked:(id)sender;

@end
