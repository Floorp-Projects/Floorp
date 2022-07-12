/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutLoginsChild"];

const { LoginHelper } = ChromeUtils.import(
  "resource://gre/modules/LoginHelper.jsm"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "ClipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1",
  "nsIClipboardHelper"
);

const TELEMETRY_EVENT_CATEGORY = "pwmgr";
const TELEMETRY_MIN_MS_BETWEEN_OPEN_MANAGEMENT = 5000;

let gLastOpenManagementBrowserId = null;
let gLastOpenManagementEventTime = Number.NEGATIVE_INFINITY;
let gPrimaryPasswordPromise;

function recordTelemetryEvent(event) {
  try {
    let { method, object, extra = {}, value = null } = event;
    Services.telemetry.recordEvent(
      TELEMETRY_EVENT_CATEGORY,
      method,
      object,
      value,
      extra
    );
  } catch (ex) {
    Cu.reportError(
      "AboutLoginsChild: error recording telemetry event: " + ex.message
    );
  }
}

class AboutLoginsChild extends JSWindowActorChild {
  handleEvent(event) {
    switch (event.type) {
      case "AboutLoginsInit": {
        this.#aboutLoginsInit();
        break;
      }
      case "AboutLoginsImportReportInit": {
        this.#aboutLoginsImportReportInit();
        break;
      }
      case "AboutLoginsCopyLoginDetail": {
        this.#aboutLoginsCopyLoginDetail(event.detail);
        break;
      }
      case "AboutLoginsCreateLogin": {
        this.#aboutLoginsCreateLogin(event.detail);
        break;
      }
      case "AboutLoginsDeleteLogin": {
        this.#aboutLoginsDeleteLogin(event.detail);
        break;
      }
      case "AboutLoginsExportPasswords": {
        this.#aboutLoginsExportPasswords();
        break;
      }
      case "AboutLoginsGetHelp": {
        this.#aboutLoginsGetHelp();
        break;
      }
      case "AboutLoginsImportFromBrowser": {
        this.#aboutLoginsImportFromBrowser();
        break;
      }
      case "AboutLoginsImportFromFile": {
        this.#aboutLoginsImportFromFile();
        break;
      }
      case "AboutLoginsOpenPreferences": {
        this.#aboutLoginsOpenPreferences();
        break;
      }
      case "AboutLoginsRecordTelemetryEvent": {
        this.#aboutLoginsRecordTelemetryEvent(event);
        break;
      }
      case "AboutLoginsRemoveAllLogins": {
        this.#aboutLoginsRemoveAllLogins();
        break;
      }
      case "AboutLoginsSortChanged": {
        this.#aboutLoginsSortChanged(event.detail);
        break;
      }
      case "AboutLoginsSyncEnable": {
        this.#aboutLoginsSyncEnable();
        break;
      }
      case "AboutLoginsSyncOptions": {
        this.#aboutLoginsSyncOptions();
        break;
      }
      case "AboutLoginsUpdateLogin": {
        this.#aboutLoginsUpdateLogin(event.detail);
        break;
      }
    }
  }

  #aboutLoginsInit() {
    this.sendAsyncMessage("AboutLogins:Subscribe");

