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
*   Joe Hewitt <hewitt@netscape.com> (Original Author)
*   David Haas <haas@cae.wisc.edu> 
*/

#import "CHAutoCompleteTextField.h"
#import "BrowserWindowController.h"
#import "CHPageProxyIcon.h"
#include "nsIServiceManager.h"
#include "nsMemory.h"
#include "nsString.h"
#include "CHUserDefaults.h"

static const int kMaxRows = 6;
static const int kFrameMargin = 1;
static const int kEscapeKeyCode = 53;

class AutoCompleteListener : public nsIAutoCompleteListener
{  
public:
  AutoCompleteListener(CHAutoCompleteTextView* aTextView)
  {
    NS_INIT_REFCNT();
    mTextView = aTextView;
  }
  
  NS_DECL_ISUPPORTS

  NS_IMETHODIMP OnStatus(const PRUnichar* aText) { return NS_OK; }
  NS_IMETHODIMP SetParam(nsISupports *aParam) { return NS_OK; }
  NS_IMETHODIMP GetParam(nsISupports **aParam) { return NS_OK; }

  NS_IMETHODIMP OnAutoComplete(nsIAutoCompleteResults *aResults, AutoCompleteStatus aStatus)
  {
    [mTextView dataReady:aResults status:aStatus];
    return NS_OK;
  }

private:
  CHAutoCompleteTextView *mTextView;
};

NS_IMPL_ISUPPORTS1(AutoCompleteListener, nsIAutoCompleteListener)

////////////////////////////////////////////////////////////////////////

@implementation CHAutoCompleteTextView

- (void) awakeFromNib
{
  [self setAllowsUndo:YES];
  [self setFieldEditor:YES];
  [self setFont:[NSFont controlContentFontOfSize:0]];
  [self setUsesFontPanel:NO];
  [self setRichText:NO];
  [self setVerticallyResizable:NO];
  [self setBackgroundColor:[NSColor colorWithCalibratedWhite: 0.98 alpha: 1.0]];
  
  NSTableColumn *column;
  NSScrollView *scrollView;
  NSCell *dataCell;
  
  mSearchString = nil;
  mBackspaced = NO;
  mCompleteResult = NO;
  mOpenTimer = nil;
  
  mSession = nsnull;
  mResults = nsnull;
  mListener = (nsIAutoCompleteListener *)new AutoCompleteListener(self);
  NS_IF_ADDREF(mListener);

  [self setDelegate: self]; 

  // XXX the owner of the textfield should set this
  [self setSession:@"history"];
  
  // construct and configure the popup window
  mPopupWin = [[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,0,0)
                      styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO] retain];
  [mPopupWin setReleasedWhenClosed:NO];
  [mPopupWin setLevel:NSFloatingWindowLevel];
  [mPopupWin setHasShadow:YES];
  [mPopupWin setAlphaValue:0.9];

  // construct and configure the view
  mTableView = [[[NSTableView alloc] initWithFrame:NSMakeRect(0,0,0,0)] autorelease];
  [mTableView setIntercellSpacing:NSMakeSize(1, 2)];
  [mTableView setTarget:self];
  [mTableView setAction:@selector(onRowClicked:)];
  
  // Create the icon column if we have a proxy icon
  if (mProxyIcon) {
    column = [[[NSTableColumn alloc] initWithIdentifier:@"icon"] autorelease];
    [column setWidth:[mProxyIcon frame].origin.x + [mProxyIcon frame].size.width];
    dataCell = [[[NSImageCell alloc] initImageCell:nil] autorelease];
    [column setDataCell:dataCell];
    [mTableView addTableColumn: column];
  }
  
  // create the text columns
  column = [[[NSTableColumn alloc] initWithIdentifier:@"col1"] autorelease];
  [mTableView addTableColumn: column];
  column = [[[NSTableColumn alloc] initWithIdentifier:@"col2"] autorelease];
  [[column dataCell] setTextColor:[NSColor darkGrayColor]];
  [mTableView addTableColumn: column];

  // hide the table header
  [mTableView setHeaderView:nil];
  
  // construct the scroll view that contains the table view
  scrollView = [[[NSScrollView alloc] initWithFrame:NSMakeRect(0,0,0,0)] autorelease];
  [scrollView setHasVerticalScroller:YES];
  [[scrollView verticalScroller] setControlSize:NSSmallControlSize];
  [scrollView setDocumentView: mTableView];

  // construct the datasource
  mDataSource = [[CHAutoCompleteDataSource alloc] init];
  [mTableView setDataSource: mDataSource];

  [mPopupWin setContentView:scrollView];

  // listen for when window resigns from key handling
  [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onBlur:)
                                        name:NSWindowDidResignKeyNotification object:nil];

  // listen for when window is about to be moved or resized
  [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onBlur:)
                                        name:NSWindowWillMoveNotification object:nil];
  [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onResize:)
                                        name:NSWindowDidResizeNotification object:nil];

  // read the user default on if we should auto-complete the text field as the user
  // types or make them pick something from a list (a-la mozilla).
	mCompleteWhileTyping = [[NSUserDefaults standardUserDefaults] boolForKey:USER_DEFAULTS_AUTOCOMPLETE_WHILE_TYPING];
}

