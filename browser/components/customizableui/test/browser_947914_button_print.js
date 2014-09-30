/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const isOSX = (Services.appinfo.OS === "Darwin");

add_task(function() {
  info("Check print button existence and functionality");

  yield PanelUI.show();
  info("Menu panel was opened");

  yield waitForCondition(() => document.getElementById("print-button") != null);

  let printButton = document.getElementById("print-button");
  ok(printButton, "Print button exists in Panel Menu");

  if (isOSX) {
    let panelHiddenPromise = promisePanelHidden(window);
    PanelUI.hide();
    yield panelHiddenPromise;
    info("Menu panel was closed");
  }
  else {
    printButton.click();
    yield waitForCondition(() => gInPrintPreviewMode);

    ok(gInPrintPreviewMode, "Entered print preview mode");
  }
});

add_task(function asyncCleanup() {
    // close print preview
    if (gInPrintPreviewMode) {
      PrintUtils.exitPrintPreview();
      yield waitForCondition(() => !window.gInPrintPreviewMode);
      info("Exited print preview")
    }
});
