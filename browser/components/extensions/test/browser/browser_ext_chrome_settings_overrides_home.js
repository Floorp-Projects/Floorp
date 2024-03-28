/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

requestLongerTimeout(4);

ChromeUtils.defineESModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  ExtensionControlledPopup:
    "resource:///modules/ExtensionControlledPopup.sys.mjs",
  ExtensionSettingsStore:
    "resource://gre/modules/ExtensionSettingsStore.sys.mjs",
  HomePage: "resource:///modules/HomePage.sys.mjs",
});

// Named this way so they correspond to the extensions
const HOME_URI_2 = "http://example.com/";
const HOME_URI_3 = "http://example.org/";
const HOME_URI_4 = "http://example.net/";

const CONTROLLED_BY_THIS = "controlled_by_this_extension";
const CONTROLLED_BY_OTHER = "controlled_by_other_extensions";
const NOT_CONTROLLABLE = "not_controllable";

const HOMEPAGE_URL_PREF = "browser.startup.homepage";

const getHomePageURL = () => {
  return Services.prefs.getStringPref(HOMEPAGE_URL_PREF);
};

function isConfirmed(id) {
  let item = ExtensionSettingsStore.getSetting("homepageNotification", id);
  return !!(item && item.value);
}

async function assertPreferencesShown(_spotlight) {
  await TestUtils.waitForCondition(
    () => gBrowser.currentURI.spec == "about:preferences#home",
    "Should open about:preferences."
  );

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [_spotlight],
    async spotlight => {
      let doc = content.document;
      let section = await ContentTaskUtils.waitForCondition(
        () => doc.querySelector(".spotlight"),
        "The spotlight should appear."
      );
      Assert.equal(
        section.getAttribute("data-subcategory"),
        spotlight,
        "The correct section is spotlighted."
      );
    }
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

add_task(async function test_multiple_extensions_overriding_home_page() {
  let defaultHomePage = getHomePageURL();

  function background() {
    browser.test.onMessage.addListener(async msg => {
      switch (msg) {
        case "checkHomepage":
          let homepage = await browser.browserSettings.homepageOverride.get({});
          browser.test.sendMessage("homepage", homepage);
          break;
        case "trySet":
          let setResult = await browser.browserSettings.homepageOverride.set({
            value: "foo",
          });
          browser.test.assertFalse(
            setResult,
            "Calling homepageOverride.set returns false."
          );
          browser.test.sendMessage("homepageSet");
          break;
        case "tryClear":
          let clearResult =
            await browser.browserSettings.homepageOverride.clear({});
          browser.test.assertFalse(
            clearResult,
            "Calling homepageOverride.clear returns false."
          );
          browser.test.sendMessage("homepageCleared");
          break;
      }
    });
  }

  let extObj = {
    manifest: {
      chrome_settings_overrides: {},
      permissions: ["browserSettings"],
    },
    useAddonManager: "temporary",
    background,
  };

  let ext1 = ExtensionTestUtils.loadExtension(extObj);

  extObj.manifest.chrome_settings_overrides = { homepage: HOME_URI_2 };
  let ext2 = ExtensionTestUtils.loadExtension(extObj);

  extObj.manifest.chrome_settings_overrides = { homepage: HOME_URI_3 };
  let ext3 = ExtensionTestUtils.loadExtension(extObj);

  extObj.manifest.chrome_settings_overrides = { homepage: HOME_URI_4 };
  let ext4 = ExtensionTestUtils.loadExtension(extObj);

  extObj.manifest.chrome_settings_overrides = {};
  let ext5 = ExtensionTestUtils.loadExtension(extObj);

  async function checkHomepageOverride(
    ext,
    expectedValue,
    expectedLevelOfControl
  ) {
    ext.sendMessage("checkHomepage");
    let homepage = await ext.awaitMessage("homepage");
    is(
      homepage.value,
      expectedValue,
      `homepageOverride setting returns the expected value: ${expectedValue}.`
    );
    is(
      homepage.levelOfControl,
      expectedLevelOfControl,
      `homepageOverride setting returns the expected levelOfControl: ${expectedLevelOfControl}.`
    );
  }

  await ext1.startup();

  is(getHomePageURL(), defaultHomePage, "Home url should be the default");
  await checkHomepageOverride(ext1, getHomePageURL(), NOT_CONTROLLABLE);

  // Because we are expecting the pref to change when we start or unload, we
  // need to wait on a pref change.  This is because the pref management is
  // async and can happen after the startup/unload is finished.
  let prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext2.startup();
  await prefPromise;

  ok(
    getHomePageURL().endsWith(HOME_URI_2),
    "Home url should be overridden by the second extension."
  );

  await checkHomepageOverride(ext1, HOME_URI_2, CONTROLLED_BY_OTHER);

  // Verify that calling set and clear do nothing.
  ext2.sendMessage("trySet");
  await ext2.awaitMessage("homepageSet");
  await checkHomepageOverride(ext1, HOME_URI_2, CONTROLLED_BY_OTHER);

  ext2.sendMessage("tryClear");
  await ext2.awaitMessage("homepageCleared");
  await checkHomepageOverride(ext1, HOME_URI_2, CONTROLLED_BY_OTHER);

  // Because we are unloading an earlier extension, browser.startup.homepage won't change
  await ext1.unload();

  await checkHomepageOverride(ext2, HOME_URI_2, CONTROLLED_BY_THIS);

  ok(
    getHomePageURL().endsWith(HOME_URI_2),
    "Home url should be overridden by the second extension."
  );

  prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext3.startup();
  await prefPromise;

  ok(
    getHomePageURL().endsWith(HOME_URI_3),
    "Home url should be overridden by the third extension."
  );

  await checkHomepageOverride(ext3, HOME_URI_3, CONTROLLED_BY_THIS);

  // Because we are unloading an earlier extension, browser.startup.homepage won't change
  await ext2.unload();

  ok(
    getHomePageURL().endsWith(HOME_URI_3),
    "Home url should be overridden by the third extension."
  );

  await checkHomepageOverride(ext3, HOME_URI_3, CONTROLLED_BY_THIS);

  prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext4.startup();
  await prefPromise;

  ok(
    getHomePageURL().endsWith(HOME_URI_4),
    "Home url should be overridden by the third extension."
  );

  await checkHomepageOverride(ext3, HOME_URI_4, CONTROLLED_BY_OTHER);

  prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext4.unload();
  await prefPromise;

  ok(
    getHomePageURL().endsWith(HOME_URI_3),
    "Home url should be overridden by the third extension."
  );

  await checkHomepageOverride(ext3, HOME_URI_3, CONTROLLED_BY_THIS);

  prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext3.unload();
  await prefPromise;

  is(getHomePageURL(), defaultHomePage, "Home url should be reset to default");

  await ext5.startup();
  await checkHomepageOverride(ext5, defaultHomePage, NOT_CONTROLLABLE);
  await ext5.unload();
});

const HOME_URI_1 = "http://example.com/";
const USER_URI = "http://example.edu/";

add_task(async function test_extension_setting_home_page_back() {
  let defaultHomePage = getHomePageURL();

  Services.prefs.setStringPref(HOMEPAGE_URL_PREF, USER_URI);

  is(getHomePageURL(), USER_URI, "Home url should be the user set value");

  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: { chrome_settings_overrides: { homepage: HOME_URI_1 } },
    useAddonManager: "temporary",
  });

  // Because we are expecting the pref to change when we start or unload, we
  // need to wait on a pref change.  This is because the pref management is
  // async and can happen after the startup/unload is finished.
  let prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext1.startup();
  await prefPromise;

  ok(
    getHomePageURL().endsWith(HOME_URI_1),
    "Home url should be overridden by the second extension."
  );

  prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext1.unload();
  await prefPromise;

  is(getHomePageURL(), USER_URI, "Home url should be the user set value");

  Services.prefs.clearUserPref(HOMEPAGE_URL_PREF);

  is(getHomePageURL(), defaultHomePage, "Home url should be the default");
});

