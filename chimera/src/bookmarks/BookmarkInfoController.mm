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

#import "NSString+Utils.h"

#import "BookmarkInfoController.h"

#include "nsIContent.h"
#include "nsINamespaceManager.h"


@interface BookmarkInfoController(Private)

- (void)showUIElementPair: (id)aLabel control: (id) aControl;
- (void)hideUIElementPair: (id)aLabel control: (id) aControl;
- (void)commitChanges:(id)sender;
- (BOOL)commitField:(id)textField toProperty:(nsIAtom*)propertyAtom;

@end;

@implementation BookmarkInfoController

/* BookmarkInfoController singelton */
static BookmarkInfoController *sharedBookmarkInfoController = nil;

+ (id)sharedBookmarkInfoController
{
  if (!sharedBookmarkInfoController) {
    sharedBookmarkInfoController = [[BookmarkInfoController alloc] initWithWindowNibName:@"BookmarkInfoPanel"];
  }
  
  return sharedBookmarkInfoController;
}

+ (void)closeBookmarkInfoController
{
  if (sharedBookmarkInfoController)
    [sharedBookmarkInfoController close];
}

- (id)initWithWindowNibName:(NSString *)windowNibName
{
  if ((self = [super initWithWindowNibName:@"BookmarkInfoPanel"]))
  {
    //custom field editor lets us undo our changes
    mFieldEditor = [[NSTextView alloc] init];
    [mFieldEditor setAllowsUndo:YES];
    [mFieldEditor setFieldEditor:YES];
    
  }
  return self;
}

- (void)awakeFromNib
{
  // keep a ref so that we can remove and add to the its superview with impunity
  [mNameField retain];
  [mLocationField retain];
  [mKeywordField retain];
  [mDescriptionField retain];
  [mNameLabel retain];
  [mLocationLabel retain];
  [mKeywordLabel retain];
  [mDescriptionLabel retain];
  [mDockMenuCheckbox retain];
  [mTabgroupCheckbox retain];
  
  [[BookmarksManager sharedBookmarksManager] addBookmarksClient:self];
}

-(void)dealloc
{
  // this is never called
  if (self == sharedBookmarkInfoController)
    sharedBookmarkInfoController = nil;

  [[BookmarksManager sharedBookmarksManagerDontAlloc] removeBookmarksClient:self];

  [mFieldEditor release];

  [mNameField release];
  [mLocationField release];
  [mKeywordField release];
  [mDescriptionField release];
  [mNameLabel release];
  [mLocationLabel release];
  [mKeywordLabel release];
  [mDescriptionLabel release];
  [mDockMenuCheckbox release];
  [mTabgroupCheckbox release];
  
  [super dealloc];
}

-(void)controlTextDidEndEditing: (NSNotification*) aNotification
{
  [self commitChanges:[aNotification object]];
  [[mFieldEditor undoManager] removeAllActions];
}

-(void)windowDidBecomeKey:(NSNotification*) aNotification
{
  [[self window] makeFirstResponder:mNameField];
}

-(void)windowDidResignKey:(NSNotification*) aNotification
{
  [[self window] makeFirstResponder:[self window]];
	if (![[self window] isVisible])
    mBookmarkItem = nil;
}

- (void)windowWillClose:(NSNotification *)aNotification
{
  [self commitChanges:nil];
  mBookmarkItem = nil;
}

- (void)commitChanges:(id)changedField
{
  if (![mBookmarkItem contentNode])
    return;

  BOOL changed = NO;
  // Name
  if ((!changedField && [mNameField superview]) || changedField == mNameField)
    changed |= [self commitField:mNameField toProperty:BookmarksService::gNameAtom];
  
  // Location
  if ((!changedField && [mLocationField superview]) || changedField == mLocationField)
    changed |= [self commitField:mLocationField toProperty:BookmarksService::gHrefAtom];

  // Keyword
  if ((!changedField && [mKeywordField superview]) || changedField == mKeywordField)
    changed |= [self commitField:mKeywordField toProperty:BookmarksService::gKeywordAtom];

  // Description
  if ((!changedField && [mDescriptionField superview]) || changedField == mDescriptionField)
    changed |= [self commitField:mDescriptionField toProperty:BookmarksService::gDescriptionAtom];

  [[mFieldEditor undoManager] removeAllActions];
  if (changed)
    [mBookmarkItem itemChanged:YES];
}

// return YES if changed
- (BOOL)commitField:(id)textField toProperty:(nsIAtom*)propertyAtom
{
  nsAutoString attributeString;
  [[textField stringValue] assignTo_nsAString:attributeString];
  
  nsAutoString oldAttribValue;
  [mBookmarkItem contentNode]->GetAttr(kNameSpaceID_None, propertyAtom, oldAttribValue);
  
  if (!attributeString.Equals(oldAttribValue))
  {
    [mBookmarkItem contentNode]->SetAttr(kNameSpaceID_None, propertyAtom, attributeString, PR_TRUE);
    return YES;
  }

  return NO;
}

- (IBAction)dockMenuCheckboxClicked:(id)sender
{
  if ([sender state] == NSOnState)
    BookmarksService::SetDockMenuRoot([mBookmarkItem contentNode]);
  else
    BookmarksService::SetDockMenuRoot(NULL);
}

- (IBAction)tabGroupCheckboxClicked:(id)sender
{
  if ([sender state] == NSOnState)
    [mBookmarkItem contentNode]->SetAttr(kNameSpaceID_None, BookmarksService::gGroupAtom, NS_LITERAL_STRING("true"), PR_TRUE);
  else
    [mBookmarkItem contentNode]->UnsetAttr(kNameSpaceID_None, BookmarksService::gGroupAtom, PR_TRUE);
  [mBookmarkItem itemChanged:YES];
}

