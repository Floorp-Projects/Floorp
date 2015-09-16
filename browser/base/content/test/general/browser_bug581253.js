/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testURL = "data:text/plain,nothing but plain text";
var testTag = "581253_tag";
var timerID = -1;

function test() {
  registerCleanupFunction(function() {
    PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);
    if (timerID > 0) {
      clearTimeout(timerID);
    }
  });
  waitForExplicitFinish();

  let tab = gBrowser.selectedTab = gBrowser.addTab();
  tab.linkedBrowser.addEventListener("load", (function(event) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    let uri = makeURI(testURL);
    let bmTxn =
      new PlacesCreateBookmarkTransaction(uri,
                                          PlacesUtils.unfiledBookmarksFolderId,
                                          -1, "", null, []);
    PlacesUtils.transactionManager.doTransaction(bmTxn);

    ok(PlacesUtils.bookmarks.isBookmarked(uri), "the test url is bookmarked");
    waitForStarChange(true, onStarred);
  }), true);

  content.location = testURL;
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

function onStarred() {
  is(BookmarkingUI.status, BookmarkingUI.STATUS_STARRED,
     "star button indicates that the page is bookmarked");

  let uri = makeURI(testURL);
  let tagTxn = new PlacesTagURITransaction(uri, [testTag]);
  PlacesUtils.transactionManager.doTransaction(tagTxn);

  StarUI.panel.addEventListener("popupshown", onPanelShown, false);
  BookmarkingUI.star.click();
}

function onPanelShown(aEvent) {
  if (aEvent.target == StarUI.panel) {
    StarUI.panel.removeEventListener("popupshown", arguments.callee, false);
    let tagsField = document.getElementById("editBMPanel_tagsField");
    ok(tagsField.value == testTag, "tags field value was set");
    tagsField.focus();

    StarUI.panel.addEventListener("popuphidden", onPanelHidden, false);
    let removeButton = document.getElementById("editBookmarkPanelRemoveButton");
    removeButton.click();
  }
}

function onPanelHidden(aEvent) {
  if (aEvent.target == StarUI.panel) {
    StarUI.panel.removeEventListener("popuphidden", arguments.callee, false);

    executeSoon(function() {
      ok(!PlacesUtils.bookmarks.isBookmarked(makeURI(testURL)),
         "the bookmark for the test url has been removed");
      is(BookmarkingUI.status, BookmarkingUI.STATUS_UNSTARRED,
         "star button indicates that the bookmark has been removed");
      gBrowser.removeCurrentTab();
      PlacesTestUtils.clearHistory().then(finish);
    });
  }
}
