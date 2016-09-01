"use strict";

/**
 * Tests that the identity-box shows the chromeUI styling
 * when viewing about:home in a new window.
 */
add_task(function*() {
  let homepage = "about:home";
  yield SpecialPowers.pushPrefEnv({
    "set": [
      ["browser.startup.homepage", homepage],
      ["browser.startup.page", 1],
    ]
  });

  let win = OpenBrowserWindow();
  yield BrowserTestUtils.waitForEvent(win, "load");

  let browser = win.gBrowser.selectedBrowser;
  // If we've finished loading about:home already, we can check
  // right away - otherwise, we need to wait.
  if (browser.contentDocument.readyState == "complete" &&
      browser.currentURI.spec == homepage) {
    checkIdentityMode(win);
  } else {
    yield BrowserTestUtils.browserLoaded(browser, false, homepage);
    checkIdentityMode(win);
  }

  yield BrowserTestUtils.closeWindow(win);
});

function checkIdentityMode(win) {
  let identityMode = win.document.getElementById("identity-box").className;
  is(identityMode, "chromeUI", "Identity state should be chromeUI for about:home in a new window");
}
