/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

const TEST_ENGINE_NAME = "Test";
const TEST_ENGINE_ALIAS = "@test";
const TEST_ENGINE_DOMAIN = "example.com";

// Each test is a function that executes an urlbar action and returns the
// expected event object.
const tests = [
  async function(win) {
    info("Type something, blur.");
    win.gURLBar.select();
    EventUtils.synthesizeKey("x", {}, win);
    win.gURLBar.blur();
    return {
      category: "urlbar",
      method: "abandonment",
      object: "blur",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "1",
        numWords: "1",
      },
    };
  },

  async function(win) {
    info("Open the panel with DOWN, don't type, blur it.");
    await addTopSite("http://example.org/");
    win.gURLBar.value = "";
    win.gURLBar.select();
    await UrlbarTestUtils.promisePopupOpen(win, () => {
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    });
    win.gURLBar.blur();
    return {
      category: "urlbar",
      method: "abandonment",
      object: "blur",
      value: "topsites",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "0",
        numWords: "0",
      },
    };
  },

  async function(win) {
    info("With pageproxystate=valid, autoopen the panel, don't type, blur it.");
    win.gURLBar.value = "";
    await UrlbarTestUtils.promisePopupOpen(win, () => {
      win.document.getElementById("Browser:OpenLocation").doCommand();
    });
    win.gURLBar.blur();
    return {
      category: "urlbar",
      method: "abandonment",
      object: "blur",
      value: "topsites",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "0",
        numWords: "0",
      },
    };
  },

  async function(win) {
    info("Enter search mode from Top Sites.");
    await updateTopSites(sites => true, /* enableSearchShorcuts */ true);

    win.gURLBar.value = "";
    win.gURLBar.select();

    await BrowserTestUtils.waitForCondition(async () => {
      await UrlbarTestUtils.promisePopupOpen(win, () => {
        EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
      });

      if (UrlbarTestUtils.getResultCount(win) > 1) {
        return true;
      }

      win.gURLBar.view.close();
      return false;
    });

    while (win.gURLBar.searchMode?.engineName != "Google") {
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    }

    let element = UrlbarTestUtils.getSelectedRow(win);
    Assert.ok(
      element.result.source == UrlbarUtils.RESULT_SOURCE.SEARCH,
      "The selected result is a search Top Site."
    );

    let engine = element.result.payload.engine;
    let searchPromise = UrlbarTestUtils.promiseSearchComplete(win);
    EventUtils.synthesizeMouseAtCenter(element, {}, win);
    await searchPromise;
    await UrlbarTestUtils.assertSearchMode(win, {
      engineName: engine,
      source: UrlbarUtils.RESULT_SOURCE.SEARCH,
      entry: "topsites_urlbar",
    });

    await UrlbarTestUtils.exitSearchMode(win);

    // To avoid needing to add a custom search shortcut Top Site, we just
    // abandon this interaction.
    await UrlbarTestUtils.promisePopupClose(win, () => {
      win.gURLBar.blur();
    });

    return {
      category: "urlbar",
      method: "abandonment",
      object: "blur",
      value: "topsites",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "0",
        numWords: "0",
      },
    };
  },

  async function(win) {
    info("Open search mode from a tab-to-search result.");
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.tabToSearch.onboard.interactionsLeft", 0]],
    });

    await PlacesUtils.history.clear();
    for (let i = 0; i < 3; i++) {
      await PlacesTestUtils.addVisits([`https://${TEST_ENGINE_DOMAIN}/`]);
    }

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: TEST_ENGINE_DOMAIN.slice(0, 4),
    });

    let tabToSearchResult = (
      await UrlbarTestUtils.waitForAutocompleteResultAt(win, 1)
    ).result;
    Assert.equal(
      tabToSearchResult.providerName,
      "TabToSearch",
      "The second result is a tab-to-search result."
    );

    // Select the tab-to-search result.
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    let searchPromise = UrlbarTestUtils.promiseSearchComplete(win);
    EventUtils.synthesizeKey("KEY_Enter", {}, win);
    await searchPromise;
    await UrlbarTestUtils.assertSearchMode(win, {
      engineName: TEST_ENGINE_NAME,
      entry: "tabtosearch",
    });

    // Abandon the interaction since simply entering search mode is not
    // considered the end of an engagement.
    await UrlbarTestUtils.promisePopupClose(win, () => {
      win.gURLBar.blur();
    });

    await PlacesUtils.history.clear();
    await SpecialPowers.popPrefEnv();

    return {
      category: "urlbar",
      method: "abandonment",
      object: "blur",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "0",
        numWords: "0",
      },
    };
  },

  async function(win) {
    info(
      "With pageproxystate=invalid, open retained results, don't type, blur it."
    );
    win.gURLBar.value = "mochi.test";
    win.gURLBar.setPageProxyState("invalid");
    await UrlbarTestUtils.promisePopupOpen(win, () => {
      win.document.getElementById("Browser:OpenLocation").doCommand();
    });
    win.gURLBar.blur();
    return {
      category: "urlbar",
      method: "abandonment",
      object: "blur",
      value: "returned",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "10",
        numWords: "1",
      },
    };
  },
];

