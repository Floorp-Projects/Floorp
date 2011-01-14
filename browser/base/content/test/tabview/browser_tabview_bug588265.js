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
 * The Original Code is bug 588265 test.
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

function test() {
  waitForExplicitFinish();

  window.addEventListener("tabviewshown", setup, false);
  TabView.toggle();
}

function setup() {
  window.removeEventListener("tabviewshown", setup, false);

  let contentWindow = document.getElementById("tab-view").contentWindow;
  is(contentWindow.GroupItems.groupItems.length, 1, "Has only one group");

  let groupItemOne = contentWindow.GroupItems.groupItems[0];
  // add a blank tab to group one.
  createNewTabItemInGroupItem(groupItemOne, contentWindow, function() { 
    is(groupItemOne.getChildren().length, 2, "Group one has 2 tab items");

    // create group two with a blank tab.
    let groupItemTwo = createEmptyGroupItem(contentWindow, 250, 250, 40, true);
    createNewTabItemInGroupItem(groupItemTwo, contentWindow, function() {
      // start the first test.
      testGroups(groupItemOne, groupItemTwo, contentWindow);
    });
  });
}

function createNewTabItemInGroupItem(groupItem, contentWindow, callback) {
  // click on the + button to create a blank tab in group item
  let newTabButton = groupItem.container.getElementsByClassName("newTabButton");
  ok(newTabButton[0], "New tab button exists");

  let onTabViewHidden = function() {
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);

    ok(!TabView.isVisible(), "Tab View is hidden because we just opened a tab");
    TabView.toggle();
  };
  let onTabViewShown = function() {
    window.removeEventListener("tabviewshown", onTabViewShown, false);

    ok(TabView.isVisible(), "Tab View is visible");
    callback();
  };
  window.addEventListener("tabviewhidden", onTabViewHidden, false);
  window.addEventListener("tabviewshown", onTabViewShown, false);
  EventUtils.sendMouseEvent({ type: "click" }, newTabButton[0], contentWindow);
}

function testGroups(groupItemOne, groupItemTwo, contentWindow) {
  // check active tab and group
  is(contentWindow.GroupItems.getActiveGroupItem(), groupItemTwo, 
     "The group two is the active group");
  is(contentWindow.UI.getActiveTab(), groupItemTwo.getChild(0), 
     "The first tab item in group two is active");
  
  let tabItem = groupItemOne.getChild(1);
  tabItem.addSubscriber(tabItem, "tabRemoved", function() {
    tabItem.removeSubscriber(tabItem, "tabRemoved");

    is(groupItemOne.getChildren().length, 1,
      "The num of childen in group one is 1");

    // check active group and active tab
    is(contentWindow.GroupItems.getActiveGroupItem(), groupItemOne, 
       "The group one is the active group");
    is(contentWindow.UI.getActiveTab(), groupItemOne.getChild(0), 
       "The first tab item in group one is active");

    let onTabViewHidden = function() {
      window.removeEventListener("tabviewhidden", onTabViewHidden, false);
      is(groupItemOne.getChildren().length, 2, 
         "The num of childen in group one is 2");

      // clean up and finish
      groupItemTwo.addSubscriber(groupItemTwo, "close", function() {
        groupItemTwo.removeSubscriber(groupItemTwo, "close");

        gBrowser.removeTab(groupItemOne.getChild(1).tab);
        is(contentWindow.GroupItems.groupItems.length, 1, "Has only one group");
        is(groupItemOne.getChildren().length, 1, 
           "The num of childen in group one is 1");
        is(gBrowser.tabs.length, 1, "Has only one tab");

        finish();
      });
      gBrowser.removeTab(groupItemTwo.getChild(0).tab);
    }
    window.addEventListener("tabviewhidden", onTabViewHidden, false);
    EventUtils.synthesizeKey("t", { accelKey: true });
  });
  // close a tab item in group one
  tabItem.close();
}
