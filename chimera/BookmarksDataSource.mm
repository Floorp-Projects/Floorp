/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *			Simon Fraser <sfraser@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#import "BookmarksDataSource.h"
#import "BookmarkInfoController.h"

#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsINamespaceManager.h"
#include "nsIPrefBranch.h"
#include "nsIServiceManager.h"

#include "nsVoidArray.h"

#import "BookmarksService.h"
#import "StringUtils.h"

@implementation BookmarksDataSource

-(id) init
{
    if ( (self = [super init]) ) {
        mBookmarks = nsnull;
        mCachedHref = nil;
    }
    return self;
}

-(void) windowClosing
{
  if (mBookmarks) {
    mBookmarks->RemoveObserver();
    delete mBookmarks;
  }
}

-(void) ensureBookmarks
{
    if (mBookmarks)
        return;
    
    mBookmarks = new BookmarksService(self);
    mBookmarks->AddObserver();
    
    [mOutlineView setTarget: self];
    [mOutlineView setDoubleAction: @selector(openBookmark:)];
    [mOutlineView setDeleteAction: @selector(deleteBookmarks:)];
    [mOutlineView reloadData];
}

-(IBAction)addBookmark:(id)aSender
{
  [self addBookmark: aSender useSelection: YES isFolder: NO URL:nil title:nil];
}

-(IBAction)addFolder:(id)aSender
{
  [self addBookmark: aSender useSelection: YES isFolder: YES URL:nil title:nil];
}

-(void)addBookmark:(id)aSender useSelection:(BOOL)aUseSel isFolder:(BOOL)aIsFolder URL:(NSString*)aURL title:(NSString*)aTitle
{
  if (!mBookmarks)
    return;

  // We use the selected item to determine the parent only if aUseSel is YES.
  BookmarkItem* item = nil;
  if (aUseSel && ([mOutlineView numberOfSelectedRows] == 1)) {
    // There is only one selected row.  If it is a folder, use it as our parent.
    // Otherwise, use our parent,
    int index = [mOutlineView selectedRow];
    item = [mOutlineView itemAtRow: index];
    if (![mOutlineView isExpandable: item]) {
      // We can't be used as the parent.  Try our parent.
      nsIContent* content = [item contentNode];
      if (!content)
        return;

      nsCOMPtr<nsIContent> parentContent;
      content->GetParent(*getter_AddRefs(parentContent));
      nsCOMPtr<nsIContent> root;
      mBookmarks->GetRootContent(getter_AddRefs(root));
      
      // The root has no item, so we don't need to do a lookup unless we
      // aren't the root.
      if (parentContent != root) {
        PRUint32 contentID;
        parentContent->GetContentID(&contentID);
        item = [(BookmarksService::gDictionary) objectForKey: [NSNumber numberWithInt: contentID]];
      }
    }
  }

  nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(mBookmarks->gBookmarks));
  
  nsAutoString title, href;
  if (!aIsFolder) {

    // If no URL and title were specified, get them from the current page.
    if (aURL && aTitle) {
      NSStringTo_nsString(aURL, href);
      NSStringTo_nsString(aTitle, title);
    } else {
      BookmarksService::GetTitleAndHrefForBrowserView([[mBrowserWindowController getBrowserWrapper] getBrowserView],
                                                      title, href);
    }

    mCachedHref = [NSString stringWithCharacters: href.get() length: href.Length()];
    [mCachedHref retain];

  } else {   // Folder
    mCachedHref = nil;
    title = NS_LITERAL_STRING("New Folder");
  }
  
  NSTextField* textField = [mBrowserWindowController getAddBookmarkTitle];
  [textField setStringValue: [NSString stringWithCharacters: title.get() length: title.Length()]];

  [mBrowserWindowController cacheBookmarkDS: self];

  // Show/hide the bookmark all tabs checkbox as appropriate.
  NSTabView* tabView = [mBrowserWindowController getTabBrowser];
  id checkbox = [mBrowserWindowController getAddBookmarkCheckbox];
  BOOL hasSuperview = [checkbox superview] != nil;
  if (aIsFolder && hasSuperview) {
    // Just don't show it at all.
    [checkbox removeFromSuperview];
    [checkbox retain];
  }
  else if (!aIsFolder && !hasSuperview) {
    // Put it back in.
    [[[mBrowserWindowController getAddBookmarkSheetWindow] contentView] addSubview: checkbox];
    [checkbox autorelease];
  }

  // Enable the bookmark all tabs checkbox if appropriate.
  if (!aIsFolder)
    [[mBrowserWindowController getAddBookmarkCheckbox] setEnabled: ([tabView numberOfTabViewItems] > 1)];
  
  // Build up the folder list.
  NSPopUpButton* popup = [mBrowserWindowController getAddBookmarkFolder];
  BookmarksService::ConstructAddBookmarkFolderList(popup, item);
  
  [NSApp beginSheet: [mBrowserWindowController getAddBookmarkSheetWindow]
     modalForWindow: [mBrowserWindowController window]
      modalDelegate: nil //self
     didEndSelector: nil //@selector(sheetDidEnd:)
        contextInfo: nil];
}

