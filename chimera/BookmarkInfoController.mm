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
*   David Haas  <haasd@cae.wisc.edu>
*/

#import "BookmarkInfoController.h"
#import "BookmarksDataSource.h"

#include "nsIContent.h"
#include "nsINamespaceManager.h"


@interface BookmarkInfoController(Private)

- (void)showUIElementPair: (id)aLabel control: (id) aControl;
- (void)hideUIElementPair: (id)aLabel control: (id) aControl;
- (void)commitChanges:(id)sender;
- (void)commitField:(id)textField toProperty:(nsIAtom*)propertyAtom;

@end;

@implementation BookmarkInfoController

-(id) init
{
  [super initWithWindowNibName:@"BookmarkInfoPanel"];

  //custom field editor lets us undo our changes
  mFieldEditor = [[NSTextView alloc] init];
  [mFieldEditor setAllowsUndo:YES];
  [mFieldEditor setFieldEditor:YES];
  
  return self;
}

-(id) initWithOutlineView: (id)aOutlineView
{
  mOutlineView = aOutlineView;
  
  return [self init];
}

-(void)dealloc
{
  [mFieldEditor release];
  [super dealloc];
}

-(void)controlTextDidEndEditing: (NSNotification*) aNotification
{
  [self commitChanges:[aNotification object]];
  [[mFieldEditor undoManager] removeAllActions];
}

-(void)windowDidResignKey:(NSNotification*) aNotification
{
  [self commitChanges:nil];
}

// if changedField is nil, commit everything
- (void)commitChanges:(id)changedField
{
  if (![mBookmarkItem contentNode])
    return;

  // Name
  if (!changedField || changedField == mNameField)
    [self commitField:mNameField toProperty:BookmarksService::gNameAtom];
  
  // Location
  if (!changedField || changedField == mLocationField)
    [self commitField:mLocationField toProperty:BookmarksService::gHrefAtom];

  // Keyword
  if (!changedField || changedField == mKeywordField)
    [self commitField:mKeywordField toProperty:BookmarksService::gKeywordAtom];

  // Description
  if (!changedField || changedField == mDescriptionField)
    [self commitField:mDescriptionField toProperty:BookmarksService::gDescriptionAtom];

  [[mFieldEditor undoManager] removeAllActions];
  [mOutlineView reloadItem: mBookmarkItem reloadChildren: NO];
  BookmarksService::BookmarkChanged([mBookmarkItem contentNode], TRUE);
}

- (void)commitField:(id)textField toProperty:(nsIAtom*)propertyAtom
{
  unsigned int len;
  PRUnichar* buffer;
  nsXPIDLString buf;

  // we really need a category on NSString for this
  len = [[textField stringValue] length];
  buffer = new PRUnichar[len + 1];
  if (!buffer) return;
  [[textField stringValue] getCharacters:buffer];
  buffer[len] = (PRUnichar)'\0';
  buf.Adopt(buffer);
  [mBookmarkItem contentNode]->SetAttr(kNameSpaceID_None, propertyAtom, buf, PR_TRUE);
}


-(void)setBookmark: (BookmarkItem*) aBookmark
{
  // See bug 154081 - don't show this window if Bookmark doesn't exist
  // after fix - this should never happen unless disaster strikes.
  if (![aBookmark contentNode])
    return;

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
  NSString* infoForString = [NSString stringWithFormat:NSLocalizedString(@"BookmarkInfoTitle",@"Info for "), bookmarkName];
  [[self window] setTitle: infoForString];

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

-(NSText *)windowWillReturnFieldEditor:(NSWindow *)aPanel toObject:(id)aObject
{
  return mFieldEditor;
}


@end
