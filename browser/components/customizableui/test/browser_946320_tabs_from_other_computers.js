/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Preferences = Cu.import("resource://gre/modules/Preferences.jsm", {}).Preferences;

const {FxAccounts, AccountState} = Cu.import("resource://gre/modules/FxAccounts.jsm", {});

// FxA logs can be gotten at via this pref which helps debugging.
Preferences.set("services.sync.log.appender.dump", "Debug");

add_task(function() {
  yield PanelUI.show({type: "command"});

  let historyButton = document.getElementById("history-panelmenu");
  let historySubview = document.getElementById("PanelUI-history");
  let subviewShownPromise = subviewShown(historySubview);
  historyButton.click();
  yield subviewShownPromise;

  let tabsFromOtherComputers = document.getElementById("sync-tabs-menuitem2");
  is(tabsFromOtherComputers.hidden, true, "The Tabs From Other Computers menuitem should be hidden when sync isn't enabled.");

  let hiddenPanelPromise = promisePanelHidden(window);
  PanelUI.hide();
  yield hiddenPanelPromise;

  // Part 2 - When Sync is enabled the menuitem should be shown.
  yield configureIdentity();
  yield PanelUI.show({type: "command"});

  subviewShownPromise = subviewShown(historySubview);
  historyButton.click();
  yield subviewShownPromise;

  is(tabsFromOtherComputers.hidden, false, "The Tabs From Other Computers menuitem should be shown when sync is enabled.");

  let syncPrefBranch = new Preferences("services.sync.");
  syncPrefBranch.resetBranch("");
  Services.logins.removeAllLogins();

  hiddenPanelPromise = promisePanelHidden(window);
  PanelUI.toggle({type: "command"});
  yield hiddenPanelPromise;

  yield fxAccounts.signOut(/*localOnly = */true);
});

function configureIdentity() {
  // do the FxAccounts thang and wait for Sync to initialize the identity.
  configureFxAccountIdentity();
  return Weave.Service.identity.initializeWithCurrentIdentity().then(() => {
    // need to wait until this identity manager is readyToAuthenticate.
    return Weave.Service.identity.whenReadyToAuthenticate.promise;
  });
}

// Configure an instance of an FxAccount identity provider.
function configureFxAccountIdentity() {
  // A mock "storage manager" for FxAccounts that doesn't actually write anywhere.
  function MockFxaStorageManager() {
  }

  MockFxaStorageManager.prototype = {
    promiseInitialized: Promise.resolve(),

    initialize(accountData) {
      this.accountData = accountData;
    },

    finalize() {
      return Promise.resolve();
    },

    getAccountData() {
      return Promise.resolve(this.accountData);
    },

    updateAccountData(updatedFields) {
      for (let [name, value] of Iterator(updatedFields)) {
        if (value == null) {
          delete this.accountData[name];
        } else {
          this.accountData[name] = value;
        }
      }
      return Promise.resolve();
    },

    deleteAccountData() {
      this.accountData = null;
      return Promise.resolve();
    }
  }

  let user = {
    assertion: "assertion",
    email: "email",
    kA: "kA",
    kB: "kB",
    sessionToken: "sessionToken",
    uid: "user_uid",
    verified: true,
  };

  let token = {
    endpoint: null,
    duration: 300,
    id: "id",
    key: "key",
    // uid will be set to the username.
  };

  let MockInternal = {
    newAccountState(credentials) {
      isnot(credentials, "not expecting credentials");
      let storageManager = new MockFxaStorageManager();
      // and init storage with our user.
      storageManager.initialize(user);
      return new AccountState(this, storageManager);
    },
    getCertificate(data, keyPair, mustBeValidUntil) {
      this.cert = {
        validUntil: this.now() + 10000,
        cert: "certificate",
      };
      return Promise.resolve(this.cert.cert);
    },
    getCertificateSigned() {
      return Promise.resolve();
    },
  };
  let mockTSC = { // TokenServerClient
    getTokenFromBrowserIDAssertion: function(uri, assertion, cb) {
      token.uid = "username";
      cb(null, token);
    },
  };

  let fxa = new FxAccounts(MockInternal);
  Weave.Service.identity._fxaService = fxa;
  Weave.Service.identity._tokenServerClient = mockTSC;
  // Set the "account" of the browserId manager to be the "email" of the
  // logged in user of the mockFXA service.
  Weave.Service.identity._account = user.email;
}
