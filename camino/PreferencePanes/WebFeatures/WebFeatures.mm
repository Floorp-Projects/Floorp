/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   william@dell.wisner.name (William Dell Wisner)
 *   josh@mozilla.com (Josh Aas)
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

#import "WebFeatures.h"
#import "NSString+Utils.h"

#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsIPermissionManager.h"
#include "nsIPermission.h"
#include "nsISupportsArray.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsIFile.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsNetUtil.h"

// we should really get this from "CHBrowserService.h",                         
// but that requires linkage and extra search paths.                            
static NSString* XPCOMShutDownNotificationName = @"XPCOMShutDown";              

// needs to match the string in PreferenceManager.mm
static NSString* const AdBlockingChangedNotificationName = @"AdBlockingChanged";

// for camino.enable_plugins; needs to match string in BrowserWrapper.mm
static NSString* const kEnablePluginsChangedNotificationName = @"EnablePluginsChanged";

// for accessibility.tabfocus
const int kFocusLinks = (1 << 2);
const int kFocusForms = (1 << 1);

// for annoyance blocker prefs
const int kAnnoyancePrefNone = 1;
const int kAnnoyancePrefAll  = 2;
const int kAnnoyancePrefSome = 3;

@interface OrgMozillaChimeraPreferenceWebFeatures(PRIVATE)

-(NSString*)profilePath;
- (int)annoyingWindowPrefs;
- (int)preventAnimationCheckboxState;

@end

@implementation OrgMozillaChimeraPreferenceWebFeatures

- (void) dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  NS_IF_RELEASE(mManager);
  [super dealloc];
}

- (void)xpcomShutdown:(NSNotification*)notification
{
  // this nulls the pointer
  NS_IF_RELEASE(mManager);
}

- (void)mainViewDidLoad
{
  if (!mPrefService)
    return;

  // we need to register for xpcom shutdown so that we can clear the            
  // services before XPCOM is shut down. We can't rely on dealloc,              
  // because we don't know when it will get called (we might be autoreleased).  
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(xpcomShutdown:)
                                               name:XPCOMShutDownNotificationName
                                             object:nil];

  BOOL gotPref = NO;

  // Set initial value on Java/JavaScript checkboxes
  BOOL jsEnabled = [self getBooleanPref:"javascript.enabled" withSuccess:&gotPref] && gotPref;
  [mEnableJS setState:jsEnabled];
  BOOL javaEnabled = [self getBooleanPref:"security.enable_java" withSuccess:&gotPref] && gotPref;
  [mEnableJava setState:javaEnabled];

  // Set initial value on Plug-ins checkbox.
  // If we fail to get the pref, ensure we leave plugins enabled by default.
  BOOL pluginsEnabled = [self getBooleanPref:"camino.enable_plugins" withSuccess:&gotPref] || !gotPref;
  [mEnablePlugins setState:pluginsEnabled];

  // set initial value on popup blocking checkbox and disable the whitelist
  // button if it's off
  BOOL enablePopupBlocking = [self getBooleanPref:"dom.disable_open_during_load" withSuccess:&gotPref] && gotPref;  
  [mEnablePopupBlocking setState:enablePopupBlocking];
  [mEditWhitelist setEnabled:enablePopupBlocking];

  // set initial value on annoyance blocker checkbox.
  if([self annoyingWindowPrefs] == kAnnoyancePrefAll)
    [mEnableAnnoyanceBlocker setState:NSOnState];
  else if([self annoyingWindowPrefs] == kAnnoyancePrefNone)
    [mEnableAnnoyanceBlocker setState:NSOffState];
  else // annoyingWindowPrefs == kAnnoyancePrefSome
    [mEnableAnnoyanceBlocker setState:NSMixedState];

  // set initial value on image-resizing
  BOOL enableImageResize = [self getBooleanPref:"browser.enable_automatic_image_resizing" withSuccess:&gotPref];
  [mImageResize setState:enableImageResize];

  [mPreventAnimation setState:[self preventAnimationCheckboxState]];

  BOOL enableAdBlock = [self getBooleanPref:"camino.enable_ad_blocking" withSuccess:&gotPref];
  [mEnableAdBlocking setState:enableAdBlock];

  // Set inital values for tabfocus pref.  Internally, it's a bitwise additive pref:
  // bit 0 adds focus for text fields (not exposed in the UI, so not given a constant)
  // bit 1 adds focus for other form elements (kFocusForms)
  // bit 2 adds links and linked images (kFocusLinks)
  int tabFocusMask = [self getIntPref:"accessibility.tabfocus" withSuccess:&gotPref];
  [mTabToLinks setState:((tabFocusMask & kFocusLinks) ? NSOnState : NSOffState)];
  [mTabToFormElements setState:((tabFocusMask & kFocusForms) ? NSOnState : NSOffState)];

  // store permission manager service and cache the enumerator.
  nsCOMPtr<nsIPermissionManager> pm ( do_GetService(NS_PERMISSIONMANAGER_CONTRACTID) );
  mManager = pm.get();
  NS_IF_ADDREF(mManager);
}