-(void)endAddBookmark: (int)aCode
{
  if (aCode == 0)
    return;

  BOOL isGroup = NO;
  id checkbox = [mBrowserWindowController getAddBookmarkCheckbox];
  if (([checkbox superview] != nil) && [checkbox isEnabled] && ([checkbox state] == NSOnState)) {
    mCachedHref = nil;
    isGroup = YES;
  }
  
  nsAutoString title;
  NSStringTo_nsString([[mBrowserWindowController getAddBookmarkTitle] stringValue], title);

  nsAutoString tagName;
  if (mCachedHref)
    tagName = NS_LITERAL_STRING("bookmark");
  else
    tagName = NS_LITERAL_STRING("folder");
  
  nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(mBookmarks->gBookmarks));
  nsCOMPtr<nsIDOMElement> elt;
  domDoc->CreateElementNS(NS_LITERAL_STRING("http://chimera.mozdev.org/bookmarks/"),
                          tagName,
                          getter_AddRefs(elt));

  elt->SetAttribute(NS_LITERAL_STRING("name"), title);

  if (mCachedHref) {
    nsAutoString href;
    NSStringTo_nsString(mCachedHref, href);
    [mCachedHref release];
    elt->SetAttribute(NS_LITERAL_STRING("href"), href);
  }

  if (isGroup) {
    // We have to iterate over each tab and create content nodes using the
    // title/href of all the pages.  They are inserted underneath the parent.
    elt->SetAttribute(NS_LITERAL_STRING("group"), NS_LITERAL_STRING("true"));
    id tabBrowser = [mBrowserWindowController getTabBrowser];
    int count = [tabBrowser numberOfTabViewItems];
    for (int i = 0; i < count; i++) {
      id browserView = [[[tabBrowser tabViewItemAtIndex: i] view] getBrowserView];
      nsAutoString title, href;
      BookmarksService::GetTitleAndHrefForBrowserView(browserView, title, href);
      nsCOMPtr<nsIDOMElement> childElt;
      domDoc->CreateElementNS(NS_LITERAL_STRING("http://chimera.mozdev.org/bookmarks/"),
                              NS_LITERAL_STRING("bookmark"),
                              getter_AddRefs(childElt));
      childElt->SetAttribute(NS_LITERAL_STRING("name"), title);
      childElt->SetAttribute(NS_LITERAL_STRING("href"), href);
      nsCOMPtr<nsIDOMNode> dummy;
      elt->AppendChild(childElt, getter_AddRefs(dummy));
    }
  }
  
  // Figure out the parent element.
  nsCOMPtr<nsIDOMElement> parentElt;
  nsCOMPtr<nsIContent> parentContent;
  NSPopUpButton* popup = [mBrowserWindowController getAddBookmarkFolder];
  NSMenuItem* selectedItem = [popup selectedItem];
  int tag = [selectedItem tag];
  if (tag == -1) {
    mBookmarks->GetRootContent(getter_AddRefs(parentContent));
    parentElt = do_QueryInterface(parentContent);
  }
  else {
    BookmarkItem* item = [(BookmarksService::gDictionary) objectForKey: [NSNumber numberWithInt: tag]];
    // Get the content node.
    parentContent = [item contentNode];
    parentElt = do_QueryInterface(parentContent);
  }
  
  nsCOMPtr<nsIDOMNode> dummy;
  parentElt->AppendChild(elt, getter_AddRefs(dummy));

  nsCOMPtr<nsIContent> childContent(do_QueryInterface(elt));
  mBookmarks->BookmarkAdded(parentContent, childContent);
}

