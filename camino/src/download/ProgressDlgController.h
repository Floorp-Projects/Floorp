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
 *    Simon Fraser <sfraser@netscape.com>
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

#import <AppKit/AppKit.h>

#import "CHDownloadProgressDisplay.h"
#import "CHStackView.h"

/*
  How ProgressViewController and ProgressDlgController work. 
  
  ProgressDlgController manages the window the the downloads are displayed in.
  It contains a single CHStackView, a custom class that asks its datasource
  for a list of views to display, in a similar fashion to the way NSTableView
  asks its datasource for data to display. There is a single instance of
  ProgressDlgController, returned by +sharedDownloadController. 
  
  The ProgressDlgController is a subclass of CHDownloadController, which
  means that it gets asked to create new objects conforming to the
  CHDownloadProgressDisplay protocol, which are used to display
  the progress of a single download. It does so by returning instances of
  ProgressViewController, which manage an NSView that contains a progress
  indicator, some text fields for status info and a cancel button. 
  
  After a ProgressViewController is requested, the CHStackView is reloaded,
  which causes it to ask the ProgressDlgController (it's datasource) to
  provide it with a list of all the subviews to be diaplyed. It calculates
  it's new frame, and arranges the subviews in a vertical list. 
  
  The ProgressDlgController now needs to resize its window. It knows when
  to do this because is watches for changes in the CHStackViews frame (using
  NSViews built in NSNotification for this). 
  
  
  Expanding/contracting download progress views
  
  When a disclosure triangle is clicked, the ProgressViewController just swaps
  the expanded view for a smaller one. It saves the new state as the users
  preference for "browser.download.compactView". If the option key was held down,
  a notification is posted (that all ProgressViewControllers listen for) that
  makes all ProgressViewControllers change their state to the new state of the sender. 

*/

#import "CHDownloadProgressDisplay.h"

@interface ProgressDlgController : NSWindowController<CHDownloadDisplayFactory, CHStackViewDataSource>
{
  IBOutlet CHStackView  *mStackView;
  IBOutlet NSScrollView *mScrollView;
  IBOutlet NSTextField  *mNoDownloadsText;
  
  NSSize                mDefaultWindowSize;                
  NSTimer               *mDownloadTimer;
  NSMutableArray        *mProgressViewControllers;
  int                   mNumActiveDownloads;
}

+ (ProgressDlgController *)sharedDownloadController;

- (int)numDownloadsInProgress;

- (void)autosaveWindowFrame;

- (void)setupDownloadTimer;
- (void)killDownloadTimer;
- (void)setDownloadProgress:(NSTimer *)aTimer;

- (void)didStartDownload:(id <CHDownloadProgressDisplay>)progressDisplay;
- (void)didEndDownload:(id <CHDownloadProgressDisplay>)progressDisplay;
- (void)removeDownload:(id <CHDownloadProgressDisplay>)progressDisplay;

- (NSApplicationTerminateReply)allowTerminate;

@end
