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
 *   Calum Robinson <calumr@mac.com>
 *   Josh Aas <josh@mozilla.com>
 *   Nick Kreeger <nick.kreeger@park.edu>
 *   Bruce Davidson <mozilla@transoceanic.org.uk>
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

#include "nsNetError.h"

#import "NSString+Utils.h"
#import "NSView+Utils.h"

#import "ProgressDlgController.h"

#import "PreferenceManager.h"
#import "ProgressView.h"

static NSString* const kProgressWindowFrameSaveName = @"ProgressWindow";

@interface ProgressDlgController(PrivateProgressDlgController)

-(void)showErrorSheetForDownload:(id <CHDownloadProgressDisplay>)progressDisplay withStatus:(nsresult)inStatus;
-(void)rebuildViews;
-(NSArray*)selectedProgressViewControllers;
-(ProgressViewController*)progressViewControllerAtIndex:(unsigned)inIndex;
-(void)deselectDLInstancesInArray:(NSArray*)instances;
-(void)scrollIntoView:(ProgressViewController*)controller;
-(void)killDownloadTimer;
-(void)setupDownloadTimer;
-(BOOL)shouldAllowCancelAction;
-(BOOL)shouldAllowRemoveAction;
-(BOOL)shouldAllowPauseAction;
-(BOOL)shouldAllowResumeAction;
-(BOOL)shouldAllowOpenAction;
-(BOOL)fileExistsForSelectedItems;
-(void)maybeCloseWindow;
-(void)removeSuccessfulDownloads;
-(BOOL)shouldRemoveDownloadsOnQuit;
-(NSString*)downloadsPlistPath;
- (void)setToolTipForToolbarItem:(NSToolbarItem *)theItem;

@end

#pragma mark -

@implementation ProgressDlgController

static id gSharedProgressController = nil;

+(ProgressDlgController *)sharedDownloadController
{
  if (gSharedProgressController == nil) {
    gSharedProgressController = [[ProgressDlgController alloc] init];
  }
  
  return gSharedProgressController;
}

+(ProgressDlgController *)existingSharedDownloadController
{
  return gSharedProgressController;
}

-(id)init
{
  if ((self = [super initWithWindowNibName:@"ProgressDialog"]))
  {
    mProgressViewControllers = [[NSMutableArray alloc] init];
    mDefaultWindowSize = [[self window] frame].size;
    // it would be nice if we could get the frame from the name, and then
    // mess with it before setting it.
    [[self window] setFrameUsingName:kProgressWindowFrameSaveName];
    
    // these 2 bools prevent the app from locking up when the termination modal sheet
    // is running and a download completes
    mAwaitingTermination = NO;
    mShouldCloseWindow   = NO;
    
    // we "know" that the superview of the stack view is a CHFlippedShrinkWrapView
    // (it has to be, because NSScrollViews have to contain a flipped view)
    if ([[mStackView superview] respondsToSelector:@selector(setNoIntrinsicPadding)])
      [(CHShrinkWrapView*)[mStackView superview] setNoIntrinsicPadding];

    // We provide the views for the stack view, from mProgressViewControllers
    [mStackView setShowsSeparators:YES];
    [mStackView setDataSource:self];

    mSelectionPivotIndex = -1;

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(DLInstanceSelected:)
                                                 name:kDownloadInstanceSelectedNotificationName
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(DLInstanceOpened:)
                                                 name:kDownloadInstanceOpenedNotificationName
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(cancel:)
                                                 name:kDownloadInstanceCancelledNotificationName
                                               object:nil];
  }
  return self;
}

-(void)awakeFromNib
{
  NSToolbar *toolbar = [[NSToolbar alloc] initWithIdentifier:@"dlmanager1"]; // so pause/resume button will show
  [toolbar setDelegate:self];
  [toolbar setAllowsUserCustomization:YES];
  [toolbar setAutosavesConfiguration:YES];
  [[self window] setToolbar:toolbar];    
  
  mFileChangeWatcher = [[FileChangeWatcher alloc] init];
  
  // load the saved instances to mProgressViewControllers array
  [self loadProgressViewControllers];
}

-(void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  if (self == gSharedProgressController) {
    gSharedProgressController = nil;
  }
  [mProgressViewControllers release];
  [mFileChangeWatcher release];
  [self killDownloadTimer];
  [super dealloc];
}

// cancel all selected instances
-(IBAction)cancel:(id)sender
{
  [[self selectedProgressViewControllers] makeObjectsPerformSelector:@selector(cancel:) withObject:sender];
}

// reveal all selected instances in the Finder
-(IBAction)reveal:(id)sender
{
  [[self selectedProgressViewControllers] makeObjectsPerformSelector:@selector(reveal:) withObject:sender];
}

// open all selected instances
-(IBAction)open:(id)sender
{
  [[self selectedProgressViewControllers] makeObjectsPerformSelector:@selector(open:) withObject:sender];
}

