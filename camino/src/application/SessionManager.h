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
 * The Original Code is Camino code.
 *
 * The Initial Developer of the Original Code is
 * Stuart Morgan
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Morgan <stuart.morgan@alumni.case.edu>
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

#import <Cocoa/Cocoa.h>

//
// SessionManager
//
// A shared object capable of saving and restoring the application state
// so that user sessions can be persisted across quit/relaunch.
//
@interface SessionManager : NSObject
{
  NSString* mSessionStatePath;    // strong
  NSTimer*  mDelayedPersistTimer; // strong
  BOOL      mDirty;
}

+ (SessionManager*)sharedInstance;

// Notifies the SessionManager that the window state has changed. This
// will eventually cause the window state to be saved to a file in the
// profile directory, but changes may be coalesced before saving.
- (void)windowStateChanged;

// Immediately saves the window state to a file in the profile directory.
- (void)saveWindowState;

// Restores the window state from a file in the profile directory.
- (void)restoreWindowState;

// Deletes the window state file from the profile directory.
- (void)clearSavedState;

// Indicates whether there is persisted state available to restore.
- (BOOL)hasSavedState;

@end
