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
 * The Original Code is bug 595560 test.
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

let newTabOne;
let originalTab;

function test() {
  waitForExplicitFinish();

  originalTab = gBrowser.visibleTabs[0];
  newTabOne = gBrowser.addTab("http://mochi.test:8888/");

  let browser = gBrowser.getBrowserForTab(newTabOne);
  let onLoad = function() {
    browser.removeEventListener("load", onLoad, true);
    
    // show the tab view
    window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
    TabView.toggle();
  }
  browser.addEventListener("load", onLoad, true);
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);
  ok(TabView.isVisible(), "Tab View is visible");

  let contentWindow = document.getElementById("tab-view").contentWindow;
  testOne(contentWindow);
}

function testOne(contentWindow) {
  onSearchEnabledAndDisabled(contentWindow, function() {
    testTwo(contentWindow); 
  });
  // execute a find command (i.e. press cmd/ctrl F)
  document.getElementById("cmd_find").doCommand();
}

function testTwo(contentWindow) {
  onSearchEnabledAndDisabled(contentWindow, function() { 
    testThree(contentWindow);
  });
  // press /
  EventUtils.synthesizeKey("VK_SLASH", { type: "keydown" }, contentWindow);
}

function testThree(contentWindow) {
  let groupItem = createEmptyGroupItem(contentWindow, 200);

  let onTabViewHidden = function() {
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);
    TabView.toggle();
  };
  let onTabViewShown = function() {
    window.removeEventListener("tabviewshown", onTabViewShown, false);

    is(contentWindow.UI.getActiveTab(), groupItem.getChild(0), 
       "The active tab is newly created tab item");

    let onSearchEnabled = function() {
      contentWindow.removeEventListener(
        "tabviewsearchenabled", onSearchEnabled, false);

      let searchBox = contentWindow.iQ("#searchbox");

      ok(contentWindow.document.hasFocus() && 
         contentWindow.document.activeElement == searchBox[0], 
         "The search box has focus");
      searchBox.val(newTabOne._tabViewTabItem.nameEl.innerHTML);

      contentWindow.performSearch();

      let checkSelectedTab = function() {
        window.removeEventListener("tabviewhidden", checkSelectedTab, false);
        is(newTabOne, gBrowser.selectedTab, "The search result tab is shown");
        cleanUpAndFinish(groupItem.getChild(0), contentWindow);
      };
      window.addEventListener("tabviewhidden", checkSelectedTab, false);

      // use the tabview menu (the same as pressing cmd/ctrl + e)
      document.getElementById("menu_tabview").doCommand();
   };
   contentWindow.addEventListener("tabviewsearchenabled", onSearchEnabled, false);
   EventUtils.synthesizeKey("VK_SLASH", { type: "keydown" }, contentWindow);
  };
  window.addEventListener("tabviewhidden", onTabViewHidden, false);
  window.addEventListener("tabviewshown", onTabViewShown, false);
  
  // click on the + button
  let newTabButton = groupItem.container.getElementsByClassName("newTabButton");
  ok(newTabButton[0], "New tab button exists");

  EventUtils.sendMouseEvent({ type: "click" }, newTabButton[0], contentWindow);
}

function onSearchEnabledAndDisabled(contentWindow, callback) {
  let onSearchEnabled = function() {
    contentWindow.removeEventListener(
      "tabviewsearchenabled", onSearchEnabled, false);
    contentWindow.addEventListener("tabviewsearchdisabled", onSearchDisabled, false);
    contentWindow.hideSearch();
  }
  let onSearchDisabled = function() {
    contentWindow.removeEventListener(
      "tabviewsearchdisabled", onSearchDisabled, false);
    callback();
  }
  contentWindow.addEventListener("tabviewsearchenabled", onSearchEnabled, false);
}

function cleanUpAndFinish(tabItem, contentWindow) {
  gBrowser.selectedTab = originalTab;
  gBrowser.removeTab(newTabOne);
  gBrowser.removeTab(tabItem.tab);
  
  finish();
}

function createEmptyGroupItem(contentWindow, padding) {
  let pageBounds = contentWindow.Items.getPageBounds();
  pageBounds.inset(padding, padding);

  let box = new contentWindow.Rect(pageBounds);
  box.width = 300;
  box.height = 300;

  let emptyGroupItem = new contentWindow.GroupItem([], { bounds: box });

  return emptyGroupItem;
}