//
// Take care of selecting a download instance to replace the selection being removed
//
-(void)setSelectionForRemovalOfItems:(NSArray*) selectionToRemove
{
  // take care of selecting a download instance to replace the selection being removed
  NSArray* selected = [self selectedProgressViewControllers];
  unsigned int selectedCount = [selected count];
  if (selectedCount == 0) return;
  
  unsigned int indexOfLastSelection = [mProgressViewControllers indexOfObject:[selected lastObject]];
  // if dl instance after last selection exists, select it or look for something else to select
  if ((indexOfLastSelection + 1) < [mProgressViewControllers count]) {
    [(ProgressViewController*)[mProgressViewControllers objectAtIndex:(indexOfLastSelection + 1)] setSelected:YES];
  }
  else { // find the first unselected DL instance before the last one marked for removal and select it
         // use an int in the loop, not unsigned because we might make it negative
    for (int i = ([mProgressViewControllers count] - 1); i >= 0; i--)
    {
      ProgressViewController* curProgressViewController = [mProgressViewControllers objectAtIndex:i];
      if (![curProgressViewController isSelected])
      {
        [curProgressViewController setSelected:YES];
        break;
      }
    }
  }
  
  mSelectionPivotIndex = -1; // nothing is selected any more so nothing to pivot on
}

// remove all selected instances, don't remove anything that is active as a guard against bad things
-(IBAction)remove:(id)sender
{
  NSArray* selected = [self selectedProgressViewControllers];
  unsigned int selectedCount = [selected count];
  
  [self setSelectionForRemovalOfItems:selected];
  
  // now remove stuff
  for (unsigned int i = 0; i < selectedCount; i++)
  {
    ProgressViewController* selProgressViewController = [selected objectAtIndex:i];
    if (![selProgressViewController isActive])
      [self removeDownload:selProgressViewController suppressRedraw:YES];
  }
  
  [self rebuildViews];
  [self saveProgressViewControllers];
}

// delete the selected download(s), moving files to trash and clearing them from the list
-(IBAction)deleteDownloads:(id)sender
{
  NSArray* selected = [self selectedProgressViewControllers];
  unsigned int selectedCount = [selected count];
  
  [self setSelectionForRemovalOfItems:selected];
  
  // now remove stuff, don't need to check if active, the toolbar/menu validates
  for (unsigned int i = 0; i < selectedCount; i++) {
    [[selected objectAtIndex:i] deleteFile:sender];
    [self removeDownload:[selected objectAtIndex:i] suppressRedraw:YES];
  }
  
  [self rebuildViews];
  [self saveProgressViewControllers];
}

-(IBAction)pause:(id)sender
{
  [[self selectedProgressViewControllers] makeObjectsPerformSelector:@selector(pause:) withObject:sender];
  [self rebuildViews];  // because we swap in a different progress view
}

-(IBAction)resume:(id)sender
{
  [[self selectedProgressViewControllers] makeObjectsPerformSelector:@selector(resume:) withObject:sender];
  [self rebuildViews];  // because we swap in a different progress view
}

// remove all inactive instances
-(IBAction)cleanUpDownloads:(id)sender
{
  for (int i = [mProgressViewControllers count] - 1; i >= 0; i--)
  {
    ProgressViewController* curProgressViewController = [mProgressViewControllers objectAtIndex:i];
    if ((![curProgressViewController isActive]) || [curProgressViewController isCanceled])
      [self removeDownload:curProgressViewController suppressRedraw:YES];
  }
  mSelectionPivotIndex = -1;
  
  [self rebuildViews];
  [self saveProgressViewControllers];
}

// remove all downloads, cancelling if necessary
// this is used for the browser reset function
-(void)clearAllDownloads
{
  for (int i = [mProgressViewControllers count] - 1; i >= 0; i--)
  {
    ProgressViewController* curProgressViewController = [mProgressViewControllers objectAtIndex:i];
    // the ProgressViewController method "cancel:" has a sanity check, so its ok to call on anything
    // make sure downloads are not active before removing them
    [curProgressViewController cancel:self];
    [self removeDownload:curProgressViewController suppressRedraw:YES];
  }
  mSelectionPivotIndex = -1;
  
  [self rebuildViews];
  [self saveProgressViewControllers];
}

//
// -DLInstanceOpened:
// 
// Called when one of the ProgressView's should be opened (dbl clicked, for example). Open all of the
// selected instances with the Finder.
//
-(void)DLInstanceOpened:(NSNotification*)notification
{
  if ([self fileExistsForSelectedItems])
    [self open:self];
}

// calculate what buttons should be enabled/disabled because the user changed the selection state
-(void)DLInstanceSelected:(NSNotification*)notification
{
  // make sure the notification object is the kind we want
  ProgressView* selectedView = ((ProgressView*)[notification object]);
  if (![selectedView isKindOfClass:[ProgressView class]])
    return;

  ProgressViewController* selectedViewController = [selectedView getController];
  
  NSArray* selectedArray = [self selectedProgressViewControllers];

  int indexOfSelectedController = (int)[mProgressViewControllers indexOfObject:selectedViewController];
  
  // check for key modifiers and select more instances if appropriate
  // if its shift key, extend the selection one way or another
  // if its command key, just let the selection happen wherever it is (don't mess with it here)
  // if its not either clear all selections except the one that just happened
  switch ([selectedView lastModifier])
  {
    case kNoKey:
      // deselect everything
      [self deselectDLInstancesInArray:selectedArray];
      [selectedViewController setSelected:YES];
      mSelectionPivotIndex = indexOfSelectedController;
      break;

    case kCommandKey:
      if (![selectedViewController isSelected])
      {
        // if this was at the pivot index set the pivot index to -1
        if (indexOfSelectedController == mSelectionPivotIndex)
          mSelectionPivotIndex = -1;
      }
      else
      {
        if ([selectedArray count] == 1)
          mSelectionPivotIndex = indexOfSelectedController;
      }
      break;

    case kShiftKey:
      if (mSelectionPivotIndex == -1)
        mSelectionPivotIndex = indexOfSelectedController;
      else
      { 
        if ([selectedArray count] == 1)
            mSelectionPivotIndex = indexOfSelectedController;
        else
        {
          // deselect everything
          [self deselectDLInstancesInArray:selectedArray];
          if (indexOfSelectedController <= mSelectionPivotIndex)
          {
            for (int i = indexOfSelectedController; i <= mSelectionPivotIndex; i++)
              [(ProgressViewController*)[mProgressViewControllers objectAtIndex:i] setSelected:YES];
          }
          else if (indexOfSelectedController > mSelectionPivotIndex) 
          {
            for (int i = mSelectionPivotIndex; i <= indexOfSelectedController; i++)
              [(ProgressViewController*)[mProgressViewControllers objectAtIndex:i] setSelected:YES];
          }
        }
      }
      break;
  }
}

