/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This tests that the display of keyword results are not changed when the user
 * presses the override button.
 */

 add_task(async function() {
  let bm = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: "http://example.com/?q=%s",
                                                title: "test" });
  await PlacesUtils.keywords.insert({ keyword: "keyword",
                                      url: "http://example.com/?q=%s" });

  registerCleanupFunction(async function() {
    await PlacesUtils.bookmarks.remove(bm);
  });

  await promiseAutocompleteResultPopup("keyword search");
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);

  info("Before override");
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.KEYWORD,
    "Should have a keyword result");
  Assert.equal(result.displayed.title, "example.com: search",
    "Node should contain the name of the bookmark and query");
  Assert.ok(!result.displayed.action, "Should have an empty action");

  if (!UrlbarPrefs.get("quantumbar")) {
    let element = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);

    // QuantumBar doesn't have separate boxes for items.
    let urlHbox = element._urlText.parentNode.parentNode;
    Assert.ok(urlHbox.classList.contains("ac-url"), "URL hbox element sanity check");
    BrowserTestUtils.is_hidden(urlHbox, "URL element should be hidden");
  }

  info("During override");
  EventUtils.synthesizeKey("VK_SHIFT", { type: "keydown" });

  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.KEYWORD,
    "Should have a keyword result");
  Assert.equal(result.displayed.title, "example.com: search",
    "Node should contain the name of the bookmark and query");
  Assert.ok(!result.displayed.action, "Should have an empty action");

  if (!UrlbarPrefs.get("quantumbar")) {
    let element = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);

    // QuantumBar doesn't have separate boxes for items.
    let urlHbox = element._urlText.parentNode.parentNode;
    Assert.ok(urlHbox.classList.contains("ac-url"), "URL hbox element sanity check");
    BrowserTestUtils.is_hidden(urlHbox, "URL element should be hidden");
  }

  EventUtils.synthesizeKey("VK_SHIFT", { type: "keyup" });
});
