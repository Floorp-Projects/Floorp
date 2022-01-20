/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutLoginsChild"];

const { LoginHelper } = ChromeUtils.import(
  "resource://gre/modules/LoginHelper.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "ClipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1",
  "nsIClipboardHelper"
);

const TELEMETRY_EVENT_CATEGORY = "pwmgr";
const TELEMETRY_MIN_MS_BETWEEN_OPEN_MANAGEMENT = 5000;

let gLastOpenManagementBrowserId = null;
let gLastOpenManagementEventTime = Number.NEGATIVE_INFINITY;
let gMasterPasswordPromise;

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
        this.onAboutLoginsInit();
        break;
      }
      case "AboutLoginsImportReportInit": {
        this.onAboutLoginsImportReportInit();
        break;
      }
      case "AboutLoginsCopyLoginDetail": {
        this.onAboutLoginsCopyLoginDetail(event.detail);
        break;
      }
      case "AboutLoginsCreateLogin": {
        this.onAboutLoginsCreateLogin(event.detail);
        break;
      }
      case "AboutLoginsDeleteLogin": {
        this.onAboutLoginsDeleteLogin(event.detail);
        break;
      }
      case "AboutLoginsExportPasswords": {
        this.onAboutLoginsExportPasswords();
        break;
      }
      case "AboutLoginsGetHelp": {
        this.onAboutLoginsGetHelp();
        break;
      }
      case "AboutLoginsImportFromBrowser": {
        this.onAboutLoginsImportFromBrowser();
        break;
      }
      case "AboutLoginsImportFromFile": {
        this.onAboutLoginsImportFromFile();
        break;
      }
      case "AboutLoginsOpenPreferences": {
        this.onAboutLoginsOpenPreferences();
        break;
      }
      case "AboutLoginsRecordTelemetryEvent": {
        this.onAboutLoginsRecordTelemetryEvent(event);
        break;
      }
      case "AboutLoginsRemoveAllLogins": {
        this.onAboutLoginsRemoveAllLogins();
        break;
      }
      case "AboutLoginsSortChanged": {
        this.onAboutLoginsSortChanged(event.detail);
        break;
      }
      case "AboutLoginsSyncEnable": {
        this.onAboutLoginsSyncEnable();
        break;
      }
      case "AboutLoginsSyncOptions": {
        this.onAboutLoginsSyncOptions();
        break;
      }
      case "AboutLoginsUpdateLogin": {
        this.onAboutLoginsUpdateLogin(event.detail);
        break;
      }
    }
  }

  onAboutLoginsInit() {
    this.sendAsyncMessage("AboutLogins:Subscribe");

    let documentElement = this.document.documentElement;
    documentElement.classList.toggle(
      "official-branding",
      AppConstants.MOZILLA_OFFICIAL
    );

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
       * Shows the Master Password prompt if enabled, or the
       * OS auth dialog otherwise.
       * @param resolve Callback that is called with result of authentication.
       * @param messageId The string ID that corresponds to a string stored in aboutLogins.ftl.
       *                  This string will be displayed only when the OS auth dialog is used.
       */
      async promptForMasterPassword(resolve, messageId) {
        gMasterPasswordPromise = {
          resolve,
        };

        that.sendAsyncMessage("AboutLogins:PrimaryPasswordRequest", messageId);

        return gMasterPasswordPromise;
      },
      fileImportEnabled: Services.prefs.getBoolPref(
        "signon.management.page.fileImport.enabled"
      ),
      // Default to enabled just in case a search is attempted before we get a response.
      masterPasswordEnabled: true,
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

  onAboutLoginsImportReportInit() {
    this.sendAsyncMessage("AboutLogins:ImportReportInit");
    let documentElement = this.document.documentElement;
    documentElement.classList.toggle(
      "official-branding",
      AppConstants.MOZILLA_OFFICIAL
    );
  }

  onAboutLoginsCopyLoginDetail(detail) {
    ClipboardHelper.copyString(detail, ClipboardHelper.Sensitive);
  }

  onAboutLoginsCreateLogin(login) {
    this.sendAsyncMessage("AboutLogins:CreateLogin", {
      login,
    });
  }

  onAboutLoginsDeleteLogin(login) {
    this.sendAsyncMessage("AboutLogins:DeleteLogin", {
      login,
    });
  }

  onAboutLoginsExportPasswords() {
    this.sendAsyncMessage("AboutLogins:ExportPasswords");
  }

  onAboutLoginsGetHelp() {
    this.sendAsyncMessage("AboutLogins:GetHelp");
  }

  onAboutLoginsImportFromBrowser() {
    this.sendAsyncMessage("AboutLogins:ImportFromBrowser");
    recordTelemetryEvent({
      object: "import_from_browser",
      method: "mgmt_menu_item_used",
    });
  }

  onAboutLoginsImportFromFile() {
    this.sendAsyncMessage("AboutLogins:ImportFromFile");
    recordTelemetryEvent({
      object: "import_from_csv",
      method: "mgmt_menu_item_used",
    });
  }

  onAboutLoginsOpenPreferences() {
    this.sendAsyncMessage("AboutLogins:OpenPreferences");
    recordTelemetryEvent({
      object: "preferences",
      method: "mgmt_menu_item_used",
    });
  }

  onAboutLoginsRecordTelemetryEvent(event) {
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

  onAboutLoginsRemoveAllLogins() {
    this.sendAsyncMessage("AboutLogins:RemoveAllLogins");
  }

  onAboutLoginsSortChanged(detail) {
    this.sendAsyncMessage("AboutLogins:SortChanged", detail);
  }

  onAboutLoginsSyncEnable() {
    this.sendAsyncMessage("AboutLogins:SyncEnable");
  }

  onAboutLoginsSyncOptions() {
    this.sendAsyncMessage("AboutLogins:SyncOptions");
  }

  onAboutLoginsUpdateLogin(login) {
    this.sendAsyncMessage("AboutLogins:UpdateLogin", {
      login,
    });
  }

  receiveMessage(message) {
    switch (message.name) {
      case "AboutLogins:ImportReportData":
        this.onImportReportData(message.data);
        break;
      case "AboutLogins:MasterPasswordResponse":
        this.onMasterPasswordResponse(message.data);
        break;
      case "AboutLogins:RemaskPassword":
        this.onRemaskPassword(message.data);
        break;
      case "AboutLogins:Setup":
        this.onSetup(message.data);
        break;
      default:
        this.passMessageDataToContent(message);
    }
  }

  onImportReportData(data) {
    this.sendToContent("ImportReportData", data);
  }

  onMasterPasswordResponse(data) {
    if (gMasterPasswordPromise) {
      gMasterPasswordPromise.resolve(data.result);
      recordTelemetryEvent(data.telemetryEvent);
    }
  }

  onRemaskPassword(data) {
    this.sendToContent("RemaskPassword", data);
  }

  onSetup(data) {
    let waivedContent = Cu.waiveXrays(this.browsingContext.window);
    waivedContent.AboutLoginsUtils.masterPasswordEnabled =
      data.masterPasswordEnabled;
    waivedContent.AboutLoginsUtils.passwordRevealVisible =
      data.passwordRevealVisible;
    waivedContent.AboutLoginsUtils.importVisible = data.importVisible;
    waivedContent.AboutLoginsUtils.supportBaseURL = Services.urlFormatter.formatURLPref(
      "app.support.baseURL"
    );
    this.sendToContent("Setup", data);
  }

  passMessageDataToContent(message) {
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