-(void)keyDown:(NSEvent *)theEvent
{
  // we don't care about anything if no downloads exist
  if ([mProgressViewControllers count] == 0)
  {
    NSBeep();
    return;
  }

  int instanceToSelect = -1;
  BOOL shiftKeyDown = (([theEvent modifierFlags] & NSShiftKeyMask) != 0);

  if ([[theEvent characters] length] < 1)
    return;
  unichar key = [[theEvent characters] characterAtIndex:0];
  switch (key)
  {
    case NSUpArrowFunctionKey:
      {
        // find the first selected item
        int i; // we use this outside the loop so declare it here
        for (i = 0; i < (int)[mProgressViewControllers count]; i++) {
          if ([[mProgressViewControllers objectAtIndex:i] isSelected]) {
            break;
          }
        }      
        // deselect everything if the shift key isn't a modifier
        if (!shiftKeyDown)
          [self deselectDLInstancesInArray:[self selectedProgressViewControllers]];

        if (i == (int)[mProgressViewControllers count]) // if nothing was selected select the first item
          instanceToSelect = 0;
        else if (i == 0) // if selection was already at the top leave it there
          instanceToSelect = 0;
        else // select the next highest instance
          instanceToSelect = i - 1;

        // select and make sure its visible
        if (instanceToSelect != -1)
        {
          ProgressViewController* dlToSelect = [mProgressViewControllers objectAtIndex:instanceToSelect];
          [dlToSelect setSelected:YES];
          [self scrollIntoView:dlToSelect];
          if (!shiftKeyDown)
            mSelectionPivotIndex = instanceToSelect;
        }
      }
      break;

    case NSDownArrowFunctionKey:
      {
        // find the last selected item
        int i; // we use this outside the coming loop so declare it here
        for (i = [mProgressViewControllers count] - 1; i >= 0 ; i--) {
          if ([[mProgressViewControllers objectAtIndex:i] isSelected])
            break;
        }

        // deselect everything if the shift key isn't a modifier
        if (!shiftKeyDown)
          [self deselectDLInstancesInArray:[self selectedProgressViewControllers]];

        if (i < 0) // if nothing was selected select the first item
          instanceToSelect = ([mProgressViewControllers count] - 1);
        else if (i == ((int)[mProgressViewControllers count] - 1)) // if selection was already at the bottom leave it there
          instanceToSelect = ([mProgressViewControllers count] - 1);
        else // select the next lowest instance
          instanceToSelect = i + 1;

        if (instanceToSelect != -1)
        {
          ProgressViewController* dlToSelect = [mProgressViewControllers objectAtIndex:instanceToSelect];
          [dlToSelect setSelected:YES];
          [self scrollIntoView:dlToSelect];
          if (!shiftKeyDown)
            mSelectionPivotIndex = instanceToSelect;
        }
      }
      break;

    case NSDeleteFunctionKey:
    case NSDeleteCharacter:
      { // delete or fwd-delete key - remove all selected items unless an active one is selected
        if ([self shouldAllowRemoveAction])
          [self remove:self];
        else
          NSBeep();
      }
      break;

    case NSPageUpFunctionKey:
      if ([mProgressViewControllers count] > 0) {
        // make the first instance completely visible
        [self scrollIntoView:((ProgressViewController*)[mProgressViewControllers objectAtIndex:0])];
      }
      break;

    case NSPageDownFunctionKey:
      if ([mProgressViewControllers count] > 0) {
        // make the last instance completely visible
        [self scrollIntoView:((ProgressViewController*)[mProgressViewControllers lastObject])];
      }
      break;

    default:
      NSBeep();
      break;
  }
}

-(void)deselectDLInstancesInArray:(NSArray*)instances
{
  unsigned count = [instances count];
  for (unsigned i = 0; i < count; i++) {
    [(ProgressViewController*)[instances objectAtIndex:i] setSelected:NO];
  }
}

// return a mutable array with instance in order top-down
-(NSArray*)selectedProgressViewControllers
{
  NSMutableArray *selectedArray = [[NSMutableArray alloc] init];
  unsigned selectedCount = [mProgressViewControllers count];
  for (unsigned i = 0; i < selectedCount; i++) {
    if ([[mProgressViewControllers objectAtIndex:i] isSelected]) {
      // insert at zero so they're in order to-down
      [selectedArray addObject:[mProgressViewControllers objectAtIndex:i]];
    }
  }
  [selectedArray autorelease];
  return selectedArray;
}

-(ProgressViewController*)progressViewControllerAtIndex:(unsigned)inIndex
{
  return (ProgressViewController*) [mProgressViewControllers objectAtIndex:inIndex];
}

