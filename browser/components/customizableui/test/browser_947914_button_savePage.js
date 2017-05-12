/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.photon.structure.enabled", false]]});
  info("Check save page button existence");

  await PanelUI.show();
  info("Menu panel was opened");

  let savePageButton = document.getElementById("save-page-button");
  ok(savePageButton, "Save Page button exists in Panel Menu");

  let panelHiddenPromise = promisePanelHidden(window);
  PanelUI.hide();
  await panelHiddenPromise;
  info("Menu panel was closed");
});
