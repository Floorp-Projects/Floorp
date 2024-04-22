/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// General tests for the result menu that aren't related to specific result
// types.

"use strict";

const MAX_RESULTS = UrlbarPrefs.get("maxRichResults");
const RESULT_URL = "https://example.com/test";
const RESULT_HELP_URL = "https://example.com/help";
const DEFAULT_HELP_URL =
  Services.urlFormatter.formatURLPref("app.support.baseURL") +
  "awesome-bar-result-menu";

add_setup(async function () {
  // Add enough results to fill up the view.
  await PlacesUtils.history.clear();
  for (let i = 0; i < MAX_RESULTS; i++) {
    await PlacesTestUtils.addVisits("https://example.com/" + i);
  }
  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
  });
});

// Creates a provider that sets `helpUrl` on its result payload. The specified
// `helpUrl` should be loaded when the result's help menu item is clicked.
add_task(async function help_resultHelpUrl() {
  await doHelpTest({
    provider: registerTestProvider({
      resultHelpUrl: RESULT_HELP_URL,
    }),
    expectedHelpUrl: RESULT_HELP_URL,
    expectedHelpL10n: { id: "urlbar-result-menu-tip-get-help", args: null },
  });
});

// Creates a provider that does not set `helpUrl` on its result payload but
// instead implements `getResultCommands()` and returns a "help" command. The
// default help URL should be loaded when the result's help menu item is
// clicked.
add_task(async function help_getResultCommands() {
  let provider = registerTestProvider({
    resultHelpUrl: null,
  });
  let l10n = { id: "urlbar-result-menu-learn-more", args: null };
  provider.getResultCommands = () => [{ l10n, name: "help" }];

  await doHelpTest({
    provider,
    expectedHelpUrl: DEFAULT_HELP_URL,
    expectedHelpL10n: l10n,
  });
});

async function doHelpTest({
  provider,
  expectedHelpUrl,
  expectedHelpL10n,
  expectedResultIndex = 1,
}) {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "example",
    window,
  });

  await assertIsTestResult(expectedResultIndex);

  let result = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    expectedResultIndex
  );
  let menuButton = result.element.row._buttons.get("menu");
  Assert.ok(menuButton, "Sanity check: menu button should exist");

  let menuitem = await UrlbarTestUtils.openResultMenuAndGetItem({
    window,
    command: "help",
    resultIndex: expectedResultIndex,
    openByMouse: true,
  });
  Assert.ok(menuitem, "Help menu item should exist");

  let l10nAttrs = document.l10n.getAttributes(menuitem);
  Assert.deepEqual(
    l10nAttrs,
    expectedHelpL10n,
    "The l10n ID attribute was correctly set"
  );

  // The result menu needs to be closed before calling
  // `openResultMenuAndClickItem()` below; otherwise it will wait on a
  // `popupshown` event that will never come.
  gURLBar.view.resultMenu.hidePopup(true);

  // We assume clicking "help" will load a page in a new tab.
  let loadPromise = BrowserTestUtils.waitForNewTab(gBrowser);

  await UrlbarTestUtils.openResultMenuAndClickItem(window, "help", {
    resultIndex: expectedResultIndex,
    openByMouse: true,
  });

  info("Waiting for load");
  await loadPromise;
  await TestUtils.waitForTick();
  Assert.equal(
    gBrowser.currentURI.spec,
    expectedHelpUrl,
    "The load URL should be the help URL"
  );
  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  UrlbarProvidersManager.unregisterProvider(provider);
}

// (SHIFT+)TABs through a result with a menu button. The result is the second
// result and has other results after it.
add_task(async function keyboardSelection_secondResult() {
  let provider = registerTestProvider();
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

  info("Arrow down to the main part of the result.");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  assertMainPartSelected(1);

  info("TAB to the button.");
  EventUtils.synthesizeKey("KEY_Tab");
  assertButtonSelected(2);

  info("TAB to the next (third) result.");
  EventUtils.synthesizeKey("KEY_Tab");
  assertOtherResultSelected(3, "next result");

  info("SHIFT+TAB to the menu button.");
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  assertButtonSelected(2);

  info("SHIFT+TAB to the main part of the result.");
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  assertMainPartSelected(1);

  info("Arrow up to the previous (first) result.");
  EventUtils.synthesizeKey("KEY_ArrowUp");
  assertOtherResultSelected(0, "previous result");

  await UrlbarTestUtils.promisePopupClose(window);
  UrlbarProvidersManager.unregisterProvider(provider);
});

