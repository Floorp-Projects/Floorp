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

#import "NSString+Utils.h"
#import "NSPasteboard+Utils.h"

#import "BrowserWindowController.h"
#import "HistoryDataSource.h"
#import "CHBrowserView.h"

#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFResource.h"
#include "nsIBrowserHistory.h"
#include "nsIServiceManager.h"

#include "nsXPIDLString.h"
#include "nsString.h"

#include "nsComponentManagerUtils.h"

//
// class HistoryDataSourceObserver
//
// An observer, enabled whenver the history panel in the drawer is selected. Tracks
// changes to the RDF data source and pokes the history outline view
//

class HistoryDataSourceObserver : public nsIRDFObserver
{
public:
  HistoryDataSourceObserver(HistoryDataSource* dataSource)
  : mHistoryDataSource(dataSource)
  { 
    NS_INIT_ISUPPORTS();
  }
  virtual ~HistoryDataSourceObserver() { }
  
  NS_DECL_ISUPPORTS

  NS_IMETHOD OnAssert(nsIRDFDataSource*, nsIRDFResource*, nsIRDFResource*, nsIRDFNode*);
  NS_IMETHOD OnUnassert(nsIRDFDataSource*, nsIRDFResource*, nsIRDFResource*, nsIRDFNode*);

  NS_IMETHOD OnMove(nsIRDFDataSource*, nsIRDFResource*, nsIRDFResource*, nsIRDFResource*, nsIRDFNode*) 
    { return NS_OK; }

  NS_IMETHOD OnChange(nsIRDFDataSource*, nsIRDFResource*, nsIRDFResource*, nsIRDFNode*, nsIRDFNode*);

  NS_IMETHOD BeginUpdateBatch(nsIRDFDataSource*) { return NS_OK; }
  NS_IMETHOD EndUpdateBatch(nsIRDFDataSource*)   { return NS_OK; }

private:

  HistoryDataSource* mHistoryDataSource;
};

NS_IMPL_ISUPPORTS1(HistoryDataSourceObserver, nsIRDFObserver)


//
// OnAssert
//
// Catches newly added items to the history, for surfing while the drawer is
// open.
//
NS_IMETHODIMP
HistoryDataSourceObserver::OnAssert(nsIRDFDataSource*, nsIRDFResource*, 
                                      nsIRDFResource* aProperty, nsIRDFNode*)
{
  const char* p;
  aProperty->GetValueConst(&p);
  if (strcmp("http://home.netscape.com/NC-rdf#Date", p) == 0)
    [mHistoryDataSource setNeedsRefresh:YES];

  return NS_OK;
}

//
// OnUnassert
//
// This gets called on redirects, when nsGlobalHistory::RemovePage is called.
//
NS_IMETHODIMP
HistoryDataSourceObserver::OnUnassert(nsIRDFDataSource*, nsIRDFResource*, 
                                      nsIRDFResource* aProperty, nsIRDFNode*)
{
  const char* p;
  aProperty->GetValueConst(&p);
  if (strcmp("http://home.netscape.com/NC-rdf#Date", p) == 0)
    [mHistoryDataSource setNeedsRefresh:YES];

  return NS_OK;
}

//
// OnChange
//
// Catches items that are already in history, but need to be moved because you're
// visiting them and they change date
//
NS_IMETHODIMP
HistoryDataSourceObserver::OnChange(nsIRDFDataSource*, nsIRDFResource*, 
                                      nsIRDFResource* aProperty, nsIRDFNode*, nsIRDFNode*)
{
  const char* p;
  aProperty->GetValueConst(&p);

  if (strcmp("http://home.netscape.com/NC-rdf#Date", p) == 0)
    [mHistoryDataSource setNeedsRefresh:YES];

  return NS_OK;
}

#pragma mark -

@interface HistoryDataSource(Private)

- (void)cleanupHistory;

@end

@implementation HistoryDataSource

- (void) dealloc
{
  [self cleanupHistory];
  [super dealloc];
}

// "non-virtual" cleanup method -- safe to call from dealloc.
- (void)cleanupHistory
{
  if (mDataSource && mObserver)
  {
    mDataSource->RemoveObserver(mObserver);
    NS_RELEASE(mObserver);		// nulls it
  }
}

// "virtual" method; called from superclass
- (void)cleanupDataSource
{
 	[self cleanupHistory];
  [super cleanupDataSource];
}

