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

#import "ProgressDlgController.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsIWebBrowserPersist.h"
#include "nsIInputStream.h"
#include "nsIURL.h"
#include "nsILocalFile.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIWebProgressListener.h"
#include "nsIDownload.h"
#include "nsIComponentManager.h"
#include "nsIPref.h"

static NSString *SaveFileToolbarIdentifier        = @"Save File Dialog Toolbar";
static NSString *CancelToolbarItemIdentifier      = @"Cancel Toolbar Item";
static NSString *ShowFileToolbarItemIdentifier    = @"Show File Toolbar Item";
static NSString *OpenFileToolbarItemIdentifier    = @"Open File Toolbar Item";
static NSString *LeaveOpenToolbarItemIdentifier   = @"Leave Open Toggle Toolbar Item";

static NSString *ProgressWindowFrameSaveName      = @"ProgressWindow";

@implementation ChimeraDownloadControllerFactory : DownloadControllerFactory

- (NSWindowController<CHDownloadProgressDisplay> *)createDownloadController
{
  NSWindowController* progressController = [[ProgressDlgController alloc] initWithWindowNibName: @"ProgressDialog"];
  NSAssert([progressController conformsToProtocol:@protocol(CHDownloadProgressDisplay)],
              @"progressController should conform to CHDownloadProgressDisplay protocol");
  return progressController;
}

@end

#pragma mark -


@interface ProgressDlgController(Private)
-(void)setupToolbar;
@end

@implementation ProgressDlgController

- (void)dealloc
{
  // if we get here because we're quitting, the listener will still be alive
  // yet we're going away. As a result, we need to tell the d/l listener to
  // forget it ever met us and necko will clean it up on its own.
  if ( mDownloader) 
    mDownloader->DetachDownloadDisplay();
  NS_IF_RELEASE(mDownloader);
  [super dealloc];
}

- (void)windowDidLoad
{
  [super windowDidLoad];
  
  mDownloadIsComplete = NO;

  if (!mIsFileSave) {
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
    PRBool save = PR_FALSE;
    prefs->GetBoolPref("browser.download.progressDnldDialog.keepAlive", &save);
    mSaveFileDialogShouldStayOpen = save;
  }
  
  [self setupToolbar];
  [mProgressBar setUsesThreadedAnimation:YES];      
  [mProgressBar startAnimation:self];   // move to onStateChange
}

- (void)setupToolbar
{
    NSToolbar *toolbar = [[[NSToolbar alloc] initWithIdentifier:SaveFileToolbarIdentifier] autorelease];

    [toolbar setDisplayMode:NSToolbarDisplayModeDefault];
    [toolbar setAllowsUserCustomization:YES];
    [toolbar setAutosavesConfiguration:YES];
    [toolbar setDelegate:self];
    [[self window] setToolbar:toolbar];
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects: CancelToolbarItemIdentifier,
        ShowFileToolbarItemIdentifier,
        OpenFileToolbarItemIdentifier,
        LeaveOpenToolbarItemIdentifier,
        NSToolbarCustomizeToolbarItemIdentifier,
        NSToolbarFlexibleSpaceItemIdentifier,
        NSToolbarSpaceItemIdentifier,
        NSToolbarSeparatorItemIdentifier,
        nil];
}

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects: CancelToolbarItemIdentifier,
        NSToolbarFlexibleSpaceItemIdentifier,
        LeaveOpenToolbarItemIdentifier,
        NSToolbarFlexibleSpaceItemIdentifier,
        ShowFileToolbarItemIdentifier,
        OpenFileToolbarItemIdentifier,
        nil];
}

- (BOOL)validateToolbarItem:(NSToolbarItem *)toolbarItem
{
  if ([toolbarItem action] == @selector(cancel))  // cancel button
    return (!mDownloadIsComplete);
  if ([toolbarItem action] == @selector(pauseAndResumeDownload))  // pause/resume button
    return (NO);  // Hey - it hasn't been hooked up yet. !mDownloadIsComplete when it is.
  if ([toolbarItem action] == @selector(showFile))  // show file
    return (mDownloadIsComplete);
  if ([toolbarItem action] == @selector(openFile))  // open file
    return (mDownloadIsComplete);
  return YES;           // turn it on otherwise.
}

