/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { UrlbarTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/UrlbarTestUtils.sys.mjs"
);

// Checking that the awesomebar popup gets COMBOBOX_LIST role instead of
// LISTBOX, since its parent is a <panel> (see Bug 1422465)
add_task(async function testAutocompleteRichResult() {
  let tab = await openNewTab("data:text/html;charset=utf-8,");
  let accService = await initAccessibilityService();

  info("Opening the URL bar and entering a key to show the urlbar panel");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "a",
  });

  info("Waiting for accessibility to be created for the results list");
  let resultsView;
  resultsView = gURLBar.view.panel.querySelector(".urlbarView-results");
  await TestUtils.waitForCondition(() =>
    accService.getAccessibleFor(resultsView)
  );

  info("Confirming that the special case is handled in XULListboxAccessible");
  let accessible = accService.getAccessibleFor(resultsView);
  is(accessible.role, ROLE_COMBOBOX_LIST, "Right role");

  BrowserTestUtils.removeTab(tab);
});

registerCleanupFunction(async function() {
  await shutdownAccessibilityService();
});
