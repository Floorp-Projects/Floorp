/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that if the selectedElement is removed from the DOM, the view still
// sets a selection on the next received results.

add_task(async function () {
  let view = gURLBar.view;
  // We need a heuristic provider that the Muxer will prefer over other
  // heuristics and that will return results after the first onQueryResults.
  // Luckily TEST providers come first in the heuristic group!
  let result = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.URL,
    UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
    { url: "https://example.com/1", title: "example" }
  );
  result.heuristic = true;
  // To ensure the selectedElement is removed, we use this special property that
  // asks the view to generate new content for the row.
  result.testForceNewContent = true;

  let receivedResults = false;
  let firstSelectedElement;
  let delayResultsPromise = new Promise(resolve => {
    gURLBar.controller.addQueryListener({
      async onQueryResults(queryContext) {
        Assert.ok(!receivedResults, "Should execute only once");
        gURLBar.controller.removeQueryListener(this);
        receivedResults = true;
        // Store the corrent selection.
        firstSelectedElement = view.selectedElement;
        Assert.ok(firstSelectedElement, "There should be a selected element");
        Assert.ok(
          view.selectedResult.heuristic,
          "Selected result should be a heuristic"
        );
        Assert.notEqual(
          result,
          view.selectedResult,
          "Should not immediately select our result"
        );
        resolve();
      },
    });
  });

  let delayedHeuristicProvider = new UrlbarTestUtils.TestProvider({
    delayResultsPromise,
    results: [result],
    type: UrlbarUtils.PROVIDER_TYPE.HEURISTIC,
  });
  UrlbarProvidersManager.registerProvider(delayedHeuristicProvider);
  registerCleanupFunction(async function () {
    UrlbarProvidersManager.unregisterProvider(delayedHeuristicProvider);
    await UrlbarTestUtils.promisePopupClose(window);
    gURLBar.handleRevert();
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "exa",
  });
  Assert.ok(receivedResults, "Results observer was invoked");
  Assert.ok(
    UrlbarTestUtils.getResultCount(window) > 0,
    `There should be some results in the view.`
  );
  Assert.ok(view.isOpen, `The view should be open.`);
  Assert.ok(view.selectedElement.isConnected, "selectedElement is connected");
  Assert.equal(view.selectedElementIndex, 0, "selectedElementIndex is correct");
  Assert.deepEqual(
    view.getResultFromElement(view.selectedElement),
    result,
    "result is the expected one"
  );
  Assert.notEqual(
    view.selectedElement,
    firstSelectedElement,
    "Selected element should have changed"
  );
  Assert.ok(
    !firstSelectedElement.isConnected,
    "Previous selected element should be disconnected"
  );
});
