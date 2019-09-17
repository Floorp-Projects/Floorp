/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "UrlbarTestUtils",
  "resource://testing-common/UrlbarTestUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

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
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/b" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/c" }
    ),
  ];

  let provider = new TipTestProvider(matches);
  UrlbarProvidersManager.registerProvider(provider);

  gURLBar.search("test");
  await promiseSearchComplete();

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    4,
    "There should be four results in the view."
  );
  let secondResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(
    secondResult.type,
    UrlbarUtils.RESULT_TYPE.TIP,
    "The second result should be a tip."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    0,
    "The first result should be selected."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      "urlbarView-tip-button"
    ),
    "The selected element should be the tip button."
  );
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    1,
    "getSelectedIndex should return 1 even though the tip button is selected."
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
    "getSelectedIndex should return 1 even though the help button is selected."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    2,
    "The third result should be selected."
  );

  EventUtils.synthesizeKey("KEY_ArrowUp");
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      "urlbarView-tip-help"
    ),
    "The selected element should be the tip help button."
  );

  EventUtils.synthesizeKey("VK_RETURN");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  Assert.equal(
    gURLBar.value,
    HELP_URL,
    "Should have navigated to the tip's help page."
  );

  gURLBar.search("test");
  await promiseSearchComplete();

  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      "urlbarView-tip-button"
    ),
    "The selected element should be the tip button."
  );
  EventUtils.synthesizeKey("VK_RETURN");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  Assert.equal(gURLBar.value, TIP_URL, "Should have navigated to the tip URL.");

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

  gURLBar.search("test");
  await promiseSearchComplete();

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
    UrlbarTestUtils.getSelectedRowIndex(window),
    0,
    "getSelectedIndex should return 0."
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
    0,
    "getSelectedIndex should return 0."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
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

  UrlbarProvidersManager.unregisterProvider(provider);
});

add_task(async function mouseSelection() {
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

  gURLBar.search("test");
  await promiseSearchComplete();
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      "urlbarView-tip-help"
    ),
    "The selected element should be the tip help button."
  );
  let element = UrlbarTestUtils.getSelectedElement(window);
  EventUtils.synthesizeMouseAtCenter(element, {}, element.ownerGlobal);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  Assert.equal(
    gURLBar.value,
    HELP_URL,
    "Should have navigated to the tip's help page."
  );

  gURLBar.search("test");
  await promiseSearchComplete();
  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      "urlbarView-tip-button"
    ),
    "The selected element should be the tip button."
  );
  element = UrlbarTestUtils.getSelectedElement(window);
  EventUtils.synthesizeMouseAtCenter(element, {}, element.ownerGlobal);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  Assert.equal(gURLBar.value, TIP_URL, "Should have navigated to the tip URL.");

  UrlbarProvidersManager.unregisterProvider(provider);
});
