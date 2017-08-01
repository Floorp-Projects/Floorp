/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function() {
  info("Check history button existence and functionality");
  CustomizableUI.addWidgetToArea("history-panelmenu", CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);
  registerCleanupFunction(() => CustomizableUI.reset());

  await document.getElementById("nav-bar").overflowable.show();
  info("Menu panel was opened");

  let historyButton = document.getElementById("history-panelmenu");
  ok(historyButton, "History button appears in Panel Menu");

  let historyPanel = document.getElementById("PanelUI-history");
  let promise = BrowserTestUtils.waitForEvent(historyPanel, "ViewShown");
  historyButton.click();
  await promise;
  ok(historyPanel.getAttribute("current"), "History Panel is in view");

  let panelHiddenPromise = promiseOverflowHidden(window);
  document.getElementById("widget-overflow").hidePopup();
  await panelHiddenPromise
  info("Menu panel was closed");
});
