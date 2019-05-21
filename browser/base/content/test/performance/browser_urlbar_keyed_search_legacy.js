"use strict";

// This tests searching in the legacy urlbar implementation (a.k.a. the
// awesomebar).

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
const EXPECTED_REFLOWS_FIRST_OPEN = [];
if (AppConstants.platform == "linux" ||
    AppConstants.platform == "win" ||
    // macOS 10.14 Mojave (Darwin version 18)
    AppConstants.isPlatformAndVersionAtLeast("macosx", "18")) {
  EXPECTED_REFLOWS_FIRST_OPEN.push({
    stack: [
      "__rebuild@chrome://browser/content/search/search-one-offs.js",
      /* This is limited to a one-line stack, because the next item is an async
         function and as such not supported on all trees, according to bug 1501761.
      "async*set popup@chrome://browser/content/search/search-one-offs.js",
      "_syncOneOffSearchesEnabled@chrome://browser/content/urlbarBindings.xml",
      "toggleOneOffSearches@chrome://browser/content/urlbarBindings.xml",
      "_enableOrDisableOneOffSearches@chrome://browser/content/urlbarBindings.xml",
      "@chrome://browser/content/urlbarBindings.xml",
      "_openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openAutocompletePopup@chrome://browser/content/urlbarBindings.xml",
      "openPopup@chrome://global/content/bindings/autocomplete.xml",
      "set_popupOpen@chrome://global/content/bindings/autocomplete.xml",*/
    ],
  });
}
EXPECTED_REFLOWS_FIRST_OPEN.push(
  {
    stack: [
      "_handleOverflow@chrome://global/content/elements/autocomplete-richlistitem.js",
      "handleOverUnderflow@chrome://global/content/elements/autocomplete-richlistitem.js",
      "_reuseAcItem@chrome://global/content/elements/autocomplete-richlistitem.js",
      "_appendCurrentResult@chrome://global/content/bindings/autocomplete.xml",
      "_invalidate@chrome://global/content/bindings/autocomplete.xml",
      "invalidate@chrome://global/content/bindings/autocomplete.xml",
    ],
    maxCount: 60, // This number should only ever go down - never up.
  },

  {
    stack: [
      "_handleOverflow@chrome://global/content/elements/autocomplete-richlistitem.js",
      "handleOverUnderflow@chrome://global/content/elements/autocomplete-richlistitem.js",
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
  }
);

// These extra reflows happen on beta/release as one of the default bookmarks in
// bookmarks.html.in has a long URL.
if (AppConstants.RELEASE_OR_BETA) {
  EXPECTED_REFLOWS_FIRST_OPEN.push({
    stack: [
      "_handleOverflow@chrome://global/content/elements/autocomplete-richlistitem.js",
      "_onUnderflow@chrome://global/content/elements/autocomplete-richlistitem.js",
      "MozAutocompleteRichlistitem/<@chrome://global/content/elements/autocomplete-richlistitem.js",
    ],
    maxCount: 6,
  },
  {
    stack: [
      "_handleOverflow@chrome://global/content/elements/autocomplete-richlistitem.js",
      "_onOverflow@chrome://global/content/elements/autocomplete-richlistitem.js",
      "MozAutocompleteRichlistitem/<@chrome://global/content/elements/autocomplete-richlistitem.js",
    ],
    maxCount: 6,
  },
  {
    stack: [
      "_handleOverflow@chrome://global/content/elements/autocomplete-richlistitem.js",
      "_adjustAcItem@chrome://global/content/elements/autocomplete-richlistitem.js",
      "_appendCurrentResult@chrome://global/content/bindings/autocomplete.xml",
      "_invalidate@chrome://global/content/bindings/autocomplete.xml",
      "invalidate@chrome://global/content/bindings/autocomplete.xml",
    ],
    maxCount: 12,
  });
}

add_task(async function awesomebar() {
  await runUrlbarTest(true, true, EXPECTED_REFLOWS_FIRST_OPEN);
});
