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

#import "Tabs.h"

@implementation OrgMozillaChimeraPreferenceTabs

- (id)initWithBundle:(NSBundle *)bundle
{
  self = [super initWithBundle:bundle];
  return self;
}

- (void)dealloc
{
  [super dealloc];
}

- (void)mainViewDidLoad
{
  if (!mPrefService)
    return;

  BOOL gotPref;
  
  [radioOpenTabsForCommand selectCellWithTag:[self getBooleanPref:"browser.tabs.opentabfor.middleclick" withSuccess:&gotPref]];
  [radioOpenForAE selectCellWithTag:[self getIntPref:"browser.reuse_window" withSuccess:&gotPref]];
  [checkboxLoadTabsInBackground setState:[self getBooleanPref:"browser.tabs.loadInBackground" withSuccess:&gotPref]];
}

- (IBAction)checkboxClicked:(id)sender
{
  if (!mPrefService)
    return;

  if (sender == radioOpenTabsForCommand) {
    [self setPref:"browser.tabs.opentabfor.middleclick" toBoolean:[[sender selectedCell] tag]];
  }
  else if (sender == radioOpenForAE) {
    [self setPref:"browser.reuse_window" toInt:[[sender selectedCell] tag]];
  }
  else if (sender == checkboxLoadTabsInBackground) {
    [self setPref:"browser.tabs.loadInBackground" toBoolean:[sender state]];
  }
}

@end
