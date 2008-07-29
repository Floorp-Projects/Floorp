function invokeUsingCtrlD(close) {
  if (close)
    EventUtils.sendMouseEvent({ type: "click", button: 1 }, "editBookmarkPanelDoneButton");
  else
    EventUtils.synthesizeKey("d", { accelKey: true });
}

function invokeUsingStarButton(close) {
  if (close)
    EventUtils.sendMouseEvent({ type: "click", button: 1 }, "editBookmarkPanelDoneButton");
  else
    EventUtils.sendMouseEvent({ type: "click", button: 1 }, "star-button");
}

let testBrowser = null;

// test bug 432599
function test() {
  waitForExplicitFinish();

  let testTab = gBrowser.addTab();
  gBrowser.selectedTab = testTab;
  testBrowser = gBrowser.getBrowserForTab(testTab);
  testBrowser.addEventListener("load", initTest, true);

  testBrowser.contentWindow.location = 'data:text/plain,Content';
}

function initTest() {
  let invokers = [invokeUsingStarButton, invokeUsingCtrlD];

  // first, bookmark the page
  PlacesCommandHook.bookmarkPage(testBrowser);

  invokers.forEach(checkBookmarksPanel);

  finish();
}

function checkBookmarksPanel(invoker)
{
  let titleElement = document.getElementById("editBookmarkPanelTitle");
  let removeElement = document.getElementById("editBookmarkPanelRemoveButton");

  // invoke the panel for the first time
  invoker();

  let initialValue = titleElement.value;
  let initialRemoveHidden = removeElement.hidden;
  invoker(true);

  // invoke the panel for the second time
  invoker();

  is(titleElement.value, initialValue, "The bookmark panel's title should be the same");
  is(removeElement.hidden, initialRemoveHidden, "The bookmark panel's visibility should not change");
  invoker(true);
}