- (void) dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  if (mSearchString)
    [mSearchString release];
    
  [mPopupWin release];
  [mDataSource release];

  NS_IF_RELEASE(mSession);
  NS_IF_RELEASE(mResults);
  NS_IF_RELEASE(mListener);
}

- (void) setSession:(NSString *)aSession
{
  NS_IF_RELEASE(mSession);

  // XXX add aSession to contract id
  nsCOMPtr<nsIAutoCompleteSession> session =
    do_GetService("@mozilla.org/autocompleteSession;1?type=history");
  mSession = session;
  NS_IF_ADDREF(mSession);
}

- (NSString *) session
{
  // XXX return session name
  return @"";
}

- (NSTableView *) tableView
{
  return mTableView;
}

- (int) visibleRows
{
  int minRows = [mDataSource rowCount];
  return minRows < kMaxRows ? minRows : kMaxRows;
}

// searching ////////////////////////////

- (void) startSearch:(NSString*)aString complete:(BOOL)aComplete
{  
  if (mSearchString)
    [mSearchString release];
  mSearchString = [aString retain];

  mCompleteResult = aComplete;

  if ([self isOpen]) {
    [self performSearch];
  } else {
    // delay the search when the popup is not yet opened so that users 
    // don't see a jerky flashing popup when they start typing for the first time
    if (mOpenTimer) {
      [mOpenTimer invalidate];
      [mOpenTimer release];
    }
    
    mOpenTimer = [[NSTimer scheduledTimerWithTimeInterval:0.2 target:self selector:@selector(searchTimer:)
                          userInfo:nil repeats:NO] retain];
  }
}

- (void) performSearch
{
  PRUnichar* chars = nsMemory::Alloc(([mSearchString length]+1) * sizeof(PRUnichar));
  [mSearchString getCharacters:chars];
  chars[[mSearchString length]] = 0; // I shouldn't have to do this
  nsresult rv = mSession->OnStartLookup(chars, mResults, mListener);
  nsMemory::Free(chars);

  if (NS_FAILED(rv))
    NSLog(@"Unable to perform autocomplete lookup");
}

- (void) dataReady:(nsIAutoCompleteResults*)aResults status:(AutoCompleteStatus)aStatus
{
  NS_IF_RELEASE(mResults);
  mResults = nsnull;
  
  if (aStatus == nsIAutoCompleteStatus::failed) {
    [mDataSource setErrorMessage:@""];
  } else if (aStatus == nsIAutoCompleteStatus::ignored) {
    [mDataSource setErrorMessage:@""];
  } else if (aStatus == nsIAutoCompleteStatus::noMatch) {
    [mDataSource setErrorMessage:@""];
  } else if (aStatus == nsIAutoCompleteStatus::matchFound) {
    mResults = aResults;
    NS_IF_ADDREF(mResults);
    [mDataSource setResults:aResults];
    [self completeDefaultResult];
  }

  if ([mDataSource rowCount] > 0) {
    [mTableView noteNumberOfRowsChanged];
    [self openPopup];
  } else {
    [self closePopup];
  }
}