//
// ensureDataSourceLoaded
//
// Called when the history panel is selected or brought into view. Inits all
// the RDF-fu to make history go. We defer loading everything because it's 
// sorta slow and we don't want to take the hit when the user creates new windows
// or just opens the bookmarks panel.
//
- (void) ensureDataSourceLoaded
{
  [super ensureDataSourceLoaded];
  
  NS_ASSERTION(mRDFService, "Uh oh, RDF service not loaded in parent class");
  
  if ( !mDataSource )
  {
    // Get the Global History DataSource
    mRDFService->GetDataSource("rdf:history", &mDataSource);
    // Get the Date Folder Root
    mRDFService->GetResource("NC:HistoryByDate", &mRootResource);
  
    [mOutlineView setTarget: self];
    [mOutlineView setDoubleAction: @selector(openHistoryItem:)];
    [mOutlineView setDeleteAction: @selector(deleteHistoryItems:)];
  
    mObserver = new HistoryDataSourceObserver(self);
    if ( mObserver ) {
      NS_ADDREF(mObserver);
      mDataSource->AddObserver(mObserver);
    }
    [mOutlineView reloadData];
  }
  else
  {
    // everything is loaded, but we have to refresh our tree otherwise
    // changes that took place while the drawer was closed won't be noticed
    [mOutlineView reloadData];
  }
  
  NS_ASSERTION(mDataSource, "Uh oh, History RDF Data source not created");
}

- (void)enableObserver
{
  mUpdatesEnabled = YES;
}

-(void)disableObserver
{
  mUpdatesEnabled = NO;
}

- (void)setNeedsRefresh:(BOOL)needsRefresh
{
  mNeedsRefresh = needsRefresh;
}

- (BOOL)needsRefresh
{
  return mNeedsRefresh;
}

- (void)refresh
{
  if (mNeedsRefresh)
  {
    [self invalidateCachedItems];
    if (mUpdatesEnabled)
    {
      // this can be very slow! See bug 180109.
      //NSLog(@"history reload started");
      [self reloadDataForItem:nil reloadChildren:NO];
      //NSLog(@"history reload done");
    }
    mNeedsRefresh = NO; 
  }
}

//
// createCellContents:withColumn:byItem
//
// override to create an NSAttributedString instead of just the string with the
// given text. We add an icon and adjust the positioning of the text w/in the cell
//
-(id) createCellContents:(NSString*)inValue withColumn:(NSString*)inColumn byItem:(id) inItem
{
  if ([inValue length] == 0)
    inValue = [self getPropertyString:@"http://home.netscape.com/NC-rdf#URL" forItem:inItem];
  
  NSMutableAttributedString *cellValue = [[[NSMutableAttributedString alloc] initWithString:inValue] autorelease];
  
  if ([inColumn isEqualToString:@"http://home.netscape.com/NC-rdf#Name"])
  {
    NSFileWrapper     *fileWrapper    = [[NSFileWrapper alloc] initRegularFileWithContents:nil];
    NSTextAttachment  *textAttachment = [[NSTextAttachment alloc] initWithFileWrapper:fileWrapper];

    // Create an attributed string to hold the empty attachment, then release the components.
    NSMutableAttributedString *attachmentAttrString = [NSMutableAttributedString attributedStringWithAttachment:textAttachment];
    [textAttachment release];
    [fileWrapper release];

    //Get the cell of the text attachment.
    NSCell* attachmentAttrStringCell = (NSCell *)[(NSTextAttachment *)[attachmentAttrString attribute:
              NSAttachmentAttributeName atIndex:0 effectiveRange:nil] attachmentCell];

    if ([self outlineView:mOutlineView isItemExpandable:inItem])
      [attachmentAttrStringCell setImage:[NSImage imageNamed:@"folder"]];
    else
      [attachmentAttrStringCell setImage:[NSImage imageNamed:@"smallbookmark"]];

    //Insert the image
    [cellValue replaceCharactersInRange:NSMakeRange(0, 0) withAttributedString:attachmentAttrString];

    //Tweak the baseline to vertically center the text.
    [cellValue addAttribute:NSBaselineOffsetAttributeName
                      value:[NSNumber numberWithFloat:-5.0]
                      range:NSMakeRange(0, 1)];
  }

  return cellValue;
}


//
// filterDragItems:
//
// Walk the list of items, filtering out any folder. Returns a new list
// that has been autoreleased.
//
- (NSArray*)filterDragItems:(NSArray*)inItems
{
  NSMutableArray* outItems = [[[NSMutableArray alloc] init] autorelease];
  
  NSEnumerator *enumerator = [inItems objectEnumerator];
  id obj;
  while ( (obj = [enumerator nextObject]) ) {
    if ( ! [mOutlineView isExpandable: obj] )    // if it's not a folder, we can drag it
      [outItems addObject:obj];
  }
  
  return outItems;
}