-(IBAction)deleteBookmarks: (id)aSender
{
  if (!mBookmarks)
    return;

  int index = [mOutlineView selectedRow];
  if (index == -1)
    return;

  // first, see how many items are selected
  BOOL haveBookmarks = NO;
  
  NSEnumerator* testSelRows = [mOutlineView selectedRowEnumerator];
  for (NSNumber* currIndex = [testSelRows nextObject];
      currIndex != nil;
      currIndex = [testSelRows nextObject])
  {
    index = [currIndex intValue];
    BookmarkItem* item = [mOutlineView itemAtRow: index];
    if ([mOutlineView isExpandable: item]) {
      // dumb check to see if we're deleting an empty folder. Should really
      // recurse down
      if ([self outlineView:mOutlineView numberOfChildrenOfItem: item] > 0)
        haveBookmarks = YES;
    } else
      haveBookmarks = YES;
  }

  // ideally, we should count the number of doomed bookmarks and tell the user
  if (haveBookmarks) {
    NSString *alert         = NSLocalizedString(@"DeteleBookmarksAlert",@"");
    NSString *message       = NSLocalizedString(@"DeteleBookmarksMsg",@"");
    NSString *okButton      = NSLocalizedString(@"DeteleBookmarksOKButton",@"");
    NSString *cancelButton  = NSLocalizedString(@"DeteleBookmarksCancelButton",@"");
    if (NSRunAlertPanel(alert, message, okButton, cancelButton, nil) != NSAlertDefaultReturn)
      return;
  }
  
  // we'll run into problems if a parent item and one if its children are both selected.
  // A cheap way of having to avoid scanning the list to remove children is to have the
  // outliner collapse all items that are being deleted. This will cull the selection
  // for us and eliminate any children that happened to be selected.
  NSEnumerator* selRows = [mOutlineView selectedRowEnumerator];
  for (NSNumber* currIndex = [selRows nextObject];
      currIndex != nil;
      currIndex = [selRows nextObject]) {
    index = [currIndex intValue];
    BookmarkItem* item = [mOutlineView itemAtRow: index];
    [mOutlineView collapseItem: item];
  }

  // create array of items we need to delete. Deleting items out of of the
  // selection array is problematic for some reason.
  NSMutableArray* itemsToDelete = [[[NSMutableArray alloc] init] autorelease];
  selRows = [mOutlineView selectedRowEnumerator];
  for (NSNumber* currIndex = [selRows nextObject];
      currIndex != nil;
      currIndex = [selRows nextObject]) {
    index = [currIndex intValue];
    BookmarkItem* item = [mOutlineView itemAtRow: index];
    [itemsToDelete addObject: item];
  }

  // delete all bookmarks that are in our array
  int count = [itemsToDelete count];
  for (int i = 0; i < count; i++) {
    BookmarkItem* item = [itemsToDelete objectAtIndex: i];
    [self deleteBookmark: item];
  }

  // restore selection to location near last item deleted or last item
  int total = [mOutlineView numberOfRows];
  if (index >= total)
    index = total - 1;
  [mOutlineView selectRow: index byExtendingSelection: NO];
  // lame, but makes sure we catch all delete events in Info Panel
  [[NSNotificationCenter defaultCenter] postNotificationName:@"NSOutlineViewSelectionDidChangeNotification" object:mOutlineView];

}

