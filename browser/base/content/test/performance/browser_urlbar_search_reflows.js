"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
                                  "resource://testing-common/PlacesTestUtils.jsm");

/**
 * WHOA THERE: We should never be adding new things to EXPECTED_REFLOWS. This
 * is a whitelist that should slowly go away as we improve the performance of
 * the front-end. Instead of adding more reflows to the whitelist, you should
 * be modifying your code to avoid the reflow.
 *
 * See https://developer.mozilla.org/en-US/Firefox/Performance_best_practices_for_Firefox_fe_engineers
 * for tips on how to do that.
 */

/* These reflows happen only the first time the awesomebar panel opens. */
const EXPECTED_REFLOWS_FIRST_OPEN = [
  // Bug 1357054
  {
    stack: [
      "_rebuild@chrome://browser/content/search/search.xml",
      "set_popup@chrome://browser/content/search/search.xml",
      "enableOneOffSearches@chrome://browser/content/urlbarBindings.xml",
      "_enableOrDisableOneOffSearches@chrome://browser/content/urlbarBindings.xml",
      "urlbar_XBL_Constructor/<@chrome://browser/content/urlbarBindings.xml",
      "openPopup@chrome://global/content/bindings/popup.xml",
      "_openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openPopup@chrome://global/content/bindings/autocomplete.xml",
      "set_popupOpen@chrome://global/content/bindings/autocomplete.xml"
    ],
    times: 2, // This number should only ever go down - never up.
  },

  {
    stack: [
      "adjustSiteIconStart@chrome://global/content/bindings/autocomplete.xml",
      "set_siteIconStart@chrome://global/content/bindings/autocomplete.xml",
      "_openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openPopup@chrome://global/content/bindings/autocomplete.xml",
      "set_popupOpen@chrome://global/content/bindings/autocomplete.xml",
    ],
  },

  {
    stack: [
      "adjustSiteIconStart@chrome://global/content/bindings/autocomplete.xml",
      "_reuseAcItem@chrome://global/content/bindings/autocomplete.xml",
      "_appendCurrentResult@chrome://global/content/bindings/autocomplete.xml",
      "_invalidate@chrome://global/content/bindings/autocomplete.xml",
      "invalidate@chrome://global/content/bindings/autocomplete.xml",
    ],
    times: 9, // This number should only ever go down - never up.
  },

  {
    stack: [
      "adjustHeight@chrome://global/content/bindings/autocomplete.xml",
      "onxblpopupshown@chrome://global/content/bindings/autocomplete.xml"
    ],
    times: 5, // This number should only ever go down - never up.
  },

  {
    stack: [
      "_handleOverflow@chrome://global/content/bindings/autocomplete.xml",
      "handleOverUnderflow@chrome://global/content/bindings/autocomplete.xml",
      "set_siteIconStart@chrome://global/content/bindings/autocomplete.xml",
    ],
    times: 6, // This number should only ever go down - never up.
  },

  {
    stack: [
      "adjustHeight@chrome://global/content/bindings/autocomplete.xml",
      "_invalidate/this._adjustHeightTimeout<@chrome://global/content/bindings/autocomplete.xml",
    ],
    times: 3, // This number should only ever go down - never up.
  },

  {
    stack: [
      "_handleOverflow@chrome://global/content/bindings/autocomplete.xml",
      "handleOverUnderflow@chrome://global/content/bindings/autocomplete.xml",
      "_reuseAcItem@chrome://global/content/bindings/autocomplete.xml",
      "_appendCurrentResult@chrome://global/content/bindings/autocomplete.xml",
      "_invalidate@chrome://global/content/bindings/autocomplete.xml",
      "invalidate@chrome://global/content/bindings/autocomplete.xml"
    ],
    times: 399, // This number should only ever go down - never up.
  },

  {
    stack: [
      "_openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openPopup@chrome://global/content/bindings/autocomplete.xml",
      "set_popupOpen@chrome://global/content/bindings/autocomplete.xml",
    ],
    times: 3, // This number should only ever go down - never up.
  },

  // Bug 1359989
  {
    stack: [
      "openPopup@chrome://global/content/bindings/popup.xml",
      "_openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openPopup@chrome://global/content/bindings/autocomplete.xml",
      "set_popupOpen@chrome://global/content/bindings/autocomplete.xml",
    ],
  },
];

