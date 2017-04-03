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
  window.addEventListener("DOMContentLoaded", this, {once: true});
}

ManageProfileDialog.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports, Ci.nsIObserver]),

  _elements: {},

  /**
   * Count the number of "formautofill-storage-changed" events epected to
   * receive to prevent repeatedly loading profiles.
   * @type {number}
   */
  _pendingChangeCount: 0,

  /**
   * Get the selected options on the profiles element.
   *
   * @returns {array<DOMElement>}
   */
  get _selectedOptions() {
    return Array.from(this._elements.profiles.selectedOptions);
  },

  init() {
    this._elements = {
      profiles: document.getElementById("profiles"),
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
   * Load profiles and render them.
   *
   * @returns {promise}
   */
  loadProfiles() {
    return this.getProfiles().then(profiles => {
      log.debug("profiles:", profiles);
      this.renderProfileElements(profiles);
      this.updateButtonsStates(this._selectedOptions.length);
    });
  },

  /**
   * Get profiles from storage.
   *
   * @returns {promise}
   */
  getProfiles() {
    return new Promise(resolve => {
      Services.cpmm.addMessageListener("FormAutofill:Profiles", function getResult(result) {
        Services.cpmm.removeMessageListener("FormAutofill:Profiles", getResult);
        resolve(result.data);
      });
      Services.cpmm.sendAsyncMessage("FormAutofill:GetProfiles", {});
    });
  },

  /**
   * Render the profiles onto the page while maintaining selected options if
   * they still exist.
   *
   * @param  {array<object>} profiles
   */
  renderProfileElements(profiles) {
    let selectedGuids = this._selectedOptions.map(option => option.value);
    this.clearProfileElements();
    for (let profile of profiles) {
      let option = new Option(this.getProfileLabel(profile),
                              profile.guid,
                              false,
                              selectedGuids.includes(profile.guid));
      option.profile = profile;
      this._elements.profiles.appendChild(option);
    }
  },

  /**
   * Remove all existing profile elements.
   */
  clearProfileElements() {
    let parent = this._elements.profiles;
    while (parent.lastChild) {
      parent.removeChild(parent.lastChild);
    }
  },

  /**
   * Remove profiles by guids.
   * Keep track of the number of "formautofill-storage-changed" events to
   * ignore before loading profiles.
   *
   * @param  {array<string>} guids
   */
  removeProfiles(guids) {
    this._pendingChangeCount += guids.length - 1;
    Services.cpmm.sendAsyncMessage("FormAutofill:RemoveProfiles", {guids});
  },

  /**
   * Get profile display label. It should display up to two pieces of
   * information, separated by a comma.
   *
   * @param  {object} profile
   * @returns {string}
   */
  getProfileLabel(profile) {
    // TODO: Implement a smarter way for deciding what to display
    //       as option text. Possibly improve the algorithm in
    //       ProfileAutoCompleteResult.jsm and reuse it here.
    const fieldOrder = [
      "street-address",  // Street address
      "address-level2",  // City/Town
      "organization",    // Company or organization name
      "address-level1",  // Province/State (Standardized code if possible)
      "country",         // Country
      "postal-code",     // Postal code
      "tel",             // Phone number
      "email",           // Email address
    ];

    let parts = [];
    for (const fieldName of fieldOrder) {
      let string = profile[fieldName];
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
   * Open the edit profile dialog to create/edit a profile.
   *
   * @param  {object} profile [optional]
   */
  openEditDialog(profile) {
    window.openDialog(EDIT_PROFILE_URL, null,
                      "chrome,centerscreen,modal,width=600,height=370",
                      profile);
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
        this.loadProfiles();
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
    }
  },

  /**
   * Handle click events
   *
   * @param  {DOMEvent} event
   */
  handleClick(event) {
    if (event.target == this._elements.remove) {
      this.removeProfiles(this._selectedOptions.map(option => option.value));
    } else if (event.target == this._elements.add) {
      this.openEditDialog();
    } else if (event.target == this._elements.edit) {
      this.openEditDialog(this._selectedOptions[0].profile);
    }
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "formautofill-storage-changed": {
        if (this._pendingChangeCount) {
          this._pendingChangeCount -= 1;
          return;
        }
        this.loadProfiles();
      }
    }
  },

  /**
   * Attach event listener
   */
  attachEventListeners() {
    window.addEventListener("unload", this, {once: true});
    this._elements.profiles.addEventListener("change", this);
    this._elements.controlsContainer.addEventListener("click", this);
    Services.obs.addObserver(this, "formautofill-storage-changed", false);
  },

  /**
   * Remove event listener
   */
  detachEventListeners() {
    this._elements.profiles.removeEventListener("change", this);
    this._elements.controlsContainer.removeEventListener("click", this);
    Services.obs.removeObserver(this, "formautofill-storage-changed");
  },
};

new ManageProfileDialog();
