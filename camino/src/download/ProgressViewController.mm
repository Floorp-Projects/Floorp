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
 *    Calum Robinson <calumr@mac.com>
 *    Simon Fraser <sfraser@netscape.com>
 *    Josh Aas <josha@mac.com>
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


#import "NSView+Utils.h"

#import "ProgressViewController.h"
#import "ProgressDlgController.h"
#import "PreferenceManager.h"
#import "ProgressView.h"

enum {
  kLabelTagFilename = 1000,
  kLabelTagStatus,
  kLabelTagTimeRemaining,
  kLabelTagIcon
};

@interface ProgressViewController(ProgressViewControllerPrivate)

-(void)viewDidLoad;
-(void)refreshDownloadInfo;
-(void)launchFileIfAppropriate;

@end

@implementation ProgressViewController

+(NSString *)formatTime:(int)seconds
{
  NSMutableString *theTime = [NSMutableString stringWithCapacity:8];
  
  NSString *padZero = [NSString stringWithString:@"0"];
  //write out new elapsed time
  if (seconds >= 3600) {
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
+ (NSString *)formatFuzzyTime:(int)seconds
{
  // check for seconds first
  if (seconds < 60) {
    if (seconds < 7)
      return [NSString stringWithFormat:NSLocalizedString(@"UnderSec", @"Under %d seconds"), 5];
    if (seconds < 13)
      return [NSString stringWithFormat:NSLocalizedString(@"UnderSec", @"Under %d seconds"), 10];
    return [NSString stringWithFormat:NSLocalizedString(@"UnderMin", @"Under a minute")];
  }    
  // seconds becomes minutes and we keep checking.  
  seconds = seconds/60;
  if (seconds < 60) {
    if (seconds < 2)
      return [NSString stringWithFormat:NSLocalizedString(@"AboutMin",@"About a minute")];
    // OK, tell the good people how much time we have left.
    return [NSString stringWithFormat:NSLocalizedString(@"AboutMins",@"About %d minutes"), seconds];
  }
  //this download will never seemingly never end. now seconds become hours.
  seconds = seconds/60;
  if (seconds < 2)
    return [NSString stringWithFormat:NSLocalizedString(@"AboutHour", @"Over an hour")];
  return [NSString stringWithFormat:NSLocalizedString(@"AboutHours", @"Over %d hours"), seconds];
}

+(NSString*)formatBytes:(float)bytes
{
  // if bytes are negative, we return question marks.
  if (bytes < 0)
    return [NSString stringWithString:@"?"];
  // bytes first.
  if (bytes < 1024)
    return [NSString stringWithFormat:@"%.1f bytes",bytes];
  // kb
  bytes = bytes/1024;
  if (bytes < 1024)
    return [NSString stringWithFormat:@"%.1f KB",bytes];
  // mb
  bytes = bytes/1024;
  if (bytes < 1024)
    return [NSString stringWithFormat:@"%.1f MB",bytes];
  // gb
  bytes = bytes/1024;
  return [NSString stringWithFormat:@"%.1f GB",bytes];
}

#pragma mark -

-(id)init
{
  if ((self = [super init])) {
    [NSBundle loadNibNamed:@"ProgressView" owner:self];
    [self viewDidLoad];
  }
  return self;
}

-(void)dealloc
{
  // if we get here because we're quitting, the listener will still be alive
  // yet we're going away. As a result, we need to tell the d/l listener to
  // forget it ever met us and necko will clean it up on its own.
  if (mDownloader) {
    mDownloader->DetachDownloadDisplay();
  }
  NS_IF_RELEASE(mDownloader);
  
  [mStartTime release];
  [mSourceURL release];
  [mDestPath release];
  [mProgressBar release];

  // loading the nib has retained these, we have to release them.
  [mCompletedView release];
  [mProgressView release];

  [super dealloc];
}

-(void)viewDidLoad
{
  [mProgressBar retain]; // make sure it survives being moved between views
                         // this isn't necessarily better. Need to profile.
  [mProgressBar setUsesThreadedAnimation:YES];
  // give the views this controller as their controller
  [mCompletedView setController:self];
  [mProgressView setController:self];
}

-(ProgressView*)view
{
  return (mDownloadDone ? mCompletedView : mProgressView);
}

-(IBAction)copySourceURL:(id)sender
{
  NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
  [pasteboard declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
  [pasteboard setString:mSourceURL forType:NSStringPboardType];
}

-(IBAction)cancel:(id)sender
{
  // don't allow download to be cancelled twice or we get a nasty crash
  if (!mUserCancelled) {
    mUserCancelled = YES;
    
    if (mDownloader) { // we should always have one
      mDownloader->CancelDownload();
    }
    
    // note that we never want to delete downloaded files here,
    // because the file does not have its final path yet.
    // Necko will delete the evil temp file.
  }
}

-(IBAction)reveal:(id)sender
{
  if (![[NSWorkspace sharedWorkspace] selectFile:mDestPath
      inFileViewerRootedAtPath:[mDestPath stringByDeletingLastPathComponent]]) {
    NSBeep();
  }
}

-(IBAction)open:(id)sender
{
  if (![[NSWorkspace sharedWorkspace] openFile:mDestPath]) {
    NSBeep();
  }
  return;
}

-(IBAction)remove:(id)sender
{
  [mProgressWindowController removeDownload:self];
}

// Called just before the view will be shown to the user
-(void)downloadDidStart
{
  mStartTime = [[NSDate alloc] init];
  [self refreshDownloadInfo];
}

-(void)downloadDidEnd
{
  if (!mDownloadDone) { // some error conditions can cause this to get called twice
    // retain the selection when the view switches - do this before mDownloadDone changes
    [mCompletedView setSelected:[self isSelected]];
    
    mDownloadDone = YES;
    mDownloadTime = -[mStartTime timeIntervalSinceNow];
    [mProgressBar stopAnimation:self];
    
    // get the Finder to update - doesn't work on some earlier version of Mac OS X
    // (I think it was fixed by 10.2.2 or so)
    [[NSWorkspace sharedWorkspace] noteFileSystemChanged:mDestPath];
    
    [self refreshDownloadInfo];
    [self launchFileIfAppropriate];
  }
}

-(void)launchFileIfAppropriate
{
  if (!mIsFileSave && !mUserCancelled && !mDownloadingError) {
    if ([[PreferenceManager sharedInstance] getBooleanPref:"browser.download.autoDispatch" withSuccess:NULL]) {
      [[NSWorkspace sharedWorkspace] openFile:mDestPath withApplication:nil andDeactivate:NO];
    }
  }
}

// this handles lots of things - all of the status updates
-(void)refreshDownloadInfo
{
  NSView* curView = [self view];
  NSString* filename = [mDestPath lastPathComponent];
  id iconLabel = [curView viewWithTag:kLabelTagIcon];
  
  id filenameLabel = [curView viewWithTag:kLabelTagFilename];
  [filenameLabel setStringValue:filename];
  
  if (iconLabel) { // update the icon image
    NSImage *iconImage = [[NSWorkspace sharedWorkspace] iconForFile:mDestPath];
    // sometimes the finder doesn't have an icon for us (rarely)
    // when that happens just leave it at what it was before
    if (iconImage != nil) {
      [iconLabel setImage:iconImage];
    }
  }
  
  if (mDownloadDone) { // just update the status field
    id statusLabel = [curView viewWithTag:kLabelTagStatus];
    if (statusLabel) {
      NSString* statusString;
      if (mUserCancelled) {
        statusString = NSLocalizedString(@"DownloadCancelled", @"Cancelled");
      }
      else if (mDownloadingError) {
        statusString = NSLocalizedString(@"DownloadInterrupted", @"Interrupted");
      }
      else {
        statusString = [NSString stringWithFormat:NSLocalizedString(@"DownloadCompleted", @"Completed in %@ (%@)"),
          [[self class] formatTime:(int)mDownloadTime], [[self class] formatBytes:mDownloadSize]];
      }
      
      [statusLabel setStringValue:statusString];
    }
  }
  else {
    NSTimeInterval elapsedTime = -[mStartTime timeIntervalSinceNow];
    
    // update status field
    id statusLabel = [curView viewWithTag:kLabelTagStatus];
    if (statusLabel) {
      NSString *statusLabelString = NSLocalizedString(@"DownloadStatusString", @"%@ of %@ (at %@/sec)");
      float byteSec = mCurrentProgress / elapsedTime;
      [statusLabel setStringValue:[NSString stringWithFormat:statusLabelString, 
                   [[self class] formatBytes:mCurrentProgress],
                   (mDownloadSize > 0 ? [[self class] formatBytes:mDownloadSize] : @"?"),
                   [[self class] formatBytes:byteSec]]];
    }
    
    id timeLabel = [curView viewWithTag:kLabelTagTimeRemaining];
    if (timeLabel) {
      if (mDownloadSize > 0) {
        int secToGo = (int)ceil((elapsedTime * mDownloadSize / mCurrentProgress) - elapsedTime);
        [timeLabel setStringValue:[[self class] formatFuzzyTime:secToGo]];
      }
      else { // mDownloadSize is undetermined.  Set remaining time to question marks.
        [timeLabel setStringValue:NSLocalizedString(@"DownloadCalculatingString", @"Unknown")];
      }
    }
  }
}

-(void)setProgressWindowController:(ProgressDlgController*)progressWindowController
{
  mProgressWindowController = progressWindowController;
}

-(BOOL)isActive
{
  return !mDownloadDone;
}

-(BOOL)isCanceled
{
  return mUserCancelled;
}

-(BOOL)isSelected
{
  return [[self view] isSelected];
}

-(NSMenu*)contextualMenu
{
  NSMenu *menu = [[NSMenu alloc] init];
  NSMenuItem *revealItem;
  NSMenuItem *cancelItem;
  NSMenuItem *removeItem;
  NSMenuItem *openItem;
  NSMenuItem *copySourceURLItem;
  
  revealItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"dlRevealCMLabel", @"Show in Finder")
                                   action:@selector(reveal:) keyEquivalent:@""];
  cancelItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"dlCancelCMLabel", @"Cancel")
                                   action:@selector(cancel:) keyEquivalent:@""];
  removeItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"dlRemoveCMLabel", @"Remove")
                                   action:@selector(remove:) keyEquivalent:@""];
  openItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"dlOpenCMLabel", @"Open")
                                   action:@selector(open:) keyEquivalent:@""];
  copySourceURLItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"dlCopySourceURLCMLabel", @"Copy Source URL")
                                   action:@selector(copySourceURL:) keyEquivalent:@""];
  
  [revealItem setTarget:mProgressWindowController];
  [cancelItem setTarget:mProgressWindowController];
  [removeItem setTarget:mProgressWindowController];
  [openItem setTarget:mProgressWindowController];
  [copySourceURLItem setTarget:self];
  
  [menu addItem:revealItem];
  [menu addItem:cancelItem];
  [menu addItem:removeItem];
  [menu addItem:openItem];
  [menu addItem:[NSMenuItem separatorItem]];
  [menu addItem:copySourceURLItem];
  
  [revealItem release];
  [cancelItem release];
  [removeItem release];
  [openItem release];
  [copySourceURLItem release];
  
  return [menu autorelease];    
}

