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
 * The Original Code is bug 591706 test.
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
  waitForExplicitFinish();

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  if (TabView.isVisible())
    onTabViewWindowLoaded();
  else
    TabView.show();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  ok(TabView.isVisible(), "Tab View is visible");

  let contentWindow = document.getElementById("tab-view").contentWindow;
  let [originalTab] = gBrowser.visibleTabs;

  // Create a first tab and orphan it
  let firstTab = gBrowser.loadOneTab("about:blank#1", {inBackground: true});
  let firstTabItem = firstTab.tabItem;
  let currentGroup = contentWindow.GroupItems.getActiveGroupItem();
  ok(currentGroup.getChildren().some(function(child) child == firstTabItem),"The first tab was made in the current group");
  contentWindow.GroupItems.getActiveGroupItem().remove(firstTabItem);
  ok(!currentGroup.getChildren().some(function(child) child == firstTabItem),"The first tab was orphaned");

  // Create a group and make it active
  let box = new contentWindow.Rect(10, 10, 300, 300);
  let group = new contentWindow.GroupItem([], { bounds: box });
  ok(group.isEmpty(), "This group is empty");
  contentWindow.GroupItems.setActiveGroupItem(group);
  
  // Create a second tab in this new group
  let secondTab = gBrowser.loadOneTab("about:blank#2", {inBackground: true});
  let secondTabItem = secondTab.tabItem;
  ok(group.getChildren().some(function(child) child == secondTabItem),"The second tab was made in our new group");
  is(group.getChildren().length, 1, "Only one tab in the first group");
  isnot(firstTab.linkedBrowser.contentWindow.location, secondTab.linkedBrowser.contentWindow.location, "The two tabs must have different locations");

  // Add the first tab to the group *programmatically*, without specifying a dropPos
  group.add(firstTabItem);
  is(group.getChildren().length, 2, "Two tabs in the group");
  is(group.getChildren()[0].tab.linkedBrowser.contentWindow.location, secondTab.linkedBrowser.contentWindow.location, "The second tab was there first");
  is(group.getChildren()[1].tab.linkedBrowser.contentWindow.location, firstTab.linkedBrowser.contentWindow.location, "The first tab was just added and went to the end of the line");
  
  group.addSubscriber(group, "close", function() {
    group.removeSubscriber(group, "close");

    ok(group.isEmpty(), "The group is empty again");

    is(contentWindow.GroupItems.getActiveGroupItem(), null, "The active group is gone");
    contentWindow.GroupItems.setActiveGroupItem(currentGroup);
    isnot(contentWindow.GroupItems.getActiveGroupItem(), null, "There is an active group");
    is(gBrowser.tabs.length, 1, "There is only one tab left");
    is(gBrowser.visibleTabs.length, 1, "There is also only one visible tab");

    let onTabViewHidden = function() {
      window.removeEventListener("tabviewhidden", onTabViewHidden, false);
      finish();
    };
    window.addEventListener("tabviewhidden", onTabViewHidden, false);
    gBrowser.selectedTab = originalTab;

    TabView.hide();
  });

  // Get rid of the group and its children
  group.closeAll();
  // close undo group
  let closeButton = group.$undoContainer.find(".close");
  EventUtils.sendMouseEvent(
    { type: "click" }, closeButton[0], contentWindow);
}

function simulateDragDrop(srcElement, offsetX, offsetY, contentWindow) {
  // enter drag mode
  let dataTransfer;

  EventUtils.synthesizeMouse(
    srcElement, 1, 1, { type: "mousedown" }, contentWindow);
  event = contentWindow.document.createEvent("DragEvents");
  event.initDragEvent(
    "dragenter", true, true, contentWindow, 0, 0, 0, 0, 0,
    false, false, false, false, 1, null, dataTransfer);
  srcElement.dispatchEvent(event);

  // drag over
  for (let i = 4; i >= 0; i--)
    EventUtils.synthesizeMouse(
      srcElement,  Math.round(offsetX/5),  Math.round(offsetY/4),
      { type: "mousemove" }, contentWindow);
  event = contentWindow.document.createEvent("DragEvents");
  event.initDragEvent(
    "dragover", true, true, contentWindow, 0, 0, 0, 0, 0,
    false, false, false, false, 0, null, dataTransfer);
  srcElement.dispatchEvent(event);

  // drop
  EventUtils.synthesizeMouse(srcElement, 0, 0, { type: "mouseup" }, contentWindow);
  event = contentWindow.document.createEvent("DragEvents");
  event.initDragEvent(
    "drop", true, true, contentWindow, 0, 0, 0, 0, 0,
    false, false, false, false, 0, null, dataTransfer);
  srcElement.dispatchEvent(event);
}
