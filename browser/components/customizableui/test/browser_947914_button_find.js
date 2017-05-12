/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.photon.structure.enabled", false]]});
  info("Check find button existence and functionality");

  await PanelUI.show();
  info("Menu panel was opened");

  let findButton = document.getElementById("find-button");
  ok(findButton, "Find button exists in Panel Menu");

  findButton.click();
  ok(!gFindBar.hasAttribute("hidden"), "Findbar opened successfully");

  // close find bar
  gFindBar.close();
  info("Findbar was closed");
});
