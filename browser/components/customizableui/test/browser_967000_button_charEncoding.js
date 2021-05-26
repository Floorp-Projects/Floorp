/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_PAGE =
  "http://mochi.test:8888/browser/browser/components/customizableui/test/support/test_967000_charEncoding_page.html";

add_task(async function() {
  info("Check Character Encoding button functionality");

  // add the Character Encoding button to the panel
  CustomizableUI.addWidgetToArea(
    "characterencoding-button",
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );

  // This test used to rely on the implied 100ms initial timer of the
  // TestUtils.waitForCondition call made within waitForOverflowButtonShown.
  // See bug 1498063.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 100));
  await waitForOverflowButtonShown();
  registerCleanupFunction(() => CustomizableUI.reset());

  info("Waiting for the overflow panel to open");
  await document.getElementById("nav-bar").overflowable.show();

  let charEncodingButton = document.getElementById("characterencoding-button");
  ok(
    charEncodingButton,
    "The Character Encoding button was added to the Panel Menu"
  );
  is(
    charEncodingButton.getAttribute("disabled"),
    "true",
    "The Character encoding button is initially disabled"
  );

  let newTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PAGE,
    true,
    true
  );

  await document.getElementById("nav-bar").overflowable.show();
  ok(
    !charEncodingButton.hasAttribute("disabled"),
    "The Character encoding button gets enabled"
  );
  BrowserTestUtils.removeTab(newTab);
});

add_task(async function asyncCleanup() {
  // reset the panel to the default state
  await resetCustomization();
  ok(CustomizableUI.inDefaultState, "The UI is in default state again.");
});
