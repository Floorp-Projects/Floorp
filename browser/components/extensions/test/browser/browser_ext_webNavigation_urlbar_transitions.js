/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
                                  "resource://testing-common/PlacesTestUtils.jsm");

const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

function* promiseAutocompleteResultPopup(inputText) {
  gURLBar.focus();
  gURLBar.value = inputText;
  gURLBar.controller.startSearch(inputText);
  yield promisePopupShown(gURLBar.popup);
  yield BrowserTestUtils.waitForCondition(() => {
    return gURLBar.controller.searchStatus >=
      Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH;
  });
}

function* addBookmark(bookmark) {
  if (bookmark.keyword) {
    yield PlacesUtils.keywords.insert({
      keyword: bookmark.keyword,
      url: bookmark.url,
    });
  }

  yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: bookmark.url,
    title: bookmark.title,
  });

  registerCleanupFunction(function* () {
    yield PlacesUtils.bookmarks.eraseEverything();
  });
}

function addSearchEngine(basename) {
  return new Promise((resolve, reject) => {
    info("Waiting for engine to be added: " + basename);
    let url = getRootDirectory(gTestPath) + basename;
    Services.search.addEngine(url, null, "", false, {
      onSuccess: (engine) => {
        info(`Search engine added: ${basename}`);
        registerCleanupFunction(() => Services.search.removeEngine(engine));
        resolve(engine);
      },
      onError: (errCode) => {
        ok(false, `addEngine failed with error code ${errCode}`);
        reject();
      },
    });
  });
}

function* prepareSearchEngine() {
  let oldCurrentEngine = Services.search.currentEngine;
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, true);
  let engine = yield addSearchEngine(TEST_ENGINE_BASENAME);
  Services.search.currentEngine = engine;

  registerCleanupFunction(function* () {
    Services.prefs.clearUserPref(SUGGEST_URLBAR_PREF);
    Services.search.currentEngine = oldCurrentEngine;

    // Make sure the popup is closed for the next test.
    gURLBar.blur();
    gURLBar.popup.selectedIndex = -1;
    gURLBar.popup.hidePopup();
    ok(!gURLBar.popup.popupOpen, "popup should be closed");

    // Clicking suggestions causes visits to search results pages, so clear that
    // history now.
    yield PlacesTestUtils.clearHistory();
  });
}

add_task(function* test_webnavigation_urlbar_typed_transitions() {
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

  yield extension.startup();
  yield SimpleTest.promiseFocus(window);

  yield extension.awaitMessage("ready");

  gURLBar.focus();
  gURLBar.textValue = "http://example.com/?q=typed";

  EventUtils.synthesizeKey("VK_RETURN", {altKey: true});

  yield extension.awaitFinish("webNavigation.from_address_bar.typed");

  yield extension.unload();
  info("extension unloaded");
});

add_task(function* test_webnavigation_urlbar_bookmark_transitions() {
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

  yield addBookmark({
    title: "Bookmark To Click",
    url: "http://example.com/?q=bookmark",
  });

  yield extension.startup();
  yield SimpleTest.promiseFocus(window);

  yield extension.awaitMessage("ready");

  yield promiseAutocompleteResultPopup("Bookmark To Click");

  let item = gURLBar.popup.richlistbox.getItemAtIndex(1);
  item.click();
  yield extension.awaitFinish("webNavigation.from_address_bar.auto_bookmark");

  yield extension.unload();
  info("extension unloaded");
});

add_task(function* test_webnavigation_urlbar_keyword_transition() {
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

  yield addBookmark({
    title: "Test Keyword",
    url: "http://example.com/?q=%s",
    keyword: "testkw",
  });

  yield extension.startup();
  yield SimpleTest.promiseFocus(window);

  yield extension.awaitMessage("ready");

  yield promiseAutocompleteResultPopup("testkw search");

  let item = gURLBar.popup.richlistbox.getItemAtIndex(0);
  item.click();

  yield extension.awaitFinish("webNavigation.from_address_bar.keyword");

  yield extension.unload();
  info("extension unloaded");
});

add_task(function* test_webnavigation_urlbar_search_transitions() {
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

  yield extension.startup();
  yield SimpleTest.promiseFocus(window);

  yield extension.awaitMessage("ready");

  yield prepareSearchEngine();
  yield promiseAutocompleteResultPopup("foo");

  let item = gURLBar.popup.richlistbox.getItemAtIndex(0);
  item.click();

  yield extension.awaitFinish("webNavigation.from_address_bar.generated");

  yield extension.unload();
  info("extension unloaded");
});
