/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const REFERENCE_DATE = Date.now();
const LOGIN_USERNAME = "username";
const LOGIN_PASSWORD = "password";
const OLD_HOST = "http://mozilla.org";
const NEW_HOST = "http://mozilla.com";
const FXA_HOST = "chrome://FirefoxAccounts";

async function checkLoginExists(origin, shouldExist) {
  const logins = await Services.logins.searchLoginsAsync({ origin });
  equal(
    logins.length,
    shouldExist ? 1 : 0,
    `Login for origin ${origin} should ${shouldExist ? "" : "not"} be found.`
  );
}

async function addLogin(host, timestamp) {
  await checkLoginExists(host, false);
  let login = Cc["@mozilla.org/login-manager/loginInfo;1"].createInstance(
    Ci.nsILoginInfo
  );
  login.init(host, "", null, LOGIN_USERNAME, LOGIN_PASSWORD);
  login.QueryInterface(Ci.nsILoginMetaInfo);
  login.timePasswordChanged = timestamp;
  await Services.logins.addLoginAsync(login);
  await checkLoginExists(host, true);
}

async function setupPasswords() {
  // Remove all logins if any (included FxAccounts one in case one got captured in
  // a conditioned profile, see Bug 1853617).
  Services.logins.removeAllLogins();
  await addLogin(FXA_HOST, REFERENCE_DATE);
  await addLogin(NEW_HOST, REFERENCE_DATE);
  await addLogin(OLD_HOST, REFERENCE_DATE - 10000);
}

add_task(async function testPasswords() {
  function background() {
    browser.test.onMessage.addListener(async (msg, options) => {
      if (msg == "removeHistory") {
        await browser.browsingData.removePasswords(options);
      } else {
        await browser.browsingData.remove(options, { passwords: true });
      }
      browser.test.sendMessage("passwordsRemoved");
    });
  }

  const extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["browsingData"],
    },
  });

  async function testRemovalMethod(method) {
    // Clear passwords with no since value.
    await setupPasswords();
    extension.sendMessage(method, {});
    await extension.awaitMessage("passwordsRemoved");

    await checkLoginExists(OLD_HOST, false);
    await checkLoginExists(NEW_HOST, false);
    await checkLoginExists(FXA_HOST, true);

    // Clear passwords with recent since value.
    await setupPasswords();
    extension.sendMessage(method, { since: REFERENCE_DATE - 1000 });
    await extension.awaitMessage("passwordsRemoved");

    await checkLoginExists(OLD_HOST, true);
    await checkLoginExists(NEW_HOST, false);
    await checkLoginExists(FXA_HOST, true);

    // Clear passwords with old since value.
    await setupPasswords();
    extension.sendMessage(method, { since: REFERENCE_DATE - 20000 });
    await extension.awaitMessage("passwordsRemoved");

    await checkLoginExists(OLD_HOST, false);
    await checkLoginExists(NEW_HOST, false);
    await checkLoginExists(FXA_HOST, true);
  }

  await extension.startup();

  await testRemovalMethod("removePasswords");
  await testRemovalMethod("remove");

  await extension.unload();
});
