/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Some specific (certified) apps need to get access to certain Firefox Accounts
 * functionality that allows them to manage accounts (this is mostly sign up,
 * sign in, logout and delete) and get information about the currently existing
 * ones.
 *
 * This service listens for requests coming from these apps, triggers the
 * appropriate Fx Accounts flows and send reponses back to the UI.
 *
 * The communication mechanism is based in mozFxAccountsContentEvent (for
 * messages coming from the UI) and mozFxAccountsChromeEvent (for messages
 * sent from the chrome side) custom events.
 */

"use strict";

this.EXPORTED_SYMBOLS = ["FxAccountsMgmtService"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");

XPCOMUtils.defineLazyModuleGetter(this, "FxAccountsManager",
  "resource://gre/modules/FxAccountsManager.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SystemAppProxy",
                                  "resource://gre/modules/SystemAppProxy.jsm");

this.FxAccountsMgmtService = {
  _onFulfill: function(aMsgId, aData) {
    SystemAppProxy._sendCustomEvent("mozFxAccountsChromeEvent", {
      id: aMsgId,
      data: aData ? aData : null
    });
  },

  _onReject: function(aMsgId, aReason) {
    SystemAppProxy._sendCustomEvent("mozFxAccountsChromeEvent", {
      id: aMsgId,
      error: aReason ? aReason : null
    });
  },

  init: function() {
    Services.obs.addObserver(this, ONLOGIN_NOTIFICATION, false);
    Services.obs.addObserver(this, ONVERIFIED_NOTIFICATION, false);
    Services.obs.addObserver(this, ONLOGOUT_NOTIFICATION, false);
    SystemAppProxy.addEventListener("mozFxAccountsContentEvent",
                                    FxAccountsMgmtService);
  },

  observe: function(aSubject, aTopic, aData) {
    log.debug("Observed " + aTopic);
    switch (aTopic) {
      case ONLOGIN_NOTIFICATION:
      case ONVERIFIED_NOTIFICATION:
      case ONLOGOUT_NOTIFICATION:
        // FxAccounts notifications have the form of fxaccounts:*
        SystemAppProxy._sendCustomEvent("mozFxAccountsUnsolChromeEvent", {
          eventName: aTopic.substring(aTopic.indexOf(":") + 1)
        });
        break;
    }
  },

  handleEvent: function(aEvent) {
    let msg = aEvent.detail;
    log.debug("MgmtService got content event: " + JSON.stringify(msg));
    let self = FxAccountsMgmtService;

    if (!msg.id) {
      return;
    }

    if (msg.error) {
      self._onReject(msg.id, msg.error);
      return;
    }

    let data = msg.data;
    if (!data) {
      return;
    }
    // Backwards compatibility: handle accountId coming from Gaia
    if (data.accountId && typeof(data.email === "undefined")) {
      data.email = data.accountId;
      delete data.accountId;
    }

    switch(data.method) {
      case "getAccounts":
        FxAccountsManager.getAccount().then(
          account => {
            // We only expose the email and verification status so far.
            self._onFulfill(msg.id, account);
          },
          reason => {
            self._onReject(msg.id, reason);
          }
        ).then(null, Components.utils.reportError);
        break;
      case "logout":
        FxAccountsManager.signOut().then(
          () => {
            self._onFulfill(msg.id);
          },
          reason => {
            self._onReject(msg.id, reason);
          }
        ).then(null, Components.utils.reportError);
        break;
      case "queryAccount":
        FxAccountsManager.queryAccount(data.email).then(
          result => {
            self._onFulfill(msg.id, result);
          },
          reason => {
            self._onReject(msg.id, reason);
          }
        ).then(null, Components.utils.reportError);
        break;
      case "resendVerificationEmail":
        FxAccountsManager.resendVerificationEmail().then(
          () => {
            self._onFulfill(msg.id);
          },
          reason => {
            self._onReject(msg.id, reason);
          }
        ).then(null, Components.utils.reportError);
        break;
      case "signIn":
      case "signUp":
      case "refreshAuthentication":
        FxAccountsManager[data.method](data.email, data.password).then(
          user => {
            self._onFulfill(msg.id, user);
          },
          reason => {
            self._onReject(msg.id, reason);
          }
        ).then(null, Components.utils.reportError);
        break;
    }
  }
};

FxAccountsMgmtService.init();
