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

#import "ProgressDlgController.h"

#import "ProgressViewController.h"
#import "PreferenceManager.h"

static NSString *ProgressWindowFrameSaveName      = @"ProgressWindow";



@interface ProgressDlgController(PrivateProgressDlgController)

- (void)resizeWindowToFit;
- (void)rebuildViews;
- (NSSize)windowSizeForStackSize:(NSSize)stackSize;

@end



@implementation ProgressDlgController

static id gSharedProgressController = nil;

+ (ProgressDlgController *)sharedDownloadController;
{
  if (gSharedProgressController == nil)
    gSharedProgressController = [[ProgressDlgController alloc] init];
  
  return gSharedProgressController;
}

- (id)init
{
  if ((self == [super initWithWindowNibName:@"ProgressDialog"]))
  {
    // Register for notifications when the stack view changes size
    [[NSNotificationCenter defaultCenter] addObserver:self
                                            selector:@selector(stackViewResized:)
                                            name:StackViewResizedNotificationName
                                            object:nil];

    mProgressViewControllers = [[NSMutableArray alloc] init];
    
    mDefaultWindowSize = [[self window] frame].size;
    // it would be nice if we could get the frame from the name, and then
    // mess with it before setting it.
    [[self window] setFrameUsingName:ProgressWindowFrameSaveName];
    // set the window to its default height
    NSRect windowFrame = [[self window] frame];
    windowFrame.size.height = mDefaultWindowSize.height;
    [[self window] setFrame:windowFrame display:NO];
    
    // We provide the views for the stack view, from mProgressViewControllers
    [mStackView setDataSource:self];
    
    [mScrollView setDrawsBackground:NO];
    [mNoDownloadsText retain];		// so we can remove it from its superview
  }

  return self;
}

- (void)dealloc
{
  if (self == gSharedProgressController)
    gSharedProgressController = nil;

  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [mProgressViewControllers release];
  [mNoDownloadsText release];
  [self killDownloadTimer];
  [super dealloc];
}

- (void)didStartDownload:(id <CHDownloadProgressDisplay>)progressDisplay
{
  [self showWindow:nil];		// make sure the window is visible

	[self rebuildViews];
  [self setupDownloadTimer];
}

- (void)didEndDownload:(id <CHDownloadProgressDisplay>)progressDisplay
{
  [self rebuildViews];		// to swap in the completed view
}

- (void)removeDownload:(id <CHDownloadProgressDisplay>)progressDisplay
{
  [mProgressViewControllers removeObject:progressDisplay];
  
  if ([mProgressViewControllers count] == 0)
  {
    // Stop doing stuff if there aren't any downloads going on
    [self killDownloadTimer];
  }
  
  [self rebuildViews];
}

- (void)stackViewResized:(NSNotification *)notification
{
  NSDictionary* userInfo = [notification userInfo];
  NSSize oldStackSize = [[userInfo objectForKey:@"oldsize"] sizeValue];
  
  // this code is used to auto-resize the downloads window when
  // its contents change size, if the window is in its standard, "zoomed"
  // state. This allows the user to choose between auto-resizing behavior,
  // by leaving the window alone, or their own size, by resizing it.
  
  // get the size the window would have been if it had been in the
  // standard state, given the old size of the contents
  NSSize oldZoomedWindowSize = [self windowSizeForStackSize:oldStackSize];
  NSSize curWindowSize = [[self window] frame].size;
    
  // only resize if the window matches the stack size
  if (CHCloseSizes(oldZoomedWindowSize, curWindowSize, 4.0))
    [self resizeWindowToFit];
}

// given the dimensions of our stack view, return the dimensions of the window,
// assuming the window is zoomed to show as much of the contents as possible.
- (NSSize)windowSizeForStackSize:(NSSize)stackSize
{
  NSSize actualScrollFrame = [mScrollView frame].size;
  NSSize scrollFrameSize   = [NSScrollView frameSizeForContentSize:stackSize
               hasHorizontalScroller:NO hasVerticalScroller:YES borderType:NSNoBorder];
  
  // frameSizeForContentSize seems to return a width 1 pixel too narrow
  scrollFrameSize.width += 1;
  
  NSRect contentRect       = [[[self window] contentView] frame];
  contentRect.size.width  += scrollFrameSize.width  - actualScrollFrame.width;
  contentRect.size.height += scrollFrameSize.height - actualScrollFrame.height;
  contentRect.origin = [[self window] convertBaseToScreen:contentRect.origin];	// convert to screen

  NSRect advisoryWindowFrame = [NSWindow frameRectForContentRect:contentRect styleMask:[[self window] styleMask]];
  NSRect constrainedRect     = [[self window] constrainFrameRect:advisoryWindowFrame toScreen:[[self window] screen]];
  return constrainedRect.size;
}