add_task(async function test_disable() {
  const ID = "id@tests.mozilla.org";
  let defaultHomePage = getHomePageURL();

  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: {
        gecko: {
          id: ID,
        },
      },
      chrome_settings_overrides: {
        homepage: HOME_URI_1,
      },
    },
    useAddonManager: "temporary",
  });

  let prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext1.startup();
  await prefPromise;

  is(
    getHomePageURL(),
    HOME_URI_1,
    "Home url should be overridden by the extension."
  );

  let addon = await AddonManager.getAddonByID(ID);
  is(addon.id, ID, "Found the correct add-on.");

  let disabledPromise = awaitEvent("shutdown", ID);
  prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await addon.disable();
  await Promise.all([disabledPromise, prefPromise]);

  is(getHomePageURL(), defaultHomePage, "Home url should be the default");

  let enabledPromise = awaitEvent("ready", ID);
  prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await addon.enable();
  await Promise.all([enabledPromise, prefPromise]);

  is(
    getHomePageURL(),
    HOME_URI_1,
    "Home url should be overridden by the extension."
  );

  prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext1.unload();
  await prefPromise;

  is(getHomePageURL(), defaultHomePage, "Home url should be the default");
});

add_task(async function test_local() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: { chrome_settings_overrides: { homepage: "home.html" } },
    useAddonManager: "temporary",
  });

  let prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext1.startup();
  await prefPromise;

  let homepage = getHomePageURL();
  ok(
    homepage.startsWith("moz-extension") && homepage.endsWith("home.html"),
    "Home url should be relative to extension."
  );

  await ext1.unload();
});

