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
 *   Simon Woodside <sbwoodside@yahoo.com>
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
#import "ExtendedOutlineView.h"
#import "PreferenceManager.h"
#import "HistoryItem.h"

#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFResource.h"
#include "nsIBrowserHistory.h"
#include "nsIServiceManager.h"

#include "nsXPIDLString.h"
#include "nsString.h"

#include "nsComponentManagerUtils.h"

//
// Uses the Gecko RDF data source for history,
// and presents a Cocoa API through the NSOutlineViewDataSource protocol
//

class HistoryRDFObserver : public nsIRDFObserver
{
public:
  HistoryRDFObserver(HistoryDataSource* dataSource)
  : mHistoryDataSource(dataSource)
  { 
  }
  virtual ~HistoryRDFObserver() { }
  
  NS_DECL_ISUPPORTS

  NS_IMETHOD OnAssert(nsIRDFDataSource*, nsIRDFResource*, nsIRDFResource*, nsIRDFNode*);
  NS_IMETHOD OnUnassert(nsIRDFDataSource*, nsIRDFResource*, nsIRDFResource*, nsIRDFNode*);

  NS_IMETHOD OnMove(nsIRDFDataSource*, nsIRDFResource*, nsIRDFResource*, nsIRDFResource*, nsIRDFNode*) 
    { return NS_OK; }

  NS_IMETHOD OnChange(nsIRDFDataSource*, nsIRDFResource*, nsIRDFResource*, nsIRDFNode*, nsIRDFNode*);

  NS_IMETHOD OnBeginUpdateBatch(nsIRDFDataSource*) { return NS_OK; }
  NS_IMETHOD OnEndUpdateBatch(nsIRDFDataSource*)   { return NS_OK; }

private:

  HistoryDataSource* mHistoryDataSource;
};

NS_IMPL_ISUPPORTS1(HistoryRDFObserver, nsIRDFObserver)


