/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// When the heuristic result is not the first result added, it should still be
// selected.

"use strict";

// When the heuristic result is not the first result added, it should still be
// selected.
add_task(async function slowHeuristicSelected() {
  // First, add a provider that adds a heuristic result on a delay.  Both this
  // provider and the one below have a high priority so that only they are used
  // during the test.
  let engine = await Services.search.getDefault();
  let heuristicResult = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.SEARCH,
    UrlbarUtils.RESULT_SOURCE.SEARCH,
    {
      suggestion: "test",
      engine: engine.name,
    }
  );
  heuristicResult.heuristic = true;
  let heuristicProvider = new UrlbarTestUtils.TestProvider({
    results: [heuristicResult],
    name: "heuristicProvider",
    priority: Infinity,
    addTimeout: 500,
  });
  UrlbarProvidersManager.registerProvider(heuristicProvider);

  // Second, add another provider that adds a non-heuristic result immediately
  // with suggestedIndex = 1.
  let nonHeuristicResult = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.TIP,
    UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
    {
      text: "This is a test tip.",
      buttonText: "Done",
      type: "test",
      helpUrl: "http://example.com/",
    }
  );
  nonHeuristicResult.suggestedIndex = 1;
  let nonHeuristicProvider = new UrlbarTestUtils.TestProvider({
    results: [nonHeuristicResult],
    name: "nonHeuristicProvider",
    priority: Infinity,
  });
  UrlbarProvidersManager.registerProvider(nonHeuristicProvider);

  // Do a search.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "test",
    window,
    waitForFocus: SimpleTest.waitForFocus,
  });

  // The first result should be the heuristic and it should be selected.
  let actualHeuristic = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(actualHeuristic.type, UrlbarUtils.RESULT_TYPE.SEARCH);
  Assert.equal(
    UrlbarTestUtils.getSelectedElement(window),
    actualHeuristic.element.row
  );
  Assert.equal(UrlbarTestUtils.getSelectedElementIndex(window), 0);

  // Check the second result for good measure.
  let actualNonHeuristic = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    1
  );
  Assert.equal(actualNonHeuristic.type, UrlbarUtils.RESULT_TYPE.TIP);

  await UrlbarTestUtils.promisePopupClose(window);
  UrlbarProvidersManager.unregisterProvider(heuristicProvider);
  UrlbarProvidersManager.unregisterProvider(nonHeuristicProvider);
});

// When the heuristic result is not the first result added but a one-off search
// button is already selected, the heuristic result should not steal the
// selection from the one-off button.
add_task(async function oneOffRemainsSelected() {
  // First, add a provider that adds a heuristic result on a delay.  Both this
  // provider and the one below have a high priority so that only they are used
  // during the test.
  let engine = await Services.search.getDefault();
  let heuristicResult = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.SEARCH,
    UrlbarUtils.RESULT_SOURCE.SEARCH,
    {
      suggestion: "test",
      engine: engine.name,
    }
  );
  heuristicResult.heuristic = true;
  let heuristicProvider = new UrlbarTestUtils.TestProvider({
    results: [heuristicResult],
    name: "heuristicProvider",
    priority: Infinity,
    addTimeout: 500,
  });
  UrlbarProvidersManager.registerProvider(heuristicProvider);

  // Second, add another provider that adds a non-heuristic result immediately
  // with suggestedIndex = 1.
  let nonHeuristicResult = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.TIP,
    UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
    {
      text: "This is a test tip.",
      buttonText: "Done",
      type: "test",
      helpUrl: "http://example.com/",
    }
  );
  nonHeuristicResult.suggestedIndex = 1;
  let nonHeuristicProvider = new UrlbarTestUtils.TestProvider({
    results: [nonHeuristicResult],
    name: "nonHeuristicProvider",
    priority: Infinity,
  });
  UrlbarProvidersManager.registerProvider(nonHeuristicProvider);

  // Do a search but don't wait for it to finish.
  let searchPromise = UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "test",
    window,
    waitForFocus: SimpleTest.waitForFocus,
  });

  // When the view opens, press the up arrow key to select the one-off search
  // settings button.  There's no point in selecting instead the non-heuristic
  // result because once we do that, the search is canceled, and the heuristic
  // result will never be added.
  await UrlbarTestUtils.promisePopupOpen(window, () => {});
  EventUtils.synthesizeKey("KEY_ArrowUp");

  // Wait for the search to finish.
  await searchPromise;

  // The first result should be the heuristic.
  let actualHeuristic = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(actualHeuristic.type, UrlbarUtils.RESULT_TYPE.SEARCH);

  // Check the second result for good measure.
  let actualNonHeuristic = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    1
  );
  Assert.equal(actualNonHeuristic.type, UrlbarUtils.RESULT_TYPE.TIP);

  // No result should be selected.
  Assert.equal(UrlbarTestUtils.getSelectedElement(window), null);
  Assert.equal(UrlbarTestUtils.getSelectedElementIndex(window), -1);

  // The one-off settings button should be selected.
  Assert.equal(
    window.gURLBar.view.oneOffSearchButtons.selectedButton,
    window.gURLBar.view.oneOffSearchButtons.settingsButtonCompact
  );

  await UrlbarTestUtils.promisePopupClose(window);
  UrlbarProvidersManager.unregisterProvider(heuristicProvider);
  UrlbarProvidersManager.unregisterProvider(nonHeuristicProvider);
});
