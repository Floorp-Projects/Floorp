/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  Management: "resource://gre/modules/Extension.sys.mjs",
});

const { AboutNewTab } = ChromeUtils.importESModule(
  "resource:///modules/AboutNewTab.sys.mjs"
);

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

const {
  createAppInfo,
  promiseRestartManager,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "42", "42");

function awaitEvent(eventName) {
  return new Promise(resolve => {
    Management.once(eventName, (e, ...args) => resolve(...args));
  });
}

const DEFAULT_NEW_TAB_URL = AboutNewTab.newTabURL;

add_task(async function test_multiple_extensions_overriding_newtab_page() {
  const NEWTAB_URI_2 = "webext-newtab-1.html";
  const NEWTAB_URI_3 = "webext-newtab-2.html";
  const EXT_2_ID = "ext2@tests.mozilla.org";
  const EXT_3_ID = "ext3@tests.mozilla.org";

  const CONTROLLED_BY_THIS = "controlled_by_this_extension";
  const CONTROLLED_BY_OTHER = "controlled_by_other_extensions";
  const NOT_CONTROLLABLE = "not_controllable";

  const NEW_TAB_PRIVATE_ALLOWED = "browser.newtab.privateAllowed";
  const NEW_TAB_EXTENSION_CONTROLLED = "browser.newtab.extensionControlled";

  function background() {
    browser.test.onMessage.addListener(async msg => {
      switch (msg) {
        case "checkNewTabPage":
          let newTabPage = await browser.browserSettings.newTabPageOverride.get(
            {}
          );
          browser.test.sendMessage("newTabPage", newTabPage);
          break;
        case "trySet":
          let setResult = await browser.browserSettings.newTabPageOverride.set({
            value: "foo",
          });
          browser.test.assertFalse(
            setResult,
            "Calling newTabPageOverride.set returns false."
          );
          browser.test.sendMessage("newTabPageSet");
          break;
        case "tryClear":
          let clearResult =
            await browser.browserSettings.newTabPageOverride.clear({});
          browser.test.assertFalse(
            clearResult,
            "Calling newTabPageOverride.clear returns false."
          );
          browser.test.sendMessage("newTabPageCleared");
          break;
      }
    });
  }

  async function checkNewTabPageOverride(
    ext,
    expectedValue,
    expectedLevelOfControl
  ) {
    ext.sendMessage("checkNewTabPage");
    let newTabPage = await ext.awaitMessage("newTabPage");

    ok(
      newTabPage.value.endsWith(expectedValue),
      `newTabPageOverride setting returns the expected value ending with: ${expectedValue}.`
    );
    equal(
      newTabPage.levelOfControl,
      expectedLevelOfControl,
      `newTabPageOverride setting returns the expected levelOfControl: ${expectedLevelOfControl}.`
    );
  }

  function verifyNewTabSettings(ext, expectedLevelOfControl) {
    if (expectedLevelOfControl !== NOT_CONTROLLABLE) {
      // Verify the preferences are set as expected.
      let policy = WebExtensionPolicy.getByID(ext.id);
      equal(
        policy && policy.privateBrowsingAllowed,
        Services.prefs.getBoolPref(NEW_TAB_PRIVATE_ALLOWED),
        "private browsing flag set correctly"
      );
      ok(
        Services.prefs.getBoolPref(NEW_TAB_EXTENSION_CONTROLLED),
        `extension controlled flag set correctly`
      );
    } else {
      ok(
        !Services.prefs.prefHasUserValue(NEW_TAB_PRIVATE_ALLOWED),
        "controlled flag reset"
      );
      ok(
        !Services.prefs.prefHasUserValue(NEW_TAB_EXTENSION_CONTROLLED),
        "controlled flag reset"
      );
    }
  }

  let extObj = {
    manifest: {
      chrome_url_overrides: {},
      permissions: ["browserSettings"],
    },
    useAddonManager: "temporary",
    background,
  };

  let ext1 = ExtensionTestUtils.loadExtension(extObj);

  extObj.manifest.chrome_url_overrides = { newtab: NEWTAB_URI_2 };
  extObj.manifest.browser_specific_settings = { gecko: { id: EXT_2_ID } };
  let ext2 = ExtensionTestUtils.loadExtension(extObj);

  extObj.manifest.chrome_url_overrides = { newtab: NEWTAB_URI_3 };
  extObj.manifest.browser_specific_settings.gecko.id = EXT_3_ID;
  extObj.incognitoOverride = "spanning";
  let ext3 = ExtensionTestUtils.loadExtension(extObj);

  equal(
    AboutNewTab.newTabURL,
    DEFAULT_NEW_TAB_URL,
    "newTabURL is set to the default."
  );

  await promiseStartupManager();

  await ext1.startup();
  equal(
    AboutNewTab.newTabURL,
    DEFAULT_NEW_TAB_URL,
    "newTabURL is still set to the default."
  );

  await checkNewTabPageOverride(ext1, AboutNewTab.newTabURL, NOT_CONTROLLABLE);
  verifyNewTabSettings(ext1, NOT_CONTROLLABLE);

  await ext2.startup();
  ok(
    AboutNewTab.newTabURL.endsWith(NEWTAB_URI_2),
    "newTabURL is overridden by the second extension."
  );
  await checkNewTabPageOverride(ext1, NEWTAB_URI_2, CONTROLLED_BY_OTHER);
  verifyNewTabSettings(ext2, CONTROLLED_BY_THIS);

  // Verify that calling set and clear do nothing.
  ext2.sendMessage("trySet");
  await ext2.awaitMessage("newTabPageSet");
  await checkNewTabPageOverride(ext1, NEWTAB_URI_2, CONTROLLED_BY_OTHER);
  verifyNewTabSettings(ext2, CONTROLLED_BY_THIS);

  ext2.sendMessage("tryClear");
  await ext2.awaitMessage("newTabPageCleared");
  await checkNewTabPageOverride(ext1, NEWTAB_URI_2, CONTROLLED_BY_OTHER);
  verifyNewTabSettings(ext2, CONTROLLED_BY_THIS);

  // Disable the second extension.
  let addon = await AddonManager.getAddonByID(EXT_2_ID);
  let disabledPromise = awaitEvent("shutdown");
  await addon.disable();
  await disabledPromise;
  equal(
    AboutNewTab.newTabURL,
    DEFAULT_NEW_TAB_URL,
    "newTabURL url is reset to the default after second extension is disabled."
  );
  await checkNewTabPageOverride(ext1, AboutNewTab.newTabURL, NOT_CONTROLLABLE);
  verifyNewTabSettings(ext1, NOT_CONTROLLABLE);

  // Re-enable the second extension.
  let enabledPromise = awaitEvent("ready");
  await addon.enable();
  await enabledPromise;
  ok(
    AboutNewTab.newTabURL.endsWith(NEWTAB_URI_2),
    "newTabURL is overridden by the second extension."
  );
  await checkNewTabPageOverride(ext2, NEWTAB_URI_2, CONTROLLED_BY_THIS);
  verifyNewTabSettings(ext2, CONTROLLED_BY_THIS);

  await ext1.unload();
  ok(
    AboutNewTab.newTabURL.endsWith(NEWTAB_URI_2),
    "newTabURL is still overridden by the second extension."
  );
  await checkNewTabPageOverride(ext2, NEWTAB_URI_2, CONTROLLED_BY_THIS);
  verifyNewTabSettings(ext2, CONTROLLED_BY_THIS);

  await ext3.startup();
  ok(
    AboutNewTab.newTabURL.endsWith(NEWTAB_URI_3),
    "newTabURL is overridden by the third extension."
  );
  await checkNewTabPageOverride(ext2, NEWTAB_URI_3, CONTROLLED_BY_OTHER);
  verifyNewTabSettings(ext3, CONTROLLED_BY_THIS);

  // Disable the second extension.
  disabledPromise = awaitEvent("shutdown");
  await addon.disable();
  await disabledPromise;
  ok(
    AboutNewTab.newTabURL.endsWith(NEWTAB_URI_3),
    "newTabURL is still overridden by the third extension."
  );
  await checkNewTabPageOverride(ext3, NEWTAB_URI_3, CONTROLLED_BY_THIS);
  verifyNewTabSettings(ext3, CONTROLLED_BY_THIS);

  // Re-enable the second extension.
  enabledPromise = awaitEvent("ready");
  await addon.enable();
  await enabledPromise;
  ok(
    AboutNewTab.newTabURL.endsWith(NEWTAB_URI_3),
    "newTabURL is still overridden by the third extension."
  );
  await checkNewTabPageOverride(ext3, NEWTAB_URI_3, CONTROLLED_BY_THIS);
  verifyNewTabSettings(ext3, CONTROLLED_BY_THIS);

  await ext3.unload();
  ok(
    AboutNewTab.newTabURL.endsWith(NEWTAB_URI_2),
    "newTabURL reverts to being overridden by the second extension."
  );
  await checkNewTabPageOverride(ext2, NEWTAB_URI_2, CONTROLLED_BY_THIS);
  verifyNewTabSettings(ext2, CONTROLLED_BY_THIS);

  await ext2.unload();
  equal(
    AboutNewTab.newTabURL,
    DEFAULT_NEW_TAB_URL,
    "newTabURL url is reset to the default."
  );
  ok(
    !Services.prefs.prefHasUserValue(NEW_TAB_PRIVATE_ALLOWED),
    "controlled flag reset"
  );
  ok(
    !Services.prefs.prefHasUserValue(NEW_TAB_EXTENSION_CONTROLLED),
    "controlled flag reset"
  );

  await promiseShutdownManager();
});

