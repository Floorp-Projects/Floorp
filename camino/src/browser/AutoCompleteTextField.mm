/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (Original Author)
 *   David Haas <haasd@cae.wisc.edu>
 *   Nick Kreeger <nick.kreeger@park.edu>
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

#import "NSString+Utils.h"

#import "AutoCompleteTextField.h"
#import "BrowserWindowController.h"
#import "PageProxyIcon.h"
#import "CHBrowserService.h"

#include "nsIAutoCompleteSession.h"
#include "nsIAutoCompleteResults.h"
#include "nsIAutoCompleteListener.h"
#include "nsIBrowserHistory.h"

#include "nsIServiceManager.h"
#include "nsIWebProgressListener.h"
#include "nsMemory.h"
#include "nsString.h"
#include "UserDefaults.h"

static const int kMaxRows = 6;
static const int kFrameMargin = 1;

const float kSecureIconRightOffset = 19.0;    // offset from right size of url bar
const float kSecureIconYOrigin = 3.0;
const float kSecureIconSize = 16.0;

const float kFeedIconRightOffest = 17.0;
const float kFeedIconYOrigin = 5.0;
const float kFeedIconWidth = 14.0;
const float kFeedIconHeight = 13.0;

// stole this from NSPasteboard+Utils.mm
static NSString* kCorePasteboardFlavorType_url  = @"CorePasteboardFlavorType 0x75726C20"; // 'url '  url
// posted when the feed icon's menu is about to be drawn
NSString* const kWillShowFeedMenu = @"WillShowFeedMenu";

@interface AutoCompleteTextField(Private)

- (NSTableView *) tableView;
- (int) visibleRows;

- (void) startSearch:(NSString*)aString complete:(BOOL)aComplete;
- (void) performSearch;
- (BOOL) caretIsAtEndOfLine;
- (void) dataReady:(nsIAutoCompleteResults*)aResults status:(AutoCompleteStatus)aStatus;
- (void) searchTimer:(NSTimer *)aTimer;

- (void) completeDefaultResult;
- (void) completeSelectedResult;
- (void) completeResult:(int)aRow;
- (void) enterResult:(int)aRow;

- (void) cleanup;
- (void) setStringUndoably:(NSString*)aString fromLocation:(unsigned int)aLocation;

- (void) openPopup;
- (void) resizePopup:(BOOL)forceResize;
- (void) closePopup;
- (BOOL) isOpen;

- (void) selectRowAt:(int)aRow;
- (void) selectRowBy:(int)aRows;

- (void) onRowClicked:(NSNotification *)aNote;
- (void) onBlur:(NSNotification *)aNote;
- (void) onResize:(NSNotification *)aNote;
- (void) onUndoOrRedo:(NSNotification *)aNote;

- (void)positionFeedIcon;

@end

#pragma mark -

@interface AutoCompleteWindow : NSWindow
- (BOOL)isKeyWindow;
@end

@implementation AutoCompleteWindow

- (BOOL)isKeyWindow
{
  return YES;
}

@end

#pragma mark -

//
// AutoCompleteTextCell
//
// Text cell subclass used to make room for the proxy icon inside the textview
//
@interface AutoCompleteTextCell : NSTextFieldCell
{
  BOOL mDisplaySecureIcon;    // YES if currently displaying the security icon
  BOOL mDisplayFeedIcon;      // YES if currently displaying the feed icon
}
- (void)calcControlViewSizeForSecureIcon:(BOOL)inIsVisible;
- (void)calcControlViewSizeForFeedIcon:(BOOL)inIsVisible;
- (BOOL)isSecureIconDisplayed;
- (BOOL)isFeedIconDisplayed;
@end

@implementation AutoCompleteTextCell

//
// -initTextCell:
//
// Handles initializing our members, start out assuming we're not showing a secure icon
//
- (id)initTextCell:(NSString*)inStr
{
  if ((self = [super initTextCell:inStr])) {
    mDisplaySecureIcon = NO;
    mDisplayFeedIcon = NO;
  }
  return self;
}

