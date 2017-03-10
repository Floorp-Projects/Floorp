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
  yield BrowserTestUtils.firstBrowserLoaded(win, false);

  let browser = win.gBrowser.selectedBrowser;
  is(browser.currentURI.spec, homepage, "Loaded the correct homepage");
  checkIdentityMode(win);

  yield BrowserTestUtils.closeWindow(win);
});

function checkIdentityMode(win) {
  let identityMode = win.document.getElementById("identity-box").className;
  is(identityMode, "chromeUI", "Identity state should be chromeUI for about:home in a new window");
}
