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
  const NEWTAB_URI_1 = "webext-newtab-1.html";
  const NEWTAB_URI_2 = "webext-newtab-2.html";
  const EXT_2_ID = "ext2@tests.mozilla.org";

  equal(aboutNewTabService.newTabURL, "about:newtab",
     "Default newtab url is about:newtab");

  await promiseStartupManager();

  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {"chrome_url_overrides": {}},
    useAddonManager: "temporary",
  });

  let ext2 = ExtensionTestUtils.loadExtension({
    manifest: {
      "chrome_url_overrides": {newtab: NEWTAB_URI_1},
      applications: {
        gecko: {
          id: EXT_2_ID,
        },
      },
    },
    useAddonManager: "temporary",
  });

  let ext3 = ExtensionTestUtils.loadExtension({
    manifest: {"chrome_url_overrides": {newtab: NEWTAB_URI_2}},
    useAddonManager: "temporary",
  });

  await ext1.startup();
  equal(aboutNewTabService.newTabURL, "about:newtab",
       "Default newtab url is still about:newtab");

  await ext2.startup();
  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_1),
     "Newtab url is overriden by the second extension.");

  // Disable the second extension.
  let addon = await AddonManager.getAddonByID(EXT_2_ID);
  let disabledPromise = awaitEvent("shutdown");
  addon.userDisabled = true;
  await disabledPromise;
  equal(aboutNewTabService.newTabURL, "about:newtab",
        "Newtab url is about:newtab after second extension is disabled.");

  // Re-enable the second extension.
  let enabledPromise = awaitEvent("ready");
  addon.userDisabled = false;
  await enabledPromise;
  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_1),
     "Newtab url is overriden by the second extension.");

  await ext1.unload();
  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_1),
     "Newtab url is still overriden by the second extension.");

  await ext3.startup();
  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_2),
   "Newtab url is overriden by the third extension.");

  // Disable the second extension.
  disabledPromise = awaitEvent("shutdown");
  addon.userDisabled = true;
  await disabledPromise;
  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_2),
   "Newtab url is still overriden by the third extension.");

  // Re-enable the second extension.
  enabledPromise = awaitEvent("ready");
  addon.userDisabled = false;
  await enabledPromise;
  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_2),
   "Newtab url is still overriden by the third extension.");

  await ext3.unload();
  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI_1),
     "Newtab url reverts to being overriden by the second extension.");

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
