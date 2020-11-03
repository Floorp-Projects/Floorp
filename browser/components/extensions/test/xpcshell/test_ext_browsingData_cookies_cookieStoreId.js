/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

// "Normal" cookie
const COOKIE_NORMAL = {
  host: "example.com",
  name: "test_cookie",
  path: "/",
  originAttributes: {},
};
// Private browsing cookie
const COOKIE_PRIVATE = {
  host: "example.net",
  name: "test_cookie",
  path: "/",
  originAttributes: {
    privateBrowsingId: 1,
  },
};
// "firefox-container-1" cookie
const COOKIE_CONTAINER = {
  host: "example.org",
  name: "test_cookie",
  path: "/",
  originAttributes: {
    userContextId: 1,
  },
};

function cookieExists(cookie) {
  return Services.cookies.cookieExists(
    cookie.host,
    cookie.path,
    cookie.name,
    cookie.originAttributes
  );
}

function addCookie(cookie) {
  const THE_FUTURE = Date.now() + 5 * 60;

  Services.cookies.add(
    cookie.host,
    cookie.path,
    cookie.name,
    "test",
    false,
    false,
    false,
    THE_FUTURE,
    cookie.originAttributes,
    Ci.nsICookie.SAMESITE_NONE,
    Ci.nsICookie.SCHEME_HTTPS
  );

  ok(cookieExists(cookie), `Cookie ${cookie.name} was created.`);
}

async function setUpCookies() {
  Services.cookies.removeAll();

  addCookie(COOKIE_NORMAL);
  addCookie(COOKIE_PRIVATE);
  addCookie(COOKIE_CONTAINER);
}

add_task(async function testCookies() {
  Services.prefs.setBoolPref("privacy.userContext.enabled", true);

  function background() {
    browser.test.onMessage.addListener(async (msg, options) => {
      if (msg == "removeCookies") {
        await browser.browsingData.removeCookies(options);
      } else {
        await browser.browsingData.remove(options, { cookies: true });
      }
      browser.test.sendMessage("cookiesRemoved");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["browsingData"],
    },
  });

  async function testRemovalMethod(method) {
    // Clear only "normal"/default cookies.
    await setUpCookies();

    extension.sendMessage(method, { cookieStoreId: "firefox-default" });
    await extension.awaitMessage("cookiesRemoved");

    ok(!cookieExists(COOKIE_NORMAL), "Normal cookie was removed");
    ok(cookieExists(COOKIE_PRIVATE), "Private cookie was not removed");
    ok(cookieExists(COOKIE_CONTAINER), "Container cookie was not removed");

    // Clear container cookie
    await setUpCookies();

    extension.sendMessage(method, { cookieStoreId: "firefox-container-1" });
    await extension.awaitMessage("cookiesRemoved");

    ok(cookieExists(COOKIE_NORMAL), "Normal cookie was not removed");
    ok(cookieExists(COOKIE_PRIVATE), "Private cookie was not removed");
    ok(!cookieExists(COOKIE_CONTAINER), "Container cookie was removed");

    // Clear private cookie
    await setUpCookies();

    extension.sendMessage(method, { cookieStoreId: "firefox-private" });
    await extension.awaitMessage("cookiesRemoved");

    ok(cookieExists(COOKIE_NORMAL), "Normal cookie was not removed");
    ok(!cookieExists(COOKIE_PRIVATE), "Private cookie was removed");
    ok(cookieExists(COOKIE_CONTAINER), "Container cookie was not removed");

    // Clear container cookie with correct hostname
    await setUpCookies();

    extension.sendMessage(method, {
      cookieStoreId: "firefox-container-1",
      hostnames: ["example.org"],
    });
    await extension.awaitMessage("cookiesRemoved");

    ok(cookieExists(COOKIE_NORMAL), "Normal cookie was not removed");
    ok(cookieExists(COOKIE_PRIVATE), "Private cookie was not removed");
    ok(!cookieExists(COOKIE_CONTAINER), "Container cookie was removed");

    // Clear container cookie with incorrect hostname; nothing is removed
    await setUpCookies();

    extension.sendMessage(method, {
      cookieStoreId: "firefox-container-1",
      hostnames: ["example.com"],
    });
    await extension.awaitMessage("cookiesRemoved");

    ok(cookieExists(COOKIE_NORMAL), "Normal cookie was not removed");
    ok(cookieExists(COOKIE_PRIVATE), "Private cookie was not removed");
    ok(cookieExists(COOKIE_CONTAINER), "Container cookie was not removed");

    // Clear private cookie with correct hostname
    await setUpCookies();

    extension.sendMessage(method, {
      cookieStoreId: "firefox-private",
      hostnames: ["example.net"],
    });
    await extension.awaitMessage("cookiesRemoved");

    ok(cookieExists(COOKIE_NORMAL), "Normal cookie was not removed");
    ok(!cookieExists(COOKIE_PRIVATE), "Private cookie was removed");
    ok(cookieExists(COOKIE_CONTAINER), "Container cookie was not removed");

    // Clear private cookie with incorrect hostname; nothing is removed
    await setUpCookies();

    extension.sendMessage(method, {
      cookieStoreId: "firefox-private",
      hostnames: ["example.com"],
    });
    await extension.awaitMessage("cookiesRemoved");

    ok(cookieExists(COOKIE_NORMAL), "Normal cookie was not removed");
    ok(cookieExists(COOKIE_PRIVATE), "Private cookie was not removed");
    ok(cookieExists(COOKIE_CONTAINER), "Container cookie was not removed");

    // Clear private cookie by hostname
    await setUpCookies();

    extension.sendMessage(method, {
      hostnames: ["example.net"],
    });
    await extension.awaitMessage("cookiesRemoved");

    ok(cookieExists(COOKIE_NORMAL), "Normal cookie was not removed");
    ok(!cookieExists(COOKIE_PRIVATE), "Private cookie was removed");
    ok(cookieExists(COOKIE_CONTAINER), "Container cookie was not removed");
  }

  await extension.startup();

  await testRemovalMethod("removeCookies");
  await testRemovalMethod("remove");

  await extension.unload();
});
