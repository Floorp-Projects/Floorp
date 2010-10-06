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
 * The Original Code is tabview test for orphaned tabs (bug 595893).
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
let newWin;

function test() {
  waitForExplicitFinish();

  newWin = 
    window.openDialog(getBrowserURL(), "_blank", "all,dialog=no", "about:blank");

  let onLoad = function() {
    newWin.removeEventListener("load", onLoad, false);

    tabOne = newWin.gBrowser.addTab();

    newWin.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
    newWin.TabView.toggle();
  }
  newWin.addEventListener("load", onLoad, false);
}

function onTabViewWindowLoaded() {
  newWin.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  ok(newWin.TabView.isVisible(), "Tab View is visible");

  let contentWindow = newWin.document.getElementById("tab-view").contentWindow;

  // 1) the tab should belong to a group, and no orphan tabs
  ok(tabOne.tabItem.parent, "Tab one belongs to a group");
  is(contentWindow.GroupItems.getOrphanedTabs().length, 0, "No orphaned tabs");

  // 2) create a group, add a blank tab 
  let groupItem = createEmptyGroupItem(contentWindow, 200);

  let onTabViewHidden = function() {
    newWin.removeEventListener("tabviewhidden", onTabViewHidden, false);

    // 3) the new group item should have one child and no orphan tab
    is(groupItem.getChildren().length, 1, "The group item has an item");
    is(contentWindow.GroupItems.getOrphanedTabs().length, 0, "No orphaned tabs");
    
    // 4) check existence of stored group data for both tabs before finishing
    let tabData = contentWindow.Storage.getTabData(tabOne);
    ok(tabData && contentWindow.TabItems.storageSanity(tabData) && tabData.groupID, 
       "Tab one has stored group data");

    let tabItem = groupItem.getChild(0);
    let tabData = contentWindow.Storage.getTabData(tabItem.tab);
    ok(tabData && contentWindow.TabItems.storageSanity(tabData) && tabData.groupID, 
       "Tab two has stored group data");

    // clean up and finish the test
    newWin.gBrowser.removeTab(tabOne);
    newWin.gBrowser.removeTab(tabItem.tab);
    whenWindowObservesOnce(newWin, "domwindowclosed", function() {
      finish();
    });
    newWin.close();
  };
  newWin.addEventListener("tabviewhidden", onTabViewHidden, false);

  // click on the + button
  let newTabButton = groupItem.container.getElementsByClassName("newTabButton");
  ok(newTabButton[0], "New tab button exists");

  EventUtils.sendMouseEvent({ type: "click" }, newTabButton[0], contentWindow);
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

function whenWindowObservesOnce(win, topic, callback) {
  let windowWatcher = 
    Cc["@mozilla.org/embedcomp/window-watcher;1"].getService(Ci.nsIWindowWatcher);
  function windowObserver(subject, topicName, aData) {
    if (win == subject.QueryInterface(Ci.nsIDOMWindow) && topic == topicName) {
      windowWatcher.unregisterNotification(windowObserver);
      callback();
    }
  }
  windowWatcher.registerNotification(windowObserver);
}
