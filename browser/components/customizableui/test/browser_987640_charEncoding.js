/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_PAGE =
  "http://mochi.test:8888/browser/browser/components/customizableui/test/support/test_967000_charEncoding_page.html";

add_task(async function() {
  info("Check Character Encoding panel functionality");

  // add the Character Encoding button to the panel
  CustomizableUI.addWidgetToArea(
    "characterencoding-button",
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );

  await waitForOverflowButtonShown();

  let newTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PAGE,
    true,
    true
  );

  await document.getElementById("nav-bar").overflowable.show();
  let charEncodingButton = document.getElementById("characterencoding-button");

  ok(
    !charEncodingButton.hasAttribute("disabled"),
    "The encoding button should be enabled"
  );

  let browserStopPromise = BrowserTestUtils.browserStopped(gBrowser, TEST_PAGE);
  charEncodingButton.click();
  await browserStopPromise;
  is(
    gBrowser.selectedBrowser.characterSet,
    "UTF-8",
    "The encoding should be changed to UTF-8"
  );
  ok(
    !gBrowser.selectedBrowser.mayEnableCharacterEncodingMenu,
    "The encoding menu should be disabled"
  );

  is(
    charEncodingButton.getAttribute("disabled"),
    "true",
    "We should disable the encoding button in toolbar"
  );

  CustomizableUI.removeWidgetFromArea("characterencoding-button");
  CustomizableUI.addWidgetToArea(
    "characterencoding-button",
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );
  await waitForOverflowButtonShown();
  await document.getElementById("nav-bar").overflowable.show();
  charEncodingButton = document.getElementById("characterencoding-button");

  // check the encoding menu again
  is(
    charEncodingButton.getAttribute("disabled"),
    "true",
    "We should disable the encoding button in overflow menu"
  );

  BrowserTestUtils.removeTab(newTab);
});

add_task(async function asyncCleanup() {
  // reset the panel to the default state
  await resetCustomization();
  ok(CustomizableUI.inDefaultState, "The UI is in default state again.");
});