//
// -drawingRectForBounds:
//
// Overridden to adjust the bounds we draw into inside the full rectangle. The proxy icon
// takes away space on the left side (always) and the secure icon (if visible) takes away space
// on the right side. The remainder in the middle is where we're allowed to draw the text of
// the text field.
//
- (NSRect)drawingRectForBounds:(NSRect)theRect
{
  const float kProxyIconOffset = 19.0;
  
  theRect.origin.x += kProxyIconOffset;
  theRect.size.width -= kProxyIconOffset;
  if ([self isSecureIconDisplayed])
    theRect.size.width -= kSecureIconRightOffset;
  if ([self isFeedIconDisplayed])
    theRect.size.width -= kFeedIconRightOffest;
    
  return [super drawingRectForBounds:theRect];
}

//
// -calcControlViewSizeForSecureIcon:
//
// Indicates whether or not we need to take away space on the right side for the security
// icon. Causes the cell's drawing rect to be recalculated.
//
- (void)calcControlViewSizeForSecureIcon:(BOOL)inIsVisible
{
  mDisplaySecureIcon = inIsVisible;
  [(NSControl*)[self controlView] calcSize];
}

//
// -calcControlViewSizeForFeedIcon:
//
// Return the feed icons visibility state.
//
- (void)calcControlViewSizeForFeedIcon:(BOOL)inIsVisible
{
  mDisplayFeedIcon = inIsVisible;
  [(NSControl*)[self controlView] calcSize];
}

//
// -isSecureIconDisplayed:
//
// Return the secure icons visibility state.
//
- (BOOL)isSecureIconDisplayed
{
  return mDisplaySecureIcon;
}

//
// -isFeedIconDisplayed:
//
// Accessor method to help the layout the feed icon to the left of the security icon. This
// method is used when |displaySecureIcon:| is called to determine if the feed icon has been
// displayed before the security icon.
//
- (BOOL)isFeedIconDisplayed
{
  return mDisplayFeedIcon;
}

@end

@interface ClickMenuImageView : NSImageView
{
  // Notifications will only be posted if this string is set in |setMenuNotificationName:|
  NSString* mMenuNotificationName;  
}
- (void)setMenuNotificationName:(NSString*)aNotificationName;
@end

@implementation ClickMenuImageView

- (id)init
{
  if ((self = [super init]))
    mMenuNotificationName = nil;
  
  return self;
}

- (void)dealloc
{
  if (mMenuNotificationName)
    [mMenuNotificationName release];
  
  [super dealloc];
}

// Setting the notification variable in this method will enable notification postings.
- (void)setMenuNotificationName:(NSString*)aNotificationName
{
  mMenuNotificationName = [aNotificationName retain];
}

+ (NSMenu*)defaultMenu
{
  // we need to return something here to get the NSToolbar to forward context clicks to our view.
  return [[[NSMenu alloc] initWithTitle:@"Dummy"] autorelease];
}

- (NSMenu*)menuForEvent:(NSEvent *)theEvent
{
  // If we are the a feed icon, post this notification so the menu can get built.
  if (mMenuNotificationName) {
    [[NSNotificationCenter defaultCenter] postNotificationName:mMenuNotificationName 
                                                        object:self];
  }
  return [self menu];
}

- (void)mouseDown:(NSEvent*)theEvent
{
  // By default a right-click will show the context menu for our image view subclass, 
  // but a left click will not show a context. Since we want to show the context menus
  // on a left click, foward this mouse down event as a right mouse down event.
  [self rightMouseDown:theEvent];
}

- (void)resetCursorRects
{
  [self addCursorRect:[self bounds] cursor:[NSCursor arrowCursor]];
}

@end

#pragma mark -

class AutoCompleteListener : public nsIAutoCompleteListener
{  
public:
  AutoCompleteListener(AutoCompleteTextField* aTextField)
  {
    mTextField = aTextField;
  }
  
  NS_DECL_ISUPPORTS

  NS_IMETHODIMP OnStatus(const PRUnichar* aText) { return NS_OK; }
  NS_IMETHODIMP SetParam(nsISupports *aParam) { return NS_OK; }
  NS_IMETHODIMP GetParam(nsISupports **aParam) { return NS_OK; }

  NS_IMETHODIMP OnAutoComplete(nsIAutoCompleteResults *aResults, AutoCompleteStatus aStatus)
  {
    [mTextField dataReady:aResults status:aStatus];
    return NS_OK;
  }

private:
  AutoCompleteTextField *mTextField;
};

NS_IMPL_ISUPPORTS1(AutoCompleteListener, nsIAutoCompleteListener)

#pragma mark -

@implementation AutoCompleteTextField

