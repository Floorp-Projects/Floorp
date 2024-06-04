/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test the behavior of the tab and the urlbar when opening normal web page by
// clicking link that the target is "other".

ChromeUtils.defineESModuleGetters(this, {
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.sys.mjs",
});

/* import-globals-from common_link_in_tab_title_and_url_prefilled.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/tabbrowser/test/browser/tabs/common_link_in_tab_title_and_url_prefilled.js",
  this
);

add_task(async function normal_page__other_target__foreground() {
  await doTestInSameWindow({
    link: "wait-a-bit--other-target",
    openBy: OPEN_BY.CLICK,
    openAs: OPEN_AS.FOREGROUND,
    loadingState: {
      tab: BLANK_TITLE,
      urlbar: UrlbarTestUtils.trimURL(BLANK_URL),
    },
    async actionWhileLoading(onTabLoaded) {
      info("Wait until loading the link target");
      await onTabLoaded;
    },
    finalState: {
      tab: WAIT_A_BIT_PAGE_TITLE,
      urlbar: UrlbarTestUtils.trimURL(WAIT_A_BIT_URL),
      history: [WAIT_A_BIT_URL],
    },
  });
});

add_task(async function normal_page__other_target__foreground__abort() {
  await doTestInSameWindow({
    link: "wait-a-bit--other-target",
    openBy: OPEN_BY.CLICK,
    openAs: OPEN_AS.FOREGROUND,
    loadingState: {
      tab: BLANK_TITLE,
      urlbar: UrlbarTestUtils.trimURL(BLANK_URL),
    },
    async actionWhileLoading() {
      info("Abort loading");
      document.getElementById("stop-button").click();
    },
    finalState: {
      tab: BLANK_TITLE,
      urlbar: UrlbarTestUtils.trimURL(BLANK_URL),
      history: [],
    },
  });
});

add_task(async function normal_page__other_target__foreground__timeout() {
  await doTestInSameWindow({
    link: "request-timeout--other-target",
    openBy: OPEN_BY.CLICK,
    openAs: OPEN_AS.FOREGROUND,
    loadingState: {
      tab: BLANK_TITLE,
      urlbar: UrlbarTestUtils.trimURL(BLANK_URL),
    },
    async actionWhileLoading(onTabLoaded) {
      info("Wait until loading the link target");
      await onTabLoaded;
    },
    finalState: {
      tab: REQUEST_TIMEOUT_LOADING_TITLE,
      urlbar: UrlbarTestUtils.trimURL(REQUEST_TIMEOUT_URL),
      history: [REQUEST_TIMEOUT_URL],
    },
  });
});

add_task(async function normal_page__foreground__session_restore() {
  await doSessionRestoreTest({
    link: "wait-a-bit--other-target",
    openBy: OPEN_BY.CLICK,
    openAs: OPEN_AS.FOREGROUND,
    expectedSessionRestored: false,
  });
});

add_task(async function normal_page__other_target__background() {
  await doTestInSameWindow({
    link: "wait-a-bit--other-target",
    openBy: OPEN_BY.CLICK,
    openAs: OPEN_AS.BACKGROUND,
    loadingState: {
      tab: WAIT_A_BIT_LOADING_TITLE,
      urlbar: UrlbarTestUtils.trimURL(HOME_URL),
    },
    async actionWhileLoading(onTabLoaded) {
      info("Wait until loading the link target");
      await onTabLoaded;
    },
    finalState: {
      tab: WAIT_A_BIT_PAGE_TITLE,
      urlbar: UrlbarTestUtils.trimURL(HOME_URL),
      history: [WAIT_A_BIT_URL],
    },
  });
});

add_task(async function normal_page__other_target__background__abort() {
  await doTestInSameWindow({
    link: "wait-a-bit--other-target",
    openBy: OPEN_BY.CLICK,
    openAs: OPEN_AS.BACKGROUND,
    loadingState: {
      tab: WAIT_A_BIT_LOADING_TITLE,
      urlbar: UrlbarTestUtils.trimURL(HOME_URL),
    },
    async actionWhileLoading() {
      info("Abort loading");
      document.getElementById("stop-button").click();
    },
    finalState: {
      tab: WAIT_A_BIT_LOADING_TITLE,
      urlbar: UrlbarTestUtils.trimURL(HOME_URL),
      history: [],
    },
  });
});

add_task(async function normal_page__other_target__background__timeout() {
  await doTestInSameWindow({
    link: "request-timeout--other-target",
    openBy: OPEN_BY.CLICK,
    openAs: OPEN_AS.BACKGROUND,
    loadingState: {
      tab: REQUEST_TIMEOUT_LOADING_TITLE,
      urlbar: UrlbarTestUtils.trimURL(HOME_URL),
    },
    async actionWhileLoading(onTabLoaded) {
      info("Wait until loading the link target");
      await onTabLoaded;
    },
    finalState: {
      tab: REQUEST_TIMEOUT_LOADING_TITLE,
      urlbar: UrlbarTestUtils.trimURL(HOME_URL),
      history: [REQUEST_TIMEOUT_URL],
    },
  });
});

add_task(async function normal_page__foreground__session_restore() {
  await doSessionRestoreTest({
    link: "wait-a-bit--other-target",
    openBy: OPEN_BY.CLICK,
    openAs: OPEN_AS.BACKGROUND,
    expectedSessionRestored: false,
  });
});