add_task(async function test_multiple() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_settings_overrides: {
        homepage:
          "https://mozilla.org/|https://developer.mozilla.org/|https://addons.mozilla.org/",
      },
    },
    useAddonManager: "temporary",
  });

  let prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await extension.startup();
  await prefPromise;

  is(
    getHomePageURL(),
    "https://mozilla.org/%7Chttps://developer.mozilla.org/%7Chttps://addons.mozilla.org/",
    "The homepage encodes | so only one homepage is allowed"
  );

  await extension.unload();
});

add_task(async function test_doorhanger_homepage_button() {
  let defaultHomePage = getHomePageURL();
  // These extensions are temporarily loaded so that the AddonManager can see
  // them and the extension's shutdown handlers are called.
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: { chrome_settings_overrides: { homepage: "ext1.html" } },
    files: { "ext1.html": "<h1>1</h1>" },
    useAddonManager: "temporary",
  });
  let ext2 = ExtensionTestUtils.loadExtension({
    manifest: { chrome_settings_overrides: { homepage: "ext2.html" } },
    files: { "ext2.html": "<h1>2</h1>" },
    useAddonManager: "temporary",
  });

  let panel = ExtensionControlledPopup._getAndMaybeCreatePanel(document);
  let popupnotification = document.getElementById(
    "extension-homepage-notification"
  );

  await ext1.startup();
  await ext2.startup();

  let popupShown = promisePopupShown(panel);
  BrowserCommands.home();
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, () =>
    gURLBar.value.endsWith("ext2.html")
  );
  await popupShown;

  // Click Manage.
  let popupHidden = promisePopupHidden(panel);
  // Ensures the preferences tab opens, checks the spotlight, and then closes it
  let spotlightShown = assertPreferencesShown("homeOverride");
  popupnotification.secondaryButton.click();
  await popupHidden;
  await spotlightShown;

  let prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext2.unload();
  await prefPromise;

  // Expect a new doorhanger for the next extension.
  popupShown = promisePopupShown(panel);
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");
  let openHomepage = TestUtils.topicObserved("browser-open-homepage-start");
  BrowserCommands.home();
  await openHomepage;
  await popupShown;
  await TestUtils.waitForCondition(
    () => gURLBar.value.endsWith("ext1.html"),
    "ext1 is in control"
  );

  // Click manage again.
  popupHidden = promisePopupHidden(panel);
  // Ensures the preferences tab opens, checks the spotlight, and then closes it
  spotlightShown = assertPreferencesShown("homeOverride");
  popupnotification.secondaryButton.click();
  await popupHidden;
  await spotlightShown;

  prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext1.unload();
  await prefPromise;

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  openHomepage = TestUtils.topicObserved("browser-open-homepage-start");
  BrowserCommands.home();
  await openHomepage;

  is(getHomePageURL(), defaultHomePage, "The homepage is set back to default");
});