add_task(async function init() {
  await PlacesUtils.history.clear();

  // Create a new search engine and mark it as default
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "searchSuggestionEngine.xml"
  );
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);
  await Services.search.moveEngine(engine, 0);

  await SearchTestUtils.installSearchExtension({
    name: TEST_ENGINE_NAME,
    keyword: TEST_ENGINE_ALIAS,
    search_url: `https://${TEST_ENGINE_DOMAIN}/`,
  });

  // This test used to rely on the initial timer of
  // TestUtils.waitForCondition. See bug 1667216.
  let originalWaitForCondition = TestUtils.waitForCondition;
  TestUtils.waitForCondition = async function(
    condition,
    msg,
    interval = 100,
    maxTries = 50
  ) {
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, 100));

    return originalWaitForCondition(condition, msg, interval, maxTries);
  };

  registerCleanupFunction(async function() {
    await Services.search.setDefault(oldDefaultEngine);
    await PlacesUtils.history.clear();
    TestUtils.waitForCondition = originalWaitForCondition;
  });
});

async function doTest(eventTelemetryEnabled) {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.eventTelemetry.enabled", eventTelemetryEnabled]],
  });

  const win = await BrowserTestUtils.openNewBrowserWindow();

  // This is not necessary after each loop, because assertEvents does it.
  Services.telemetry.clearEvents();
  Services.telemetry.clearScalars();

  for (let i = 0; i < tests.length; i++) {
    info(`Running test at index ${i}`);
    let events = await tests[i](win);
    if (!Array.isArray(events)) {
      events = [events];
    }
    // Always blur to ensure it's not accounted as an additional abandonment.
    win.gURLBar.setSearchMode({});
    win.gURLBar.blur();
    TelemetryTestUtils.assertEvents(eventTelemetryEnabled ? events : [], {
      category: "urlbar",
    });

    // Scalars should be recorded regardless of `eventTelemetry.enabled`.
    let scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
    TelemetryTestUtils.assertScalar(
      scalars,
      "urlbar.engagement",
      events.filter(e => e.method == "engagement").length || undefined
    );
    TelemetryTestUtils.assertScalar(
      scalars,
      "urlbar.abandonment",
      events.filter(e => e.method == "abandonment").length || undefined
    );

    await UrlbarTestUtils.formHistory.clear(win);
  }

  await BrowserTestUtils.closeWindow(win);
  await SpecialPowers.popPrefEnv();
}

add_task(async function enabled() {
  await doTest(true);
});

add_task(async function disabled() {
  await doTest(false);
});

/**
 * Replaces the contents of Top Sites with the specified site.
 * @param {string} site
 *   A site to add to Top Sites.
 */
async function addTopSite(site) {
  await PlacesUtils.history.clear();
  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits(site);
  }

  await updateTopSites(sites => sites && sites[0] && sites[0].url == site);
}
