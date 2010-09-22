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
 * The Original Code is tabview test for bug 587040.
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

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.toggle();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  ok(TabView.isVisible(), "Tab View is visible");

  let [originalTab] = gBrowser.visibleTabs;
  let contentWindow = document.getElementById("tab-view").contentWindow;

  // create a group
  let box = new contentWindow.Rect(20, 20, 300, 300);
  let groupItem = new contentWindow.GroupItem([], { bounds: box });

  let onTabViewShown = function() {
    window.removeEventListener("tabviewshown", onTabViewShown, false);

    is(groupItem.getChildren().length, 2, "The new group has two tab items");
    // start the test
    testSameTabItemAndClickGroup(contentWindow, groupItem);
  };

  // create a tab item in the new group
  let onTabViewHidden = function() {
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);

    ok(!TabView.isVisible(), "Tab View is hidden because we just opened a tab");

    let anotherTab = gBrowser.addTab("http://mochi.test:8888/");
    let browser = gBrowser.getBrowserForTab(anotherTab);
    let onLoad = function() {
      browser.removeEventListener("load", onLoad, true);
    
      window.addEventListener("tabviewshown", onTabViewShown, false);
      TabView.toggle();
    }
    browser.addEventListener("load", onLoad, true);
  };
  window.addEventListener("tabviewhidden", onTabViewHidden, false);

  // click on the + button to create a blank tab
  let newTabButton = groupItem.container.getElementsByClassName("newTabButton");
  ok(newTabButton[0], "New tab button exists");

  EventUtils.sendMouseEvent({ type: "click" }, newTabButton[0], contentWindow);
}

function testSameTabItemAndClickGroup(contentWindow, groupItem, originalTab) {
  let activeTabItem = groupItem.getActiveTab();
  ok(activeTabItem, "Has active item");

  let onTabViewHidden = function() {
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);

    is(activeTabItem.tab, gBrowser.selectedTab, "The correct tab is selected");
    TabView.toggle();
  };

  let onTabViewShown = function() {
    window.removeEventListener("tabviewshown", onTabViewShown, false);

    testDifferentTabItemAndClickGroup(contentWindow, groupItem, originalTab);
  };
  window.addEventListener("tabviewhidden", onTabViewHidden, false);
  window.addEventListener("tabviewshown", onTabViewShown, false);
  
  let groupElement = groupItem.container;
  EventUtils.sendMouseEvent({ type: "mousedown" }, groupElement, contentWindow);
  EventUtils.sendMouseEvent({ type: "mouseup" }, groupElement, contentWindow);
}

function testDifferentTabItemAndClickGroup(contentWindow, groupItem, originalTab) {
  let activeTabItem = groupItem.getActiveTab();
  ok(activeTabItem, "Has old active item");

  // press tab
  EventUtils.synthesizeKey("VK_TAB", {}, contentWindow);

  let newActiveTabItem = groupItem.getActiveTab();
  ok(newActiveTabItem, "Has new active item");

  isnot(newActiveTabItem, activeTabItem, "The old and new active items are different");

  let onTabViewHidden = function() {
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);

    is(newActiveTabItem.tab, gBrowser.selectedTab, "The correct tab is selected");
    
    // clean up and finish the test
    groupItem.addSubscriber(groupItem, "close", function() {
      groupItem.removeSubscriber(groupItem, "close");
      finish();  
    });
    gBrowser.selectedTab = originalTab;

    groupItem.closeAll();
    // close undo group
    let closeButton = groupItem.$undoContainer.find(".close");
    EventUtils.sendMouseEvent(
      { type: "click" }, closeButton[0], contentWindow);
  };
  window.addEventListener("tabviewhidden", onTabViewHidden, false);

  // press the group Item  
  let groupElement = groupItem.container;
  EventUtils.sendMouseEvent({ type: "mousedown" }, groupElement, contentWindow);
  EventUtils.sendMouseEvent({ type: "mouseup" }, groupElement, contentWindow);
}
