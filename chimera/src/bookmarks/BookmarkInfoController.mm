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
*/

#import "BookmarkInfoController.h"
#import "BookmarksDataSource.h"

#include "nsIContent.h"
#include "nsINamespaceManager.h"

@implementation BookmarkInfoController

-(id) init
{
#if 0
  [mNameField         setDelegate: self];
  [mLocationField     setDelegate: self];
  [mKeywordField      setDelegate: self];
  [mDescriptionField  setDelegate: self];
#endif

  [super initWithWindowNibName:@"BookmarkInfoPanel"];

  return self;
}

-(id) initWithOutlineView: (id)aOutlineView
{
  mOutlineView = aOutlineView;
  
  return [self init];
}

-(void)windowDidLoad
{

}

-(void)controlTextDidEndEditing: (NSNotification*) aNotification
{
  if (![mBookmarkItem contentNode])
    return;
 
  unsigned int len;
  PRUnichar* buffer;
  nsXPIDLString buf;
  
  // Name
  len = [[mNameField stringValue] length];
  buffer = new PRUnichar[len + 1];
  if (!buffer) return;
  
  [[mNameField stringValue] getCharacters:buffer];
  buffer[len] = (PRUnichar)'\0';
  
  buf.Adopt(buffer);
  [mBookmarkItem contentNode]->SetAttr(kNameSpaceID_None, BookmarksService::gNameAtom, buf, PR_TRUE);

  // Location
  len = [[mLocationField stringValue] length];
  buffer = new PRUnichar[len + 1];
  if (!buffer) return;
  
  [[mLocationField stringValue] getCharacters:buffer];
  buffer[len] = (PRUnichar)'\0';
  
  buf.Adopt(buffer);
  [mBookmarkItem contentNode]->SetAttr(kNameSpaceID_None, BookmarksService::gHrefAtom, buf, PR_TRUE);
  
  // Keyword
  len = [[mKeywordField stringValue] length];
  buffer = new PRUnichar[len + 1];
  if (!buffer) return;
  
  [[mKeywordField stringValue] getCharacters:buffer];
  buffer[len] = (PRUnichar)'\0';
  
  buf.Adopt(buffer);
  [mBookmarkItem contentNode]->SetAttr(kNameSpaceID_None, BookmarksService::gKeywordAtom, buf, PR_TRUE);
  
  // Description
  len = [[mDescriptionField stringValue] length];
  buffer = new PRUnichar[len + 1];
  if (!buffer) return;
  
  [[mDescriptionField stringValue] getCharacters:buffer];
  buffer[len] = (PRUnichar)'\0';
  
  buf.Adopt(buffer);
  [mBookmarkItem contentNode]->SetAttr(kNameSpaceID_None, BookmarksService::gDescriptionAtom, buf, PR_TRUE);
  
  [mOutlineView reloadItem: mBookmarkItem reloadChildren: NO];
  BookmarksService::BookmarkChanged([mBookmarkItem contentNode], TRUE);  
}

-(void)setBookmark: (BookmarkItem*) aBookmark
{
  if (aBookmark) {
    nsAutoString group;
    [aBookmark contentNode]->GetAttr(kNameSpaceID_None, BookmarksService::gGroupAtom, group);
    BOOL isGroup = !group.IsEmpty();
    BOOL isFolder = !isGroup && [mOutlineView isExpandable: aBookmark];

    // First, Show/Hide the appropriate UI
    if (isGroup) {
      [self showUIElementPair: mNameLabel        control: mNameField];
      [self showUIElementPair: mKeywordLabel     control: mKeywordField];
      [self showUIElementPair: mDescriptionLabel control: mDescriptionField];
      [self hideUIElementPair: mLocationLabel    control: mLocationField];
    }
    else if (isFolder) {
      [self showUIElementPair: mNameLabel        control: mNameField];
      [self showUIElementPair: mDescriptionLabel control: mDescriptionField];
      [self hideUIElementPair: mKeywordLabel     control: mKeywordField];
      [self hideUIElementPair: mLocationLabel    control: mLocationField];
    }
    else {
      [self showUIElementPair: mNameLabel        control: mNameField];
      [self showUIElementPair: mDescriptionLabel control: mDescriptionField];
      [self showUIElementPair: mKeywordLabel     control: mKeywordField];
      [self showUIElementPair: mLocationLabel    control: mLocationField];
    }
  
    // Then, fill with appropriate values from Bookmarks
    nsAutoString value;
  
    [aBookmark contentNode]->GetAttr(kNameSpaceID_None, BookmarksService::gNameAtom, value);
    NSString* bookmarkName = [NSString stringWithCharacters: value.get() length: value.Length()];
    [mNameField setStringValue: bookmarkName];
    NSString* infoForString = [NSString stringWithCString: "Info for "];
    [[self window] setTitle: [infoForString stringByAppendingString: bookmarkName]];

    if (!isGroup && !isFolder) {
      [aBookmark contentNode]->GetAttr(kNameSpaceID_None, BookmarksService::gHrefAtom, value);
      [mLocationField setStringValue: [NSString stringWithCharacters: value.get() length: value.Length()]];
    }
    
    if (!isFolder) {
      [aBookmark contentNode]->GetAttr(kNameSpaceID_None, BookmarksService::gKeywordAtom, value);
      [mKeywordField setStringValue: [NSString stringWithCharacters: value.get() length: value.Length()]];
    }
    
    [aBookmark contentNode]->GetAttr(kNameSpaceID_None, BookmarksService::gDescriptionAtom, value);
    [mDescriptionField setStringValue: [NSString stringWithCharacters: value.get() length: value.Length()]];
  }
  else {
    [[self window] setTitle: [NSString stringWithCString: "Bookmark Info"]];
    
    [self hideUIElementPair: mNameLabel        control: mNameField];
    [self hideUIElementPair: mDescriptionLabel control: mDescriptionField];
    [self hideUIElementPair: mKeywordLabel     control: mKeywordField];
    [self hideUIElementPair: mLocationLabel    control: mLocationField];
  }
  
  mBookmarkItem = aBookmark;  
}

-(void)showUIElementPair: (id)aLabel control:(id)aControl
{
  if ([aLabel superview] == nil) {
    [[[self window] contentView] addSubview: aLabel];
    [aLabel autorelease];
  }
  if ([aControl superview] == nil) {
    [[[self window] contentView] addSubview: aControl];
    [aControl autorelease];
  }
}

-(void)hideUIElementPair: (id)aLabel control:(id)aControl
{
  if ([aLabel superview] != nil) {
    [aLabel removeFromSuperview];
    [aLabel retain];
  }
  if ([aControl superview] != nil) {
    [aControl removeFromSuperview];
    [aControl retain]; 
  }
}

@end
