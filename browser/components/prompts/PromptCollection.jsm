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
  beforeUnloadCheck(browsingContext) {
    let title;
    let message;
    let leaveLabel;
    let stayLabel;

    try {
      title = this.domBundle.GetStringFromName("OnBeforeUnloadTitle");
      message = this.domBundle.GetStringFromName("OnBeforeUnloadMessage");
      leaveLabel = this.domBundle.GetStringFromName(
        "OnBeforeUnloadLeaveButton"
      );
      stayLabel = this.domBundle.GetStringFromName("OnBeforeUnloadStayButton");
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

XPCOMUtils.defineLazyGetter(
  PromptCollection.prototype,
  "domBundle",
  function() {
    let bundle = Services.strings.createBundle(
      "chrome://global/locale/dom/dom.properties"
    );
    if (!bundle) {
      throw new Error("String bundle for dom not present!");
    }
    return bundle;
  }
);

PromptCollection.prototype.classID = Components.ID(
  "{7913837c-9623-11ea-bb37-0242ac130002}"
);
PromptCollection.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIPromptCollection",
]);
