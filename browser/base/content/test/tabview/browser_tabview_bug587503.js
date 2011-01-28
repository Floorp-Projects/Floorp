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
 * The Original Code is bug 587503 test.
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

  let currentGroup = contentWindow.GroupItems.getActiveGroupItem();

  // Create a group and make it active
  let box = new contentWindow.Rect(100, 100, 400, 430);
  let group = new contentWindow.GroupItem([], { bounds: box });
  ok(group.isEmpty(), "This group is empty");
  contentWindow.GroupItems.setActiveGroupItem(group);
  
  // Create a bunch of tabs in the group
  let tabs = [];
  tabs.push(gBrowser.loadOneTab("about:blank#0", {inBackground: true}));
  tabs.push(gBrowser.loadOneTab("about:blank#1", {inBackground: true}));
  tabs.push(gBrowser.loadOneTab("about:blank#2", {inBackground: true}));
  tabs.push(gBrowser.loadOneTab("about:blank#3", {inBackground: true}));
  tabs.push(gBrowser.loadOneTab("about:blank#4", {inBackground: true}));
  tabs.push(gBrowser.loadOneTab("about:blank#5", {inBackground: true}));
  tabs.push(gBrowser.loadOneTab("about:blank#6", {inBackground: true}));

  ok(!group.shouldStack(group._children.length), "Group should not stack.");
  is(group._columns, 3, "There should be three columns.");

  // PREPARE FINISH:
  group.addSubscriber(group, "close", function() {
    group.removeSubscriber(group, "close");

    ok(group.isEmpty(), "The group is empty again");

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
  
  // STAGE 1: move the last tab to the third position
  let currentTarget = tabs[6]._tabViewTabItem;
  let currentPos = currentTarget.getBounds().center();
  let targetPos = tabs[2]._tabViewTabItem.getBounds().center();
  let vector = new contentWindow.Point(targetPos.x - currentPos.x,
                                       targetPos.y - currentPos.y);
  checkDropIndexAndDropSpace(currentTarget, group, vector.x, vector.y, contentWindow,
                             function(index, dropSpaceActiveValues) {
    // Now: 0, 1, 6, 2, 3, 4, 5
    is(index, 2, "Tab 6 is now in the third position");
    is(dropSpaceActiveValues[0], true, "dropSpace was always showing");

    // STAGE 2: move the second tab to the end of the list
    let currentTarget = tabs[1]._tabViewTabItem;
    let currentPos = currentTarget.getBounds().center();
    // pick a point in that empty bottom part of the group
    let groupBounds = group.getBounds();
    let bottomPos = new contentWindow.Point(
                      (groupBounds.left + groupBounds.right) / 2,
                      groupBounds.bottom - 15);
    let vector = new contentWindow.Point(bottomPos.x - currentPos.x,
                                         bottomPos.y - currentPos.y);
    checkDropIndexAndDropSpace(currentTarget, group, vector.x, vector.y, contentWindow,
                               function(index, dropSpaceActiveValues) {
      // Now: 0, 6, 2, 3, 4, 5, 1
      is(index, 6, "Tab 1 is now at the end of the group");
      is(dropSpaceActiveValues[0], true, "dropSpace was always showing");
    
      // STAGE 3: move the fifth tab outside the group
      // Note: there should be room below the active group...
      let currentTarget = tabs[4]._tabViewTabItem;
      let currentPos = currentTarget.getBounds().center();
      // Pick a point below the group.
      let belowPos = new contentWindow.Point(
                        (groupBounds.left + groupBounds.right) / 2,
                        groupBounds.bottom + 300);
      let vector = new contentWindow.Point(belowPos.x - currentPos.x,
                                           belowPos.y - currentPos.y);
      checkDropIndexAndDropSpace(currentTarget, group, vector.x, vector.y, contentWindow,
                                 function(index, dropSpaceActiveValues) {
        // Now: 0, 6, 2, 3, 5, 1
        is(index, -1, "Tab 5 is no longer in the group");
        contentWindow.Utils.log('dropSpaceActiveValues',dropSpaceActiveValues);
        is(dropSpaceActiveValues[0], true, "The group began by showing a dropSpace");
        is(dropSpaceActiveValues[dropSpaceActiveValues.length - 1], false, "In the end, the group was not showing a dropSpace");
        
        // We wrap this in a setTimeout with 1000ms delay in order to wait for the
        // tab to resize, as it does after we drop it in stage 3 outside of the group.
        setTimeout(function() {
          // STAGE 4: move the fifth tab back into the group, on the second row.
          let currentTarget = tabs[4]._tabViewTabItem;
          let currentPos = currentTarget.getBounds().center();
          let targetPos = tabs[5]._tabViewTabItem.getBounds().center();
          // contentWindow.Utils.log(targetPos, currentPos);
          vector = new contentWindow.Point(targetPos.x - currentPos.x,
                                               targetPos.y - currentPos.y);
          // Call with time = 4000
          checkDropIndexAndDropSpace(currentTarget, group, vector.x, vector.y, contentWindow,
                                     function(index, dropSpaceActiveValues) {

            is(group._columns, 3, "There should be three columns.");

            // Now: 0, 6, 2, 3, 4, 5, 1
            is(index, 4, "Tab 5 is back and again the fifth tab.");
            contentWindow.Utils.log('dropSpaceActiveValues',dropSpaceActiveValues);
            is(dropSpaceActiveValues[0], false, "The group began by not showing a dropSpace");
            is(dropSpaceActiveValues[dropSpaceActiveValues.length - 1], true, "In the end, the group was showing a dropSpace");
            
            // Get rid of the group and its children
            // The group close will trigger a finish().
            group.closeAll();
            group.closeHidden();
          }, 6000, false);
        },1000);
        
      });
    
    });

  });
}

function simulateSlowDragDrop(srcElement, offsetX, offsetY, contentWindow, time) {
  // enter drag mode
  let dataTransfer;

  // contentWindow.Utils.log('offset', offsetX, offsetY);
  let bounds = srcElement.getBoundingClientRect();
  // contentWindow.Utils.log('original center', bounds.left + bounds.width / 2, bounds.top + bounds.height / 2);

  EventUtils.synthesizeMouse(
    srcElement, 2, 2, { type: "mousedown" }, contentWindow);
  let event = contentWindow.document.createEvent("DragEvents");
  event.initDragEvent(
    "dragenter", true, true, contentWindow, 0, 0, 0, 0, 0,
    false, false, false, false, 1, null, dataTransfer);
  srcElement.dispatchEvent(event);
  
  let steps = 20;
  
  // drag over
  let moveIncremental = function moveIncremental(i, steps) {
    // calculate how much to move
    let offsetXDiff = Math.round(i * offsetX / steps) - Math.round((i - 1) * offsetX / steps);
    let offsetYDiff = Math.round(i * offsetY / steps) - Math.round((i - 1) * offsetY / steps);
    // contentWindow.Utils.log('step', offsetXDiff, offsetYDiff);
    // simulate mousemove
    EventUtils.synthesizeMouse(
      srcElement, offsetXDiff + 2, offsetYDiff + 2,
      { type: "mousemove" }, contentWindow);
    // simulate dragover
    let event = contentWindow.document.createEvent("DragEvents");
    event.initDragEvent(
      "dragover", true, true, contentWindow, 0, 0, 0, 0, 0,
      false, false, false, false, 0, null, dataTransfer);
    srcElement.dispatchEvent(event);
    let bounds = srcElement.getBoundingClientRect();
    // contentWindow.Utils.log(i, 'center', bounds.left + bounds.width / 2, bounds.top + bounds.height / 2);
  };
  for (let i = 1; i <= steps; i++)
    setTimeout(moveIncremental, i / (steps + 1) * time, i, steps);

  // drop
  let finalDrop = function finalDrop() {
    EventUtils.synthesizeMouseAtCenter(srcElement, { type: "mouseup" }, contentWindow);
    event = contentWindow.document.createEvent("DragEvents");
    event.initDragEvent(
      "drop", true, true, contentWindow, 0, 0, 0, 0, 0,
      false, false, false, false, 0, null, dataTransfer);
    srcElement.dispatchEvent(event);
    contentWindow.iQ(srcElement).css({border: 'green 1px solid'});
  }
  setTimeout(finalDrop, time);
}

function checkDropIndexAndDropSpace(item, group, offsetX, offsetY, contentWindow, callback, time) {
  contentWindow.UI.setActiveTab(item);
  let dropSpaceActiveValues = [];
  let recordDropSpaceValue = function() {
    dropSpaceActiveValues.push(group._dropSpaceActive);
  };
//  contentWindow.iQ(item.container).css({border: 'red 1px solid'});
  let onDrop = function() {
    item.container.removeEventListener('dragover', recordDropSpaceValue, false);
    item.container.removeEventListener('drop', onDrop, false);
    let index = group._children.indexOf(item);
    callback(index, dropSpaceActiveValues);
  };
  item.container.addEventListener('dragover', recordDropSpaceValue, false);
  item.container.addEventListener('drop', onDrop, false);
  simulateSlowDragDrop(item.container, offsetX, offsetY, contentWindow, time || 1000);
}
