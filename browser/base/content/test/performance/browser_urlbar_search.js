"use strict";

// This tests searching in the urlbar (a.k.a. the quantumbar).

/**
 * WHOA THERE: We should never be adding new things to EXPECTED_REFLOWS. This
 * is a whitelist that should slowly go away as we improve the performance of
 * the front-end. Instead of adding more reflows to the whitelist, you should
 * be modifying your code to avoid the reflow.
 *
 * See https://developer.mozilla.org/en-US/Firefox/Performance_best_practices_for_Firefox_fe_engineers
 * for tips on how to do that.
 */

/* These reflows happen only the first time the panel opens. */
const EXPECTED_REFLOWS_FIRST_OPEN = [
  {
    stack: [
      "__rebuild@chrome://browser/content/search/search-one-offs.js",
      /* This is limited to a one-line stack, because the next item is an async
         function and as such not supported on all trees, according to bug 1501761.
      "async*_rebuild@chrome://browser/content/search/search-one-offs.js",
      "async*_on_popupshowing@chrome://browser/content/search/search-one-offs.js",
      "handleEvent@chrome://browser/content/search/search-one-offs.js",
      "_openPanel@resource:///modules/UrlbarView.jsm",
      "onQueryResults@resource:///modules/UrlbarView.jsm",
      "_notify@resource:///modules/UrlbarController.jsm",
      "receiveResults@resource:///modules/UrlbarController.jsm",
      "notifyResults@resource:///modules/UrlbarProvidersManager.jsm",
      "add@resource:///modules/UrlbarProvidersManager.jsm",
      "onSearchResult@resource:///modules/UrlbarProviderUnifiedComplete.jsm",
      */
    ],
  },
  // This is the this.panel.openPopup() call in UrlbarView._openPanel.  See bug
  // 1359989, which was filed against the legacy awesomebar but applies here too
  // because it seems to be caused by platform code.
  {
    stack: [
      "_openPanel@resource:///modules/UrlbarView.jsm",
      "onQueryResults@resource:///modules/UrlbarView.jsm",
    ],
  },
];

/* These reflows happen every time the panel opens. */
const EXPECTED_REFLOWS_SECOND_OPEN = [
  // This is the this.panel.openPopup() call in UrlbarView._openPanel.  See bug
  // 1359989, which was filed against the legacy awesomebar but applies here too
  // because it seems to be caused by platform code.
  {
    stack: [
      "_openPanel@resource:///modules/UrlbarView.jsm",
      "onQueryResults@resource:///modules/UrlbarView.jsm",
    ],
  },
];

add_task(async function quantumbar() {
  await runUrlbarTest(false, false, EXPECTED_REFLOWS_FIRST_OPEN,
                      EXPECTED_REFLOWS_SECOND_OPEN);
});
