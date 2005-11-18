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
 
#import <Cocoa/Cocoa.h>
#import <PreferencePanes/NSPreferencePane.h>

#import "PreferencePaneBase.h"

#import "nsCOMArray.h"

#import "ExtendedTableView.h"
#import "SearchTextField.h"

class nsIPref;
class nsIPermissionManager;
class nsIPermission;
class nsICookieManager;
class nsICookie;

@class ExtendedTableView;

@interface OrgMozillaChimeraPreferencePrivacy : PreferencePaneBase
{
  // pane
  IBOutlet NSMatrix*          mCookieBehavior;
  IBOutlet NSButton*          mAskAboutCookies;
  
  IBOutlet NSButton*          mStorePasswords;
  IBOutlet NSButton*          mAutoFillPasswords;

  BOOL                        mSortedAscending;   // sort direction for tables in sheets

  // permission sheet
  IBOutlet id                 mPermissionsPanel;
  IBOutlet ExtendedTableView* mPermissionsTable;
  IBOutlet NSTableColumn*     mPermissionColumn;
  IBOutlet SearchTextField*   mPermissionFilterField;
  nsIPermissionManager*       mPermissionManager;   // STRONG (should be nsCOMPtr)
  nsCOMArray<nsIPermission>*  mCachedPermissions;   // parallel list for speed, STRONG
      
  // cookie sheet
  IBOutlet id                 mCookiesPanel;
  IBOutlet ExtendedTableView* mCookiesTable;
  IBOutlet SearchTextField*   mCookiesFilterField;
  nsICookieManager*           mCookieManager;
  nsCOMArray<nsICookie>*      mCachedCookies;
}

// main panel button actions
-(IBAction) clickCookieBehavior:(id)aSender;
-(IBAction) clickAskAboutCookies:(id)sender;
-(IBAction) clickStorePasswords:(id)sender;
-(IBAction) clickAutoFillPasswords:(id)sender;
-(IBAction) launchKeychainAccess:(id)sender;

// cookie editing functions
-(void) populateCookieCache;
-(IBAction) editCookies:(id)aSender;
-(IBAction) editCookiesDone:(id)aSender;
-(IBAction) removeCookies:(id)aSender;
-(IBAction) removeAllCookies:(id)aSender;
-(IBAction) blockCookiesFromSites:(id)aSender;
-(IBAction) removeCookiesAndBlockSites:(id)aSender;

// permission editing functions
-(void) populatePermissionCache;
-(IBAction) editPermissions:(id)aSender;
-(IBAction) editPermissionsDone:(id)aSender;
-(IBAction) removeCookiePermissions:(id)aSender;
-(IBAction) removeAllCookiePermissions:(id)aSender;
-(int) getRowForPermissionWithHost:(NSString *)aHost;

-(void) mapCookiePrefToGUI:(int)pref;

// data source informal protocol (NSTableDataSource)
- (int)numberOfRowsInTableView:(NSTableView *)aTableView;
- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex;
- (void)tableView:(NSTableView *)aTableView setObjectValue:anObject forTableColumn:(NSTableColumn *)aTableColumn  row:(int)rowIndex;

// NSTableView delegate methods
- (void)tableView:(NSTableView *)aTableView didClickTableColumn:(NSTableColumn *)aTableColumn;

// sorting support methods
- (void)sortCookiesByColumn:(NSTableColumn *)aTableColumn inAscendingOrder:(BOOL)ascending;
- (void)sortPermissionsByColumn:(NSTableColumn *)aTableColumn inAscendingOrder:(BOOL)ascending;

  //filtering methods
- (void) filterCookiesPermissionsWithString: (NSString*) inFilterString;
- (void) filterCookiesWithString: (NSString*) inFilterString;
@end
