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
      "_openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openPopup@chrome://global/content/bindings/autocomplete.xml",
      "set_popupOpen@chrome://global/content/bindings/autocomplete.xml"
    ],
  },

  {
    stack: [
      "adjustHeight@chrome://global/content/bindings/autocomplete.xml",
      "onxblpopupshown@chrome://global/content/bindings/autocomplete.xml"
    ],
    maxCount: 5, // This number should only ever go down - never up.
  },

  {
    stack: [
      "adjustHeight@chrome://global/content/bindings/autocomplete.xml",
      "_invalidate/this._adjustHeightTimeout<@chrome://global/content/bindings/autocomplete.xml",
    ],
    maxCount: 51, // This number should only ever go down - never up.
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
    maxCount: 60, // This number should only ever go down - never up.
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
    maxCount: 6, // This number should only ever go down - never up.
  },

  // Bug 1359989
  {
    stack: [
      "_openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openPopup@chrome://global/content/bindings/autocomplete.xml",
      "set_popupOpen@chrome://global/content/bindings/autocomplete.xml",
    ],
  },
];

// These extra reflows happen on beta/release as one of the default bookmarks in
// bookmarks.html.in has a long URL.
if (AppConstants.RELEASE_OR_BETA) {
  EXPECTED_REFLOWS_FIRST_OPEN.push({
    stack: [
      "_handleOverflow@chrome://global/content/bindings/autocomplete.xml",
      "_onUnderflow@chrome://global/content/bindings/autocomplete.xml",
      "onunderflow@chrome://browser/content/browser.xul",
    ],
    maxCount: 6,
  });
  EXPECTED_REFLOWS_FIRST_OPEN.push({
    stack: [
      "_handleOverflow@chrome://global/content/bindings/autocomplete.xml",
      "_onOverflow@chrome://global/content/bindings/autocomplete.xml",
      "onoverflow@chrome://browser/content/browser.xul",
    ],
    maxCount: 6,
  });
  EXPECTED_REFLOWS_FIRST_OPEN.push({
    stack: [
      "_handleOverflow@chrome://global/content/bindings/autocomplete.xml",
      "_adjustAcItem@chrome://global/content/bindings/autocomplete.xml",
      "_appendCurrentResult@chrome://global/content/bindings/autocomplete.xml",
      "_invalidate@chrome://global/content/bindings/autocomplete.xml",
      "invalidate@chrome://global/content/bindings/autocomplete.xml",
    ],
    maxCount: 12,
  });
}

/**
 * Returns a Promise that resolves once the AwesomeBar popup for a particular
 * window has appeared after having done a search for its input text.
 *
 * @param win (browser window)
 *        The window to do the search in.
 * @returns Promise
 */
async function promiseSearchComplete(win) {
  let URLBar = win.gURLBar;
  if (URLBar.popup.state != "open") {
    await BrowserTestUtils.waitForEvent(URLBar.popup, "popupshown");
  }
  await BrowserTestUtils.waitForCondition(() => {
    return URLBar.controller.searchStatus >=
      Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH;
  });

  // There are several setTimeout(fn, 0); calls inside autocomplete.xml
  // that we need to wait for. Since those have higher priority than
  // idle callbacks, we can be sure they will have run once this
  // idle callback is called. The timeout seems to be required in
  // automation - presumably because the machines can be pretty busy
  // especially if it's GC'ing from previous tests.
  await new Promise(resolve => win.requestIdleCallback(resolve, { timeout: 1000 }));
}

add_task(async function setup() {
  await addDummyHistoryEntries();
});

/**
 * This test ensures that there are no unexpected uninterruptible reflows when
 * typing into the URL bar with the default values in Places.
 */
add_task(async function() {
  let win = await prepareSettledWindow();

  let URLBar = win.gURLBar;
  let popup = URLBar.popup;

  URLBar.focus();
  URLBar.value = "";

  let dropmarkerRect = document.getAnonymousElementByAttribute(gURLBar,
    "anonid", "historydropmarker").getBoundingClientRect();
  let textBoxRect = document.getAnonymousElementByAttribute(gURLBar,
    "anonid", "textbox-input-box").getBoundingClientRect();

  await withPerfObserver(async function() {
    let oldInvalidate = popup.invalidate.bind(popup);
    let oldResultsAdded = popup.onResultsAdded.bind(popup);

    // We need to invalidate the frame tree outside of the normal
    // mechanism since invalidations and result additions to the
    // URL bar occur without firing JS events (which is how we
    // normally know to dirty the frame tree).
    popup.invalidate = (reason) => {
      dirtyFrame(win);
      oldInvalidate(reason);
    };

    popup.onResultsAdded = () => {
      dirtyFrame(win);
      oldResultsAdded();
    };

    // Only keying in 6 characters because the number of reflows triggered
    // is so high that we risk timing out the test if we key in any more.
    const SEARCH_TERM = "ows-10";
    for (let i = 0; i < SEARCH_TERM.length; ++i) {
      let char = SEARCH_TERM[i];
      EventUtils.synthesizeKey(char, {}, win);
      await promiseSearchComplete(win);
    }

    let hiddenPromise = BrowserTestUtils.waitForEvent(URLBar.popup, "popuphidden");
    EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
    await hiddenPromise;
  }, {expectedReflows: EXPECTED_REFLOWS_FIRST_OPEN,
      frames: {filter: rects => rects.filter(r => !(
        // We put text into the urlbar so expect its textbox to change.
        (r.x1 >= textBoxRect.left && r.x2 <= textBoxRect.right &&
         r.y1 >= textBoxRect.top && r.y2 <= textBoxRect.bottom) ||
        // The dropmarker is replaced with the go arrow during the test.
        // dropmarkerRect.left isn't always an integer, hence the - 1 and + 1
        (r.x1 >= dropmarkerRect.left - 1 && r.x2 <= dropmarkerRect.right + 1 &&
         r.y1 >= dropmarkerRect.top && r.y2 <= dropmarkerRect.bottom)
        // XXX For some reason the awesomebar panel isn't in our screenshots,
        // but that's where we actually expect many changes.
      ))}}, win);

  await BrowserTestUtils.closeWindow(win);
});
