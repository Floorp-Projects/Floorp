/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
const EDIT_PROFILE_URL = "chrome://formautofill/content/editProfile.xhtml";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://formautofill/FormAutofillUtils.jsm");

this.log = null;
FormAutofillUtils.defineLazyLogGetter(this, "manageProfiles");

function ManageProfileDialog() {
  this.prefWin = window.opener;
  window.addEventListener("DOMContentLoaded", this, {once: true});
}

ManageProfileDialog.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports, Ci.nsIObserver]),

  _elements: {},

  /**
   * Count the number of "formautofill-storage-changed" events epected to
   * receive to prevent repeatedly loading addresses.
   * @type {number}
   */
  _pendingChangeCount: 0,

  /**
   * Get the selected options on the addresses element.
   *
   * @returns {array<DOMElement>}
   */
  get _selectedOptions() {
    return Array.from(this._elements.addresses.selectedOptions);
  },

  init() {
    this._elements = {
      addresses: document.getElementById("profiles"),
      controlsContainer: document.getElementById("controls-container"),
      remove: document.getElementById("remove"),
      add: document.getElementById("add"),
      edit: document.getElementById("edit"),
    };
    this.attachEventListeners();
  },

  uninit() {
    log.debug("uninit");
    this.detachEventListeners();
    this._elements = null;
  },

  /**
   * Load addresses and render them.
   *
   * @returns {promise}
   */
  loadAddresses() {
    return this.getAddresses().then(addresses => {
      log.debug("addresses:", addresses);
      // Sort by last modified time starting with most recent
      addresses.sort((a, b) => b.timeLastModified - a.timeLastModified);
      this.renderAddressElements(addresses);
      this.updateButtonsStates(this._selectedOptions.length);
    });
  },

  /**
   * Get addresses from storage.
   *
   * @returns {promise}
   */
  getAddresses() {
    return new Promise(resolve => {
      Services.cpmm.addMessageListener("FormAutofill:Addresses", function getResult(result) {
        Services.cpmm.removeMessageListener("FormAutofill:Addresses", getResult);
        resolve(result.data);
      });
      Services.cpmm.sendAsyncMessage("FormAutofill:GetAddresses", {});
    });
  },

  /**
   * Render the addresses onto the page while maintaining selected options if
   * they still exist.
   *
   * @param  {array<object>} addresses
   */
  renderAddressElements(addresses) {
    let selectedGuids = this._selectedOptions.map(option => option.value);
    this.clearAddressElements();
    for (let address of addresses) {
      let option = new Option(this.getAddressLabel(address),
                              address.guid,
                              false,
                              selectedGuids.includes(address.guid));
      option.address = address;
      this._elements.addresses.appendChild(option);
    }
  },

  /**
   * Remove all existing address elements.
   */
  clearAddressElements() {
    let parent = this._elements.addresses;
    while (parent.lastChild) {
      parent.removeChild(parent.lastChild);
    }
  },

  /**
   * Remove addresses by guids.
   * Keep track of the number of "formautofill-storage-changed" events to
   * ignore before loading addresses.
   *
   * @param  {array<string>} guids
   */
  removeAddresses(guids) {
    this._pendingChangeCount += guids.length - 1;
    Services.cpmm.sendAsyncMessage("FormAutofill:RemoveAddresses", {guids});
  },

  /**
   * Get address display label. It should display up to two pieces of
   * information, separated by a comma.
   *
   * @param  {object} address
   * @returns {string}
   */
  getAddressLabel(address) {
    // TODO: Implement a smarter way for deciding what to display
    //       as option text. Possibly improve the algorithm in
    //       ProfileAutoCompleteResult.jsm and reuse it here.
    const fieldOrder = [
      "name",
      "-moz-street-address-one-line",  // Street address
      "address-level2",  // City/Town
      "organization",    // Company or organization name
      "address-level1",  // Province/State (Standardized code if possible)
      "country-name",    // Country name
      "postal-code",     // Postal code
      "tel",             // Phone number
      "email",           // Email address
    ];

    let parts = [];
    if (address["street-address"]) {
      address["-moz-street-address-one-line"] = FormAutofillUtils.toOneLineAddress(
        address["street-address"]
      );
    }
    for (const fieldName of fieldOrder) {
      let string = address[fieldName];
      if (string) {
        parts.push(string);
      }
      if (parts.length == 2) {
        break;
      }
    }
    return parts.join(", ");
  },

  /**
   * Open the edit address dialog to create/edit an address.
   *
   * @param  {object} address [optional]
   */
  openEditDialog(address) {
    this.prefWin.gSubDialog.open(EDIT_PROFILE_URL, null, address);
  },

  /**
   * Enable/disable the Edit and Remove buttons based on number of selected
   * options.
   *
   * @param  {number} selectedCount
   */
  updateButtonsStates(selectedCount) {
    log.debug("updateButtonsStates:", selectedCount);
    if (selectedCount == 0) {
      this._elements.edit.setAttribute("disabled", "disabled");
      this._elements.remove.setAttribute("disabled", "disabled");
    } else if (selectedCount == 1) {
      this._elements.edit.removeAttribute("disabled");
      this._elements.remove.removeAttribute("disabled");
    } else if (selectedCount > 1) {
      this._elements.edit.setAttribute("disabled", "disabled");
      this._elements.remove.removeAttribute("disabled");
    }
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
        this.loadAddresses();
        break;
      }
      case "click": {
        this.handleClick(event);
        break;
      }
      case "change": {
        this.updateButtonsStates(this._selectedOptions.length);
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
    if (event.target == this._elements.remove) {
      this.removeAddresses(this._selectedOptions.map(option => option.value));
    } else if (event.target == this._elements.add) {
      this.openEditDialog();
    } else if (event.target == this._elements.edit ||
               event.target.parentNode == this._elements.addresses && event.detail > 1) {
      this.openEditDialog(this._selectedOptions[0].address);
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

  observe(subject, topic, data) {
    switch (topic) {
      case "formautofill-storage-changed": {
        if (this._pendingChangeCount) {
          this._pendingChangeCount -= 1;
          return;
        }
        this.loadAddresses();
      }
    }
  },

  /**
   * Attach event listener
   */
  attachEventListeners() {
    window.addEventListener("unload", this, {once: true});
    window.addEventListener("keypress", this);
    this._elements.addresses.addEventListener("change", this);
    this._elements.addresses.addEventListener("click", this);
    this._elements.controlsContainer.addEventListener("click", this);
    Services.obs.addObserver(this, "formautofill-storage-changed");
  },

  /**
   * Remove event listener
   */
  detachEventListeners() {
    window.removeEventListener("keypress", this);
    this._elements.addresses.removeEventListener("change", this);
    this._elements.addresses.removeEventListener("click", this);
    this._elements.controlsContainer.removeEventListener("click", this);
    Services.obs.removeObserver(this, "formautofill-storage-changed");
  },
};

new ManageProfileDialog();
