/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This frame script only exists to mediate communications between the
 * unprivileged frame in a content process and the privileged dialog wrapper
 * in the UI process on the main thread.
 *
 * `paymentChromeToContent` messages from the privileged wrapper are converted
 * into DOM events of the same name.
 * `paymentContentToChrome` custom DOM events from the unprivileged frame are
 * converted into messages of the same name.
 *
 * Business logic should stay out of this shim.
 */

"use strict";

/* eslint-env mozilla/frame-script */
/* global Services */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "FormAutofill",
  "resource://formautofill/FormAutofill.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FormAutofillUtils",
  "resource://formautofill/FormAutofillUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);

const SAVE_CREDITCARD_DEFAULT_PREF = "dom.payments.defaults.saveCreditCard";
const SAVE_ADDRESS_DEFAULT_PREF = "dom.payments.defaults.saveAddress";

let PaymentFrameScript = {
  init() {
    XPCOMUtils.defineLazyGetter(this, "log", () => {
      let { ConsoleAPI } = ChromeUtils.import(
        "resource://gre/modules/Console.jsm"
      );
      return new ConsoleAPI({
        maxLogLevelPref: "dom.payments.loglevel",
        prefix: "paymentDialogFrameScript",
      });
    });

    addEventListener("paymentContentToChrome", this, false, true);

    addMessageListener("paymentChromeToContent", this);
  },

  handleEvent(event) {
    this.sendToChrome(event);
  },

  receiveMessage({ data: { messageType, data } }) {
    this.sendToContent(messageType, data);
  },

  setupContentConsole() {
    let privilegedLogger = content.window.console.createInstance({
      maxLogLevelPref: "dom.payments.loglevel",
      prefix: "paymentDialogContent",
    });

    let contentLogObject = Cu.waiveXrays(content).log;
    for (let name of ["error", "warn", "info", "debug"]) {
      Cu.exportFunction(
        privilegedLogger[name].bind(privilegedLogger),
        contentLogObject,
        {
          defineAs: name,
        }
      );
    }
  },

  /**
   * Expose privileged utility functions to the unprivileged page.
   */
  exposeUtilityFunctions() {
    let waivedContent = Cu.waiveXrays(content);
    let PaymentDialogUtils = {
      DEFAULT_REGION: FormAutofill.DEFAULT_REGION,
      countries: FormAutofill.countries,

      getAddressLabel(address, addressFields = null) {
        return FormAutofillUtils.getAddressLabel(address, addressFields);
      },

      getCreditCardNetworks() {
        let networks = FormAutofillUtils.getCreditCardNetworks();
        return Cu.cloneInto(networks, waivedContent);
      },

      isCCNumber(value) {
        return FormAutofillUtils.isCCNumber(value);
      },

      getFormFormat(country) {
        let format = FormAutofillUtils.getFormFormat(country);
        return Cu.cloneInto(format, waivedContent);
      },

      findAddressSelectOption(selectEl, address, fieldName) {
        return FormAutofillUtils.findAddressSelectOption(
          selectEl,
          address,
          fieldName
        );
      },

      getDefaultPreferences() {
        let prefValues = Cu.cloneInto(
          {
            saveCreditCardDefaultChecked: Services.prefs.getBoolPref(
              SAVE_CREDITCARD_DEFAULT_PREF,
              false
            ),
            saveAddressDefaultChecked: Services.prefs.getBoolPref(
              SAVE_ADDRESS_DEFAULT_PREF,
              false
            ),
          },
          waivedContent
        );
        return Cu.cloneInto(prefValues, waivedContent);
      },

      isOfficialBranding() {
        return AppConstants.MOZILLA_OFFICIAL;
      },
    };
    waivedContent.PaymentDialogUtils = Cu.cloneInto(
      PaymentDialogUtils,
      waivedContent,
      {
        cloneFunctions: true,
      }
    );
  },

  sendToChrome({ detail }) {
    let { messageType } = detail;
    if (messageType == "initializeRequest") {
      this.setupContentConsole();
      this.exposeUtilityFunctions();
    }
    this.log.debug("sendToChrome:", messageType, detail);
    this.sendMessageToChrome(messageType, detail);
  },

  sendToContent(messageType, detail = {}) {
    this.log.debug("sendToContent", messageType, detail);
    let response = Object.assign({ messageType }, detail);
    let event = new content.CustomEvent("paymentChromeToContent", {
      detail: Cu.cloneInto(response, content),
    });
    content.dispatchEvent(event);
  },

  sendMessageToChrome(messageType, data = {}) {
    sendAsyncMessage(
      "paymentContentToChrome",
      Object.assign(data, { messageType })
    );
  },
};

PaymentFrameScript.init();
