/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* globals ChromeUtils, Assert, add_task */
"use strict";

// ClientID fails without...
do_get_profile();

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);
const { ClientID } = ChromeUtils.import("resource://gre/modules/ClientID.jsm");
const { Discovery } = ChromeUtils.import("resource:///modules/Discovery.jsm");
const { ContextualIdentityService } = ChromeUtils.import(
  "resource://gre/modules/ContextualIdentityService.jsm"
);

const TAAR_COOKIE_NAME = "taarId";

add_task(async function test_discovery() {
  let uri = Services.io.newURI("https://example.com/foobar");

  // Ensure the prefs we need
  Services.prefs.setBoolPref("browser.discovery.enabled", true);
  Services.prefs.setBoolPref("browser.discovery.containers.enabled", true);
  Services.prefs.setBoolPref("datareporting.healthreport.uploadEnabled", true);
  Services.prefs.setCharPref("browser.discovery.sites", uri.host);

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.discovery.enabled");
    Services.prefs.clearUserPref("browser.discovery.containers.enabled");
    Services.prefs.clearUserPref("browser.discovery.sites");
    Services.prefs.clearUserPref("datareporting.healthreport.uploadEnabled");
  });

  // This is normally initialized by telemetry, force id creation.  This results
  // in Discovery setting the cookie.
  await ClientID.getClientID();
  await Discovery.update();

  ok(
    Services.cookies.cookieExists(uri.host, "/", TAAR_COOKIE_NAME, {}),
    "cookie exists"
  );
  ok(
    !Services.cookies.cookieExists(uri.host, "/", TAAR_COOKIE_NAME, {
      privateBrowsingId: 1,
    }),
    "no private cookie exists"
  );
  ContextualIdentityService.getPublicIdentities().forEach(identity => {
    let { userContextId } = identity;
    equal(
      Services.cookies.cookieExists(uri.host, "/", TAAR_COOKIE_NAME, {
        userContextId,
      }),
      identity.public,
      "cookie exists"
    );
  });

  // Test the addition of a new container.
  let changed = TestUtils.topicObserved("cookie-changed", (subject, data) => {
    let cookie = subject.QueryInterface(Ci.nsICookie);
    equal(cookie.name, TAAR_COOKIE_NAME, "taar cookie exists");
    equal(cookie.host, uri.host, "cookie exists for host");
    equal(
      cookie.originAttributes.userContextId,
      container.userContextId,
      "cookie userContextId is correct"
    );
    return true;
  });
  let container = ContextualIdentityService.create(
    "New Container",
    "Icon",
    "Color"
  );
  await changed;

  // Test disabling
  Discovery.enabled = false;
  // Wait for the update to remove the cookie.
  await TestUtils.waitForCondition(() => {
    return !Services.cookies.cookieExists(uri.host, "/", TAAR_COOKIE_NAME, {});
  });

  ContextualIdentityService.getPublicIdentities().forEach(identity => {
    let { userContextId } = identity;
    ok(
      !Services.cookies.cookieExists(uri.host, "/", TAAR_COOKIE_NAME, {
        userContextId,
      }),
      "no cookie exists"
    );
  });

  // turn off containers
  Services.prefs.setBoolPref("browser.discovery.containers.enabled", false);

  Discovery.enabled = true;
  await TestUtils.waitForCondition(() => {
    return Services.cookies.cookieExists(uri.host, "/", TAAR_COOKIE_NAME, {});
  });
  // make sure we did not set cookies on containers
  ContextualIdentityService.getPublicIdentities().forEach(identity => {
    let { userContextId } = identity;
    ok(
      !Services.cookies.cookieExists(uri.host, "/", TAAR_COOKIE_NAME, {
        userContextId,
      }),
      "no cookie exists"
    );
  });

  // Make sure clientId changes update discovery
  changed = TestUtils.topicObserved("cookie-changed", (subject, data) => {
    if (data !== "added") {
      return false;
    }
    let cookie = subject.QueryInterface(Ci.nsICookie);
    equal(cookie.name, TAAR_COOKIE_NAME, "taar cookie exists");
    equal(cookie.host, uri.host, "cookie exists for host");
    return true;
  });
  await ClientID.removeClientID();
  await ClientID.getClientID();
  await changed;

  // Make sure disabling telemetry disables discovery.
  Services.prefs.setBoolPref("datareporting.healthreport.uploadEnabled", false);
  await TestUtils.waitForCondition(() => {
    return !Services.cookies.cookieExists(uri.host, "/", TAAR_COOKIE_NAME, {});
  });
});
