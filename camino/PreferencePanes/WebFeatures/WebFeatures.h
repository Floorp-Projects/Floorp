/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
*
* The contents of this file are subject to the Mozilla Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS
* IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
* implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is the Mozilla browser.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation. Portions created by Netscape are
* Copyright (C) 2002 Netscape Communications Corporation. All
* Rights Reserved.
*
* Contributor(s):
*   william@dell.wisner.name (William Dell Wisner)
*   joshmoz@gmail.com (Josh Aas)
*/

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

  IBOutlet NSButton *mEnablePopupBlocking;
  IBOutlet NSButton *mImageResize;
  IBOutlet NSButton *mEditWhitelist;
  
  IBOutlet id mWhitelistPanel;
  IBOutlet ExtendedTableView*   mWhitelistTable;
  IBOutlet NSTextField*         mAddField;
  IBOutlet NSButton*            mAddButton;

  nsIPermissionManager* mManager;         // STRONG (should be nsCOMPtr)  
  nsISupportsArray* mCachedPermissions;		// parallel list of permissions for speed, STRONG (should be nsCOMPtr)
  
  IBOutlet NSButton* mEnableAnnoyanceBlocker;
}

-(IBAction) clickEnableJS:(id)sender;
-(IBAction) clickEnableJava:(id)sender;

-(IBAction) clickEnablePopupBlocking:(id)sender;
-(IBAction) clickEnableImageResizing:(id)sender;
-(IBAction) editWhitelist:(id)sender;

-(IBAction) clickEnableAnnoyanceBlocker:(id)sender;
-(void) setAnnoyingWindowPrefsTo:(BOOL)inValue;

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
