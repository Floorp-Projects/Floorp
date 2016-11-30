/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "setTimeout",
                                  "resource://gre/modules/Timer.jsm");

const COOKIE = {
  host: "example.com",
  name: "test_cookie",
  path: "/",
};

function addCookie(cookie) {
  Services.cookies.add(cookie.host, cookie.path, cookie.name, "test", false, false, false, Date.now() / 1000 + 10000);
  ok(Services.cookies.cookieExists(cookie), `Cookie ${cookie.name} was created.`);
}

add_task(async function testCookies() {
  function background() {
    browser.test.onMessage.addListener(async (msg, options) => {
      if (msg == "removeCookies") {
        await browser.browsingData.removeCookies(options);
      } else {
        await browser.browsingData.remove(options, {cookies: true});
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
    // Add a cookie which will end up with an older creationTime.
    let oldCookie = Object.assign({}, COOKIE, {name: Date.now()});
    addCookie(oldCookie);
    await new Promise(resolve => setTimeout(resolve, 10));
    let since = Date.now();

    // Add a cookie which will end up with a more recent creationTime.
    addCookie(COOKIE);

    // Clear cookies with a since value.
    extension.sendMessage(method, {since});
    await extension.awaitMessage("cookiesRemoved");

    ok(Services.cookies.cookieExists(oldCookie), "Old cookie was not removed.");
    ok(!Services.cookies.cookieExists(COOKIE), "Recent cookie was removed.");

    // Clear cookies with no since value and valid originTypes.
    addCookie(COOKIE);
    extension.sendMessage(
      method,
      {originTypes: {unprotectedWeb: true, protectedWeb: false}});
    await extension.awaitMessage("cookiesRemoved");

    ok(!Services.cookies.cookieExists(COOKIE), `Cookie ${COOKIE.name}  was removed.`);
    ok(!Services.cookies.cookieExists(oldCookie), `Cookie ${oldCookie.name}  was removed.`);
  }

  await extension.startup();

  await testRemovalMethod("removeCookies");
  await testRemovalMethod("remove");

  await extension.unload();
});
