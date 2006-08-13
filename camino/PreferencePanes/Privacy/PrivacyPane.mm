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
 
#import "PrivacyPane.h"

#import "NSString+Utils.h"
#import "NSArray+Utils.h"

#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsIPref.h"
#include "nsNetCID.h"
#include "nsICookie.h"
#include "nsICookieManager.h"
#include "nsICookiePermission.h"
#include "nsIPermissionManager.h"
#include "nsISimpleEnumerator.h"
#include "nsIPermission.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "STFPopUpButtonCell.h"

// we should really get this from "CHBrowserService.h",
// but that requires linkage and extra search paths.
static NSString* XPCOMShutDownNotificationName = @"XPCOMShutDown";

#import "ExtendedTableView.h"

// prefs for keychain password autofill
static const char* const gUseKeychainPref = "chimera.store_passwords_with_keychain";
static const char* const gAutoFillEnabledPref = "chimera.keychain_passwords_autofill";

// network.cookie.lifetimePolicy settings
const int kAcceptCookiesNormally = 0;
const int kWarnAboutCookies = 1;

// sort order indicators
const int kSortReverse = 1;

//
// category on NSTableView for 10.2+ that reveals private api points to
// get the sort indicators. These are named images on 10.3 but we can use
// these as a good fallback
//
@interface NSTableView(Undocumented)
+ (NSImage*)_defaultTableHeaderSortImage;
+ (NSImage*)_defaultTableHeaderReverseSortImage;
@end

@interface NSTableView(Extensions)
+ (NSImage*)indicatorImage:(BOOL)inSortAscending;
@end

@implementation NSTableView(Extensions)

//
// +indicatorImage:
//
// Tries two different methods to get the sort indicator image. First it tries
// the named image, which is only available on 10.3+. If that fails, it tries a
// private api available on 10.2+. If that fails, setting the indicator to a nil
// image is still fine, it just clears it.
//
+ (NSImage*)indicatorImage:(BOOL)inSortAscending
{
  NSImage* image = nil;
  if (inSortAscending) {
    image = [NSImage imageNamed:@"NSAscendingSortIndicator"];
    if (!image && [NSTableView respondsToSelector:@selector(_defaultTableHeaderSortImage)])
      image = [NSTableView _defaultTableHeaderSortImage];
  }
  else {
    image = [NSImage imageNamed:@"NSDescendingSortIndicator"];
    if (!image && [NSTableView respondsToSelector:@selector(_defaultTableHeaderReverseSortImage)])
      image = [NSTableView _defaultTableHeaderReverseSortImage];
  }
  return image;
}

@end

#pragma mark -

@interface OrgMozillaChimeraPreferencePrivacy(Private)

- (void)addPermission:(int)inPermission forHost:(NSString*)inHost;

- (int)numCookiesSelectedInCookiePanel;
- (int)numPermissionsSelectedInPermissionsPanel;
// get the number of unique cookie sites that are selected,
// and if it's just one, return the site name (with leading period removed, if necessary)
- (int)numUniqueCookieSitesSelected:(NSString**)outSiteName;
- (NSString*)permissionsBlockingNameForCookieHostname:(NSString*)inHostname;
- (NSArray*)selectedCookieSites;

@end

#pragma mark -

// callbacks for sorting the permission list
PR_STATIC_CALLBACK(int) indexForCapability(int aCapability)
{
  switch (aCapability)
  {
    case nsIPermissionManager::DENY_ACTION:
      return eDenyIndex;
	
    case nsICookiePermission::ACCESS_SESSION:
      return eSessionOnlyIndex;
    
    case nsIPermissionManager::ALLOW_ACTION:
    default:
      return eAllowIndex;
  }
}

PR_STATIC_CALLBACK(int) comparePermHosts(nsIPermission* aPerm1, nsIPermission* aPerm2, void* aData)
{
  nsCAutoString host1;
  aPerm1->GetHost(host1);
  nsCAutoString host2;
  aPerm2->GetHost(host2);
  
  if ((int)aData == kSortReverse)
    return Compare(host2, host1);
  else
    return Compare(host1, host2);
}

PR_STATIC_CALLBACK(int) compareCapabilities(nsIPermission* aPerm1, nsIPermission* aPerm2, void* aData)
{
  PRUint32 cap1 = 0;
  aPerm1->GetCapability(&cap1);
  PRUint32 cap2 = 0;
  aPerm2->GetCapability(&cap2);
  
  if(cap1 == cap2)
    return comparePermHosts(aPerm1, aPerm2, nsnull); //always break ties in alphabetical order
  if ((int)aData == kSortReverse)
    return (indexForCapability(cap2) < indexForCapability(cap1)) ? -1 : 1;
  else 
    return (indexForCapability(cap1) < indexForCapability(cap2)) ? -1 : 1;
}

PR_STATIC_CALLBACK(int) compareCookieHosts(nsICookie* aCookie1, nsICookie* aCookie2, void* aData)
{
  nsCAutoString host1;
  aCookie1->GetHost(host1);
  nsCAutoString host2;
  aCookie2->GetHost(host2);
  if ((int)aData == kSortReverse)
    return Compare(host2, host1);
  else
    return Compare(host1, host2);
}

PR_STATIC_CALLBACK(int) compareNames(nsICookie* aCookie1, nsICookie* aCookie2, void* aData)
{
  nsCAutoString name1;
  aCookie1->GetName(name1);
  nsCAutoString name2;
  aCookie2->GetName(name2);
  
  if ((int)aData == kSortReverse)
    return Compare(name2, name1);
  else
    return Compare(name1, name2);
}

