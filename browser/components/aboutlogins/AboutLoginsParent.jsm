/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutLoginsParent"];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.defineModuleGetter(this, "E10SUtils",
                               "resource://gre/modules/E10SUtils.jsm");
ChromeUtils.defineModuleGetter(this, "LoginHelper",
                               "resource://gre/modules/LoginHelper.jsm");
ChromeUtils.defineModuleGetter(this, "Services",
                               "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "log", () => {
  return LoginHelper.createLogger("AboutLoginsParent");
});

const ABOUT_LOGINS_ORIGIN = "about:logins";

const PRIVILEGED_PROCESS_PREF =
  "browser.tabs.remote.separatePrivilegedContentProcess";
const PRIVILEGED_PROCESS_ENABLED =
  Services.prefs.getBoolPref(PRIVILEGED_PROCESS_PREF, false);

// When the privileged content process is enabled, we expect about:logins
// to load in it. Otherwise, it's in a normal web content process.
const EXPECTED_ABOUTLOGINS_REMOTE_TYPE =
  PRIVILEGED_PROCESS_ENABLED ? E10SUtils.PRIVILEGED_REMOTE_TYPE
                             : E10SUtils.DEFAULT_REMOTE_TYPE;

const isValidLogin = login => {
  return !(login.hostname || "").startsWith("chrome://");
};

const convertSubjectToLogin = subject => {
    subject.QueryInterface(Ci.nsILoginMetaInfo).QueryInterface(Ci.nsILoginInfo);
    const login = LoginHelper.loginToVanillaObject(subject);
    if (!isValidLogin(login)) {
      return null;
    }
    return login;
};

var AboutLoginsParent = {
  _subscribers: new WeakSet(),

  // Listeners are added in BrowserGlue.jsm
  receiveMessage(message) {
    // Only respond to messages sent from about:logins.
    if (message.target.remoteType != EXPECTED_ABOUTLOGINS_REMOTE_TYPE ||
        message.target.contentPrincipal.originNoSuffix != ABOUT_LOGINS_ORIGIN) {
      return;
    }

    switch (message.name) {
      case "AboutLogins:DeleteLogin": {
        let login = LoginHelper.vanillaObjectToLogin(message.data.login);
        Services.logins.removeLogin(login);
        break;
      }
      case "AboutLogins:OpenSite": {
        let guid = message.data.login.guid;
        let logins = LoginHelper.searchLoginsWithObject({guid});
        if (!logins || logins.length != 1) {
          log.warn(`AboutLogins:OpenSite: expected to find a login for guid: ${guid} but found ${(logins || []).length}`);
          return;
        }

        message.target.ownerGlobal.openWebLinkIn(logins[0].hostname, "tab", {relatedToCurrent: true});
        break;
      }
      case "AboutLogins:Subscribe": {
        if (!ChromeUtils.nondeterministicGetWeakSetKeys(this._subscribers).length) {
          Services.obs.addObserver(this, "passwordmgr-storage-changed");
        }
        this._subscribers.add(message.target);

        let messageManager = message.target.messageManager;
        messageManager.sendAsyncMessage("AboutLogins:AllLogins", this.getAllLogins());
        break;
      }
      case "AboutLogins:UpdateLogin": {
        let loginUpdates = message.data.login;
        let logins = LoginHelper.searchLoginsWithObject({guid: loginUpdates.guid});
        if (!logins || logins.length != 1) {
          log.warn(`AboutLogins:UpdateLogin: expected to find a login for guid: ${loginUpdates.guid} but found ${(logins || []).length}`);
          return;
        }

        let modifiedLogin = logins[0].clone();
        if (loginUpdates.hasOwnProperty("username")) {
          modifiedLogin.username = loginUpdates.username;
        }
        if (loginUpdates.hasOwnProperty("password")) {
          modifiedLogin.password = loginUpdates.password;
        }

        Services.logins.modifyLogin(logins[0], modifiedLogin);
        break;
      }
    }
  },

  observe(subject, topic, type) {
    if (!ChromeUtils.nondeterministicGetWeakSetKeys(this._subscribers).length) {
      Services.obs.removeObserver(this, "passwordmgr-storage-changed");
      return;
    }

    switch (type) {
      case "addLogin": {
        const login = convertSubjectToLogin(subject);
        if (!login) {
          return;
        }
        this.messageSubscribers("AboutLogins:LoginAdded", login);
        break;
      }
      case "modifyLogin": {
        subject.QueryInterface(Ci.nsIArrayExtensions);
        const login = convertSubjectToLogin(subject.GetElementAt(1));
        if (!login) {
          return;
        }
        this.messageSubscribers("AboutLogins:LoginModified", login);
        break;
      }
      case "removeLogin": {
        const login = convertSubjectToLogin(subject);
        if (!login) {
          return;
        }
        this.messageSubscribers("AboutLogins:LoginRemoved", login);
      }
      default: {
        break;
      }
    }
  },

  messageSubscribers(name, details) {
    let subscribers = ChromeUtils.nondeterministicGetWeakSetKeys(this._subscribers);
    for (let subscriber of subscribers) {
      if (subscriber.remoteType != EXPECTED_ABOUTLOGINS_REMOTE_TYPE ||
          !subscriber.contentPrincipal ||
          subscriber.contentPrincipal.originNoSuffix != ABOUT_LOGINS_ORIGIN) {
        this._subscribers.delete(subscriber);
        continue;
      }
      try {
        subscriber.messageManager.sendAsyncMessage(name, details);
      } catch (ex) {}
    }
  },

  getAllLogins() {
    return Services.logins
                   .getAllLogins()
                   .filter(isValidLogin)
                   .map(LoginHelper.loginToVanillaObject);
  },
};
