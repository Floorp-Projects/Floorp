/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

async function doNotSearchModeTest({ trigger, assert }) {
  await doTest(async browser => {
    await openPopup("x");

    await trigger();
    await assert();
  });
}

async function doSearchEngineTest({ trigger, assert }) {
  await doTest(async browser => {
    await openPopup("x");
    await UrlbarTestUtils.enterSearchMode(window);

    await trigger();
    await assert();
  });
}

async function doBookmarksTest({ trigger, assert }) {
  await doTest(async browser => {
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url: "https://example.com/bookmark",
      title: "bookmark",
    });
    await openPopup("bookmark");
    await UrlbarTestUtils.enterSearchMode(window, {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    });
    await selectRowByURL("https://example.com/bookmark");

    await trigger();
    await assert();
  });
}

async function doHistoryTest({ trigger, assert }) {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.autoFill", false]],
  });

  await doTest(async browser => {
    await PlacesTestUtils.addVisits("https://example.com/test");
    await openPopup("example");
    await UrlbarTestUtils.enterSearchMode(window, {
      source: UrlbarUtils.RESULT_SOURCE.HISTORY,
    });
    await selectRowByURL("https://example.com/test");

    await trigger();
    await assert();
  });

  await SpecialPowers.popPrefEnv();
}

async function doTabTest({ trigger, assert }) {
  const tab = BrowserTestUtils.addTab(gBrowser, "https://example.com/");

  await doTest(async browser => {
    await openPopup("example");
    await UrlbarTestUtils.enterSearchMode(window, {
      source: UrlbarUtils.RESULT_SOURCE.TABS,
    });
    await selectRowByProvider("Places");

    await trigger();
    await assert();
  });

  BrowserTestUtils.removeTab(tab);
}

async function doActionsTest({ trigger, assert }) {
  await doTest(async browser => {
    await openPopup("add");
    await UrlbarTestUtils.enterSearchMode(window, {
      source: UrlbarUtils.RESULT_SOURCE.ACTIONS,
    });
    await selectRowByProvider("quickactions");

    await trigger();
    await assert();
  });
}