//
// OnAssert
//
// Catches newly added items to the history, for surfing while the drawer is
// open.
//
NS_IMETHODIMP
HistoryRDFObserver::OnAssert(nsIRDFDataSource*, nsIRDFResource*,
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
HistoryRDFObserver::OnUnassert(nsIRDFDataSource*, nsIRDFResource*,
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
HistoryRDFObserver::OnChange(nsIRDFDataSource*, nsIRDFResource*, 
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
- (void)cleanupDataSource;
- (void)removeItemFromGeckoHistory:(id)anItem withService:(nsIBrowserHistory*)inHistService;

@end

#pragma mark -

@implementation HistoryDataSource

- (void) dealloc
{
  [self cleanupHistory];
  [super dealloc];
}

// "non-virtual" cleanup method -- safe to call from dealloc.
- (void)cleanupHistory
{
  if (mRDFDataSource && mObserver) {
    mRDFDataSource->RemoveObserver(mObserver);
    NS_RELEASE(mObserver);		// nulls it
  }
}

// "virtual" method; called from superclass
- (void)cleanupDataSource;
{
  [self cleanupHistory];
  [super cleanupDataSource];
}

-(bool) loaded;
{
  return mLoaded;
}

- (HistoryItem *)rootRDFItem;
{
  return mRootHistoryItem;
}

- (void)setRootRDFItem:(HistoryItem *)item;
{
  [mRootHistoryItem autorelease];
  mRootHistoryItem = [item retain];
}

//
// loadLazily
//
// Called when the history panel is selected or brought into view. Inits all
// the RDF-fu to make history go. We defer loading everything because it's 
// sorta slow and we don't want to take the hit when the user creates new windows
// or just opens the bookmarks panel.
//
// in addition currently we don't get any gecko updates after we load so this
// will make sure it's up-to-date
//
- (void) loadLazily;
{
  [super loadLazily];
  
  NS_ASSERTION(mRDFService, "Uh oh, RDF service not loaded in parent class");
  
  if( !mRDFDataSource ) {
    // Get the Global History DataSource
    mRDFService->GetDataSource("rdf:history", &mRDFDataSource);
    // Get the Date Folder Root
    nsIRDFResource* aRDFRootResource;
    mRDFService->GetResource(nsDependentCString("NC:HistoryByDate"), &aRDFRootResource);
    HistoryItem * newRoot = [HistoryItem alloc];
    [newRoot initWithRDFResource:aRDFRootResource RDFDataSource:mRDFDataSource parent:nil];
    [self setRootRDFItem:newRoot];


    // identifiers: title url description keyword
  
    mObserver = new HistoryRDFObserver(self);
    if ( mObserver ) {
      NS_ADDREF(mObserver);
      mRDFDataSource->AddObserver(mObserver);
    }
    [mOutlineView reloadData];
  }
  else
  {
    // everything is loaded, but we have to refresh our tree otherwise
    // changes that took place while the drawer was closed won't be noticed
    [mOutlineView reloadData];
  }
  
  NS_ASSERTION(mRDFDataSource, "Uh oh, History RDF Data source not created");
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
      [self reloadDataForItem:nil reloadChildren:NO];
    }
    mNeedsRefresh = NO; 
  }
}

// this should only come from a context menu
// ... because we use [aSender representedObject] apparently...
-(IBAction)openHistoryItemInNewTab:(id)aSender
{
  NSString *url = [aSender representedObject];
  if ([url isKindOfClass:[NSString class]]) {
    BOOL loadInBackground = [[PreferenceManager sharedInstance] getBooleanPref:"browser.tabs.loadInBackground" withSuccess:NULL];
    [mBrowserWindowController openNewTabWithURL:url referrer:nil loadInBackground: loadInBackground];
  }
}

// this should only come from a context menu
// ... because we use [aSender representedObject] apparently...
-(IBAction)openHistoryItemInNewWindow:(id)aSender
{
  NSString *url = [aSender representedObject];
  if ([url isKindOfClass:[NSString class]]) {
    BOOL loadInBackground = [[PreferenceManager sharedInstance] getBooleanPref:"browser.tabs.loadInBackground" withSuccess:NULL];
    [mBrowserWindowController openNewWindowWithURL:url referrer: nil loadInBackground: loadInBackground];  
  }
}

- (IBAction)openHistoryItem:(id)aSender
{
  int index = [mOutlineView selectedRow];
  if (index == -1)
    return;
  id item = [mOutlineView itemAtRow: index];
  if (!item)
    return;
  if ([mOutlineView isExpandable: item]) { //TODO [item class]
    if ([mOutlineView isItemExpanded: item])
      [mOutlineView collapseItem: item];
    else
      [mOutlineView expandItem: item];
  }
  else {
    // The history view obeys the app preference for cmd-click -> open in new window or tab
    if (![item isKindOfClass: [HistoryItem class]])
      return;
    NSString* url = [item url];
    BOOL loadInBackground = [[PreferenceManager sharedInstance] getBooleanPref:"browser.tabs.loadInBackground" withSuccess:NULL];
    if (GetCurrentKeyModifiers() & cmdKey) {
      if ([[PreferenceManager sharedInstance] getBooleanPref:"browser.tabs.opentabfor.middleclick" withSuccess:NULL])
        [mBrowserWindowController openNewTabWithURL:url referrer:nil loadInBackground:loadInBackground];
      else
        [mBrowserWindowController openNewWindowWithURL:url referrer: nil loadInBackground:loadInBackground];  
    }
    else
      [[mBrowserWindowController getBrowserWrapper] loadURI:url referrer:nil flags:NSLoadFlagsNone activate:YES];
  }
}

// Walks the selection removing the selected items from the global history
// in batch-mode, then reload the outline.
- (IBAction)deleteHistoryItems:(id)aSender
{
  int index = [mOutlineView selectedRow];
  if (index == -1)
    return;

  // If just 1 row was selected, keep it so the user can delete again immediately
  BOOL clearSelectionWhenDone = ([mOutlineView numberOfSelectedRows] > 1);
  
  // row numbers will stay in sync until table is invalidated
  // removing children of removed items will fail silently (harmless)
  NSEnumerator* rowEnum = [mOutlineView selectedRowEnumerator];
  int currentRow;
  while( (currentRow = [[rowEnum nextObject] intValue]) ) {
    HistoryItem * item = [mOutlineView itemAtRow:currentRow];
    if( [item isKindOfClass:[RDFItem class]] ) {
      [(HistoryItem*)item deleteFromGecko];
      [[item parent] deleteChildFromCache:item];
    }
  }
  if ( clearSelectionWhenDone )
    [mOutlineView deselectAll:self];
  [mOutlineView reloadData];     // tell outline view the data source has changed
}


#pragma mark -

// Implementation of NSOutlineViewDataSource protocol

// identifiers: title url description keyword
- (id)outlineView:(NSOutlineView*)aOutlineView objectValueForTableColumn:(NSTableColumn*)aTableColumn byItem:(id)aItem
{
  if (!mRDFDataSource || !aItem)
    return nil;
  // use the table column identifier as a key
  if( [[aTableColumn identifier] isEqualToString:@"title"] )
    return [aItem name];
  else if( [[aTableColumn identifier] isEqualToString:@"url"] )
    return [aItem url];
//  else if( [[aTableColumn identifier] isEqualToString:@"description"] )
//    return [aItem date];
  return nil;
// TODO truncate string
//  - (void)truncateToWidth:(float)maxWidth at:kTruncateAtMiddle withAttributes:(NSDictionary *)attributes
}

- (BOOL)outlineView:(NSOutlineView *)ov writeItems:(NSArray*)items toPasteboard:(NSPasteboard*)pboard;
{
  //Need to filter out folders from the list, only allow the urls to be dragged
  NSMutableArray* toDrag = [[[NSMutableArray alloc] init] autorelease];
  NSEnumerator *enumerator = [items objectEnumerator];
  id obj;
  while( (obj = [enumerator nextObject]) ) {
    if( ![obj isExpandable] )
      [toDrag addObject:obj];
  }
  
  int count = [toDrag count];
  if (count == 1) {
    id item = [toDrag objectAtIndex: 0];
    // if we have just one item, we add some more flavours
    NSString* title = [item name];
    NSString *cleanedTitle
      = [title stringByReplacingCharactersInSet:[NSCharacterSet controlCharacterSet] withString:@" "];
    [pboard declareURLPasteboardWithAdditionalTypes:[NSArray array] owner:self];
    [pboard setDataForURL:[item url] title:cleanedTitle];
    return YES;
  }
  if( count > 1 ) {
    // not sure what to do for multiple drag. just cancel for now.
    return NO;
  }
  return NO;
}

// Only show a tooltip if we're not a folder. For items, use name\nurl as the format.
- (NSString *)outlineView:(NSOutlineView *)outlineView tooltipStringForItem:(id)anItem;
{
  if( ![anItem isExpandable] ) {
    NSString* pageTitle = [anItem name];
    NSString* url = [anItem url];
    return [NSString stringWithFormat:@"%@\n%@", pageTitle, [url stringByTruncatingTo:80 at:kTruncateAtEnd]];
  }
  return nil;
}

// TODO site icons
- (void)outlineView:(NSOutlineView *)outlineView willDisplayCell:(NSCell *)inCell forTableColumn:(NSTableColumn *)tableColumn item:(id)item;
{
  // set the image on the name column. the url column doesn't have an image.
 if ([[tableColumn identifier] isEqualToString:@"title"]) {
    if ( [outlineView isExpandable:item] )
      [inCell setImage:[NSImage imageNamed:@"folder"]];
    else
      [inCell setImage:[NSImage imageNamed:@"smallbookmark"]];
  }
}

- (NSMenu *)outlineView:(NSOutlineView *)outlineView contextMenuForItem:(id)item;
{
  if (nil == item || [mOutlineView isExpandable: item])
    return nil;

  // get item's URL, prep context menu
  NSString *nulString = [NSString string];
  NSMenu *contextMenu = [[[NSMenu alloc] initWithTitle:@"snookums"] autorelease];

  // open in new window
  NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Open in New Window",@"") action:@selector(openHistoryItemInNewWindow:) keyEquivalent:nulString];
  [menuItem setTarget:self];
  [menuItem setRepresentedObject:[item url]];
  [contextMenu addItem:menuItem];
  [menuItem release];

  // open in new tab
  menuItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Open in New Tab",@"") action:@selector(openHistoryItemInNewTab:) keyEquivalent:nulString];
  [menuItem setTarget:self];
  [menuItem setRepresentedObject:[item url]];
  [contextMenu addItem:menuItem];
  [menuItem release];

  [contextMenu addItem:[NSMenuItem separatorItem]];

  // delete
  menuItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Delete",@"") action:@selector(deleteHistoryItems:) keyEquivalent:nulString];
  [menuItem setTarget:self];
  [contextMenu addItem:menuItem];
  [menuItem release];
  return contextMenu;
}

@end
