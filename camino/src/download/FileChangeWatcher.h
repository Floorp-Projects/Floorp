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
* Nick Kreeger
* Portions created by the Initial Developer are Copyright (C) 2006
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
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

//
// Any object that needs a file to be polled can implement this protocol and 
// register itself with the watch queue.
//
@protocol WatchedFileDelegate

//
// This method will need to return the full path of the file/folder 
// that is being watched.
//
-(const char*)representedFilePath;

//
// This method gets called when the watcher recieves news that the
// watched path has changed.
// Note: This method will be called on a background thread.
//
-(void)fileDidChange;

@end


//
// This class provides a file polling service to any object
// that implements the |WatchedFileDelegate| protocol.
//
@interface FileChangeWatcher : NSObject
{
@private
  NSMutableArray* mWatchedFileDelegates;     // strong ref
  NSMutableArray* mWatchedFileDescriptors;   // strong ref
  
  int          mQueueFileDesc;
  BOOL         mShouldRunThread;
  BOOL         mThreadIsRunning;
}

//
// Add an object implementing the |WatchedFileDelegate| to the 
// watch queue. 
// Note: An object will only be added once to the queue.
//
-(void)addWatchedFileDelegate:(id<WatchedFileDelegate>)aWatchedFileDelegate;

//
// Remove an object implementing the |WatchedFileDelegate| from the watch queue.
//
-(void)removeWatchedFileDelegate:(id<WatchedFileDelegate>)aWatchedFileDelegate;

@end
