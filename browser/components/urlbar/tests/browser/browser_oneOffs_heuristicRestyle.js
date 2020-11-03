/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that non-search heuristic results are updated to search results when a
 * one-off is selected.
 */

"use strict";

XPCOMUtils.defineLazyGetter(this, "oneOffSearchButtons", () => {
  return UrlbarTestUtils.getOneOffSearchButtons(window);
});

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

async function urlResultIsNotRestyled(resultDetails) {
  Assert.equal(
    resultDetails.type,
    UrlbarUtils.RESULT_TYPE.URL,
    "The restyled result is a URL result."
  );
  Assert.equal(
    resultDetails.displayed.title,
    resultDetails.title,
    "The displayed title is equal to the payload title."
  );
  let [actionText] = await document.l10n.formatValues([
    { id: "urlbar-result-action-visit" },
  ]);
  Assert.equal(
    resultDetails.displayed.action,
    actionText,
    "The result's action text is unchanged."
  );
}

async function urlResultIsRestyled(
  resultDetails,
  searchString,
  selectedOneOff
) {
  let engine = selectedOneOff.engine;
  let source = selectedOneOff.source;
  if (!engine && !source) {
    Assert.fail("An invalid one-off was passed to urlbarResultIsRestyled");
    return;
  }
  Assert.equal(
    resultDetails.type,
    UrlbarUtils.RESULT_TYPE.URL,
    "The restyled result is still a URL result."
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
  Assert.notEqual(
    resultDetails.displayed.title,
    resultDetails.title,
    "The displayed title is different from the payload title."
  );
  Assert.equal(
    resultDetails.displayed.title,
    searchString,
    "The restyled result's title should be equal to the search string."
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

async function assertState(result, oneOff, textValue = undefined) {
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    result,
    "Expected result should be selected"
  );
  Assert.equal(
    oneOffSearchButtons.selectedButtonIndex,
    oneOff,
    "Expected one-off should be selected"
  );
  if (textValue !== undefined) {
    Assert.equal(gURLBar.value, textValue, "Expected value");
  }
  if (result == 0) {
    let resultDetails = await UrlbarTestUtils.getDetailsOfResultAt(
      window,
      result
    );
    if (oneOff >= 0) {
      await urlResultIsRestyled(
        resultDetails,
        textValue,
        oneOffSearchButtons.selectedButton
      );
    } else {
      await urlResultIsNotRestyled(resultDetails);
    }
  }
}

add_task(async function init() {
  let oldDefaultEngine = await Services.search.getDefault();
  let engine = await Services.search.addEngineWithDetails("Test", {
    template: `http://example.com/?search={searchTerms}`,
  });
  await Services.search.setDefault(engine);
  await Services.search.moveEngine(engine, 0);

  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits("https://mozilla.org/");
  }

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update2", true],
      ["browser.urlbar.update2.oneOffsRefresh", true],
      ["browser.urlbar.update2.localOneOffs", true],
      ["browser.urlbar.suggest.searches", false],
    ],
  });
  registerCleanupFunction(async function() {
    await Services.search.setDefault(oldDefaultEngine);
    await Services.search.removeEngine(engine);
    await PlacesUtils.history.clear();
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

add_task(async function basic() {
  const typedValue = "mozilla.or";
  let rebuildPromise = BrowserTestUtils.waitForEvent(
    oneOffSearchButtons,
    "rebuild"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: typedValue,
    fireInputEvent: true,
  });
  await rebuildPromise;
  let originalResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  await assertState(0, -1, `${typedValue}g/`);

  // Alt+Down to the first one-off.  Now the first result and the first one-off
  // should both be selected. The input is still autofilled to `mozilla.org/`.
  info(
    "Alt+Arrow to one-offs, observe heuristic is restyled as a search result."
  );
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
  await searchPromise;
  await assertState(0, 0, typedValue);

  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
  await assertState(0, 1, typedValue);

  info("Select the second result and observe the heuristic is restored.");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(
    result.image,
    originalResult.image,
    "The original heuristic icon was restored."
  );
  EventUtils.synthesizeKey("KEY_ArrowUp");
  await assertState(0, -1, `${typedValue}g/`);

  info("Cycle past the one-offs and observe the heuristic is restored.");
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
  await assertState(0, 0, typedValue);

  EventUtils.synthesizeKey("KEY_ArrowUp", { altKey: true });
  await assertState(0, -1, typedValue);

  info(
    "Cycle through the one-offs, observe heuristic is restyled as a search result."
  );
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
  await assertState(0, 0, typedValue);

  info("Click the heuristic result and observe it confirms search mode.");
  let selectedButton = oneOffSearchButtons.selectedButton;
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: selectedButton.engine.name,
    entry: "oneoff",
    isPreview: true,
  });
  let heuristicRow = await UrlbarTestUtils.waitForAutocompleteResultAt(
    window,
    0
  );
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeMouseAtCenter(heuristicRow, {});
  await searchPromise;
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: selectedButton.engine.name,
    entry: "oneoff",
    isPreview: false,
  });

  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
});

add_task(async function localOneOff() {
  const typedValue = "mozilla.org";
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: typedValue,
    fireInputEvent: true,
  });

  let numButtons = oneOffSearchButtons.getSelectableButtons(false).length;
  await assertState(0, -1, `${typedValue}/`);

  info("Select the history one-off.");
  EventUtils.synthesizeKey("KEY_ArrowUp", { altKey: true });
  await assertState(0, numButtons - 1, typedValue);

  info("Select the tabs one-off and check favicon and action text change.");
  EventUtils.synthesizeKey("KEY_ArrowUp", { altKey: true });
  await assertState(0, numButtons - 2, typedValue);

  await UrlbarTestUtils.promisePopupClose(window);
});
