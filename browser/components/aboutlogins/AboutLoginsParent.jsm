/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutLoginsParent"];

ChromeUtils.defineModuleGetter(this, "LoginHelper",
                               "resource://gre/modules/LoginHelper.jsm");
ChromeUtils.defineModuleGetter(this, "Services",
                               "resource://gre/modules/Services.jsm");

const ABOUT_LOGINS_ORIGIN = "about:logins";

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
    if (message.target.contentPrincipal.originNoSuffix != ABOUT_LOGINS_ORIGIN) {
      return;
    }

    switch (message.name) {
      case "AboutLogins:DeleteLogin": {
        let login = LoginHelper.vanillaObjectToLogin(message.data.login);
        Services.logins.removeLogin(login);
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
      if (subscriber.contentPrincipal.originNoSuffix != ABOUT_LOGINS_ORIGIN) {
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
