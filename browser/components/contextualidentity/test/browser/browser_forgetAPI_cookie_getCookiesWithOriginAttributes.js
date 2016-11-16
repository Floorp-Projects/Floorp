/*
 * Bug 1278037 - A Test case for checking whether forgetting APIs are working for cookies.
 */

const { classes: Cc, Constructor: CC, interfaces: Ci, utils: Cu } = Components;

const TEST_HOST = "example.com";
const TEST_URL = "http://" + TEST_HOST + "/browser/browser/components/contextualidentity/test/browser/";

const USER_CONTEXTS = [
  "default",
  "personal",
];

//
// Support functions.
//

function* openTabInUserContext(uri, userContextId) {
  // Open the tab in the correct userContextId.
  let tab = gBrowser.addTab(uri, {userContextId});

  // Select tab and make sure its browser is focused.
  gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  let browser = gBrowser.getBrowserForTab(tab);
  yield BrowserTestUtils.browserLoaded(browser);
  return {tab, browser};
}

function getCookiesForOA(host, userContextId) {
  return Services.cookies.getCookiesFromHost(host, {userContextId});
}

//
// Test functions.
//

add_task(function* setup() {
  // Make sure userContext is enabled.
  yield SpecialPowers.pushPrefEnv({"set": [
      [ "privacy.userContext.enabled", true ],
  ]});
});

add_task(function* test_cookie_getCookiesWithOriginAttributes() {
  let tabs = [];
  let cookieName = "userContextId";

  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    // Load the page in 2 different contexts and set a cookie
    // which should only be visible in that context.
    let value = USER_CONTEXTS[userContextId];

    // Open our tab in the given user context.
    tabs[userContextId] = yield* openTabInUserContext(TEST_URL+ "file_reflect_cookie_into_title.html?" + value, userContextId);

    // Close this tab.
    yield BrowserTestUtils.removeTab(tabs[userContextId].tab);
  }

  // Check that cookies have been set properly.
  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    let enumerator = getCookiesForOA(TEST_HOST, userContextId);
    ok(enumerator.hasMoreElements(), "Cookies available");

    let foundCookie = enumerator.getNext().QueryInterface(Ci.nsICookie2);
    is(foundCookie["name"], cookieName, "Check cookie name");
    is(foundCookie["value"], USER_CONTEXTS[userContextId], "Check cookie value");
  }

  // Using getCookiesWithOriginAttributes() to get all cookies for a certain
  // domain by using the originAttributes pattern, and clear all these cookies.
  let enumerator = Services.cookies.getCookiesWithOriginAttributes(JSON.stringify({}), TEST_HOST);
  while (enumerator.hasMoreElements()) {
    let cookie = enumerator.getNext().QueryInterface(Ci.nsICookie);
    Services.cookies.remove(cookie.host, cookie.name, cookie.path, false, cookie.originAttributes);
  }

  // Check that whether cookies has been cleared.
  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    let e = getCookiesForOA(TEST_HOST, userContextId);
    ok(!e.hasMoreElements(), "No Cookie should be here");
  }
});