//
// -clickEnableJS:
//
// Enable and disable JavaScript
//
-(IBAction) clickEnableJS:(id)sender
{
  [self setPref:"javascript.enabled" toBoolean:([sender state] == NSOnState)];
}

//
// -clickEnableJava:
//
// Enable and disable Java
//
-(IBAction) clickEnableJava:(id)sender
{
  [self setPref:"security.enable_java" toBoolean:([sender state] == NSOnState)];
}

//
// -clickEnablePlugins:
//
// Enable and disable plugins
//
-(IBAction) clickEnablePlugins:(id)sender
{
  [self setPref:"camino.enable_plugins" toBoolean:([sender state] == NSOnState)];
  [[NSNotificationCenter defaultCenter] postNotificationName:kEnablePluginsChangedNotificationName object:nil];
}

//
// -clickEnableAdBlocking:
//
// Enable and disable ad blocking via a userContent.css file that we provide in our
// package, copied into the user's profile.
//
- (IBAction)clickEnableAdBlocking:(id)sender
{
  [self setPref:"camino.enable_ad_blocking" toBoolean:([sender state] == NSOnState)];
  [[NSNotificationCenter defaultCenter] postNotificationName:AdBlockingChangedNotificationName object:nil]; 
}

//
// clickEnablePopupBlocking
//
// Enable and disable mozilla's popup blocking feature. We use a combination of 
// two prefs to suppress bad popups.
//
- (IBAction)clickEnablePopupBlocking:(id)sender
{
  [self setPref:"dom.disable_open_during_load" toBoolean:([sender state] == NSOnState)];
  [mEditWhitelist setEnabled:[sender state]];
}

//
// clickEnableImageResizing:
//
// Enable and disable mozilla's auto image resizing feature.
//
-(IBAction) clickEnableImageResizing:(id)sender
{
  [self setPref:"browser.enable_automatic_image_resizing" toBoolean:([sender state] == NSOnState)];
}

//
// -clickPreventAnimation:
//
// Enable and disable mozilla's limiting of how animated images repeat
//
-(IBAction) clickPreventAnimation:(id)sender
{
  [sender setAllowsMixedState:NO];
  [self setPref:"image.animation_mode" toString:([sender state] ? @"once" : @"normal")];
}

//
// populatePermissionCache:
//
// walks the permission list for popups building up a cache that
// we can quickly refer to later.
//
-(void) populatePermissionCache:(nsISupportsArray*)inPermissions
{
  nsCOMPtr<nsISimpleEnumerator> permEnum;
  if ( mManager ) 
    mManager->GetEnumerator(getter_AddRefs(permEnum));

  if ( inPermissions && permEnum ) {
    PRBool hasMoreElements = PR_FALSE;
    permEnum->HasMoreElements(&hasMoreElements);
    while ( hasMoreElements ) {
      nsCOMPtr<nsISupports> curr;
      permEnum->GetNext(getter_AddRefs(curr));
      nsCOMPtr<nsIPermission> currPerm(do_QueryInterface(curr));
      if ( currPerm ) {
        nsCAutoString type;
        currPerm->GetType(type);
        if ( type.Equals(NS_LITERAL_CSTRING("popup")) )
          inPermissions->AppendElement(curr);
      }
      permEnum->HasMoreElements(&hasMoreElements);
    }
  }
}

