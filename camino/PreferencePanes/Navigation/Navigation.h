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
*/

#import <PreferencePaneBase.h>

@interface OrgMozillaChimeraPreferenceNavigation : PreferencePaneBase
{
  // IBOutlet NSButton *buttonSystemPreferences;
	IBOutlet NSTextField *textFieldHomePage;
	IBOutlet NSTextField *textFieldSearchPage;
  
	IBOutlet NSButton *checkboxUseSystemHomePage;
  IBOutlet NSButton *checkboxNewTabBlank;
  IBOutlet NSButton *checkboxNewWindowBlank;
  
  IBOutlet id radioOpenTabsForCommand;
  IBOutlet id radioOpenForAE;
  IBOutlet NSButton *checkboxLoadTabsInBackground;
  
  IBOutlet NSTextField *textFieldHistoryDays;
  
  IBOutlet NSTextField* mDownloadFolder;
  IBOutlet NSButton* mEnableHelperApps;
}

- (IBAction)openSystemInternetPanel:(id)sender;
- (IBAction)checkboxClicked:(id)sender;
- (IBAction)checkboxUseSystemHomePageClicked:(id)sender;
- (IBAction)checkboxStartPageClicked:(id)sender;
- (IBAction)historyDaysModified:(id)sender;
- (IBAction)clearGlobalHistory:(id)sender;

- (NSString*)getSystemHomePage;
- (NSString*)getCurrentHomePage;

- (NSString*)getSystemSearchPage;
- (NSString*)getCurrentSearchPage;

-(IBAction) clearDiskCache:(id)aSender;

@end
