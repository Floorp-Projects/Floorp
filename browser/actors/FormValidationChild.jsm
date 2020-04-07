/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Handles the validation callback from nsIFormFillController and
 * the display of the help panel on invalid elements.
 */

var EXPORTED_SYMBOLS = ["FormValidationChild"];

const { BrowserUtils } = ChromeUtils.import(
  "resource://gre/modules/BrowserUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

class FormValidationChild extends JSWindowActorChild {
  constructor() {
    super();
    this._validationMessage = "";
    this._element = null;
  }

  actorCreated() {
    // Listening to ‘pageshow’ event is only relevant
    // if an invalid form popup was open. So we add
    // a listener here and not during registration to
    // avoid a premature instantiation of the actor.
    this.contentWindow.addEventListener("pageshow", this);
  }

  /*
   * Events
   */

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "MozInvalidForm":
        aEvent.preventDefault();
        this.notifyInvalidSubmit(aEvent.target, aEvent.detail);
        break;
      case "pageshow":
        if (this._isRootDocumentEvent(aEvent)) {
          this._hidePopup();
        }
        break;
      case "input":
        this._onInput(aEvent);
        break;
      case "blur":
        this._onBlur(aEvent);
        break;
    }
  }

  /*
   * nsIFormSubmitObserver
   */

  notifyInvalidSubmit(aFormElement, aInvalidElements) {
    // Show a validation message on the first focusable element.
    for (let element of aInvalidElements) {
      // Insure that this is the FormSubmitObserver associated with the
      // element / window this notification is about.
      if (this.contentWindow != element.ownerGlobal.document.defaultView) {
        return;
      }

      if (
        !(
          ChromeUtils.getClassName(element) === "HTMLInputElement" ||
          ChromeUtils.getClassName(element) === "HTMLTextAreaElement" ||
          ChromeUtils.getClassName(element) === "HTMLSelectElement" ||
          ChromeUtils.getClassName(element) === "HTMLButtonElement"
        )
      ) {
        continue;
      }

      if (!Services.focus.elementIsFocusable(element, 0)) {
        continue;
      }

      // Update validation message before showing notification
      this._validationMessage = element.validationMessage;

      // Don't connect up to the same element more than once.
      if (this._element == element) {
        this._showPopup(element);
        break;
      }
      this._element = element;

      element.focus();

      // Watch for input changes which may change the validation message.
      element.addEventListener("input", this);

      // Watch for focus changes so we can disconnect our listeners and
      // hide the popup.
      element.addEventListener("blur", this);

      this._showPopup(element);
      break;
    }
  }

  /*
   * Internal
   */

  /*
   * Handles input changes on the form element we've associated a popup
   * with. Updates the validation message or closes the popup if form data
   * becomes valid.
   */
  _onInput(aEvent) {
    let element = aEvent.originalTarget;

    // If the form input is now valid, hide the popup.
    if (element.validity.valid) {
      this._hidePopup();
      return;
    }

    // If the element is still invalid for a new reason, we should update
    // the popup error message.
    if (this._validationMessage != element.validationMessage) {
      this._validationMessage = element.validationMessage;
      this._showPopup(element);
    }
  }

  /*
   * Blur event handler in which we disconnect from the form element and
   * hide the popup.
   */
  _onBlur(aEvent) {
    aEvent.originalTarget.removeEventListener("input", this);
    aEvent.originalTarget.removeEventListener("blur", this);
    this._element = null;
    this._hidePopup();
  }

  /*
   * Send the show popup message to chrome with appropriate position
   * information. Can be called repetitively to update the currently
   * displayed popup position and text.
   */
  _showPopup(aElement) {
    // Collect positional information and show the popup
    let panelData = {};

    panelData.message = this._validationMessage;

    // Note, this is relative to the browser and needs to be translated
    // in chrome.
    panelData.contentRect = BrowserUtils.getElementBoundingRect(aElement);

    // We want to show the popup at the middle of checkbox and radio buttons
    // and where the content begin for the other elements.
    let offset = 0;

    if (
      aElement.tagName == "INPUT" &&
      (aElement.type == "radio" || aElement.type == "checkbox")
    ) {
      panelData.position = "bottomcenter topleft";
    } else {
      let win = aElement.ownerGlobal;
      let style = win.getComputedStyle(aElement);
      if (style.direction == "rtl") {
        offset =
          parseInt(style.paddingRight) + parseInt(style.borderRightWidth);
      } else {
        offset = parseInt(style.paddingLeft) + parseInt(style.borderLeftWidth);
      }
      let zoomFactor = this.contentWindow.windowUtils.fullZoom;
      panelData.offset = Math.round(offset * zoomFactor);
      panelData.position = "after_start";
    }
    this.sendAsyncMessage("FormValidation:ShowPopup", panelData);
  }

  _hidePopup() {
    this.sendAsyncMessage("FormValidation:HidePopup", {});
  }

  _isRootDocumentEvent(aEvent) {
    if (this.contentWindow == null) {
      return true;
    }
    let target = aEvent.originalTarget;
    return (
      target == this.document ||
      (target.ownerDocument && target.ownerDocument == this.document)
    );
  }
}
