/*
 * Bug 1334587 - A Test case for checking whether forgetting APIs are working for cookies.
 */

const CC = Components.Constructor;

const TEST_HOST = "example.com";
const TEST_URL =
  "http://" +
  TEST_HOST +
  "/browser/browser/components/contextualidentity/test/browser/";

const USER_CONTEXTS = ["default", "personal", "work"];

const DELETE_CONTEXT = 1;
const COOKIE_NAME = "userContextId";

//
// Support functions.
//

function getCookiesForOA(host, userContextId) {
  return Services.cookies.getCookiesFromHost(host, { userContextId });
}

//
// Test functions.
//

add_setup(async function () {
  // Make sure userContext is enabled.
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.userContext.enabled", true]],
  });
});

function checkCookies(ignoreContext = null) {
  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    if (ignoreContext && userContextId === String(ignoreContext)) {
      continue;
    }
    let cookies = getCookiesForOA(TEST_HOST, userContextId);
    ok(cookies.length, "Cookies available");

    let foundCookie = cookies[0];
    is(foundCookie.name, COOKIE_NAME, "Check cookie name");
    is(foundCookie.value, USER_CONTEXTS[userContextId], "Check cookie value");
  }
}

function deleteCookies(onlyContext = null) {
  // Using getCookiesWithOriginAttributes() to get all cookies for a certain
  // domain by using the originAttributes pattern, and clear all these cookies.
  for (let cookie of Services.cookies.getCookiesWithOriginAttributes(
    JSON.stringify({}),
    TEST_HOST
  )) {
    if (!onlyContext || cookie.originAttributes.userContextId == onlyContext) {
      Services.cookies.remove(
        cookie.host,
        cookie.name,
        cookie.path,
        cookie.originAttributes
      );
    }
  }
}

add_task(async function test_cookie_getCookiesWithOriginAttributes() {
  let tabs = [];

  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    // Load the page in different contexts and set a cookie
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
    is(foundCookie.name, COOKIE_NAME, "Check cookie name");
    is(foundCookie.value, USER_CONTEXTS[userContextId], "Check cookie value");
  }
  checkCookies();

  deleteCookies(DELETE_CONTEXT);

  checkCookies(DELETE_CONTEXT);

  deleteCookies();

  // Check that whether cookies has been cleared.
  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    let cookies = getCookiesForOA(TEST_HOST, userContextId);
    ok(!cookies.length, "No Cookie should be here");
  }
});