+ (Class) cellClass
{
  return [AutoCompleteTextCell class];
}

// This method shouldn't be necessary according to the docs. The superclass's
// constructors should read in the cellClass and build a properly configured
// instance on their own. Instead they ignore cellClass and build a NSTextFieldCell.
- (id)initWithCoder:(NSCoder *)coder
{
  [super initWithCoder:coder];
  AutoCompleteTextCell* cell = [[[AutoCompleteTextCell alloc] initTextCell:@""] autorelease];
  [cell setEditable:[self isEditable]];
  [cell setDrawsBackground:[self drawsBackground]];
  [cell setBordered:[self isBordered]];
  [cell setBezeled:[self isBezeled]];
  [cell setScrollable:YES];
  [self setCell:cell];
  return self;
}

- (void) awakeFromNib
{
  NSTableColumn *column;
  NSScrollView *scrollView;
  NSCell *dataCell;
  
  mListener = (nsIAutoCompleteListener *)new AutoCompleteListener(self);
  NS_IF_ADDREF(mListener);

  [self setFont:[NSFont controlContentFontOfSize:0]];
  [self setDelegate: self];

  // XXX the owner of the textfield should set this
  [self setSession:@"history"];
  
  // construct and configure the popup window
  mPopupWin = [[AutoCompleteWindow alloc] initWithContentRect:NSMakeRect(0,0,0,0)
                      styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
  [mPopupWin setReleasedWhenClosed:NO];
  [mPopupWin setHasShadow:YES];
  [mPopupWin setAlphaValue:0.9];

  // construct and configure the view
  mTableView = [[[NSTableView alloc] initWithFrame:NSMakeRect(0,0,0,0)] autorelease];
  [mTableView setIntercellSpacing:NSMakeSize(1, 2)];
  [mTableView setTarget:self];
  [mTableView setAction:@selector(onRowClicked:)];
  
  // if we have a proxy icon
  if (mProxyIcon) {
    // place the proxy icon on top of this view so we can see it
    [mProxyIcon retain];
    [mProxyIcon removeFromSuperviewWithoutNeedingDisplay];
    [self addSubview:mProxyIcon];
    [mProxyIcon release];
    [mProxyIcon setFrameOrigin: NSMakePoint(3, 3)];
    // Create the icon column
    column = [[[NSTableColumn alloc] initWithIdentifier:@"icon"] autorelease];
    [column setWidth:[mProxyIcon frame].origin.x + [mProxyIcon frame].size.width];
    dataCell = [[[NSImageCell alloc] initImageCell:nil] autorelease];
    [column setDataCell:dataCell];
    [mTableView addTableColumn: column];
  }
  
  // cache the bg color for secure pages. create a secure icon view which we'll display at the far right
  // of the url bar. We hold onto a strong ref so that it doesn't get destroyed when we remove it from
  // the view hierarchy (for non-secure pages). Set its resize mask so that it moves as you resize the
  // window and mark this view that it needs to resize its subviews so it will automagically move the
  // secure icon when it's visible.
  mSecureBackgroundColor = [[NSColor colorWithDeviceRed:1.0 green:1.0 blue:0.777 alpha:1.0] retain];
  mLock = [[ClickMenuImageView alloc] initWithFrame:NSMakeRect([self bounds].size.width - kSecureIconRightOffset, 
                                                          kSecureIconYOrigin, kSecureIconSize, kSecureIconSize)];  // we own this
  [mLock setAutoresizingMask:(NSViewNotSizable | NSViewMinXMargin)];
  [mLock setMenu:mLockIconContextMenu];
  
  mFeedIcon = [[ClickMenuImageView alloc] initWithFrame:NSMakeRect([self bounds].size.width - kFeedIconRightOffest, 
                                                              kFeedIconYOrigin, kFeedIconWidth, kFeedIconHeight)];  // we own this
  [mFeedIcon setMenuNotificationName:kWillShowFeedMenu];
  [mFeedIcon setAutoresizingMask:(NSViewNotSizable | NSViewMinXMargin)];
  [mFeedIcon setToolTip:NSLocalizedString(@"FeedDetected", nil)];
  [mFeedIcon setImage:[NSImage imageNamed:@"feed"]];
  
  [self setAutoresizesSubviews:YES];

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
  mDataSource = [[AutoCompleteDataSource alloc] init];
  [mTableView setDataSource: mDataSource];

  [mPopupWin setContentView:scrollView];

  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(onResize:)
                                               name:NSWindowDidResizeNotification
                                             object:nil];

  // listen for Undo/Redo to make sure autocomplete doesn't get horked.
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(onUndoOrRedo:)
                                               name:NSUndoManagerDidRedoChangeNotification
                                             object:[[self fieldEditor] undoManager]];

  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(onUndoOrRedo:)
                                               name:NSUndoManagerDidUndoChangeNotification
                                             object:[[self fieldEditor] undoManager]];

  // register for embedding shutting down, to clean up history stuff
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(shutdown:)
                                               name:TermEmbeddingNotificationName
                                             object:nil];
  
  // read the user default on if we should auto-complete the text field as the user
  // types or make them pick something from a list (a-la mozilla).
  mCompleteWhileTyping = [[NSUserDefaults standardUserDefaults] boolForKey:USER_DEFAULTS_AUTOCOMPLETE_WHILE_TYPING];
        
    // register for string & URL drags
  [self registerForDraggedTypes:[NSArray arrayWithObjects:kCorePasteboardFlavorType_url, NSURLPboardType, NSStringPboardType, nil]];
}

