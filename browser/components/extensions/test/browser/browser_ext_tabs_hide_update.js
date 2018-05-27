"use strict";

ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm", this);

AddonTestUtils.initMochitest(this);

const ID = "@test-tabs-addon";

async function updateExtension(ID, options) {
  let xpi = AddonTestUtils.createTempWebExtensionFile(options);
  await Promise.all([
    AddonTestUtils.promiseWebExtensionStartup(ID),
    AddonManager.installTemporaryAddon(xpi),
  ]);
}

async function disableExtension(ID) {
  let disabledPromise = awaitEvent("shutdown", ID);
  let addon = await AddonManager.getAddonByID(ID);
  await addon.disable();
  await disabledPromise;
}

function getExtension() {
  async function background() {
    let tabs = await browser.tabs.query({url: "http://example.com/"});
    let testTab = tabs[0];

    browser.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
      if ("hidden" in changeInfo) {
        browser.test.assertEq(tabId, testTab.id, "correct tab was hidden");
        browser.test.assertTrue(changeInfo.hidden, "tab is hidden");
        browser.test.sendMessage("changeInfo");
      }
    });

    let hidden = await browser.tabs.hide(testTab.id);
    browser.test.assertEq(hidden[0], testTab.id, "tabs.hide hide the tab");
    tabs = await browser.tabs.query({hidden: true});
    browser.test.assertEq(tabs[0].id, testTab.id, "tabs.query result was hidden");
    browser.test.sendMessage("ready");
  }

  let extdata = {
    manifest: {
      version: "1.0",
      "applications": {
        "gecko": {
          "id": ID,
        },
      },
      permissions: ["tabs", "tabHide"],
    },
    background,
    useAddonManager: "temporary",
  };
  return ExtensionTestUtils.loadExtension(extdata);
}

// Test our update handling.  Currently this means any hidden tabs will be
// shown when a tabHide extension is shutdown.  We additionally test the
// tabs.onUpdated listener gets called with hidden state changes.
add_task(async function test_tabs_update() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
  await BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[0]);

  const extension = getExtension();
  await extension.startup();

  // test onUpdated
  await Promise.all([
    extension.awaitMessage("ready"),
    extension.awaitMessage("changeInfo"),
  ]);
  Assert.ok(tab.hidden, "Tab is hidden by extension");

  // Test that update doesn't hide tabs when tabHide permission is present.
  let extdata = {
    manifest: {
      version: "2.0",
      "applications": {
        "gecko": {
          "id": ID,
        },
      },
      permissions: ["tabs", "tabHide"],
    },
  };
  await updateExtension(ID, extdata);
  Assert.ok(tab.hidden, "Tab is hidden hidden after update");

  // Test that update does hide tabs when tabHide permission is removed.
  extdata.manifest = {
    version: "3.0",
    "applications": {
      "gecko": {
        "id": ID,
      },
    },
    permissions: ["tabs"],
  };
  await updateExtension(ID, extdata);
  Assert.ok(!tab.hidden, "Tab is not hidden hidden after update");

  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});


// Test our update handling.  Currently this means any hidden tabs will be
// shown when a tabHide extension is shutdown.  We additionally test the
// tabs.onUpdated listener gets called with hidden state changes.
add_task(async function test_tabs_disable() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
  await BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[0]);

  const extension = getExtension();
  await extension.startup();

  // test onUpdated
  await Promise.all([
    extension.awaitMessage("ready"),
    extension.awaitMessage("changeInfo"),
  ]);
  Assert.ok(tab.hidden, "Tab is hidden by extension");

  // Test that disable does hide tabs.
  await disableExtension(ID);
  Assert.ok(!tab.hidden, "Tab is not hidden hidden after disable");

  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});
