/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

ChromeUtils.import("resource:///modules/CustomizableUI.jsm", {});

add_task(async function testScreenshotButtonDisabled() {
  info("Test the Screenshots button in the panel");

  CustomizableUI.addWidgetToArea(
    "screenshot-button",
    CustomizableUI.AREA_NAVBAR
  );

  let screenshotBtn = document.getElementById("screenshot-button");
  Assert.ok(screenshotBtn, "The screenshots button was added to the nav bar");

  await BrowserTestUtils.withNewTab("https://example.com/", () => {
    Assert.equal(
      screenshotBtn.disabled,
      false,
      "Screenshots button is enabled"
    );
  });
  await BrowserTestUtils.withNewTab("about:home", () => {
    Assert.equal(
      screenshotBtn.disabled,
      true,
      "Screenshots button is now disabled"
    );
  });
});
