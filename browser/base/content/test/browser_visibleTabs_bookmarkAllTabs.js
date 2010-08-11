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
 * The Original Code is tabbrowser visibleTabs Bookmark All Tabs test.
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
  is(gBrowser.visibleTabs.length, 1, "1 tab should be open");  
  is(Disabled(), true, "Bookmark All Tabs should be hidden");

  // Add a tab
  let testTab = gBrowser.addTab();
  is(gBrowser.visibleTabs.length, 2, "2 tabs should be open");
  is(Disabled(), false, "Bookmark All Tabs should be available");

  // Hide the original tab
  gBrowser.selectedTab = testTab;
  gBrowser.showOnlyTheseTabs([testTab]);
  is(gBrowser.visibleTabs.length, 1, "1 tab should be visible");  
  is(Disabled(), true, "Bookmark All Tabs should be hidden as there is only one visible tab");
  
  // Add a tab that will get pinned
  let pinned = gBrowser.addTab();
  gBrowser.pinTab(pinned);
  is(gBrowser.visibleTabs.length, 2, "2 tabs should be visible now");
  is(Disabled(), false, "Bookmark All Tabs should be available as there are two visible tabs");

  // Show all tabs
  let allTabs = [tab for each (tab in gBrowser.tabs)];
  gBrowser.showOnlyTheseTabs(allTabs);

  // reset the environment  
  gBrowser.removeTab(testTab);
  gBrowser.removeTab(pinned);
  is(gBrowser.visibleTabs.length, 1, "only orig is left and visible");
  is(gBrowser.tabs.length, 1, "sanity check that it matches");
  is(Disabled(), true, "Bookmark All Tabs should be hidden");
  is(gBrowser.selectedTab, origTab, "got the orig tab");
  is(origTab.hidden, false, "and it's not hidden -- visible!");
}

function Disabled() {
  let command = document.getElementById("Browser:BookmarkAllTabs");
  return command.hasAttribute("disabled") && command.getAttribute("disabled") === "true";
}