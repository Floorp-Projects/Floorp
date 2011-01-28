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
 * The Original Code is tabview bug 612470 test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Patrick Walton <pcwalton@mozilla.com>
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

// Tests that groups behave properly when closing all tabs but app tabs.

let appTab, contentWindow;
let originalGroup, originalGroupTab, newGroup, newGroupTab;

function test() {
  waitForExplicitFinish();

  appTab = gBrowser.selectedTab;
  gBrowser.pinTab(appTab);
  originalGroupTab = gBrowser.addTab();

  addEventListener("tabviewshown", createGroup, false);
  TabView.toggle();
}

function createGroup() {
  removeEventListener("tabviewshown", createGroup, false);

  contentWindow = document.getElementById("tab-view").contentWindow;
  is(contentWindow.GroupItems.groupItems.length, 1, "There's only one group");

  originalGroup = contentWindow.GroupItems.groupItems[0];

  // Create a new group.
  let box = new contentWindow.Rect(20, 400, 300, 300);
  newGroup = new contentWindow.GroupItem([], { bounds: box });

  contentWindow.GroupItems.setActiveGroupItem(newGroup);

  addEventListener("tabviewhidden", addTab, false);
  TabView.toggle();
}

function addTab() {
  removeEventListener("tabviewhidden", addTab, false);

  newGroupTab = gBrowser.addTab();
  is(newGroup.getChildren().length, 1, "One tab is in the new group");
  executeSoon(removeTab);
}

function removeTab() {
  is(gBrowser.visibleTabs.length, 2, "There are two tabs displayed");
  gBrowser.removeTab(newGroupTab);

  is(newGroup.getChildren().length, 0, "No tabs are in the new group");
  is(gBrowser.visibleTabs.length, 1, "There is one tab displayed");
  is(contentWindow.GroupItems.groupItems.length, 2,
     "There are two groups still");

  addEventListener("tabviewshown", checkForRemovedGroup, false);
  TabView.toggle();
}

function checkForRemovedGroup() {
  removeEventListener("tabviewshown", checkForRemovedGroup, false);

  is(contentWindow.GroupItems.groupItems.length, 1,
     "There is now only one group");

  addEventListener("tabviewhidden", finishTest, false);
  TabView.toggle();
}

function finishTest() {
  removeEventListener("tabviewhidden", finishTest, false);

  gBrowser.removeTab(originalGroupTab);
  gBrowser.unpinTab(appTab);
  finish();
}

