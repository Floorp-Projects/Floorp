/* Any copyright is dedicated to the Public Domain.
 *  * http://creativecommons.org/publicdomain/zero/1.0/ */
/*
 * Test searching for the selected text using the context menu
 */

const ENGINE_NAME = "mozSearch";
const ENGINE_ID = "mozsearch-engine@search.mozilla.org";
const PRIVATE_ENGINE_NAME = "mozPrivateSearch";
const PRIVATE_ENGINE_ID = "mozsearch-engine-private@search.mozilla.org";

let engine;
let privateEngine;

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // We want select events to be fired.
      ["dom.select_events.enabled", true],
      ["browser.search.separatePrivateDefault", true],
    ],
  });

  await Services.search.init();

  // replace the path we load search engines from with
  // the path to our test data.
  let searchExtensions = getChromeDir(getResolvedURI(gTestPath));
  searchExtensions.append("mozsearch");
  let resProt = Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler);
  let originalSubstitution = resProt.getSubstitution("search-extensions");
  resProt.setSubstitution(
    "search-extensions",
    Services.io.newURI("file://" + searchExtensions.path)
  );

  await Services.search.ensureBuiltinExtension(ENGINE_ID);
  await Services.search.ensureBuiltinExtension(PRIVATE_ENGINE_ID);

  engine = await Services.search.getEngineByName(ENGINE_NAME);
  Assert.ok(engine, "Got a search engine");
  let defaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);

  privateEngine = await Services.search.getEngineByName(PRIVATE_ENGINE_NAME);
  Assert.ok(privateEngine, "Got a search engine");
  let defaultPrivateEngine = await Services.search.getDefaultPrivate();
  await Services.search.setDefaultPrivate(privateEngine);

  registerCleanupFunction(async () => {
    await Services.search.setDefault(defaultEngine);
    await Services.search.setDefaultPrivate(defaultPrivateEngine);
    await Services.search.removeEngine(engine);
    await Services.search.removeEngine(privateEngine);

    resProt.setSubstitution("search-extensions", originalSubstitution);
    // Finial re-init to make sure we drop the added built-in engines.
    await Services.search.reInit();
  });
});

async function checkContextMenu(win, expectedName, expectedBaseUrl) {
  let contextMenu = win.document.getElementById("contentAreaContextMenu");
  Assert.ok(contextMenu, "Got context menu XUL");

  let tab = await BrowserTestUtils.openNewForegroundTab(
    win.gBrowser,
    "data:text/plain;charset=utf8,test%20search"
  );

  await ContentTask.spawn(tab.linkedBrowser, "", async function() {
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
    "Search " + expectedName + " for \u201ctest search\u201d",
    "Check context menu label"
  );
  Assert.equal(
    searchItem.disabled,
    false,
    "Check that search context menu item is enabled"
  );

  let searchTab = await BrowserTestUtils.openNewForegroundTab(
    win.gBrowser,
    () => {
      searchItem.click();
    }
  );

  Assert.equal(
    win.gBrowser.currentURI.spec,
    expectedBaseUrl + "?test=test+search&ie=utf-8&channel=contextsearch",
    "Checking context menu search URL"
  );

  contextMenu.hidePopup();

  BrowserTestUtils.removeTab(searchTab);
  BrowserTestUtils.removeTab(tab);
}

add_task(async function test_normalWindow() {
  await checkContextMenu(
    window,
    ENGINE_NAME,
    "https://example.com/browser/browser/components/search/test/browser/"
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
    "https://example.com/browser/browser/components/search/test/browser/"
  );
});