- (BOOL)validateMenuItem:(id <NSMenuItem>)menuItem
{
  SEL action = [menuItem action];
  if (action == @selector(cancel:)) {
    return ((!mUserCancelled) && (!mDownloadDone));
  }
  if (action == @selector(remove:)) {
    return (mUserCancelled || mDownloadDone);
  }
  return YES;
}

#pragma mark -

-(void)onStartDownload:(BOOL)isFileSave
{
  mIsFileSave = isFileSave;
  [self downloadDidStart];
  [mProgressWindowController didStartDownload:self];
  
  // need to do this after the view as been put in the window, otherwise it doesn't work
  [mProgressBar startAnimation:self];
}

-(void)onEndDownload:(BOOL)completedOK
{
  mDownloadingError = !completedOK;
  
  [self downloadDidEnd];
  [mProgressWindowController didEndDownload:self];
}

-(void)setProgressTo:(long)aCurProgress ofMax:(long)aMaxProgress
{
  mCurrentProgress = aCurProgress;         // fall back for stat calcs
  mDownloadSize = aMaxProgress;
  
  if (![mProgressBar isIndeterminate]) {   //most likely - just update value
    if (aCurProgress == aMaxProgress) {    //handles little bug in FTP download size
      [mProgressBar setMaxValue:aMaxProgress];
    }
    [mProgressBar setDoubleValue:aCurProgress];
  }
  else if (aMaxProgress > 0) {            // ok, we're starting up with good max & cur values
    [mProgressBar setIndeterminate:NO];
    [mProgressBar setMaxValue:aMaxProgress];
    [mProgressBar setDoubleValue:aCurProgress];
  } // if neither case was true, it's barber pole city
}

