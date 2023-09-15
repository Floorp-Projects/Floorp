/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test reporting of scrolling interactions after DOM history API use.
 */

"use strict";

const TEST_URL =
  "https://example.com/browser/browser/components/places/tests/browser/interactions/scrolling.html";
const TEST_URL2 = "https://example.com/browser";

async function waitForScrollEvent(aBrowser, aTask) {
  let promise = BrowserTestUtils.waitForContentEvent(aBrowser, "scroll");

  // This forces us to send a message to the browser's process and receive a response which ensures
  // that the message sent to register the scroll event listener will also have been processed by
  // the content process. Without this it is possible for our scroll task to send a higher priority
  // message which can be processed by the content process before the message to register the scroll
  // event listener.
  await SpecialPowers.spawn(aBrowser, [], () => {});

  await aTask();
  await promise;
}

add_task(async function test_scroll_pushState() {
  await Interactions.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await SpecialPowers.spawn(browser, [], function () {
      const heading = content.document.getElementById("heading");
      heading.focus();
    });

    await waitForScrollEvent(browser, () =>
      EventUtils.synthesizeKey("KEY_ArrowDown")
    );

    await ContentTask.spawn(browser, TEST_URL2, url => {
      content.history.pushState(null, "", url);
    });

    await waitForScrollEvent(browser, () =>
      EventUtils.synthesizeKey("KEY_ArrowDown")
    );

    BrowserTestUtils.startLoadingURIString(browser, "about:blank");
    await BrowserTestUtils.browserLoaded(browser, false, "about:blank");

    await assertDatabaseValues([
      {
        url: TEST_URL,
        scrollingDistanceIsGreaterThan: 0,
        scrollingTimeIsGreaterThan: 0,
      },
      {
        url: TEST_URL2,
        scrollingDistanceIsGreaterThan: 0,
        scrollingTimeIsGreaterThan: 0,
      },
    ]);
  });
});

add_task(async function test_scroll_pushState_sameUrl() {
  await Interactions.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await SpecialPowers.spawn(browser, [], function () {
      const heading = content.document.getElementById("heading");
      heading.focus();
    });

    await waitForScrollEvent(browser, () =>
      EventUtils.synthesizeKey("KEY_ArrowDown")
    );

    await ContentTask.spawn(browser, TEST_URL, url => {
      content.history.pushState(null, "", url);
    });

    // As the page hasn't changed there will be no interactions saved yet.
    await assertDatabaseValues([]);

    await waitForScrollEvent(browser, () =>
      EventUtils.synthesizeKey("KEY_ArrowDown")
    );

    BrowserTestUtils.startLoadingURIString(browser, "about:blank");
    await BrowserTestUtils.browserLoaded(browser, false, "about:blank");

    await assertDatabaseValues([
      {
        url: TEST_URL,
        scrollingDistanceIsGreaterThan: 0,
        scrollingTimeIsGreaterThan: 0,
      },
    ]);
  });
});

add_task(async function test_scroll_replaceState() {
  await Interactions.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await SpecialPowers.spawn(browser, [], function () {
      const heading = content.document.getElementById("heading");
      heading.focus();
    });

    await waitForScrollEvent(browser, () =>
      EventUtils.synthesizeKey("KEY_ArrowDown")
    );

    await ContentTask.spawn(browser, TEST_URL2, url => {
      content.history.replaceState(null, "", url);
    });

    await waitForScrollEvent(browser, () =>
      EventUtils.synthesizeKey("KEY_ArrowDown")
    );

    BrowserTestUtils.startLoadingURIString(browser, "about:blank");
    await BrowserTestUtils.browserLoaded(browser, false, "about:blank");

    await assertDatabaseValues([
      {
        url: TEST_URL,
        scrollingDistanceIsGreaterThan: 0,
        scrollingTimeIsGreaterThan: 0,
      },
      {
        url: TEST_URL2,
        scrollingDistanceIsGreaterThan: 0,
        scrollingTimeIsGreaterThan: 0,
      },
    ]);
  });
});

add_task(async function test_scroll_replaceState_sameUrl() {
  await Interactions.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await SpecialPowers.spawn(browser, [], function () {
      const heading = content.document.getElementById("heading");
      heading.focus();
    });

    await waitForScrollEvent(browser, () =>
      EventUtils.synthesizeKey("KEY_ArrowDown")
    );

    await ContentTask.spawn(browser, TEST_URL, url => {
      content.history.replaceState(null, "", url);
    });

    // As the page hasn't changed there will be no interactions saved yet.
    await assertDatabaseValues([]);

    await waitForScrollEvent(browser, () =>
      EventUtils.synthesizeKey("KEY_ArrowDown")
    );

    BrowserTestUtils.startLoadingURIString(browser, "about:blank");
    await BrowserTestUtils.browserLoaded(browser, false, "about:blank");

    await assertDatabaseValues([
      {
        url: TEST_URL,
        scrollingDistanceIsGreaterThan: 0,
        scrollingTimeIsGreaterThan: 0,
      },
    ]);
  });
});

add_task(async function test_scroll_hashchange() {
  await Interactions.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await SpecialPowers.spawn(browser, [], function () {
      const heading = content.document.getElementById("heading");
      heading.focus();
    });

    await waitForScrollEvent(browser, () =>
      EventUtils.synthesizeKey("KEY_ArrowDown")
    );

    await ContentTask.spawn(browser, TEST_URL + "#foo", url => {
      content.history.replaceState(null, "", url);
    });

    await waitForScrollEvent(browser, () =>
      EventUtils.synthesizeKey("KEY_ArrowDown")
    );

    BrowserTestUtils.startLoadingURIString(browser, "about:blank");
    await BrowserTestUtils.browserLoaded(browser, false, "about:blank");

    await assertDatabaseValues([
      {
        url: TEST_URL,
        scrollingDistanceIsGreaterThan: 0,
        scrollingTimeIsGreaterThan: 0,
      },
    ]);
  });
});