// Tests that we handle the upgrade/downgrade process correctly
// when an extension is installed temporarily on top of a permanently
// installed one.
add_task(async function test_temporary_installation() {
  const ID = "newtab@tests.mozilla.org";
  const PAGE1 = "page1.html";
  const PAGE2 = "page2.html";

  equal(
    AboutNewTab.newTabURL,
    DEFAULT_NEW_TAB_URL,
    "newTabURL is set to the default."
  );

  await promiseStartupManager();

  let permanent = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: {
        gecko: { id: ID },
      },
      chrome_url_overrides: {
        newtab: PAGE1,
      },
    },
    useAddonManager: "permanent",
  });

  await permanent.startup();
  ok(
    AboutNewTab.newTabURL.endsWith(PAGE1),
    "newTabURL is overridden by permanent extension."
  );

  let temporary = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: {
        gecko: { id: ID },
      },
      chrome_url_overrides: {
        newtab: PAGE2,
      },
    },
    useAddonManager: "temporary",
  });

  await temporary.startup();
  ok(
    AboutNewTab.newTabURL.endsWith(PAGE2),
    "newTabURL is overridden by temporary extension."
  );

  await promiseRestartManager();
  await permanent.awaitStartup();

  ok(
    AboutNewTab.newTabURL.endsWith(PAGE1),
    "newTabURL is back to the value set by permanent extension."
  );

  await permanent.unload();

  equal(
    AboutNewTab.newTabURL,
    DEFAULT_NEW_TAB_URL,
    "newTabURL is set back to the default."
  );
  await promiseShutdownManager();
});
