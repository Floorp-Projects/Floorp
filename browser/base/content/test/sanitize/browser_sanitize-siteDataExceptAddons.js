/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.defineESModuleGetters(this, {
  PermissionTestUtils: "resource://testing-common/PermissionTestUtils.sys.mjs",
});

async function createAddon() {
  let extData = {
    manifest: {
      name: "Test Extension",
      permissions: ["unlimitedStorage"],
    },
    files: {
      "testpage.html": "<h1>Extension Test Page</h1>",
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extData);
  await extension.startup();

  return extension;
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.sanitize.useOldClearHistoryDialog", false]],
  });
});

async function checkAddonPermissionClearingWithPref(prefs, expectedValue) {
  let extension = await createAddon();

  // Use the extension policy to get the principal object
  let policy = WebExtensionPolicy.getByID(extension.id);
  let extensionPrincipal = policy.extension.principal;

  // The addon gets a persistent-storage permission because of
  // "unlimitedStorage" being present in loadExtension
  Assert.equal(
    PermissionTestUtils.testExactPermission(
      extensionPrincipal,
      "persistent-storage"
    ),
    Services.perms.ALLOW_ACTION,
    "Addon persistent permission exists"
  );

  // Add a site permission that is expected to be cleared
  let siteURI = Services.io.newURI("https://testdomain.org");

  PermissionTestUtils.add(
    siteURI,
    "persistent-storage",
    Services.perms.ALLOW_ACTION
  );

  // Lets clear cookies and site data
  let clearHistoryDialog = new ClearHistoryDialogHelper();

  clearHistoryDialog.onload = function () {
    this.checkPrefCheckbox("cookiesAndStorage", prefs.cookiesAndStorage);
    this.checkPrefCheckbox("siteSettings", prefs.siteSettings);

    this.acceptDialog();
  };

  clearHistoryDialog.open();
  await clearHistoryDialog.promiseClosed;

  Assert.equal(
    PermissionTestUtils.testExactPermission(
      extensionPrincipal,
      "persistent-storage"
    ),
    expectedValue,
    "Addon permission persists"
  );

  Assert.equal(
    PermissionTestUtils.testExactPermission(siteURI, "persistent-storage"),
    Services.perms.UNKNOWN_ACTION,
    "Site permission removed"
  );

  // cleanup
  Services.perms.removeAll();
  await extension.unload();
}

add_task(async function test_clearing_cookiesAndStorage() {
  await checkAddonPermissionClearingWithPref(
    { cookiesAndStorage: true, siteSettings: false },
    Services.perms.ALLOW_ACTION
  );
});

add_task(async function test_clearing_siteSettings() {
  await checkAddonPermissionClearingWithPref(
    { cookiesAndStorage: false, siteSettings: true },
    Services.perms.UNKNOWN_ACTION
  );
});
