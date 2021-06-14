/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests reporting of typing interactions.
 */

"use strict";

const TEST_URL =
  "https://example.com/browser/browser/components/places/tests/browser/keyword_form.html";
const TEST_URL2 = "https://example.com/browser";
const TEST_URL3 =
  "https://example.com/browser/browser/base/content/test/contextMenu/subtst_contextmenu_input.html";

const sentence = "The quick brown fox jumps over the lazy dog.";
const sentenceFragments = [
  "The quick",
  " brown fox",
  " jumps over the lazy dog.",
];
const longSentence =
  "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Maecenas ut purus a libero cursus scelerisque. In hac habitasse platea dictumst. Quisque posuere ante sed consequat volutpat.";

// Reduce the end-of-typing threshold to improve test speed
const TYPING_INTERACTION_TIMEOUT = 50;
// A delay that's long enough to trigger the end of typing timeout
const POST_TYPING_DELAY = TYPING_INTERACTION_TIMEOUT + 50;
const defaultTypingTimeout = Interactions._getTypingTimeout();

function reduceTypingTimeoutForTests(timeout) {
  Interactions._setTypingTimeout(timeout);
}

function resetTypingTimeout() {
  Interactions._setTypingTimeout(defaultTypingTimeout);
}

add_task(async function setup() {
  sinon.spy(Interactions, "_updateDatabase");
  Interactions.reset();
  disableIdleService();

  reduceTypingTimeoutForTests(TYPING_INTERACTION_TIMEOUT);

  registerCleanupFunction(() => {
    sinon.restore();
    resetTypingTimeout();
  });
});

async function assertDatabaseValues(expected) {
  await BrowserTestUtils.waitForCondition(
    () => Interactions._updateDatabase.callCount == expected.length,
    `Should have saved to the database Interactions._updateDatabase.callCount: ${Interactions._updateDatabase.callCount},
     expected.length: ${expected.length}`
  );

  let args = Interactions._updateDatabase.args;
  for (let i = 0; i < expected.length; i++) {
    let actual = args[i][0];
    Assert.equal(
      actual.url,
      expected[i].url,
      "Should have saved the interaction into the database"
    );

    if (expected[i].keypresses) {
      Assert.equal(
        actual.keypresses,
        expected[i].keypresses,
        "Should have saved the keypresses into the database"
      );
    }

    if (expected[i].exactTypingTime) {
      Assert.equal(
        actual.typingTime,
        expected[i].exactTypingTime,
        "Should have stored the exact typing time."
      );
    } else if (expected[i].typingTimeIsGreaterThan) {
      Assert.greater(
        actual.typingTime,
        expected[i].typingTimeIsGreaterThan,
        "Should have stored at least this amount of typing time."
      );
    } else if (expected[i].typingTimeIsLessThan) {
      Assert.less(
        actual.typingTime,
        expected[i].typingTimeIsLessThan,
        "Should have stored less than this amount of typing time."
      );
    }
  }
}

async function sendTextToInput(browser, text) {
  await SpecialPowers.spawn(browser, [], function() {
    const input = content.document.querySelector(
      "#form1 > input[name='search']"
    );
    input.focus();
  });
  await EventUtils.sendString(text);
}

add_task(async function test_load_and_navigate_away_no_keypresses() {
  sinon.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    BrowserTestUtils.loadURI(browser, TEST_URL2);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_URL2);

    await assertDatabaseValues([
      {
        url: TEST_URL,
        keypresses: 0,
        exactTypingTime: 0,
      },
    ]);

    BrowserTestUtils.loadURI(browser, "about:blank");
    await BrowserTestUtils.browserLoaded(browser, false, "about:blank");

    await assertDatabaseValues([
      {
        url: TEST_URL,
        keypresses: 0,
        exactTypingTime: 0,
      },
      {
        url: TEST_URL2,
        keypresses: 0,
        exactTypingTime: 0,
      },
    ]);
  });
});

add_task(async function test_load_type_and_navigate_away() {
  sinon.reset();

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await sendTextToInput(browser, sentence);

    BrowserTestUtils.loadURI(browser, TEST_URL2);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_URL2);

    await assertDatabaseValues([
      {
        url: TEST_URL,
        keypresses: sentence.length,
        typingTimeIsGreaterThan: 0,
      },
    ]);

    BrowserTestUtils.loadURI(browser, "about:blank");
    await BrowserTestUtils.browserLoaded(browser, false, "about:blank");

    await assertDatabaseValues([
      {
        url: TEST_URL,
        keypresses: sentence.length,
        typingTimeIsGreaterThan: 0,
      },
      {
        url: TEST_URL2,
        keypresses: 0,
        exactTypingTime: 0,
      },
    ]);
  });
});

add_task(async function test_no_typing_close_tab() {
  sinon.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {});

  await assertDatabaseValues([
    {
      url: TEST_URL,
      keypresses: 0,
      exactTypingTime: 0,
    },
  ]);
});

add_task(async function test_typing_close_tab() {
  sinon.reset();

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await sendTextToInput(browser, sentence);
  });

  await assertDatabaseValues([
    {
      url: TEST_URL,
      keypresses: sentence.length,
      typingTimeIsGreaterThan: 0,
    },
  ]);
});

add_task(async function test_single_key_typing_and_delay() {
  sinon.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    // Single keystrokes with a delay between each, are not considered typing
    const text = ["T", "h", "e"];

    for (let i = 0; i < text.length; i++) {
      await sendTextToInput(browser, text[i]);

      // We do need to wait here because typing is defined as a series of keystrokes followed by a delay.
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(r => setTimeout(r, POST_TYPING_DELAY));
    }
  });

  // Since we typed single keys with delays between each, there should be no typing added to the database
  await assertDatabaseValues([
    {
      url: TEST_URL,
      keypresses: 0,
      exactTypingTime: 0,
    },
  ]);
});

