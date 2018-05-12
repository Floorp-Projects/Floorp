/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyGetter(this, "Management", () => {
  // eslint-disable-next-line no-shadow
  const {Management} = ChromeUtils.import("resource://gre/modules/Extension.jsm", {});
  return Management;
});

ChromeUtils.defineModuleGetter(this, "AddonManager",
                               "resource://gre/modules/AddonManager.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");

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

const DEFAULT_NEW_TAB_URL = aboutNewTabService.newTabURL;

add_task(async function test_multiple_extensions_overriding_newtab_page() {
  const NEWTAB_URI_2 = "webext-newtab-1.html";
  const NEWTAB_URI_3 = "webext-newtab-2.html";
  const EXT_2_ID = "ext2@tests.mozilla.org";
  const EXT_3_ID = "ext3@tests.mozilla.org";

  const CONTROLLED_BY_THIS = "controlled_by_this_extension";
  const CONTROLLED_BY_OTHER = "controlled_by_other_extensions";
  const NOT_CONTROLLABLE = "not_controllable";

  function background() {
    browser.test.onMessage.addListener(async msg => {
      switch (msg) {
        case "checkNewTabPage":
          let newTabPage = await browser.browserSettings.newTabPageOverride.get({});
          browser.test.sendMessage("newTabPage", newTabPage);
          break;
        case "trySet":
          let setResult = await browser.browserSettings.newTabPageOverride.set({value: "foo"});
          browser.test.assertFalse(setResult, "Calling newTabPageOverride.set returns false.");
          browser.test.sendMessage("newTabPageSet");
          break;
        case "tryClear":
          let clearResult = await browser.browserSettings.newTabPageOverride.clear({});
          browser.test.assertFalse(clearResult, "Calling newTabPageOverride.clear returns false.");
          browser.test.sendMessage("newTabPageCleared");
          break;
      }
    });
  }

  async function checkNewTabPageOverride(ext, expectedValue, expectedLevelOfControl) {
    ext.sendMessage("checkNewTabPage");
    let newTabPage = await ext.awaitMessage("newTabPage");

    ok(newTabPage.value.endsWith(expectedValue),
       `newTabPageOverride setting returns the expected value ending with: ${expectedValue}.`);
    equal(newTabPage.levelOfControl, expectedLevelOfControl,
          `newTabPageOverride setting returns the expected levelOfControl: ${expectedLevelOfControl}.`);
  }

  let extObj = {
    manifest: {
      "chrome_url_overrides": {},
      permissions: ["browserSettings"],
    },
    useAddonManager: "temporary",
    background,
  };

  let ext1 = ExtensionTestUtils.loadExtension(extObj);

  extObj.manifest.chrome_url_overrides = {newtab: NEWTAB_URI_2};
  extObj.manifest.applications = {gecko: {id: EXT_2_ID}};
  let ext2 = ExtensionTestUtils.loadExtension(extObj);

  extObj.manifest.chrome_url_overrides = {newtab: NEWTAB_URI_3};
  extObj.manifest.applications.gecko.id =  EXT_3_ID;
  let ext3 = ExtensionTestUtils.loadExtension(extObj);

  equal(aboutNewTabService.newTabURL, DEFAULT_NEW_TAB_URL,
        "newTabURL is set to the default.");

  await promiseStartupManager();

  await ext1.startup();
  equal(aboutNewTabService.newTabURL, DEFAULT_NEW_TAB_URL,
        "newTabURL is still set to the default.");

  await checkNewTabPageOverride(ext1, aboutNewTabService.newTabURL, NOT_CONTROLLABLE);

  await ext2.startup();
  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_2),
     "newTabURL is overridden by the second extension.");
  await checkNewTabPageOverride(ext1, NEWTAB_URI_2, CONTROLLED_BY_OTHER);

  // Verify that calling set and clear do nothing.
  ext2.sendMessage("trySet");
  await ext2.awaitMessage("newTabPageSet");
  await checkNewTabPageOverride(ext1, NEWTAB_URI_2, CONTROLLED_BY_OTHER);

  ext2.sendMessage("tryClear");
  await ext2.awaitMessage("newTabPageCleared");
  await checkNewTabPageOverride(ext1, NEWTAB_URI_2, CONTROLLED_BY_OTHER);

  // Disable the second extension.
  let addon = await AddonManager.getAddonByID(EXT_2_ID);
  let disabledPromise = awaitEvent("shutdown");
  await addon.disable();
  await disabledPromise;
  equal(aboutNewTabService.newTabURL, DEFAULT_NEW_TAB_URL,
        "newTabURL url is reset to the default after second extension is disabled.");
  await checkNewTabPageOverride(ext1, aboutNewTabService.newTabURL, NOT_CONTROLLABLE);

  // Re-enable the second extension.
  let enabledPromise = awaitEvent("ready");
  await addon.enable();
  await enabledPromise;
  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_2),
     "newTabURL is overridden by the second extension.");
  await checkNewTabPageOverride(ext2, NEWTAB_URI_2, CONTROLLED_BY_THIS);

  await ext1.unload();
  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_2),
     "newTabURL is still overridden by the second extension.");
  await checkNewTabPageOverride(ext2, NEWTAB_URI_2, CONTROLLED_BY_THIS);

  await ext3.startup();
  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_3),
     "newTabURL is overridden by the third extension.");
  await checkNewTabPageOverride(ext2, NEWTAB_URI_3, CONTROLLED_BY_OTHER);

  // Disable the second extension.
  disabledPromise = awaitEvent("shutdown");
  await addon.disable();
  await disabledPromise;
  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_3),
     "newTabURL is still overridden by the third extension.");
  await checkNewTabPageOverride(ext3, NEWTAB_URI_3, CONTROLLED_BY_THIS);

  // Re-enable the second extension.
  enabledPromise = awaitEvent("ready");
  await addon.enable();
  await enabledPromise;
  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_3),
     "newTabURL is still overridden by the third extension.");
  await checkNewTabPageOverride(ext3, NEWTAB_URI_3, CONTROLLED_BY_THIS);

  await ext3.unload();
  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_2),
     "newTabURL reverts to being overridden by the second extension.");
  await checkNewTabPageOverride(ext2, NEWTAB_URI_2, CONTROLLED_BY_THIS);

  await ext2.unload();
  equal(aboutNewTabService.newTabURL, DEFAULT_NEW_TAB_URL,
        "newTabURL url is reset to the default.");

  await promiseShutdownManager();
});