- (void) dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [self cleanup];
  [super dealloc];
}

- (void)cleanup
{
  [mSearchString release];
  mSearchString = nil;
  
  [mPopupWin release];
  mPopupWin = nil;
  
  [mDataSource release];
  mDataSource = nil;
  
  [mSecureBackgroundColor release];
  mSecureBackgroundColor = nil;
  
  [mLock release]; 
  mLock = nil;
  [mFeedIcon release];
  mFeedIcon = nil;

  NS_IF_RELEASE(mSession);
  NS_IF_RELEASE(mResults);
  NS_IF_RELEASE(mListener);
}

- (void)shutdown: (NSNotification*)aNotification
{
  [self cleanup];
}

- (void) setSession:(NSString *)aSession
{
  NS_IF_RELEASE(mSession);

  // XXX add aSession to contract id
  nsCOMPtr<nsIAutoCompleteSession> session =
    do_GetService(NS_GLOBALHISTORY_AUTOCOMPLETE_CONTRACTID);
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

- (void) setPageProxyIcon:(NSImage *)aImage
{
  [mProxyIcon setImage:aImage];
}

-(id) fieldEditor
{
  return [[self window] fieldEditor:NO forObject:self];
}

// this catches the escape key
- (IBAction)cancel:(id)sender
{
  [self revertText];
}

#pragma mark -

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
    
    // note that we've created a circular reference here
    mOpenTimer = [[NSTimer scheduledTimerWithTimeInterval:0.2 target:self selector:@selector(searchTimer:)
                          userInfo:nil repeats:NO] retain];
  }
}

- (void) performSearch
{
  // sometimes we get a null mSession, and if we don't check for that we crash
  if (mSession) {
    nsAutoString searchString;
    [mSearchString assignTo_nsAString:searchString];
    nsresult rv = mSession->OnStartLookup(searchString.get(), mResults, mListener);
    if (NS_FAILED(rv))
      NSLog(@"Unable to perform autocomplete lookup");
  }
}

- (BOOL)caretIsAtEndOfLine
{
  NSRange selectedLocation = [[self fieldEditor] selectedRange];
  return (selectedLocation.length == 0) && (selectedLocation.location == [[self stringValue] length]);
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
  if (mSearchString)
    [mSearchString release];
  mSearchString = nil;
  NS_IF_RELEASE(mResults);
  mResults = nsnull;

  [mDataSource setResults:nil];

  [self closePopup];
}

#pragma mark -

// handling the popup /////////////////////////////////

- (void) openPopup
{
  [self resizePopup:YES];

  // show the popup
  if ([mPopupWin isVisible] == NO)
  {
    [[self window] addChildWindow:mPopupWin ordered:NSWindowAbove];
    [mPopupWin orderFront:nil];
  }
}