/* These reflows happen everytime the awesomebar panel opens. */
const EXPECTED_REFLOWS_SECOND_OPEN = [
  // Bug 1357054
  {
    stack: [
      "_rebuild@chrome://browser/content/search/search.xml",
      "handleEvent@chrome://browser/content/search/search.xml",
      "openPopup@chrome://global/content/bindings/popup.xml",
      "_openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openPopup@chrome://global/content/bindings/autocomplete.xml",
      "set_popupOpen@chrome://global/content/bindings/autocomplete.xml",
    ],
  },

  {
    stack: [
      "adjustHeight@chrome://global/content/bindings/autocomplete.xml",
      "onxblpopupshown@chrome://global/content/bindings/autocomplete.xml"
    ],
    times: 3, // This number should only ever go down - never up.
  },

  {
    stack: [
      "adjustHeight@chrome://global/content/bindings/autocomplete.xml",
      "_invalidate/this._adjustHeightTimeout<@chrome://global/content/bindings/autocomplete.xml",
    ],
    times: 3, // This number should only ever go down - never up.
  },

  {
    stack: [
      "_handleOverflow@chrome://global/content/bindings/autocomplete.xml",
      "handleOverUnderflow@chrome://global/content/bindings/autocomplete.xml",
      "_reuseAcItem@chrome://global/content/bindings/autocomplete.xml",
      "_appendCurrentResult@chrome://global/content/bindings/autocomplete.xml",
      "_invalidate@chrome://global/content/bindings/autocomplete.xml",
      "invalidate@chrome://global/content/bindings/autocomplete.xml"
    ],
    times: 444, // This number should only ever go down - never up.
  },

  // Bug 1384256
  {
    stack: [
      "_openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openPopup@chrome://global/content/bindings/autocomplete.xml",
      "set_popupOpen@chrome://global/content/bindings/autocomplete.xml",
    ],
    times: 3, // This number should only ever go down - never up.
  },

  // Bug 1359989
  {
    stack: [
      "openPopup@chrome://global/content/bindings/popup.xml",
      "_openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openPopup@chrome://global/content/bindings/autocomplete.xml",
      "set_popupOpen@chrome://global/content/bindings/autocomplete.xml",
    ],
  },
];

/**
 * Returns a Promise that resolves once the AwesomeBar popup for a particular
 * window has appeared after having done a search for its input text.
 *
 * @param win (browser window)
 *        The window to do the search in.
 * @returns Promise
 */
async function promiseAutocompleteResultPopup(win) {
  let URLBar = win.gURLBar;
  URLBar.controller.startSearch(URLBar.value);
  await BrowserTestUtils.waitForEvent(URLBar.popup, "popupshown");
  await BrowserTestUtils.waitForCondition(() => {
    return URLBar.controller.searchStatus >=
      Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH;
  });
  let matchCount = URLBar.popup._matchCount;
  await BrowserTestUtils.waitForCondition(() => {
    return URLBar.popup.richlistbox.childNodes.length == matchCount;
  });

  URLBar.controller.stopSearch();
  // There are several setTimeout(fn, 0); calls inside autocomplete.xml
  // that we need to wait for. Since those have higher priority than
  // idle callbacks, we can be sure they will have run once this
  // idle callback is called. The timeout seems to be required in
  // automation - presumably because the machines can be pretty busy
  // especially if it's GC'ing from previous tests.
  await new Promise(resolve => win.requestIdleCallback(resolve, { timeout: 1000 }));

  let hiddenPromise = BrowserTestUtils.waitForEvent(URLBar.popup, "popuphidden");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
  await hiddenPromise;
}

const SEARCH_TERM = "urlbar-reflows";

add_task(async function setup() {
  const NUM_VISITS = 10;
  let visits = [];

  for (let i = 0; i < NUM_VISITS; ++i) {
    visits.push({
      uri: `http://example.com/urlbar-reflows-${i}`,
      title: `Reflow test for URL bar entry #${i}`,
    });
  }

  await PlacesTestUtils.addVisits(visits);

  registerCleanupFunction(async function() {
    await PlacesTestUtils.clearHistory();
  });
});

/**
 * This test ensures that there are no unexpected
 * uninterruptible reflows when typing into the URL bar
 * with the default values in Places.
 */
add_task(async function() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await ensureNoPreloadedBrowser(win);

  let URLBar = win.gURLBar;
  let popup = URLBar.popup;

  URLBar.focus();
  URLBar.value = SEARCH_TERM;
  let testFn = async function(dirtyFrameFn) {
    let oldInvalidate = popup.invalidate.bind(popup);
    let oldResultsAdded = popup.onResultsAdded.bind(popup);

    // We need to invalidate the frame tree outside of the normal
    // mechanism since invalidations and result additions to the
    // URL bar occur without firing JS events (which is how we
    // normally know to dirty the frame tree).
    popup.invalidate = (reason) => {
      dirtyFrameFn();
      oldInvalidate(reason);
    };

    popup.onResultsAdded = () => {
      dirtyFrameFn();
      oldResultsAdded();
    };

    await promiseAutocompleteResultPopup(win);
  };

  await withReflowObserver(testFn, EXPECTED_REFLOWS_FIRST_OPEN, win);

  await withReflowObserver(testFn, EXPECTED_REFLOWS_SECOND_OPEN, win);

  await BrowserTestUtils.closeWindow(win);
});
