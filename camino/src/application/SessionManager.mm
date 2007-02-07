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

#import "SessionManager.h"

#import "PreferenceManager.h"
#import "ProgressDlgController.h"
#import "BrowserWrapper.h"
#import "BrowserTabView.h"
#import "BrowserWindowController.h"
#import "BrowserTabViewItem.h"

const NSString* kBrowserWindowListKey = @"BrowserWindows";
const NSString* kDownloadWindowKey = @"DownloadWindow";
const NSString* kTabListKey = @"Tabs";
const NSString* kSelectedTabKey = @"SelectedTab";
const NSString* kFrameKey = @"Frame";
const NSString* kIsVisibleKey = @"Visible";
const NSString* kIsKeyWindowKey = @"Key";
const NSString* kIsMiniaturizedKey = @"Miniaturized";
const NSString* kToolbarIsVisibleKey = @"ToolbarVisible";
const NSString* kBookmarkBarIsVisibleKey = @"BookmarkBarVisible";

// Number of seconds to coalesce changes before saving them
const NSTimeInterval kPersistDelay = 60.0;

@interface SessionManager(Private)

- (NSDictionary*)dictionaryForCurrentWindowState;
- (void)setWindowStateFromDictionary:(NSDictionary*)windowState;
- (void)setWindowStateIsDirty:(BOOL)isDirty;

@end

@implementation SessionManager

- (id)init
{
  if ((self = [super init])) {
    NSString* profileDir = [[PreferenceManager sharedInstance] profilePath];
    mSessionStatePath = [[profileDir stringByAppendingPathComponent:@"WindowState.plist"] retain];
  }
  return self;
}

- (void)dealloc
{
  [mSessionStatePath release];
  [mDelayedPersistTimer release];
  [super dealloc];
}

+ (SessionManager*)sharedInstance
{
  static SessionManager* sharedSessionManager = nil;
  if (!sharedSessionManager)
    sharedSessionManager = [[SessionManager alloc] init];

  return sharedSessionManager;
}

// Private method to gather all the window state into an NSDictionary. The
// dictionary must contain only plist-safe types so that it can be written
// to file.
- (NSDictionary*)dictionaryForCurrentWindowState
{
  NSArray* windows = [NSApp orderedWindows];
  NSMutableDictionary* state = [NSMutableDictionary dictionary];

  // save browser windows
  NSMutableArray* storedBrowserWindows = [NSMutableArray arrayWithCapacity:[windows count]];
  NSEnumerator* windowEnumerator = [windows objectEnumerator];
  NSWindow* window;
  while ((window = [windowEnumerator nextObject])) {
    if ([[window windowController] isMemberOfClass:[BrowserWindowController class]]) {
      BrowserWindowController* browser = (BrowserWindowController*)[window windowController];

      NSMutableArray* storedTabs = [NSMutableArray array];
      BrowserTabView* tabView = [browser getTabBrowser];
      NSEnumerator* tabEnumerator = [[tabView tabViewItems] objectEnumerator];
      BrowserTabViewItem* tab;
      while ((tab = [tabEnumerator nextObject])) {
        NSString* foundWindowURL = [(BrowserWrapper*)[tab view] getCurrentURI];
        [storedTabs addObject:foundWindowURL];
      }
      int selectedTabIndex = [tabView indexOfTabViewItem:[tabView selectedTabViewItem]];
      NSMutableDictionary* windowInfo = [NSDictionary dictionaryWithObjectsAndKeys:
          storedTabs, kTabListKey,
          [NSNumber numberWithInt:selectedTabIndex], kSelectedTabKey,
          [NSNumber numberWithBool:[window isMiniaturized]], kIsMiniaturizedKey,
          [NSNumber numberWithBool:[[window toolbar] isVisible]], kToolbarIsVisibleKey,
          [NSNumber numberWithBool:[[browser bookmarkToolbar] isVisible]], kBookmarkBarIsVisibleKey,
          NSStringFromRect([window frame]), kFrameKey,
          nil];

      //TODO (possibly): session history, scroll position, chrome flags, tab scroll positon (trunk)
      [storedBrowserWindows addObject:windowInfo];
    }
  }
  [state setValue:storedBrowserWindows forKey:kBrowserWindowListKey];

  // save download window state
  ProgressDlgController* downloadWindowController = [ProgressDlgController existingSharedDownloadController];
  NSDictionary* downloadWindowState = [NSDictionary dictionaryWithObjectsAndKeys:
      [NSNumber numberWithBool:[[downloadWindowController window] isMiniaturized]], kIsMiniaturizedKey,
      [NSNumber numberWithBool:[[downloadWindowController window] isKeyWindow]], kIsKeyWindowKey,
      [NSNumber numberWithBool:[[downloadWindowController window] isVisible]], kIsVisibleKey,
      nil];
  [state setValue:downloadWindowState forKey:kDownloadWindowKey];

  return state;
}