PR_STATIC_CALLBACK(int) comparePaths(nsICookie* aCookie1, nsICookie* aCookie2, void* aData)
{
  nsCAutoString path1;
  aCookie1->GetPath(path1);
  nsCAutoString path2;
  aCookie2->GetPath(path2);
  
  if ((int)aData == kSortReverse)
    return Compare(path1, path2);
  else
    return Compare(path2, path1);
}

PR_STATIC_CALLBACK(int) compareSecures(nsICookie* aCookie1, nsICookie* aCookie2, void* aData)
{
  PRBool secure1;
  aCookie1->GetIsSecure(&secure1);
  PRBool secure2;
  aCookie2->GetIsSecure(&secure2);
  
  if (secure1 == secure2)
    return 0;
  if ((int)aData == kSortReverse)
    return (secure2) ? -1 : 1;
  else
    return (secure1) ? -1 : 1;
}

PR_STATIC_CALLBACK(int) compareExpires(nsICookie* aCookie1, nsICookie* aCookie2, void* aData)
{
  PRUint64 expires1;
  aCookie1->GetExpires(&expires1);
  PRUint64 expires2;
  aCookie2->GetExpires(&expires2);
  
  if (expires1 == expires2) return 0;
  if ((int)aData == kSortReverse)
    return (expires2 < expires1) ? -1 : 1;
  else
    return (expires1 < expires2) ? -1 : 1;
}

PR_STATIC_CALLBACK(int) compareValues(nsICookie* aCookie1, nsICookie* aCookie2, void* aData)
{
  nsCAutoString value1;
  aCookie1->GetValue(value1);
  nsCAutoString value2;
  aCookie2->GetValue(value2);
  if ((int)aData == kSortReverse)
    return Compare(value2, value1);
  else
    return Compare(value1, value2);
}

#pragma mark -

@implementation OrgMozillaChimeraPreferencePrivacy

-(void) dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  // NOTE: no need to worry about mCachedPermissions or mCachedCookies because if we're going away
  // the respective sheets have closed and cleaned up.
  
  NS_IF_RELEASE(mPermissionManager);
  NS_IF_RELEASE(mCookieManager);
  [super dealloc];
}

- (void)xpcomShutdown:(NSNotification*)notification
{
  // these null the pointer
  NS_IF_RELEASE(mPermissionManager);
  NS_IF_RELEASE(mCookieManager);
}

-(void) mainViewDidLoad
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

  // Hookup cookie prefs.
  PRInt32 acceptCookies = eAcceptAllCookies;
  mPrefService->GetIntPref("network.cookie.cookieBehavior", &acceptCookies);
  [self mapCookiePrefToGUI:acceptCookies];
  // lifetimePolicy now controls asking about cookies, despite being totally unintuitive
  PRInt32 lifetimePolicy = kAcceptCookiesNormally;
  mPrefService->GetIntPref("network.cookie.lifetimePolicy", &lifetimePolicy);
  if (lifetimePolicy == kWarnAboutCookies)
    [mAskAboutCookies setState:YES];
  
  // store permission manager service and cache the enumerator.
  nsCOMPtr<nsIPermissionManager> pm(do_GetService(NS_PERMISSIONMANAGER_CONTRACTID));
  mPermissionManager = pm.get();
  NS_IF_ADDREF(mPermissionManager);

  // store cookie manager service
  nsCOMPtr<nsICookieManager> cm(do_GetService(NS_COOKIEMANAGER_CONTRACTID));
  mCookieManager = cm.get();
  NS_IF_ADDREF(mCookieManager);

  // Keychain checkboxes
  PRBool storePasswords = PR_TRUE;
  mPrefService->GetBoolPref(gUseKeychainPref, &storePasswords);
  [mStorePasswords setState:(storePasswords ? NSOnState : NSOffState)];

  PRBool autoFillPasswords = PR_TRUE;
  mPrefService->GetBoolPref(gAutoFillEnabledPref, &autoFillPasswords);
  if(storePasswords)
    [mAutoFillPasswords setState:(autoFillPasswords ? NSOnState : NSOffState)];
  else {
    [mAutoFillPasswords setState:NSOffState];
    [mAutoFillPasswords setEnabled:NO];
  }

  // set up policy popups
  NSPopUpButtonCell *popupButtonCell = [mPermissionColumn dataCell];
  [popupButtonCell setEditable:YES];
  [popupButtonCell addItemsWithTitles:[NSArray arrayWithObjects:[self getLocalizedString:@"Allow"],
                                                                [self getLocalizedString:@"Allow for Session"],
																[self getLocalizedString:@"Deny"],
																nil]];
  
  //remove the popup from the filter input fields
  [[mPermissionFilterField cell] setHasPopUpButton: NO];
  [[mCookiesFilterField cell] setHasPopUpButton: NO];
}

-(void) mapCookiePrefToGUI:(int)pref
{
  [mCookieBehavior selectCellWithTag:pref];
  [mAskAboutCookies setEnabled:(pref == eAcceptAllCookies || pref == eAcceptCookiesFromOriginatingServer)];
}

//
// Stored cookie editing methods
//

-(void) populateCookieCache
{
  nsCOMPtr<nsISimpleEnumerator> cookieEnum;
  if (mCookieManager)
    mCookieManager->GetEnumerator(getter_AddRefs(cookieEnum));
  
  mCachedCookies = new nsCOMArray<nsICookie>;
  if (mCachedCookies && cookieEnum) {
    mCachedCookies->Clear();
    PRBool hasMoreElements = PR_FALSE;
    cookieEnum->HasMoreElements(&hasMoreElements);
    while (hasMoreElements) {
      nsCOMPtr<nsICookie> cookie;
      cookieEnum->GetNext(getter_AddRefs(cookie));
      mCachedCookies->AppendObject(cookie);
      cookieEnum->HasMoreElements(&hasMoreElements);
    }
  }
}

