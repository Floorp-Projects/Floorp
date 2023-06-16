/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const REFERENCE_DATE = Date.now();
const LOGIN_USERNAME = "username";
const LOGIN_PASSWORD = "password";
const OLD_HOST = "http://mozilla.org";
const NEW_HOST = "http://mozilla.com";
const FXA_HOST = "chrome://FirefoxAccounts";

function checkLoginExists(host, shouldExist) {
  const logins = Services.logins.findLogins(host, "", null);
  equal(
    logins.length,
    shouldExist ? 1 : 0,
    `Login was ${shouldExist ? "" : "not "} found.`
  );
}

async function addLogin(host, timestamp) {
  checkLoginExists(host, false);
  let login = Cc["@mozilla.org/login-manager/loginInfo;1"].createInstance(
    Ci.nsILoginInfo
  );
  login.init(host, "", null, LOGIN_USERNAME, LOGIN_PASSWORD);
  login.QueryInterface(Ci.nsILoginMetaInfo);
  login.timePasswordChanged = timestamp;
  await Services.logins.addLoginAsync(login);
  checkLoginExists(host, true);
}

async function setupPasswords() {
  Services.logins.removeAllUserFacingLogins();
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

    checkLoginExists(OLD_HOST, false);
    checkLoginExists(NEW_HOST, false);
    checkLoginExists(FXA_HOST, true);

    // Clear passwords with recent since value.
    await setupPasswords();
    extension.sendMessage(method, { since: REFERENCE_DATE - 1000 });
    await extension.awaitMessage("passwordsRemoved");

    checkLoginExists(OLD_HOST, true);
    checkLoginExists(NEW_HOST, false);
    checkLoginExists(FXA_HOST, true);

    // Clear passwords with old since value.
    await setupPasswords();
    extension.sendMessage(method, { since: REFERENCE_DATE - 20000 });
    await extension.awaitMessage("passwordsRemoved");

    checkLoginExists(OLD_HOST, false);
    checkLoginExists(NEW_HOST, false);
    checkLoginExists(FXA_HOST, true);
  }

  await extension.startup();

  await testRemovalMethod("removePasswords");
  await testRemovalMethod("remove");

  await extension.unload();
});
