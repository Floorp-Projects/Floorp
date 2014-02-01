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

this.FxAccountsMgmtService = {

  _sendChromeEvent: function(aEventName, aMsg) {
    if (!this._shell) {
      return;
    }
    log.debug("Chrome event " + JSON.stringify(aMsg));
    this._shell.sendCustomEvent(aEventName, aMsg);
  },

  _onFullfill: function(aMsgId, aData) {
    this._sendChromeEvent("mozFxAccountsChromeEvent", {
      id: aMsgId,
      data: aData ? aData : null
    });
  },

  _onReject: function(aMsgId, aReason) {
    this._sendChromeEvent("mozFxAccountsChromeEvent", {
      id: aMsgId,
      error: aReason ? aReason : null
    });
  },

  init: function() {
    Services.obs.addObserver(this, "content-start", false);
    Services.obs.addObserver(this, ONLOGIN_NOTIFICATION, false);
    Services.obs.addObserver(this, ONVERIFIED_NOTIFICATION, false);
    Services.obs.addObserver(this, ONLOGOUT_NOTIFICATION, false);
  },

  observe: function(aSubject, aTopic, aData) {
    log.debug("Observed " + aTopic);
    switch (aTopic) {
      case "content-start":
        this._shell = Services.wm.getMostRecentWindow("navigator:browser").shell;
        let content = this._shell.contentBrowser.contentWindow;
        content.addEventListener("mozFxAccountsContentEvent",
                                 FxAccountsMgmtService);
        Services.obs.removeObserver(this, "content-start");
        break;
      case ONLOGIN_NOTIFICATION:
      case ONVERIFIED_NOTIFICATION:
      case ONLOGOUT_NOTIFICATION:
        // FxAccounts notifications have the form of fxaccounts:*
        this._sendChromeEvent("mozFxAccountsUnsolChromeEvent", {
          eventName: aTopic.substring(aTopic.indexOf(":") + 1)
        });
        break;
    }
  },

  handleEvent: function(aEvent) {
    let msg = aEvent.detail;
    log.debug("Got content msg " + JSON.stringify(msg));
    let self = FxAccountsMgmtService;

    if (!msg.id) {
      return;
    }

    let data = msg.data;
    if (!data) {
      return;
    }

    switch(data.method) {
      case "getAccounts":
        FxAccountsManager.getAccount().then(
          account => {
            // We only expose the email and verification status so far.
            self._onFullfill(msg.id, account);
          },
          reason => {
            self._onReject(msg.id, reason);
          }
        ).then(null, Components.utils.reportError);
        break;
      case "logout":
        FxAccountsManager.signOut().then(
          () => {
            self._onFullfill(msg.id);
          },
          reason => {
            self._onReject(msg.id, reason);
          }
        ).then(null, Components.utils.reportError);
        break;
      case "queryAccount":
        FxAccountsManager.queryAccount(data.accountId).then(
          result => {
            self._onFullfill(msg.id, result);
          },
          reason => {
            self._onReject(msg.id, reason);
          }
        ).then(null, Components.utils.reportError);
        break;
      case "signIn":
      case "signUp":
        FxAccountsManager[data.method](data.accountId, data.password).then(
          user => {
            self._onFullfill(msg.id, user);
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
