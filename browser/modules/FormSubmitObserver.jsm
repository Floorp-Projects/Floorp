/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Handles the validation callback from nsIFormFillController and
 * the display of the help panel on invalid elements.
 */

"use strict";

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

var HTMLInputElement = Ci.nsIDOMHTMLInputElement;
var HTMLTextAreaElement = Ci.nsIDOMHTMLTextAreaElement;
var HTMLSelectElement = Ci.nsIDOMHTMLSelectElement;
var HTMLButtonElement = Ci.nsIDOMHTMLButtonElement;

this.EXPORTED_SYMBOLS = [ "FormSubmitObserver" ];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/BrowserUtils.jsm");

function FormSubmitObserver(aWindow, aTabChildGlobal) {
  this.init(aWindow, aTabChildGlobal);
}

FormSubmitObserver.prototype =
{
  _validationMessage: "",
  _content: null,
  _element: null,

  /*
   * Public apis
   */

  init: function(aWindow, aTabChildGlobal)
  {
    this._content = aWindow;
    this._tab = aTabChildGlobal;
    this._mm =
      this._content.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDocShell)
                   .sameTypeRootTreeItem
                   .QueryInterface(Ci.nsIDocShell)
                   .QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIContentFrameMessageManager);

    // nsIFormSubmitObserver callback about invalid forms. See HTMLFormElement
    // for details.
    Services.obs.addObserver(this, "invalidformsubmit", false);
    this._tab.addEventListener("pageshow", this, false);
    this._tab.addEventListener("unload", this, false);
  },

  uninit: function()
  {
    Services.obs.removeObserver(this, "invalidformsubmit");
    this._content.removeEventListener("pageshow", this, false);
    this._content.removeEventListener("unload", this, false);
    this._mm = null;
    this._element = null;
    this._content = null;
    this._tab = null;
  },

  /*
   * Events
   */

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "pageshow":
        if (this._isRootDocumentEvent(aEvent)) {
          this._hidePopup();
        }
        break;
      case "unload":
        this.uninit();
        break;
      case "input":
        this._onInput(aEvent);
        break;
      case "blur":
        this._onBlur(aEvent);
        break;
    }
  },

  /*
   * nsIFormSubmitObserver
   */

  notifyInvalidSubmit : function(aFormElement, aInvalidElements)
  {
    // We are going to handle invalid form submission attempt by focusing the
    // first invalid element and show the corresponding validation message in a
    // panel attached to the element.
    if (!aInvalidElements.length) {
      return;
    }

    // Insure that this is the FormSubmitObserver associated with the
    // element / window this notification is about.
    let element = aInvalidElements.queryElementAt(0, Ci.nsISupports);
    if (this._content != element.ownerGlobal.top.document.defaultView) {
      return;
    }

    if (!(element instanceof HTMLInputElement ||
          element instanceof HTMLTextAreaElement ||
          element instanceof HTMLSelectElement ||
          element instanceof HTMLButtonElement)) {
      return;
    }

    // Update validation message before showing notification
    this._validationMessage = element.validationMessage;

    // Don't connect up to the same element more than once.
    if (this._element == element) {
      this._showPopup(element);
      return;
    }
    this._element = element;

    element.focus();

    // Watch for input changes which may change the validation message.
    element.addEventListener("input", this, false);

    // Watch for focus changes so we can disconnect our listeners and
    // hide the popup.
    element.addEventListener("blur", this, false);

    this._showPopup(element);
  },

  /*
   * Internal
   */

  /*
   * Handles input changes on the form element we've associated a popup
   * with. Updates the validation message or closes the popup if form data
   * becomes valid.
   */
  _onInput: function(aEvent) {
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
  },

  /*
   * Blur event handler in which we disconnect from the form element and
   * hide the popup.
   */
  _onBlur: function(aEvent) {
    aEvent.originalTarget.removeEventListener("input", this, false);
    aEvent.originalTarget.removeEventListener("blur", this, false);
    this._element = null;
    this._hidePopup();
  },

  /*
   * Send the show popup message to chrome with appropriate position
   * information. Can be called repetitively to update the currently
   * displayed popup position and text.
   */
  _showPopup: function(aElement) {
    // Collect positional information and show the popup
    let panelData = {};

    panelData.message = this._validationMessage;

    // Note, this is relative to the browser and needs to be translated
    // in chrome.
    panelData.contentRect = BrowserUtils.getElementBoundingRect(aElement);

    // We want to show the popup at the middle of checkbox and radio buttons
    // and where the content begin for the other elements.
    let offset = 0;

    if (aElement.tagName == 'INPUT' &&
        (aElement.type == 'radio' || aElement.type == 'checkbox')) {
      panelData.position = "bottomcenter topleft";
    } else {
      let win = aElement.ownerGlobal;
      let style = win.getComputedStyle(aElement, null);
      if (style.direction == 'rtl') {
        offset = parseInt(style.paddingRight) + parseInt(style.borderRightWidth);
      } else {
        offset = parseInt(style.paddingLeft) + parseInt(style.borderLeftWidth);
      }
      let zoomFactor = this._getWindowUtils().fullZoom;
      panelData.offset = Math.round(offset * zoomFactor);
      panelData.position = "after_start";
    }
    this._mm.sendAsyncMessage("FormValidation:ShowPopup", panelData);
  },

  _hidePopup: function() {
    this._mm.sendAsyncMessage("FormValidation:HidePopup", {});
  },

  _getWindowUtils: function() {
    return this._content.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
  },

  _isRootDocumentEvent: function(aEvent) {
    if (this._content == null) {
      return true;
    }
    let target = aEvent.originalTarget;
    return (target == this._content.document ||
            (target.ownerDocument && target.ownerDocument == this._content.document));
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.nsIFormSubmitObserver])
};