-(IBAction) editCookies:(id)aSender
{
  // build parallel cookie list
  [self populateCookieCache];
  
  [mCookiesTable setDeleteAction:@selector(removeCookies:)];
  [mCookiesTable setTarget:self];

  // start sorted by host
  mCachedCookies->Sort(compareCookieHosts, nsnull);
  NSTableColumn* sortedColumn = [mCookiesTable tableColumnWithIdentifier:@"Website"];
  [mCookiesTable setHighlightedTableColumn:sortedColumn];
  if ([mCookiesTable respondsToSelector:@selector(setIndicatorImage:inTableColumn:)])
    [mCookiesTable setIndicatorImage:[NSTableView indicatorImage:YES] inTableColumn:sortedColumn];
  mSortedAscending = YES;
  
  // ensure a row is selected (cocoa doesn't do this for us, but will keep
  // us from unselecting a row once one is set; go figure).
  [mCookiesTable selectRow: 0 byExtendingSelection: NO];
  
  // use alternating row colors on 10.3+
  if ([mCookiesTable respondsToSelector:@selector(setUsesAlternatingRowBackgroundColors:)]) {
    [mCookiesTable setUsesAlternatingRowBackgroundColors:YES];
    NSArray* columns = [mCookiesTable tableColumns];
    if (columns) {
      int numColumns = [columns count];
      for (int i = 0; i < numColumns; ++i)
        [[[columns objectAtIndex:i] dataCell] setDrawsBackground:NO];
    }
  }
  
  //clear the filter field
  [mCookiesFilterField setStringValue: @""];

  // we shouldn't need to do this, but the scrollbar won't enable unless we
  // force the table to reload its data. Oddly it gets the number of rows correct,
  // it just forgets to tell the scrollbar. *shrug*
  [mCookiesTable reloadData];
  
  [mCookiesPanel setFrameAutosaveName:@"cookies_sheet"];
  
  // bring up sheet
  [NSApp beginSheet:mCookiesPanel
     modalForWindow:[mAskAboutCookies window]   // any old window accessor
      modalDelegate:self
     didEndSelector:NULL
        contextInfo:NULL];
  NSSize min = {440, 240};
  [mCookiesPanel setMinSize:min];
}

-(IBAction) removeCookies:(id)aSender
{
  int rowToSelect = -1;

  if (mCachedCookies && mCookieManager) {
    NSArray *rows = [[mCookiesTable selectedRowEnumerator] allObjects];
    NSEnumerator *e = [rows reverseObjectEnumerator];
    NSNumber *index;
    while ((index = [e nextObject]))
    {
      int row = [index intValue];
      if (rowToSelect == -1)
        rowToSelect = row;
      else
        --rowToSelect;

      nsCAutoString host, name, path;
      mCachedCookies->ObjectAt(row)->GetHost(host);
      mCachedCookies->ObjectAt(row)->GetName(name);
      mCachedCookies->ObjectAt(row)->GetPath(path);
      mCookieManager->Remove(host, name, path, PR_FALSE);  // don't block permanently
      mCachedCookies->RemoveObjectAt(row);
    }
  }
  
  [mCookiesTable reloadData];

  if (rowToSelect >=0 && rowToSelect < [mCookiesTable numberOfRows])
    [mCookiesTable selectRow:rowToSelect byExtendingSelection:NO];
  else
    [mCookiesTable deselectAll:self];
}

-(IBAction) removeAllCookies: (id)aSender
{
  if (NSRunCriticalAlertPanel([self getLocalizedString:@"RemoveAllCookiesWarningTitle"],
                              [self getLocalizedString:@"RemoveAllCookiesWarning"],
                              [self getLocalizedString:@"Remove All Cookies"],
                              [self getLocalizedString:@"CancelButtonText"],
                              nil) == NSAlertDefaultReturn)
  {
    if (mCookieManager) {
      // remove all cookies from cookie manager
      mCookieManager->RemoveAll();
      // create new cookie cache
      delete mCachedCookies;
      mCachedCookies = new nsCOMArray<nsICookie>;
    }
    [mCookiesTable reloadData];
  }
}

-(IBAction) blockCookiesFromSites:(id)aSender
{
  if (mCachedCookies && mPermissionManager) {
    NSArray *rows = [[mCookiesTable selectedRowEnumerator] allObjects];
    NSEnumerator *e = [rows reverseObjectEnumerator];
    NSNumber *index;
    while ((index = [e nextObject]))
    {
      int row = [index intValue];

      nsCAutoString host, name, path;
      mCachedCookies->ObjectAt(row)->GetHost(host);
      [self addPermission:nsIPermissionManager::DENY_ACTION forHost:[NSString stringWith_nsACString:host]];
    }
  }
}

-(IBAction) removeCookiesAndBlockSites:(id)aSender
{
  if (mCachedCookies && mPermissionManager)
  {
    // first fetch the list of sites
    NSArray* selectedSites = [self selectedCookieSites];
    int rowToSelect = -1;
    
    // remove the cookies
    for (int row = 0; row < mCachedCookies->Count(); row++)
    {
      nsCAutoString host, name, path;
      // only search on the host
      mCachedCookies->ObjectAt(row)->GetHost(host);
      NSString* cookieHost = [NSString stringWith_nsACString:host];
      if ([selectedSites containsObject:cookieHost])
      {
        if (rowToSelect == -1)
          rowToSelect = row;

        mCachedCookies->ObjectAt(row)->GetName(name);
        mCachedCookies->ObjectAt(row)->GetPath(path);
        mCookieManager->Remove(host, name, path, PR_FALSE);  // don't block permanently
        mCachedCookies->RemoveObjectAt(row);
        --row;    // to account for removal
      }
    }
    
    // and block the sites
    NSEnumerator* sitesEnum = [selectedSites objectEnumerator];
    NSString* curSite;
    while ((curSite = [sitesEnum nextObject]))
      [self addPermission:nsIPermissionManager::DENY_ACTION forHost:curSite];

    // and reload data
    [mCookiesTable reloadData];

    if (rowToSelect >=0 && rowToSelect < [mCookiesTable numberOfRows])
      [mCookiesTable selectRow:rowToSelect byExtendingSelection:NO];
    else
      [mCookiesTable deselectAll:self];
  }
}

