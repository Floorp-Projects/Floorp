/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const URL = "http://mochi.test:8888/browser/browser/base/content/test/general/test_offline_gzip.html";

registerCleanupFunction(function() {
  // Clean up after ourself
  let uri = Services.io.newURI(URL);
  let principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
  Services.perms.removeFromPrincipal(principal, "offline-app");
  Services.prefs.clearUserPref("offline-apps.allow_by_default");
});

var cacheCount = 0;
var intervalID = 0;

//
// Handle "message" events which are posted from the iframe upon
// offline cache events.
//
function handleMessageEvents(event) {
  cacheCount++;
  switch (cacheCount) {
    case 1:
      // This is the initial caching off offline data.
      is(event.data, "oncache", "Child was successfully cached.");
      // Reload the frame; this will generate an error message
      // in the case of bug 501422.
      event.source.location.reload();
      // Use setInterval to repeatedly call a function which
      // checks that one of two things has occurred:  either
      // the offline cache is udpated (which means our iframe
      // successfully reloaded), or the string "error" appears
      // in the iframe, as in the case of bug 501422.
      intervalID = setInterval(function() {
        // Sometimes document.body may not exist, and trying to access
        // it will throw an exception, so handle this case.
        try {
          var bodyInnerHTML = event.source.document.body.innerHTML;
        } catch (e) {
          bodyInnerHTML = "";
        }
        if (cacheCount == 2 || bodyInnerHTML.includes("error")) {
          clearInterval(intervalID);
          is(cacheCount, 2, "frame not reloaded successfully");
          if (cacheCount != 2) {
            finish();
          }
        }
      }, 100);
      break;
    case 2:
      is(event.data, "onupdate", "Child was successfully updated.");
      clearInterval(intervalID);
      finish();
      break;
    default:
      // how'd we get here?
      ok(false, "cacheCount not 1 or 2");
  }
}

function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref("offline-apps.allow_by_default", true);

  // Open a new tab.
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, URL);
  registerCleanupFunction(() => gBrowser.removeCurrentTab());

  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(() => {
    // eslint-disable-next-line mozilla/no-cpows-in-tests
    let window = gBrowser.selectedBrowser.contentWindow;

    window.addEventListener("message", handleMessageEvents);
  });
}