-(void)setDownloadListener:(CHDownloader*)aDownloader
{
  if (mDownloader != aDownloader) {
    NS_IF_RELEASE(mDownloader);
  }
  
  NS_IF_ADDREF(mDownloader = aDownloader);
}

#if 0
/*
 This is kind of a hack. It should probably be done somewhere else so Mozilla can have
 it too, but until Apple fixes the problems with the setting of comments without 
 reverting to Applescript, I have left it in. 
 
 Turned off for now, until we find a better way to do this. Won't Carbon APIs work?
 */
- (void)tryToSetFinderComments
{
  if (mDestPath && mSourceURL) {
    CFURLRef fileURL = CFURLCreateWithFileSystemPath(   NULL,
                                                        (CFStringRef)mDestPath,
                                                        kCFURLPOSIXPathStyle,
                                                        NO);
    
    NSString *hfsPath = (NSString *)CFURLCopyFileSystemPath(fileURL,
                                                            kCFURLHFSPathStyle);
    
    CFRelease(fileURL);
    
    NSAppleScript *setCommentScript = [[NSAppleScript alloc] initWithSource:
      [NSString stringWithFormat:@"tell application \"Finder\" to set comment of file \"%@\" to \"%@\"", hfsPath, mSourceURL]];
    NSDictionary *errorInfo = NULL;
    
    [setCommentScript executeAndReturnError:&errorInfo];
    
    if (errorInfo) {
      NSLog(@"Get error when running AppleScript to set comments for '%@':\n %@", 
            mDestPath,
            [errorInfo objectForKey:NSAppleScriptErrorMessage]);
    }
  }
}

#endif

-(void)setSourceURL:(NSString*)aSourceURL
{
  [mSourceURL autorelease];
  mSourceURL = [aSourceURL copy];
  [mProgressView setToolTip:mSourceURL];
  [mCompletedView setToolTip:mSourceURL];
  //[self tryToSetFinderComments];
}

-(void)setDestinationPath:(NSString*)aDestPath
{
  [mDestPath autorelease];
  mDestPath = [aDestPath copy];
  //[self tryToSetFinderComments];
}

@end
