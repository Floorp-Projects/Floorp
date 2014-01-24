/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function() {
  info("Check history button existence and functionality");
  yield PanelUI.show();

  let historyButton = document.getElementById("history-panelmenu");
  ok(historyButton, "History button appears in Panel Menu");

  historyButton.click();
  let historyPanel = document.getElementById("PanelUI-history");
  ok(historyPanel.getAttribute("current"), "History Panel is in view");

  yield PanelUI.hide();
});
