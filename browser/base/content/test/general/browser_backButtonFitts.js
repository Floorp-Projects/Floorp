/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(function* () {
  let firstLocation = "http://example.org/browser/browser/base/content/test/general/dummy_page.html";
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, firstLocation);

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    // Push the state before maximizing the window and clicking below.
    content.history.pushState("page2", "page2", "page2");

    // While in the child process, add a listener for the popstate event here. This
    // event will fire when the mouse click happens.
    content.addEventListener("popstate", function onPopState() {
      content.removeEventListener("popstate", onPopState, false);
      sendAsyncMessage("Test:PopStateOccurred", { location: content.document.location.href });
    }, false);
  });

  window.maximize();

  // Find where the nav-bar is vertically.
  var navBar = document.getElementById("nav-bar");
  var boundingRect = navBar.getBoundingClientRect();
  var yPixel = boundingRect.top + Math.floor(boundingRect.height / 2);
  var xPixel = 0; // Use the first pixel of the screen since it is maximized.

  let resultLocation = yield new Promise(resolve => {
    messageManager.addMessageListener("Test:PopStateOccurred", function statePopped(message) {
      messageManager.removeMessageListener("Test:PopStateOccurred", statePopped);
      resolve(message.data.location);
    });

    EventUtils.synthesizeMouseAtPoint(xPixel, yPixel, {}, window);
  });

  is(resultLocation, firstLocation, "Clicking the first pixel should have navigated back.");
  window.restore();

  gBrowser.removeCurrentTab();
});
