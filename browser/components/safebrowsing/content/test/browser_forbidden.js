/* Ensure that pages in the forbidden list are blocked. */

const PREF_FORBIDDEN_ENABLED = "browser.safebrowsing.forbiddenURIs.enabled";
const BENIGN_PAGE = "http://example.com/";
const FORBIDDEN_PAGE = "http://www.itisatrap.org/firefox/forbidden.html";
var tabbrowser = null;

registerCleanupFunction(function() {
  tabbrowser = null;
  Services.prefs.clearUserPref(PREF_FORBIDDEN_ENABLED);
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

function testBenignPage(window) {
  info("Non-forbidden content must not be blocked");
  var getmeout_button = window.document.getElementById("getMeOutButton");
  var ignorewarning_button = window.document.getElementById("ignoreWarningButton");
  ok(!getmeout_button, "GetMeOut button not present");
  ok(!ignorewarning_button, "IgnoreWarning button not present");
}

function testForbiddenPage(window) {
  info("Forbidden content must be blocked");
  ok(true, "about:blocked was shown");
}

add_task(function* testNormalBrowsing() {
  tabbrowser = gBrowser;
  let tab = tabbrowser.selectedTab = tabbrowser.addTab();

  info("Load a test page that's not forbidden");
  yield promiseTabLoadEvent(tab, BENIGN_PAGE, "load");
  testBenignPage(tab.ownerDocument.defaultView);

  info("Load a test page that is forbidden");
  yield promiseTabLoadEvent(tab, FORBIDDEN_PAGE, "AboutBlockedLoaded");
  testForbiddenPage(tab.ownerDocument.defaultView);
});
