/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This tests that the urlbar panel closes when clicking certain ui elements.
 */

"use strict";

add_setup(function () {
  // We intentionally turn off this a11y check, because the following
  // clicks is purposefully targeting non-interactive elements to dismiss
  // the opened URL Bar with a mouse which can be done by assistive
  // technology and keyboard by pressing `Esc` key, this rule check shall
  // be ignored by a11y_checks suite.
  AccessibilityUtils.setEnv({ mustHaveAccessibleRule: false });

  registerCleanupFunction(async () => {
    // Usually, the AccessibilityUtils environment should be reset right after
    // the click, but in this case there are no other testable interactions
    // between iterations of the use case task besides those clicks that we are
    // setting the environment with.
    AccessibilityUtils.resetEnv();
  });
});

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
