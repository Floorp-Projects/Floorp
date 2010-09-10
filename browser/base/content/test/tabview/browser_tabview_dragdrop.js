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
 * The Original Code is tabview drag and drop test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Raymond Lee <raymond@appcoast.com>
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
  waitForExplicitFinish();

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.toggle();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  ok(TabView.isVisible(), "Tab View is visible");

  let contentWindow = document.getElementById("tab-view").contentWindow;
  let [originalTab] = gBrowser.visibleTabs;

  // create group one and two
  let boxOne = new contentWindow.Rect(20, 20, 300, 300);
  let groupOne = new contentWindow.GroupItem([], { bounds: boxOne });
  ok(groupOne.isEmpty(), "This group is empty");

  let boxTwo = new contentWindow.Rect(20, 400, 300, 300);
  let groupTwo = new contentWindow.GroupItem([], { bounds: boxTwo });

  groupOne.addSubscriber(groupOne, "childAdded", function() {
    groupOne.removeSubscriber(groupOne, "childAdded");
    groupTwo.newTab();
  });

  let count = 0;
  let onTabViewShown = function() {
    if (count == 2) {
      window.removeEventListener("tabviewshown", onTabViewShown, false);
      addTest(contentWindow, groupOne.id, groupTwo.id, originalTab);
    }
  };
  let onTabViewHidden = function() {
    TabView.toggle();
    if (++count == 2)
      window.removeEventListener("tabviewhidden", onTabViewHidden, false);
  };
  window.addEventListener("tabviewshown", onTabViewShown, false);
  window.addEventListener("tabviewhidden", onTabViewHidden, false);

  // open tab in group
  groupOne.newTab();
}

function addTest(contentWindow, groupOneId, groupTwoId, originalTab) {
  let groupOne = contentWindow.GroupItems.groupItem(groupOneId);
  let groupTwo = contentWindow.GroupItems.groupItem(groupTwoId);
  let groupOneTabItemCount = groupOne.getChildren().length;
  let groupTwoTabItemCount = groupTwo.getChildren().length;
  is(groupOneTabItemCount, 1, "GroupItem one has one tab");
  is(groupTwoTabItemCount, 1, "GroupItem two has one tab as well");

  let tabItem = groupOne.getChild(0);
  ok(tabItem, "The tab item exists");

  // calculate the offsets
  let groupTwoRect = groupTwo.getBounds();
  let groupTwoRectCenter = groupTwoRect.center();
  let tabItemRect = tabItem.getBounds();
  let tabItemRectCenter = tabItemRect.center();
  let offsetX =
    Math.round(groupTwoRectCenter.x - tabItemRectCenter.x);
  let offsetY =
    Math.round(groupTwoRectCenter.y - tabItemRectCenter.y);

  function endGame() {
    groupTwo.removeSubscriber(groupTwo, "childAdded");

    is(groupOne.getChildren().length, --groupOneTabItemCount,
       "The number of children in group one is decreased by 1");
    is(groupTwo.getChildren().length, ++groupTwoTabItemCount,
       "The number of children in group two is increased by 1");
  
    let onTabViewHidden = function() {
      window.removeEventListener("tabviewhidden", onTabViewHidden, false);
      groupTwo.closeAll();
      // close undo group
      let closeButton = groupTwo.$undoContainer.find(".close");
      EventUtils.sendMouseEvent(
        { type: "click" }, closeButton[0], contentWindow);
    };
    groupTwo.addSubscriber(groupTwo, "close", function() {
      groupTwo.removeSubscriber(groupTwo, "close");
      finish();  
    });
    window.addEventListener("tabviewhidden", onTabViewHidden, false);
    gBrowser.selectedTab = originalTab;
  }
  groupTwo.addSubscriber(groupTwo, "childAdded", endGame);
  
  simulateDragDrop(tabItem.container, offsetX, offsetY, contentWindow);
}

function simulateDragDrop(tabItem, offsetX, offsetY, contentWindow) {
  // enter drag mode
  let dataTransfer;

  EventUtils.synthesizeMouse(
    tabItem, 1, 1, { type: "mousedown" }, contentWindow);
  event = contentWindow.document.createEvent("DragEvents");
  event.initDragEvent(
    "dragenter", true, true, contentWindow, 0, 0, 0, 0, 0,
    false, false, false, false, 1, null, dataTransfer);
  tabItem.dispatchEvent(event);

  // drag over
  if (offsetX || offsetY) {
    let Ci = Components.interfaces;
    let utils = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor).
                              getInterface(Ci.nsIDOMWindowUtils);
    let rect = tabItem.getBoundingClientRect();
    for (let i = 1; i <= 5; i++) {
      let left = rect.left + Math.round(i * offsetX / 5);
      let top = rect.top + Math.round(i * offsetY / 5);
      utils.sendMouseEvent("mousemove", left, top, 0, 1, 0);
    }
    event = contentWindow.document.createEvent("DragEvents");
    event.initDragEvent(
      "dragover", true, true, contentWindow, 0, 0, 0, 0, 0,
      false, false, false, false, 0, null, dataTransfer);
    tabItem.dispatchEvent(event);
  }
  
  // drop
  EventUtils.synthesizeMouse(tabItem, 0, 0, { type: "mouseup" }, contentWindow);
  event = contentWindow.document.createEvent("DragEvents");
  event.initDragEvent(
    "drop", true, true, contentWindow, 0, 0, 0, 0, 0,
    false, false, false, false, 0, null, dataTransfer);
  tabItem.dispatchEvent(event);
}