- (void) searchTimer:(NSTimer *)aTimer
{
  [mOpenTimer release];
  mOpenTimer = nil;

  [self performSearch];
}

- (void) clearResults
{
  // clear out search data
  mSearchString = nil;
  mResults = nil;

  [mDataSource setResults:nil];

  [self closePopup];
}

// handling the popup /////////////////////////////////

- (void) openPopup
{
  [self resizePopup];

  // show the popup
  if ([mPopupWin isVisible] == NO)
    [mPopupWin orderFront:nil];
}

- (void) resizePopup
{
  NSRect locationFrame, winFrame;
  NSPoint locationOrigin;
  int tableHeight;
  
  if ([self visibleRows] == 0) {
    [self closePopup];
    return;
  }

  // get the origin of the location bar in coordinates of the root view
  locationFrame = [[[[self superview] superview] superview] frame];
  locationOrigin = [[[[[self superview] superview] superview] superview] convertPoint:locationFrame.origin toView:[[[self window] contentView] superview]];

  // get the height of the table view
  winFrame = [[self window] frame];
  tableHeight = (int)([mTableView rowHeight]+[mTableView intercellSpacing].height)*[self visibleRows];

  // make the columns split the width of the popup
  [[mTableView tableColumnWithIdentifier:@"col1"] setWidth:locationFrame.size.width/2];
  
  // position the popup anchored to bottom/left of location bar (
  [mPopupWin setFrame:NSMakeRect(winFrame.origin.x + locationOrigin.x + kFrameMargin,
                                 ((winFrame.origin.y + locationOrigin.y) - tableHeight) - kFrameMargin,
                                 locationFrame.size.width - (2*kFrameMargin),
                                 tableHeight) display:NO];
}

- (void) closePopup
{
  [mPopupWin close];
}

- (BOOL) isOpen
{
  return [mPopupWin isVisible];
}

// url completion ////////////////////////////

- (void) completeDefaultResult
{
  PRInt32 defaultRow;
  mResults->GetDefaultItemIndex(&defaultRow);
  
  if (mCompleteResult && mCompleteWhileTyping) {
    [self selectRowAt:defaultRow];
    [self completeResult:defaultRow];
  } else {
    [self selectRowAt:-1];
  }
}

- (void) completeSelectedResult
{
  [self completeResult:[mTableView selectedRow]];
}

- (void) completeResult:(int)aRow
{
  NSRange matchRange;
  NSString *result1;

  if (aRow < 0 && mSearchString) {
    [self setString:mSearchString];
  } else {
    if ([mDataSource rowCount] <= 0)
      return;
  
    result1 = [mDataSource resultString:aRow column:@"col1"];
    matchRange = [result1 rangeOfString:mSearchString];
    if (matchRange.length > 0) {
      // cut off everything in the result string before the search string
      result1 = [result1 substringWithRange:NSMakeRange(matchRange.location, [result1 length]-matchRange.location)];
      
#if 1
      // fill in the textfield with the matching string
      [self setString:result1];
      
      // select the text after the search string
      [self setSelectedRange:NSMakeRange([mSearchString length], [result1 length]-[mSearchString length])];
#endif
    }
  }
}

- (void) enterResult:(int)aRow
{
  if (aRow >= 0 && [mDataSource rowCount] > 0) {
    [self setString: [mDataSource resultString:[mTableView selectedRow] column:@"col1"]];
    [self selectAll:self];
    [self closePopup];
  } else if (mOpenTimer) {
    // if there was a search timer going when we hit enter, cancel it
    [mOpenTimer invalidate];
    [mOpenTimer release];
    mOpenTimer = nil;
  }
}

- (void) revertText
{
  BrowserWindowController *controller = (BrowserWindowController *)[[self window] windowController];
  NSString *url = [[controller getBrowserWrapper] getCurrentURLSpec];
  if (url) {
    [self clearResults];
    [[self undoManager] removeAllActions];
    [self setString:url];
    [self selectAll:self];
  }
}

// selecting rows /////////////////////////////////////////

