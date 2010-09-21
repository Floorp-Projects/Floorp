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
 * The Original Code is tabview undo group test.
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

  let contentWindow = document.getElementById("tab-view").contentWindow;

  // create a group item
  let box = new contentWindow.Rect(20, 400, 300, 300);
  let groupItem = new contentWindow.GroupItem([], { bounds: box });

  // create a tab item in the new group
  let onTabViewHidden = function() {
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);

    ok(!TabView.isVisible(), "Tab View is hidden because we just opened a tab");
    // show tab view
    TabView.toggle();
  };
  let onTabViewShown = function() {
    window.removeEventListener("tabviewshown", onTabViewShown, false);

    is(groupItem.getChildren().length, 1, "The new group has a tab item");
    // start the tests
    testUndoGroup(contentWindow, groupItem);
  };
  window.addEventListener("tabviewhidden", onTabViewHidden, false);
  window.addEventListener("tabviewshown", onTabViewShown, false);

  // click on the + button
  let newTabButton = groupItem.container.getElementsByClassName("newTabButton");
  ok(newTabButton[0], "New tab button exists");

  EventUtils.sendMouseEvent({ type: "click" }, newTabButton[0], contentWindow);
}

function testUndoGroup(contentWindow, groupItem) {
  groupItem.addSubscriber(groupItem, "groupHidden", function() {
    groupItem.removeSubscriber(groupItem, "groupHidden");

    // check the data of the group
    let theGroupItem = contentWindow.GroupItems.groupItem(groupItem.id);
    ok(theGroupItem, "The group item still exists");
    is(theGroupItem.getChildren().length, 1, 
       "The tab item in the group still exists");

    // check the visibility of the group element and undo element
    is(theGroupItem.container.style.display, "none", 
       "The group element is hidden");
    ok(theGroupItem.$undoContainer, "Undo container is avaliable");

    EventUtils.sendMouseEvent(
      { type: "click" }, theGroupItem.$undoContainer[0], contentWindow);
  });

  groupItem.addSubscriber(groupItem, "groupShown", function() {
    groupItem.removeSubscriber(groupItem, "groupShown");

    // check the data of the group
    let theGroupItem = contentWindow.GroupItems.groupItem(groupItem.id);
    ok(theGroupItem, "The group item still exists");
    is(theGroupItem.getChildren().length, 1, 
       "The tab item in the group still exists");

    // check the visibility of the group element and undo element
    is(theGroupItem.container.style.display, "", "The group element is visible");
    ok(!theGroupItem.$undoContainer, "Undo container is not avaliable");
    
    // start the next test
    testCloseUndoGroup(contentWindow, groupItem);
  });

  let closeButton = groupItem.container.getElementsByClassName("close");
  ok(closeButton, "Group item close button exists");
  EventUtils.sendMouseEvent({ type: "click" }, closeButton[0], contentWindow);
}

function testCloseUndoGroup(contentWindow, groupItem) {
  groupItem.addSubscriber(groupItem, "groupHidden", function() {
    groupItem.removeSubscriber(groupItem, "groupHidden");

    // check the data of the group
    let theGroupItem = contentWindow.GroupItems.groupItem(groupItem.id);
    ok(theGroupItem, "The group item still exists");
    is(theGroupItem.getChildren().length, 1, 
       "The tab item in the group still exists");

    // check the visibility of the group element and undo element
    is(theGroupItem.container.style.display, "none", 
       "The group element is hidden");
    ok(theGroupItem.$undoContainer, "Undo container is avaliable");

    // click on close
    let closeButton = theGroupItem.$undoContainer.find(".close");
    EventUtils.sendMouseEvent(
      { type: "click" }, closeButton[0], contentWindow);
  });

  groupItem.addSubscriber(groupItem, "close", function() {
    groupItem.removeSubscriber(groupItem, "close");

    let theGroupItem = contentWindow.GroupItems.groupItem(groupItem.id);
    ok(!theGroupItem, "The group item doesn't exists");

    let endGame = function() {
      window.removeEventListener("tabviewhidden", endGame, false);
      ok(!TabView.isVisible(), "Tab View is hidden");
      finish();
    };
    window.addEventListener("tabviewhidden", endGame, false);

    // after the last selected tabitem is closed, there would be not active
    // tabitem on the UI so we set the active tabitem before toggling the 
    // visibility of tabview
    let tabItems = contentWindow.TabItems.getItems();
    ok(tabItems[0], "A tab item exists");
    contentWindow.UI.setActiveTab(tabItems[0]);

    TabView.toggle();
  });

  let closeButton = groupItem.container.getElementsByClassName("close");
  ok(closeButton, "Group item close button exists");
  EventUtils.sendMouseEvent({ type: "click" }, closeButton[0], contentWindow);
}