- (void)resizeWindowToFit
{
  if ([mProgressViewControllers count] > 0)
  {
    NSSize scrollFrameSize = [NSScrollView frameSizeForContentSize:[mStackView bounds].size
          hasHorizontalScroller:NO hasVerticalScroller:YES borderType:NSNoBorder];
    NSSize curScrollFrameSize = [mScrollView frame].size;
    
    NSRect windowFrame = [[self window] frame];

    float frameDelta = (scrollFrameSize.height - curScrollFrameSize.height);
    windowFrame.size.height += frameDelta;
    windowFrame.origin.y    -= frameDelta;		// maintain top

    [[self window] setFrame:windowFrame display:YES];
  }
}

- (void)rebuildViews
{
  [mStackView reloadSubviews];
  
  if ([mProgressViewControllers count] == 0)
  {
    [[[self window] contentView] addSubview:mNoDownloadsText];
  }
  else
  {
    [mNoDownloadsText removeFromSuperview];
  }
  
}

- (int)numDownloadsInProgress
{
  unsigned int numViews = [mProgressViewControllers count];
  int          numActive = 0;
  
  for (unsigned int i = 0; i < numViews; i++)
  {
    if ([[mProgressViewControllers objectAtIndex:i] isActive])
      ++numActive;
  }
  return numActive;
}

-(void)autosaveWindowFrame
{
  [[self window] saveFrameUsingName:ProgressWindowFrameSaveName];
}

- (void)windowWillClose:(NSNotification *)notification
{
  [self autosaveWindowFrame];
}

- (void)killDownloadTimer
{
  if (mDownloadTimer)
  {
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

// Called by our timer to refresh all the download stats
- (void)setDownloadProgress:(NSTimer *)aTimer
{
  [mProgressViewControllers makeObjectsPerformSelector:@selector(refreshDownloadInfo)];
  // if the window is minimized, we want to update the dock image here. But how?
}

- (NSApplicationTerminateReply)allowTerminate
{
  if ([self numDownloadsInProgress] > 0)
  {
    // make sure the window is visible
    [self showWindow:self];
    
    NSString *alert     = NSLocalizedString(@"QuitWithDownloadsMsg", @"Really Quit?");
    NSString *message   = NSLocalizedString(@"QuitWithDownloadsExpl", @"");
    NSString *okButton  = NSLocalizedString(@"QuitWithdownloadsButtonDefault",@"Cancel");
    NSString *altButton = NSLocalizedString(@"QuitWithdownloadsButtonAlt",@"Quit");

    // while the panel is up, download dialogs won't update (no timers firing) but
    // downloads continue (PLEvents being processed)
    id panel = NSGetAlertPanel(alert, message, okButton, altButton, nil, message);
    
    [NSApp beginSheet:panel
            modalForWindow:[self window]
            modalDelegate:self
            didEndSelector:@selector(sheetDidEnd:returnCode:contextInfo:)
            contextInfo:NULL];
    int sheetResult = [NSApp runModalForWindow: panel];
    [NSApp endSheet: panel];
    [panel orderOut: self];
    NSReleaseAlertPanel(panel);
    
    return (sheetResult == NSAlertDefaultReturn) ? NSTerminateCancel : NSTerminateNow;
  }

  return NSTerminateNow;
}

- (void)sheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
  [NSApp stopModalWithCode:returnCode];
}

#pragma mark -

// implement to zoom to a size that just fits the contents
- (NSRect)windowWillUseStandardFrame:(NSWindow *)sender defaultFrame:(NSRect)defaultFrame
{
  NSSize scrollFrameSize = [NSScrollView frameSizeForContentSize:[mStackView bounds].size
        hasHorizontalScroller:NO hasVerticalScroller:YES borderType:NSNoBorder];
  
  NSSize curScrollFrameSize = [mScrollView frame].size;
  float frameDelta = (scrollFrameSize.height - curScrollFrameSize.height);

  NSRect windowFrame = [[self window] frame];
  windowFrame.size.height += frameDelta;
  windowFrame.origin.y    -= frameDelta;		// maintain top

  windowFrame.size.width  = mDefaultWindowSize.width;
  // cocoa will ensure that the window fits onscreen for us
	return windowFrame;
}

#pragma mark -

/*
    CHStackView datasource methods
*/

- (int)subviewsForStackView:(CHStackView *)stackView
{
  return [mProgressViewControllers count];
}

- (NSView *)viewForStackView:(CHStackView *)aResizingView atIndex:(int)index
{
  return [[mProgressViewControllers objectAtIndex:index] view];
}

#pragma mark -

/*
    Just create a progress view, but don't display it (otherwise the URL fields etc. 
    are just blank)
*/
- (id <CHDownloadProgressDisplay>)createProgressDisplay
{
  ProgressViewController *newController = [[ProgressViewController alloc] init];
  [newController setProgressWindowController:self];
  [mProgressViewControllers addObject:newController];
  
  return newController;
}

@end