- (void) selectRowAt:(int)aRow
{
  if (aRow >= -1 && [mDataSource rowCount] > 0) {
    // show the popup
    if ([mPopupWin isVisible] == NO)
      [mPopupWin orderFront:nil];

    [mTableView selectRow:aRow byExtendingSelection:NO];
    [mTableView scrollRowToVisible: aRow];
  }
}

- (void) selectRowBy:(int)aRows
{
  int row = [mTableView selectedRow];
  
  if (row == -1 && aRows < 0) {
    // if nothing is selected and you scroll up, go to last row
    row = [mTableView numberOfRows]-1;
  } else if (row == [mTableView numberOfRows]-1 && aRows == 1) {
    // if the last row is selected and you scroll down, go to first row
    row = 0;
  } else if (aRows+row < 0) {
    // if you scroll up beyond first row...
    if (row == 0)
      row = -1; // ...and first row is selected, select nothing
    else
      row = 0; // ...else, go to first row
  } else if (aRows+row >= [mTableView numberOfRows]) {
    // if you scroll down beyond the last row...
    if (row == [mTableView numberOfRows]-1)
      row = 0; // and last row is selected, select first row
    else
      row = [mTableView numberOfRows]-1; // else, go to last row
  } else {
    // no special case, just increment current row
    row += aRows;
  }
    
  [self selectRowAt:row];
}

// event handlers ////////////////////////////////////////////

- (void) onRowClicked:(id)sender
{
  [self enterResult:[mTableView clickedRow]];
  [[[self window] windowController] goToLocationFromToolbarURLField:self];
}

- (void) onBlur:(id)sender
{
  [self closePopup];
}

- (void) onResize:(id)sender
{
  [self resizePopup];
}

// NSTextField delegate //////////////////////////////////

- (void)textDidChange:(NSNotification *)aNotification
{
  NSRange range = [self selectedRange];
  
  // make sure we're typing at the end of the string
  if (range.location == [[self string] length])
    [self startSearch:[self string] complete:!mBackspaced];
  else
    [self clearResults];
  
  mBackspaced = NO;
}

- (void)textDidEndEditing:(NSNotification *)aNotification
{
  [self closePopup];
  [self setSelectedRange:NSMakeRange(0,0)];
  [[self undoManager] removeAllActions];
  if ([[[aNotification userInfo] objectForKey:@"NSTextMovement"] intValue] == NSReturnTextMovement)
    [[[self window] windowController] goToLocationFromToolbarURLField:self];
}

- (BOOL)textView:(NSTextView *)textView doCommandBySelector:(SEL)command
{
  if (command == @selector(insertNewline:)) {
    [self enterResult:[mTableView selectedRow]];
  } else if (command == @selector(moveUp:)) {
    [self selectRowBy:-1];
    [self completeSelectedResult];
    return YES;
  } else if (command == @selector(moveDown:)) {
    [self selectRowBy:1];
    [self completeSelectedResult];
    return YES;
  } else if (command == @selector(scrollPageUp:)) {
    [self selectRowBy:-kMaxRows];
    [self completeSelectedResult];
  } else if (command == @selector(scrollPageDown:)) {
    [self selectRowBy:kMaxRows];
    [self completeSelectedResult];
  } else if (command == @selector(moveToBeginningOfDocument:)) {
    [self selectRowAt:0];
    [self completeSelectedResult];
  } else if (command == @selector(moveToEndOfDocument:)) {
    [self selectRowAt:[mTableView numberOfRows]-1];
    [self completeSelectedResult];
  } else if (command == @selector(insertTab:)) {
    if ([mPopupWin isVisible]) {
      [self selectRowBy:1];
      [self completeSelectedResult];
      return YES;
    }
  } else if (command == @selector(deleteBackward:) || 
             command == @selector(deleteForward:)) {
    // if the user deletes characters, we need to know so that
    // we can prevent autocompletion later when search results come in
    if ([[self string] length] > 1)
      mBackspaced = YES;
  }
  
  return NO;
}

- (void)keyDown:(NSEvent *)theEvent
{
  if ([theEvent keyCode] == kEscapeKeyCode)
    [self revertText];
  else 
    [super keyDown:theEvent];  
}

- (void) setPageProxyIcon:(NSImage *)aImage
{
  [mProxyIcon setImage:aImage];
}

@end
