/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyGetter(this, "Management", () => {
  const {Management} = Cu.import("resource://gre/modules/Extension.jsm", {});
  return Management;
});

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

Cu.import("resource://testing-common/AddonTestUtils.jsm");

const {
  createAppInfo,
  promiseRestartManager,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

function awaitEvent(eventName) {
  return new Promise(resolve => {
    Management.once(eventName, (e, ...args) => resolve(...args));
  });
}

add_task(async function test_multiple_extensions_overriding_newtab_page() {
  const NEWTAB_URI_2 = "webext-newtab-1.html";
  const NEWTAB_URI_3 = "webext-newtab-2.html";
  const EXT_2_ID = "ext2@tests.mozilla.org";
  const EXT_3_ID = "ext3@tests.mozilla.org";

  const CONTROLLABLE = "controllable_by_this_extension";
  const CONTROLLED_BY_THIS = "controlled_by_this_extension";
  const CONTROLLED_BY_OTHER = "controlled_by_other_extensions";

  function background() {
    browser.test.onMessage.addListener(async msg => {
      switch (msg) {
        case "checkNewTabPage":
          let newTabPage = await browser.browserSettings.newTabPageOverride.get({});
          browser.test.sendMessage("newTabPage", newTabPage);
          break;
        case "trySet":
          await browser.browserSettings.newTabPageOverride.set({value: "foo"});
          browser.test.sendMessage("newTabPageSet");
          break;
        case "tryClear":
          await browser.browserSettings.newTabPageOverride.clear({});
          browser.test.sendMessage("newTabPageCleared");
          break;
      }
    });
  }

  async function checkNewTabPageOverride(ext, expectedValue, expectedLevelOfControl) {
    ext.sendMessage("checkNewTabPage");
    let newTabPage = await ext.awaitMessage("newTabPage");

    if (expectedValue) {
      ok(newTabPage.value.endsWith(expectedValue),
         `newTabPageOverride setting returns the expected value ending with: ${expectedValue}.`);
    } else {
      equal(newTabPage.value, expectedValue,
            `newTabPageOverride setting returns the expected value: ${expectedValue}.`);
    }
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

  equal(aboutNewTabService.newTabURL, "about:newtab",
     "Default newtab url is about:newtab");

  await promiseStartupManager();

  await ext1.startup();
  equal(aboutNewTabService.newTabURL, "about:newtab",
       "Default newtab url is still about:newtab");

  await checkNewTabPageOverride(ext1, null, CONTROLLABLE);

  await ext2.startup();
  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_2),
     "Newtab url is overriden by the second extension.");
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
  addon.userDisabled = true;
  await disabledPromise;
  equal(aboutNewTabService.newTabURL, "about:newtab",
        "Newtab url is about:newtab after second extension is disabled.");
  await checkNewTabPageOverride(ext1, null, CONTROLLABLE);

  // Re-enable the second extension.
  let enabledPromise = awaitEvent("ready");
  addon.userDisabled = false;
  await enabledPromise;
  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_2),
     "Newtab url is overriden by the second extension.");
  await checkNewTabPageOverride(ext2, NEWTAB_URI_2, CONTROLLED_BY_THIS);

  await ext1.unload();
  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_2),
     "Newtab url is still overriden by the second extension.");
  await checkNewTabPageOverride(ext2, NEWTAB_URI_2, CONTROLLED_BY_THIS);

  await ext3.startup();
  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_3),
   "Newtab url is overriden by the third extension.");
  await checkNewTabPageOverride(ext2, NEWTAB_URI_3, CONTROLLED_BY_OTHER);

  // Disable the second extension.
  disabledPromise = awaitEvent("shutdown");
  addon.userDisabled = true;
  await disabledPromise;
  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_3),
   "Newtab url is still overriden by the third extension.");
  await checkNewTabPageOverride(ext3, NEWTAB_URI_3, CONTROLLED_BY_THIS);

  // Re-enable the second extension.
  enabledPromise = awaitEvent("ready");
  addon.userDisabled = false;
  await enabledPromise;
  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_3),
   "Newtab url is still overriden by the third extension.");
  await checkNewTabPageOverride(ext3, NEWTAB_URI_3, CONTROLLED_BY_THIS);

  await ext3.unload();
  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_2),
     "Newtab url reverts to being overriden by the second extension.");
  await checkNewTabPageOverride(ext2, NEWTAB_URI_2, CONTROLLED_BY_THIS);

  await ext2.unload();
  equal(aboutNewTabService.newTabURL, "about:newtab",
     "Newtab url is reset to about:newtab");

  await promiseShutdownManager();
});

// Tests that we handle the upgrade/downgrade process correctly
// when an extension is installed temporarily on top of a permanently
// installed one.
add_task(async function test_temporary_installation() {
  const ID = "newtab@tests.mozilla.org";
  const PAGE1 = "page1.html";
  const PAGE2 = "page2.html";

  equal(aboutNewTabService.newTabURL, "about:newtab",
        "Default newtab url is about:newtab");

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
     "newtab url is overridden by permanent extension");

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
     "newtab url is overridden by temporary extension");

  await promiseRestartManager();
  await permanent.awaitStartup();

  ok(aboutNewTabService.newTabURL.endsWith(PAGE1),
     "newtab url is back to the value set by permanent extension");

  await permanent.unload();

  equal(aboutNewTabService.newTabURL, "about:newtab",
        "newtab url is back to default about:newtab");
  await promiseShutdownManager();
});