-(void)deleteBookmark:(id)aItem
{
  nsCOMPtr<nsIContent> content = [aItem contentNode];
  nsCOMPtr<nsIDOMElement> child(do_QueryInterface(content));
  if (!child)
    return;
  if (child == BookmarksService::gToolbarRoot)
    return; // Don't allow the personal toolbar to be deleted.
  
  nsCOMPtr<nsIDOMNode> parent;
  child->GetParentNode(getter_AddRefs(parent));
  nsCOMPtr<nsIContent> parentContent(do_QueryInterface(parent));
  nsCOMPtr<nsIDOMNode> dummy;
  if (parent)
    parent->RemoveChild(child, getter_AddRefs(dummy));
  mBookmarks->BookmarkRemoved(parentContent, content);
}

-(IBAction)openBookmark: (id)aSender
{
  int index = [mOutlineView selectedRow];
  if (index == -1)
    return;

  id item = [mOutlineView itemAtRow: index];
  if (!item)
    return;

  nsIContent* content = [item contentNode];
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(content));
  nsAutoString group;
  content->GetAttr(kNameSpaceID_None, BookmarksService::gGroupAtom, group);
  if (!group.IsEmpty())
    mBookmarks->OpenBookmarkGroup([mBrowserWindowController getTabBrowser], elt);
  else if ([mOutlineView isExpandable: item]) {
    if ([mOutlineView isItemExpanded: item])
      [mOutlineView collapseItem: item];
    else
      [mOutlineView expandItem: item];
  }
  else {
    nsAutoString href;
    content->GetAttr(kNameSpaceID_None, BookmarksService::gHrefAtom, href);
    if (!href.IsEmpty()) {
      NSString* url = [NSString stringWithCharacters: href.get() length: href.Length()];
      [[[mBrowserWindowController getBrowserWrapper] getBrowserView] loadURI: url referrer:nil flags: NSLoadFlagsNone];
      // Focus and activate our content area.
      [[[mBrowserWindowController getBrowserWrapper] getBrowserView] setActive: YES];
    }
  }
}

-(NSString*) resolveKeyword: (NSString*) aKeyword
{
  return BookmarksService::ResolveKeyword(aKeyword);
}

//
// outlineView:shouldEditTableColumn:item: (delegate method)
//
// Called by the outliner to determine whether or not we should allow the 
// user to edit this item. For now, Cocoa doesn't correctly handle editing
// of attributed strings with icons, so we can't turn this on. :(
//
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldEditTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
  return NO;
}

- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item
{
    if (!mBookmarks)
        return nil;
       
    nsCOMPtr<nsIContent> content;
    if (!item)
        BookmarksService::GetRootContent(getter_AddRefs(content));
    else
        content = [item contentNode];
    
    nsCOMPtr<nsIContent> child;
    content->ChildAt(index, *getter_AddRefs(child));
    if ( child )
      return BookmarksService::GetWrapperFor(child);
    
    return nil;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
    if (!mBookmarks)
        return NO;
    
    if (!item)
        return YES; // The root node is always open.
    
    BOOL isExpandable = [item isFolder];

// XXXben - persistence of folder open state
// I'm adding this code, turned off, until I can figure out how to refresh the NSOutlineView's
// row count. Currently the items are expanded, but the outline view continues to believe it had
// the number of rows it had before the item was opened visible, until the view is resized. 
#if 0
    if (isExpandable) {
      PRBool isOpen = content->HasAttr(kNameSpaceID_None, BookmarksService::gOpenAtom);
      if (isOpen)
        [mOutlineView expandItem: item];
      else
        [mOutlineView collapseItem: item];
    }
#endif
    
    return isExpandable;
}

- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
    if (!mBookmarks)
        return 0;
  
    nsCOMPtr<nsIContent> content;
    if (!item)
        mBookmarks->GetRootContent(getter_AddRefs(content));
    else 
        content = [item contentNode];
    
    PRInt32 childCount;
    content->ChildCount(childCount);
    
    return childCount;
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
    NSString *columnName = [tableColumn identifier];
    NSMutableAttributedString *cellValue = [[NSMutableAttributedString alloc] init];
    NSFileWrapper *fileWrapper = [[NSFileWrapper alloc] initRegularFileWithContents:nil];
    NSTextAttachment *textAttachment = [[NSTextAttachment alloc] initWithFileWrapper:fileWrapper];
    NSMutableAttributedString *attachmentAttrString = nil;
    NSCell *attachmentAttrStringCell;

    if ([columnName isEqualToString: @"name"]) {
        nsIContent* content = [item contentNode];
        nsAutoString nameAttr;
        content->GetAttr(kNameSpaceID_None, BookmarksService::gNameAtom, nameAttr);
        
        //Set cell's textual contents
        [cellValue replaceCharactersInRange:NSMakeRange(0, [cellValue length]) withString:[NSString stringWithCharacters: nameAttr.get() length: nameAttr.Length()]];
        
        //Create an attributed string to hold the empty attachment, then release the components.
        attachmentAttrString = [[NSMutableAttributedString attributedStringWithAttachment:textAttachment] retain];
        [textAttachment release];
        [fileWrapper release];

        //Get the cell of the text attachment.
        attachmentAttrStringCell = (NSCell *)[(NSTextAttachment *)[attachmentAttrString attribute:NSAttachmentAttributeName atIndex:0 effectiveRange:nil] attachmentCell];
        //Figure out which image to add, and set the cell's image.
        // Use the bookmark groups image for groups.
        if ([self outlineView:outlineView isItemExpandable:item]) {
          nsIContent* content = [item contentNode];
          nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(content));
          nsAutoString group;
          content->GetAttr(kNameSpaceID_None, BookmarksService::gGroupAtom, group);
          if (!group.IsEmpty())
            [attachmentAttrStringCell setImage:[NSImage imageNamed:@"groupbookmark"]];
          else
            [attachmentAttrStringCell setImage:[NSImage imageNamed:@"folder"]];
        }
        else
          [attachmentAttrStringCell setImage:[NSImage imageNamed:@"smallbookmark"]];
        
        //Insert the image
        [cellValue replaceCharactersInRange:NSMakeRange(0, 0) withAttributedString:attachmentAttrString];
        
        //Tweak the baseline to vertically center the text.
        [cellValue addAttribute:NSBaselineOffsetAttributeName
                          value:[NSNumber numberWithFloat:-3.0]
                          range:NSMakeRange(0, 1)];
    }
    return cellValue;
}

- (void)outlineView:(NSOutlineView *)outlineView setObjectValue:(id)object forTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
#if NOT_USED
  // ignore all this. It doesn't work, but i'm leaving it here just in case we ever try to turn 
  // this code back on. We have to remove the attributes from the string in order to correctly
  // set it in the DOM.
  
  NSString *columnName = [tableColumn identifier];
  if ( [columnName isEqualTo:@"name"] ) {
    // remove the attributes
    int strLen = [object length];
    NSMutableAttributedString *cellValue = [[NSMutableAttributedString alloc] initWithAttributedString:object];
    [cellValue removeAttribute:NSBaselineOffsetAttributeName range:NSMakeRange(0,1)];
    [cellValue removeAttribute:NSAttachmentAttributeName range:NSMakeRange(0,strLen)];

    // extract the unicode
    strLen = [cellValue length];
    PRUnichar* buffer = new PRUnichar[strLen + 1];
    buffer[strLen] = '\0';
    if ( !buffer )
      return;
    [cellValue getCharacters: buffer];
    nsAutoString nameAttr;
    nameAttr.Adopt(buffer);
    
    // stash it into the dom.
    nsIContent* content = [item contentNode];
    content->SetAttr(kNameSpaceID_None, BookmarksService::gNameAtom, nameAttr, PR_TRUE);
    
    [cellValue release];
  }
