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

#import "NSString+Utils.h"
#import "CHBookmarksButton.h"

#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsINamespaceManager.h"
#include "nsIPrefBranch.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsCRT.h"

#import "BookmarkInfoController.h"
#import "BookmarksDataSource.h"
#import "BookmarksService.h"

@implementation CHBookmarksButton

- (id)initWithFrame:(NSRect)frame
{
    if ( (self = [super initWithFrame:frame]) ) {
        [self setBezelStyle: NSRegularSquareBezelStyle];
        [self setButtonType: NSMomentaryChangeButton];
        [self setBordered: NO];
        [self setImagePosition: NSImageLeft];
        [self setRefusesFirstResponder: YES];
        [self setFont: [NSFont labelFontOfSize: 11.0]];
    }
  return self;
}

-(id)initWithFrame:(NSRect)frame element:(nsIDOMElement*)element bookmarksService:(BookmarksService*)bookmarksService
{
  if ( (self = [self initWithFrame:frame]) ) {
      mBookmarksService = bookmarksService;
      [self setElement:element];
  }
  return self;
}

-(IBAction)openBookmark:(id)aSender
{
  // See if we're a group.
  nsAutoString group;
  mElement->GetAttribute(NS_LITERAL_STRING("group"), group);
  if (!group.IsEmpty()) {
    BookmarksService::OpenBookmarkGroup([[[self window] windowController] getTabBrowser], mElement);
    return;
  }
  
  // Get the href attribute.  This is the URL we want to load.
  nsAutoString href;
  mElement->GetAttribute(NS_LITERAL_STRING("href"), href);
  if (href.IsEmpty())
    return;
  NSString* url = [NSString stringWith_nsAString: href];

  // Now load the URL in the window.
  BrowserWindowController* brController = [[self window] windowController];
  [brController loadURL: url referrer:nil activate:YES];
}

-(IBAction)openBookmarkInNewTab:(id)aSender
{
  nsCOMPtr<nsIPrefBranch> pref(do_GetService("@mozilla.org/preferences-service;1"));
  if (!pref)
   return; // Something bad happened if we can't get prefs.

  // Get the href attribute.  This is the URL we want to load.
  nsAutoString hrefAttr;
  mElement->GetAttribute(NS_LITERAL_STRING("href"), hrefAttr);
  NSString* hrefStr = [NSString stringWith_nsAString:hrefAttr];

  PRBool loadInBackground;
  pref->GetBoolPref("browser.tabs.loadInBackground", &loadInBackground);

  BrowserWindowController* brController = [[self window] windowController];
  [brController openNewTabWithURL: hrefStr referrer:nil loadInBackground: loadInBackground];
}

-(IBAction)openBookmarkInNewWindow:(id)aSender
{
  nsCOMPtr<nsIPrefBranch> pref(do_GetService("@mozilla.org/preferences-service;1"));
  if (!pref)
    return; // Something bad happened if we can't get prefs.

  // Get the href attribute.  This is the URL we want to load.
  nsAutoString hrefAttr;
  mElement->GetAttribute(NS_LITERAL_STRING("href"), hrefAttr);
  NSString* hrefStr = [NSString stringWith_nsAString:hrefAttr];

  PRBool loadInBackground;
  pref->GetBoolPref("browser.tabs.loadInBackground", &loadInBackground);

  nsAutoString group;
  mElement->GetAttribute(NS_LITERAL_STRING("group"), group);

  BrowserWindowController* brController = [[self window] windowController];
  if (group.IsEmpty()) 
    [brController openNewWindowWithURL: hrefStr referrer: nil loadInBackground: loadInBackground];
  else
    [brController openNewWindowWithGroup: mElement loadInBackground: loadInBackground];
}

-(IBAction)showBookmarkInfo:(id)aSender
{
  BookmarkInfoController *bic = [BookmarkInfoController sharedBookmarkInfoController]; 

  [bic showWindow:bic];
  [bic setBookmark:mBookmarkItem];
}

-(IBAction)deleteBookmarks: (id)aSender
{
  if (mElement == BookmarksService::gToolbarRoot)
    return; // Don't allow the personal toolbar to be deleted.
  nsCOMPtr<nsIContent> content(do_QueryInterface(mElement));
  
  nsCOMPtr<nsIDOMNode> parent;
  mElement->GetParentNode(getter_AddRefs(parent));
  nsCOMPtr<nsIContent> parentContent(do_QueryInterface(parent));
  nsCOMPtr<nsIDOMNode> dummy;
  if (parent)
    parent->RemoveChild(mElement, getter_AddRefs(dummy));
  BookmarksService::BookmarkRemoved(parentContent, content);
}

