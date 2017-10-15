/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const isOSX = (Services.appinfo.OS === "Darwin");

add_task(async function() {
  CustomizableUI.addWidgetToArea("print-button", CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);
  registerCleanupFunction(() => CustomizableUI.reset());
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com/",
  }, async function() {
    info("Check print button existence and functionality");

    await waitForOverflowButtonShown();

    await document.getElementById("nav-bar").overflowable.show();
    info("Menu panel was opened");

    await waitForCondition(() => document.getElementById("print-button") != null);

    let printButton = document.getElementById("print-button");
    ok(printButton, "Print button exists in Panel Menu");

    if (isOSX) {
      let panelHiddenPromise = promiseOverflowHidden(window);
      document.getElementById("widget-overflow").hidePopup();
      await panelHiddenPromise;
      info("Menu panel was closed");
    } else {
      printButton.click();
      await waitForCondition(() => gInPrintPreviewMode);

      ok(gInPrintPreviewMode, "Entered print preview mode");

      // close print preview
      if (gInPrintPreviewMode) {
        PrintUtils.exitPrintPreview();
        await waitForCondition(() => !window.gInPrintPreviewMode);
        info("Exited print preview");
      }
    }
  });
});

