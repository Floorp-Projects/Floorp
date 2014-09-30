/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function() {
  info("Check save page button existence");

  yield PanelUI.show();
  info("Menu panel was opened");

  let savePageButton = document.getElementById("save-page-button");
  ok(savePageButton, "Save Page button exists in Panel Menu");

  let panelHiddenPromise = promisePanelHidden(window);
  PanelUI.hide();
  yield panelHiddenPromise;
  info("Menu panel was closed");
});
