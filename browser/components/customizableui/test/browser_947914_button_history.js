/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.photon.structure.enabled", false]]});
  info("Check history button existence and functionality");

  await PanelUI.show();
  info("Menu panel was opened");

  let historyButton = document.getElementById("history-panelmenu");
  ok(historyButton, "History button appears in Panel Menu");

  let historyPanel = document.getElementById("PanelUI-history");
  let promise = BrowserTestUtils.waitForEvent(historyPanel, "ViewShown");
  historyButton.click();
  await promise;
  ok(historyPanel.getAttribute("current"), "History Panel is in view");

  let panelHiddenPromise = promisePanelHidden(window);
  PanelUI.hide();
  await panelHiddenPromise
  info("Menu panel was closed");
});