-(void)autosaveWindowFrame
{
  [[self window] saveFrameUsingName: ProgressWindowFrameSaveName];
}

- (NSToolbarItem *) toolbar:(NSToolbar *)toolbar
      itemForItemIdentifier:(NSString *)itemIdent
  willBeInsertedIntoToolbar:(BOOL)willBeInserted
{
    NSToolbarItem *toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier:itemIdent] autorelease];

    if ( [itemIdent isEqual:CancelToolbarItemIdentifier] ) {
        [toolbarItem setLabel:NSLocalizedString(@"Cancel",@"Cancel")];
        [toolbarItem setPaletteLabel:NSLocalizedString(@"CancelPaletteLabel",@"Cancel Download")];
        [toolbarItem setToolTip:NSLocalizedString(@"CancelToolTip",@"Cancel this file download")];
        [toolbarItem setImage:[NSImage imageNamed:@"saveCancel"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(cancel)];
    } else if ( [itemIdent isEqual:ShowFileToolbarItemIdentifier] ) {
        [toolbarItem setLabel:NSLocalizedString(@"Show File",@"Show File")];
        [toolbarItem setPaletteLabel:NSLocalizedString(@"Show File",@"Show File")];
        [toolbarItem setToolTip:NSLocalizedString(@"ShowToolTip",@"Show the saved file in the Finder")];
        [toolbarItem setImage:[NSImage imageNamed:@"saveShowFile"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(showFile)];
    } else if ( [itemIdent isEqual:OpenFileToolbarItemIdentifier] ) {
        [toolbarItem setLabel:NSLocalizedString(@"Open File",@"Open File")];
        [toolbarItem setPaletteLabel:NSLocalizedString(@"Open File",@"Open File")];
        [toolbarItem setToolTip:NSLocalizedString(@"OpenToolTip",@"Open the saved file in its default application.")];
        [toolbarItem setImage:[NSImage imageNamed:@"saveOpenFile"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(openFile)];
    } else if ( [itemIdent isEqual:LeaveOpenToolbarItemIdentifier] ) {
        if ( !mIsFileSave ) {
            if ( mSaveFileDialogShouldStayOpen ) {
                [toolbarItem setLabel:NSLocalizedString(@"Leave Open",@"Leave Open")];
                [toolbarItem setPaletteLabel:NSLocalizedString(@"Toggle Close Behavior",@"Toggle Close Behavior")];
                [toolbarItem setToolTip:NSLocalizedString(@"LeaveOpenToolTip",@"Window will stay open when download finishes.")];
                [toolbarItem setImage:[NSImage imageNamed:@"saveLeaveOpenYES"]];
                [toolbarItem setTarget:self];
                [toolbarItem setAction:@selector(toggleLeaveOpen)];
            } else {
                [toolbarItem setLabel:NSLocalizedString(@"Close When Done",@"Close When Done")];
                [toolbarItem setPaletteLabel:NSLocalizedString(@"Toggle Close Behavior",@"Toggle Close Behavior")];
                [toolbarItem setToolTip:NSLocalizedString(@"CloseWhenDoneToolTip",@"Window will close automatically when download finishes.")];
                [toolbarItem setImage:[NSImage imageNamed:@"saveLeaveOpenNO"]];
                [toolbarItem setTarget:self];
                [toolbarItem setAction:@selector(toggleLeaveOpen)];
            }
            if ( willBeInserted ) {
                leaveOpenToggleToolbarItem = toolbarItem; //establish reference
         }
        }
    } else {
        toolbarItem = nil;
    }

    return toolbarItem;
}

-(void)cancel
{
  if (mDownloader)    // we should always have one
    mDownloader->CancelDownload();
  
  // clean up downloaded file. - do it here on in CancelDownload?
  NSFileManager *fileManager = [NSFileManager defaultManager];
  NSString *thePath = [[mToField stringValue] stringByExpandingTildeInPath];
  if ([fileManager isDeletableFileAtPath:thePath])
    // if we delete it, fantastic.  if not, oh well.  better to move to trash instead?
    [fileManager removeFileAtPath:thePath handler:nil];
  
  // we can _not_ set the |mIsDownloadComplete| flag here because the download really
  // isn't done yet. We'll probably continue to process more PLEvents that are already
  // in the queue until we get a STATE_STOP state change. As a result, we just keep
  // going until that comes in (and it will, because we called CancelDownload() above).
  // Ensure that the window goes away when we get there by flipping the 'stay alive'
  // flag. (bug 154913)
  mSaveFileDialogShouldStayOpen = NO;
}

-(void)showFile
{
  NSString *theFile = [[mToField stringValue] stringByExpandingTildeInPath];
  if ([[NSWorkspace sharedWorkspace] selectFile:theFile
                       inFileViewerRootedAtPath:[theFile stringByDeletingLastPathComponent]])
    return;
  // hmmm.  it didn't work.  that's odd. need localized error messages. for now, just beep.
  NSBeep();
}

-(void)openFile
{
  NSString *theFile = [[mToField stringValue] stringByExpandingTildeInPath];
  if ([[NSWorkspace sharedWorkspace] openFile:theFile])
    return;
  // hmmm.  it didn't work.  that's odd.  need localized error message. for now, just beep.
  NSBeep();
    
}

-(void)toggleLeaveOpen
{
    if ( ! mSaveFileDialogShouldStayOpen ) {
        mSaveFileDialogShouldStayOpen = YES;
        [leaveOpenToggleToolbarItem setLabel:NSLocalizedString(@"Leave Open",@"Leave Open")];
        [leaveOpenToggleToolbarItem setPaletteLabel:NSLocalizedString(@"Toggle Close Behavior",@"Toggle Close Behavior")];
        [leaveOpenToggleToolbarItem setToolTip:NSLocalizedString(@"LeaveOpenToolTip",@"Window will stay open when download finishes.")];
        [leaveOpenToggleToolbarItem setImage:[NSImage imageNamed:@"saveLeaveOpenYES"]];
    } else {
        mSaveFileDialogShouldStayOpen = NO;
        [leaveOpenToggleToolbarItem setLabel:NSLocalizedString(@"Close When Done",@"Close When Done")];
        [leaveOpenToggleToolbarItem setPaletteLabel:NSLocalizedString(@"Toggle Close Behavior",@"Toggle Close Behavior")];
        [leaveOpenToggleToolbarItem setToolTip:NSLocalizedString(@"CloseWhenDoneToolTip",@"Window will close automatically when download finishes.")];
        [leaveOpenToggleToolbarItem setImage:[NSImage imageNamed:@"saveLeaveOpenNO"]];
    }
    
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
    prefs->SetBoolPref("browser.download.progressDnldDialog.keepAlive", mSaveFileDialogShouldStayOpen);
}

- (void)windowWillClose:(NSNotification *)notification
{
  [self autosaveWindowFrame];
  [self autorelease];
}

- (BOOL)windowShouldClose:(NSNotification *)notification
{
  [self killDownloadTimer];
  if (!mDownloadIsComplete) { //whoops.  hard cancel.
    [self cancel];
    return NO;  // let setDownloadProgress handle the close.
  }
  return YES; 
}

- (void)killDownloadTimer
{
  if (mDownloadTimer) {
    [mDownloadTimer invalidate];
    [mDownloadTimer release];
    mDownloadTimer = nil;
  }    
}
- (void)setupDownloadTimer
{
  [self killDownloadTimer];
  mDownloadTimer = [[NSTimer scheduledTimerWithTimeInterval:1.0
                                                     target:self
                                                   selector:@selector(setDownloadProgress:)
                                                   userInfo:nil
                                                    repeats:YES] retain];
}

-(NSString *)formatTime:(int)seconds
{
  NSMutableString *theTime =[[[NSMutableString alloc] initWithCapacity:8] autorelease];
  [theTime setString:@""];
  NSString *padZero = [NSString stringWithString:@"0"];
  //write out new elapsed time
  if (seconds >= 3600){
    [theTime appendFormat:@"%d:",(seconds / 3600)];
    seconds = seconds % 3600;
  }
  NSString *elapsedMin = [NSString stringWithFormat:@"%d:",(seconds / 60)];
  if ([elapsedMin length] == 2)
    [theTime appendString:[padZero stringByAppendingString:elapsedMin]];
  else
    [theTime appendString:elapsedMin];
  seconds = seconds % 60;
  NSString *elapsedSec = [NSString stringWithFormat:@"%d",seconds];
  if ([elapsedSec length] == 2)
    [theTime appendString:elapsedSec];
  else
    [theTime appendString:[padZero stringByAppendingString:elapsedSec]];
  return theTime;
}
// fuzzy time gives back strings like "about 5 seconds"
-(NSString *)formatFuzzyTime:(int)seconds
{
  // check for seconds first
  if (seconds < 60) {
    if (seconds < 7)
      return [[[NSString alloc] initWithFormat:NSLocalizedString(@"UnderSec",@"Under %d seconds"),5] autorelease];
    if (seconds < 13)
      return [[[NSString alloc] initWithFormat:NSLocalizedString(@"UnderSec",@"Under %d seconds"),10] autorelease];
    return [[[NSString alloc] initWithString:NSLocalizedString(@"UnderMin",@"Under a minute")] autorelease];
  }    
  // seconds becomes minutes and we keep checking.  
  seconds = seconds/60;
  if (seconds < 60) {
    if (seconds < 2)
       return [[[NSString alloc] initWithString:NSLocalizedString(@"AboutMin",@"About a minute")] autorelease];
    // OK, tell the good people how much time we have left.
    return [[[NSString alloc] initWithFormat:NSLocalizedString(@"AboutMins",@"About %d minutes"),seconds] autorelease];
  }
  //this download will never seemingly never end. now seconds become hours.
  seconds = seconds/60;
  if (seconds < 2)
    return [[[NSString alloc] initWithString:NSLocalizedString(@"AboutHour",@"Over an hour")] autorelease];
  return [[[NSString alloc] initWithFormat:NSLocalizedString(@"AboutHours",@"Over %d hours"),seconds] autorelease];
}


-(NSString *)formatBytes:(float)bytes
{   // this is simpler than my first try.  I peaked at Omnigroup byte formatting code.
    // if bytes are negative, we return question marks.
  if (bytes < 0)
    return [[[NSString alloc] initWithString:@"???"] autorelease];
  // bytes first.
  if (bytes < 1024)
    return [[[NSString alloc] initWithFormat:@"%.1f bytes",bytes] autorelease];
  // kb
  bytes = bytes/1024;
  if (bytes < 1024)
    return [[[NSString alloc] initWithFormat:@"%.1f KB",bytes] autorelease];
  // mb
  bytes = bytes/1024;
  if (bytes < 1024)
    return [[[NSString alloc] initWithFormat:@"%.1f MB",bytes] autorelease];
  // gb
  bytes = bytes/1024;
  return [[[NSString alloc] initWithFormat:@"%.1f GB",bytes] autorelease];
}

// this handles lots of things.
- (void)setDownloadProgress:(NSTimer *)downloadTimer;
{
  // XXX this logic needs cleaning up.
  
  // Ack! we're closing the window with the download still running!
  if (mDownloadIsComplete)
  {
    [[self window] performClose:self];  
    return;
  }
  // get the elapsed time
  NSArray *elapsedTimeArray = [[mElapsedTimeLabel stringValue] componentsSeparatedByString:@":"];
  int j = [elapsedTimeArray count];
  int elapsedSec = [[elapsedTimeArray objectAtIndex:(j-1)] intValue] + [[elapsedTimeArray objectAtIndex:(j-2)] intValue]*60;
  if (j==3)  // this download is taking forever.
    elapsedSec += [[elapsedTimeArray objectAtIndex:0] intValue]*3600;
  // update elapsed time
  [mElapsedTimeLabel setStringValue:[self formatTime:(++elapsedSec)]];
  // for status field & time left
  float maxBytes = ([mProgressBar maxValue]);
  float byteSec = mCurrentProgress/elapsedSec;
  // OK - if downloadTimer is nil, we're done - fix maxBytes value for status report.
  if (!downloadTimer)
    maxBytes = mCurrentProgress;
  // update status field
  NSString *labelString = NSLocalizedString(@"LabelString",@"%@ of %@ total (at %@/sec)");
  [mStatusLabel setStringValue: [NSString stringWithFormat:labelString, [self formatBytes:mCurrentProgress], [self formatBytes:maxBytes], [self formatBytes:byteSec]]];
  // updating estimated time left field
  // if maxBytes < 0, can't calc time left.
  // if !downloadTimer, download is finished.  either way, make sure time left is 0.
  if ((maxBytes > 0) && (downloadTimer))
  {
    int secToGo = (int)ceil((elapsedSec*maxBytes/mCurrentProgress) - elapsedSec);
    [mTimeLeftLabel setStringValue:[self formatFuzzyTime:secToGo]];
  }
  else if (!downloadTimer)
  {                            // download done.  Set remaining time to 0, fix progress bar & cancel button
    mDownloadIsComplete = YES;            // all done. we got a STATE_STOP
    [mTimeLeftLabel setStringValue:@""];
    [self setProgressTo:mCurrentProgress ofMax:mCurrentProgress];
    if (!mSaveFileDialogShouldStayOpen)
      [[self window] performClose:self];  // close window
    else
      [[self window] update];             // redraw window
  }
  else //maxBytes is undetermined.  Set remaining time to question marks.
      [mTimeLeftLabel setStringValue:@"???"];
}

#pragma mark -

// CHDownloadProgressDisplay protocol methods

- (void)onStartDownload:(BOOL)isFileSave;
{
  mIsFileSave = isFileSave;
  
  [self window];		// make the window
  [[self window] setFrameUsingName: ProgressWindowFrameSaveName];

  [self showWindow: self];
  [self setupDownloadTimer];
}

- (void)onEndDownload
{
  // if we're quitting, our progress window is already gone and we're in the
  // process of shutting down gecko and all the d/l listeners. The timer, at 
  // that point, is the only thing keeping us alive. Killing it will cause
  // us to go away immediately, so kung-fu deathgrip it until we're done twiddling
  // bits on ourself.
  [self retain];                           // Enter The Dragon!
    [self killDownloadTimer];
    [self setDownloadProgress:nil];
  [self release];
}

- (void)setProgressTo:(long)aCurProgress ofMax:(long)aMaxProgress
{
  mCurrentProgress = aCurProgress;         // fall back for stat calcs

  if (![mProgressBar isIndeterminate]) 	  //most likely - just update value
  {
    if (aCurProgress == aMaxProgress)      //handles little bug in FTP download size
      [mProgressBar setMaxValue:aMaxProgress];

    [mProgressBar setDoubleValue:aCurProgress];
  }
  else if (aMaxProgress > 0)	             // ok, we're starting up with good max & cur values
  {
    [mProgressBar setIndeterminate:NO];
    [mProgressBar setMaxValue:aMaxProgress];
    [mProgressBar setDoubleValue:aCurProgress];
  } // if neither case was true, it's barber pole city.
}

-(void) setDownloadListener: (CHDownloader*)aDownloader
{
  if (mDownloader != aDownloader)
    NS_IF_RELEASE(mDownloader);

  NS_IF_ADDREF(mDownloader = aDownloader);
}

- (void)setSourceURL:(NSString*)aSourceURL
{
  [mFromField setStringValue: aSourceURL];  
  [mFromField display];  // force an immmeditate update
}

- (void)setDestinationPath:(NSString*)aDestPath
{
  [mToField setStringValue: [aDestPath stringByAbbreviatingWithTildeInPath]];
  [mToField display];   // force an immmeditate update
  
  // also set the window title
  NSString* downloadFileName = [aDestPath lastPathComponent];
  if ([downloadFileName length] == 0)
    downloadFileName = aDestPath;
  
  [[self window] setTitle:[NSString stringWithFormat:NSLocalizedString(@"DownloadingTitle", @""), downloadFileName]];
}

@end