-(IBAction) editCookiesDone:(id)aSender
{
  // save stuff
  [mCookiesPanel orderOut:self];
  [NSApp endSheet:mCookiesPanel];

  delete mCachedCookies;
  mCachedCookies = nsnull;
}

//
// Site permission editing methods

-(void) populatePermissionCache
{
  nsCOMPtr<nsISimpleEnumerator> permEnum;
  if (mPermissionManager) 
    mPermissionManager->GetEnumerator(getter_AddRefs(permEnum));
  
  mCachedPermissions = new nsCOMArray<nsIPermission>;
  if (mCachedPermissions && permEnum) {
    mCachedPermissions->Clear();
    PRBool hasMoreElements = PR_FALSE;
    permEnum->HasMoreElements(&hasMoreElements);
    while (hasMoreElements) {
      nsCOMPtr<nsISupports> curr;
      permEnum->GetNext(getter_AddRefs(curr));
      nsCOMPtr<nsIPermission> currPerm(do_QueryInterface(curr));
      if (currPerm) {
        nsCAutoString type;
        currPerm->GetType(type);
        if (type.Equals(NS_LITERAL_CSTRING("cookie")))
          mCachedPermissions->AppendObject(currPerm);
      }
      permEnum->HasMoreElements(&hasMoreElements);
    }
  }
}

-(IBAction) editPermissions:(id)aSender
{
  // build parallel permission list for speed with a lot of blocked sites
  [self populatePermissionCache];

  [mPermissionsTable setDeleteAction:@selector(removeCookiePermissions:)];
  [mPermissionsTable setTarget:self];
  
  // start sorted by host
  mCachedPermissions->Sort(comparePermHosts, nsnull);
  NSTableColumn* sortedColumn = [mPermissionsTable tableColumnWithIdentifier:@"Website"];
  [mPermissionsTable setHighlightedTableColumn:sortedColumn];
  if ([mPermissionsTable respondsToSelector:@selector(setIndicatorImage:inTableColumn:)])
    [mPermissionsTable setIndicatorImage:[NSTableView indicatorImage:YES] inTableColumn:sortedColumn];
  mSortedAscending = YES;
  
  // ensure a row is selected (cocoa doesn't do this for us, but will keep
  // us from unselecting a row once one is set; go figure).
  [mPermissionsTable selectRow:0 byExtendingSelection:NO];
  
  // use alternating row colors on 10.3+
  if ([mPermissionsTable respondsToSelector:@selector(setUsesAlternatingRowBackgroundColors:)])
    [mPermissionsTable setUsesAlternatingRowBackgroundColors:YES];
  
  //clear the filter field
  [mPermissionFilterField setStringValue: @""];
  
  // we shouldn't need to do this, but the scrollbar won't enable unless we
  // force the table to reload its data. Oddly it gets the number of rows correct,
  // it just forgets to tell the scrollbar. *shrug*
  [mPermissionsTable reloadData];
  
  [mPermissionsPanel setFrameAutosaveName:@"permissions_sheet"];
  
  // bring up sheet
  [NSApp beginSheet:mPermissionsPanel
     modalForWindow:[mAskAboutCookies window]   // any old window accessor
      modalDelegate:self
     didEndSelector:NULL
        contextInfo:NULL];
  NSSize min = {440, 240};
  [mPermissionsPanel setMinSize:min];
}

-(IBAction) removeCookiePermissions:(id)aSender
{
  int rowToSelect = -1;
  
  if (mCachedPermissions && mPermissionManager) {
    // remove from parallel array and cookie permissions list
    NSArray *rows = [[mPermissionsTable selectedRowEnumerator] allObjects];
    NSEnumerator *e = [rows reverseObjectEnumerator];
    NSNumber *index;
    while ((index = [e nextObject]))  {
      int row = [index intValue];
      if (rowToSelect == -1)
        rowToSelect = row;
      else
        --rowToSelect;
      nsCAutoString host;
      mCachedPermissions->ObjectAt(row)->GetHost(host);
      mPermissionManager->Remove(host, "cookie");
      mCachedPermissions->RemoveObjectAt(row);
    }
  } 

  [mPermissionsTable reloadData];

  if (rowToSelect >=0 && rowToSelect < [mPermissionsTable numberOfRows])
    [mPermissionsTable selectRow:rowToSelect byExtendingSelection:NO];
  else
    [mPermissionsTable deselectAll: self];   // don't want any traces of previous selection
}