-(void)scrollIntoView:(ProgressViewController*)controller
{
  NSView* dlView = [controller view];
  NSRect instanceFrame = [[mScrollView contentView] convertRect:[dlView bounds] fromView:dlView];
  NSRect visibleRect = [[mScrollView contentView] documentVisibleRect];

  if (!NSContainsRect(visibleRect, instanceFrame)) { // if instance isn't completely visible
    if (instanceFrame.origin.y < visibleRect.origin.y) { // if the dl instance is at least partly above visible rect
      // just go to the instance's frame origin point
      [[mScrollView contentView] scrollToPoint:instanceFrame.origin];
    }
    else { // if the dl instance is at least partly below visible rect
      // take  instance's frame origin y, subtract content view height, 
      // add instance view height, no parenthesizing
      NSPoint adjustedPoint = NSMakePoint(0, (instanceFrame.origin.y - NSHeight([[mScrollView contentView] frame])) + NSHeight(instanceFrame));
      [[mScrollView contentView] scrollToPoint:adjustedPoint];
    }
    [mScrollView reflectScrolledClipView:[mScrollView contentView]];
  }
}

-(void)didStartDownload:(ProgressViewController*)progressDisplay
{
  if (![[self window] isVisible]) {
    [self showWindow:nil]; // make sure the window is visible
  }
  
  // a common cause of user confusion is the window being visible but behind other windows. They
  // have no idea the download was successful, and click the link two or three times before
  // looking around to see what happened. We always want the download window to come to the
  // front on a new download (unless they set a user_pref);
  BOOL gotPref = NO;
  BOOL bringToFront = [[PreferenceManager sharedInstance] getBooleanPref:"browser.download.progressDnldDialog.bringToFront" withSuccess:&gotPref];
  if (gotPref && bringToFront)
    [[self window] makeKeyAndOrderFront:self];
  
  [self rebuildViews];
  [self setupDownloadTimer];
  
  // downloads should be individually selected when initiated
  [self deselectDLInstancesInArray:[self selectedProgressViewControllers]];
  [(ProgressViewController*)progressDisplay setSelected:YES];
  
  // make sure new download is visible
  [self scrollIntoView:progressDisplay];
}

-(void)didEndDownload:(id <CHDownloadProgressDisplay>)progressDisplay withSuccess:(BOOL)completedOK statusCode:(nsresult)status
{
  [self rebuildViews]; // to swap in the completed view
  [[[self window] toolbar] validateVisibleItems]; // force update which doesn't always happen

  // close the window if user has set pref to close when all downloads complete
  if (completedOK)
  {
    if (!mAwaitingTermination)
      [self maybeCloseWindow];
    else
      mShouldCloseWindow = YES;
  }
  else if (NS_FAILED(status) && status != NS_BINDING_ABORTED)  // if it's an error, and not just canceled, show sheet
  {
    [self showErrorSheetForDownload:progressDisplay withStatus:status];
  }
  
  [self saveProgressViewControllers];
}

-(void)maybeCloseWindow
{
  // only check if there are zero downloads running
  if ([self numDownloadsInProgress] == 0)
  {
    BOOL gotPref;
    BOOL keepDownloadsOpen = [[PreferenceManager sharedInstance] getBooleanPref:"browser.download.progressDnldDialog.keepAlive" withSuccess:&gotPref];
    if (gotPref && !keepDownloadsOpen)
    {
      // don't call -performClose: on the window, because we don't want Cocoa to look
      // for the option key and try to close all windows
      [self close];
    }
  }
}

-(void)showErrorSheetForDownload:(id <CHDownloadProgressDisplay>)progressDisplay withStatus:(nsresult)inStatus
{
  NSString* errorMsgFmt = NSLocalizedString(@"DownloadErrorMsgFmt", @"");
  NSString* errorExplString = nil;
  
  NSString* destFilePath = [progressDisplay destinationPath];
  NSString* fileName = [destFilePath displayNameOfLastPathComponent];
  
  NSString* errorMsg = [NSString stringWithFormat:errorMsgFmt, fileName];

  switch (inStatus)
  {
    case NS_ERROR_FILE_DISK_FULL:
    case NS_ERROR_FILE_NO_DEVICE_SPACE:
      {
        NSString* fmtString = NSLocalizedString(@"DownloadErrorNoDiskSpaceOnVolumeFmt", @"");
        errorExplString = [NSString stringWithFormat:fmtString, [destFilePath volumeNamePathComponent]];
      }
      break;
      
    case NS_ERROR_FILE_ACCESS_DENIED:
      {
        NSString* fmtString = NSLocalizedString(@"DownloadErrorDestinationWriteProtectedFmt", @"");
        NSString* destDirPath = [destFilePath stringByDeletingLastPathComponent];
        errorExplString = [NSString stringWithFormat:fmtString, [destDirPath displayNameOfLastPathComponent]];
      }
      break;

    case NS_ERROR_FILE_TOO_BIG:
    case NS_ERROR_FILE_READ_ONLY:
    default:
      {
        errorExplString = NSLocalizedString(@"DownloadErrorOther", @"");
        NSLog(@"Download failure code: %X", inStatus);
      }
      break;
  }
  
  NSBeginAlertSheet(errorMsg,
                    nil,    // default button ("OK")
                    nil,    // alt button (none)
                    nil,    // other button (nil)
                    [self window],
                    self,
                    @selector(downloadErrorSheetDidEnd:returnCode:contextInfo:),
                    nil,    // didDismissSelector
                    NULL,   // context info
                    errorExplString);
}

