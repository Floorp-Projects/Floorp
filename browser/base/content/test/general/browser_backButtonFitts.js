/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function test () {
  waitForExplicitFinish();
  var firstLocation = "http://example.org/browser/browser/base/content/test/general/dummy_page.html";
  gBrowser.selectedTab = gBrowser.addTab(firstLocation);
  gBrowser.selectedBrowser.addEventListener("pageshow", function onPageShow1() {
    gBrowser.selectedBrowser.removeEventListener("pageshow", onPageShow1);
    gBrowser.selectedBrowser.contentWindow.history.pushState("page2", "page2", "page2");
    window.maximize();

    // Find where the nav-bar is vertically.
    var navBar = document.getElementById("nav-bar");
    var boundingRect = navBar.getBoundingClientRect();
    var yPixel = boundingRect.top + Math.floor(boundingRect.height / 2);
    var xPixel = 0; // Use the first pixel of the screen since it is maximized.

    gBrowser.selectedBrowser.contentWindow.addEventListener("popstate", function onPopState() {
      gBrowser.selectedBrowser.contentWindow.removeEventListener("popstate", onPopState);
      is(gBrowser.selectedBrowser.contentDocument.location.href, firstLocation,
         "Clicking the first pixel should have navigated back.");
      window.restore();
      gBrowser.removeCurrentTab();
      finish();
    });
    EventUtils.synthesizeMouseAtPoint(xPixel, yPixel, {}, window);
  });
}
