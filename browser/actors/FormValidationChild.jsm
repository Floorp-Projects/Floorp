/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Handles the validation callback from nsIFormFillController and
 * the display of the help panel on invalid elements.
 */

var EXPORTED_SYMBOLS = ["FormValidationChild"];

const { LayoutUtils } = ChromeUtils.import(
  "resource://gre/modules/LayoutUtils.jsm"
);

class FormValidationChild extends JSWindowActorChild {
  constructor() {
    super();
    this._validationMessage = "";
    this._element = null;
  }

  /*
   * Events
   */

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "MozInvalidForm":
        aEvent.preventDefault();
        this.notifyInvalidSubmit(aEvent.detail);
        break;
      case "pageshow":
        if (this._isRootDocumentEvent(aEvent)) {
          this._hidePopup();
        }
        break;
      case "pagehide":
        // Act as if the element is being blurred. This will remove any
        // listeners and hide the popup.
        this._onBlur();
        break;
      case "input":
        this._onInput(aEvent);
        break;
      case "blur":
        this._onBlur(aEvent);
        break;
    }
  }

  notifyInvalidSubmit(aInvalidElements) {
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
          ChromeUtils.getClassName(element) === "HTMLButtonElement" ||
          element.isFormAssociatedCustomElements
        )
      ) {
        continue;
      }

      let validationMessage = element.isFormAssociatedCustomElements
        ? element.internals.validationMessage
        : element.validationMessage;

      if (element.isFormAssociatedCustomElements) {
        // For element that are form-associated custom elements, user agents
        // should use their validation anchor instead.
        element = element.internals.validationAnchor;
      }

      if (!element || !Services.focus.elementIsFocusable(element, 0)) {
        continue;
      }

      // Update validation message before showing notification
      this._validationMessage = validationMessage;

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
    if (this._element) {
      this._element.removeEventListener("input", this);
      this._element.removeEventListener("blur", this);
    }
    this._hidePopup();
    this._element = null;
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

    panelData.screenRect = LayoutUtils.getElementBoundingScreenRect(aElement);

    // We want to show the popup at the middle of checkbox and radio buttons
    // and where the content begin for the other elements.
    if (
      aElement.tagName == "INPUT" &&
      (aElement.type == "radio" || aElement.type == "checkbox")
    ) {
      panelData.position = "bottomcenter topleft";
    } else {
      panelData.position = "after_start";
    }
    this.sendAsyncMessage("FormValidation:ShowPopup", panelData);

    aElement.ownerGlobal.addEventListener("pagehide", this, {
      mozSystemGroup: true,
    });
  }

  _hidePopup() {
    this.sendAsyncMessage("FormValidation:HidePopup", {});
    this._element.ownerGlobal.removeEventListener("pagehide", this, {
      mozSystemGroup: true,
    });
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
