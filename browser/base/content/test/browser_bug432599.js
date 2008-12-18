function invokeUsingCtrlD(phase) {
  switch (phase) {
  case 1:
    EventUtils.synthesizeKey("d", { accelKey: true });
    break;
  case 2:
  case 4:
    EventUtils.synthesizeKey("VK_ESCAPE", {});
    break;
  case 3:
    EventUtils.synthesizeKey("d", { accelKey: true });
    EventUtils.synthesizeKey("d", { accelKey: true });
    break;
  }
}

function invokeUsingStarButton(phase) {
  switch (phase) {
  case 1:
    EventUtils.sendMouseEvent({ type: "click" }, "star-button");
    break;
  case 2:
  case 4:
    EventUtils.synthesizeKey("VK_ESCAPE", {});
    break;
  case 3:
    EventUtils.synthesizeMouse(document.getElementById("star-button"),
                               1, 1, { clickCount: 2 });
    break;
  }
}

// test bug 432599
function test() {
  waitForExplicitFinish();

  let testTab = gBrowser.addTab();
  gBrowser.selectedTab = testTab;
  let testBrowser = gBrowser.getBrowserForTab(testTab);
  testBrowser.addEventListener("load", initTest, true);

  testBrowser.contentWindow.location = "data:text/plain,Content";
}

let invokers = [invokeUsingStarButton, invokeUsingCtrlD];
let currentInvoker = 0;

function initTest() {
  // first, bookmark the page
  let app = Components.classes["@mozilla.org/fuel/application;1"]
                      .getService(Components.interfaces.fuelIApplication);
  let ios = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Ci.nsIIOService);
  app.bookmarks.toolbar.addBookmark("Bug 432599 Test",
                                    ios.newURI("data:text/plain,Content", null, null));

  checkBookmarksPanel(invokers[currentInvoker], 1);
}

let initialValue;
let initialRemoveHidden;

let popupElement = document.getElementById("editBookmarkPanel");
let titleElement = document.getElementById("editBookmarkPanelTitle");
let removeElement = document.getElementById("editBookmarkPanelRemoveButton");

function checkBookmarksPanel(invoker, phase)
{
  let onPopupShown = function(aEvent) {
    if (aEvent.originalTarget == popupElement) {
      checkBookmarksPanel(invoker, phase + 1);
      popupElement.removeEventListener("popupshown", onPopupShown, false);
    }
  };
  let onPopupHidden = function(aEvent) {
    if (aEvent.originalTarget == popupElement) {
      if (phase < 4) {
        checkBookmarksPanel(invoker, phase + 1);
      } else {
        ++ currentInvoker;
        if (currentInvoker < invokers.length) {
          checkBookmarksPanel(invokers[currentInvoker], 1);
        } else {
          gBrowser.removeCurrentTab();
          finish();
        }
      }
      popupElement.removeEventListener("popuphidden", onPopupHidden, false);
    }
  };

  switch (phase) {
  case 1:
  case 3:
    popupElement.addEventListener("popupshown", onPopupShown, false);
    break;
  case 2:
    popupElement.addEventListener("popuphidden", onPopupHidden, false);
    initialValue = titleElement.value;
    initialRemoveHidden = removeElement.hidden;
    break;
  case 4:
    popupElement.addEventListener("popuphidden", onPopupHidden, false);
    is(titleElement.value, initialValue, "The bookmark panel's title should be the same");
    is(removeElement.hidden, initialRemoveHidden, "The bookmark panel's visibility should not change");
    break;
  }
  invoker(phase);
}