#endif
}


- (BOOL)outlineView:(NSOutlineView *)ov writeItems:(NSArray*)items toPasteboard:(NSPasteboard*)pboard 
{
  if (!mBookmarks || [mOutlineView selectedRow] == -1) {
    return NO;
  }
 
#ifdef FILTER_DESCENDANT_ON_DRAG
  NSArray *toDrag = BookmarksService::FilterOutDescendantsForDrag(items);
#else
  NSArray *toDrag = items;
#endif
  int count = [toDrag count];
  if (count > 0) {
    // Create Pasteboard Data
    NSMutableArray *draggedID = [NSMutableArray arrayWithCapacity: count];
    
    for (int i = 0; i < count; i++)
      [draggedID addObject: [[toDrag objectAtIndex: i] contentID]];
    
    if (count == 1) {
      // if we have just one item, we add some more flavours
      [pboard declareTypes: [NSArray arrayWithObjects:
          @"MozBookmarkType", NSURLPboardType, NSStringPboardType, nil] owner: self];
      [pboard setPropertyList: draggedID forType: @"MozBookmarkType"];

      NSString* itemURL = [[toDrag objectAtIndex: 0] url];
      [pboard setString:itemURL forType: NSStringPboardType];
      [[NSURL URLWithString:itemURL] writeToPasteboard: pboard];
      // maybe construct the @"MozURLType" type here also
    }
    else {
      // multiple bookmarks. Array sof strings or NSURLs seem to
      // confuse receivers. Not sure what the correct way is.
      [pboard declareTypes: [NSArray arrayWithObject: @"MozBookmarkType"] owner: self];
      [pboard setPropertyList: draggedID 		forType: @"MozBookmarkType"];
    }

    return YES;
  }

  return NO;
}


- (NSDragOperation)outlineView:(NSOutlineView*)ov validateDrop:(id <NSDraggingInfo>)info proposedItem:(id)item proposedChildIndex:(int)index 
{
  NSArray* types = [[info draggingPasteboard] types];

  //  if the index is -1, deny the drop
  if (index == NSOutlineViewDropOnItemIndex)
    return NSDragOperationNone;

  if ([types containsObject: @"MozBookmarkType"]) {
    NSArray *draggedIDs = [[info draggingPasteboard] propertyListForType: @"MozBookmarkType"];
    BookmarkItem* parent;
    parent = (item) ? item : BookmarksService::GetRootItem();
    return (BookmarksService::IsBookmarkDropValid(parent, index, draggedIDs)) ? NSDragOperationGeneric : NSDragOperationNone;
  } else if ([types containsObject: @"MozURLType"]) {
    return NSDragOperationGeneric;
  }

  return NSDragOperationNone;
}

- (BOOL)outlineView:(NSOutlineView*)ov acceptDrop:(id <NSDraggingInfo>)info item:(id)item childIndex:(int)index {
  NSArray *types = [[info draggingPasteboard] types];
  BookmarkItem* parent = (item) ? item : BookmarksService::GetRootItem();

  if ([types containsObject: @"MozBookmarkType"]) {
    NSArray *draggedItems = [[info draggingPasteboard] propertyListForType: @"MozBookmarkType"];
    BookmarksService::PerformBookmarkDrop(parent, index, draggedItems);
    return YES;
  }
  else if ([types containsObject: @"MozURLType"]) {
    NSDictionary* proxy = [[info draggingPasteboard] propertyListForType: @"MozURLType"];    
    BookmarkItem* beforeItem = [self outlineView:ov child:index ofItem:item];
    return BookmarksService::PerformProxyDrop(parent, beforeItem, proxy);
  }

  return NO;
}

