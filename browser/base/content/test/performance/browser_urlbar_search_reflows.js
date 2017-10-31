"use strict";

// There are a _lot_ of reflows in this test, and processing them takes
// time. On slower builds, we need to boost our allowed test time.
requestLongerTimeout(5);

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
    times: 1, // This number should only ever go down - never up.
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
    times: 36, // This number should only ever go down - never up.
  },

  {
    stack: [
      "_handleOverflow@chrome://global/content/bindings/autocomplete.xml",
      "handleOverUnderflow@chrome://global/content/bindings/autocomplete.xml",
      "_openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openPopup@chrome://global/content/bindings/autocomplete.xml",
      "set_popupOpen@chrome://global/content/bindings/autocomplete.xml",
    ],
    times: 6, // This number should only ever go down - never up.
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
    times: 24, // This number should only ever go down - never up.
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

const SEARCH_TERM = "urlbar-reflows-" + Date.now();

add_task(async function setup() {
  await addDummyHistoryEntries(SEARCH_TERM);
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
  };

  info("First opening");
  await withReflowObserver(testFn, EXPECTED_REFLOWS_FIRST_OPEN, win);

  info("Second opening");
  await withReflowObserver(testFn, EXPECTED_REFLOWS_SECOND_OPEN, win);

  await BrowserTestUtils.closeWindow(win);
});