//
// editWhitelist:
//
// put up a sheet to allow people to edit the popup blocker whitelist
//
-(IBAction) editWhitelist:(id)sender
{
  // build parallel permission list for speed with a lot of blocked sites
  NS_NewISupportsArray(&mCachedPermissions);     // ADDREFs
  [self populatePermissionCache:mCachedPermissions];

  [NSApp beginSheet:mWhitelistPanel
        modalForWindow:[mEditWhitelist window]   // any old window accessor
        modalDelegate:self
        didEndSelector:@selector(editWhitelistSheetDidEnd:returnCode:contextInfo:)
        contextInfo:NULL];

  // ensure a row is selected (cocoa doesn't do this for us, but will keep
  // us from unselecting a row once one is set; go figure).
  [mWhitelistTable selectRow:0 byExtendingSelection:NO];

  [mWhitelistTable setDeleteAction:@selector(removeWhitelistSite:)];
  [mWhitelistTable setTarget:self];

  [mAddButton setEnabled:NO];

  // we shouldn't need to do this, but the scrollbar won't enable unless we
  // force the table to reload its data. Oddly it gets the number of rows correct,
  // it just forgets to tell the scrollbar. *shrug*
  [mWhitelistTable reloadData];
}

// whitelist sheet methods
-(IBAction) editWhitelistDone:(id)aSender
{
  // save stuff??

  [mWhitelistPanel orderOut:self];
  [NSApp endSheet:mWhitelistPanel];

  NS_IF_RELEASE(mCachedPermissions);
}

-(IBAction) removeWhitelistSite:(id)aSender
{
  if ( mCachedPermissions && mManager ) {
    // remove from parallel array and cookie permissions list
    int row = [mWhitelistTable selectedRow];

    // remove from permission manager (which is done by host, not by row), then 
    // remove it from our parallel array (which is done by row). Since we keep a
    // parallel array, removing multiple items by row is very difficult since after 
    // deleting, the array is out of sync with the next cocoa row we're told to remove. Punt!
    nsCOMPtr<nsISupports> rowItem = dont_AddRef(mCachedPermissions->ElementAt(row));
    nsCOMPtr<nsIPermission> perm ( do_QueryInterface(rowItem) );
    if ( perm ) {
      nsCAutoString host;
      perm->GetHost(host);
      mManager->Remove(host, "popup");           // could this api _be_ any worse? Come on!

      mCachedPermissions->RemoveElementAt(row);
    }
    [mWhitelistTable reloadData];
  }
}

//
// addWhitelistSite:
//
// adds a new site to the permission manager whitelist for popups
//
-(IBAction) addWhitelistSite:(id)sender
{
  if ( mCachedPermissions && mManager ) {
    // ensure url has a http:// on the front or NS_NewURI will fail. The PM
    // really doesn't care what the protocol is, we just need to have something
    NSString* url = [[mAddField stringValue] stringByRemovingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if ( ![url rangeOfString:@"http://"].length && ![url rangeOfString:@"https://"].length )
      url = [NSString stringWithFormat:@"http://%@", url];

    const char* siteURL = [url UTF8String];
    nsCOMPtr<nsIURI> newURI;
    NS_NewURI(getter_AddRefs(newURI), siteURL);
    if ( newURI ) {
      mManager->Add(newURI, "popup", nsIPermissionManager::ALLOW_ACTION);
      mCachedPermissions->Clear();
      [self populatePermissionCache:mCachedPermissions];

      [mAddField setStringValue:@""];
      [mAddButton setEnabled:NO];
      [mWhitelistTable reloadData];
    }
  }
}

- (void) editWhitelistSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo
{

}

// data source informal protocol (NSTableDataSource)
- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
  PRUint32 numRows = 0;
  if ( mCachedPermissions )
    mCachedPermissions->Count(&numRows);

  return (int) numRows;
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
  NSString* retVal = nil;
  if ( mCachedPermissions ) {
    nsCOMPtr<nsISupports> rowItem = dont_AddRef(mCachedPermissions->ElementAt(rowIndex));
    nsCOMPtr<nsIPermission> perm ( do_QueryInterface(rowItem) );
    if ( perm ) {
      // only 1 column and it's the website url column
      nsCAutoString host;
      perm->GetHost(host);
      retVal = [NSString stringWithCString:host.get()];
    }
  }

  return retVal;
}

