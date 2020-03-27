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

let lastOpenManagementOuterWindowID = null;
let lastOpenManagementEventTime = Number.NEGATIVE_INFINITY;
let masterPasswordPromise;

class AboutLoginsChild extends JSWindowActorChild {
  handleEvent(event) {
    switch (event.type) {
      case "AboutLoginsInit": {
        this.sendAsyncMessage("AboutLogins:Subscribe");

        let documentElement = this.document.documentElement;
        documentElement.classList.toggle(
          "official-branding",
          AppConstants.MOZILLA_OFFICIAL
        );

        let waivedContent = Cu.waiveXrays(this.browsingContext.window);
        let that = this;
        let AboutLoginsUtils = {
          doLoginsMatch(loginA, loginB) {
            return LoginHelper.doLoginsMatch(loginA, loginB, {});
          },
          getLoginOrigin(uriString) {
            return LoginHelper.getLoginOrigin(uriString);
          },
          /**
           * Shows the Master Password prompt if enabled, or the
           * OS auth dialog otherwise.
           * @param resolve Callback that is called with result of authentication.
           * @param messageId The string ID that corresponds to a string stored in aboutLogins.ftl.
           *                  This string will be displayed only when the OS auth dialog is used.
           */
          async promptForMasterPassword(resolve, messageId) {
            masterPasswordPromise = {
              resolve,
            };

            that.sendAsyncMessage(
              "AboutLogins:MasterPasswordRequest",
              messageId
            );

            return masterPasswordPromise;
          },
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

        const SUPPORT_URL =
          Services.urlFormatter.formatURLPref("app.support.baseURL") +
          "firefox-lockwise";
        let loginIntro = Cu.waiveXrays(
          this.document.querySelector("login-intro")
        );
        loginIntro.supportURL = SUPPORT_URL;
        break;
      }
      case "AboutLoginsCopyLoginDetail": {
        ClipboardHelper.copyString(event.detail);
        break;
      }
      case "AboutLoginsCreateLogin": {
        this.sendAsyncMessage("AboutLogins:CreateLogin", {
          login: event.detail,
        });
        break;
      }
      case "AboutLoginsDeleteLogin": {
        this.sendAsyncMessage("AboutLogins:DeleteLogin", {
          login: event.detail,
        });
        break;
      }
      case "AboutLoginsGetHelp": {
        this.sendAsyncMessage("AboutLogins:GetHelp");
        break;
      }
      case "AboutLoginsHideFooter": {
        this.sendAsyncMessage("AboutLogins:HideFooter");
        break;
      }
      case "AboutLoginsImport": {
        this.sendAsyncMessage("AboutLogins:Import");
        break;
      }
      case "AboutLoginsOpenMobileAndroid": {
        this.sendAsyncMessage("AboutLogins:OpenMobileAndroid", {
          source: event.detail,
        });
        break;
      }
      case "AboutLoginsOpenMobileIos": {
        this.sendAsyncMessage("AboutLogins:OpenMobileIos", {
          source: event.detail,
        });
        break;
      }
      case "AboutLoginsOpenPreferences": {
        this.sendAsyncMessage("AboutLogins:OpenPreferences");
        break;
      }
      case "AboutLoginsRecordTelemetryEvent": {
        let { method, object, extra = {} } = event.detail;

        if (method == "open_management") {
          let { docShell } = this.browsingContext;
          // Compare to the last time open_management was recorded for the same
          // outerWindowID to not double-count them due to a redirect to remove
          // the entryPoint query param (since replaceState isn't allowed for
          // about:). Don't use performance.now for the tab since you can't
          // compare that number between different tabs and this JSM is shared.
          let now = docShell.now();
          if (
            docShell.outerWindowID == lastOpenManagementOuterWindowID &&
            now - lastOpenManagementEventTime <
              TELEMETRY_MIN_MS_BETWEEN_OPEN_MANAGEMENT
          ) {
            return;
          }
          lastOpenManagementEventTime = now;
          lastOpenManagementOuterWindowID = docShell.outerWindowID;
        }

        try {
          Services.telemetry.recordEvent(
            TELEMETRY_EVENT_CATEGORY,
            method,
            object,
            null,
            extra
          );
        } catch (ex) {
          Cu.reportError(
            "AboutLoginsChild: error recording telemetry event: " + ex.message
          );
        }
        break;
      }
      case "AboutLoginsSortChanged": {
        this.sendAsyncMessage("AboutLogins:SortChanged", event.detail);
        break;
      }
      case "AboutLoginsSyncEnable": {
        this.sendAsyncMessage("AboutLogins:SyncEnable");
        break;
      }
      case "AboutLoginsSyncOptions": {
        this.sendAsyncMessage("AboutLogins:SyncOptions");
        break;
      }
      case "AboutLoginsUpdateLogin": {
        this.sendAsyncMessage("AboutLogins:UpdateLogin", {
          login: event.detail,
        });
        break;
      }
    }
  }

  receiveMessage(message) {
    switch (message.name) {
      case "AboutLogins:MasterPasswordResponse":
        if (masterPasswordPromise) {
          masterPasswordPromise.resolve(message.data);
        }
        break;
      case "AboutLogins:Setup":
        let waivedContent = Cu.waiveXrays(this.browsingContext.window);
        waivedContent.AboutLoginsUtils.masterPasswordEnabled =
          message.data.masterPasswordEnabled;
        waivedContent.AboutLoginsUtils.passwordRevealVisible =
          message.data.passwordRevealVisible;
        waivedContent.AboutLoginsUtils.importVisible =
          message.data.importVisible;
        this.sendToContent("Setup", message.data);
        break;
      default:
        this.passMessageDataToContent(message);
    }
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