-(IBAction) removeAllCookiePermissions: (id)aSender
{
  if (NSRunCriticalAlertPanel([self getLocalizedString:@"RemoveAllCookiePermissionsWarningTitle"],
                              [self getLocalizedString:@"RemoveAllCookiePermissionsWarning"],
                              [self getLocalizedString:@"Remove All Exceptions"],
                              [self getLocalizedString:@"CancelButtonText"],
                              nil) == NSAlertDefaultReturn)
  {
    if (mPermissionManager)
    {
      // since the permissions manager stores not just cookie permissions,
      // but also images etc, we have to manually remove just the cookie
      // ones. Ugh.
      
      // we have to keep a list of permissions to remove, because it's
      // not safe to remove while enumerating
      nsCOMArray<nsIPermission> permissionsToRemove;
      
      nsCOMPtr<nsISimpleEnumerator> permEnum;
      mPermissionManager->GetEnumerator(getter_AddRefs(permEnum));
      if (permEnum)
      {
        PRBool hasMoreElements = PR_FALSE;
        while (NS_SUCCEEDED(permEnum->HasMoreElements(&hasMoreElements)) && hasMoreElements)
        {
          nsCOMPtr<nsISupports> curr;
          permEnum->GetNext(getter_AddRefs(curr));
          nsCOMPtr<nsIPermission> currPerm(do_QueryInterface(curr));
          if (currPerm)
          {
            nsCAutoString type;
            currPerm->GetType(type);
            if (type.Equals(NS_LITERAL_CSTRING("cookie")))
              permissionsToRemove.AppendObject(currPerm);
          }
        }
      }

      // now do the removal
      int numDoomed = permissionsToRemove.Count();
      for (int i = 0; i < numDoomed; ++i)
      {
        nsCAutoString curHost, curType;
        permissionsToRemove.ObjectAt(i)->GetHost(curHost);
        permissionsToRemove.ObjectAt(i)->GetType(curType);
        mPermissionManager->Remove(curHost, curType.get());
      }

      delete mCachedPermissions;
      mCachedPermissions = new nsCOMArray<nsIPermission>;
    }
    [mPermissionsTable reloadData];
  }
}

-(IBAction) editPermissionsDone:(id)aSender
{
  // save stuff
  [mPermissionsPanel orderOut:self];
  [NSApp endSheet:mPermissionsPanel];
  
  delete mCachedPermissions;
  mCachedPermissions = nsnull;
}

-(int) getRowForPermissionWithHost:(NSString *)aHost
{
  nsCAutoString host;
  if (mCachedPermissions) {
    int numRows = mCachedPermissions->Count();
    for (int row = 0; row < numRows; ++row) {
      mCachedPermissions->ObjectAt(row)->GetHost(host);
      if ([[NSString stringWithUTF8String:host.get()] isEqualToString:aHost]) return row;
    }
  }
  return -1;
}

//
// NSTableDataSource protocol methods
//

-(int) numberOfRowsInTableView:(NSTableView *)aTableView
{
  PRUint32 numRows = 0;
  if (aTableView == mPermissionsTable) {
    if (mCachedPermissions)
      numRows = mCachedPermissions->Count();
  } else if (aTableView == mCookiesTable) {
    if (mCachedCookies)
      numRows = mCachedCookies->Count();
  }

  return (int) numRows;
}

-(id) tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
  NSString* retVal = nil;
  if (aTableView == mPermissionsTable) {
    if (mCachedPermissions) {
      if ([[aTableColumn identifier] isEqualToString:@"Website"]) {
        // website url column
        nsCAutoString host;
        mCachedPermissions->ObjectAt(rowIndex)->GetHost(host);
        retVal = [NSString stringWithUTF8String:host.get()];
      } else {
        // policy column
        PRUint32 capability = PR_FALSE;
        mCachedPermissions->ObjectAt(rowIndex)->GetCapability(&capability);
        return [NSNumber numberWithInt:indexForCapability(capability)];
      }
    }
  }
  else if (aTableView == mCookiesTable) {
    if (mCachedCookies) {
      nsCAutoString cookieVal;
      if ([[aTableColumn identifier] isEqualToString: @"Website"]) {
        mCachedCookies->ObjectAt(rowIndex)->GetHost(cookieVal);
      } else if ([[aTableColumn identifier] isEqualToString: @"Name"]) {
        mCachedCookies->ObjectAt(rowIndex)->GetName(cookieVal);
      } else if ([[aTableColumn identifier] isEqualToString: @"Path"]) {
        mCachedCookies->ObjectAt(rowIndex)->GetPath(cookieVal);
      } else if ([[aTableColumn identifier] isEqualToString: @"Secure"]) {
        PRBool secure = PR_FALSE;
        mCachedCookies->ObjectAt(rowIndex)->GetIsSecure(&secure);
        if (secure)
          retVal = [self getLocalizedString:@"yes"];
        else
          retVal = [self getLocalizedString:@"no"];
        return retVal;
      } else if ([[aTableColumn identifier] isEqualToString: @"Expires"]) {
        PRUint64 expires = 0;
        mCachedCookies->ObjectAt(rowIndex)->GetExpires(&expires);
        if (expires == 0) {
          // if expires is 0, it's a session cookie; display as expiring on the current date.
          // It's not perfect, but it's better than showing the epoch.
          NSDate *date = [NSDate date];
          return date;   // special case return
        } else {
          NSDate *date = [NSDate dateWithTimeIntervalSince1970: (NSTimeInterval)expires];
          return date;   // special case return
        }
      } else if ([[aTableColumn identifier] isEqualToString: @"Value"]) {
        mCachedCookies->ObjectAt(rowIndex)->GetValue(cookieVal);
      }
      retVal = [NSString stringWithCString: cookieVal.get()];
    }
  }
  
  return retVal;
}

