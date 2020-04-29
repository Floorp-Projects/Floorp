/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

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
    popup.openPopup(anchor, "bottomcenter topright");
  }
};
