/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function () {
  let firstLocation =
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://example.org/browser/browser/base/content/test/general/dummy_page.html";
  await BrowserTestUtils.openNewForegroundTab(gBrowser, firstLocation);

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, async function () {
    // Push the state before maximizing the window and clicking below.
    content.history.pushState("page2", "page2", "page2");
  });

  window.maximize();

  // Find where the nav-bar is vertically.
  var navBar = document.getElementById("nav-bar");
  var boundingRect = navBar.getBoundingClientRect();
  var yPixel = boundingRect.top + Math.floor(boundingRect.height / 2);
  var xPixel = 0; // Use the first pixel of the screen since it is maximized.

  let popStatePromise = BrowserTestUtils.waitForContentEvent(
    gBrowser.selectedBrowser,
    "popstate",
    true
  );
  EventUtils.synthesizeMouseAtPoint(xPixel, yPixel, {}, window);
  await popStatePromise;

  is(
    gBrowser.selectedBrowser.currentURI.spec,
    firstLocation,
    "Clicking the first pixel should have navigated back."
  );
  window.restore();

  gBrowser.removeCurrentTab();
});
