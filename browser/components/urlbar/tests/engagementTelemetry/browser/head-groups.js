/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

async function doHeuristicsTest({ trigger, assert }) {
  await doTest(async browser => {
    await openPopup("x");

    await trigger();
    await assert();
  });
}

async function doAdaptiveHistoryTest({ trigger, assert }) {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.autoFill", false]],
  });

  await doTest(async browser => {
    await PlacesTestUtils.addVisits(["https://example.com/test"]);
    await UrlbarUtils.addToInputHistory("https://example.com/test", "examp");

    await openPopup("exa");
    await selectRowByURL("https://example.com/test");

    await trigger();
    await assert();
  });

  await SpecialPowers.popPrefEnv();
}

async function doSearchHistoryTest({ trigger, assert }) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.searches", true],
      ["browser.urlbar.maxHistoricalSearchSuggestions", 2],
    ],
  });

  await doTest(async browser => {
    await UrlbarTestUtils.formHistory.add(["foofoo", "foobar"]);

    await openPopup("foo");
    await selectRowByURL("http://mochi.test:8888/?terms=foofoo");

    await trigger();
    await assert();
  });

  await SpecialPowers.popPrefEnv();
}

async function doSearchSuggestTest({ trigger, assert }) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.searches", true],
      ["browser.urlbar.maxHistoricalSearchSuggestions", 2],
    ],
  });

  await doTest(async browser => {
    await openPopup("foo");
    await selectRowByURL("http://mochi.test:8888/?terms=foofoo");

    await trigger();
    await assert();
  });

  await SpecialPowers.popPrefEnv();
}

async function doTopPickTest({ trigger, assert }) {
  const cleanupQuickSuggest = await ensureQuickSuggestInit();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.bestMatch.enabled", true]],
  });

  await doTest(async browser => {
    await openPopup("sponsored");
    await selectRowByURL("https://example.com/sponsored");

    await trigger();
    await assert();
  });

  await SpecialPowers.popPrefEnv();
  cleanupQuickSuggest();
}

async function doTopSiteTest({ trigger, assert }) {
  await doTest(async browser => {
    await addTopSites("https://example.com/");

    await showResultByArrowDown();
    await selectRowByURL("https://example.com/");

    await trigger();
    await assert();
  });
}

async function doRemoteTabTest({ trigger, assert }) {
  const remoteTab = await loadRemoteTab("https://example.com");

  await doTest(async browser => {
    await openPopup("example");
    await selectRowByProvider("RemoteTabs");

    await trigger();
    await assert();
  });

  await remoteTab.unload();
}

async function doAddonTest({ trigger, assert }) {
  const addon = loadOmniboxAddon({ keyword: "omni" });
  await addon.startup();

  await doTest(async browser => {
    await openPopup("omni test");

    await trigger();
    await assert();
  });

  await addon.unload();
}

async function doGeneralBookmarkTest({ trigger, assert }) {
  await doTest(async browser => {
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url: "https://example.com/bookmark",
      title: "bookmark",
    });

    await openPopup("bookmark");
    await selectRowByURL("https://example.com/bookmark");

    await trigger();
    await assert();
  });
}

async function doGeneralHistoryTest({ trigger, assert }) {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.autoFill", false]],
  });

  await doTest(async browser => {
    await PlacesTestUtils.addVisits("https://example.com/test");

    await openPopup("example");
    await selectRowByURL("https://example.com/test");

    await trigger();
    await assert();
  });

  await SpecialPowers.popPrefEnv();
}

async function doSuggestTest({ trigger, assert }) {
  const cleanupQuickSuggest = await ensureQuickSuggestInit();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.bestMatch.enabled", false]],
  });

  await doTest(async browser => {
    await openPopup("nonsponsored");
    await selectRowByURL("https://example.com/nonsponsored");

    await trigger();
    await assert();
  });

  await SpecialPowers.popPrefEnv();
  cleanupQuickSuggest();
}

async function doAboutPageTest({ trigger, assert }) {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.maxRichResults", 3]],
  });

  await doTest(async browser => {
    await openPopup("about:");
    await selectRowByURL("about:robots");

    await trigger();
    await assert();
  });

  await SpecialPowers.popPrefEnv();
}

async function doSuggestedIndexTest({ trigger, assert }) {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.unitConversion.enabled", true]],
  });

  await doTest(async browser => {
    await openPopup("1m to cm");
    await selectRowByProvider("UnitConversion");

    await trigger();
    await assert();
  });

  await SpecialPowers.popPrefEnv();
}
