/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that heuristic results are updated/restyled to search results when a
 * one-off is selected.
 */

"use strict";

ChromeUtils.defineLazyGetter(this, "oneOffSearchButtons", () => {
  return UrlbarTestUtils.getOneOffSearchButtons(window);
});

const TEST_DEFAULT_ENGINE_NAME = "Test";

const HISTORY_URL = "https://mozilla.org/";

const KEYWORD = "kw";
const KEYWORD_URL = "https://mozilla.org/search?q=%s";

// Expected result data for our test results.
const RESULT_DATA_BY_TYPE = {
  [UrlbarUtils.RESULT_TYPE.URL]: {
    icon: `page-icon:${HISTORY_URL}`,
    actionL10n: {
      id: "urlbar-result-action-visit",
    },
  },
  [UrlbarUtils.RESULT_TYPE.SEARCH]: {
    icon: "chrome://global/skin/icons/search-glass.svg",
    actionL10n: {
      id: "urlbar-result-action-search-w-engine",
      args: { engine: TEST_DEFAULT_ENGINE_NAME },
    },
  },
  [UrlbarUtils.RESULT_TYPE.KEYWORD]: {
    icon: `page-icon:${KEYWORD_URL}`,
  },
};

function getSourceIcon(source) {
  switch (source) {
    case UrlbarUtils.RESULT_SOURCE.BOOKMARKS:
      return "chrome://browser/skin/bookmark.svg";
    case UrlbarUtils.RESULT_SOURCE.HISTORY:
      return "chrome://browser/skin/history.svg";
    case UrlbarUtils.RESULT_SOURCE.TABS:
      return "chrome://browser/skin/tab.svg";
    default:
      return null;
  }
}

/**
 * Asserts that the heuristic result is *not* restyled to look like a search
 * result.
 *
 * @param {UrlbarUtils.RESULT_TYPE} expectedType
 *   The expected type of the heuristic.
 * @param {object} resultDetails
 *   The return value of UrlbarTestUtils.getDetailsOfResultAt(window, 0).
 */
async function heuristicIsNotRestyled(expectedType, resultDetails) {
  Assert.equal(
    resultDetails.type,
    expectedType,
    "The restyled result is the expected type."
  );

  Assert.equal(
    resultDetails.displayed.title,
    resultDetails.title,
    "The displayed title is equal to the payload title."
  );

  let data = RESULT_DATA_BY_TYPE[expectedType];
  Assert.ok(data, "Sanity check: Expected type is recognized");

  let [actionText] = data.actionL10n
    ? await document.l10n.formatValues([data.actionL10n])
    : [""];

  if (
    expectedType === UrlbarUtils.RESULT_TYPE.URL &&
    resultDetails.result.heuristic &&
    resultDetails.result.payload.title
  ) {
    Assert.equal(
      resultDetails.displayed.url,
      resultDetails.result.payload.displayUrl
    );
  } else {
    Assert.equal(
      resultDetails.displayed.action,
      actionText,
      "The result has the expected non-styled action text."
    );
  }

  Assert.equal(
    BrowserTestUtils.is_visible(resultDetails.element.separator),
    !!actionText,
    "The title separator is " + (actionText ? "visible" : "hidden")
  );
  Assert.equal(
    BrowserTestUtils.is_visible(resultDetails.element.action),
    !!actionText,
    "The action text is " + (actionText ? "visible" : "hidden")
  );

  Assert.equal(
    resultDetails.image,
    data.icon,
    "The result has the expected non-styled icon."
  );
}

/**
 * Asserts that the heuristic result is restyled to look like a search result.
 *
 * @param {UrlbarUtils.RESULT_TYPE} expectedType
 *   The expected type of the heuristic.
 * @param {object} resultDetails
 *   The return value of UrlbarTestUtils.getDetailsOfResultAt(window, 0).
 * @param {string} searchString
 *   The current search string.  The restyled heuristic result's title is
 *   expected to be this string.
 * @param {element} selectedOneOff
 *   The selected one-off button.
 */
