/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let initialTabsLength = gBrowser.tabs.length;

  let newTab1 = gBrowser.selectedTab = gBrowser.addTab("about:robots", {skipAnimation: true});
  let newTab2 = gBrowser.selectedTab = gBrowser.addTab("about:about", {skipAnimation: true});
  let newTab3 = gBrowser.selectedTab = gBrowser.addTab("about:config", {skipAnimation: true});
  registerCleanupFunction(function () {
    while (gBrowser.tabs.length > initialTabsLength) {
      gBrowser.removeTab(gBrowser.tabs[initialTabsLength]);
    }
  });

  is(gBrowser.tabs.length, initialTabsLength + 3, "new tabs are opened");
  is(gBrowser.tabs[initialTabsLength], newTab1, "newTab1 position is correct");
  is(gBrowser.tabs[initialTabsLength + 1], newTab2, "newTab2 position is correct");
  is(gBrowser.tabs[initialTabsLength + 2], newTab3, "newTab3 position is correct");

  let dataTransfer;
  let trapDrag = function(event) {
    dataTransfer = event.dataTransfer;
  };
  window.addEventListener("dragstart", trapDrag, true);
  registerCleanupFunction(function () {
    window.removeEventListener("dragstart", trapDrag, true);
  });

  let windowUtil = window.QueryInterface(Components.interfaces.nsIInterfaceRequestor).
                          getInterface(Components.interfaces.nsIDOMWindowUtils);
  let ds = Components.classes["@mozilla.org/widget/dragservice;1"].
           getService(Components.interfaces.nsIDragService);

  function dragAndDrop(tab1, tab2, copy) {
    let ctrlKey = copy;
    let altKey = copy;

    let rect = tab1.getBoundingClientRect();
    let x = rect.width / 2;
    let y = rect.height / 2;
    let diffX = 10;

    ds.startDragSession();
    try {
      EventUtils.synthesizeMouse(tab1, x, y, { type: "mousedown" }, window);
      EventUtils.synthesizeMouse(tab1, x + diffX, y, { type: "mousemove" }, window);

      dataTransfer.dropEffect = copy ? "copy" : "move";

      let event = window.document.createEvent("DragEvents");
      event.initDragEvent("dragover", true, true, window, 0,
                          tab2.boxObject.screenX + x + diffX,
                          tab2.boxObject.screenY + y,
                          x + diffX, y, ctrlKey, altKey, false, false, 0, null, dataTransfer);
      windowUtil.dispatchDOMEventViaPresShell(tab2, event, true);

      event = window.document.createEvent("DragEvents");
      event.initDragEvent("drop", true, true, window, 0,
                          tab2.boxObject.screenX + x + diffX,
                          tab2.boxObject.screenY + y,
                          x + diffX, y, ctrlKey, altKey, false, false, 0, null, dataTransfer);
      windowUtil.dispatchDOMEventViaPresShell(tab2, event, true);

      EventUtils.synthesizeMouse(tab2, x + diffX, y, { type: "mouseup" }, window);
    } finally {
      ds.endDragSession(true);
    }
  }

  dragAndDrop(newTab1, newTab2, false);
  is(gBrowser.tabs.length, initialTabsLength + 3, "tabs are still there");
  is(gBrowser.tabs[initialTabsLength], newTab2, "newTab2 and newTab1 are swapped");
  is(gBrowser.tabs[initialTabsLength + 1], newTab1, "newTab1 and newTab2 are swapped");
  is(gBrowser.tabs[initialTabsLength + 2], newTab3, "newTab3 stays same place");

  dragAndDrop(newTab2, newTab1, true);
  is(gBrowser.tabs.length, initialTabsLength + 4, "a tab is duplicated");
  is(gBrowser.tabs[initialTabsLength], newTab2, "newTab2 stays same place");
  is(gBrowser.tabs[initialTabsLength + 1], newTab1, "newTab1 stays same place");
  is(gBrowser.tabs[initialTabsLength + 3], newTab3, "a new tab is inserted before newTab3");
}