// currently, this only applies to the site allow/deny, since that's the only editable column
-(void) tableView:(NSTableView *)aTableView
   setObjectValue:anObject
   forTableColumn:(NSTableColumn *)aTableColumn
              row:(int)rowIndex
{
  if (aTableView == mPermissionsTable && aTableColumn == mPermissionColumn) {
    if (mCachedPermissions && mPermissionManager) {
      // create a URI from the hostname of the changed site
      nsCAutoString host;
      mCachedPermissions->ObjectAt(rowIndex)->GetHost(host);
      NSString* url = [NSString stringWithFormat:@"http://%s", host.get()];
      const char* siteURL = [url UTF8String];
      nsCOMPtr<nsIURI> newURI;
      NS_NewURI(getter_AddRefs(newURI), siteURL);
      if (newURI) {
        // nsIPermissions are immutable, and there's no API to change the action,
        // so instead we have to delete the old pref and insert a new one.
        // remove the old entry
        mPermissionManager->Remove(host, "cookie"); // XXX necessary?
        // add a new entry with the new permission
        switch ([anObject intValue])
		{
		  case eAllowIndex:
            mPermissionManager->Add(newURI, "cookie", nsIPermissionManager::ALLOW_ACTION);
			break;

          case eDenyIndex:
            mPermissionManager->Add(newURI, "cookie", nsIPermissionManager::DENY_ACTION);
			break;

          case eSessionOnlyIndex:
            mPermissionManager->Add(newURI, "cookie", nsICookiePermission::ACCESS_SESSION);
			break;
        }
        // there really should be a better way to keep the cache up-to-date than rebuilding
        // it, but the nsIPermissionManager interface doesn't have a way to get a pointer
        // to a site's nsIPermission. It's this, use a custom class that duplicates the
        // information (wasting a lot of memory), or find a way to tie in to
        // PERM_CHANGE_NOTIFICATION to get the new nsIPermission that way.
        //re-filter
        [self filterCookiesPermissionsWithString:[mPermissionFilterField stringValue]];
        // re-aquire selection of the changed permission
        int selectedRowIndex = [self getRowForPermissionWithHost:[NSString stringWithUTF8String:host.get()]];
        nsCOMPtr<nsIPermission> selectedItem = (selectedRowIndex != -1) ?
          mCachedPermissions->ObjectAt(selectedRowIndex) : nsnull;
        // re-sort
        [self sortPermissionsByColumn:[mPermissionsTable highlightedTableColumn] inAscendingOrder:mSortedAscending];
        // scroll to the new position of changed item
        if (selectedItem) {
          int newRowIndex = mCachedPermissions->IndexOf(selectedItem);
          if (newRowIndex >= 0) {
            [aTableView selectRow:newRowIndex byExtendingSelection:NO];
            [aTableView scrollRowToVisible:newRowIndex];
          }
        }
      }
    }
  }
}

-(void) sortPermissionsByColumn:(NSTableColumn *)aTableColumn inAscendingOrder:(BOOL)ascending
{
  if(mCachedPermissions) {
    if ([[aTableColumn identifier] isEqualToString:@"Website"])
      mCachedPermissions->Sort(comparePermHosts, (ascending) ? nsnull : (void *)kSortReverse);
    else
      mCachedPermissions->Sort(compareCapabilities, (ascending) ? nsnull : (void *)kSortReverse);
    [mPermissionsTable setHighlightedTableColumn:aTableColumn];
    [mPermissionsTable reloadData];
  }
}

-(void) sortCookiesByColumn:(NSTableColumn *)aTableColumn inAscendingOrder:(BOOL)ascending
{
  if(mCachedCookies) {
    if ([[aTableColumn identifier] isEqualToString:@"Website"])
      mCachedCookies->Sort(compareCookieHosts, (ascending) ? nsnull : (void *)kSortReverse);
    else if ([[aTableColumn identifier] isEqualToString:@"Name"])
      mCachedCookies->Sort(compareNames, (ascending) ? nsnull : (void *)kSortReverse);
    else if ([[aTableColumn identifier] isEqualToString:@"Path"])
      mCachedCookies->Sort(comparePaths, (ascending) ? nsnull : (void *)kSortReverse);
    else if ([[aTableColumn identifier] isEqualToString:@"Secure"])
      mCachedCookies->Sort(compareSecures, (ascending) ? nsnull : (void *)kSortReverse);
    else if ([[aTableColumn identifier] isEqualToString:@"Expires"])
      mCachedCookies->Sort(compareExpires, (ascending) ? nsnull : (void *)kSortReverse);
    else if ([[aTableColumn identifier] isEqualToString:@"Value"])
      mCachedCookies->Sort(compareValues, (ascending) ? nsnull : (void *)kSortReverse);
    [mCookiesTable setHighlightedTableColumn:aTableColumn];
    [mCookiesTable reloadData];
  }
}

// NSTableView delegate methods