- (void)controlTextDidChange:(NSNotification*)notification
{
  [mAddButton setEnabled:[[mAddField stringValue] length] > 0];
}

//
// clickTabFocusCheckboxes:
//
// Enable and disable tabbing to various elements.  Internally, it's a bitwise additive pref:
// bit 0 adds focus for text fields (not exposed in the UI, so not given a constant)
// bit 1 adds focus for other form elements (kFocusForms)
// bit 2 adds links and linked images (kFocusLinks)
// By default, the pref is set to binary 011 (text fields and forms)
//
-(IBAction) clickTabFocusCheckboxes:(id)sender
{
  BOOL gotPref;
  int tabFocusValue = [self getIntPref:"accessibility.tabfocus" withSuccess:&gotPref];

  if (sender == mTabToFormElements)
  {
    if ([sender state])
      tabFocusValue |= kFocusForms; // turn the forms bit on
    else
      tabFocusValue &= ~kFocusForms; // turn the forms bit off
  } 
  else // sender == mTabToLinks
  {
    if ([sender state])
      tabFocusValue |= kFocusLinks; // turn the links bit on
    else
      tabFocusValue &= ~kFocusLinks; // turn the links bit off
  }
  [self setPref:"accessibility.tabfocus" toInt:tabFocusValue];
}

//
// clickEnableAnnoyanceBlocker:
//
// Enable and disable prefs for allowing webpages to be annoying and move/resize the
// window or tweak the status bar and make it unusable.
//
-(IBAction) clickEnableAnnoyanceBlocker:(id)sender
{
  [sender setAllowsMixedState:NO];
  if ( [sender state] ) 
    [self setAnnoyingWindowPrefsTo:YES];
  else
    [self setAnnoyingWindowPrefsTo:NO];
}

//
// setAnnoyingWindowPrefsTo:
//
// Set all the prefs that allow webpages to muck with the status bar and window position
// (ie, be really annoying) to the given value
//
-(void) setAnnoyingWindowPrefsTo:(BOOL)inValue
{
    [self setPref:"dom.disable_window_move_resize" toBoolean:inValue];
    [self setPref:"dom.disable_window_status_change" toBoolean:inValue];
    [self setPref:"dom.disable_window_flip" toBoolean:inValue];
}

- (int)annoyingWindowPrefs
{
  BOOL disableStatusChangePref = [self getBooleanPref:"dom.disable_window_status_change" withSuccess:NULL];
  BOOL disableMoveResizePref = [self getBooleanPref:"dom.disable_window_move_resize" withSuccess:NULL];
  BOOL disableWindowFlipPref = [self getBooleanPref:"dom.disable_window_flip" withSuccess:NULL];

  if(disableStatusChangePref && disableMoveResizePref && disableWindowFlipPref)
    return kAnnoyancePrefAll;
  if(!disableStatusChangePref && !disableMoveResizePref && !disableWindowFlipPref)
    return kAnnoyancePrefNone;

  return kAnnoyancePrefSome;
}

- (int)preventAnimationCheckboxState
{
  NSString* preventAnimation = [self getStringPref:"image.animation_mode" withSuccess:NULL];
  if ([preventAnimation isEqualToString:@"once"])
    return NSOnState;
  else if ([preventAnimation isEqualToString:@"normal"])
    return NSOffState;
  else
    return NSMixedState;
}

//
// -profilePath
//
// Returns the path for our post 0.8 profiles stored in Application Support/Camino.
// Copied from the pref manager which we can't use w/out compiling it into our bundle.
//
- (NSString*) profilePath
{
  nsCOMPtr<nsIFile> appSupportDir;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILES_ROOT_DIR,
                                       getter_AddRefs(appSupportDir));
  if (NS_FAILED(rv))
    return nil;
  nsCAutoString nativePath;
  rv = appSupportDir->GetNativePath(nativePath);
  if (NS_FAILED(rv))
    return nil;
  return [NSString stringWithUTF8String:nativePath.get()];
}

@end
