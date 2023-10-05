/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests we clear interactions when history is cleared.
 */

"use strict";

const TEST_URL =
  "https://example.com/browser/browser/components/places/tests/browser/keyword_form.html";
const TEST_URL_2 =
  "https://example.org/browser/browser/components/places/tests/browser/keyword_form.html";
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

add_task(async function test_clear_history() {
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await sendTextToInput(browser, sentence);

    BrowserTestUtils.startLoadingURIString(browser, TEST_URL_2);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_URL_2);

    await sendTextToInput(browser, sentence);

    BrowserTestUtils.startLoadingURIString(browser, "about:blank");
    await BrowserTestUtils.browserLoaded(browser, false, "about:blank");
  });

  Assert.ok(
    await PlacesUtils.history.hasVisits(TEST_URL),
    "Check visits were added"
  );
  Assert.ok(
    await PlacesUtils.history.hasVisits(TEST_URL_2),
    "Check visits were added"
  );

  info("Check interactions were added");
  await assertDatabaseValues([
    {
      url: TEST_URL,
      keypresses: sentence.length,
      typingTimeIsGreaterThan: 0,
    },
    {
      url: TEST_URL_2,
      keypresses: sentence.length,
      typingTimeIsGreaterThan: 0,
    },
  ]);

  await PlacesUtils.history.clear();

  Assert.ok(
    await PlacesTestUtils.isPageInDB(TEST_URL),
    "Bookmarked page remains in the database"
  );
  Assert.ok(
    !(await PlacesUtils.history.hasVisits(TEST_URL)),
    "Check visits were removed"
  );
  Assert.ok(
    !(await PlacesTestUtils.isPageInDB(TEST_URL_2)),
    "Non bookmarked page was removed from the database"
  );

  info("Check all interactions have been removed.");
  await assertDatabaseValues([]);
});
