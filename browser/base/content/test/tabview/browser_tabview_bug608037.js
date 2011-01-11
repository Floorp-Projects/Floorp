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
 * The Original Code is a test for bug 608037.
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

let tabOne;
let tabTwo;

function test() {
  waitForExplicitFinish();

  tabOne = gBrowser.addTab("http://mochi.test:8888/");
  tabTwo = gBrowser.addTab("http://mochi.test:8888/browser/browser/base/content/test/tabview/dummy_page.html");

  // make sure our tabs are loaded so their titles are right
  let stillToLoad = 0;
  let onLoad = function(event) {
    event.target.removeEventListener("load", onLoad, true);

    stillToLoad--;
    if (!stillToLoad) {
      // show the tab view
      window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
      ok(!TabView.isVisible(), "Tab View is hidden");

      // make sure the tab one is selected because undoCloseTab() would remove
      // the selected tab if it's a blank tab.
      gBrowser.selectedTab = tabOne;

      TabView.toggle();
    }
  }

  let newTabs = [ tabOne, tabTwo ];
  newTabs.forEach(function(tab) {
    stillToLoad++; 
    tab.linkedBrowser.addEventListener("load", onLoad, true);
  });
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  let contentWindow = document.getElementById("tab-view").contentWindow;
  let groupItems = contentWindow.GroupItems.groupItems;
  is(groupItems.length, 1, "There is only one group");
  is(groupItems[0].getChildren().length, 3, "The group has three tab items");

  gBrowser.removeTab(tabTwo);
  ok(TabView.isVisible(), "Tab View is still visible after removing a tab");
  is(groupItems[0].getChildren().length, 2, "The group has two tab items");

  tabTwo = undoCloseTab(0);
  tabTwo._tabViewTabItem.addSubscriber(tabTwo, "reconnected", function() {
    tabTwo._tabViewTabItem.removeSubscriber(tabTwo, "reconnected");

    ok(TabView.isVisible(), "Tab View is still visible after restoring a tab");
    is(groupItems[0].getChildren().length, 3, "The group still has three tab items");
  
    // clean up and finish
    let endGame = function() {
      window.removeEventListener("tabviewhidden", endGame, false);
  
      gBrowser.removeTab(tabOne);
      gBrowser.removeTab(tabTwo);
      finish();
    }
    window.addEventListener("tabviewhidden", endGame, false);
    TabView.toggle();
  });
}
