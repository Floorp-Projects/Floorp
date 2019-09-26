/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const MEGABAR_PREF = "browser.urlbar.megabar";
const HELP_URL = "about:mozilla";
const TIP_URL = "about:about";

// Tests keyboard selection within UrlbarUtils.RESULT_TYPE.TIP results.

/**
 * A test provider.
 */
class TipTestProvider extends UrlbarProvider {
  constructor(matches) {
    super();
    this._matches = matches;
  }
  get name() {
    return "TipTestProvider";
  }
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }
  isActive(context) {
    return true;
  }
  isRestricting(context) {
    return true;
  }
  async startQuery(context, addCallback) {
    Assert.ok(true, "Tip provider was invoked");
    this._context = context;
    for (const match of this._matches) {
      addCallback(this, match);
    }
  }
  cancelQuery(context) {}
  pickResult(result, details) {}
}

add_task(async function tipIsSecondResult() {
  let matches = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/a" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.TIP,
      UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      {
        icon: "",
        text: "This is a test intervention.",
        buttonText: "Done",
        data: "test",
        helpUrl: HELP_URL,
        buttonUrl: TIP_URL,
      }
    ),
  ];

  let provider = new TipTestProvider(matches);
  UrlbarProvidersManager.registerProvider(provider);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "test",
    window,
    waitForFocus: SimpleTest.waitForFocus,
  });

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "There should be two results in the view."
  );
  let secondResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(
    secondResult.type,
    UrlbarUtils.RESULT_TYPE.TIP,
    "The second result should be a tip."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    0,
    "The first element should be selected."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      "urlbarView-tip-button"
    ),
    "The selected element should be the tip button."
  );
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    1,
    "The first element should be selected."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      "urlbarView-tip-help"
    ),
    "The selected element should be the tip help button."
  );
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    1,
    "getSelectedRowIndex should return 1 even though the help button is selected."
  );
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    2,
    "The third element should be selected."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  await BrowserTestUtils.waitForCondition(() => {
    info("Waiting for one-off to become selected.");
    let oneOff = document.querySelector(
      ".urlbarView .search-panel-one-offs .searchbar-engine-one-off-item:first-child"
    );
    return oneOff.hasAttribute("selected");
  });
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    -1,
    "No results should be selected."
  );

  EventUtils.synthesizeKey("KEY_ArrowUp");
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      "urlbarView-tip-help"
    ),
    "The selected element should be the tip help button."
  );

  gURLBar.view.close();
  UrlbarProvidersManager.unregisterProvider(provider);
});

add_task(async function tipIsOnlyResult() {
  let matches = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.TIP,
      UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      {
        icon: "",
        text: "This is a test intervention.",
        buttonText: "Done",
        data: "test",
        helpUrl:
          "https://support.mozilla.org/en-US/kb/delete-browsing-search-download-history-firefox",
      }
    ),
  ];

  let provider = new TipTestProvider(matches);
  UrlbarProvidersManager.registerProvider(provider);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "test",
    window,
    waitForFocus: SimpleTest.waitForFocus,
  });

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    1,
    "There should be one result in the view."
  );
  let firstResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(
    firstResult.type,
    UrlbarUtils.RESULT_TYPE.TIP,
    "The first and only result should be a tip."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      "urlbarView-tip-button"
    ),
    "The selected element should be the tip button."
  );
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    0,
    "The first element should be selected."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      "urlbarView-tip-help"
    ),
    "The selected element should be the tip help button."
  );
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    1,
    "The second element should be selected."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.equal(
    UrlbarTestUtils.getSelectedElementIndex(window),
    -1,
    "There should be no selection."
  );

  EventUtils.synthesizeKey("KEY_ArrowUp");
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      "urlbarView-tip-help"
    ),
    "The selected element should be the tip help button."
  );

  gURLBar.view.close();
  UrlbarProvidersManager.unregisterProvider(provider);
});

add_task(async function mouseSelection() {
  window.windowUtils.disableNonTestMouseEvents(true);
  registerCleanupFunction(() => {
    window.windowUtils.disableNonTestMouseEvents(false);
  });

  let matches = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.TIP,
      UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      {
        icon: "",
        text: "This is a test intervention.",
        buttonText: "Done",
        data: "test",
        helpUrl: HELP_URL,
        buttonUrl: TIP_URL,
      }
    ),
  ];

  let provider = new TipTestProvider(matches);
  UrlbarProvidersManager.registerProvider(provider);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "test",
    window,
    waitForFocus: SimpleTest.waitForFocus,
  });
  let element = document.querySelector(".urlbarView-row .urlbarView-tip-help");
  let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  EventUtils.synthesizeMouseAtCenter(element, {}, element.ownerGlobal);
  await loadPromise;
  Assert.equal(
    gURLBar.value,
    HELP_URL,
    "Should have navigated to the tip's help page."
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "test",
    window,
    waitForFocus: SimpleTest.waitForFocus,
  });
  element = document.querySelector(".urlbarView-row .urlbarView-tip-button");
  loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  EventUtils.synthesizeMouseAtCenter(element, {}, element.ownerGlobal);
  await loadPromise;
  Assert.equal(gURLBar.value, TIP_URL, "Should have navigated to the tip URL.");

  UrlbarProvidersManager.unregisterProvider(provider);
});
