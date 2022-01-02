/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the help/info button that appears for results whose payloads have a
// `helpUrl` property.

"use strict";

const MAX_RESULTS = UrlbarPrefs.get("maxRichResults");
const RESULT_URL = "http://example.com/test";
const RESULT_HELP_URL = "http://example.com/help";

add_task(async function init() {
  // Add enough results to fill up the view.
  await PlacesUtils.history.clear();
  for (let i = 0; i < MAX_RESULTS; i++) {
    await PlacesTestUtils.addVisits("http://example.com/" + i);
  }
  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
  });
});

// Sets `helpL10nId` on the result payload and makes sure the help button ends
// up with a corresponding l10n attribute.
add_task(async function title_helpL10nId() {
  let helpL10nId = "urlbar-tip-help-icon";
  let provider = registerTestProvider(1, { helpL10nId });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "example",
    window,
  });

  await assertIsTestResult(1);

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  let helpButton = result.element.row._elements.get("helpButton");
  Assert.ok(helpButton, "Sanity check: help button should exist");

  let l10nAttrs = document.l10n.getAttributes(helpButton);
  Assert.deepEqual(
    l10nAttrs,
    { id: helpL10nId, args: null },
    "The l10n ID attribute was correctly set"
  );

  await UrlbarTestUtils.promisePopupClose(window);
  UrlbarProvidersManager.unregisterProvider(provider);
});

// Arrows up and down through a result with a help button.  The result is the
// second result and has other results after it.
add_task(async function keyboardSelection_secondResult() {
  let provider = registerTestProvider(1);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "example",
    window,
  });

  // Sanity-check initial state.
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    MAX_RESULTS,
    "There should be MAX_RESULTS results in the view"
  );
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    0,
    "The heuristic result should be selected"
  );
  await assertIsTestResult(1);

  // Arrow down to the main part of the result.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  assertMainPartSelected(1);

  // Arrow down to the help button.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  assertHelpButtonSelected(2);

  // Arrow down to the next (third) result.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  assertOtherResultSelected(3, "next result");

  // Arrow up to the help button.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  assertHelpButtonSelected(2);

  // Arrow up to the main part of the result.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  assertMainPartSelected(1);

  // Arrow up to the previous (first) result.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  assertOtherResultSelected(0, "previous result");

  await UrlbarTestUtils.promisePopupClose(window);
  UrlbarProvidersManager.unregisterProvider(provider);
});

// Arrows up and down through a result with a help button.  The result is the
// last result.
add_task(async function keyboardSelection_lastResult() {
  let provider = registerTestProvider(MAX_RESULTS - 1);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "example",
    window,
  });

  // Sanity-check initial state.
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    MAX_RESULTS,
    "There should be MAX_RESULTS results in the view"
  );
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    0,
    "The heuristic result should be selected"
  );
  await assertIsTestResult(MAX_RESULTS - 1);

  // Arrow down to the main part of the result.
  EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: MAX_RESULTS - 1 });
  assertMainPartSelected(MAX_RESULTS - 1);

  // Arrow down to the help button.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  assertHelpButtonSelected(MAX_RESULTS);

  // Arrow down to the first one-off.  If this test is running alone, the
  // one-offs will rebuild themselves when the view is opened above, and they
  // may not be visible yet.  Wait for the first one to become visible before
  // trying to select it.
  await TestUtils.waitForCondition(() => {
    return (
      gURLBar.view.oneOffSearchButtons.buttons.firstElementChild &&
      BrowserTestUtils.is_visible(
        gURLBar.view.oneOffSearchButtons.buttons.firstElementChild
      )
    );
  }, "Waiting for first one-off to become visible.");

  EventUtils.synthesizeKey("KEY_ArrowDown");
  await TestUtils.waitForCondition(() => {
    return gURLBar.view.oneOffSearchButtons.selectedButton;
  }, "Waiting for one-off to become selected.");
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    -1,
    "No results should be selected."
  );

  // Arrow up to the help button.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  assertHelpButtonSelected(MAX_RESULTS);

  // Arrow up to the main part of the result.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  assertMainPartSelected(MAX_RESULTS - 1);

  // Arrow up to the previous result.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  assertOtherResultSelected(MAX_RESULTS - 2, "previous result");

  await UrlbarTestUtils.promisePopupClose(window);
  UrlbarProvidersManager.unregisterProvider(provider);
});

// Picks the main part of the test result -- the non-help-button part -- with
// the keyboard.
add_task(async function pick_mainPart_keyboard() {
  await doPickTest({ pickHelpButton: false, useKeyboard: true });
});

// Picks the help button with the keyboard.
add_task(async function pick_helpButton_keyboard() {
  await doPickTest({ pickHelpButton: true, useKeyboard: true });
});

// Picks the main part of the test result -- the non-help-button part -- with
// the mouse.
add_task(async function pick_mainPart_mouse() {
  await doPickTest({ pickHelpButton: false, useKeyboard: false });
});

// Picks the help button with the mouse.
add_task(async function pick_helpButton_mouse() {
  await doPickTest({ pickHelpButton: true, useKeyboard: false });
});

