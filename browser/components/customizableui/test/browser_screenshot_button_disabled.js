/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

add_task(async function testScreenshotButtonPrefDisabled() {
  info("Test the Screenshots widget not available");

  Assert.ok(
    Services.prefs.getBoolPref("extensions.screenshots.disabled"),
    "Sceenshots feature is disabled"
  );

  CustomizableUI.addWidgetToArea(
    "screenshot-button",
    CustomizableUI.AREA_NAVBAR
  );

  let screenshotBtn = document.getElementById("screenshot-button");
  Assert.ok(!screenshotBtn, "Screenshot button is unavailable");
});
