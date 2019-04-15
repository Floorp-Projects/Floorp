/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(this, "PlacesUtils",
                               "resource://gre/modules/PlacesUtils.jsm");
ChromeUtils.defineModuleGetter(this, "UrlbarTestUtils",
                               "resource://testing-common/UrlbarTestUtils.jsm");

const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

function promiseAutocompleteResultPopup(value) {
  return UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value,
  });
}

async function addBookmark(bookmark) {
  if (bookmark.keyword) {
    await PlacesUtils.keywords.insert({
      keyword: bookmark.keyword,
      url: bookmark.url,
    });
  }

  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: bookmark.url,
    title: bookmark.title,
  });

  registerCleanupFunction(async function() {
    await PlacesUtils.bookmarks.eraseEverything();
  });
}

async function addSearchEngine(basename) {
  info("Waiting for engine to be added: " + basename);
  let url = getRootDirectory(gTestPath) + basename;
  let engine = await Services.search.addEngine(url, "", false);

  info(`Search engine added: ${basename}`);
  registerCleanupFunction(async () => Services.search.removeEngine(engine));
  return engine;
}

async function prepareSearchEngine() {
  let oldDefaultEngine = await Services.search.getDefault();
  let suggestionsEnabled = Services.prefs.getBoolPref(SUGGEST_URLBAR_PREF);
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, true);
  let engine = await addSearchEngine(TEST_ENGINE_BASENAME);
  await Services.search.setDefault(engine);

  registerCleanupFunction(async function() {
    Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, suggestionsEnabled);
    await Services.search.setDefault(oldDefaultEngine);

    // Make sure the popup is closed for the next test.
    await UrlbarTestUtils.promisePopupClose(window);

    // Clicking suggestions causes visits to search results pages, so clear that
    // history now.
    await PlacesUtils.history.clear();
  });
}

add_task(async function test_webnavigation_urlbar_typed_transitions() {
  function backgroundScript() {
    browser.webNavigation.onCommitted.addListener((msg) => {
      browser.test.assertEq("http://example.com/?q=typed", msg.url,
                            "Got the expected url");
      // assert from_address_bar transition qualifier
      browser.test.assertTrue(msg.transitionQualifiers &&
                          msg.transitionQualifiers.includes("from_address_bar"),
                              "Got the expected from_address_bar transitionQualifier");
      browser.test.assertEq("typed", msg.transitionType,
                            "Got the expected transitionType");
      browser.test.notifyPass("webNavigation.from_address_bar.typed");
    });

    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: backgroundScript,
    manifest: {
      permissions: ["webNavigation"],
    },
  });

  await extension.startup();
  await SimpleTest.promiseFocus(window);

  await extension.awaitMessage("ready");

  gURLBar.focus();
  const inputValue = "http://example.com/?q=typed";
  gURLBar.inputField.value = inputValue.slice(0, -1);
  EventUtils.sendString(inputValue.slice(-1));
  EventUtils.synthesizeKey("VK_RETURN", {altKey: true});

  await extension.awaitFinish("webNavigation.from_address_bar.typed");

  await extension.unload();
});

add_task(async function test_webnavigation_urlbar_typed_closed_popup_transitions() {
  function backgroundScript() {
    browser.webNavigation.onCommitted.addListener((msg) => {
      browser.test.assertEq("http://example.com/?q=typedClosed", msg.url,
                            "Got the expected url");
      // assert from_address_bar transition qualifier
      browser.test.assertTrue(msg.transitionQualifiers &&
                              msg.transitionQualifiers.includes("from_address_bar"),
                              "Got the expected from_address_bar transitionQualifier");
      browser.test.assertEq("typed", msg.transitionType,
                            "Got the expected transitionType");
      browser.test.notifyPass("webNavigation.from_address_bar.typed");
    });

    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: backgroundScript,
    manifest: {
      permissions: ["webNavigation"],
    },
  });

  await extension.startup();
  await SimpleTest.promiseFocus(window);

  await extension.awaitMessage("ready");
  await promiseAutocompleteResultPopup("http://example.com/?q=typedClosed");
  await UrlbarTestUtils.promiseSearchComplete(window);
  // Closing the popup forces a different code route that handles no results
  // being displayed.
  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("VK_RETURN", {});

  await extension.awaitFinish("webNavigation.from_address_bar.typed");

  await extension.unload();
});

