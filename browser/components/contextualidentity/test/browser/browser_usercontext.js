/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


const USER_CONTEXTS = [
  "default",
  "personal",
  "work",
];

const BASE_URI = "http://mochi.test:8888/browser/browser/components/"
  + "contextualidentity/test/browser/file_reflect_cookie_into_title.html";


// opens `uri' in a new tab with the provided userContextId and focuses it.
// returns the newly opened tab
function openTabInUserContext(uri, userContextId) {
  // open the tab in the correct userContextId
  let tab = gBrowser.addTab(uri, {userContextId});

  // select tab and make sure its browser is focused
  gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  return tab;
}

add_task(function* setup() {
  // make sure userContext is enabled.
  yield SpecialPowers.pushPrefEnv({"set": [
    ["privacy.userContext.enabled", true],
    ["dom.ipc.processCount", 1]
  ]});
});

add_task(function* test() {
  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    // load the page in 3 different contexts and set a cookie
    // which should only be visible in that context
    let cookie = USER_CONTEXTS[userContextId];

    // open our tab in the given user context
    let tab = openTabInUserContext(BASE_URI + "?" + cookie, userContextId);

    // wait for tab load
    yield BrowserTestUtils.browserLoaded(gBrowser.getBrowserForTab(tab));

    // remove the tab
    gBrowser.removeTab(tab);
  }

  {
    // Set a cookie in a different context so we can detect if that affects
    // cross-context properly. If we don't do that, we get an UNEXPECTED-PASS
    // for the localStorage case for the last tab we set.
    let tab = openTabInUserContext(BASE_URI + "?foo", 9999);
    yield BrowserTestUtils.browserLoaded(gBrowser.getBrowserForTab(tab));
    gBrowser.removeTab(tab);
  }

  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    // Load the page without setting the cookie this time
    let expectedContext = USER_CONTEXTS[userContextId];

    let tab = openTabInUserContext(BASE_URI, userContextId);

    // wait for load
    let browser = gBrowser.getBrowserForTab(tab);
    yield BrowserTestUtils.browserLoaded(browser);

    // get the title
    let title = browser.contentDocument.title.trim().split("|");

    // check each item in the title and validate it meets expectatations
    for (let part of title) {
      let [storageMethodName, value] = part.split("=");
      is(value, expectedContext,
            "the title reflects the expected contextual identity of " +
            expectedContext + " for method " + storageMethodName + ": " + value);
    }

    gBrowser.removeTab(tab);
  }
});