async function doPickTest({ pickHelpButton, useKeyboard }) {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    let index = 1;
    let provider = registerTestProvider(index);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      value: "example",
      window,
    });

    // Sanity-check initial state.
    Assert.equal(
      UrlbarTestUtils.getSelectedElementIndex(window),
      0,
      "The heuristic result should be selected"
    );
    await assertIsTestResult(1);

    let clickTarget;
    if (useKeyboard) {
      // Arrow down to the result.
      if (pickHelpButton) {
        EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: index + 1 });
        assertHelpButtonSelected(index + 1);
      } else {
        EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: index });
        assertMainPartSelected(index);
      }
    } else {
      // Get the click target.
      let result = await UrlbarTestUtils.getDetailsOfResultAt(window, index);
      clickTarget = pickHelpButton
        ? result.element.row._elements.get("helpButton")
        : result.element.row._content;
      Assert.ok(
        clickTarget,
        "Click target found, pickHelpButton=" + pickHelpButton
      );
    }

    // Pick the result.  The appropriate URL should load.
    let loadPromise = pickHelpButton
      ? BrowserTestUtils.waitForNewTab(gBrowser)
      : BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    await Promise.all([
      loadPromise,
      UrlbarTestUtils.promisePopupClose(window, () => {
        if (useKeyboard) {
          EventUtils.synthesizeKey("KEY_Enter");
        } else {
          EventUtils.synthesizeMouseAtCenter(clickTarget, {});
        }
      }),
    ]);
    Assert.equal(
      gBrowser.selectedBrowser.currentURI.spec,
      pickHelpButton ? RESULT_HELP_URL : RESULT_URL,
      "Expected URL should have loaded"
    );

    if (pickHelpButton) {
      BrowserTestUtils.removeTab(gBrowser.selectedTab);
    }
    UrlbarProvidersManager.unregisterProvider(provider);
  });
}

/**
 * Registers a provider that creates a result with a help button.
 *
 * @param {number} suggestedIndex
 *   The result's suggestedIndex.
 * @param {object} [extraPayloadProperties]
 *   Properties other than `url` and `helpUrl` to set on the payload.
 * @returns {UrlbarProvider}
 *   The new provider.
 */
function registerTestProvider(suggestedIndex, extraPayloadProperties = {}) {
  let results = [
    Object.assign(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        Object.assign(
          {
            url: RESULT_URL,
            helpUrl: RESULT_HELP_URL,
          },
          extraPayloadProperties
        )
      ),
      { suggestedIndex }
    ),
  ];
  let provider = new UrlbarTestUtils.TestProvider({ results });
  UrlbarProvidersManager.registerProvider(provider);
  return provider;
}

/**
 * Asserts that the result at the given index is our test result with a help
 * button.
 *
 * @param {number} index
 *   The expected index of the test result.
 */
async function assertIsTestResult(index) {
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, index);
  Assert.equal(
    result.type,
    UrlbarUtils.RESULT_TYPE.URL,
    "The second result should be a URL"
  );
  Assert.equal(
    result.url,
    RESULT_URL,
    "The result's URL should be the expected URL"
  );

  let { row } = result.element;
  let helpButton = row._elements.get("helpButton");
  Assert.ok(helpButton, "The result should have a help button");
  Assert.ok(helpButton.id, "Help button has an ID");
  Assert.ok(row._content.id, "Row-inner has an ID");
  Assert.equal(
    row.getAttribute("role"),
    "presentation",
    "Row should have role=presentation"
  );
  Assert.equal(
    row._content.getAttribute("role"),
    "option",
    "Row-inner should have role=option"
  );
}

/**
 * Asserts that a particular element is selected.
 *
 * @param {number} expectedSelectedElementIndex
 *   The expected selected element index.
 * @param {string} expectedClassName
 *   A class name of the expected selected element.
 * @param {string} msg
 *   A string to include in the assertion message.
 */
function assertSelection(expectedSelectedElementIndex, expectedClassName, msg) {
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    expectedSelectedElementIndex,
    "Expected selected element index: " + msg
  );
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      expectedClassName
    ),
    "Expected selected element: " + msg
  );
}

/**
 * Asserts that the main part of our test resut -- the non-help-button part --
 * is selected.
 *
 * @param {number} expectedSelectedElementIndex
 *   The expected selected element index.
 */
function assertMainPartSelected(expectedSelectedElementIndex) {
  assertSelection(
    expectedSelectedElementIndex,
    "urlbarView-row-inner",
    "main part of test result"
  );
}

/**
 * Asserts that the help button part of our test resut is selected.
 *
 * @param {number} expectedSelectedElementIndex
 *   The expected selected element index.
 */
function assertHelpButtonSelected(expectedSelectedElementIndex) {
  assertSelection(
    expectedSelectedElementIndex,
    "urlbarView-help",
    "help button"
  );
}

/**
 * Asserts that a result other than our test result is selected.
 *
 * @param {number} expectedSelectedElementIndex
 *   The expected selected element index.
 * @param {string} msg
 *   A string to include in the assertion message.
 */
function assertOtherResultSelected(expectedSelectedElementIndex, msg) {
  assertSelection(expectedSelectedElementIndex, "urlbarView-row", msg);
}