-(void)downloadErrorSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
}

-(void)removeDownload:(id <CHDownloadProgressDisplay>)progressDisplay suppressRedraw:(BOOL)suppressRedraw
{
  [progressDisplay displayWillBeRemoved];
  // This is sometimes called by code that thinks it can continue
  // to use |progressDisplay|. Extended the lifetime slightly as a
  // band-aid, but this logic should really be reworked.
  [[progressDisplay retain] autorelease];
  [mProgressViewControllers removeObject:progressDisplay];
  
  if ([mProgressViewControllers count] == 0) {
    // Stop doing stuff if there aren't any downloads going on
    [self killDownloadTimer];
  }
  
  if (!suppressRedraw)
    [self rebuildViews];
}

-(void)rebuildViews
{
  [mStackView adaptToSubviews];
}

-(int)numDownloadsInProgress
{
  unsigned numViews = [mProgressViewControllers count];
  int numActive = 0;
  
  for (unsigned int i = 0; i < numViews; i++) {
    if ([[mProgressViewControllers objectAtIndex:i] isActive]) {
      ++numActive;
    }
  }
  return numActive;
}

-(void)autosaveWindowFrame
{
  [[self window] saveFrameUsingName:kProgressWindowFrameSaveName];
}

-(void)windowWillClose:(NSNotification *)notification
{
  [self autosaveWindowFrame];
}

-(void)killDownloadTimer
{
  if (mDownloadTimer) {
    [mDownloadTimer invalidate];
    [mDownloadTimer release];
    mDownloadTimer = nil;
  }
}

// Called by our timer to refresh all the download stats
- (void)setDownloadProgress:(NSTimer *)aTimer
{
  [mProgressViewControllers makeObjectsPerformSelector:@selector(refreshDownloadInfo)];
}

- (void)setupDownloadTimer
{
  [self killDownloadTimer];
  // note that this sets up a retain cycle between |self| and the timer,
  // which has to be broken out of band, before we'll be dealloc'd.
  mDownloadTimer = [[NSTimer scheduledTimerWithTimeInterval:1.0
                                                     target:self
                                                   selector:@selector(setDownloadProgress:)
                                                   userInfo:nil
                                                    repeats:YES] retain];
  // make sure it fires even when the mouse is down
  [[NSRunLoop currentRunLoop] addTimer:mDownloadTimer forMode:NSEventTrackingRunLoopMode];
}

-(NSApplicationTerminateReply)allowTerminate
{
  BOOL shouldTerminate = YES;
  BOOL downloadsInProgress = ([self numDownloadsInProgress] > 0 ? YES : NO);
	
  if (downloadsInProgress)
  {
    // make sure the window is visible
    [self showWindow:self];
    
    NSString *alert     = NSLocalizedString(@"QuitWithDownloadsMsg", nil);
    NSString *message   = NSLocalizedString(@"QuitWithDownloadsExpl", nil);
    NSString *okButton  = NSLocalizedString(@"QuitWithdownloadsButtonDefault", nil);
    NSString *altButton = NSLocalizedString(@"QuitButtonText", nil);
    
    // while the panel is up, download dialogs won't update (no timers firing) but
    // downloads continue (PLEvents being processed)
    id panel = NSGetAlertPanel(alert, message, okButton, altButton, nil, message);
    
    // set this bool to true so that if a download finishes while the modal sheet is up we don't lock
    mAwaitingTermination = YES;
    
    [NSApp beginSheet:panel
       modalForWindow:[self window]
        modalDelegate:self
       didEndSelector:@selector(sheetDidEnd:returnCode:contextInfo:)
          contextInfo:NULL];
    int sheetResult = [NSApp runModalForWindow: panel];
    [NSApp endSheet: panel];
    [panel orderOut: self];
    NSReleaseAlertPanel(panel);
    
    if (sheetResult == NSAlertDefaultReturn)
    {
      mAwaitingTermination = NO;
      
      // Check to see if a request to close the download window was made while the modal sheet was running.
      // If so, close the download window according to the user's pref.
      if (mShouldCloseWindow)
      {
        [self maybeCloseWindow];
        mShouldCloseWindow = NO;
      }
      
      shouldTerminate = NO;
    }
  }
  
  return shouldTerminate ? NSTerminateNow : NSTerminateCancel;
}

// Called by MainController when the application is about to terminate.
// Either save the progress view's or remove them according to the download removal pref.
-(void)applicationWillTerminate
{
  // Check the download item removal policy here to see if downloads should be removed when camino quits.
  if ([self shouldRemoveDownloadsOnQuit])
    [self removeSuccessfulDownloads];
  
  // Since the pref is not set to remove the downloads when Camino quits, save them here before the app terminates.
  else  
    [self saveProgressViewControllers];
}

-(void)sheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
  [NSApp stopModalWithCode:returnCode];
}

-(void)saveProgressViewControllers
{
  unsigned int arraySize = [mProgressViewControllers count];
  NSMutableArray* downloadArray = [[[NSMutableArray alloc] initWithCapacity:arraySize] autorelease];
  
  NSEnumerator* downloadsEnum = [mProgressViewControllers objectEnumerator];
  ProgressViewController* curController;
  while ((curController = [downloadsEnum nextObject]))
  {
    [downloadArray addObject:[curController downloadInfoDictionary]]; 
  }
  
  // now save the array
  [downloadArray writeToFile:[self downloadsPlistPath] atomically: YES];
}