-(void)setBookmark: (BookmarkItem*) aBookmark
{
  // See bug 154081 - don't show this window if Bookmark doesn't exist
  // after fix - this should never happen unless disaster strikes.
  if (![aBookmark contentNode])
    return;

  BOOL isGroup  = [aBookmark isGroup];
  BOOL isFolder = !isGroup && [aBookmark isFolder];

  // First, Show/Hide the appropriate UI
  if (isGroup) 
  {
    [self showUIElementPair: mNameLabel        control: mNameField];
    [self hideUIElementPair: mLocationLabel    control: mLocationField];
    [self showUIElementPair: mKeywordLabel     control: mKeywordField];
    [self showUIElementPair: mDescriptionLabel control: mDescriptionField];

    [mVariableFieldsContainer addSubview:mTabgroupCheckbox];
    [mVariableFieldsContainer addSubview:mDockMenuCheckbox];
    [mNameField        setNextKeyView:mTabgroupCheckbox];
    [mTabgroupCheckbox setNextKeyView:mDockMenuCheckbox];
    [mDockMenuCheckbox setNextKeyView:mKeywordField];
    [mKeywordField     setNextKeyView:mDescriptionField];

    [mTabgroupCheckbox setEnabled:![aBookmark isToobarRoot]];
  }
  else if (isFolder)
  {
    [self showUIElementPair: mNameLabel        control: mNameField];
    [self hideUIElementPair: mLocationLabel    control: mLocationField];
    [self hideUIElementPair: mKeywordLabel     control: mKeywordField];
    [self showUIElementPair: mDescriptionLabel control: mDescriptionField];

    [mVariableFieldsContainer addSubview:mDockMenuCheckbox];
    [mVariableFieldsContainer addSubview:mTabgroupCheckbox];
    [mNameField        setNextKeyView:mDockMenuCheckbox];
    [mDockMenuCheckbox setNextKeyView:mTabgroupCheckbox];
    [mTabgroupCheckbox setNextKeyView:mDescriptionField];
    
    [mTabgroupCheckbox setEnabled:![aBookmark isToobarRoot]];
  }
  else
  {
    [self showUIElementPair: mNameLabel        control: mNameField];
    [self showUIElementPair: mLocationLabel    control: mLocationField];
    [self showUIElementPair: mKeywordLabel     control: mKeywordField];
    [self showUIElementPair: mDescriptionLabel control: mDescriptionField];

    [mDockMenuCheckbox removeFromSuperview];
    [mTabgroupCheckbox removeFromSuperview];
    [mNameField 			setNextKeyView:mLocationField];
    [mLocationField 	setNextKeyView:mKeywordField];
    [mKeywordField    setNextKeyView:mDescriptionField];
  }
  
  // Then, fill with appropriate values from Bookmarks
  NSString* bookmarkName = [aBookmark name];
  NSString* infoForString = [NSString stringWithFormat:NSLocalizedString(@"BookmarkInfoTitle", @"Info for "), bookmarkName];
  [[self window] setTitle: infoForString];

  if (isGroup)
  {
    [mDockMenuCheckbox setState:([aBookmark isDockMenuRoot] ? NSOnState : NSOffState)];
    [mTabgroupCheckbox setState:NSOnState];
    [mKeywordField setStringValue: [aBookmark keyword]];
  }
  else if (isFolder)
  {
    [mDockMenuCheckbox setState:([aBookmark isDockMenuRoot] ? NSOnState : NSOffState)];
    [mTabgroupCheckbox setState:NSOffState];
  }
  else
  {
    [mKeywordField setStringValue: [aBookmark keyword]];
    [mLocationField setStringValue: [aBookmark url]];
  }
  
  [mNameField setStringValue: bookmarkName];
  [mDescriptionField setStringValue: [aBookmark descriptionString]];
  
  mBookmarkItem = aBookmark;  
}

-(BookmarkItem *)bookmark
{
  return mBookmarkItem;
}

-(void)showUIElementPair: (id)aLabel control:(id)aControl
{
  if ([aLabel superview] == nil)
    [mVariableFieldsContainer addSubview: aLabel];

  if ([aControl superview] == nil)
    [mVariableFieldsContainer addSubview: aControl];
    
  // we need to resize the fields in case the user resized the window when they were hidden
  NSRect containerBounds = [mVariableFieldsContainer bounds];
  NSRect controlFrame    = [aControl frame];
  controlFrame.size.width = (containerBounds.size.width - controlFrame.origin.x - 20.0);
  [aControl setFrame:controlFrame];
}

-(void)hideUIElementPair: (id)aLabel control:(id)aControl
{
  if ([aLabel superview] != nil)
    [aLabel removeFromSuperview];

  if ([aControl superview] != nil)
    [aControl removeFromSuperview];
}

-(NSText *)windowWillReturnFieldEditor:(NSWindow *)aPanel toObject:(id)aObject
{
  return mFieldEditor;
}

#pragma mark -

- (void)bookmarkAdded:(nsIContent*)bookmark inContainer:(nsIContent*)container isChangedRoot:(BOOL)isRoot
{
}

- (void)bookmarkRemoved:(nsIContent*)bookmark inContainer:(nsIContent*)container isChangedRoot:(BOOL)isRoot
{
  if ([mBookmarkItem contentNode] == bookmark)
    mBookmarkItem = nil;
}

- (void)bookmarkChanged:(nsIContent*)bookmark
{
}

- (void)specialFolder:(EBookmarksFolderType)folderType changedTo:(nsIContent*)newFolderContent
{
}

@end
