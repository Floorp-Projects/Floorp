/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implements doorhanger singleton that wraps up the PopupNotifications and handles
 * the doorhager UI for formautofill related features.
 */

/* exported FormAutofillDoorhanger */

"use strict";

this.EXPORTED_SYMBOLS = ["FormAutofillDoorhanger"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://formautofill/FormAutofillUtils.jsm");

this.log = null;
FormAutofillUtils.defineLazyLogGetter(this, this.EXPORTED_SYMBOLS[0]);

const BUNDLE_URI = "chrome://formautofill/locale/formautofill.properties";
const GetStringFromName = Services.strings.createBundle(BUNDLE_URI).GetStringFromName;
let changeAutofillOptsKey = "changeAutofillOptions";
let viewAutofillOptsKey = "viewAutofillOptionsLink";
if (AppConstants.platform != "macosx") {
  changeAutofillOptsKey += "OSX";
  viewAutofillOptsKey += "OSX";
}

const CONTENT = {
  firstTimeUse: {
    notificationId: "autofill-address",
    message: GetStringFromName("saveAddressesMessage"),
    anchor: {
      id: "autofill-address-notification-icon",
      URL: "chrome://formautofill/content/formfill-anchor.svg",
      tooltiptext: GetStringFromName("openAutofillMessagePanel"),
    },
    mainAction: {
      label: GetStringFromName(changeAutofillOptsKey),
      accessKey: "C",
      callbackState: "open-pref",
    },
    options: {
      persistWhileVisible: true,
      popupIconURL: "chrome://formautofill/content/icon-address-save.svg",
    },
  },
  update: {
    notificationId: "autofill-address",
    message: GetStringFromName("updateAddressMessage"),
    anchor: {
      id: "autofill-address-notification-icon",
      URL: "chrome://formautofill/content/formfill-anchor.svg",
      tooltiptext: GetStringFromName("openAutofillMessagePanel"),
    },
    mainAction: {
      label: GetStringFromName("updateAddressLabel"),
      accessKey: "U",
      callbackState: "update",
    },
    secondaryActions: [{
      label: GetStringFromName("createAddressLabel"),
      accessKey: "C",
      callbackState: "create",
    }],
    options: {
      persistWhileVisible: true,
      popupIconURL: "chrome://formautofill/content/icon-address-update.svg",
    },
  },
};

let FormAutofillDoorhanger = {
  /**
   * Generate the main action and secondary actions from content parameters and
   * promise resolve.
   *
   * @private
   * @param  {Object} mainActionParams
   *         Parameters for main action.
   * @param  {Array<Object>} secondaryActionParams
   *         Array of the parameters for secondary actions.
   * @param  {Function} resolve Should be called in action callback.
   * @returns {Array<Object>}
              Return the mainAction and secondary actions in an array for showing doorhanger
   */
  _createActions(mainActionParams, secondaryActionParams, resolve) {
    if (!mainActionParams) {
      return [null, null];
    }

    let {label, accessKey, callbackState} = mainActionParams;
    let callback = resolve.bind(null, callbackState);
    let mainAction = {label, accessKey, callback};

    if (!secondaryActionParams) {
      return [mainAction, null];
    }

    let secondaryActions = [];
    for (let params of secondaryActionParams) {
      let cb = resolve.bind(null, params.callbackState);
      secondaryActions.push({
        label: params.label,
        accessKey: params.accessKey,
        callback: cb,
      });
    }

    return [mainAction, secondaryActions];
  },
  /**
   * Append the link label element to the popupnotificationcontent.
   * @param  {XULElement} browser
   *         Target browser element for showing doorhanger.
   * @param  {string} id
   *         The ID of the doorhanger.
   */
  _appendPrivacyPanelLink(browser, id) {
    let notificationId = id + "-notification";
    let chromeDoc = browser.ownerDocument;
    let notification = chromeDoc.getElementById(notificationId);

    if (!notification.querySelector("popupnotificationcontent")) {
      let notificationcontent = chromeDoc.createElement("popupnotificationcontent");
      let privacyLinkElement = chromeDoc.createElement("label");
      privacyLinkElement.className = "text-link";
      privacyLinkElement.setAttribute("useoriginprincipal", true);
      privacyLinkElement.setAttribute("href", "about:preferences#privacy");
      privacyLinkElement.setAttribute("value", GetStringFromName(viewAutofillOptsKey));
      notificationcontent.appendChild(privacyLinkElement);
      notification.append(notificationcontent);
    }
  },
  /**
   * Create an image element for notification anchor if it doesn't already exist.
   * @param  {XULElement} browser
   *         Target browser element for showing doorhanger.
   * @param  {Object} anchor
   *         Anchor options for setting the anchor element.
   * @param  {string} anchor.id
   *         ID of the anchor element.
   * @param  {string} anchor.URL
   *         Path of the icon asset.
   * @param  {string} anchor.tooltiptext
   *         Tooltip string for the anchor.
   */
  _setAnchor(browser, anchor) {
    let chromeDoc = browser.ownerDocument;
    let {id, URL, tooltiptext} = anchor;
    let anchorEt = chromeDoc.getElementById(id);
    if (!anchorEt) {
      let notificationPopupBox =
        chromeDoc.getElementById("notification-popup-box");
      // Icon shown on URL bar
      let anchorElement = chromeDoc.createElement("image");
      anchorElement.id = id;
      anchorElement.setAttribute("src", URL);
      anchorElement.classList.add("notification-anchor-icon");
      anchorElement.setAttribute("role", "button");
      anchorElement.setAttribute("tooltiptext", tooltiptext);
      notificationPopupBox.appendChild(anchorElement);
    }
  },
  /**
   * Show different types of doorhanger by leveraging PopupNotifications.
   * @param  {XULElement} browser
   *         Target browser element for showing doorhanger.
   * @param  {string} type
   *         The type of the doorhanger. There will have first time use/update/credit card.
   * @returns {Promise}
              Resolved with action type when action callback is triggered.
   */
  async show(browser, type) {
    log.debug("show doorhanger with type:", type);
    return new Promise((resolve) => {
      let content = CONTENT[type];
      let chromeWin = browser.ownerGlobal;
      content.options.eventCallback = (topic) => {
        log.debug("eventCallback:", topic);

        // The doorhanger is customizable only when notification box is shown
        if (topic != "shown") {
          return;
        }

        // There's no preferences link or other customization in first time use doorhanger.
        if (type == "firstTimeUse") {
          return;
        }

        this._appendPrivacyPanelLink(browser, content.notificationId);
      };
      this._setAnchor(browser, content.anchor);
      chromeWin.PopupNotifications.show(
        browser,
        content.notificationId,
        content.message,
        content.anchor.id,
        ...this._createActions(content.mainAction, content.secondaryActions, resolve),
        content.options,
      );
    });
  },
};