-(void)loadProgressViewControllers
{
  NSArray*  downloads = [NSArray arrayWithContentsOfFile:[self downloadsPlistPath]];
  
  if (downloads)
  {
    NSEnumerator* downloadsEnum = [downloads objectEnumerator];
    NSDictionary* downloadsDictionary;
    while((downloadsDictionary = [downloadsEnum nextObject]))
    {
      ProgressViewController* curController = [[ProgressViewController alloc] initWithDictionary:downloadsDictionary
                                                                             andWindowController:self];
      [mProgressViewControllers addObject:curController];
      [curController release];
    }
    
    [self rebuildViews];
  }
}

-(void)addFileDelegateToWatchList:(id<WatchedFileDelegate>)aWatchedFileDelegate
{
  [mFileChangeWatcher addWatchedFileDelegate:aWatchedFileDelegate];
}

-(void)removeFileDelegateFromWatchList:(id<WatchedFileDelegate>)aWatchedFileDelegate
{
  [mFileChangeWatcher removeWatchedFileDelegate:aWatchedFileDelegate];
}

// Remove the successful downloads from the downloads list
-(void)removeSuccessfulDownloads
{
  NSEnumerator* downloadsEnum = [mProgressViewControllers objectEnumerator];
  ProgressViewController* curProgView = nil;
  
  while ((curProgView = [downloadsEnum nextObject]))
  {
    // Remove successful downloads from the list
    if ([curProgView hasSucceeded])
      [mProgressViewControllers removeObject:curProgView];
  }
  
  [self saveProgressViewControllers];
}

// Return true if the pref is set to remove downloads when the application quits
-(BOOL)shouldRemoveDownloadsOnQuit
{
  int downloadRemovalPolicy = [[PreferenceManager sharedInstance] getIntPref:"browser.download.downloadRemoveAction" 
                                                                 withSuccess:NULL];
  
  return downloadRemovalPolicy == kRemoveDownloadsOnQuitPrefValue ? YES : NO;
}

// Get the downloads.plist path
-(NSString*)downloadsPlistPath
{
  return [[[PreferenceManager sharedInstance] profilePath] stringByAppendingPathComponent:@"downloads.plist"];
}

-(BOOL)shouldAllowCancelAction
{
  // if no selections are inactive or canceled then allow cancel
  NSEnumerator* progViewEnum = [[self selectedProgressViewControllers] objectEnumerator];
  ProgressViewController* curController;
  while ((curController = [progViewEnum nextObject]))
  {
    if ((![curController isActive]) || [curController isCanceled])
      return NO;
  }
  return YES;
}

-(BOOL)shouldAllowRemoveAction
{
  // if no selections are active then allow remove
  NSEnumerator* progViewEnum = [[self selectedProgressViewControllers] objectEnumerator];
  ProgressViewController* curController;
  while ((curController = [progViewEnum nextObject]))
  {
    if ([curController isActive])
      return NO;
  }
  return YES;
}

-(BOOL)shouldAllowMoveToTrashAction
{
  NSEnumerator* progViewEnum = [[self selectedProgressViewControllers] objectEnumerator];
  ProgressViewController* curController;
  while ((curController = [progViewEnum nextObject]))
  {
    if ([curController isActive] || ![curController fileExists]) {
      return NO;
    }
  }
  return YES;
}

-(BOOL)fileExistsForSelectedItems
{
  NSEnumerator* progViewEnum = [[self selectedProgressViewControllers] objectEnumerator];
  ProgressViewController* curController;
  while ((curController = [progViewEnum nextObject]))
  {
    if ([curController isCanceled] || ![curController fileExists])
      return NO;
  }
  
  return YES;
}

-(BOOL)shouldAllowOpenAction
{
  NSEnumerator* progViewEnum = [[self selectedProgressViewControllers] objectEnumerator];
  ProgressViewController* curController;
  while ((curController = [progViewEnum nextObject]))
  {
    if ([curController isActive] || [curController isCanceled] || ![curController fileExists])
      return NO;
  }
  
  return YES;
}

- (BOOL)shouldAllowPauseAction
{
  NSEnumerator* progViewEnum = [[self selectedProgressViewControllers] objectEnumerator];
  ProgressViewController* curController;
  while ((curController = [progViewEnum nextObject]))
  {
    if ([curController isPaused] || ![curController isActive])
      return NO;
  }
  
  return YES;
}

-(BOOL)shouldAllowResumeAction
{
  NSEnumerator* progViewEnum = [[self selectedProgressViewControllers] objectEnumerator];
  ProgressViewController* curController;
  while ((curController = [progViewEnum nextObject]))
  {
    if (![curController isPaused] || ![curController isActive])
      return NO;
  }
  
  return YES;
}

-(BOOL)validateMenuItem:(NSMenuItem*)menuItem
{
  SEL action = [menuItem action];
  if (action == @selector(cancel:)) {
    return [self shouldAllowCancelAction];
  }
  else if (action == @selector(remove:)) {
    return [self shouldAllowRemoveAction];
  }
  else if (action == @selector(open:)) {
    return [self shouldAllowOpenAction];
  }
  else if (action == @selector(reveal:)) {
    return [self fileExistsForSelectedItems];
  }
  else if (action == @selector(pause:)) {
    return [self shouldAllowPauseAction];
  }
  else if (action == @selector(resume:)) {
    return [self shouldAllowResumeAction];
  }
  else if (action == @selector(deleteDownloads:)) {
    return [self shouldAllowMoveToTrashAction];
  }
  return YES;
}


