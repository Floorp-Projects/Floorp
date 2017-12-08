/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Checking that the awesomebar popup gets COMBOBOX_LIST role instead of
// LISTBOX, since its parent is a <panel> (see Bug 1422465)
add_task(async function testAutocompleteRichResult() {
  let tab = await openNewTab("data:text/html;charset=utf-8,");
  let accService = await initAccessibilityService();

  info("Opening the URL bar and entering a key to show the PopupAutoCompleteRichResult panel");
  let urlbar = document.getElementById("urlbar");
  urlbar.focus();
  let urlbarPopup = document.getElementById("PopupAutoCompleteRichResult");
  let shown = BrowserTestUtils.waitForEvent(urlbarPopup, "popupshown");
  EventUtils.synthesizeKey("a", {});
  await shown;

  info("Waiting for accessibility to be created for the richlistbox");
  let richlistbox = document.getAnonymousElementByAttribute(urlbarPopup, "anonid", "richlistbox");
  await BrowserTestUtils.waitForCondition(() => accService.getAccessibleFor(richlistbox));

  info("Confirming that the special case is handled in XULListboxAccessible");
  let accessible = accService.getAccessibleFor(richlistbox);
  is(accessible.role, ROLE_COMBOBOX_LIST, "Right role");

  await BrowserTestUtils.removeTab(tab);
});

registerCleanupFunction(async function() {
  await shutdownAccessibilityService();
});
