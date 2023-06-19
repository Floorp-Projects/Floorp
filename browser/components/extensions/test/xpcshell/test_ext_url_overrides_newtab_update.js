/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { AboutNewTab } = ChromeUtils.importESModule(
  "resource:///modules/AboutNewTab.sys.mjs"
);

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

const {
  createAppInfo,
  createTempWebExtensionFile,
  promiseCompleteAllInstalls,
  promiseFindAddonUpdates,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

AddonTestUtils.init(this);

// Allow for unsigned addons.
AddonTestUtils.overrideCertDB();

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "42", "42");

add_task(async function test_url_overrides_newtab_update() {
  const EXTENSION_ID = "test_url_overrides_update@tests.mozilla.org";
  const NEWTAB_URI = "webext-newtab-1.html";
  const PREF_EM_CHECK_UPDATE_SECURITY = "extensions.checkUpdateSecurity";

  const testServer = createHttpServer();
  const port = testServer.identity.primaryPort;

  // The test extension uses an insecure update url.
  Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

  testServer.registerPathHandler("/test_update.json", (request, response) => {
    response.write(`{
      "addons": {
        "${EXTENSION_ID}": {
          "updates": [
            {
              "version": "2.0",
              "update_link": "http://localhost:${port}/addons/test_url_overrides-2.0.xpi"
            }
          ]
        }
      }
    }`);
  });

  let webExtensionFile = createTempWebExtensionFile({
    manifest: {
      version: "2.0",
      browser_specific_settings: {
        gecko: {
          id: EXTENSION_ID,
        },
      },
    },
  });

  testServer.registerFile(
    "/addons/test_url_overrides-2.0.xpi",
    webExtensionFile
  );

  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      version: "1.0",
      browser_specific_settings: {
        gecko: {
          id: EXTENSION_ID,
          update_url: `http://localhost:${port}/test_update.json`,
        },
      },
      chrome_url_overrides: { newtab: NEWTAB_URI },
    },
  });

  let defaultNewTabURL = AboutNewTab.newTabURL;
  equal(
    AboutNewTab.newTabURL,
    defaultNewTabURL,
    `Default newtab url is ${defaultNewTabURL}.`
  );

  await extension.startup();

  equal(
    extension.version,
    "1.0",
    "The installed addon has the expected version."
  );
  ok(
    AboutNewTab.newTabURL.endsWith(NEWTAB_URI),
    "Newtab url is overridden by the extension."
  );

  let update = await promiseFindAddonUpdates(extension.addon);
  let install = update.updateAvailable;

  await promiseCompleteAllInstalls([install]);

  await extension.awaitStartup();

  equal(
    extension.version,
    "2.0",
    "The updated addon has the expected version."
  );
  equal(
    AboutNewTab.newTabURL,
    defaultNewTabURL,
    "Newtab url reverted to the default after update."
  );

  await extension.unload();

  await promiseShutdownManager();
});