- (NSString *)outlineView:(NSOutlineView *)outlineView tooltipStringForItem:(id)item
{
  NSString* descStr = nil;
  NSString* hrefStr = nil;
  nsIContent* content = [item contentNode];
  nsAutoString value;

  content->GetAttr(kNameSpaceID_None, BookmarksService::gDescriptionAtom, value);
  if (value.Length())
    descStr = [NSString stringWithCharacters:value.get() length:value.Length()];

  // Only description for folders
  if ([item isFolder])
    return descStr;
  
  // Extract the URL from the item
  content->GetAttr(kNameSpaceID_None, BookmarksService::gHrefAtom, value);
  if (value.Length())
    hrefStr = [NSString stringWithCharacters:value.get() length:value.Length()];

  if (!hrefStr)
    return descStr;
  else if (!descStr)
    return hrefStr;

  // Display both URL and description
  return [NSString stringWithFormat:@"%@\n%@", hrefStr, descStr];
}

/*
- (NSMenu *)outlineView:(NSOutlineView *)outlineView contextMenuForItem:(id)item
{
 // TODO - return (custom?) context menu for item here.
 // Note that according to HIG, there should never be disabled items in
 // a context menu - instead, items that do not apply should be removed.
 // We could nicely do that here.
}
*/

- (void)reloadDataForItem:(id)item reloadChildren: (BOOL)aReloadChildren
{
  if (!item)
    [mOutlineView reloadData];
  else
    [mOutlineView reloadItem: item reloadChildren: aReloadChildren];
}

-(IBAction)openBookmarkInNewTab:(id)aSender
{
  int index = [mOutlineView selectedRow];
  if (index == -1)
    return;
  if ([mOutlineView numberOfSelectedRows] == 1) {
    nsCOMPtr<nsIPrefBranch> pref(do_GetService("@mozilla.org/preferences-service;1"));
    if (!pref)
        return; // Something bad happened if we can't get prefs.

    BookmarkItem* item = [mOutlineView itemAtRow: index];
    nsAutoString hrefAttr;
    [item contentNode]->GetAttr(kNameSpaceID_None, BookmarksService::gHrefAtom, hrefAttr);
  
    // stuff it into the string
    NSString* hrefStr = [NSString stringWithCharacters:hrefAttr.get() length:hrefAttr.Length()];

    PRBool loadInBackground;
    pref->GetBoolPref("browser.tabs.loadInBackground", &loadInBackground);

    [mBrowserWindowController openNewTabWithURL: hrefStr referrer:nil loadInBackground: loadInBackground];
  }
}

-(IBAction)openBookmarkInNewWindow:(id)aSender
{
  int index = [mOutlineView selectedRow];
  if (index == -1)
    return;
  if ([mOutlineView numberOfSelectedRows] == 1) {
    nsCOMPtr<nsIPrefBranch> pref(do_GetService("@mozilla.org/preferences-service;1"));
    if (!pref)
        return; // Something bad happened if we can't get prefs.

    BookmarkItem* item = [mOutlineView itemAtRow: index];
    nsAutoString hrefAttr;
    [item contentNode]->GetAttr(kNameSpaceID_None, BookmarksService::gHrefAtom, hrefAttr);
  
    // stuff it into the string
    NSString* hrefStr = [NSString stringWithCharacters:hrefAttr.get() length:hrefAttr.Length()];

    PRBool loadInBackground;
    pref->GetBoolPref("browser.tabs.loadInBackground", &loadInBackground);

    nsAutoString group;
    [item contentNode]->GetAttr(kNameSpaceID_None, BookmarksService::gGroupAtom, group);
    if (group.IsEmpty()) 
      [mBrowserWindowController openNewWindowWithURL: hrefStr referrer: nil loadInBackground: loadInBackground];
    else {
      nsCOMPtr<nsIDOMElement> elt(do_QueryInterface([item contentNode]));
      [mBrowserWindowController openNewWindowWithGroup: elt loadInBackground: loadInBackground];
    }
  }
}

-(void)openBookmarkGroup:(id)aTabView groupElement:(nsIDOMElement*)aFolder
{
  mBookmarks->OpenBookmarkGroup(aTabView, aFolder);
}

