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
 * The Original Code is tabbrowser visibleTabs context menu test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Michael Yoshitaka Erlewine <mitcho@mitcho.com>
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
  is(gBrowser.visibleTabs.length, 1, "there is one visible tab");
  let testTab = gBrowser.addTab();
  is(gBrowser.visibleTabs.length, 2, "there are now two visible tabs");

  // Check the context menu with two tabs
  updateTabContextMenu(origTab);
  is(document.getElementById("context_closeTab").disabled, false, "Close Tab is enabled");
  is(document.getElementById("context_reloadAllTabs").disabled, false, "Reload All Tabs is enabled");

  // Hide the original tab.
  gBrowser.selectedTab = testTab;
  gBrowser.showOnlyTheseTabs([testTab]);
  is(gBrowser.visibleTabs.length, 1, "now there is only one visible tab");
  
  // Check the context menu with one tab.
  updateTabContextMenu(testTab);
  is(document.getElementById("context_closeTab").disabled, false, "Close Tab is enabled when more than one tab exists");
  is(document.getElementById("context_reloadAllTabs").disabled, true, "Reload All Tabs is disabled");
  
  // Add a tab that will get pinned
  // So now there's one pinned tab, one visible unpinned tab, and one hidden tab
  let pinned = gBrowser.addTab();
  gBrowser.pinTab(pinned);
  is(gBrowser.visibleTabs.length, 2, "now there are two visible tabs");

  // Check the context menu on the unpinned visible tab
  updateTabContextMenu(testTab);
  is(document.getElementById("context_closeOtherTabs").disabled, true, "Close Other Tabs is disabled");

  // Show all tabs
  let allTabs = [tab for each (tab in gBrowser.tabs)];
  gBrowser.showOnlyTheseTabs(allTabs);

  // Check the context menu now
  updateTabContextMenu(testTab);
  is(document.getElementById("context_closeOtherTabs").disabled, false, "Close Other Tabs is enabled");
  
  gBrowser.removeTab(testTab);
  gBrowser.removeTab(pinned);
}
