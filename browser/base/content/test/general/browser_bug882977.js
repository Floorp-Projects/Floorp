"use strict";

/**
 * Tests that the identity-box shows the chromeUI styling
 * when viewing such a page in a new window.
 */
add_task(async function() {
  let homepage = "about:preferences";
  await SpecialPowers.pushPrefEnv({
    "set": [
      ["browser.startup.homepage", homepage],
      ["browser.startup.page", 1],
    ],
  });

  let win = OpenBrowserWindow();
  await BrowserTestUtils.firstBrowserLoaded(win, false);

  let browser = win.gBrowser.selectedBrowser;
  is(browser.currentURI.spec, homepage, "Loaded the correct homepage");
  checkIdentityMode(win);

  await BrowserTestUtils.closeWindow(win);
});

function checkIdentityMode(win) {
  let identityMode = win.document.getElementById("identity-box").className;
  is(identityMode, "chromeUI", "Identity state should be chromeUI for about:home in a new window");
}
