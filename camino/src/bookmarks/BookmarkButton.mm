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
 *   David Hyatt <hyatt@mozilla.org> (Original Author)
 *   David Haas <haasd@cae.wisc.edu>
 *   Bruce Davidson <Bruce.Davidson@ipl.com>
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

#import "BookmarkButton.h"
#import "NSString+Utils.h"
#import "NSPasteboard+Utils.h"
#import "DraggableImageAndTextCell.h"
#import "BookmarkManager.h"
#import "Bookmark.h"
#import "BookmarkFolder.h"
#import "BookmarkMenu.h"
#import "BookmarkInfoController.h"
#import "BrowserWindowController.h"
#import "MainController.h"
#import "PreferenceManager.h"

@interface BookmarkButton(Private)

- (void)showFolderPopupAction:(id)aSender;
- (void)showFolderPopup:(NSEvent*)event;

@end


@implementation BookmarkButton

- (id)initWithFrame:(NSRect)frame
{
  if ((self = [super initWithFrame:frame])) {
    DraggableImageAndTextCell* newCell = [[[DraggableImageAndTextCell alloc] init] autorelease];
    [newCell setDraggable:YES];
    [self setCell:newCell];

    [self setBezelStyle:NSRegularSquareBezelStyle];
    [self setButtonType:NSMomentaryChangeButton];
    [self setBordered:NO];
    [self setImagePosition:NSImageLeft];
    [self setRefusesFirstResponder:YES];
    [self setFont:[NSFont labelFontOfSize:11.0]];

    mLastEventWasMenu = NO;
  }
  return self;
}

- (id)initWithFrame:(NSRect)frame item:(BookmarkItem*)item
{
  if ((self = [self initWithFrame:frame])) {
    [self setBookmarkItem:item];
  }
  return self;
}

- (void)dealloc
{
  [mItem release];
  [super dealloc];
}

- (void)setBookmarkItem:(BookmarkItem*)aItem
{
  [aItem retain];
  [mItem release];
  mItem = aItem;

  if ([aItem isKindOfClass:[Bookmark class]]) {
    Bookmark* bookmarkItem = (Bookmark*)aItem;
    if ([bookmarkItem isSeparator]) {
      [self setTitle:nil];
      [self setImage:[NSImage imageNamed:@"bm_separator"]];
      return;
    }
    [self setAction:@selector(openBookmark:)];

    NSString* tooltipString = [NSString stringWithFormat:NSLocalizedString(@"BookmarkButtonTooltipFormat", nil),
                                                [bookmarkItem title],
                                                [bookmarkItem url]];
    // using "\n\n" as a tooltip string causes Cocoa to hang when displaying the tooltip,
    // so be paranoid about not doing that
    if (![tooltipString isEqualToString:@"\n\n"])
      [self setToolTip:tooltipString];
  }
  else {
    [[self cell] setClickHoldTimeout:0.5];
    if ([(BookmarkFolder *)aItem isGroup])
      [self setAction:@selector(openBookmark:)];
    else
      [self setAction:@selector(showFolderPopupAction:)];
  }
  [self setTitle:[aItem title]];
  [self setImage:[aItem icon]];
  [self setTarget:self];
}

- (void)bookmarkChanged:(BOOL*)outNeedsReflow
{
  // Don't let separator items change - we love them just the way they are
  if ([mItem isSeparator]) {
    *outNeedsReflow = NO;
    return;
  }

  if (![[self title] isEqualToString:[mItem title]]) {
    *outNeedsReflow = YES;    // assume title width changed
    [self setTitle:[mItem title]];
  }

  if ([self image] != [mItem icon]) {
    // all images are the same size, so this won't trigger reflows
    *outNeedsReflow = !NSEqualSizes([[self image] size], [[mItem icon] size]);
    [self setImage:[mItem icon]];
  }

  // folder items can be toggled between folders and tab groups
  if ([mItem isKindOfClass:[BookmarkFolder class]]) {
    if ([(BookmarkFolder *)mItem isGroup])
      [self setAction:@selector(openBookmark:)];
    else
      [self setAction:@selector(showFolderPopupAction:)];
  }
}

