/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  waitForExplicitFinish();

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("browser.startup.homepage");
    Services.prefs.clearUserPref("browser.startup.page");
    win.close();
  });

  let homepage = "about:home";
  Services.prefs.setCharPref("browser.startup.homepage", homepage);
  Services.prefs.setIntPref("browser.startup.page", 1);
  let win = OpenBrowserWindow();
  whenDelayedStartupFinished(win, function() {
    let browser = win.gBrowser.selectedBrowser;
    if (browser.contentDocument.readyState == "complete" &&
        browser.currentURI.spec == homepage) {
      checkIdentityMode(win);
      return;
    }

    browser.addEventListener("load", function onLoad() {
      if (browser.currentURI.spec != homepage)
        return;
      browser.removeEventListener("load", onLoad, true);
      checkIdentityMode(win);
    }, true);
  });
}

function checkIdentityMode(win) {
  let identityMode = win.document.getElementById("identity-box").className;
  is(identityMode, "chromeUI", "Identity state should be chromeUI for about:home in a new window");
  finish();
}
