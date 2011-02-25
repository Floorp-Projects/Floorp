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
 * The Original Code is bug 622872 test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Michael Yoshitaka Erlewine <mitcho@mitcho.com>
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
  newWindowWithTabView(part1);
}

// PART 1:
// 1. Create a new tab (called newTab)
// 2. Orphan it. Activate this orphan tab.
// 3. Zoom into it.
function part1(win) {
  ok(win.TabView.isVisible(), "Tab View is visible");

  let contentWindow = win.document.getElementById("tab-view").contentWindow;
  is(win.gBrowser.tabs.length, 1, "In the beginning, there was one tab.");
  let [originalTab] = win.gBrowser.visibleTabs;
  let originalGroup = contentWindow.GroupItems.getActiveGroupItem();
  ok(originalGroup.getChildren().some(function(child) {
    return child == originalTab._tabViewTabItem;
  }),"The current active group is the one with the original tab in it.");

  // Create a new tab and orphan it
  let newTab = win.gBrowser.loadOneTab("about:mozilla", {inBackground: true});

  let newTabItem = newTab._tabViewTabItem;
  ok(originalGroup.getChildren().some(function(child) child == newTabItem),"The new tab was made in the current group");
  contentWindow.GroupItems.getActiveGroupItem().remove(newTabItem);
  ok(!originalGroup.getChildren().some(function(child) child == newTabItem),"The new tab was orphaned");
  newTabItem.pushAway();
  // activate this tab item
  contentWindow.UI.setActiveTab(newTabItem);

  // PART 2: close this orphan tab (newTab)
  let part2 = function part2() {
    win.removeEventListener("tabviewhidden", part2, false);

    is(win.gBrowser.selectedTab, newTab, "We zoomed into that new tab.");
    ok(!win.TabView.isVisible(), "Tab View is hidden, because we're looking at the new tab");

    newTab.addEventListener("TabClose", function() {
      newTab.removeEventListener("TabClose", arguments.callee, false);

      win.addEventListener("tabviewshown", part3, false);
      executeSoon(function() { win.TabView.toggle(); });
    }, false);
    win.gBrowser.removeTab(newTab);
  }

  let secondNewTab;
  // PART 3: now back in Panorama, open a new tab via the "new tab" menu item (or equivalent)
  // We call this secondNewTab.
  let part3 = function part3() {
    win.removeEventListener("tabviewshown", part3, false);

    ok(win.TabView.isVisible(), "Tab View is visible.");

    is(win.gBrowser.tabs.length, 1, "Only one tab again.");
    is(win.gBrowser.tabs[0], originalTab, "That one tab is the original tab.");

    let groupItems = contentWindow.GroupItems.groupItems;
    is(groupItems.length, 1, "Only one group");

    ok(!contentWindow.GroupItems.getActiveOrphanTab(), "There is no active orphan tab.");
    ok(win.TabView.isVisible(), "Tab View is visible.");

    win.gBrowser.tabContainer.addEventListener("TabSelect", function() {
      win.gBrowser.tabContainer.removeEventListener("TabSelect", arguments.callee, false);
      executeSoon(part4);
    }, false);
    win.document.getElementById("cmd_newNavigatorTab").doCommand();
  }

  // PART 4: verify it opened in the original group, and go back into Panorama
  let part4 = function part4() {
    ok(!win.TabView.isVisible(), "Tab View is hidden");

    is(win.gBrowser.tabs.length, 2, "There are two tabs total now.");
    is(win.gBrowser.visibleTabs.length, 2, "We're looking at both of them.");

    let foundOriginalTab = false;
    // we can't use forEach because win.gBrowser.tabs is only array-like.
    for (let i = 0; i < win.gBrowser.tabs.length; i++) {
      let tab = win.gBrowser.tabs[i];
      if (tab === originalTab)
        foundOriginalTab = true;
      else
        secondNewTab = tab;
    }
    ok(foundOriginalTab, "One of those tabs is the original tab.");
    ok(secondNewTab, "We found another tab... this is secondNewTab");

    is(win.gBrowser.selectedTab, secondNewTab, "This second new tab is what we're looking at.");

    win.addEventListener("tabviewshown", part5, false);
    win.TabView.toggle();
  }

  // PART 5: make sure we only have one group with both tabs now, and finish.
  let part5 = function part5() {
    win.removeEventListener("tabviewshown", part5, false);

    is(win.gBrowser.tabs.length, 2, "There are of course still two tabs.");

    let groupItems = contentWindow.GroupItems.groupItems;
    is(groupItems.length, 1, "There is one group now");
    is(groupItems[0], originalGroup, "It's the original group.");
    is(originalGroup.getChildren().length, 2, "It has two children.");

    // finish!
    win.close();
    finish();
  }

  // Okay, all set up now. Let's get this party started!
  afterAllTabItemsUpdated(function() {
    win.addEventListener("tabviewhidden", part2, false);
    win.TabView.toggle();
  }, win);
}