async function heuristicIsRestyled(
  expectedType,
  resultDetails,
  searchString,
  selectedOneOff
) {
  let engine = selectedOneOff.engine;
  let source = selectedOneOff.source;
  if (!engine && !source) {
    Assert.ok(false, "An invalid one-off was passed to urlbarResultIsRestyled");
    return;
  }
  Assert.equal(
    resultDetails.type,
    expectedType,
    "The restyled result is still the expected type."
  );

  let actionText;
  if (engine) {
    [actionText] = await document.l10n.formatValues([
      {
        id: "urlbar-result-action-search-w-engine",
        args: { engine: engine.name },
      },
    ]);
  } else if (source) {
    [actionText] = await document.l10n.formatValues([
      {
        id: `urlbar-result-action-search-${UrlbarUtils.getResultSourceName(
          source
        )}`,
      },
    ]);
  }
  Assert.equal(
    resultDetails.displayed.action,
    actionText,
    "Restyled result's action text should be updated"
  );

  Assert.equal(
    resultDetails.displayed.title,
    searchString,
    "The restyled result's title should be equal to the search string."
  );

  Assert.ok(
    BrowserTestUtils.is_visible(resultDetails.element.separator),
    "The restyled result's title separator should be visible"
  );
  Assert.ok(
    BrowserTestUtils.is_visible(resultDetails.element.action),
    "The restyled result's action text should be visible"
  );

  if (engine) {
    Assert.equal(
      resultDetails.image,
      engine.iconURI?.spec || UrlbarUtils.ICON.SEARCH_GLASS,
      "The restyled result's icon should be the engine's icon."
    );
  } else if (source) {
    Assert.equal(
      resultDetails.image,
      getSourceIcon(source),
      "The restyled result's icon should be the local one-off's icon."
    );
  }
}

/**
 * Asserts that the specified one-off (if any) is selected and that the
 * heuristic result is either restyled or not restyled as appropriate.  If
 * there's a selected one-off, then the heuristic is expected to be restyled; if
 * there's no selected one-off, then it's expected not to be restyled.
 *
 * @param {string} searchString
 *   The current search string.  If a one-off is selected, then the restyled
 *   heuristic result's title is expected to be this string.
 * @param {UrlbarUtils.RESULT_TYPE} expectedHeuristicType
 *   The expected type of the heuristic.
 * @param {number} expectedSelectedOneOffIndex
 *   The index of the expected selected one-off button.  If no one-off is
 *   expected to be selected, then pass -1.
 */
async function assertState(
  searchString,
  expectedHeuristicType,
  expectedSelectedOneOffIndex
) {
  Assert.equal(
    oneOffSearchButtons.selectedButtonIndex,
    expectedSelectedOneOffIndex,
    "Expected one-off should be selected"
  );

  let resultDetails = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  if (expectedSelectedOneOffIndex >= 0) {
    await heuristicIsRestyled(
      expectedHeuristicType,
      resultDetails,
      searchString,
      oneOffSearchButtons.selectedButton
    );
  } else {
    await heuristicIsNotRestyled(expectedHeuristicType, resultDetails);
  }
}

add_setup(async function () {
  await SearchTestUtils.installSearchExtension(
    {
      name: TEST_DEFAULT_ENGINE_NAME,
      keyword: "@test",
    },
    { setAsDefault: true }
  );
  let engine = Services.search.getEngineByName(TEST_DEFAULT_ENGINE_NAME);
  await Services.search.moveEngine(engine, 0);

  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits(HISTORY_URL);
  }

  await PlacesUtils.keywords.insert({
    keyword: KEYWORD,
    url: KEYWORD_URL,
  });

  registerCleanupFunction(async function () {
    await PlacesUtils.history.clear();
    await PlacesUtils.keywords.remove(KEYWORD);
  });

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.searches", false]],
  });

  // Move the mouse away from the view so that a result or one-off isn't
  // inadvertently highlighted.  See bug 1659011.
  EventUtils.synthesizeMouse(
    gURLBar.inputField,
    0,
    0,
    { type: "mousemove" },
    window
  );
});

