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
 *   Simon Fraser <sfraser@netscape.com>
 *   Calum Robinson <calumr@mac.com>
 *   Josh Aas <josh@mozilla.com>
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
  indicator and some text fields for status info. 
  
  After a ProgressViewController is requested, the CHStackView is reloaded,
  which causes it to ask the ProgressDlgController (it's datasource) to
  provide it with a list of all the subviews to be diaplyed. It calculates
  it's new frame, and arranges the subviews in a vertical list.
*/

#import "CHDownloadProgressDisplay.h"
#import "ProgressViewController.h"
#import "FileChangeWatcher.h"

//
// interface ProgressDlgController
//
// A window controller managing multiple simultaneous downloads. The user
// can cancel, remove, or get information about any of the downloads it
// manages. It maintains one |ProgressViewController| object for each download.
//

@interface ProgressDlgController : NSWindowController<CHDownloadDisplayFactory, CHStackViewDataSource>
{
  IBOutlet CHStackView*  mStackView;
  IBOutlet NSScrollView* mScrollView;
  
  NSSize                mDefaultWindowSize;
  NSTimer*              mDownloadTimer;            // used for updating the status, STRONG ref
  NSMutableArray*       mProgressViewControllers;  // the downloads we manage, STRONG ref
  int                   mNumActiveDownloads;
  int                   mSelectionPivotIndex;
  BOOL                  mShouldCloseWindow;         // true when a download completes when termination modal sheet is up
  BOOL                  mAwaitingTermination;       // true when we are waiting for users termination modal sheet
  
  FileChangeWatcher*    mFileChangeWatcher;         // strong ref.
}

+(ProgressDlgController *)sharedDownloadController;           // creates if necessary
+(ProgressDlgController *)existingSharedDownloadController;   // doesn't create

-(IBAction)cancel:(id)sender;
-(IBAction)remove:(id)sender;
-(IBAction)reveal:(id)sender;
-(IBAction)cleanUpDownloads:(id)sender;
-(IBAction)open:(id)sender;
-(IBAction)pause:(id)sender;
-(IBAction)resume:(id)sender;

-(int)numDownloadsInProgress;
-(void)clearAllDownloads;
-(void)didStartDownload:(ProgressViewController*)progressDisplay;
-(void)didEndDownload:(id <CHDownloadProgressDisplay>)progressDisplay withSuccess:(BOOL)completedOK statusCode:(nsresult)status;
-(void)removeDownload:(id <CHDownloadProgressDisplay>)progressDisplay suppressRedraw:(BOOL)suppressRedraw;
-(NSApplicationTerminateReply)allowTerminate;
-(void)applicationWillTerminate;
-(void)saveProgressViewControllers;
-(void)loadProgressViewControllers;

-(void)addFileDelegateToWatchList:(id<WatchedFileDelegate>)aWatchedFileDelegate;
-(void)removeFileDelegateFromWatchList:(id<WatchedFileDelegate>)aWatchedFileDelegate;

@end
