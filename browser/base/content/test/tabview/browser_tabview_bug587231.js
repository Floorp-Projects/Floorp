/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let activeTab;
let testTab;
let testGroup;
let contentWindow;

function test() {
  waitForExplicitFinish();

  // create new tab
  testTab = gBrowser.addTab("about:blank");

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.toggle();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);
  ok(TabView.isVisible(), "Tab View is visible");

  contentWindow = document.getElementById("tab-view").contentWindow;

  // create group
  let testGroupRect = new contentWindow.Rect(20, 20, 300, 300);
  testGroup = new contentWindow.GroupItem([], { bounds: testGroupRect });
  ok(testGroup.isEmpty(), "This group is empty");
  
  ok(testTab._tabViewTabItem, "tab item exists");

  // place tab in group
  let testTabItem = testTab._tabViewTabItem;

  if (testTabItem.parent)
    testTabItem.parent.remove(testTabItem);
  testGroup.add(testTabItem);

  ok(testTab._tabViewTabItem, "tab item exists after adding to group");

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
    testGroup.remove(testTab._tabViewTabItem);
    testTab._tabViewTabItem.close();
    testGroup.close();

    let currentTabs = contentWindow.TabItems.getItems();
    ok(currentTabs[0], "A tab item exists to make active");
    contentWindow.UI.setActive(currentTabs[0]);
    
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