add_task(async function arrow_engine_url() {
  await doArrowTest("mozilla.or", UrlbarUtils.RESULT_TYPE.URL, false);
});

add_task(async function arrow_engine_search() {
  await doArrowTest("test", UrlbarUtils.RESULT_TYPE.SEARCH, false);
});

add_task(async function arrow_engine_keyword() {
  await doArrowTest(`${KEYWORD} test`, UrlbarUtils.RESULT_TYPE.KEYWORD, false);
});

add_task(async function arrow_local_url() {
  await doArrowTest("mozilla.or", UrlbarUtils.RESULT_TYPE.URL, true);
});

add_task(async function arrow_local_search() {
  await doArrowTest("test", UrlbarUtils.RESULT_TYPE.SEARCH, true);
});

add_task(async function arrow_local_keyword() {
  await doArrowTest(`${KEYWORD} test`, UrlbarUtils.RESULT_TYPE.KEYWORD, true);
});

/**
 * Arrows down to the one-offs, checks the heuristic, and clicks it.
 *
 * @param {string} searchString
 *   The search string to use.
 * @param {UrlbarUtils.RESULT_TYPE} expectedHeuristicType
 *   The type of heuristic result that the search string is expected to trigger.
 * @param {boolean} useLocal
 *   Whether to test a local one-off or an engine one-off.  If true, test a
 *   local one-off.  If false, test an engine one-off.
 */
async function doArrowTest(searchString, expectedHeuristicType, useLocal) {
  await doTest(searchString, expectedHeuristicType, useLocal, async () => {
    info(
      "Arrow down to the one-offs, observe heuristic is restyled as a search result."
    );
    let resultCount = UrlbarTestUtils.getResultCount(window);
    let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: resultCount });
    await searchPromise;
    await assertState(searchString, expectedHeuristicType, 0);

    let depth = 1;
    if (useLocal) {
      for (; !oneOffSearchButtons.selectedButton.source; depth++) {
        EventUtils.synthesizeKey("KEY_ArrowDown");
      }
      Assert.ok(
        oneOffSearchButtons.selectedButton.source,
        "Selected one-off is local"
      );
      await assertState(searchString, expectedHeuristicType, depth - 1);
    }

    info(
      "Arrow up out of the one-offs, observe heuristic styling is restored."
    );
    EventUtils.synthesizeKey("KEY_ArrowUp", { repeat: depth });
    await assertState(searchString, expectedHeuristicType, -1);

    info(
      "Arrow back down into the one-offs, observe heuristic is restyled as a search result."
    );
    EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: depth });
    await assertState(searchString, expectedHeuristicType, depth - 1);
  });
}

add_task(async function altArrow_engine_url() {
  await doAltArrowTest("mozilla.or", UrlbarUtils.RESULT_TYPE.URL, false);
});

add_task(async function altArrow_engine_search() {
  await doAltArrowTest("test", UrlbarUtils.RESULT_TYPE.SEARCH, false);
});

add_task(async function altArrow_engine_keyword() {
  await doAltArrowTest(
    `${KEYWORD} test`,
    UrlbarUtils.RESULT_TYPE.KEYWORD,
    false
  );
});

add_task(async function altArrow_local_url() {
  await doAltArrowTest("mozilla.or", UrlbarUtils.RESULT_TYPE.URL, true);
});

add_task(async function altArrow_local_search() {
  await doAltArrowTest("test", UrlbarUtils.RESULT_TYPE.SEARCH, true);
});

add_task(async function altArrow_local_keyword() {
  await doAltArrowTest(
    `${KEYWORD} test`,
    UrlbarUtils.RESULT_TYPE.KEYWORD,
    true
  );
});

/**
 * Alt-arrows down to the one-offs so that the heuristic remains selected,
 * checks the heuristic, and clicks it.
 *
 * @param {string} searchString
 *   The search string to use.
 * @param {UrlbarUtils.RESULT_TYPE} expectedHeuristicType
 *   The type of heuristic result that the search string is expected to trigger.
 * @param {boolean} useLocal
 *   Whether to test a local one-off or an engine one-off.  If true, test a
 *   local one-off.  If false, test an engine one-off.
 */