    let win = this.browsingContext.window;
    let waivedContent = Cu.waiveXrays(win);
    let that = this;
    let AboutLoginsUtils = {
      doLoginsMatch(loginA, loginB) {
        return LoginHelper.doLoginsMatch(loginA, loginB, {});
      },
      getLoginOrigin(uriString) {
        return LoginHelper.getLoginOrigin(uriString);
      },
      setFocus(element) {
        Services.focus.setFocus(element, Services.focus.FLAG_BYKEY);
      },
      /**
       * Shows the Primary Password prompt if enabled, or the
       * OS auth dialog otherwise.
       * @param resolve Callback that is called with result of authentication.
       * @param messageId The string ID that corresponds to a string stored in aboutLogins.ftl.
       *                  This string will be displayed only when the OS auth dialog is used.
       */
      async promptForPrimaryPassword(resolve, messageId) {
        gPrimaryPasswordPromise = {
          resolve,
        };

        that.sendAsyncMessage("AboutLogins:PrimaryPasswordRequest", messageId);

        return gPrimaryPasswordPromise;
      },
      fileImportEnabled: Services.prefs.getBoolPref(
        "signon.management.page.fileImport.enabled"
      ),
      // Default to enabled just in case a search is attempted before we get a response.
      primaryPasswordEnabled: true,
      passwordRevealVisible: true,
    };
    waivedContent.AboutLoginsUtils = Cu.cloneInto(
      AboutLoginsUtils,
      waivedContent,
      {
        cloneFunctions: true,
      }
    );
  }

  #aboutLoginsImportReportInit() {
    this.sendAsyncMessage("AboutLogins:ImportReportInit");
  }

  #aboutLoginsCopyLoginDetail(detail) {
    lazy.ClipboardHelper.copyString(detail, lazy.ClipboardHelper.Sensitive);
  }

  #aboutLoginsCreateLogin(login) {
    this.sendAsyncMessage("AboutLogins:CreateLogin", {
      login,
    });
  }

  #aboutLoginsDeleteLogin(login) {
    this.sendAsyncMessage("AboutLogins:DeleteLogin", {
      login,
    });
  }

  #aboutLoginsExportPasswords() {
    this.sendAsyncMessage("AboutLogins:ExportPasswords");
  }

  #aboutLoginsGetHelp() {
    this.sendAsyncMessage("AboutLogins:GetHelp");
  }

  #aboutLoginsImportFromBrowser() {
    this.sendAsyncMessage("AboutLogins:ImportFromBrowser");
    recordTelemetryEvent({
      object: "import_from_browser",
      method: "mgmt_menu_item_used",
    });
  }

  #aboutLoginsImportFromFile() {
    this.sendAsyncMessage("AboutLogins:ImportFromFile");
    recordTelemetryEvent({
      object: "import_from_csv",
      method: "mgmt_menu_item_used",
    });
  }

  #aboutLoginsOpenPreferences() {
    this.sendAsyncMessage("AboutLogins:OpenPreferences");
    recordTelemetryEvent({
      object: "preferences",
      method: "mgmt_menu_item_used",
    });
  }

  #aboutLoginsRecordTelemetryEvent(event) {
    let { method } = event.detail;

    if (method == "open_management") {
      let { docShell } = this.browsingContext;
      // Compare to the last time open_management was recorded for the same
      // outerWindowID to not double-count them due to a redirect to remove
      // the entryPoint query param (since replaceState isn't allowed for
      // about:). Don't use performance.now for the tab since you can't
      // compare that number between different tabs and this JSM is shared.
      let now = docShell.now();
      if (
        this.browsingContext.browserId == gLastOpenManagementBrowserId &&
        now - gLastOpenManagementEventTime <
          TELEMETRY_MIN_MS_BETWEEN_OPEN_MANAGEMENT
      ) {
        return;
      }
      gLastOpenManagementEventTime = now;
      gLastOpenManagementBrowserId = this.browsingContext.browserId;
    }
    recordTelemetryEvent(event.detail);
  }

  #aboutLoginsRemoveAllLogins() {
    this.sendAsyncMessage("AboutLogins:RemoveAllLogins");
  }

  #aboutLoginsSortChanged(detail) {
    this.sendAsyncMessage("AboutLogins:SortChanged", detail);
  }

  #aboutLoginsSyncEnable() {
    this.sendAsyncMessage("AboutLogins:SyncEnable");
  }

  #aboutLoginsSyncOptions() {
    this.sendAsyncMessage("AboutLogins:SyncOptions");
  }

  #aboutLoginsUpdateLogin(login) {
    this.sendAsyncMessage("AboutLogins:UpdateLogin", {
      login,
    });
  }

  receiveMessage(message) {
    switch (message.name) {
      case "AboutLogins:ImportReportData":
        this.#importReportData(message.data);
        break;
      case "AboutLogins:PrimaryPasswordResponse":
        this.#primaryPasswordResponse(message.data);
        break;
      case "AboutLogins:RemaskPassword":
        this.#remaskPassword(message.data);
        break;
      case "AboutLogins:Setup":
        this.#setup(message.data);
        break;
      default:
        this.#passMessageDataToContent(message);
    }
  }

  #importReportData(data) {
    this.sendToContent("ImportReportData", data);
  }

  #primaryPasswordResponse(data) {
    if (gPrimaryPasswordPromise) {
      gPrimaryPasswordPromise.resolve(data.result);
      recordTelemetryEvent(data.telemetryEvent);
    }
  }

  #remaskPassword(data) {
    this.sendToContent("RemaskPassword", data);
  }

  #setup(data) {
    let utils = Cu.waiveXrays(this.browsingContext.window).AboutLoginsUtils;
    utils.primaryPasswordEnabled = data.primaryPasswordEnabled;
    utils.passwordRevealVisible = data.passwordRevealVisible;
    utils.importVisible = data.importVisible;
    utils.supportBaseURL = Services.urlFormatter.formatURLPref(
      "app.support.baseURL"
    );
    this.sendToContent("Setup", data);
  }

  #passMessageDataToContent(message) {
    this.sendToContent(message.name.replace("AboutLogins:", ""), message.data);
  }

  sendToContent(messageType, detail) {
    let win = this.document.defaultView;
    let message = Object.assign({ messageType }, { value: detail });
    let event = new win.CustomEvent("AboutLoginsChromeToContent", {
      detail: Cu.cloneInto(message, win),
    });
    win.dispatchEvent(event);
  }
}
