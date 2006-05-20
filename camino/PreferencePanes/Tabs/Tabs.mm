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
 *   joshmoz@gmail.com (Josh Aas)
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

#import "Tabs.h"
#import "nsIBrowserDOMWindow.h"

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
  [mTabBarVisiblity setState:[self getBooleanPref:"camino.tab_bar_always_visible" withSuccess:&gotPref]];
  [mSingleWindowMode setState:([self getIntPref:"browser.link.open_newwindow" withSuccess:&gotPref] == nsIBrowserDOMWindow::OPEN_NEWTAB)];
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
  else if (sender == mTabBarVisiblity) {
    [self setPref:"camino.tab_bar_always_visible" toBoolean:[sender state]];
  }
  else if (sender == mSingleWindowMode) {
    int newState = [sender state] ? nsIBrowserDOMWindow::OPEN_NEWTAB : nsIBrowserDOMWindow::OPEN_DEFAULTWINDOW;
    [self setPref:"browser.link.open_newwindow" toInt:newState];
  }
}

@end