// (SHIFT+)TABs through a result with a help button.  The result is the
// last result.
add_task(async function keyboardSelection_lastResult() {
  let provider = registerTestProvider({ suggestedIndex: MAX_RESULTS - 1 });
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

  let numSelectable = MAX_RESULTS * 2 - 2;

  // Arrow down to the main part of the result.
  EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: MAX_RESULTS - 1 });
  assertMainPartSelected(numSelectable - 1);

  // TAB to the menu button.
  EventUtils.synthesizeKey("KEY_Tab");
  assertButtonSelected(numSelectable);

  // Arrow down to the first one-off.  If this test is running alone, the
  // one-offs will rebuild themselves when the view is opened above, and they
  // may not be visible yet.  Wait for the first one to become visible before
  // trying to select it.
  await TestUtils.waitForCondition(() => {
    return (
      gURLBar.view.oneOffSearchButtons.buttons.firstElementChild &&
      BrowserTestUtils.isVisible(
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

  // SHIFT+TAB to the menu button.
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  assertButtonSelected(numSelectable);

  // SHIFT+TAB to the main part of the result.
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  assertMainPartSelected(numSelectable - 1);

  // Arrow up to the previous result.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  assertOtherResultSelected(numSelectable - 3, "previous result");

  await UrlbarTestUtils.promisePopupClose(window);
  UrlbarProvidersManager.unregisterProvider(provider);
});

// Picks the main part of the test result with the keyboard.
add_task(async function pick_mainPart_keyboard() {
  await doPickTest({ pickHelp: false, useKeyboard: true });
});

// Picks the help command with the keyboard.
add_task(async function pick_help_keyboard() {
  await doPickTest({ pickHelp: true, useKeyboard: true });
});

// Picks the main part of the test result with the mouse.
add_task(async function pick_mainPart_mouse() {
  await doPickTest({ pickHelp: false, useKeyboard: false });
});

// Picks the help command with the mouse.
add_task(async function pick_help_mouse() {
  await doPickTest({ pickHelp: true, useKeyboard: false });
});

async function doPickTest({ pickHelp, useKeyboard }) {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    let index = 1;
    let provider = registerTestProvider({ suggestedIndex: index });
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
    await assertIsTestResult(index);

    if (useKeyboard) {
      // Arrow down to the result.
      EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: index });
      assertMainPartSelected(index * 2 - 1);
    }

    // Pick the result.  The appropriate URL should load.
    let loadPromise = pickHelp
      ? BrowserTestUtils.waitForNewTab(gBrowser)
      : BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    await Promise.all([
      loadPromise,
      UrlbarTestUtils.promisePopupClose(window, async () => {
        if (pickHelp) {
          await UrlbarTestUtils.openResultMenuAndPressAccesskey(window, "h", {
            openByMouse: !useKeyboard,
            resultIndex: index,
          });
        } else if (useKeyboard) {
          EventUtils.synthesizeKey("KEY_Enter");
        } else {
          let result = await UrlbarTestUtils.getDetailsOfResultAt(
            window,
            index
          );
          EventUtils.synthesizeMouseAtCenter(result.element.row._content, {});
        }
      }),
    ]);
    Assert.equal(
      gBrowser.selectedBrowser.currentURI.spec,
      pickHelp ? RESULT_HELP_URL : RESULT_URL,
      "Expected URL should have loaded"
    );

    if (pickHelp) {
      BrowserTestUtils.removeTab(gBrowser.selectedTab);
    }
    UrlbarProvidersManager.unregisterProvider(provider);

    // Avoid showing adaptive history autofill.
    await PlacesTestUtils.clearInputHistory();
  });
}

/**
 * Registers a provider that creates a result optionally with a help URL.
 *
 * @param {object} options
 *   Options object
 * @param {number} options.suggestedIndex
 *   The result's suggestedIndex.
 * @param {string} options.resultHelpUrl
 *   The result's helpUrl. Pass a falsey value to prevent setting a helpUrl.
 * @returns {UrlbarProvider}
 *   The new provider.
 */
function registerTestProvider({
  suggestedIndex = 1,
  resultHelpUrl = RESULT_HELP_URL,
} = {}) {
  let payload = { url: RESULT_URL };
  if (resultHelpUrl) {
    payload = {
      ...payload,
      helpUrl: resultHelpUrl,
      helpL10n: {
        id: "urlbar-result-menu-tip-get-help",
      },
    };
  }

  let provider = new UrlbarTestUtils.TestProvider({
    results: [
      Object.assign(
        new UrlbarResult(
          UrlbarUtils.RESULT_TYPE.URL,
          UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          payload
        ),
        { suggestedIndex }
      ),
    ],
  });
  UrlbarProvidersManager.registerProvider(provider);
  return provider;
}

/**
 * Asserts that the result at the given index is our test result with a menu
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
  Assert.ok(row._buttons.get("menu"), "The result should have a menu button");
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
    `Expected selected element: ${msg} (${
      UrlbarTestUtils.getSelectedElement(window).classList
    } == ${expectedClassName})`
  );
}

/**
 * Asserts that the main part of our test result is selected.
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
 * Asserts that the menu button is selected.
 *
 * @param {number} expectedSelectedElementIndex
 *   The expected selected element index.
 */
function assertButtonSelected(expectedSelectedElementIndex) {
  assertSelection(
    expectedSelectedElementIndex,
    "urlbarView-button-menu",
    "menu button"
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
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    expectedSelectedElementIndex,
    "Expected other selected element index: " + msg
  );
}
