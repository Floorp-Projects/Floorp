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

enum
{
  kLabelTagFilename    = 1000,
  kLabelTagProgress,
  kLabelTagSource,
  kLabelTagDestination,
  kLabelTagTimeRemaining,
  kLabelTagStatus,		// 1005
  kLabelTagTimeRemainingLabel
};


// Notification sent when user holds option key and expands/contracts a progress view
static NSString *ProgressViewsShouldResize = @"ProgressViewsShouldResize";

@interface ProgressViewController(ProgressViewControllerPrivate)

- (void)viewDidLoad;
- (void)refreshDownloadInfo;
- (void)moveProgressBarToCurrentView;
- (void)updateButtons;

@end

@implementation ProgressViewController

+ (NSString *)formatTime:(int)seconds
{
  NSMutableString *theTime = [NSMutableString stringWithCapacity:8];

  NSString *padZero = [NSString stringWithString:@"0"];
  //write out new elapsed time
  if (seconds >= 3600)
  {
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

+ (NSString *)formatBytes:(float)bytes
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

- (id)init
{
  if ((self = [super init]))
  {
    [NSBundle loadNibNamed:@"ProgressView" owner:self];
    
    [self viewDidLoad];
    
    // Register for notifications when one of the progress views is expanded/contracted
    // whilst holding the option button
    [[NSNotificationCenter defaultCenter] addObserver:self
                                            selector:@selector(changeCollapsedStateNotification:)
                                            name:ProgressViewsShouldResize
                                            object:nil];
    
    [mExpandedRevealButton setEnabled:NO];
    [mExpandedOpenButton   setEnabled:NO];
  }
  
  return self;
}

- (void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self
                                          name:ProgressViewsShouldResize
                                          object:nil];
  
  // if we get here because we're quitting, the listener will still be alive
  // yet we're going away. As a result, we need to tell the d/l listener to
  // forget it ever met us and necko will clean it up on its own.
  if (mDownloader) 
    mDownloader->DetachDownloadDisplay();
  NS_IF_RELEASE(mDownloader);

  [mStartTime release];
  [mSourceURL release];
  [mDestPath release];
  [mProgressBar release];
  
  [super dealloc];
}

// Save the expand/contract view pref (called when the user clicks the dislosure triangle)
- (void)setCompactViewPref
{
  [[PreferenceManager sharedInstance] setPref:"browser.download.compactView" toBoolean:mViewIsCompact];
}

- (void)viewDidLoad
{
  mViewIsCompact = [[PreferenceManager sharedInstance] getBooleanPref:"browser.download.compactView" withSuccess:NULL];
  [mProgressBar retain];		// make sure it survives being moved between views
  
  if (mViewIsCompact)
    [self moveProgressBarToCurrentView];
  
  // this isn't necessarily better. Need to profile.
  [mProgressBar setUsesThreadedAnimation:YES];      
}

- (NSView *)view
{
  if (mViewIsCompact)
    return (mDownloadDone ? mCompletedViewCompact : mProgressViewCompact);
  else
    return (mDownloadDone ? mCompletedView : mProgressView);
}

- (IBAction)toggleDisclosure:(id)sender
{
  mViewIsCompact = !mViewIsCompact;
  
  [self moveProgressBarToCurrentView];

  // Is option/alt held down?
  if ([[[sender window] currentEvent] modifierFlags] & NSAlternateKeyMask)
  {
    // Get all progress views to look the same as self
    [[NSNotificationCenter defaultCenter] postNotificationName:ProgressViewsShouldResize
                                            object:[NSNumber numberWithBool:mViewIsCompact]];
  }
  
  // Set the pref only when the user clicks the disclosure triangle
  [self setCompactViewPref];
  
  // Re-calculate the new view & window sizes
  [[NSNotificationCenter defaultCenter] postNotificationName:StackViewReloadNotificationName
                                          object:self];
  [self refreshDownloadInfo];
}

- (void)changeCollapsedStateNotification:(NSNotification *)notification
{
  // note that this will get called on the view that is being clicked, as well
  // as the other views. Don't do redundant work here, like redrawing.
  mViewIsCompact = [[notification object] boolValue];
  // Don't call [enclosingStackView reloadSubviews]; here, because it will be done
  // by the original view that was option-clicked, not us
}

-(void)cancel
{
  mUserCancelled = YES;

  if (mDownloader)    // we should always have one
    mDownloader->CancelDownload();

  // clean up downloaded file. - do it here or in CancelDownload?
  NSFileManager *fileManager = [NSFileManager defaultManager];
  if ([fileManager isDeletableFileAtPath:mDestPath])
  {
    // if we delete it, fantastic. if not, oh well.  better to move to trash instead?
    [fileManager removeFileAtPath:mDestPath handler:nil];
  }
}

- (IBAction)close:(id)sender
{
  if (!mDownloadDone)
  {
    mRemoveWhenDone = YES;
    [self cancel];
  }
  else
  {
    [mProgressWindowController removeDownload:self];
  }
}

- (IBAction)stop:(id)sender
{
  [self cancel];
}

- (IBAction)reveal:(id)sender
{
  if ([[NSWorkspace sharedWorkspace] selectFile:mDestPath
                        inFileViewerRootedAtPath:[mDestPath stringByDeletingLastPathComponent]])
    return;
  // hmmm.  it didn't work.  that's odd. need localized error messages. for now, just beep.
  NSBeep();
}

- (IBAction)open:(id)sender
{
  if ([[NSWorkspace sharedWorkspace] openFile:mDestPath])
    return;
  // hmmm.  it didn't work.  that's odd.  need localized error message. for now, just beep.
  NSBeep();
}

// Called just before the view will be shown to the user
- (void)downloadDidStart
{
  mStartTime = [[NSDate alloc] init];
//  [mProgressBar startAnimation:self];	// moved to onStartDownload
  [self refreshDownloadInfo];
}

- (void)downloadDidEnd
{
  mDownloadDone = YES;
  mDownloadTime = -[mStartTime timeIntervalSinceNow];
  [mProgressBar stopAnimation:self];
  
  [mExpandedCancelButton setEnabled:NO];
  [self refreshDownloadInfo];
}

// this handles lots of things.
- (void)refreshDownloadInfo
{
  NSView* curView = [self view];
  
  NSString* filename = [mSourceURL lastPathComponent];
  NSString *destPath = [mDestPath stringByAbbreviatingWithTildeInPath];
  NSString* tooltipFormat = NSLocalizedString(mDownloadDone ? @"DownloadedTooltipFormat" : @"DownloadingTooltipFormat", @"");

  id filenameLabel = [curView viewWithTag:kLabelTagFilename];
  [filenameLabel setStringValue:filename];
  [filenameLabel setToolTip:[NSString stringWithFormat:tooltipFormat, [mSourceURL lastPathComponent], mSourceURL, destPath]];
  
  id destLabel = [curView viewWithTag:kLabelTagDestination];
  [destLabel setStringValue:destPath];

  id locationLabel = [curView viewWithTag:kLabelTagSource];
  [locationLabel setStringValue:mSourceURL];

  if (mDownloadDone)
  {
    id statusLabel = [curView viewWithTag:kLabelTagStatus];
    if (statusLabel)
    {
      NSString* statusString;
      if (mUserCancelled)
        statusString = NSLocalizedString(@"DownloadCancelled", @"Cancelled");
      else if (mDownloadingError)
        statusString = NSLocalizedString(@"DownloadInterrupted", @"Interrupted");
      else
        statusString = NSLocalizedString(@"DownloadCompleted", @"Completed");
      
      [statusLabel setStringValue: statusString];
    }

    // set progress label
    id progressLabel = [curView viewWithTag:kLabelTagProgress];
    if (progressLabel)
    {
      float byteSec = mCurrentProgress / mDownloadTime;
      // show how much we downloaded, become some types of disconnects make us think
      // we finished successfully
      [progressLabel setStringValue:[NSString stringWithFormat:
          NSLocalizedString(@"DownloadDoneStatusString", @"%@ of %@ done (at %@/sec)"), 
              [[self class] formatBytes:mCurrentProgress],
              [[self class] formatBytes:mDownloadSize],
              [[self class] formatBytes:byteSec]]];
    }
    
    id timeLabel = [curView viewWithTag:kLabelTagTimeRemaining];
    if (timeLabel)
    	[timeLabel setStringValue:[[self class] formatTime:(int)mDownloadTime]];
      
    id timeLabelLabel = [curView viewWithTag:kLabelTagTimeRemainingLabel];
    if (timeLabelLabel)
      [timeLabelLabel setStringValue:NSLocalizedString(@"DownloadRemainingLabelDone", @"Time elapsed:")];
      
    [self updateButtons];
  }
  else
  {
    NSTimeInterval elapsedTime = -[mStartTime timeIntervalSinceNow];

    // update status field
    id progressLabel = [curView viewWithTag:kLabelTagProgress];
    if (progressLabel)
    {
      NSString *statusLabelString = NSLocalizedString(@"DownloadStatusString", @"%@ of %@ total (at %@/sec)");
      float byteSec = mCurrentProgress / elapsedTime;
      [progressLabel setStringValue:[NSString stringWithFormat:statusLabelString, 
                                      [[self class] formatBytes:mCurrentProgress],
                                      (mDownloadSize > 0 ? [[self class] formatBytes:mDownloadSize] : @"?"),
                                      [[self class] formatBytes:byteSec]]];
    }
    
    id timeLabel = [curView viewWithTag:kLabelTagTimeRemaining];
    if (timeLabel)
    {
      if (mDownloadSize > 0)
      {
        int secToGo = (int)ceil((elapsedTime * mDownloadSize / mCurrentProgress) - elapsedTime);
        [timeLabel setStringValue:[[self class] formatFuzzyTime:secToGo]];
      }
      else // mDownloadSize is undetermined.  Set remaining time to question marks.
      {
        NSString *calculatingString = NSLocalizedString(@"DownloadCalculatingString", @"Unknown");
        [timeLabel setStringValue:calculatingString];
      }
    }
  }
}

- (void)updateButtons
{
  // note: this will stat every time, which will be expensive! We could use
  // FNNotify/FNSubscribe to avoid this (writing a Cocoa wrapper around it).
  if (mDownloadDone && !mDownloadingError)
  {
    BOOL destFileExists = [[NSFileManager defaultManager] fileExistsAtPath:mDestPath];
    [mExpandedRevealButton setEnabled:destFileExists];
    [mExpandedOpenButton   setEnabled:destFileExists];
  }
}

- (void)moveProgressBarToCurrentView
{
  [mProgressBar moveToView:(mViewIsCompact ? mProgressViewCompact : mProgressView) resize:YES];
  [mProgressBar startAnimation:self];		// this is necessary to keep it animating for some reason
}

- (void)setProgressWindowController:(ProgressDlgController*)progressWindowController
{
  mProgressWindowController = progressWindowController;
}

- (BOOL)isActive
{
  return !mDownloadDone;
}

#pragma mark -

- (void)onStartDownload:(BOOL)isFileSave
{
  mIsFileSave = isFileSave;
  [self downloadDidStart];
  [mProgressWindowController didStartDownload:self];

  // need to do this after the view as been put in the window, otherwise it doesn't work
  [mProgressBar startAnimation:self];
}

- (void)onEndDownload:(BOOL)completedOK
{
  mDownloadingError = !completedOK;

  [self downloadDidEnd];
  [mProgressWindowController didEndDownload:self];
  if (mRemoveWhenDone)
    [mProgressWindowController removeDownload:self];
}

- (void)setProgressTo:(long)aCurProgress ofMax:(long)aMaxProgress
{
  mCurrentProgress = aCurProgress;         // fall back for stat calcs
  mDownloadSize = aMaxProgress;
  
  if (![mProgressBar isIndeterminate])      //most likely - just update value
  {
    if (aCurProgress == aMaxProgress)      //handles little bug in FTP download size
      [mProgressBar setMaxValue:aMaxProgress];
    
    [mProgressBar setDoubleValue:aCurProgress];
  }
  else if (aMaxProgress > 0)               // ok, we're starting up with good max & cur values
  {
    [mProgressBar setIndeterminate:NO];
    [mProgressBar setMaxValue:aMaxProgress];
    [mProgressBar setDoubleValue:aCurProgress];
  } // if neither case was true, it's barber pole city.
}

-(void)setDownloadListener:(CHDownloader*)aDownloader
{
  if (mDownloader != aDownloader)
    NS_IF_RELEASE(mDownloader);
  
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
  if (mDestPath && mSourceURL)
  {
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
    
    if (errorInfo)
    {
        NSLog(@"Get error when running AppleScript to set comments for '%@':\n %@", 
                                mDestPath,
                                [errorInfo objectForKey:NSAppleScriptErrorMessage]);
    }
  }
}
#endif

- (void)setSourceURL:(NSString*)aSourceURL
{
  [mSourceURL autorelease];
  mSourceURL = [aSourceURL copy];
  
  //[self tryToSetFinderComments];
}

- (void)setDestinationPath:(NSString*)aDestPath
{
  [mDestPath autorelease];
  mDestPath = [aDestPath copy];
  //[self tryToSetFinderComments];
}

@end
