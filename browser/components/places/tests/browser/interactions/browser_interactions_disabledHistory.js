/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests we don't record interactions when history is disabled, even if the page
 * is bookmarked.
 */

"use strict";

const TEST_URL =
  "https://example.com/browser/browser/components/places/tests/browser/keyword_form.html";
const TEST_URL_AWAY = "https://example.com/browser";

const sentence = "The quick brown fox jumps over the lazy dog.";

async function sendTextToInput(browser, text) {
  // Reset to later verify that the provided text matches the value.
  await SpecialPowers.spawn(browser, [], function () {
    const input = content.document.querySelector(
      "#form1 > input[name='search']"
    );
    input.focus();
    input.value = "";
  });

  EventUtils.sendString(text);

  await SpecialPowers.spawn(browser, [{ text }], async function (args) {
    await ContentTaskUtils.waitForCondition(
      () =>
        content.document.querySelector("#form1 > input[name='search']").value ==
        args.text,
      "Text has been set on input"
    );
  });
}

add_setup(async function () {
  await Interactions.reset();
  await PlacesUtils.bookmarks.insert({
    url: TEST_URL,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  registerCleanupFunction(async () => {
    await Interactions.reset();
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

add_task(async function test_disabled_history() {
  await SpecialPowers.pushPrefEnv({
    set: [["places.history.enabled", false]],
  });
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await sendTextToInput(browser, sentence);

    BrowserTestUtils.startLoadingURIString(browser, TEST_URL_AWAY);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_URL_AWAY);

    await assertDatabaseValues([]);

    BrowserTestUtils.startLoadingURIString(browser, "about:blank");
    await BrowserTestUtils.browserLoaded(browser, false, "about:blank");

    await assertDatabaseValues([]);
  });
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_disableGlobalHistory() {
  await Interactions.reset();

  await PlacesUtils.history.clear();
  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser));
  let browser = tab.linkedBrowser;
  browser.setAttribute("disableglobalhistory", "true");

  BrowserTestUtils.startLoadingURIString(browser, TEST_URL);
  await BrowserTestUtils.browserLoaded(browser, false, TEST_URL);
  Assert.ok(
    !browser.browsingContext.useGlobalHistory,
    "browserContext should be updated after the first load"
  );

  await sendTextToInput(browser, sentence);

  BrowserTestUtils.startLoadingURIString(browser, TEST_URL_AWAY);
  await BrowserTestUtils.browserLoaded(browser, false, TEST_URL_AWAY);

  Assert.ok(
    !(await PlacesUtils.history.hasVisits(TEST_URL)),
    "Check visits were not added"
  );
  Assert.ok(
    !(await PlacesUtils.history.hasVisits(TEST_URL_AWAY)),
    "Check visits were not added"
  );
  await assertDatabaseValues([]);
  BrowserTestUtils.removeTab(tab);
});
