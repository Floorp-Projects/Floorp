"use strict";

const TEST_URL = "http://example.com";
const MAX_EXPIRY = Math.pow(2, 62);

function getSingleCookie() {
  let cookies = Array.from(Services.cookies.enumerator);
  Assert.equal(cookies.length, 1, "expected one cookie");
  return cookies[0];
}

async function verifyRestore(sameSiteSetting) {
  Services.cookies.removeAll();

  // Make sure that sessionstore.js can be forced to be created by setting
  // the interval pref to 0.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.sessionstore.interval", 0]],
  });

  let tab = BrowserTestUtils.addTab(gBrowser, TEST_URL);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  // Add a cookie with specific same-site setting.
  let r = Math.floor(Math.random() * MAX_EXPIRY);
  Services.cookies.add(
    TEST_URL,
    "/",
    "name" + r,
    "value" + r,
    false,
    false,
    true,
    MAX_EXPIRY,
    {},
    sameSiteSetting
  );
  await TabStateFlusher.flush(tab.linkedBrowser);

  // Get the sessionstore state for the window.
  let state = ss.getBrowserState();

  // Verify our cookie got set.
  let cookie = getSingleCookie();

  // Remove the cookie.
  Services.cookies.removeAll();

  // Restore the window state.
  await setBrowserState(state);

  // At this point, the cookie should be restored.
  let cookie2 = getSingleCookie();

  is(
    cookie2.sameSite,
    cookie.sameSite,
    "cookie same-site flag successfully restored"
  );

  // Clean up.
  Services.cookies.removeAll();
  BrowserTestUtils.removeTab(gBrowser.tabs[1]);
}

/**
 * Tests that cookie.sameSite flag is stored and restored correctly by
 * sessionstore.
 */
add_task(async function() {
  // Test for various possible values of cookie.sameSite.
  await verifyRestore(Ci.nsICookie.SAMESITE_NONE);
  await verifyRestore(Ci.nsICookie.SAMESITE_LAX);
  await verifyRestore(Ci.nsICookie.SAMESITE_STRICT);
});
