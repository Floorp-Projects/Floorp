/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test the behavior of the tab and the urlbar when opening normal web page by
// clicking link that has no target.

/* import-globals-from common_link_in_tab_title_and_url_prefilled.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/tabbrowser/test/browser/tabs/common_link_in_tab_title_and_url_prefilled.js",
  this
);

const { UrlbarTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/UrlbarTestUtils.sys.mjs"
);

add_task(async function normal_page__no_target() {
  await doTestInSameWindow({
    link: "wait-a-bit--no-target",
    openBy: OPEN_BY.CLICK,
    openAs: OPEN_AS.FOREGROUND,
    loadingState: {
      // Inherit the title and URL until finishing loading a new link when the
      // link is opened in same tab.
      tab: HOME_TITLE,
      urlbar: UrlbarTestUtils.trimURL(HOME_URL),
    },
    async actionWhileLoading(onTabLoaded) {
      info("Wait until loading the link target");
      await onTabLoaded;
    },
    finalState: {
      tab: WAIT_A_BIT_PAGE_TITLE,
      urlbar: UrlbarTestUtils.trimURL(WAIT_A_BIT_URL),
      history: [HOME_URL, WAIT_A_BIT_URL],
    },
  });
});

add_task(async function normal_page__no_target__abort() {
  await doTestInSameWindow({
    link: "wait-a-bit--no-target",
    openBy: OPEN_BY.CLICK,
    openAs: OPEN_AS.FOREGROUND,
    loadingState: {
      tab: HOME_TITLE,
      urlbar: UrlbarTestUtils.trimURL(HOME_URL),
    },
    async actionWhileLoading() {
      info("Abort loading");
      document.getElementById("stop-button").click();
    },
    finalState: {
      tab: HOME_TITLE,
      urlbar: UrlbarTestUtils.trimURL(HOME_URL),
      history: [HOME_URL],
    },
  });
});

add_task(async function normal_page__no_target__timeout() {
  await doTestInSameWindow({
    link: "request-timeout--no-target",
    openBy: OPEN_BY.CLICK,
    openAs: OPEN_AS.FOREGROUND,
    loadingState: {
      tab: HOME_TITLE,
      urlbar: UrlbarTestUtils.trimURL(HOME_URL),
    },
    async actionWhileLoading(onTabLoaded) {
      info("Wait until loading the link target");
      await onTabLoaded;
    },
    finalState: {
      tab: REQUEST_TIMEOUT_LOADING_TITLE,
      urlbar: UrlbarTestUtils.trimURL(REQUEST_TIMEOUT_URL),
      history: [HOME_URL, REQUEST_TIMEOUT_URL],
    },
  });
});

add_task(async function normal_page__no_target__session_restore() {
  await doSessionRestoreTest({
    link: "wait-a-bit--no-target",
    openBy: OPEN_BY.CLICK,
    openAs: OPEN_AS.FOREGROUND,
    expectedSessionRestored: false,
  });
});