- (void) tableView:(NSTableView *)aTableView didClickTableColumn:(NSTableColumn *)aTableColumn
{
  // reverse the sort if clicking again on the same column
  if (aTableColumn == [aTableView highlightedTableColumn])
    mSortedAscending = !mSortedAscending;
  else
    mSortedAscending = YES;

  // adjust sort indicator on new column, removing from old column
  if ([aTableView respondsToSelector:@selector(setIndicatorImage:inTableColumn:)]) {
    [aTableView setIndicatorImage:nil inTableColumn:[aTableView highlightedTableColumn]];
    [aTableView setIndicatorImage:[NSTableView indicatorImage:mSortedAscending] inTableColumn:aTableColumn];
  }
  
  if (aTableView == mPermissionsTable) {
    if (mCachedPermissions) {
      // save the currently selected rows, if any.
      nsCOMArray<nsIPermission> selectedItems;
      NSEnumerator *e = [mPermissionsTable selectedRowEnumerator];
      NSNumber *index;
      while ((index = [e nextObject])) {
        int row = [index intValue];
        selectedItems.AppendObject(mCachedPermissions->ObjectAt(row));
      }
      // sort the table data
      [self sortPermissionsByColumn:aTableColumn inAscendingOrder:mSortedAscending];
      // if any rows were selected before, find them again
      [mPermissionsTable deselectAll:self];
      for (int i = 0; i < selectedItems.Count(); ++i) {
        int newRowIndex = mCachedPermissions->IndexOf(selectedItems.ObjectAt(i));
        if (newRowIndex >= 0) {
          // scroll to the first item (arbitrary, but at least one should show)
          if (i == 0)
            [mPermissionsTable scrollRowToVisible:newRowIndex];
          [mPermissionsTable selectRow:newRowIndex byExtendingSelection:YES];
        }
      }
    }
  } else if (aTableView == mCookiesTable) {
    if (mCachedCookies) {
      // save the currently selected rows, if any.
      nsCOMArray<nsICookie> selectedItems;
      NSEnumerator *e = [mCookiesTable selectedRowEnumerator];
      NSNumber *index;
      while ((index = [e nextObject])) {
        int row = [index intValue];
        selectedItems.AppendObject(mCachedCookies->ObjectAt(row));
      }
      // sort the table data
      [self sortCookiesByColumn:aTableColumn inAscendingOrder:mSortedAscending];
      // if any rows were selected before, find them again
      [mCookiesTable deselectAll:self];
      for (int i = 0; i < selectedItems.Count(); ++i) {
        int newRowIndex = mCachedCookies->IndexOf(selectedItems.ObjectAt(i));
        if (newRowIndex >= 0) {
          // scroll to the first item (arbitrary, but at least one should show)
          if (i == 0)
            [mCookiesTable scrollRowToVisible:newRowIndex];
          [mCookiesTable selectRow:newRowIndex byExtendingSelection:YES];
        }
      }
    }
  }
}

- (void)tableViewSelectionDidChange:(NSNotification *)aNotification
{
  if ([aNotification object] == mCookiesTable)
  {
    // nothing to do
  }
}

//
// Buttons
//

-(IBAction) clickCookieBehavior:(id)sender
{
  int row = [mCookieBehavior selectedRow];
  [self setPref:"network.cookie.cookieBehavior" toInt:row];
  [self mapCookiePrefToGUI:row];
}

-(IBAction) clickAskAboutCookies:(id)sender
{
  [self setPref:"network.cookie.lifetimePolicy" toInt:([sender state] == NSOnState) ? kWarnAboutCookies : kAcceptCookiesNormally];
}


//
// clickStorePasswords
//
-(IBAction) clickStorePasswords:(id)sender
{
  if (!mPrefService)
    return;
  mPrefService->SetBoolPref("chimera.store_passwords_with_keychain",
                            ([mStorePasswords state] == NSOnState) ? PR_TRUE : PR_FALSE);

  if([mStorePasswords state] == NSOnState)
    [mAutoFillPasswords setState:([self getBooleanPref:"chimera.keychain_passwords_autofill" withSuccess:NULL] ? NSOnState : NSOffState)];
  else
    [mAutoFillPasswords setState:NSOffState];

  [mAutoFillPasswords setEnabled:([mStorePasswords state] == NSOnState)];
}

//
// clickAutoFillPasswords
//
// Set pref if autofill is enabled
//
-(IBAction) clickAutoFillPasswords:(id)sender
{
  if (!mPrefService)
    return;
  mPrefService->SetBoolPref("chimera.keychain_passwords_autofill",
                            ([mAutoFillPasswords state] == NSOnState) ? PR_TRUE : PR_FALSE);
}

-(IBAction) launchKeychainAccess:(id)sender
{
  FSRef fsRef;
  CFURLRef urlRef;
  OSErr err = ::LSGetApplicationForInfo('APPL', 'kcmr', NULL, kLSRolesAll, &fsRef, &urlRef);
  if (!err) {
    CFStringRef fileSystemURL = ::CFURLCopyFileSystemPath(urlRef, kCFURLPOSIXPathStyle);
    [[NSWorkspace sharedWorkspace] launchApplication:(NSString*)fileSystemURL];
    CFRelease(fileSystemURL);
  }
}

- (void)controlTextDidChange:(NSNotification *)aNotification
{
  NSString *filterString = [[aNotification object] stringValue];
  
  // find out if we are filtering the permission or the cookies
  if (([aNotification object] == mPermissionFilterField) && mCachedPermissions && mPermissionManager) {
    // the user wants to filter down the list of cookies. Reinitialize the list of permission in case
    // they deleted or replaced a letter.
    [self filterCookiesPermissionsWithString:filterString];
    // re-sort
    [self sortPermissionsByColumn:[mPermissionsTable highlightedTableColumn] inAscendingOrder:mSortedAscending];
      
    [mPermissionsTable deselectAll: self];   // don't want any traces of previous selection
    [mPermissionsTable reloadData];
  } 
  else if (([aNotification object] == mCookiesFilterField) && mCachedCookies && mCookieManager) {
    // reinitialize the list of cookies in case user deleted a letter or replaced a letter
    [self filterCookiesWithString:filterString];
    // re-sort
    [self sortCookiesByColumn:[mCookiesTable highlightedTableColumn] inAscendingOrder:mSortedAscending];
    
    [mCookiesTable deselectAll: self];   // don't want any traces of previous selection
    [mCookiesTable reloadData];
  }
}