- (void) resizePopup:(BOOL)forceResize
{
  if ([self visibleRows] == 0) {
    [self closePopup];
    return;
  }
  
  // don't waste time resizing stuff that is not visible (unless we're about
  // to show the popup)
  if (![self isOpen] && !forceResize)
    return;

  // get the origin of the location bar in coordinates of the root view
  NSRect locationFrame = [self bounds];
  NSPoint locationOrigin = locationFrame.origin;
  locationOrigin.y += NSHeight(locationFrame);    // we want bottom left
  locationOrigin = [self convertPoint:locationOrigin toView:nil];

  // get the height of the table view
  NSRect winFrame = [[self window] frame];
  int tableHeight = (int)([mTableView rowHeight] + [mTableView intercellSpacing].height) * [self visibleRows];

  // make the columns split the width of the popup
  [[mTableView tableColumnWithIdentifier:@"col1"] setWidth:(locationFrame.size.width / 2.0f)];
  
  // position the popup anchored to bottom/left of location bar
  NSRect popupFrame = NSMakeRect(winFrame.origin.x + locationOrigin.x + kFrameMargin,
                                 ((winFrame.origin.y + locationOrigin.y) - tableHeight) - kFrameMargin,
                                 locationFrame.size.width - (2 * kFrameMargin),
                                 tableHeight);
  [mPopupWin setFrame:popupFrame display:NO];
}

- (void) closePopup
{
  [[mPopupWin parentWindow] removeChildWindow:mPopupWin];
  [mPopupWin orderOut:nil];
}

- (BOOL) isOpen
{
  return [mPopupWin isVisible];
}

//
// -userHasTyped
//
// Returns whether the user has typed anything into the url bar since the last 
// time the url was set (by loading a page). We know this is the case by looking
// at if there is a search string.
//
- (BOOL) userHasTyped
{
  return ( mSearchString != nil );
}

#pragma mark -

// Handle feed detection and opening

//
// -displayFeedIcon:
//
// Called when BrowserWindowController wants us to show the RSS icon.
//
- (void)displayFeedIcon:(BOOL)inDisplay
{
  if (inDisplay) {
    if (![mFeedIcon superview]) {
      // showing the icon. The user may have resized the window (and thus this view) while
      // the view was hidden (and thus not part of the view hierarchy) so we have to reposition
      // it manually before we add it.
      [[self cell] calcControlViewSizeForFeedIcon:YES];
      [self addSubview:mFeedIcon];
    }
    [self positionFeedIcon];
  }
  else if ([mFeedIcon superview]) {
    // hiding the icon. We don't have to do anything more than remove it from view.
    [[self cell] calcControlViewSizeForFeedIcon:NO];
    [mFeedIcon removeFromSuperview];
  }
}

//
// -positionFeedIcon
//
// Called when the position of the feed icon in the text field needs to be checked.
//
-(void)positionFeedIcon
{
  // position the feed icon to the left of the security icon
  if ([[self cell] isSecureIconDisplayed])
    [mFeedIcon setFrameOrigin:NSMakePoint([self bounds].size.width - (kFeedIconWidth + kSecureIconRightOffset), kFeedIconYOrigin)];
  // position the feed icon to the furthest right of the url bar
  else
    [mFeedIcon setFrameOrigin:NSMakePoint([self bounds].size.width - kFeedIconRightOffest, kFeedIconYOrigin)];
}

//
// -setFeedIconContextMenu:
//
// Set the context menu for the feed icon with a menu that has been built for the
// feeds found on the page.
//
- (void)setFeedIconContextMenu:(NSMenu*)inMenu
{
  [mFeedIcon setMenu:inMenu];
}

#pragma mark -

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
  if (aRow < 0 && mSearchString) {
    // reset to the original search string with the insertion point at the end. Note
    // we have to make our range before we call setStringUndoably: because it calls
    // clearResults() which destroys |mSearchString|.
    NSRange selectAtEnd = NSMakeRange([mSearchString length],0);
    [self setStringUndoably:mSearchString fromLocation:0];
    [[self fieldEditor] setSelectedRange:selectAtEnd];
  }
  else {
    if ([mDataSource rowCount] <= 0)
      return;

    // Fill in the suggestion from the list, but change only the text 
    // after what is typed and select just that part. This allows the
    // user to see what they have typed and what change the autocomplete
    // makes while allowing them to continue typing w/out having to
    // reset the insertion point. 
    NSString *result = [mDataSource resultForRow:aRow columnIdentifier:@"col1"];
    
    // figure out where to start the match, depending on whether the user typed the protocol part
    int protocolLength = 0;
    NSURL* resultURL = [NSURL URLWithString:result];
    NSURL* searchURL = [NSURL URLWithString:mSearchString];
    if (resultURL)
    {
      if (([[searchURL scheme] length] == 0) || ![[searchURL scheme] isEqualToString:[resultURL scheme]])
        protocolLength = [[resultURL scheme] length];
    }
        
    NSRange matchRange = [result rangeOfString:mSearchString options:NSCaseInsensitiveSearch range:NSMakeRange(protocolLength, [result length] - protocolLength)];
    if (matchRange.length > 0 && matchRange.location != NSNotFound) {
      unsigned int location = matchRange.location + matchRange.length;
      result = [result substringWithRange:NSMakeRange(location, [result length]-location)];
      [self setStringUndoably:result fromLocation:[mSearchString length]];
    }
  }
}