- (BookmarkItem*)bookmarkItem
{
  return mItem;
}

- (IBAction)openBookmark:(id)aSender
{
  BrowserWindowController* brController = [[self window] windowController];
  BookmarkItem *item = [self bookmarkItem];
  BOOL reverseBGPref = ([[NSApp currentEvent] modifierFlags] & NSShiftKeyMask) != 0;
  EBookmarkOpenBehavior openBehavior = eBookmarkOpenBehavior_Preferred;
  if (([[NSApp currentEvent] modifierFlags] & NSCommandKeyMask) != 0)
    openBehavior = eBookmarkOpenBehavior_NewPreferred;

  [[NSApp delegate] loadBookmark:item withBWC:brController openBehavior:openBehavior reverseBgToggle:reverseBGPref];
}

- (IBAction)openBookmarkInNewTab:(id)aSender
{
  BrowserWindowController* brController = [[self window] windowController];
  BookmarkItem *item = [self bookmarkItem];
  BOOL reverseBGPref = ([aSender keyEquivalentModifierMask] & NSShiftKeyMask) != 0;

  [[NSApp delegate] loadBookmark:item withBWC:brController openBehavior:eBookmarkOpenBehavior_NewTab reverseBgToggle:reverseBGPref];
}

- (IBAction)openBookmarkInNewWindow:(id)aSender
{
  BrowserWindowController* brController = [[self window] windowController];
  BookmarkItem *item = [self bookmarkItem];
  BOOL reverseBGPref = ([aSender keyEquivalentModifierMask] & NSShiftKeyMask) != 0;

  [[NSApp delegate] loadBookmark:item withBWC:brController openBehavior:eBookmarkOpenBehavior_NewWindow reverseBgToggle:reverseBGPref];
}

- (IBAction)copyURLs:(id)aSender
{
  [[BookmarkManager sharedBookmarkManager] copyBookmarksURLs:[NSArray arrayWithObject:[self bookmarkItem]]
                                                toPasteboard:[NSPasteboard generalPasteboard]];
}

- (IBAction)showBookmarkInfo:(id)aSender
{
  BookmarkInfoController *bic = [BookmarkInfoController sharedBookmarkInfoController];
  [bic setBookmark:[self bookmarkItem]];
  [bic showWindow:self];
}

- (IBAction)revealBookmark:(id)aSender
{
  [[[self window] windowController] revealBookmark:[self bookmarkItem]];
}

- (IBAction)deleteBookmarks:(id)aSender
{
  BookmarkItem *item = [self bookmarkItem];
  BOOL deleted = [[item parent] deleteChild:item];
  if (deleted)
    [self removeFromSuperview];
}

- (IBAction)addFolder:(id)aSender
{
  BookmarkManager* bmManager = [BookmarkManager sharedBookmarkManager];
  BookmarkFolder* toolbarFolder = [bmManager toolbarFolder];
  BookmarkFolder* aFolder = [toolbarFolder addBookmarkFolder];
  [aFolder setTitle:NSLocalizedString(@"NewBookmarkFolder", nil)];
}

- (void)drawRect:(NSRect)aRect
{
  [super drawRect:aRect];
}

- (NSMenu*)menuForEvent:(NSEvent*)aEvent
{
  mLastEventWasMenu = YES;
  NSArray* theItemArray = [NSArray arrayWithObject:[self bookmarkItem]];
  return [[BookmarkManager sharedBookmarkManager] contextMenuForItems:theItemArray fromView:nil target:self];
}

- (void)showFolderPopupAction:(id)aSender
{
  [self showFolderPopup:[NSApp currentEvent]];
}

