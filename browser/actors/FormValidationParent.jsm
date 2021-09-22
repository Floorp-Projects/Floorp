/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Chrome side handling of form validation popup.
 */

"use strict";

var EXPORTED_SYMBOLS = ["FormValidationParent"];

class FormValidationParent extends JSWindowActorParent {
  constructor() {
    super();

    this._panel = null;
  }

  /*
   * Public apis
   */

  uninit() {
    this._panel = null;
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

        this._showPopup(data);
        break;
      case "FormValidation:HidePopup":
        this._hidePopup();
        break;
    }
  }

  observe(aSubject, aTopic, aData) {
    this._hidePopup();
  }

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "FullZoomChange":
      case "TextZoomChange":
      case "scroll":
        this._hidePopup();
        break;
      case "popuphiding":
        this._onPopupHiding(aEvent);
        break;
    }
  }

  /*
   * Internal
   */

  _onPopupHiding(aEvent) {
    aEvent.originalTarget.removeEventListener("popuphiding", this, true);
    let tabBrowser = aEvent.originalTarget.ownerGlobal.gBrowser;
    tabBrowser.selectedBrowser.removeEventListener("scroll", this, true);
    tabBrowser.selectedBrowser.removeEventListener("FullZoomChange", this);
    tabBrowser.selectedBrowser.removeEventListener("TextZoomChange", this);

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
    if (!previouslyShown) {
      // Cleanup after the popup is hidden
      this._panel.addEventListener("popuphiding", this, true);

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
