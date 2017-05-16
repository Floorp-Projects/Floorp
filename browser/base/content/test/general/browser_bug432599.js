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
     EventUtils.synthesizeMouseAtCenter(BookmarkingUI.star, {});
    break;
  case 2:
  case 4:
    EventUtils.synthesizeKey("VK_ESCAPE", {});
    break;
  case 3:
     EventUtils.synthesizeMouseAtCenter(BookmarkingUI.star,
                                        { clickCount: 2 });
    break;
  }
}

var testURL = "data:text/plain,Content";
var bookmarkId;

function add_bookmark(aURI, aTitle) {
  return PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                              aURI, PlacesUtils.bookmarks.DEFAULT_INDEX,
                                              aTitle);
}

// test bug 432599
function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedBrowser.addEventListener("load", function() {
    waitForStarChange(false, initTest);
  }, {capture: true, once: true});

  content.location = testURL;
}

function initTest() {
  // First, bookmark the page.
  bookmarkId = add_bookmark(makeURI(testURL), "Bug 432599 Test");

  checkBookmarksPanel(invokers[currentInvoker], 1);
}

function waitForStarChange(aValue, aCallback) {
  let expectedStatus = aValue ? BookmarkingUI.STATUS_STARRED
                              : BookmarkingUI.STATUS_UNSTARRED;
  if (BookmarkingUI.status == BookmarkingUI.STATUS_UPDATING ||
      BookmarkingUI.status != expectedStatus) {
    info("Waiting for star button change.");
    setTimeout(waitForStarChange, 50, aValue, aCallback);
    return;
  }
  aCallback();
}

var invokers = [invokeUsingStarButton, invokeUsingCtrlD];
var currentInvoker = 0;

var initialValue;
var initialRemoveHidden;

var popupElement = document.getElementById("editBookmarkPanel");
var titleElement = document.getElementById("editBookmarkPanelTitle");
var removeElement = document.getElementById("editBookmarkPanelRemoveButton");

function checkBookmarksPanel(invoker, phase) {
  let onPopupShown = function(aEvent) {
    if (aEvent.originalTarget == popupElement) {
      popupElement.removeEventListener("popupshown", arguments.callee);
      checkBookmarksPanel(invoker, phase + 1);
    }
  };
  let onPopupHidden = function(aEvent) {
    if (aEvent.originalTarget == popupElement) {
      popupElement.removeEventListener("popuphidden", arguments.callee);
      if (phase < 4) {
        checkBookmarksPanel(invoker, phase + 1);
      } else {
        ++currentInvoker;
        if (currentInvoker < invokers.length) {
          checkBookmarksPanel(invokers[currentInvoker], 1);
        } else {
          gBrowser.removeTab(gBrowser.selectedTab, {skipPermitUnload: true});
          PlacesUtils.bookmarks.removeItem(bookmarkId);
          executeSoon(finish);
        }
      }
    }
  };

  switch (phase) {
  case 1:
  case 3:
    popupElement.addEventListener("popupshown", onPopupShown);
    break;
  case 2:
    popupElement.addEventListener("popuphidden", onPopupHidden);
    initialValue = titleElement.value;
    initialRemoveHidden = removeElement.hidden;
    break;
  case 4:
    popupElement.addEventListener("popuphidden", onPopupHidden);
    is(titleElement.value, initialValue, "The bookmark panel's title should be the same");
    is(removeElement.hidden, initialRemoveHidden, "The bookmark panel's visibility should not change");
    break;
  }
  invoker(phase);
}