add_task(async function test_doorhanger_new_window() {
  // These extensions are temporarily loaded so that the AddonManager can see
  // them and the extension's shutdown handlers are called.
  let ext1Id = "ext1@mochi.test";
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_settings_overrides: { homepage: "ext1.html" },
      browser_specific_settings: {
        gecko: { id: ext1Id },
      },
      name: "Ext1",
    },
    files: { "ext1.html": "<h1>1</h1>" },
    useAddonManager: "temporary",
  });
  let ext2 = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.sendMessage("url", browser.runtime.getURL("ext2.html"));
    },
    manifest: {
      chrome_settings_overrides: { homepage: "ext2.html" },
      name: "Ext2",
    },
    files: { "ext2.html": "<h1>2</h1>" },
    useAddonManager: "temporary",
  });

  await ext1.startup();
  await ext2.startup();
  let url = await ext2.awaitMessage("url");

  await SpecialPowers.pushPrefEnv({ set: [["browser.startup.page", 1]] });

  let windowOpenedPromise = BrowserTestUtils.waitForNewWindow({ url });
  let win = OpenBrowserWindow();
  await windowOpenedPromise;
  let doc = win.document;
  let panel = ExtensionControlledPopup._getAndMaybeCreatePanel(doc);
  await promisePopupShown(panel);

  let description = doc.getElementById(
    "extension-homepage-notification-description"
  );

  await TestUtils.waitForCondition(
    () => win.gURLBar.value.endsWith("ext2.html"),
    "ext2 is in control"
  );

  is(
    description.textContent,
    "An extension,  Ext2, changed what you see when you open your homepage and new windows.Learn more",
    "The extension name is in the popup"
  );

  // Click Manage.
  let popupHidden = promisePopupHidden(panel);
  let popupnotification = doc.getElementById("extension-homepage-notification");
  popupnotification.secondaryButton.click();
  await popupHidden;

  let prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext2.unload();
  await prefPromise;

  // Expect a new doorhanger for the next extension.
  let popupShown = promisePopupShown(panel);
  await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank");
  let openHomepage = TestUtils.topicObserved("browser-open-homepage-start");
  win.BrowserCommands.home();
  await openHomepage;
  await popupShown;

  await TestUtils.waitForCondition(
    () => win.gURLBar.value.endsWith("ext1.html"),
    "ext1 is in control"
  );

  is(
    description.textContent,
    "An extension,  Ext1, changed what you see when you open your homepage and new windows.Learn more",
    "The extension name is in the popup"
  );

  // Click Keep Changes.
  popupnotification.button.click();
  await TestUtils.waitForCondition(() => isConfirmed(ext1Id));

  ok(
    getHomePageURL().endsWith("ext1.html"),
    "The homepage is still the first eextension"
  );

  await BrowserTestUtils.closeWindow(win);
  await ext1.unload();

  ok(!isConfirmed(ext1Id), "The confirmation is cleaned up on uninstall");
  // Skipping for window leak in debug builds, follow up bug: 1678412
}).skip(AppConstants.DEBUG);

async function testHomePageWindow(options = {}) {
  let windowOpenedPromise = BrowserTestUtils.waitForNewWindow();
  let win = OpenBrowserWindow(options.options);
  let openHomepage = TestUtils.topicObserved("browser-open-homepage-start");
  await windowOpenedPromise;
  let doc = win.document;
  let panel = ExtensionControlledPopup._getAndMaybeCreatePanel(doc);

  let popupShown = options.expectPanel && promisePopupShown(panel);
  win.BrowserCommands.home();
  await Promise.all([
    BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser),
    openHomepage,
    popupShown,
  ]);

  await options.test(win);

  if (options.expectPanel) {
    let popupHidden = promisePopupHidden(panel);
    panel.hidePopup();
    await popupHidden;
  }
  await BrowserTestUtils.closeWindow(win);
}

add_task(async function test_overriding_home_page_incognito_not_allowed() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.startup.page", 1]],
  });

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_settings_overrides: { homepage: "home.html" },
      name: "extension",
    },
    files: { "home.html": "<h1>1</h1>" },
    useAddonManager: "temporary",
  });

  await extension.startup();
  let url = `moz-extension://${extension.uuid}/home.html`;

  await testHomePageWindow({
    expectPanel: true,
    test(win) {
      let doc = win.document;
      let description = doc.getElementById(
        "extension-homepage-notification-description"
      );
      let popupnotification = description.closest("popupnotification");
      is(
        description.textContent,
        "An extension,  extension, changed what you see when you open your homepage and new windows.Learn more",
        "The extension name is in the popup"
      );
      is(
        popupnotification.hidden,
        false,
        "The expected popup notification is visible"
      );

      Assert.equal(HomePage.get(win), url, "The homepage is not set");
      Assert.equal(
        win.gURLBar.value,
        url,
        "home page not used in private window"
      );
    },
  });

  await testHomePageWindow({
    expectPanel: false,
    options: { private: true },
    test(win) {
      Assert.notEqual(HomePage.get(win), url, "The homepage is not set");
      Assert.notEqual(
        win.gURLBar.value,
        url,
        "home page not used in private window"
      );
    },
  });

  await extension.unload();
});

add_task(async function test_overriding_home_page_incognito_spanning() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_settings_overrides: { homepage: "home.html" },
      name: "private extension",
      browser_specific_settings: {
        gecko: { id: "@spanning-home" },
      },
    },
    files: { "home.html": "<h1>1</h1>" },
    useAddonManager: "permanent",
    incognitoOverride: "spanning",
  });

  await extension.startup();

  // private window uses extension homepage
  await testHomePageWindow({
    expectPanel: true,
    options: { private: true },
    test(win) {
      Assert.equal(
        HomePage.get(win),
        `moz-extension://${extension.uuid}/home.html`,
        "The homepage is set"
      );
      Assert.equal(
        win.gURLBar.value,
        `moz-extension://${extension.uuid}/home.html`,
        "extension is control in window"
      );
    },
  });

  await extension.unload();
});