//
// -showFolderPopup:
//
// For bookmarks that are folders, display their children in a menu. Uses a transient
// NSPopUpButtonCell to handle the menu tracking. Even though the toolbar is drawn
// at 11pt, use the normal font size for these submenus. Not only do context menus use
// this size, it's easier on the eyes.
//
- (void)showFolderPopup:(NSEvent*)event
{
  BookmarkMenu* bmMenu = [[[BookmarkMenu alloc] initWithTitle:@"" bookmarkFolder:(BookmarkFolder*)[self bookmarkItem]] autorelease];
  // dummy first item
  id dummyItem = [bmMenu addItemWithTitle:@"" action:NULL keyEquivalent:@""];
  [bmMenu setItemBeforeCustomItems:dummyItem];

  // use a temporary NSPopUpButtonCell to display the menu.
  NSPopUpButtonCell *popupCell = [[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:YES];
  [popupCell setMenu:bmMenu];
  [popupCell trackMouse:event inRect:[self bounds] ofView:self untilMouseUp:YES];
  mLastEventWasMenu = YES;
  [popupCell release];
}

- (void)mouseDown:(NSEvent*)aEvent
{
  mLastEventWasMenu = NO;
  [super mouseDown:aEvent];
  if ([[self cell] lastClickHoldTimedOut])
    [self showFolderPopup:aEvent];
}

- (unsigned int)draggingSourceOperationMaskForLocal:(BOOL)localFlag
{
  if (localFlag)
    return (NSDragOperationCopy | NSDragOperationGeneric | NSDragOperationMove);

  return (NSDragOperationCopy | NSDragOperationGeneric | NSDragOperationLink | NSDragOperationDelete);
}

- (void)mouseDragged:(NSEvent*)aEvent
{
  // hack to prevent a drag while viewing a popup or context menu from moving the folder unexpectedly
  // (unless the drag happens on the folder itself)
  if (mLastEventWasMenu && ([self hitTest:[[self superview] convertPoint:[aEvent locationInWindow] fromView:nil]] == nil))
    return;

  BookmarkItem *item = [self bookmarkItem];
  BOOL isSingleBookmark = [item isKindOfClass:[Bookmark class]];
  NSPasteboard *pboard = [NSPasteboard pasteboardWithName:NSDragPboard];
  NSString     *title = [item title];
  if (isSingleBookmark) {
    [pboard declareURLPasteboardWithAdditionalTypes:[NSArray arrayWithObject:kCaminoBookmarkListPBoardType] owner:self];
    NSString *url = [(Bookmark *)item url];
    NSString *cleanedTitle = [title stringByReplacingCharactersInSet:[NSCharacterSet controlCharacterSet] withString:@" "];
    [pboard setDataForURL:url title:cleanedTitle];
  }
  else {
    [pboard declareTypes:[NSArray arrayWithObject:kCaminoBookmarkListPBoardType] owner:self];
  }

  // kCaminoBookmarkListPBoardType
  NSArray *pointerArray = [BookmarkManager serializableArrayWithBookmarkItems:[NSArray arrayWithObject:item]];
  [pboard setPropertyList:pointerArray forType:kCaminoBookmarkListPBoardType];

  // If the drag results in the bookmark button being (re)moved, it could get
  // deallocated too soon.  This occurs with SDK >= 10.3, but not earlier.
  // Change in cleanup strategy?  Hold on tight.
  [[self retain] autorelease];
  [self dragImage:[MainController createImageForDragging:[self image]
                                                   title:([item isSeparator] ? @"" : title)]
               at:NSMakePoint(0, NSHeight([self bounds]))
           offset:NSMakeSize(0, 0)
            event:aEvent
       pasteboard:pboard
           source:self
        slideBack:YES];
}

- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
{
  if (operation == NSDragOperationDelete) {
    NSPasteboard* pboard = [NSPasteboard pasteboardWithName:NSDragPboard];
    NSArray* bookmarks = [BookmarkManager bookmarkItemsFromSerializableArray:[pboard propertyListForType:kCaminoBookmarkListPBoardType]];
    if (bookmarks) {
      for (unsigned int i = 0; i < [bookmarks count]; ++i) {
        BookmarkItem* item = [bookmarks objectAtIndex:i];
        [[item parent] deleteChild:item];
      }
    }
  }
}

- (BOOL)acceptsFirstResponder
{
  return ![[self bookmarkItem] isSeparator];
}

@end
