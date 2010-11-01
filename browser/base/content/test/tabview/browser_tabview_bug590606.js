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
 * The Original Code is bug 590606 test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Raymond Lee <raymond@appcoast.com>
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

let originalTab;
let newTabOne;

function test() {
  waitForExplicitFinish();

  originalTab = gBrowser.visibleTabs[0];
  // add a tab to the existing group.
  newTabOne = gBrowser.addTab();

  let onTabviewShown = function() {
    window.removeEventListener("tabviewshown", onTabviewShown, false);

    let contentWindow = document.getElementById("tab-view").contentWindow;

    is(contentWindow.GroupItems.groupItems.length, 1, 
       "There is one group item on startup");
    let groupItemOne = contentWindow.GroupItems.groupItems[0];
    is(groupItemOne.getChildren().length, 2, 
       "There should be two tab items in that group.");
    is(gBrowser.selectedTab, groupItemOne.getChild(0).tab,
       "The currently selected tab should be the first tab in the groupItemOne");

    // create another group with a tab.
    let groupItemTwo = createEmptyGroupItem(contentWindow, 300, 300, 200);

    let onTabViewHidden = function() {
      window.removeEventListener("tabviewhidden", onTabViewHidden, false);
      // start the test
      testGroupSwitch(contentWindow, groupItemOne, groupItemTwo);
    };
    window.addEventListener("tabviewhidden", onTabViewHidden, false);

    // click on the + button
    let newTabButton = groupItemTwo.container.getElementsByClassName("newTabButton");
    ok(newTabButton[0], "New tab button exists");
    EventUtils.sendMouseEvent({ type: "click" }, newTabButton[0], contentWindow);
  };
  window.addEventListener("tabviewshown", onTabviewShown, false);
  TabView.toggle();
}

function testGroupSwitch(contentWindow, groupItemOne, groupItemTwo) {
  is(gBrowser.selectedTab, groupItemTwo.getChild(0).tab,
     "The currently selected tab should be the only tab in the groupItemTwo");

  // switch to groupItemOne
  tabItem = contentWindow.GroupItems.getNextGroupItemTab(false);
  if (tabItem)
    gBrowser.selectedTab = tabItem.tab;
  is(gBrowser.selectedTab, groupItemOne.getChild(0).tab,
    "The currently selected tab should be the first tab in the groupItemOne");
  
  // switch to the second tab in groupItemOne
  gBrowser.selectedTab = groupItemOne.getChild(1).tab;

  // switch to groupItemTwo
  tabItem = contentWindow.GroupItems.getNextGroupItemTab(false);
  if (tabItem)
    gBrowser.selectedTab = tabItem.tab;
  is(gBrowser.selectedTab, groupItemTwo.getChild(0).tab,
    "The currently selected tab should be the only tab in the groupItemTwo");

  // switch to groupItemOne
  tabItem = contentWindow.GroupItems.getNextGroupItemTab(false);
  if (tabItem)
    gBrowser.selectedTab = tabItem.tab;
  is(gBrowser.selectedTab, groupItemOne.getChild(1).tab,
    "The currently selected tab should be the second tab in the groupItemOne");

  // cleanup.
  gBrowser.removeTab(groupItemTwo.getChild(0).tab);
  gBrowser.removeTab(newTabOne);

  finish();
}
