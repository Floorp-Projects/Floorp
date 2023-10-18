/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test the behavior of the tab and the urlbar when opening about:blank by clicking link.

ChromeUtils.defineESModuleGetters(this, {
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.sys.mjs",
});

/* import-globals-from common_link_in_tab_title_and_url_prefilled.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/base/content/test/tabs/common_link_in_tab_title_and_url_prefilled.js",
  this
);

add_task(async function blank_target__foreground() {
  await doTestInSameWindow({
    link: "blank-page--blank-target",
    openBy: OPEN_BY.CLICK,
    openAs: OPEN_AS.FOREGROUND,
    loadingState: {
      tab: BLANK_TITLE,
      urlbar: "",
    },
    async actionWhileLoading(onTabLoaded) {
      info("Wait until loading the link target");
      await onTabLoaded;
    },
    finalState: {
      tab: BLANK_TITLE,
      urlbar: "",
      history: [BLANK_URL],
    },
  });
});

add_task(async function blank_target__background() {
  await doTestInSameWindow({
    link: "blank-page--blank-target",
    openBy: OPEN_BY.CLICK,
    openAs: OPEN_AS.BACKGROUND,
    loadingState: {
      tab: BLANK_TITLE,
      urlbar: UrlbarTestUtils.trimURL(HOME_URL),
    },
    async actionWhileLoading(onTabLoaded) {
      info("Wait until loading the link target");
      await onTabLoaded;
    },
    finalState: {
      tab: BLANK_TITLE,
      urlbar: UrlbarTestUtils.trimURL(HOME_URL),
      history: [BLANK_URL],
    },
  });
});

add_task(async function other_target__foreground() {
  await doTestInSameWindow({
    link: "blank-page--other-target",
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
      tab: BLANK_TITLE,
      urlbar: UrlbarTestUtils.trimURL(BLANK_URL),
      history: [BLANK_URL],
    },
  });
});

add_task(async function other_target__background() {
  await doTestInSameWindow({
    link: "blank-page--other-target",
    openBy: OPEN_BY.CLICK,
    openAs: OPEN_AS.BACKGROUND,
    loadingState: {
      tab: BLANK_TITLE,
      urlbar: UrlbarTestUtils.trimURL(HOME_URL),
    },
    async actionWhileLoading(onTabLoaded) {
      info("Wait until loading the link target");
      await onTabLoaded;
    },
    finalState: {
      tab: BLANK_TITLE,
      urlbar: UrlbarTestUtils.trimURL(HOME_URL),
      history: [BLANK_URL],
    },
  });
});

add_task(async function by_script() {
  await doTestInSameWindow({
    link: "blank-page--by-script",
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
      tab: BLANK_TITLE,
      urlbar: UrlbarTestUtils.trimURL(BLANK_URL),
      history: [BLANK_URL],
    },
  });
});

add_task(async function no_target() {
  await doTestInSameWindow({
    link: "blank-page--no-target",
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
      tab: BLANK_TITLE,
      urlbar: UrlbarTestUtils.trimURL(BLANK_URL),
      history: [HOME_URL, BLANK_URL],
    },
  });
});
