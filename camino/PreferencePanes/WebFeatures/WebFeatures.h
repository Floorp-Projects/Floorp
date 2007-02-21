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

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/NSPreferencePane.h>
#import "PreferencePaneBase.h"
#import "ExtendedTableView.h"

class nsIPref;
class nsIPermissionManager;
class nsISupportsArray;

@interface OrgMozillaChimeraPreferenceWebFeatures : PreferencePaneBase
{
  IBOutlet NSButton* mEnableJS;
  IBOutlet NSButton* mEnableJava;
  IBOutlet NSButton* mEnablePlugins;

  IBOutlet NSButton *mEnablePopupBlocking;
  IBOutlet NSButton *mEnableAdBlocking;
  IBOutlet NSButton *mImageResize;
  IBOutlet NSButton *mPreventAnimation;
  IBOutlet NSButton *mEditWhitelist;
  IBOutlet NSButton *mEnableFlashBlock;  

  IBOutlet id mWhitelistPanel;
  IBOutlet ExtendedTableView*   mWhitelistTable;
  IBOutlet NSTextField*         mAddField;
  IBOutlet NSButton*            mAddButton;

  nsIPermissionManager* mManager;         // STRONG (should be nsCOMPtr)  
  nsISupportsArray* mCachedPermissions;		// parallel list of permissions for speed, STRONG (should be nsCOMPtr)

  IBOutlet NSButton* mEnableAnnoyanceBlocker;

  IBOutlet NSButton* mTabToFormElements;
  IBOutlet NSButton* mTabToLinks;
}

-(IBAction) clickEnableJS:(id)sender;
-(IBAction) clickEnableJava:(id)sender;
-(IBAction) clickEnablePlugins:(id)sender;

-(IBAction) clickEnablePopupBlocking:(id)sender;
-(IBAction) clickEnableAdBlocking:(id)sender;
-(IBAction) clickEnableImageResizing:(id)sender;
-(IBAction) clickPreventAnimation:(id)sender;
-(IBAction) editWhitelist:(id)sender;
-(IBAction) clickEnableFlashBlock:(id)sender;

-(IBAction) clickEnableAnnoyanceBlocker:(id)sender;
-(void) setAnnoyingWindowPrefsTo:(BOOL)inValue;

-(IBAction) clickTabFocusCheckboxes:(id)sender;

// whitelist sheet methods
-(IBAction) editWhitelistDone:(id)aSender;
-(IBAction) removeWhitelistSite:(id)aSender;
-(IBAction) addWhitelistSite:(id)sender;
-(void) editWhitelistSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo;
-(void) populatePermissionCache:(nsISupportsArray*)inPermissions;

// data source informal protocol (NSTableDataSource)
- (int)numberOfRowsInTableView:(NSTableView *)aTableView;
- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex;

@end