- (void) enterResult:(int)aRow
{
  if (aRow >= 0 && [mDataSource rowCount] > 0) {
    [self setStringUndoably:[mDataSource resultForRow:[mTableView selectedRow] columnIdentifier:@"col1"] fromLocation:0];
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
  id parentWindowController = [[self window] windowController];
  NSString* url = nil;
  if ([parentWindowController isKindOfClass:[BrowserWindowController class]])
  {
    BrowserWindowController *controller = (BrowserWindowController*)parentWindowController;
    url = [[controller getBrowserWrapper] currentURI];
  }

  if (!url)
    url = @"";

  [self clearResults];
  NSTextView *fieldEditor = [self fieldEditor];
  [[fieldEditor undoManager] removeAllActionsWithTarget:fieldEditor];
  [fieldEditor setString:url];
  [fieldEditor selectAll:self];
}

#pragma mark -

//
// -displaySecureIcon:
//
// Shows or hides the security icon depending on |inShouldDisplay|.
//
- (void)displaySecureIcon:(BOOL)inShouldDisplay
{
  if (inShouldDisplay && ![mLock superview]) {
    // showing the icon. The user may have resized the window (and thus this view) while
    // the view was hidden (and thus not part of the view hierarchy) so we have to reposition
    // it manually before we add it.
    [[self cell] calcControlViewSizeForSecureIcon:YES];
    [mLock setFrameOrigin:NSMakePoint([self bounds].size.width - kSecureIconRightOffset, kSecureIconYOrigin)];
    [self addSubview:mLock];
    
    // If the feed icon needs to be displayed, and it was drawn before the secure icon, move the 
    // feed icon to the left hand side of the secure icon.
    if ([[self cell] isFeedIconDisplayed])
      [self positionFeedIcon];
  }
  else if (!inShouldDisplay && [mLock superview]) {
    // hiding the icon. We don't have to do anything more than remove it from view.
    [[self cell] calcControlViewSizeForSecureIcon:NO];
    [mLock removeFromSuperview];
    
    if ([[self cell] isFeedIconDisplayed])
      [self positionFeedIcon];
  }
}

//
// -setSecureState:
//
// Changes the display of the text field to indicate whether the page
// is secure or not.
//
- (void)setSecureState:(unsigned char)inState
{
  NSColor* urlBarColor = [NSColor whiteColor];
  BOOL isSecure = NO;
  switch (inState) {
    case nsIWebProgressListener::STATE_IS_INSECURE:
      [mLock setImage:[BrowserWindowController insecureIcon]];
      break;
    case nsIWebProgressListener::STATE_IS_SECURE:
      [mLock setImage:[BrowserWindowController secureIcon]];
      urlBarColor = mSecureBackgroundColor;
      isSecure = YES;
      break;
    case nsIWebProgressListener::STATE_IS_BROKEN:
      [mLock setImage:[BrowserWindowController brokenIcon]];
      urlBarColor = mSecureBackgroundColor;
      isSecure = YES;
      break;
  }
  [self setBackgroundColor:urlBarColor];
  [self displaySecureIcon:isSecure];
}

//
// -setURI
//
// the public way to change the url string so that it does the right thing for handling
// autocomplete and completions in progress
//
- (void) setURI:(NSString*)aURI
{
  // if the urlbar has focus (actually if its field editor has focus), we
  // need to use one of its routines to update the autocomplete status or
  // we could find ourselves with stale results and the popup still open. If
  // it doesn't have focus, we can bypass all that and just use normal routines.
  if ( [self fieldEditor] && [[self window] firstResponder] == [self fieldEditor] ) {
    [self setStringUndoably:aURI fromLocation:0];   // updates autocomplete correctly

    // set insertion point to the end of the url. setStringUndoably:fromLocation:
    // will leave it at the beginning of the text field for this case and
    // that really looks wrong.
    int len = [[[self fieldEditor] string] length];
    [[self fieldEditor] setSelectedRange:NSMakeRange(len, len)];
  }
  else
  {
    [self setStringValue:aURI];
  }
}

- (void)setStringUndoably:(NSString *)aString fromLocation:(unsigned int)aLocation
{
  NSTextView* fieldEditor = [self fieldEditor];
  NSString* curValue = [fieldEditor string];

  unsigned curLength = [curValue length];
  if (aLocation > curLength)    // sanity check or AppKit crashes
    return;

  if ((aLocation + [aString length] == curLength) && [curValue compare:aString options:0 range:NSMakeRange(aLocation, [aString length])] == NSOrderedSame)
    return;  // nothing to do

  NSRange range = NSMakeRange(aLocation, [curValue length] - aLocation);
  if ([fieldEditor shouldChangeTextInRange:range replacementString:aString])
  {
    [[fieldEditor textStorage] replaceCharactersInRange:range withString:aString];
    if (NSMaxRange(range) == 0) // will only be true if the field is empty
      [fieldEditor setFont:[self font]]; // wrong font will be used otherwise

    // Whenever we send [self didChangeText], we trigger the
    // textDidChange method, which will begin a new search with
    // a new search string (which we just inserted) if the selection
    // is at the end of the string.  So, we "select" the first character
    // to prevent that badness from happening.
    [fieldEditor setSelectedRange:NSMakeRange(0,0)];    
    [fieldEditor didChangeText];
  }
  
  // sanity check and don't update the highlight if we're starting from the
  // beginning of the string. There's no need for that since no autocomplete
  // result would ever replace the string from location 0.
  if (aLocation > [[fieldEditor string] length] || !aLocation)
    return;
  range = NSMakeRange(aLocation, [[fieldEditor string] length] - aLocation);
  [fieldEditor setSelectedRange:range];
}

#pragma mark -

// selecting rows /////////////////////////////////////////

- (void) selectRowAt:(int)aRow
{
  if (aRow >= -1 && [mDataSource rowCount] > 0) {
    // show the popup
    [self openPopup];

    if ( aRow == -1 )
      [mTableView deselectAll:self];
    else {
      [mTableView selectRow:aRow byExtendingSelection:NO];
      [mTableView scrollRowToVisible: aRow];
    }
  }
}

- (void) selectRowBy:(int)aRows
{
  int row = [mTableView selectedRow];
  
  if (row == -1 && aRows < 0) {
    // if nothing is selected and you scroll up, go to last row
    row = [mTableView numberOfRows]-1;
  } else if (row == [mTableView numberOfRows]-1 && aRows == 1) {
    // if the last row is selected and you scroll down, do nothing. pins
    // the selection at the bottom.
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

#pragma mark -

// event handlers ////////////////////////////////////////////

- (void) onRowClicked:(NSNotification *)aNote
{
  [self enterResult:[mTableView clickedRow]];
  [[[self window] windowController] goToLocationFromToolbarURLField:self];
}

- (void) onBlur:(NSNotification *)aNote
{
  // close up the popup and make sure we clear any past selection. We cannot
  // use |selectAt:-1| because that would show the popup, even for a brief instant
  // and cause flashing.
  [mTableView deselectAll:self];
  [self closePopup];
}

- (void) onResize:(NSNotification *)aNote
{
  if ([aNote object] == [self window])
    [self resizePopup:NO];
}

- (void) onUndoOrRedo:(NSNotification *)aNote
{
  [mTableView deselectAll:self];
  [self clearResults];
}

#pragma mark -

// Drag & Drop Methods ///////////////////////////////////////
- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
  NSDragOperation sourceDragMask = [sender draggingSourceOperationMask];
  NSPasteboard *pboard = [sender draggingPasteboard];
  if ( [[pboard types] containsObject:NSURLPboardType] ||
       [[pboard types] containsObject:NSStringPboardType] ||
       [[pboard types] containsObject:kCorePasteboardFlavorType_url]) {
    if (sourceDragMask & NSDragOperationCopy) {
      return NSDragOperationCopy;
    }
  }
  return NSDragOperationNone;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
  NSPasteboard *pboard = [sender draggingPasteboard];
  NSString *dragString = nil;
  if ( [[pboard types] containsObject:kCorePasteboardFlavorType_url] )
    dragString = [pboard stringForType:kCorePasteboardFlavorType_url];
  else if ( [[pboard types] containsObject:NSURLPboardType] )
    dragString = [[NSURL URLFromPasteboard:pboard] absoluteString];
  else if ( [[pboard types] containsObject:NSStringPboardType] ) {
    dragString = [pboard stringForType:NSStringPboardType];
    // Clean the string on the off chance it has line breaks, etc.
    dragString = [dragString stringByRemovingCharactersInSet:[NSCharacterSet controlCharacterSet]];
  }
  [self setStringValue:dragString];
  return YES;
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
  [self selectText:self];
}

#pragma mark -

// NSTextField delegate //////////////////////////////////
- (void)controlTextDidChange:(NSNotification *)aNote
{
  NSTextView *fieldEditor = [[aNote userInfo] objectForKey:@"NSFieldEditor"];

  // we are here either because the user is typing or they are selecting
  // an item in the autocomplete popup. When they are typing, the location of
  // the selection will be non-zero (wherever the insertion point is). When
  // they autocomplete, the length and the location will both be zero.
  // We use this info in the following way: if they are typing or backspacing,
  // restart the search and no longer use the selection in the popup.
  NSRange range = [fieldEditor selectedRange];
  NSString* currentText = [fieldEditor string];
  if ([currentText length] && range.location) {
    // when we ask for a NSTextView string, Cocoa returns
    // a pointer to the view's backing store.  So, the value
    // of the string continually changes as we edit the text view.
    // Since we'll edit the text view as we add in autocomplete results,
    // we've got to make a copy of the string as it currently stands
    // to know what we were searching for in the first place.
    NSString *searchString = [NSString stringWithString:currentText];
    [self startSearch:searchString complete:!mBackspaced];
  }
  else if (([mTableView selectedRow] == -1) || mBackspaced)
    [self clearResults];
  
  mBackspaced = NO;
}

- (void)controlTextDidEndEditing:(NSNotification *)aNote
{
  [self clearResults];
  id fieldEditor = [[aNote userInfo] objectForKey:@"NSFieldEditor"];
  [[fieldEditor undoManager] removeAllActionsWithTarget:fieldEditor];
}
  
- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView doCommandBySelector:(SEL)command
{
  if (command == @selector(insertNewline:)) {
    [self enterResult:[mTableView selectedRow]];
    [mTableView deselectAll:self];
  } else if (command == @selector(moveUp:)) {
    if ([self isOpen]) {
      [self selectRowBy:-1];
      [self completeSelectedResult];
      return YES;
    }
  } else if (command == @selector(moveDown:)) {
    if ([self isOpen]) {
      [self selectRowBy:1];
      [self completeSelectedResult];
      return YES;
    }
    else if ([self caretIsAtEndOfLine]) {
      [self startSearch:[self stringValue] complete:YES];
      return YES;
    }
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
  } else if (command == @selector(complete:)) {
      [self selectRowBy:1];
      [self completeSelectedResult];
      return YES;
  } else if (command == @selector(insertTab:)) {
    if ([mPopupWin isVisible]) {
      [self selectRowBy:1];
      [self completeSelectedResult];
      return YES;
    } else {
      // use the normal key view unless we know more about our siblings and have
      // explicitly nil'd out the next key view. In that case, select the window
      // to break out of the toolbar and tab through the rest of the window
      if ([self nextKeyView])
        [[self window] selectKeyViewFollowingView:self];
      else {
        NSWindow* wind = [self window];
        [wind makeFirstResponder:wind];
      }
    }
  } else if (command == @selector(deleteBackward:) || 
             command == @selector(deleteForward:)) {
    // if the user deletes characters, we need to know so that
    // we can prevent autocompletion later when search results come in
    if ([[textView string] length] > 1) {
      [self selectRowAt:-1];
      mBackspaced = YES;
    }
  }
  
  return NO;
}

#pragma mark -

// NSWindow delegate methods. We're only the window's delegate when we're inside
// the open location sheet.

- (void)windowWillClose:(NSNotification *)aNotification
{
  // make sure we hide the autocomplete popup when the Location sheet is dismissed
  [self clearResults];
}

@end
