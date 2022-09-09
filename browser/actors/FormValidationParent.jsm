/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Chrome side handling of form validation popup.
 */

"use strict";

var EXPORTED_SYMBOLS = ["FormValidationParent"];

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "BrowserWindowTracker",
  "resource:///modules/BrowserWindowTracker.jsm"
);

class PopupShownObserver {
  _weakContext = null;

  constructor(browsingContext) {
    this._weakContext = Cu.getWeakReference(browsingContext);
  }

  observe(subject, topic, data) {
    let ctxt = this._weakContext.get();
    let actor = ctxt.currentWindowGlobal?.getExistingActor("FormValidation");
    if (!actor) {
      Services.obs.removeObserver(this, "popup-shown");
      return;
    }
    // If any panel besides ourselves shows, hide ourselves again.
    if (topic == "popup-shown" && subject != actor._panel) {
      actor._hidePopup();
    }
  }

  QueryInterface = ChromeUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ]);
}

class FormValidationParent extends JSWindowActorParent {
  constructor() {
    super();

    this._panel = null;
    this._obs = null;
  }

  static hasOpenPopups() {
    for (let win of lazy.BrowserWindowTracker.orderedWindows) {
      let popups = win.document.querySelectorAll("panel,menupopup");
      for (let popup of popups) {
        let { state } = popup;
        if (state == "open" || state == "showing") {
          return true;
        }
      }
    }
    return false;
  }

  /*
   * Public apis
   */

  uninit() {
    this._panel = null;
    this._obs = null;
  }

  hidePopup() {
    this._hidePopup();
  }

  /*
   * Events
   */

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "FormValidation:ShowPopup":
        let browser = this.browsingContext.top.embedderElement;
        let window = browser.ownerGlobal;
        let data = aMessage.data;
        let tabBrowser = window.gBrowser;

        // target is the <browser>, make sure we're receiving a message
        // from the foreground tab.
        if (tabBrowser && browser != tabBrowser.selectedBrowser) {
          return;
        }

        if (FormValidationParent.hasOpenPopups()) {
          return;
        }

        this._showPopup(data);
        break;
      case "FormValidation:HidePopup":
        this._hidePopup();
        break;
    }
  }

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "FullZoomChange":
      case "TextZoomChange":
      case "scroll":
        this._hidePopup();
        break;
      case "popuphidden":
        this._onPopupHidden(aEvent);
        break;
    }
  }

  /*
   * Internal
   */

  _onPopupHidden(aEvent) {
    aEvent.originalTarget.removeEventListener("popuphidden", this, true);
    Services.obs.removeObserver(this._obs, "popup-shown");
    let tabBrowser = aEvent.originalTarget.ownerGlobal.gBrowser;
    tabBrowser.selectedBrowser.removeEventListener("scroll", this, true);
    tabBrowser.selectedBrowser.removeEventListener("FullZoomChange", this);
    tabBrowser.selectedBrowser.removeEventListener("TextZoomChange", this);

    this._obs = null;
    this._panel = null;
  }

  /*
   * Shows the form validation popup at a specified position or updates the
   * messaging and position if the popup is already displayed.
   *
   * @aPanelData - Object that contains popup information
   *  aPanelData stucture detail:
   *   screenRect - the screen rect of the target element.
   *   position - popup positional string constants.
   *   message - the form element validation message text.
   */
  _showPopup(aPanelData) {
    let previouslyShown = !!this._panel;
    this._panel = this._getAndMaybeCreatePanel();
    this._panel.firstChild.textContent = aPanelData.message;

    let browser = this.browsingContext.top.embedderElement;
    let window = browser.ownerGlobal;

    let tabBrowser = window.gBrowser;

    // Display the panel if it isn't already visible.
    if (previouslyShown) {
      return;
    }
    // Cleanup after the popup is hidden
    this._panel.addEventListener("popuphidden", this, true);
    // Hide ourselves if other popups shown
    this._obs = new PopupShownObserver(this.browsingContext);
    Services.obs.addObserver(this._obs, "popup-shown", true);

    // Hide if the user scrolls the page
    tabBrowser.selectedBrowser.addEventListener("scroll", this, true);
    tabBrowser.selectedBrowser.addEventListener("FullZoomChange", this);
    tabBrowser.selectedBrowser.addEventListener("TextZoomChange", this);

    // Open the popup
    let rect = aPanelData.screenRect;
    this._panel.openPopupAtScreenRect(
      aPanelData.position,
      rect.left,
      rect.top,
      rect.width,
      rect.height,
      false,
      false
    );
  }

  /*
   * Hide the popup if currently displayed. Will fire an event to onPopupHiding
   * above if visible.
   */
  _hidePopup() {
    this._panel?.hidePopup();
  }

  _getAndMaybeCreatePanel() {
    // Lazy load the invalid form popup the first time we need to display it.
    if (!this._panel) {
      let browser = this.browsingContext.top.embedderElement;
      let window = browser.ownerGlobal;
      let template = window.document.getElementById("invalidFormTemplate");
      if (template) {
        template.replaceWith(template.content);
      }
      this._panel = window.document.getElementById("invalid-form-popup");
    }

    return this._panel;
  }
}
