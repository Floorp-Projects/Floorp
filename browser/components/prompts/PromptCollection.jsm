/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["PromptCollection"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

/**
 * Implements nsIPromptCollection
 * @class PromptCollection
 */
class PromptCollection {
  confirmRepost(browsingContext) {
    let brandName;
    try {
      brandName = this.stringBundles.brand.GetStringFromName("brandShortName");
    } catch (exception) {
      // That's ok, we'll use a generic version of the prompt
    }

    let message;
    let resendLabel;
    try {
      if (brandName) {
        message = this.stringBundles.app.formatStringFromName(
          "confirmRepostPrompt",
          [brandName]
        );
      } else {
        // Use a generic version of this prompt.
        message = this.stringBundles.app.GetStringFromName(
          "confirmRepostPrompt"
        );
      }
      resendLabel = this.stringBundles.app.GetStringFromName(
        "resendButton.label"
      );
    } catch (exception) {
      Cu.reportError("Failed to get strings from appstrings.properties");
      return false;
    }

    let contentViewer = browsingContext?.docShell?.contentViewer;
    let modalType = contentViewer?.isTabModalPromptAllowed
      ? Ci.nsIPromptService.MODAL_TYPE_CONTENT
      : Ci.nsIPromptService.MODAL_TYPE_WINDOW;
    let buttonFlags =
      (Ci.nsIPromptService.BUTTON_TITLE_IS_STRING *
        Ci.nsIPromptService.BUTTON_POS_0) |
      (Ci.nsIPromptService.BUTTON_TITLE_CANCEL *
        Ci.nsIPromptService.BUTTON_POS_1);
    let buttonPressed = Services.prompt.confirmExBC(
      browsingContext,
      modalType,
      null,
      message,
      buttonFlags,
      resendLabel,
      null,
      null,
      null,
      {}
    );

    return buttonPressed === 0;
  }

  beforeUnloadCheck(browsingContext) {
    let title;
    let message;
    let leaveLabel;
    let stayLabel;

    try {
      title = this.stringBundles.dom.GetStringFromName("OnBeforeUnloadTitle");
      message = this.stringBundles.dom.GetStringFromName(
        "OnBeforeUnloadMessage"
      );
      leaveLabel = this.stringBundles.dom.GetStringFromName(
        "OnBeforeUnloadLeaveButton"
      );
      stayLabel = this.stringBundles.dom.GetStringFromName(
        "OnBeforeUnloadStayButton"
      );
    } catch (exception) {
      Cu.reportError("Failed to get strings from dom.properties");
      return false;
    }

    let contentViewer = browsingContext?.docShell?.contentViewer;
    let modalType = contentViewer?.isTabModalPromptAllowed
      ? Ci.nsIPromptService.MODAL_TYPE_CONTENT
      : Ci.nsIPromptService.MODAL_TYPE_WINDOW;

    let buttonFlags =
      Ci.nsIPromptService.BUTTON_POS_0_DEFAULT |
      (Ci.nsIPromptService.BUTTON_TITLE_IS_STRING *
        Ci.nsIPromptService.BUTTON_POS_0) |
      (Ci.nsIPromptService.BUTTON_TITLE_IS_STRING *
        Ci.nsIPromptService.BUTTON_POS_1);

    let buttonPressed = Services.prompt.confirmExBC(
      browsingContext,
      modalType,
      title,
      message,
      buttonFlags,
      leaveLabel,
      stayLabel,
      null,
      null,
      {}
    );

    return buttonPressed === 0;
  }
}

const BUNDLES = {
  dom: "chrome://global/locale/dom/dom.properties",
  app: "chrome://global/locale/appstrings.properties",
  brand: "chrome://branding/locale/brand.properties",
};

PromptCollection.prototype.stringBundles = {};

for (const [bundleName, bundleUrl] of Object.entries(BUNDLES)) {
  XPCOMUtils.defineLazyGetter(
    PromptCollection.prototype.stringBundles,
    bundleName,
    function() {
      let bundle = Services.strings.createBundle(bundleUrl);
      if (!bundle) {
        throw new Error("String bundle for dom not present!");
      }
      return bundle;
    }
  );
}

PromptCollection.prototype.classID = Components.ID(
  "{7913837c-9623-11ea-bb37-0242ac130002}"
);
PromptCollection.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIPromptCollection",
]);