- (void) filterCookiesPermissionsWithString: (NSString*) inFilterString
{
  // the user wants to filter down the list of cookies. Reinitialize the list of permission in case
  // they deleted or replaced a letter.
  [self populatePermissionCache];
  
  if ([inFilterString length]) {
    NSMutableArray *indexToRemove = [NSMutableArray array];
    for (int row = 0; row < mCachedPermissions->Count(); row++) {
      nsCAutoString host;
      mCachedPermissions->ObjectAt(row)->GetHost(host);
      if ([[NSString stringWithUTF8String:host.get()] rangeOfString:inFilterString].location == NSNotFound)
        // add to our buffer
        [indexToRemove addObject:[NSNumber numberWithInt:row]];
    }
    
    //remove the items at the saved indexes
    //
    NSEnumerator *theEnum = [indexToRemove reverseObjectEnumerator];
    NSNumber *currentItem;
    while (currentItem = [theEnum nextObject])
      mCachedPermissions->RemoveObjectAt([currentItem intValue]);
  }
}


- (void) filterCookiesWithString: (NSString*) inFilterString
{
  // reinitialize the list of cookies in case user deleted a letter or replaced a letter
  [self populateCookieCache];
  
  if ([inFilterString length]) {
    NSMutableArray *indexToRemove = [NSMutableArray array];
    for (int row = 0; row < mCachedCookies->Count(); row++) {
      nsCAutoString host;
      // only search on the host
      mCachedCookies->ObjectAt(row)->GetHost(host);
      if ([[NSString stringWithUTF8String:host.get()] rangeOfString:inFilterString].location == NSNotFound)
        [indexToRemove addObject:[NSNumber numberWithInt:row]];
    }
    
    //remove the items at the saved indexes
    //
    NSEnumerator *theEnum = [indexToRemove reverseObjectEnumerator];
    NSNumber *currentItem;
    while (currentItem = [theEnum nextObject])
      mCachedCookies->RemoveObjectAt([currentItem intValue]);
  }
}

- (void)addPermission:(int)inPermission forHost:(NSString*)inHost
{
  nsCOMPtr<nsIURI> hostURI;
  NS_NewURI(getter_AddRefs(hostURI), "http://");
  nsCAutoString host([[self permissionsBlockingNameForCookieHostname:inHost] UTF8String]);
  hostURI->SetHost(host);
  
  mPermissionManager->Add(hostURI, "cookie", inPermission);
}

- (int)numCookiesSelectedInCookiePanel
{
  int numSelected = 0;
  if ([mCookiesPanel isVisible])
    numSelected = [mCookiesTable numberOfSelectedRows];

  return numSelected;
}

- (int)numPermissionsSelectedInPermissionsPanel
{
  int numSelected = 0;
  if ([mPermissionsPanel isVisible])
    numSelected = [mPermissionsTable numberOfSelectedRows];

  return numSelected;
}

- (int)numUniqueCookieSitesSelected:(NSString**)outSiteName
{
  if (outSiteName)
    *outSiteName = nil;

  NSArray* selectedSites = [self selectedCookieSites];
  unsigned int numHosts = [selectedSites count];
  if (numHosts == 1 && outSiteName)
    *outSiteName = [self permissionsBlockingNameForCookieHostname:[selectedSites firstObject]];

  return numHosts;
}

- (NSString*)permissionsBlockingNameForCookieHostname:(NSString*)inHostname
{
  // if the host string starts with a '.', remove it (because this is
  // how the permissions manager looks up hosts)
  if ([inHostname hasPrefix:@"."])
    inHostname = [inHostname substringFromIndex:1];
  return inHostname;
}

- (NSArray*)selectedCookieSites
{
  // the set does the uniquifying for us
  NSMutableSet* selectedHostsSet = [[[NSMutableSet alloc] init] autorelease];
  NSEnumerator* e = [mCookiesTable selectedRowEnumerator];
  NSNumber* index;
  while ((index = [e nextObject]))
  {
    int row = [index intValue];

    nsCAutoString host;
    mCachedCookies->ObjectAt(row)->GetHost(host);

    NSString* hostString = [NSString stringWith_nsACString:host];
    [selectedHostsSet addObject:hostString];
  }
  return [selectedHostsSet allObjects];
}

- (BOOL)validateMenuItem:(NSMenuItem*)inMenuItem
{
  SEL action = [inMenuItem action];
  
  // cookies context menu
  if (action == @selector(removeCookies:))
    return ([self numCookiesSelectedInCookiePanel] > 0);

  // only allow "remove all" if we're not filtering
  if (action == @selector(removeAllCookies:))
    return ([[mCookiesFilterField stringValue] length] == 0);

  if (action == @selector(blockCookiesFromSites:))
  {
    NSString* siteName = nil;
    int numCookieSites = [self numUniqueCookieSitesSelected:&siteName];
    NSString* menuTitle = (numCookieSites == 1) ?
                            [NSString stringWithFormat:[self getLocalizedString:@"BlockCookieFromSite"], siteName] :
                            [self getLocalizedString:@"BlockCookiesFromSites"];
    [inMenuItem setTitle:menuTitle];
    return (numCookieSites > 0);
  }

  if (action == @selector(removeCookiesAndBlockSites:))
  {
    NSString* siteName = nil;
    int numCookieSites = [self numUniqueCookieSitesSelected:&siteName];

    NSString* menuTitle = (numCookieSites == 1) ?
                            [NSString stringWithFormat:[self getLocalizedString:@"RemoveAndBlockCookieFromSite"], siteName] :
                            [self getLocalizedString:@"RemoveAndBlockCookiesFromSites"];
    [inMenuItem setTitle:menuTitle];
    return (numCookieSites > 0);
  }

  // permissions context menu
  if (action == @selector(removeCookiePermissions:))
    return ([self numPermissionsSelectedInPermissionsPanel] > 0);

  // only allow "remove all" if we're not filtering
  if (action == @selector(removeAllCookiePermissions:))
    return ([[mPermissionFilterField stringValue] length] == 0);

  return YES;
}

@end