add_task(async function test_double_key_typing_and_delay() {
  sinon.reset();

  const text = ["Ab", "cd", "ef"];

  const testStartTime = Cu.now();

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    for (let i = 0; i < text.length; i++) {
      await sendTextToInput(browser, text[i]);

      // We do need to wait here because typing is defined as a series of keystrokes followed by a delay.
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(r => setTimeout(r, POST_TYPING_DELAY));
    }
  });

  await assertDatabaseValues([
    {
      url: TEST_URL,
      keypresses: text.reduce(
        (accumulator, current) => accumulator + current.length,
        0
      ),
      typingTimeIsGreaterThan: 0,
      typingTimeIsLessThan: Cu.now() - testStartTime,
    },
  ]);
});

add_task(async function test_typing_and_delay() {
  sinon.reset();

  const testStartTime = Cu.now();

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    for (let i = 0; i < sentenceFragments.length; i++) {
      await sendTextToInput(browser, sentenceFragments[i]);

      // We do need to wait here because typing is defined as a series of keystrokes followed by a delay.
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(r => setTimeout(r, POST_TYPING_DELAY));
    }
  });

  await assertDatabaseValues([
    {
      url: TEST_URL,
      keypresses: sentenceFragments.reduce(
        (accumulator, current) => accumulator + current.length,
        0
      ),
      typingTimeIsGreaterThan: 0,
      typingTimeIsLessThan: Cu.now() - testStartTime,
    },
  ]);
});

add_task(async function test_typing_and_reload() {
  sinon.reset();

  const testStartTime = Cu.now();

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await sendTextToInput(browser, sentenceFragments[0]);

    info("reload");
    browser.reload();
    await BrowserTestUtils.browserLoaded(browser);

    // First typing should have been recorded
    await assertDatabaseValues([
      {
        url: TEST_URL,
        keypresses: sentenceFragments[0].length,
        typingTimeIsGreaterThan: 0,
      },
    ]);

    await sendTextToInput(browser, sentenceFragments[1]);

    info("reload");
    browser.reload();
    await BrowserTestUtils.browserLoaded(browser);

    // Second typing should have been recorded
    await assertDatabaseValues([
      {
        url: TEST_URL,
        keypresses: sentenceFragments[0].length,
        typingTimeIsGreaterThan: 0,
        typingTimeIsLessThan: Cu.now() - testStartTime,
      },
      {
        url: TEST_URL,
        keypresses: sentenceFragments[1].length,
        typingTimeIsGreaterThan: 0,
        typingTimeIsLessThan: Cu.now() - testStartTime,
      },
    ]);
  });
});

add_task(async function test_switch_tabs_no_typing() {
  sinon.reset();

  let tab1 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
  });

  let tab2 = BrowserTestUtils.addTab(gBrowser, TEST_URL2);
  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser, false, TEST_URL2);

  info("Switch to second tab");
  gBrowser.selectedTab = tab2;

  // Only the interaction of the first tab should be recorded so far, and with no typing
  await assertDatabaseValues([
    {
      url: TEST_URL,
      keypresses: 0,
      exactTypingTime: 0,
    },
  ]);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

add_task(async function test_typing_switch_tabs() {
  sinon.reset();

  let tab1 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
  });

  await sendTextToInput(tab1.linkedBrowser, sentence);

  let tab2 = BrowserTestUtils.addTab(gBrowser, TEST_URL3);
  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser, false, TEST_URL3);

  info("Switch to second tab");
  await BrowserTestUtils.switchTab(gBrowser, tab2);

  // Only the interaction of the first tab should be recorded so far
  await assertDatabaseValues([
    {
      url: TEST_URL,
      keypresses: sentence.length,
      typingTimeIsGreaterThan: 0,
    },
  ]);

  const tab1TyingTime = Interactions._updateDatabase.args[0][0].typingTime;

  info("Switch back to first tab");
  await BrowserTestUtils.switchTab(gBrowser, tab1);

  // The interaction of the second tab should now be recorded (no typing)
  await assertDatabaseValues([
    {
      url: TEST_URL,
      keypresses: sentence.length,
      exactTypingTime: tab1TyingTime,
    },
    {
      url: TEST_URL3,
      keypresses: 0,
      typingTimeIsGreaterThan: 0,
    },
  ]);

  info("Switch back to the second tab");
  await BrowserTestUtils.switchTab(gBrowser, tab2);

  // Typing into the second tab
  await SpecialPowers.spawn(tab2.linkedBrowser, [], function() {
    const input = content.document.getElementById("input_text");
    input.focus();
  });
  await EventUtils.sendString(longSentence);

  info("Switch back to first tab");
  await BrowserTestUtils.switchTab(gBrowser, tab1);

  // The interaction of the second tab should now also be recorded (with typing)
  await assertDatabaseValues([
    {
      url: TEST_URL,
      keypresses: sentence.length,
      exactTypingTime: tab1TyingTime,
    },
    {
      url: TEST_URL3,
      keypresses: 0,
      typingTimeIsGreaterThan: 0,
    },
    {
      url: TEST_URL,
      keypresses: 0,
      typingTimeIsGreaterThan: 0,
    },
    {
      url: TEST_URL3,
      keypresses: longSentence.length,
      typingTimeIsGreaterThan: 0,
    },
  ]);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});