add_task(async function test_webnavigation_urlbar_bookmark_transitions() {
  function backgroundScript() {
    browser.webNavigation.onCommitted.addListener((msg) => {
      browser.test.assertEq("http://example.com/?q=bookmark", msg.url,
                            "Got the expected url");

      // assert from_address_bar transition qualifier
      browser.test.assertTrue(msg.transitionQualifiers &&
                          msg.transitionQualifiers.includes("from_address_bar"),
                              "Got the expected from_address_bar transitionQualifier");
      browser.test.assertEq("auto_bookmark", msg.transitionType,
                            "Got the expected transitionType");
      browser.test.notifyPass("webNavigation.from_address_bar.auto_bookmark");
    });

    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: backgroundScript,
    manifest: {
      permissions: ["webNavigation"],
    },
  });

  await addBookmark({
    title: "Bookmark To Click",
    url: "http://example.com/?q=bookmark",
  });

  await extension.startup();
  await SimpleTest.promiseFocus(window);

  await extension.awaitMessage("ready");

  await promiseAutocompleteResultPopup("Bookmark To Click");

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  EventUtils.synthesizeMouseAtCenter(result.element.row, {});
  await extension.awaitFinish("webNavigation.from_address_bar.auto_bookmark");

  await extension.unload();
});

add_task(async function test_webnavigation_urlbar_keyword_transition() {
  function backgroundScript() {
    browser.webNavigation.onCommitted.addListener((msg) => {
      browser.test.assertEq(`http://example.com/?q=search`, msg.url,
                            "Got the expected url");

      // assert from_address_bar transition qualifier
      browser.test.assertTrue(msg.transitionQualifiers &&
                          msg.transitionQualifiers.includes("from_address_bar"),
                              "Got the expected from_address_bar transitionQualifier");
      browser.test.assertEq("keyword", msg.transitionType,
                            "Got the expected transitionType");
      browser.test.notifyPass("webNavigation.from_address_bar.keyword");
    });

    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: backgroundScript,
    manifest: {
      permissions: ["webNavigation"],
    },
  });

  await addBookmark({
    title: "Test Keyword",
    url: "http://example.com/?q=%s",
    keyword: "testkw",
  });

  await extension.startup();
  await SimpleTest.promiseFocus(window);

  await extension.awaitMessage("ready");

  await promiseAutocompleteResultPopup("testkw search");

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  EventUtils.synthesizeMouseAtCenter(result.element.row, {});

  await extension.awaitFinish("webNavigation.from_address_bar.keyword");

  await extension.unload();
});

add_task(async function test_webnavigation_urlbar_search_transitions() {
  function backgroundScript() {
    browser.webNavigation.onCommitted.addListener((msg) => {
      browser.test.assertEq("http://mochi.test:8888/", msg.url,
                            "Got the expected url");

      // assert from_address_bar transition qualifier
      browser.test.assertTrue(msg.transitionQualifiers &&
                          msg.transitionQualifiers.includes("from_address_bar"),
                              "Got the expected from_address_bar transitionQualifier");
      browser.test.assertEq("generated", msg.transitionType,
                            "Got the expected 'generated' transitionType");
      browser.test.notifyPass("webNavigation.from_address_bar.generated");
    });

    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: backgroundScript,
    manifest: {
      permissions: ["webNavigation"],
    },
  });

  await extension.startup();
  await SimpleTest.promiseFocus(window);

  await extension.awaitMessage("ready");

  await prepareSearchEngine();
  await promiseAutocompleteResultPopup("foo");

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  EventUtils.synthesizeMouseAtCenter(result.element.row, {});

  await extension.awaitFinish("webNavigation.from_address_bar.generated");

  await extension.unload();
});
