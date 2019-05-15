/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

/**
 * Utility object to handle manipulations of the protections indicators in the UI
 */
var gProtectionsHandler = {
  // smart getters
  get _protectionsPopup() {
    delete this._protectionsPopup;
    return this._protectionsPopup = document.getElementById("protections-popup");
  },
  get _protectionsIconBox() {
    delete this._protectionsIconBox;
    return this._protectionsIconBox = document.getElementById("tracking-protection-icon-animatable-box");
  },
  get _protectionsPopupMainViewHeaderLabel() {
    delete this._protectionsPopupMainViewHeaderLabel;
    return this._protectionsPopupMainViewHeaderLabel =
      document.getElementById("protections-popup-mainView-panel-header-span");
  },

  handleProtectionsButtonEvent(event) {
    event.stopPropagation();
    if ((event.type == "click" && event.button != 0) ||
        (event.type == "keypress" && event.charCode != KeyEvent.DOM_VK_SPACE &&
         event.keyCode != KeyEvent.DOM_VK_RETURN)) {
      return; // Left click, space or enter only
    }

    // Make sure that the display:none style we set in xul is removed now that
    // the popup is actually needed
    this._protectionsPopup.hidden = false;

    // Refresh strings.
    this.refreshProtectionsPopup();

    // Now open the popup, anchored off the primary chrome element
    PanelMultiView.openPopup(this._protectionsPopup, this._protectionsIconBox, {
      position: "bottomcenter topleft",
      triggerEvent: event,
    }).catch(Cu.reportError);
  },

  refreshProtectionsPopup() {
    let host = gIdentityHandler.getHostForDisplay();

    // Push the appropriate strings out to the UI.
    this._protectionsPopupMainViewHeaderLabel.textContent =
      // gNavigatorBundle.getFormattedString("protections.header", [host]);
      `Tracking Protections for ${host}`;
  },
};
