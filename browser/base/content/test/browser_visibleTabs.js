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
 * The Original Code is tabbrowser visibleTabs test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Edward Lee <edilee@mozilla.com>
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

function test() {
  // There should be one tab when we start the test
  let [origTab] = gBrowser.visibleTabs;

  // Add a tab that will get pinned
  let pinned = gBrowser.addTab();
  gBrowser.pinTab(pinned);

  let testTab = gBrowser.addTab();

  let visible = gBrowser.visibleTabs;
  is(visible.length, 3, "3 tabs should be open");
  is(visible[0], pinned, "the pinned tab is first");
  is(visible[1], origTab, "original tab is next");
  is(visible[2], testTab, "last created tab is last");

  // Only show the test tab (but also get pinned and selected)
  is(gBrowser.selectedTab, origTab, "sanity check that we're on the original tab");
  gBrowser.showOnlyTheseTabs([testTab]);
  is(gBrowser.visibleTabs.length, 3, "all 3 tabs are still visible");

  // Select the test tab and only show that (and pinned)
  gBrowser.selectedTab = testTab;
  gBrowser.showOnlyTheseTabs([testTab]);

  // if the tabview frame is initialized, we need to move the orignal tab to
  // another group; otherwise, selecting a tab would make all three tabs in 
  // the same group to display.
  let tabViewWindow = TabView.getContentWindow();
  if (tabViewWindow)
    tabViewWindow.GroupItems.moveTabToGroupItem(origTab, null);

  visible = gBrowser.visibleTabs;
  is(visible.length, 2, "2 tabs should be visible including the pinned");
  is(visible[0], pinned, "first is pinned");
  is(visible[1], testTab, "next is the test tab");
  is(gBrowser.tabs.length, 3, "3 tabs should still be open");

  gBrowser.selectTabAtIndex(0);
  is(gBrowser.selectedTab, pinned, "first tab is pinned");
  gBrowser.selectTabAtIndex(1);
  is(gBrowser.selectedTab, testTab, "second tab is the test tab");
  gBrowser.selectTabAtIndex(2);
  is(gBrowser.selectedTab, testTab, "no third tab, so no change");
  gBrowser.selectTabAtIndex(0);
  is(gBrowser.selectedTab, pinned, "switch back to the pinned");
  gBrowser.selectTabAtIndex(2);
  is(gBrowser.selectedTab, pinned, "no third tab, so no change");
  gBrowser.selectTabAtIndex(-1);
  is(gBrowser.selectedTab, testTab, "last tab is the test tab");

  gBrowser.tabContainer.advanceSelectedTab(1, true);
  is(gBrowser.selectedTab, pinned, "wrapped around the end to pinned");
  gBrowser.tabContainer.advanceSelectedTab(1, true);
  is(gBrowser.selectedTab, testTab, "next to test tab");
  gBrowser.tabContainer.advanceSelectedTab(1, true);
  is(gBrowser.selectedTab, pinned, "next to pinned again");

  gBrowser.tabContainer.advanceSelectedTab(-1, true);
  is(gBrowser.selectedTab, testTab, "going backwards to last tab");
  gBrowser.tabContainer.advanceSelectedTab(-1, true);
  is(gBrowser.selectedTab, pinned, "next to pinned");
  gBrowser.tabContainer.advanceSelectedTab(-1, true);
  is(gBrowser.selectedTab, testTab, "next to test tab again");

  // Try showing all tabs
  gBrowser.showOnlyTheseTabs(Array.slice(gBrowser.tabs));
  is(gBrowser.visibleTabs.length, 3, "all 3 tabs are visible again");

  // Select the pinned tab and show the testTab to make sure selection updates
  gBrowser.selectedTab = pinned;
  gBrowser.showOnlyTheseTabs([testTab]);
  is(gBrowser.tabs[1], origTab, "make sure origTab is in the middle");
  is(origTab.hidden, true, "make sure it's hidden");
  gBrowser.removeTab(pinned);
  is(gBrowser.selectedTab, testTab, "making sure origTab was skipped");
  is(gBrowser.visibleTabs.length, 1, "only testTab is there");

  // Only show one of the non-pinned tabs (but testTab is selected)
  gBrowser.showOnlyTheseTabs([origTab]);
  is(gBrowser.visibleTabs.length, 2, "got 2 tabs");

  // Now really only show one of the tabs
  gBrowser.showOnlyTheseTabs([testTab]);
  visible = gBrowser.visibleTabs;
  is(visible.length, 1, "only the original tab is visible");
  is(visible[0], testTab, "it's the original tab");
  is(gBrowser.tabs.length, 2, "still have 2 open tabs");

  // Close the last visible tab and make sure we still get a visible tab
  gBrowser.removeTab(testTab);
  is(gBrowser.visibleTabs.length, 1, "only orig is left and visible");
  is(gBrowser.tabs.length, 1, "sanity check that it matches");
  is(gBrowser.selectedTab, origTab, "got the orig tab");
  is(origTab.hidden, false, "and it's not hidden -- visible!");
}
