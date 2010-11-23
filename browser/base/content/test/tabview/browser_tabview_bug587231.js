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
 * Sean Dunn <seanedunn@yahoo.com>
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

let activeTab;
let testTab;
let testGroup;
let contentWindow;

function test() {
  waitForExplicitFinish();
  contentWindow = document.getElementById("tab-view").contentWindow;

  // create new tab
  testTab = gBrowser.addTab("http://mochi.test:8888/");

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.toggle();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);
  ok(TabView.isVisible(), "Tab View is visible");

  // create group
  let testGroupRect = new contentWindow.Rect(20, 20, 300, 300);
  testGroup = new contentWindow.GroupItem([], { bounds: testGroupRect });
  ok(testGroup.isEmpty(), "This group is empty");
  
  // place tab in group
  let testTabItem = testTab.tabItem;

  if (testTabItem.parent)
    testTabItem.parent.remove(testTabItem);
  testGroup.add(testTabItem);

  // record last update time of tab canvas
  let initialUpdateTime = testTabItem._lastTabUpdateTime;

  // simulate resize
  let resizer = contentWindow.iQ('.iq-resizable-handle', testGroup.container)[0];
  let offsetX = 100;
  let offsetY = 100;
  let delay = 500;

  let funcChain = new Array();
  funcChain.push(function() {
    EventUtils.synthesizeMouse(
      resizer, 1, 1, { type: "mousedown" }, contentWindow);
    setTimeout(funcChain.shift(), delay);
  });
  // drag
  for (let i = 4; i >= 0; i--) {
    funcChain.push(function() {
      EventUtils.synthesizeMouse(
        resizer,  Math.round(offsetX/4),  Math.round(offsetY/4),
        { type: "mousemove" }, contentWindow);
      setTimeout(funcChain.shift(), delay);
    });
  }
  funcChain.push(function() {
    EventUtils.synthesizeMouse(resizer, 0, 0, { type: "mouseup" }, 
      contentWindow);    
    setTimeout(funcChain.shift(), delay);
  });
  funcChain.push(function() {
    // verify that update time has changed after last update
    let lastTime = testTabItem._lastTabUpdateTime;
    let hbTiming = contentWindow.TabItems._heartbeatTiming;
    ok((lastTime - initialUpdateTime) > hbTiming, "Tab has been updated:"+lastTime+"-"+initialUpdateTime+">"+hbTiming);

    // clean up
    testGroup.remove(testTab.tabItem);
    testTab.tabItem.close();
    testGroup.close();

    let currentTabs = contentWindow.TabItems.getItems();
    ok(currentTabs[0], "A tab item exists to make active");
    contentWindow.UI.setActiveTab(currentTabs[0]);
    
    window.addEventListener("tabviewhidden", finishTest, false);
    TabView.toggle();
  });
  setTimeout(funcChain.shift(), delay);
}

function finishTest() {
  window.removeEventListener("tabviewhidden", finishTest, false);
  ok(!TabView.isVisible(), "Tab View is not visible");

  finish();  
}
