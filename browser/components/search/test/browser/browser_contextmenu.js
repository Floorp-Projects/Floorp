/* Any copyright is dedicated to the Public Domain.
 *  * http://creativecommons.org/publicdomain/zero/1.0/ */
/*
 * Test searching for the selected text using the context menu
 */

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

AddonTestUtils.initMochitest(this);

const ENGINE_NAME = "mozSearch";
const PRIVATE_ENGINE_NAME = "mozPrivateSearch";
const ENGINE_DATA = new Map([
  [
    ENGINE_NAME,
    "https://example.com/browser/browser/components/search/test/browser/mozsearch.sjs",
  ],
  [PRIVATE_ENGINE_NAME, "https://example.com:443/browser/"],
]);

let engine;
let privateEngine;
let extensions = [];
let oldDefaultEngine;
let oldDefaultPrivateEngine;

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault", true],
      ["browser.search.separatePrivateDefault.ui.enabled", true],
    ],
  });

  await Services.search.init();

  for (let [name, search_url] of ENGINE_DATA) {
    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        chrome_settings_overrides: {
          search_provider: {
            name,
            search_url,
            params: [
              {
                name: "test",
                value: "{searchTerms}",
              },
            ],
          },
        },
      },
    });

    await extension.startup();
    await AddonTestUtils.waitForSearchProviderStartup(extension);
    extensions.push(extension);
  }

  engine = await Services.search.getEngineByName(ENGINE_NAME);
  Assert.ok(engine, "Got a search engine");
  oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);

  privateEngine = await Services.search.getEngineByName(PRIVATE_ENGINE_NAME);
  Assert.ok(privateEngine, "Got a search engine");
  oldDefaultPrivateEngine = await Services.search.getDefaultPrivate();
  await Services.search.setDefaultPrivate(privateEngine);
});

async function checkContextMenu(
  win,
  expectedName,
  expectedBaseUrl,
  expectedPrivateName
) {
  let contextMenu = win.document.getElementById("contentAreaContextMenu");
  Assert.ok(contextMenu, "Got context menu XUL");

  let tab = await BrowserTestUtils.openNewForegroundTab(
    win.gBrowser,
    "https://example.com/browser/browser/components/search/test/browser/test_search.html"
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [""], async function() {
    return new Promise(resolve => {
      content.document.addEventListener(
        "selectionchange",
        function() {
          resolve();
        },
        { once: true }
      );
      content.document.getSelection().selectAllChildren(content.document.body);
    });
  });

  let eventDetails = { type: "contextmenu", button: 2 };

  let popupPromise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  BrowserTestUtils.synthesizeMouseAtCenter(
    "body",
    eventDetails,
    win.gBrowser.selectedBrowser
  );
  await popupPromise;

  info("checkContextMenu");
  let searchItem = contextMenu.getElementsByAttribute(
    "id",
    "context-searchselect"
  )[0];
  Assert.ok(searchItem, "Got search context menu item");
  Assert.equal(
    searchItem.label,
    "Search " + expectedName + " for \u201ctest%20search\u201d",
    "Check context menu label"
  );
  Assert.equal(
    searchItem.disabled,
    false,
    "Check that search context menu item is enabled"
  );

  let loaded = BrowserTestUtils.waitForNewTab(
    win.gBrowser,
    expectedBaseUrl + "?test=test%2520search",
    true
  );
  searchItem.click();
  let searchTab = await loaded;
  let browser = win.gBrowser.selectedBrowser;
  await SpecialPowers.spawn(browser, [], async function() {
    Assert.ok(
      !/error/.test(content.document.body.innerHTML),
      "Ensure there were no errors loading the search page"
    );
  });

  searchItem = contextMenu.getElementsByAttribute(
    "id",
    "context-searchselect-private"
  )[0];
  Assert.ok(searchItem, "Got search in private window context menu item");
  if (PrivateBrowsingUtils.isWindowPrivate(win)) {
    Assert.ok(searchItem.hidden, "Search in private window should be hidden");
  } else {
    let expectedLabel = expectedPrivateName
      ? "Search with " + expectedPrivateName + " in a Private Window"
      : "Search in a Private Window";
    Assert.equal(searchItem.label, expectedLabel, "Check context menu label");
    Assert.equal(
      searchItem.disabled,
      false,
      "Check that search context menu item is enabled"
    );
  }

  contextMenu.hidePopup();

  BrowserTestUtils.removeTab(searchTab);
  BrowserTestUtils.removeTab(tab);
}

add_task(async function test_normalWindow() {
  await checkContextMenu(
    window,
    ENGINE_NAME,
    "https://example.com/browser/browser/components/search/test/browser/mozsearch.sjs",
    PRIVATE_ENGINE_NAME
  );
});

add_task(async function test_privateWindow() {
  const win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  registerCleanupFunction(async () => {
    await BrowserTestUtils.closeWindow(win);
  });

  await checkContextMenu(
    win,
    PRIVATE_ENGINE_NAME,
    "https://example.com/browser/"
  );
});

add_task(async function test_normalWindow_sameDefaults() {
  // Set the private default engine to be the same as the current default engine
  // in 'normal' mode.
  await Services.search.setDefaultPrivate(await Services.search.getDefault());

  await checkContextMenu(
    window,
    ENGINE_NAME,
    "https://example.com/browser/browser/components/search/test/browser/mozsearch.sjs"
  );
});

add_task(async function test_privateWindow_no_separate_engine() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // We want select events to be fired.
      ["browser.search.separatePrivateDefault", false],
    ],
  });

  const win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  registerCleanupFunction(async () => {
    await BrowserTestUtils.closeWindow(win);
  });

  await checkContextMenu(
    win,
    ENGINE_NAME,
    "https://example.com/browser/browser/components/search/test/browser/mozsearch.sjs"
  );
});

// We can't do the unload within registerCleanupFunction as that's too late for
// the test to be happy. Do it into a cleanup "test" here instead.
add_task(async function cleanup() {
  await Services.search.setDefault(oldDefaultEngine);
  await Services.search.setDefaultPrivate(oldDefaultPrivateEngine);
  await Services.search.removeEngine(engine);
  await Services.search.removeEngine(privateEngine);

  for (let extension of extensions) {
    await extension.unload();
  }
});
