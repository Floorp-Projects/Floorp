/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test reporting of scrolling interactions.
 */

"use strict";

const TEST_URL =
  "https://example.com/browser/browser/components/places/tests/browser/interactions/scrolling.html";
const TEST_URL2 = "https://example.com/browser";

async function waitForScrollEvent(aBrowser) {
  await BrowserTestUtils.waitForContentEvent(aBrowser, "scroll");
}

add_task(async function test_no_scrolling() {
  await Interactions.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    BrowserTestUtils.loadURI(browser, TEST_URL2);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_URL2);

    await assertDatabaseValues([
      {
        url: TEST_URL,
        exactscrollingDistance: 0,
        exactscrollingTime: 0,
      },
    ]);
  });
});

add_task(async function test_arrow_key_down_scroll() {
  await Interactions.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await SpecialPowers.spawn(browser, [], function() {
      const heading = content.document.getElementById("heading");
      heading.focus();
    });

    await EventUtils.synthesizeKey("KEY_ArrowDown");

    await waitForScrollEvent(browser);

    BrowserTestUtils.loadURI(browser, TEST_URL2);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_URL2);

    await assertDatabaseValues([
      {
        url: TEST_URL,
        scrollingDistanceIsGreaterThan: 0,
        scrollingTimeIsGreaterThan: 0,
      },
    ]);
  });
});

add_task(async function test_scrollIntoView() {
  await Interactions.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await SpecialPowers.spawn(browser, [], function() {
      const heading = content.document.getElementById("middleHeading");
      heading.scrollIntoView();
    });

    waitForScrollEvent(browser);

    BrowserTestUtils.loadURI(browser, TEST_URL2);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_URL2);

    // JS-triggered scrolling should not be reported
    await assertDatabaseValues([
      {
        url: TEST_URL,
        exactscrollingDistance: 0,
        exactscrollingTime: 0,
      },
    ]);
  });
});

add_task(async function test_anchor_click() {
  await Interactions.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await SpecialPowers.spawn(browser, [], function() {
      const anchor = content.document.getElementById("to_bottom_anchor");
      anchor.click();
    });

    waitForScrollEvent(browser);

    BrowserTestUtils.loadURI(browser, TEST_URL2);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_URL2);

    // The scrolling resulting from clicking on an anchor should not be reported
    await assertDatabaseValues([
      {
        url: TEST_URL,
        exactscrollingDistance: 0,
        exactscrollingTime: 0,
      },
    ]);
  });
});

add_task(async function test_window_scrollBy() {
  await Interactions.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await SpecialPowers.spawn(browser, [], function() {
      content.scrollBy(0, 100);
    });

    waitForScrollEvent(browser);

    BrowserTestUtils.loadURI(browser, TEST_URL2);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_URL2);

    // The scrolling resulting from the window.scrollBy() call should not be reported
    await assertDatabaseValues([
      {
        url: TEST_URL,
        exactscrollingDistance: 0,
        exactscrollingTime: 0,
      },
    ]);
  });
});

add_task(async function test_window_scrollTo() {
  await Interactions.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await SpecialPowers.spawn(browser, [], function() {
      content.scrollTo(0, 200);
    });

    waitForScrollEvent(browser);

    BrowserTestUtils.loadURI(browser, TEST_URL2);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_URL2);

    // The scrolling resulting from the window.scrollTo() call should not be reported
    await assertDatabaseValues([
      {
        url: TEST_URL,
        exactscrollingDistance: 0,
        exactscrollingTime: 0,
      },
    ]);
  });
});
