/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests page view time recording for interactions after DOM history API usage.
 */

const TEST_URL = "https://example.com/";
const TEST_URL2 = "https://example.com/browser";

add_task(async function test_interactions_pushState() {
  await Interactions.reset();

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    Interactions._pageViewStartTime = Cu.now() - 10000;

    await ContentTask.spawn(browser, TEST_URL2, url => {
      content.history.pushState(null, "", url);
    });

    Interactions._pageViewStartTime = Cu.now() - 20000;
  });

  await assertDatabaseValues([
    {
      url: TEST_URL,
      totalViewTime: 10000,
    },
    {
      url: TEST_URL2,
      totalViewTime: 20000,
    },
  ]);
});

add_task(async function test_interactions_pushState_sameUrl() {
  await Interactions.reset();

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    Interactions._pageViewStartTime = Cu.now() - 10000;

    await ContentTask.spawn(browser, TEST_URL, url => {
      content.history.pushState(null, "", url);
    });

    Interactions._pageViewStartTime = Cu.now() - 20000;
  });

  await assertDatabaseValues([
    {
      url: TEST_URL,
      totalViewTime: 20000,
    },
  ]);
});

add_task(async function test_interactions_replaceState() {
  await Interactions.reset();

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    Interactions._pageViewStartTime = Cu.now() - 10000;

    await ContentTask.spawn(browser, TEST_URL2, url => {
      content.history.replaceState(null, "", url);
    });

    Interactions._pageViewStartTime = Cu.now() - 20000;
  });

  await assertDatabaseValues([
    {
      url: TEST_URL,
      totalViewTime: 10000,
    },
    {
      url: TEST_URL2,
      totalViewTime: 20000,
    },
  ]);
});

add_task(async function test_interactions_replaceState_sameUrl() {
  await Interactions.reset();

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    Interactions._pageViewStartTime = Cu.now() - 10000;

    await ContentTask.spawn(browser, TEST_URL, url => {
      content.history.replaceState(null, "", url);
    });

    Interactions._pageViewStartTime = Cu.now() - 20000;
  });

  await assertDatabaseValues([
    {
      url: TEST_URL,
      totalViewTime: 20000,
    },
  ]);
});

add_task(async function test_interactions_hashchange() {
  await Interactions.reset();

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    Interactions._pageViewStartTime = Cu.now() - 10000;

    await ContentTask.spawn(browser, TEST_URL + "#foo", url => {
      content.location = url;
    });

    Interactions._pageViewStartTime = Cu.now() - 20000;
  });

  await assertDatabaseValues([
    {
      url: TEST_URL,
      totalViewTime: 20000,
    },
  ]);
});
