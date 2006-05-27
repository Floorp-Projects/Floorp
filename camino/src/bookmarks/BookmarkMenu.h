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
 * The Original Code is Chimera code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser <smfr@smfr.org>
 *   David Haas   <haasd@cae.wisc.edu>
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

#import <AppKit/AppKit.h>

#import "BookmarksClient.h"

@class BookmarkFolder;

// XXX share some of this code with HistoryMenu
@interface BookmarkMenu : NSMenu<BookmarksClient>
{
  IBOutlet NSMenuItem*  mItemBeforeCustomItems;    // the item after which we add our items. Not retained.

  BookmarkFolder*   mFolder;    // retained
  BOOL              mDirty;
  BOOL              mAppendTabsItem;
}

- (id)initWithTitle:(NSString *)inTitle bookmarkFolder:(BookmarkFolder*)inFolder;
- (void)setBookmarkFolder:(BookmarkFolder*)inFolder;
- (BookmarkFolder*)bookmarkFolder;

// set whether to append "Open in Tabs" item (default is YES)
- (void)setAppendOpenInTabs:(BOOL)inAppend;

// specify the item after which bookmark items will be added
// (they are assumed to go to the end of the menu). If nil,
// the entire menu is full of bookmark items.
- (void)setItemBeforeCustomItems:(NSMenuItem*)inItem;
- (NSMenuItem*)itemBeforeCustomItems;

// do an explicit rebuild
- (void)rebuildMenuIncludingSubmenus:(BOOL)includeSubmenus;

@end