- (BOOL)setPauseResumeToolbarItem:(NSToolbarItem*)theItem
{
  // since this is also the method that gets called when the customize dialog is run
  // set the default value of the label, tooltip, icon, and pallete label to pause
  // only set the action of the selector to pause if we validate properly
  [theItem setLabel:NSLocalizedString(@"dlPauseButtonLabel", nil)];
  [theItem setPaletteLabel:NSLocalizedString(@"dlPauseButtonLabel", nil)];
  [theItem setImage:[NSImage imageNamed:@"dl_pause.tif"]];
  
  if ([self shouldAllowPauseAction]) {
    [theItem setAction:@selector(pause:)];
    
    return [[self window] isKeyWindow]; // if not key window, dont enable
  }
  else if ([self shouldAllowResumeAction]) {
    [theItem setLabel:NSLocalizedString(@"dlResumeButtonLabel", nil)];
    [theItem setPaletteLabel:NSLocalizedString(@"dlResumeButtonLabel", nil)];
    [theItem setAction:@selector(resume:)];
    [theItem setImage:[NSImage imageNamed:@"dl_resume.tif"]];
    
    return [[self window] isKeyWindow]; // if not key window, dont enable
  }
  else {
    return NO;
  }
}

-(BOOL)validateToolbarItem:(NSToolbarItem *)theItem
{
  SEL action = [theItem action];

  // validate items not dependent on the current selection.  Must include all such items.
  if (action == @selector(showWindow:))
    return YES;
  else if (action == @selector(cleanUpDownloads:)) {
    if (![[self window] isKeyWindow])
      return NO; //XXX Get rid of me once we can resume cancelled/failed downloads

    unsigned pcControllersCount = [mProgressViewControllers count];
    for (unsigned i = 0; i < pcControllersCount; i++)
    {
      ProgressViewController* curController = [mProgressViewControllers objectAtIndex:i];
      if ((![curController isActive]) || [curController isCanceled])
        return YES;
    }
    return NO;
  }

  // validate items that depend on current selection
  if ([[self selectedProgressViewControllers] count] == 0)
    return NO;

  [self setToolTipForToolbarItem:theItem];

  if (action == @selector(remove:)) {
    return [self shouldAllowRemoveAction] && [[self window] isKeyWindow];
  }
  else if (action == @selector(open:)) { 
    return [self shouldAllowOpenAction];
  }
  else if (action == @selector(reveal:)) {
    return [self fileExistsForSelectedItems];
  }
  else if (action == @selector(cancel:)) {
    return [self shouldAllowCancelAction] && [[self window] isKeyWindow];
  }
  else if (action == @selector(pause:) || action == @selector(resume:)) {
    return [self setPauseResumeToolbarItem:theItem];
  }
  else if (action == @selector(deleteDownloads:)) {
    return [self shouldAllowMoveToTrashAction] && [[self window] isKeyWindow];
  }

  return YES;
}

