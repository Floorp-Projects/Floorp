/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* Ensure that clicking the button in the Offline mode neterror page makes the browser go online. See bug 435325. */
/* TEST_PATH=docshell/test/browser/browser_bug435325.js make -C $(OBJDIR) mochitest-browser-chrome */

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  window.addEventListener("DOMContentLoaded", checkPage, false);

  // Go offline and disable the cache, then try to load the test URL.
  Services.io.offline = true;
  Services.prefs.setBoolPref("browser.cache.disk.enable", false);
  Services.prefs.setBoolPref("browser.cache.memory.enable", false);
  content.location = "http://example.com/";
}

function checkPage() {
  if(content.location == "about:blank") {
    info("got about:blank, which is expected once, so return");
    return;
  }

  window.removeEventListener("DOMContentLoaded", checkPage, false);

  ok(Services.io.offline, "Setting Services.io.offline to true.");
  is(gBrowser.contentDocument.documentURI.substring(0,27),
    "about:neterror?e=netOffline", "Loading the Offline mode neterror page.");

  // Now press the "Try Again" button
  ok(gBrowser.contentDocument.getElementById("errorTryAgain"),
    "The error page has got a #errorTryAgain element");
  gBrowser.contentDocument.getElementById("errorTryAgain").click();

  ok(!Services.io.offline, "After clicking the Try Again button, we're back "
   +" online. This depends on Components.interfaces.nsIDOMWindowUtils being "
   +"available from untrusted content (bug 435325).");

  finish();
}

registerCleanupFunction(function() {
  Services.prefs.setBoolPref("browser.cache.disk.enable", true);
  Services.prefs.setBoolPref("browser.cache.memory.enable", true);
  Services.io.offline = false;
  gBrowser.removeCurrentTab();
});
