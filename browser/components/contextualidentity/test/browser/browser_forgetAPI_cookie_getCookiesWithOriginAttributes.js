/*
 * Bug 1278037 - A Test case for checking whether forgetting APIs are working for cookies.
 */

const CC = Components.Constructor;

const TEST_HOST = "example.com";
const TEST_URL =
  "http://" +
  TEST_HOST +
  "/browser/browser/components/contextualidentity/test/browser/";

const USER_CONTEXTS = ["default", "personal"];

//
// Support functions.
//

async function openTabInUserContext(uri, userContextId) {
  // Open the tab in the correct userContextId.
  let tab = BrowserTestUtils.addTab(gBrowser, uri, { userContextId });

  // Select tab and make sure its browser is focused.
  gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);
  return { tab, browser };
}

function getCookiesForOA(host, userContextId) {
  return Services.cookies.getCookiesFromHost(host, { userContextId });
}

//
// Test functions.
//

add_task(async function setup() {
  // Make sure userContext is enabled.
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.userContext.enabled", true]],
  });
});

add_task(async function test_cookie_getCookiesWithOriginAttributes() {
  let tabs = [];
  let cookieName = "userContextId";

  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    // Load the page in 2 different contexts and set a cookie
    // which should only be visible in that context.
    let value = USER_CONTEXTS[userContextId];

    // Open our tab in the given user context.
    tabs[userContextId] = await openTabInUserContext(
      TEST_URL + "file_reflect_cookie_into_title.html?" + value,
      userContextId
    );

    // Close this tab.
    BrowserTestUtils.removeTab(tabs[userContextId].tab);
  }

  // Check that cookies have been set properly.
  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    let cookies = getCookiesForOA(TEST_HOST, userContextId);
    ok(cookies.length, "Cookies available");

    let foundCookie = cookies[0];
    is(foundCookie.name, cookieName, "Check cookie name");
    is(foundCookie.value, USER_CONTEXTS[userContextId], "Check cookie value");
  }

  // Using getCookiesWithOriginAttributes() to get all cookies for a certain
  // domain by using the originAttributes pattern, and clear all these cookies.
  for (let cookie of Services.cookies.getCookiesWithOriginAttributes(
    JSON.stringify({}),
    TEST_HOST
  )) {
    Services.cookies.remove(
      cookie.host,
      cookie.name,
      cookie.path,
      cookie.originAttributes
    );
  }

  // Check that whether cookies has been cleared.
  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    let cookies = getCookiesForOA(TEST_HOST, userContextId);
    ok(!cookies.length, "No Cookie should be here");
  }
});