-(NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSString *)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag
{
  NSToolbarItem *theItem = [[[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier] autorelease];
  [theItem setTarget:self];
  [theItem setEnabled:NO];
  [self setToolTipForToolbarItem:theItem];

  if ([itemIdentifier isEqualToString:@"removebutton"]) {
    [theItem setLabel:NSLocalizedString(@"dlRemoveButtonLabel", nil)];
    [theItem setPaletteLabel:NSLocalizedString(@"dlRemoveButtonLabel", nil)];
    [theItem setAction:@selector(remove:)];
    [theItem setImage:[NSImage imageNamed:@"dl_remove.tif"]];
  }
  else if ([itemIdentifier isEqualToString:@"cancelbutton"]) {
    [theItem setLabel:NSLocalizedString(@"dlCancelButtonLabel", nil)];
    [theItem setPaletteLabel:NSLocalizedString(@"dlCancelButtonLabel", nil)];
    [theItem setAction:@selector(cancel:)];
    [theItem setImage:[NSImage imageNamed:@"dl_cancel.tif"]];
  }
  else if ([itemIdentifier isEqualToString:@"revealbutton"]) {
    [theItem setLabel:NSLocalizedString(@"dlRevealButtonLabel", nil)];
    [theItem setPaletteLabel:NSLocalizedString(@"dlRevealButtonLabel", nil)];
    [theItem setAction:@selector(reveal:)];
    [theItem setImage:[NSImage imageNamed:@"dl_reveal.tif"]];
  }
  else if ([itemIdentifier isEqualToString:@"openbutton"]) {
    [theItem setLabel:NSLocalizedString(@"dlOpenButtonLabel", nil)];
    [theItem setPaletteLabel:NSLocalizedString(@"dlOpenButtonLabel", nil)];
    [theItem setAction:@selector(open:)];
    [theItem setImage:[NSImage imageNamed:@"dl_open.tif"]];
  }
  else if ([itemIdentifier isEqualToString:@"cleanupbutton"]) {
    [theItem setLabel:NSLocalizedString(@"dlCleanUpButtonLabel", nil)];
    [theItem setPaletteLabel:NSLocalizedString(@"dlCleanUpButtonLabel", nil)];
    [theItem setAction:@selector(cleanUpDownloads:)];
    [theItem setImage:[NSImage imageNamed:@"dl_clearall.tif"]];
  }
  else if ([itemIdentifier isEqualToString:@"movetotrashbutton"]) {
    [theItem setLabel:NSLocalizedString(@"dlTrashButtonLabel", nil)];
    [theItem setPaletteLabel:NSLocalizedString(@"dlTrashButtonLabel", nil)];
    [theItem setAction:@selector(deleteDownloads:)];
    [theItem setImage:[NSImage imageNamed:@"dl_trash.tif"]];
  }
  else if ([itemIdentifier isEqualToString:@"pauseresumebutton"]) {
    [self setPauseResumeToolbarItem:theItem];
  }
  else {
    return nil;
  }
  return theItem;
}

-(NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar
{
  return [NSArray arrayWithObjects:@"cleanupbutton",
                                   @"removebutton",
                                   @"cancelbutton",
                                   @"pauseresumebutton",
                                   @"openbutton",
                                   @"revealbutton",
                                   @"movetotrashbutton",
                                   NSToolbarCustomizeToolbarItemIdentifier,
                                   NSToolbarFlexibleSpaceItemIdentifier,
                                   NSToolbarSpaceItemIdentifier,
                                   NSToolbarSeparatorItemIdentifier,
                                   nil];
}

-(NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar
{
  return [NSArray arrayWithObjects:@"cleanupbutton",
                                   @"removebutton",
                                   @"cancelbutton",
                                   @"pauseresumebutton",
                                   @"openbutton",
                                   NSToolbarFlexibleSpaceItemIdentifier,
                                   @"revealbutton",
                                   nil];
}

- (void)setToolTipForToolbarItem:(NSToolbarItem *)theItem
{
  NSString* toolTip = nil;
  NSString* itemIdentifier = [theItem itemIdentifier];
  BOOL plural = ([[self selectedProgressViewControllers] count] > 1);

  if ([itemIdentifier isEqualToString:@"cleanupbutton"])
    toolTip = NSLocalizedString(@"dlCleanUpButtonTooltip", nil);
  else if ([itemIdentifier isEqualToString:@"removebutton"])
    toolTip = NSLocalizedString((plural ? @"dlRemoveButtonTooltipPlural" : @"dlRemoveButtonTooltip"), nil);
  else if ([itemIdentifier isEqualToString:@"cancelbutton"])
    toolTip = NSLocalizedString((plural ? @"dlCancelButtonTooltipPlural" : @"dlCancelButtonTooltip"), nil);
  else if ([itemIdentifier isEqualToString:@"revealbutton"])
    toolTip = NSLocalizedString((plural ? @"dlRevealButtonTooltipPlural" : @"dlRevealButtonTooltip"), nil);
  else if ([itemIdentifier isEqualToString:@"openbutton"])
    toolTip = NSLocalizedString((plural ? @"dlOpenButtonTooltipPlural" : @"dlOpenButtonTooltip"), nil);
  else if ([itemIdentifier isEqualToString:@"movetotrashbutton"])
    toolTip = NSLocalizedString((plural ? @"dlTrashButtonTooltipPlural" : @"dlTrashButtonTooltip"), nil);
  else if ([itemIdentifier isEqualToString:@"pauseresumebutton"]) {
    if ([self shouldAllowResumeAction])
      toolTip = NSLocalizedString((plural ? @"dlResumeButtonTooltipPlural" : @"dlResumeButtonTooltip"), nil);
    else
      toolTip = NSLocalizedString((plural ? @"dlPauseButtonTooltipPlural" : @"dlPauseButtonTooltip"), nil);
  }

  if (toolTip)
    [theItem setToolTip:toolTip];
}

#pragma mark -

// zoom to fit contents, but don't go under minimum size
-(NSRect)windowWillUseStandardFrame:(NSWindow *)sender defaultFrame:(NSRect)defaultFrame
{
  NSRect windowFrame = [[self window] frame];
  NSSize curScrollFrameSize = [mScrollView frame].size;
  NSSize scrollFrameSize = [NSScrollView frameSizeForContentSize:[mStackView bounds].size
                                         hasHorizontalScroller:NO hasVerticalScroller:YES borderType:NSNoBorder];
  float frameDelta = (curScrollFrameSize.height - scrollFrameSize.height);
  
  // don't get vertically smaller than the default window size
  if ((windowFrame.size.height - frameDelta) < mDefaultWindowSize.height) {
    frameDelta = windowFrame.size.height - mDefaultWindowSize.height;
  }
  
  windowFrame.size.height -= frameDelta;
  windowFrame.origin.y    += frameDelta; // maintain top
  windowFrame.size.width  = mDefaultWindowSize.width;
  
  // cocoa will ensure that the window fits onscreen for us
  return windowFrame;
}

#pragma mark -

/*
 CHStackView datasource methods
 */

- (NSArray*)subviewsForStackView:(CHStackView *)stackView
{
  NSMutableArray* viewsArray = [NSMutableArray arrayWithCapacity:[mProgressViewControllers count]];
  
  unsigned int numViews = [mProgressViewControllers count];
  for (unsigned int i = 0; i < numViews; i ++)
    [viewsArray addObject:[((ProgressViewController*)[mProgressViewControllers objectAtIndex:i]) view]];
  
  return viewsArray;
}

#pragma mark -

/*
 Just create a progress view, but don't display it (otherwise the URL fields etc. 
                                                    are just blank)
 */
-(id <CHDownloadProgressDisplay>)createProgressDisplay
{
  ProgressViewController* newController = [[[ProgressViewController alloc] initWithWindowController:self] autorelease];
  [mProgressViewControllers addObject:newController];
  
  return newController;
}

@end
