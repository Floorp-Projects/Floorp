/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests clicks and enter key presses on UrlbarUtils.RESULT_TYPE.TIP results.

"use strict";

const TIP_URL = "http://example.com/tip";
const HELP_URL = "http://example.com/help";

add_setup(async function () {
  window.windowUtils.disableNonTestMouseEvents(true);
  registerCleanupFunction(() => {
    window.windowUtils.disableNonTestMouseEvents(false);
  });
  Services.telemetry.clearScalars();
});

add_task(async function enter_mainButton_url() {
  await doTest({ click: false, buttonUrl: TIP_URL });
});

add_task(async function enter_mainButton_noURL() {
  await doTest({ click: false });
});

add_task(async function enter_help() {
  await doTest({ click: false, helpUrl: HELP_URL });
});

add_task(async function mouse_mainButton_url() {
  await doTest({ click: true, buttonUrl: TIP_URL });
});

add_task(async function mouse_mainButton_noURL() {
  await doTest({ click: true });
});

add_task(async function mouse_help() {
  await doTest({ click: true, helpUrl: HELP_URL });
});

// Clicks inside a tip but not on any button.
add_task(async function mouse_insideTipButNotOnButtons() {
  let results = [makeTipResult({ buttonUrl: TIP_URL, helpUrl: HELP_URL })];
  let provider = new UrlbarTestUtils.TestProvider({ results, priority: 1 });
  UrlbarProvidersManager.registerProvider(provider);

  // Click inside the tip but outside the buttons.  Nothing should happen.  Make
  // the result the heuristic to check that the selection on the main button
  // isn't lost.
  results[0].heuristic = true;
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "test",
    window,
    fireInputEvent: true,
  });
  let row = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    0,
    "The main button's index should be selected initially"
  );
  Assert.equal(
    UrlbarTestUtils.getSelectedElement(window),
    row._buttons.get("0"),
    "The main button element should be selected initially"
  );
  EventUtils.synthesizeMouseAtCenter(row, {});
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(r => setTimeout(r, 500));
  Assert.ok(gURLBar.view.isOpen, "The view should remain open");
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    0,
    "The main button's index should remain selected"
  );
  Assert.equal(
    UrlbarTestUtils.getSelectedElement(window),
    row._buttons.get("0"),
    "The main button element should remain selected"
  );

  await UrlbarTestUtils.promisePopupClose(window);
  UrlbarProvidersManager.unregisterProvider(provider);
});

/**
 * Runs this test's main checks.
 *
 * @param {object} options
 *   Options for the test.
 * @param {boolean} options.click
 *   Pass true to trigger a click, false to trigger an enter key.
 * @param {string} [options.buttonUrl]
 *   Pass a URL if picking the main button should open a URL.  Pass nothing if
 *   a URL shouldn't be opened or if you want to pick the help button instead of
 *   the main button.
 * @param {string} [options.helpUrl]
 *   Pass a URL if you want to pick the help button.  Pass nothing if you want
 *   to pick the main button instead.
 */
async function doTest({ click, buttonUrl = undefined, helpUrl = undefined }) {
  // Open a new tab for the test if we expect to load a URL.
  let tab;
  if (buttonUrl || helpUrl) {
    tab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      url: "about:blank",
    });
  }

  // Add our test provider.
  let provider = new UrlbarTestUtils.TestProvider({
    results: [makeTipResult({ buttonUrl, helpUrl })],
    priority: 1,
  });
  UrlbarProvidersManager.registerProvider(provider);

  let onEngagementPromise = new Promise(
    resolve => (provider.onEngagement = resolve)
  );

  // Do a search to show our tip result.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "test",
    window,
    fireInputEvent: true,
  });
  let row = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  let mainButton = row._buttons.get("0");
  let target = helpUrl ? row._buttons.get("menu") : mainButton;

  // If we're picking the tip with the keyboard, TAB to select the proper
  // target.
  if (!click) {
    EventUtils.synthesizeKey("KEY_Tab", { repeat: helpUrl ? 2 : 1 });
    Assert.equal(
      UrlbarTestUtils.getSelectedElement(window),
      target,
      `${target.className} should be selected.`
    );
  }

  // Now pick the target and wait for provider.onEngagement to be called and
  // the URL to load if necessary.
  let loadPromise;
  if (buttonUrl || helpUrl) {
    loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  }
  await UrlbarTestUtils.promisePopupClose(window, () => {
    if (helpUrl) {
      UrlbarTestUtils.openResultMenuAndPressAccesskey(window, "h", {
        openByMouse: click,
        resultIndex: 0,
      });
    } else if (click) {
      EventUtils.synthesizeMouseAtCenter(target, {});
    } else {
      EventUtils.synthesizeKey("KEY_Enter");
    }
  });
  await onEngagementPromise;
  await loadPromise;

  // Check telemetry.
  let scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "urlbar.tips",
    helpUrl ? "test-help" : "test-picked",
    1
  );
  // Done.
  UrlbarProvidersManager.unregisterProvider(provider);
  if (tab) {
    BrowserTestUtils.removeTab(tab);
  }
}

function makeTipResult({ buttonUrl, helpUrl }) {
  return new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.TIP,
    UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
    {
      type: "test",
      titleL10n: { id: "urlbar-search-tips-confirm" },
      buttons: [
        {
          url: buttonUrl,
          l10n: { id: "urlbar-search-tips-confirm" },
        },
      ],
      helpUrl,
      helpL10n: {
        id: "urlbar-result-menu-tip-get-help",
      },
    }
  );
}
