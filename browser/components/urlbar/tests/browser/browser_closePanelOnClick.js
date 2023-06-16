/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This tests that the urlbar panel closes when clicking certain ui elements.
 */

"use strict";

add_task(async function () {
  await BrowserTestUtils.withNewTab("about:robots", async () => {
    for (let elt of [
      gBrowser.selectedBrowser,
      gBrowser.tabContainer,
      document.querySelector("#nav-bar toolbarspring"),
    ]) {
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        waitForFocus,
        value: "dummy",
      });
      // Must have at least one test.
      Assert.ok(!!elt, "Found a valid element: " + (elt.id || elt.localName));
      await UrlbarTestUtils.promisePopupClose(window, () =>
        EventUtils.synthesizeNativeMouseEvent({
          type: "click",
          target: elt,
          atCenter: true,
        })
      );
    }
  });
});