-(IBAction)showBookmarkInfo:(id)aSender
{
  BookmarkInfoController *bic = [BookmarkInfoController sharedBookmarkInfoController]; 

  int index = [mOutlineView selectedRow];
  BookmarkItem* item = [mOutlineView itemAtRow: index];
  [bic setBookmark:item];

  [bic showWindow:bic];
}

-(void)outlineViewSelectionDidChange: (NSNotification*) aNotification
{
  int index = [mOutlineView selectedRow];
  if (index == -1) {
    [mEditBookmarkButton setEnabled:NO];
    [mDeleteBookmarkButton setEnabled:NO];
  }
  else {
    BookmarkInfoController *bic = [BookmarkInfoController sharedBookmarkInfoController]; 

    [mEditBookmarkButton setEnabled:YES];
    [mDeleteBookmarkButton setEnabled:YES];
    if ([[bic window] isVisible]) 
      [bic setBookmark:[mOutlineView itemAtRow:index]];
  }
}

-(BOOL)validateMenuItem:(NSMenuItem*)aMenuItem
{
  int index = [mOutlineView selectedRow];
  if (index == -1)
    return NO;

  BookmarkItem* item = [mOutlineView itemAtRow: index];
  BOOL isBookmark = [mOutlineView isExpandable:item] == NO;
  
  nsAutoString group;
  [item contentNode]->GetAttr(kNameSpaceID_None, BookmarksService::gGroupAtom, group);
  BOOL isGroup = !group.IsEmpty();

  if (([aMenuItem action] == @selector(openBookmarkInNewWindow:))) {
    // Bookmarks and Bookmark Groups can be opened in a new window
    return (isBookmark || isGroup);
  }
  else if (([aMenuItem action] == @selector(openBookmarkInNewTab:))) {
    // Only Bookmarks can be opened in new tabs
    return isBookmark;
  }
  return YES;
}

- (void)outlineViewItemWillExpand:(NSNotification *)notification
{
  BookmarkItem* item = [[notification userInfo] objectForKey:[[[notification userInfo] allKeys] objectAtIndex: 0]];
  [item contentNode]->SetAttr(kNameSpaceID_None, BookmarksService::gOpenAtom, NS_LITERAL_STRING("true"), PR_FALSE);
}

- (void)outlineViewItemWillCollapse:(NSNotification *)notification
{
  BookmarkItem* item = [[notification userInfo] objectForKey:[[[notification userInfo] allKeys] objectAtIndex: 0]];
  [item contentNode]->UnsetAttr(kNameSpaceID_None, BookmarksService::gOpenAtom, PR_FALSE);
}

@end

@implementation BookmarkItem
-(nsIContent*)contentNode
{
  return mContentNode;
}

- (NSNumber*)contentID
{
  PRUint32 contentID = 0;
  mContentNode->GetContentID(&contentID);
  return [NSNumber numberWithInt: contentID];
}

- (NSString *)description
{
  nsCOMPtr<nsIContent> item = [self contentNode];
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(item));
  nsAutoString href;
  element->GetAttribute(NS_LITERAL_STRING("name"), href);
  NSString* info = [NSString stringWithCharacters: href.get() length: href.Length()];
  return [NSString stringWithFormat:@"<BookmarkItem, name = \"%@\">", info];
}

- (NSString *)url
{
  nsCOMPtr<nsIContent> item = [self contentNode];
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(item));
  nsAutoString href;
  element->GetAttribute(NS_LITERAL_STRING("href"), href);
  return [NSString stringWithCharacters: href.get() length: href.Length()];
}

-(void)setContentNode: (nsIContent*)aContentNode
{
  mContentNode = aContentNode;
}

- (id)copyWithZone:(NSZone *)aZone
{
  BookmarkItem* copy = [[[self class] allocWithZone: aZone] init];
  [copy setContentNode: mContentNode];
  return copy;
}

- (BOOL)isFolder
{
    nsCOMPtr<nsIAtom> tagName;
    mContentNode->GetTag(*getter_AddRefs(tagName));

    return (tagName == BookmarksService::gFolderAtom);
}

@end

