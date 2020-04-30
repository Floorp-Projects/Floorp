/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyGetter(this, "telemetry", () => {
  const Telemetry = require("devtools/client/shared/telemetry");
  return new Telemetry();
});

// This session id will be initialized the first time the popup is displayed.
let telemetrySessionId = null;

/**
 * Helper dedicated to toggle a popup triggered by pressing F12 if DevTools have
 * never been opened by the user.
 *
 * This popup should be anchored below the main hamburger menu of Firefox,
 * which contains the Web Developer menu.
 *
 * This is part of the OFF12 experiment which tries to disable F12 by default to
 * reduce accidental usage of DevTools and increase retention of non DevTools
 * users.
 */
exports.toggleEnableDevToolsPopup = function(doc) {
  // The popup is initially wrapped in a template tag to avoid loading
  // resources on startup. Unwrap it the first time we show the notification.
  const panelWrapper = doc.getElementById("wrapper-enable-devtools-popup");
  if (panelWrapper) {
    panelWrapper.replaceWith(panelWrapper.content);
  }

  const popup = doc.getElementById("enable-devtools-popup");

  // Use the icon of the Firefox menu in order to be aligned with the
  // position of the hamburger menu.
  const anchor = doc
    .getElementById("PanelUI-menu-button")
    .querySelector(".toolbarbutton-icon");

  const isVisible = popup.state === "open";
  if (isVisible) {
    popup.hidePopup();
  } else {
    if (!telemetrySessionId) {
      telemetrySessionId = parseInt(telemetry.msSinceProcessStart(), 10);
    }

    popup.openPopup(anchor, "bottomcenter topright");
    telemetry.recordEvent("f12_popup_displayed", "tools", null, {
      session_id: telemetrySessionId,
    });
  }
};

/**
 * If a session id was already generated here for telemetry, expose it so that
 * the toolbox can use it as its own session id.
 */
exports.getF12SessionId = function() {
  return telemetrySessionId;
};