// Private method to restore the window state from an NSDictionary.
// Note that any windows currently open are unaffected by the restore, so to
// get a clean restore all windows should be closed before calling this.
- (void)setWindowStateFromDictionary:(NSDictionary*)windowState
{
  // restore download window if it was showing
  NSDictionary* downloadWindowState = [windowState objectForKey:kDownloadWindowKey];
  ProgressDlgController* downloadController = nil;
  if ([[downloadWindowState objectForKey:kIsVisibleKey] boolValue] ||
      [[downloadWindowState objectForKey:kIsMiniaturizedKey] boolValue]) {
    downloadController = [ProgressDlgController sharedDownloadController];
    [downloadController showWindow:self];
    if ([[downloadWindowState objectForKey:kIsMiniaturizedKey] boolValue])
      [[downloadController window] miniaturize:self];
  }

  // restore browser windows
  NSArray* storedBrowserWindows = [windowState objectForKey:kBrowserWindowListKey];
  // the array is in front to back order, so we want to restore windows in reverse
  NSEnumerator* storedWindowEnumerator = [storedBrowserWindows reverseObjectEnumerator];
  NSDictionary* windowInfo;
  while ((windowInfo = [storedWindowEnumerator nextObject])) {
    BrowserWindowController* browser = [[BrowserWindowController alloc] initWithWindowNibName:@"BrowserWindow"];

    [[[browser window] toolbar] setVisible:[[windowInfo valueForKey:kToolbarIsVisibleKey] boolValue]];
    [[browser bookmarkToolbar] setVisible:[[windowInfo valueForKey:kBookmarkBarIsVisibleKey] boolValue]];
    [[browser window] setFrame:NSRectFromString([windowInfo valueForKey:kFrameKey]) display:YES];

    [browser showWindow:self];
    [browser openURLArray:[windowInfo objectForKey:kTabListKey] tabOpenPolicy:eReplaceTabs allowPopups:NO];
    [[browser getTabBrowser] selectTabViewItemAtIndex:[[windowInfo objectForKey:kSelectedTabKey] intValue]];

    if ([[windowInfo valueForKey:kIsMiniaturizedKey] boolValue])
      [[browser window] miniaturize:self];
  }

  // if the download window was key, pull it to the front
  if ([[downloadWindowState objectForKey:kIsKeyWindowKey] boolValue])
    [[downloadController window] makeKeyAndOrderFront:self];
}

- (void)saveWindowState
{
  NSData* stateData = [NSPropertyListSerialization dataFromPropertyList:[self dictionaryForCurrentWindowState]
                                                                 format:NSPropertyListBinaryFormat_v1_0
                                                       errorDescription:NULL];
  [stateData writeToFile:mSessionStatePath atomically:YES];
  [self setWindowStateIsDirty:NO];
}

- (void)restoreWindowState
{
  NSData* stateData = [NSData dataWithContentsOfFile:mSessionStatePath];
  NSDictionary* state = [NSPropertyListSerialization propertyListFromData:stateData
                                                         mutabilityOption:NSPropertyListImmutable
                                                                   format:NULL
                                                         errorDescription:NULL];
  if (state)
    [self setWindowStateFromDictionary:state];
}

- (void)clearSavedState
{
  NSFileManager* fileManager = [NSFileManager defaultManager];
  if ([fileManager fileExistsAtPath:mSessionStatePath])
    [fileManager removeFileAtPath:mSessionStatePath handler:nil];

  // Ensure that we don't immediately write out a new file
  [self setWindowStateIsDirty:NO];
}

- (void)windowStateChanged
{
  [self setWindowStateIsDirty:YES];
}

- (void)setWindowStateIsDirty:(BOOL)isDirty
{
  if (isDirty == mDirty)
    return;

  mDirty = isDirty;
  if (mDirty) {
    mDelayedPersistTimer = [[NSTimer scheduledTimerWithTimeInterval:kPersistDelay
                                                             target:self
                                                           selector:@selector(performDelayedPersist:)
                                                           userInfo:nil
                                                            repeats:NO] retain];
  }
  else {
    [mDelayedPersistTimer invalidate];
    [mDelayedPersistTimer release];
    mDelayedPersistTimer = nil;
  }
}

- (void)performDelayedPersist:(NSTimer*)theTimer
{
  [self saveWindowState];
}

- (BOOL)hasSavedState
{
  NSFileManager* fileManager = [NSFileManager defaultManager];
  return [fileManager fileExistsAtPath:mSessionStatePath];
}

@end