async function doAltArrowTest(searchString, expectedHeuristicType, useLocal) {
  await doTest(searchString, expectedHeuristicType, useLocal, async () => {
    info(
      "Alt+down into the one-offs, observe heuristic is restyled as a search result."
    );
    let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    await searchPromise;
    await assertState(searchString, expectedHeuristicType, 0);

    let depth = 1;
    if (useLocal) {
      for (; !oneOffSearchButtons.selectedButton.source; depth++) {
        EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
      }
      Assert.ok(
        oneOffSearchButtons.selectedButton.source,
        "Selected one-off is local"
      );
      await assertState(searchString, expectedHeuristicType, depth - 1);
    }

    info(
      "Arrow down and then up to re-select the heuristic, observe its styling is restored."
    );
    EventUtils.synthesizeKey("KEY_ArrowDown");
    EventUtils.synthesizeKey("KEY_ArrowUp");
    await assertState(searchString, expectedHeuristicType, -1);

    info(
      "Alt+down into the one-offs, observe the heuristic is restyled as a search result."
    );
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true, repeat: depth });
    await assertState(searchString, expectedHeuristicType, depth - 1);

    info("Alt+up out of the one-offs, observe the heuristic is restored.");
    EventUtils.synthesizeKey("KEY_ArrowUp", { altKey: true, repeat: depth });
    await assertState(searchString, expectedHeuristicType, -1);

    info(
      "Alt+down into the one-offs, observe the heuristic is restyled as a search result."
    );
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true, repeat: depth });
    await assertState(searchString, expectedHeuristicType, depth - 1);
  });
}

/**
 * The main test function.  Starts a search, asserts that the heuristic has the
 * expected type, calls a callback to run more checks, and then finally clicks
 * the restyled heuristic to make sure search mode is confirmed.
 *
 * @param {string} searchString
 *   The search string to use.
 * @param {UrlbarUtils.RESULT_TYPE} expectedHeuristicType
 *   The type of heuristic result that the search string is expected to trigger.
 * @param {boolean} useLocal
 *   Whether to test a local one-off or an engine one-off.  If true, test a
 *   local one-off.  If false, test an engine one-off.
 * @param {Function} callback
 *   This is called after the search completes.  It should perform whatever
 *   checks are necessary for the test task.  Important: When it returns, it
 *   should make sure that the first one-off is selected.
 */
async function doTest(searchString, expectedHeuristicType, useLocal, callback) {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: searchString,
    fireInputEvent: true,
  });
  await TestUtils.waitForCondition(
    () => !oneOffSearchButtons._rebuilding,
    "Waiting for one-offs to finish rebuilding"
  );

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(result.heuristic, "First result is heuristic");
  Assert.equal(
    result.type,
    expectedHeuristicType,
    "Heuristic is expected type"
  );
  await assertState(searchString, expectedHeuristicType, -1);

  await callback();

  Assert.ok(
    oneOffSearchButtons.selectedButton,
    "The callback should leave a one-off selected so that the heuristic remains re-styled"
  );

  info("Click the heuristic result and observe it confirms search mode.");
  let selectedButton = oneOffSearchButtons.selectedButton;
  let expectedSearchMode = {
    entry: "oneoff",
    isPreview: true,
  };
  if (useLocal) {
    expectedSearchMode.source = selectedButton.source;
  } else {
    expectedSearchMode.engineName = selectedButton.engine.name;
  }

  await UrlbarTestUtils.assertSearchMode(window, expectedSearchMode);

  let heuristicRow = await UrlbarTestUtils.waitForAutocompleteResultAt(
    window,
    0
  );
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeMouseAtCenter(heuristicRow, {});
  await searchPromise;

  expectedSearchMode.isPreview = false;
  await UrlbarTestUtils.assertSearchMode(window, expectedSearchMode);

  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
}
