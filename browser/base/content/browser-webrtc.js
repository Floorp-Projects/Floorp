/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

/**
 * Utility object to handle WebRTC shared tab warnings.
 */
var gSharedTabWarning = {
  /**
   * Called externally by gBrowser to determine if we're
   * in a state such that we'd want to cancel the tab switch
   * and show the tab switch warning panel instead.
   *
   * @param tab (<tab>)
   *   The tab being switched to.
   * @returns boolean
   *   True if the panel will be shown, and the tab switch should
   *   be cancelled.
   */
  willShowSharedTabWarning(tab) {
    if (!this._sharedTabWarningEnabled) {
      return false;
    }

    let shareState = webrtcUI.getWindowShareState(window);
    if (shareState == webrtcUI.SHARING_NONE) {
      return false;
    }

    if (!webrtcUI.shouldShowSharedTabWarning(tab)) {
      return false;
    }

    this._createSharedTabWarningIfNeeded();
    let panel = document.getElementById("sharing-tabs-warning-panel");
    let hbox = panel.firstChild;

    if (shareState == webrtcUI.SHARING_SCREEN) {
      hbox.setAttribute("type", "screen");
      panel.setAttribute(
        "aria-labelledby",
        "sharing-screen-warning-panel-header-span"
      );
    } else {
      hbox.setAttribute("type", "window");
      panel.setAttribute(
        "aria-labelledby",
        "sharing-window-warning-panel-header-span"
      );
    }

    let allowForSessionCheckbox = document.getElementById(
      "sharing-warning-disable-for-session"
    );
    allowForSessionCheckbox.checked = false;

    panel.openPopup(tab, "bottomleft topleft", 0, 0);

    return true;
  },

  /**
   * Called by the tab switch warning panel after it has
   * shown.
   */
  sharedTabWarningShown() {
    let allowButton = document.getElementById("sharing-warning-proceed-to-tab");
    allowButton.focus();
  },

  /**
   * Called by the button in the tab switch warning panel
   * to allow the switch to occur.
   */
  allowSharedTabSwitch() {
    let panel = document.getElementById("sharing-tabs-warning-panel");
    let allowForSession = document.getElementById(
      "sharing-warning-disable-for-session"
    ).checked;

    let tab = panel.anchorNode;
    webrtcUI.allowSharedTabSwitch(tab, allowForSession);
    this._hideSharedTabWarning();
  },

  /**
   * Called externally by gBrowser when a tab has been added.
   * When this occurs, if we're sharing this window, we notify
   * the webrtcUI module to exempt the new tab from the tab switch
   * warning, since the user opened it while they were already
   * sharing.
   *
   * @param tab (<tab>)
   *   The tab being opened.
   */
  tabAdded(tab) {
    if (this._sharedTabWarningEnabled) {
      let shareState = webrtcUI.getWindowShareState(window);
      if (shareState != webrtcUI.SHARING_NONE) {
        webrtcUI.tabAddedWhileSharing(tab);
      }
    }
  },

  get _sharedTabWarningEnabled() {
    delete this._sharedTabWarningEnabled;
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_sharedTabWarningEnabled",
      "privacy.webrtc.sharedTabWarning"
    );
    return this._sharedTabWarningEnabled;
  },

  /**
   * Internal method for hiding the tab switch warning panel.
   */
  _hideSharedTabWarning() {
    let panel = document.getElementById("sharing-tabs-warning-panel");
    if (panel) {
      panel.hidePopup();
    }
  },

  /**
   * Inserts the tab switch warning panel into the DOM
   * if it hasn't been done already yet.
   */
  _createSharedTabWarningIfNeeded() {
    // Lazy load the panel the first time we need to display it.
    if (!document.getElementById("sharing-tabs-warning-panel")) {
      let template = document.getElementById(
        "sharing-tabs-warning-panel-template"
      );
      template.replaceWith(template.content);
    }
  },
};