// Tests that we handle the upgrade/downgrade process correctly
// when an extension is installed temporarily on top of a permanently
// installed one.
add_task(async function test_temporary_installation() {
  const ID = "newtab@tests.mozilla.org";
  const PAGE1 = "page1.html";
  const PAGE2 = "page2.html";

  equal(aboutNewTabService.newTabURL, DEFAULT_NEW_TAB_URL,
        "newTabURL is set to the default.");

  await promiseStartupManager();

  let permanent = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {id: ID},
      },
      chrome_url_overrides: {
        newtab: PAGE1,
      },
    },
    useAddonManager: "permanent",
  });

  await permanent.startup();
  ok(aboutNewTabService.newTabURL.endsWith(PAGE1),
     "newTabURL is overridden by permanent extension.");

  let temporary = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {id: ID},
      },
      chrome_url_overrides: {
        newtab: PAGE2,
      },
    },
    useAddonManager: "temporary",
  });

  await temporary.startup();
  ok(aboutNewTabService.newTabURL.endsWith(PAGE2),
     "newTabURL is overridden by temporary extension.");

  await promiseRestartManager();
  await permanent.awaitStartup();

  ok(aboutNewTabService.newTabURL.endsWith(PAGE1),
     "newTabURL is back to the value set by permanent extension.");

  await permanent.unload();

  equal(aboutNewTabService.newTabURL, DEFAULT_NEW_TAB_URL,
        "newTabURL is set back to the default.");
  await promiseShutdownManager();
});
