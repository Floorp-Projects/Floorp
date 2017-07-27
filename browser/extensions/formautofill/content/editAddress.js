/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://formautofill/FormAutofillUtils.jsm");

function EditDialog() {
  this._address = window.arguments && window.arguments[0];
  window.addEventListener("DOMContentLoaded", this, {once: true});
}

EditDialog.prototype = {
  init() {
    this._elements = {
      controlsContainer: document.getElementById("controls-container"),
      cancel: document.getElementById("cancel"),
      save: document.getElementById("save"),
    };
    this.attachEventListeners();
  },

  uninit() {
    this.detachEventListeners();
    this._elements = null;
  },

  /**
   * Asks FormAutofillParent to save or update an address.
   * @param  {object} data
   *         {
   *           {string} guid [optional]
   *           {object} address
   *         }
   */
  saveAddress(data) {
    Services.cpmm.sendAsyncMessage("FormAutofill:SaveAddress", data);
  },

  /**
   * Fill the form with an address object.
   * @param  {object} address
   */
  loadInitialValues(address) {
    for (let field in address) {
      let input = document.getElementById(field);
      if (input) {
        input.value = address[field];
      }
    }
  },

  /**
   * Get inputs from the form.
   * @returns {object}
   */
  buildAddressObject() {
    return Array.from(document.forms[0].elements).reduce((obj, input) => {
      if (input.value) {
        obj[input.id] = input.value;
      }
      return obj;
    }, {});
  },

  /**
   * Handle events
   *
   * @param  {DOMEvent} event
   */
  handleEvent(event) {
    switch (event.type) {
      case "DOMContentLoaded": {
        this.init();
        if (this._address) {
          document.title = "Edit Address";
          this.loadInitialValues(this._address);
        }
        break;
      }
      case "click": {
        this.handleClick(event);
        break;
      }
      case "input": {
        // Toggle disabled attribute on the save button based on
        // whether the form is filled or empty.
        if (Object.keys(this.buildAddressObject()).length == 0) {
          this._elements.save.setAttribute("disabled", true);
        } else {
          this._elements.save.removeAttribute("disabled");
        }
        break;
      }
      case "unload": {
        this.uninit();
        break;
      }
      case "keypress": {
        this.handleKeyPress(event);
        break;
      }
    }
  },

  /**
   * Handle click events
   *
   * @param  {DOMEvent} event
   */
  handleClick(event) {
    if (event.target == this._elements.cancel) {
      window.close();
    }
    if (event.target == this._elements.save) {
      if (this._address) {
        this.saveAddress({
          guid: this._address.guid,
          address: this.buildAddressObject(),
        });
      } else {
        this.saveAddress({
          address: this.buildAddressObject(),
        });
      }
      window.close();
    }
  },

  /**
   * Handle key press events
   *
   * @param  {DOMEvent} event
   */
  handleKeyPress(event) {
    if (event.keyCode == KeyEvent.DOM_VK_ESCAPE) {
      window.close();
    }
  },

  /**
   * Attach event listener
   */
  attachEventListeners() {
    window.addEventListener("keypress", this);
    this._elements.controlsContainer.addEventListener("click", this);
    document.addEventListener("input", this);
  },

  /**
   * Remove event listener
   */
  detachEventListeners() {
    window.removeEventListener("keypress", this);
    this._elements.controlsContainer.removeEventListener("click", this);
    document.removeEventListener("input", this);
  },
};

new EditDialog();