-(IBAction)addFolder:(id)aSender
{
  // TODO
}

-(void)drawRect:(NSRect)aRect
{
  [super drawRect: aRect];
}

-(NSMenu*)menuForEvent:(NSEvent*)aEvent
{
  // Make a copy of the context menu but change it to target us
  // FIXME - this will stop to work as soon as we add items to the context menu
  // that have different targets. In that case, we probably should add code to
  // CHBookmarksToolbar that manages the context menu for the CHBookmarksButtons.
  
  NSMenu* myMenu = [[[self superview] menu] copy];
  [[myMenu itemArray] makeObjectsPerformSelector:@selector(setTarget:) withObject: self];
  [myMenu update];
  return [myMenu autorelease];
}

-(BOOL)validateMenuItem:(NSMenuItem*)aMenuItem
{
  if (!mBookmarkItem)
    return NO;
  BOOL isBookmark = [mBookmarkItem isFolder] == NO;
  
  nsAutoString group;
  mElement->GetAttribute(NS_LITERAL_STRING("group"), group);
  BOOL isGroup = !group.IsEmpty();

  if (([aMenuItem action] == @selector(openBookmarkInNewWindow:))) {
    // Bookmarks and Bookmark Groups can be opened in a new window
    return (isBookmark || isGroup);
  }
  else if (([aMenuItem action] == @selector(openBookmarkInNewTab:))) {
    // Only Bookmarks can be opened in new tabs
    BrowserWindowController* brController = [[self window] windowController];
    return isBookmark && [brController newTabsAllowed];
  }
  return YES;
}

-(void)mouseDown:(NSEvent*)aEvent
{
  // pop up a "context menu" on folders showing their contents. we check
  // for single click to fix issues with dblclicks (bug 162367)
  if (mIsFolder && [aEvent clickCount] == 1) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(mElement));
    NSMenu* menu = BookmarksService::LocateMenu(content);
    [NSMenu popUpContextMenu: menu withEvent: aEvent forView: self];
  }
  else
    [super mouseDown:aEvent];
}

-(void)setElement: (nsIDOMElement*)aElt
{
  mElement = aElt;
  nsAutoString tag;
  mElement->GetLocalName(tag);

  NSImage* bookmarkImage = mBookmarksService->CreateIconForBookmark(aElt);

  nsAutoString group;
  mElement->GetAttribute(NS_LITERAL_STRING("group"), group);
  
  if (!group.IsEmpty()) {
    mIsFolder = NO;
    [self setImage: bookmarkImage];
    [self setAction: @selector(openBookmark:)];
    [self setTarget: self];
  }
  else if (tag.Equals(NS_LITERAL_STRING("folder"))) {
    [self setImage: bookmarkImage];
    mIsFolder = YES;
  }
  else {
    mIsFolder = NO;
    [self setImage: bookmarkImage];
    [self setAction: @selector(openBookmark:)];
    [self setTarget: self];
    nsAutoString href;
    mElement->GetAttribute(NS_LITERAL_STRING("href"), href);
    NSString* helpText = [NSString stringWith_nsAString:href];
    [self setToolTip: helpText];
  }
  
  nsAutoString name;
  mElement->GetAttribute(NS_LITERAL_STRING("name"), name);
  [self setTitle: [NSString stringWith_nsAString: name]];
  
  nsCOMPtr<nsIContent> content(do_QueryInterface(mElement));
  mBookmarkItem = BookmarksService::GetWrapperFor(content);
}

-(nsIDOMElement*)element
{
  return mElement;
}

- (unsigned int)draggingSourceOperationMaskForLocal:(BOOL)flag
{
    return NSDragOperationGeneric;
}

- (void) mouseDragged: (NSEvent*) aEvent
{
  // XXX mouseDragged is never called because buttons cancel dragging while you mouse down 
  //     I have to fix this in an ugly way, by doing the "click" stuff myself and never relying
  //     on the superclass for that.  Bah!

  //  perhaps you could just implement mouseUp to perform the action (which should be the case
  //  things shouldn't happen on mouse down)  Then does mouseDragged get overridden?

  //   BookmarksService::DragBookmark(mElement, self, aEvent);
}

@end