add_task(async function test_overriding_home_page_incognito_external() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_settings_overrides: { homepage: "/home.html" },
      name: "extension",
    },
    useAddonManager: "temporary",
    files: { "home.html": "<h1>non-private home</h1>" },
  });

  await extension.startup();

  // non-private window uses extension homepage
  await testHomePageWindow({
    expectPanel: true,
    test(win) {
      Assert.equal(
        HomePage.get(win),
        `moz-extension://${extension.uuid}/home.html`,
        "The homepage is set"
      );
      Assert.equal(
        win.gURLBar.value,
        `moz-extension://${extension.uuid}/home.html`,
        "extension is control in window"
      );
    },
  });

  // private window does not use extension window
  await testHomePageWindow({
    expectPanel: false,
    options: { private: true },
    test(win) {
      Assert.notEqual(
        HomePage.get(win),
        `moz-extension://${extension.uuid}/home.html`,
        "The homepage is not set"
      );
      Assert.notEqual(
        win.gURLBar.value,
        `moz-extension://${extension.uuid}/home.html`,
        "home page not used in private window"
      );
    },
  });

  await extension.unload();
});

// This tests that the homepage provided by an extension can be opened by any extension
// and does not require web_accessible_resource entries.
async function _test_overriding_home_page_open(manifest_version) {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version,
      chrome_settings_overrides: { homepage: "home.html" },
      name: "homepage provider",
      browser_specific_settings: {
        gecko: { id: "homepage@mochitest" },
      },
    },
    files: {
      "home.html": `<h1>Home Page!</h1><pre id="result"></pre><script src="home.js"></script>`,
      "home.js": () => {
        document.querySelector("#result").textContent = "homepage loaded";
      },
    },
    useAddonManager: "permanent",
  });

  await extension.startup();

  // ensure it works and deal with initial panel prompt.
  await testHomePageWindow({
    expectPanel: true,
    async test(win) {
      Assert.equal(
        HomePage.get(win),
        `moz-extension://${extension.uuid}/home.html`,
        "The homepage is set"
      );
      Assert.equal(
        win.gURLBar.value,
        `moz-extension://${extension.uuid}/home.html`,
        "extension is control in window"
      );
      const { selectedBrowser } = win.gBrowser;
      const result = await SpecialPowers.spawn(
        selectedBrowser,
        [],
        async () => {
          const { document } = this.content;
          if (document.readyState !== "complete") {
            await new Promise(resolve => (document.onload = resolve));
          }
          return document.querySelector("#result").textContent;
        }
      );
      Assert.equal(
        result,
        "homepage loaded",
        "Overridden homepage loaded successfully"
      );
    },
  });

  // Extension used to open the homepage in a new window.
  let opener = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },
    async background() {
      let win;
      browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
        if (tab.windowId !== win.id || tab.status !== "complete") {
          return;
        }
        browser.test.sendMessage("created", tab.url);
      });
      browser.test.onMessage.addListener(async msg => {
        if (msg == "create") {
          win = await browser.windows.create({});
          browser.test.assertTrue(
            win.id !== browser.windows.WINDOW_ID_NONE,
            "New window was created."
          );
        }
      });
    },
  });

  function listener(msg) {
    Assert.ok(!/may not load or link to moz-extension/.test(msg.message));
  }
  Services.console.registerListener(listener);
  registerCleanupFunction(() => {
    Services.console.unregisterListener(listener);
  });

  await opener.startup();
  const promiseNewWindow = BrowserTestUtils.waitForNewWindow();
  await opener.sendMessage("create");
  let homepageUrl = await opener.awaitMessage("created");

  Assert.equal(
    homepageUrl,
    `moz-extension://${extension.uuid}/home.html`,
    "The homepage is set"
  );

  const newWin = await promiseNewWindow;
  Assert.equal(
    await SpecialPowers.spawn(newWin.gBrowser.selectedBrowser, [], async () => {
      const { document } = this.content;
      if (document.readyState !== "complete") {
        await new Promise(resolve => (document.onload = resolve));
      }
      return document.querySelector("#result").textContent;
    }),
    "homepage loaded",
    "Overridden homepage loaded as expected"
  );

  await BrowserTestUtils.closeWindow(newWin);
  await opener.unload();
  await extension.unload();
}

add_task(async function test_overriding_home_page_open_mv2() {
  await _test_overriding_home_page_open(2);
});

add_task(async function test_overriding_home_page_open_mv3() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.manifestV3.enabled", true]],
  });
  await _test_overriding_home_page_open(3);
});
