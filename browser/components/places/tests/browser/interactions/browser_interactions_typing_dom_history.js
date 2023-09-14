/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests reporting of typing interactions after DOM history API usage.
 */

"use strict";

const TEST_URL =
  "https://example.com/browser/browser/components/places/tests/browser/keyword_form.html";
const TEST_URL2 = "https://example.com/browser";

const sentence = "The quick brown fox jumps over the lazy dog.";

async function sendTextToInput(browser, text) {
  await SpecialPowers.spawn(browser, [], function () {
    const input = content.document.querySelector(
      "#form1 > input[name='search']"
    );
    input.focus();
    input.value = ""; // Reset to later verify that the provided text matches the value
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

add_task(async function test_typing_pushState() {
  await Interactions.reset();

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await sendTextToInput(browser, sentence);

    await ContentTask.spawn(browser, TEST_URL2, url => {
      content.history.pushState(null, "", url);
    });

    await sendTextToInput(browser, sentence);

    BrowserTestUtils.startLoadingURIString(browser, "about:blank");
    await BrowserTestUtils.browserLoaded(browser, false, "about:blank");

    await assertDatabaseValues([
      {
        url: TEST_URL,
        keypresses: sentence.length,
        typingTimeIsGreaterThan: 0,
      },
      {
        url: TEST_URL2,
        keypresses: sentence.length,
        typingTimeIsGreaterThan: 0,
      },
    ]);
  });
});

add_task(async function test_typing_pushState_sameUrl() {
  await Interactions.reset();

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await sendTextToInput(browser, sentence);

    await ContentTask.spawn(browser, TEST_URL, url => {
      content.history.pushState(null, "", url);
    });

    await sendTextToInput(browser, sentence);

    BrowserTestUtils.startLoadingURIString(browser, "about:blank");
    await BrowserTestUtils.browserLoaded(browser, false, "about:blank");

    await assertDatabaseValues([
      {
        url: TEST_URL,
        keypresses: sentence.length * 2,
        typingTimeIsGreaterThan: 0,
      },
    ]);
  });
});

add_task(async function test_typing_replaceState() {
  await Interactions.reset();

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await sendTextToInput(browser, sentence);

    await ContentTask.spawn(browser, TEST_URL2, url => {
      content.history.replaceState(null, "", url);
    });

    await sendTextToInput(browser, sentence);

    BrowserTestUtils.startLoadingURIString(browser, "about:blank");
    await BrowserTestUtils.browserLoaded(browser, false, "about:blank");

    await assertDatabaseValues([
      {
        url: TEST_URL,
        keypresses: sentence.length,
        typingTimeIsGreaterThan: 0,
      },
      {
        url: TEST_URL2,
        keypresses: sentence.length,
        typingTimeIsGreaterThan: 0,
      },
    ]);
  });
});

add_task(async function test_typing_replaceState_sameUrl() {
  await Interactions.reset();

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await sendTextToInput(browser, sentence);

    await ContentTask.spawn(browser, TEST_URL, url => {
      content.history.replaceState(null, "", url);
    });

    await sendTextToInput(browser, sentence);

    BrowserTestUtils.startLoadingURIString(browser, "about:blank");
    await BrowserTestUtils.browserLoaded(browser, false, "about:blank");

    await assertDatabaseValues([
      {
        url: TEST_URL,
        keypresses: sentence.length * 2,
        typingTimeIsGreaterThan: 0,
      },
    ]);
  });
});

add_task(async function test_typing_hashchange() {
  await Interactions.reset();

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await sendTextToInput(browser, sentence);

    await ContentTask.spawn(browser, TEST_URL + "#foo", url => {
      content.location = url;
    });

    await sendTextToInput(browser, sentence);

    BrowserTestUtils.startLoadingURIString(browser, "about:blank");
    await BrowserTestUtils.browserLoaded(browser, false, "about:blank");

    await assertDatabaseValues([
      {
        url: TEST_URL,
        keypresses: sentence.length * 2,
        typingTimeIsGreaterThan: 0,
      },
    ]);
  });
});
