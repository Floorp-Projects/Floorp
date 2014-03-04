/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const isOSX = (Services.appinfo.OS === "Darwin");

add_task(function() {
  info("Check print button existence and functionality");
  yield PanelUI.show();

  let printButton = document.getElementById("print-button");
  ok(printButton, "Print button exists in Panel Menu");

  if(isOSX) {
    yield PanelUI.hide();
  }
  else {
    printButton.click();
    yield waitForCondition(function() window.gInPrintPreviewMode);

    ok(window.gInPrintPreviewMode, "Entered print preview mode");

    // close print preview
    PrintUtils.exitPrintPreview();
    yield waitForCondition(() => !window.gInPrintPreviewMode);
  }
});