- (BOOL)outlineView:(NSOutlineView *)ov writeItems:(NSArray*)items toPasteboard:(NSPasteboard*)pboard 
{
  //Need to filter out folders from the list, only allow the urls to be dragged
  NSArray *toDrag = [self filterDragItems:items];

  int count = [toDrag count];
  if (count > 0) {    
    if (count == 1) {
      id item = [toDrag objectAtIndex: 0];
      
      // if we have just one item, we add some more flavours
      NSString* url   = [self getPropertyString:@"http://home.netscape.com/NC-rdf#URL"  forItem:item];
      NSString* title = [self getPropertyString:@"http://home.netscape.com/NC-rdf#Name" forItem:item];
      NSString *cleanedTitle = [title stringByReplacingCharactersInSet:[NSCharacterSet controlCharacterSet] withString:@" "];

      [pboard declareURLPasteboardWithAdditionalTypes:[NSArray array] owner:self];
      [pboard setDataForURL:url title:cleanedTitle];
    }
    else {
      // not sure what to do here. canceling the drag for now.
      return NO;
    }

    return YES;
  }

  return NO;
}


-(IBAction)openHistoryItem: (id)aSender
{
    int index = [mOutlineView selectedRow];
    if (index == -1)
        return;
    
    id item = [mOutlineView itemAtRow: index];
    if (!item)
        return;
    
    // expand if collapsed and double click
    if ([mOutlineView isExpandable: item]) {
      if ([mOutlineView isItemExpanded: item])
        [mOutlineView collapseItem: item];
      else
        [mOutlineView expandItem: item];
        
      return;
    }
    
    // get uri
    NSString* url = [self getPropertyString:@"http://home.netscape.com/NC-rdf#URL" forItem:item];
    [[mBrowserWindowController getBrowserWrapper] loadURI: url referrer: nil flags: NSLoadFlagsNone activate:YES];
}


//
// deleteHistoryItems:
//
// Called when user hits backspace with the history view selected. Walks the selection
// removing the selected items from the global history in batch-mode, then reloads
// the outline.
//
-(IBAction)deleteHistoryItems: (id)aSender
{
  int index = [mOutlineView selectedRow];
  if (index == -1)
    return;

  // if the user selected a bunch of rows, we want to clear the selection since when
  // those rows are deleted, the tree will try to keep the same # of rows selected and
  // it will look really odd. If just 1 row was selected, we keep it around so the user
  // can keep quickly deleting one row at a time.
  BOOL clearSelectionWhenDone = [mOutlineView numberOfSelectedRows] > 1;
  
  nsCOMPtr<nsIBrowserHistory> history = do_GetService("@mozilla.org/browser/global-history;1");
  if ( history ) {
    history->StartBatchUpdate();
    NSEnumerator* rowEnum = [mOutlineView selectedRowEnumerator];
    for ( NSNumber* currIndex = [rowEnum nextObject]; currIndex; currIndex = [rowEnum nextObject]) {
      index = [currIndex intValue];
      RDFOutlineViewItem* item = [mOutlineView itemAtRow: index];
      if (![mOutlineView isExpandable: item]) {
        NSString* urlString = [self getPropertyString:@"http://home.netscape.com/NC-rdf#URL" forItem:item];
        history->RemovePage([urlString UTF8String]);
      }
    }
    history->EndBatchUpdate();
    if ( clearSelectionWhenDone )
      [mOutlineView deselectAll:self];

    [self invalidateCachedItems];
    [mOutlineView reloadData];     // necessary or the outline is really horked
  }
}

//
// outlineView:tooltipForString
//
// Only show a tooltip if we're not a folder. For urls, use title\nurl as the format.
// We can re-use the base-class impl to get the title of the page and then just add on
// to that.
//
- (NSString *)outlineView:(NSOutlineView *)outlineView tooltipStringForItem:(id)inItem
{
  if ( ! [mOutlineView isExpandable: inItem] ) {
    // use baseclass to get title of page
    NSString* pageTitle = [super outlineView:outlineView tooltipStringForItem:inItem];
  
    // append url
    NSString* url = [self getPropertyString:@"http://home.netscape.com/NC-rdf#URL" forItem:inItem];
    return [NSString stringWithFormat:@"%@\n%@", pageTitle, [url stringByTruncatingTo:80 at:kTruncateAtEnd]];
  }
  return nil;
}

@end
