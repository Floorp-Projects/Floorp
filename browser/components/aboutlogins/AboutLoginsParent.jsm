/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutLoginsParent"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "E10SUtils",
  "resource://gre/modules/E10SUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "LoginHelper",
  "resource://gre/modules/LoginHelper.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "MigrationUtils",
  "resource:///modules/MigrationUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "UIState",
  "resource://services-sync/UIState.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);

XPCOMUtils.defineLazyGetter(this, "log", () => {
  return LoginHelper.createLogger("AboutLoginsParent");
});
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "BREACH_ALERTS_ENABLED",
  "signon.management.page.breach-alerts.enabled",
  false
);

const ABOUT_LOGINS_ORIGIN = "about:logins";
const MASTER_PASSWORD_NOTIFICATION_ID = "master-password-login-required";
const PASSWORD_SYNC_NOTIFICATION_ID = "enable-password-sync";

// about:logins will always use the privileged content process,
// even if it is disabled for other consumers such as about:newtab.
const EXPECTED_ABOUTLOGINS_REMOTE_TYPE = E10SUtils.PRIVILEGEDABOUT_REMOTE_TYPE;

const convertSubjectToLogin = subject => {
  subject.QueryInterface(Ci.nsILoginMetaInfo).QueryInterface(Ci.nsILoginInfo);
  const login = LoginHelper.loginToVanillaObject(subject);
  if (!LoginHelper.isUserFacingLogin(login)) {
    return null;
  }
  return augmentVanillaLoginObject(login);
};

const SCHEME_REGEX = new RegExp(/^http(s)?:\/\//);
const SUBDOMAIN_REGEX = new RegExp(/^www\d*\./);
const augmentVanillaLoginObject = login => {
  let title;
  try {
    // file:// URIs don't have a host property
    title = new URL(login.origin).host || login.origin;
  } catch (ex) {
    title = login.origin.replace(SCHEME_REGEX, "");
  }
  title = title.replace(SUBDOMAIN_REGEX, "");
  return Object.assign({}, login, {
    title,
  });
};

var AboutLoginsParent = {
  _l10n: null,
  _subscribers: new WeakSet(),

  // Listeners are added in BrowserGlue.jsm
  async receiveMessage(message) {
    // Only respond to messages sent from about:logins.
    if (
      message.target.remoteType != EXPECTED_ABOUTLOGINS_REMOTE_TYPE ||
      message.target.contentPrincipal.originNoSuffix != ABOUT_LOGINS_ORIGIN
    ) {
      return;
    }

    switch (message.name) {
      case "AboutLogins:CreateLogin": {
        let newLogin = message.data.login;
        // Remove the path from the origin, if it was provided.
        let origin = LoginHelper.getLoginOrigin(newLogin.origin);
        if (!origin) {
          Cu.reportError(
            "AboutLogins:CreateLogin: Unable to get an origin from the login details."
          );
          return;
        }
        newLogin.origin = origin;
        Object.assign(newLogin, {
          formActionOrigin: "",
          usernameField: "",
          passwordField: "",
        });
        Services.logins.addLogin(LoginHelper.vanillaObjectToLogin(newLogin));
        break;
      }
      case "AboutLogins:DeleteLogin": {
        let login = LoginHelper.vanillaObjectToLogin(message.data.login);
        Services.logins.removeLogin(login);
        break;
      }
      case "AboutLogins:SyncEnable": {
        message.target.ownerGlobal.gSync.openFxAEmailFirstPage(
          "password-manager"
        );
        break;
      }
      case "AboutLogins:SyncOptions": {
        message.target.ownerGlobal.gSync.openFxAManagePage("password-manager");
        break;
      }
      case "AboutLogins:Import": {
        try {
          MigrationUtils.showMigrationWizard(message.target.ownerGlobal, [
            MigrationUtils.MIGRATION_ENTRYPOINT_PASSWORDS,
          ]);
        } catch (ex) {
          Cu.reportError(ex);
        }
        break;
      }
      case "AboutLogins:OpenFeedback": {
        const FEEDBACK_URL_PREF = "signon.management.page.feedbackURL";
        const FEEDBACK_URL = Services.urlFormatter.formatURLPref(
          FEEDBACK_URL_PREF
        );
        message.target.ownerGlobal.openWebLinkIn(FEEDBACK_URL, "tab", {
          relatedToCurrent: true,
        });
        break;
      }
      case "AboutLogins:OpenFAQ": {
        const FAQ_URL_PREF = "signon.management.page.faqURL";
        const FAQ_URL = Services.prefs.getStringPref(FAQ_URL_PREF);
        message.target.ownerGlobal.openWebLinkIn(FAQ_URL, "tab", {
          relatedToCurrent: true,
        });
        break;
      }
      case "AboutLogins:OpenMobileAndroid": {
        const MOBILE_ANDROID_URL_PREF =
          "signon.management.page.mobileAndroidURL";
        const MOBILE_ANDROID_URL = Services.prefs.getStringPref(
          MOBILE_ANDROID_URL_PREF
        );
        message.target.ownerGlobal.openWebLinkIn(MOBILE_ANDROID_URL, "tab", {
          relatedToCurrent: true,
        });
        break;
      }
      case "AboutLogins:OpenMobileIos": {
        const MOBILE_IOS_URL_PREF = "signon.management.page.mobileAppleURL";
        const MOBILE_IOS_URL = Services.prefs.getStringPref(
          MOBILE_IOS_URL_PREF
        );
        message.target.ownerGlobal.openWebLinkIn(MOBILE_IOS_URL, "tab", {
          relatedToCurrent: true,
        });
        break;
      }
      case "AboutLogins:OpenPreferences": {
        message.target.ownerGlobal.openPreferences("privacy-logins");
        break;
      }
      case "AboutLogins:OpenSite": {
        let guid = message.data.login.guid;
        let logins = LoginHelper.searchLoginsWithObject({ guid });
        if (logins.length != 1) {
          log.warn(
            `AboutLogins:OpenSite: expected to find a login for guid: ${guid} but found ${
              logins.length
            }`
          );
          return;
        }

        message.target.ownerGlobal.openWebLinkIn(logins[0].origin, "tab", {
          relatedToCurrent: true,
        });
        break;
      }
      case "AboutLogins:MasterPasswordRequest": {
        // This doesn't harm if passwords are not encrypted
        let tokendb = Cc["@mozilla.org/security/pk11tokendb;1"].createInstance(
          Ci.nsIPK11TokenDB
        );
        let token = tokendb.getInternalKeyToken();

        let messageManager = message.target.messageManager;

        // If there is no master password, return as-if authentication succeeded.
        if (token.checkPassword("")) {
          messageManager.sendAsyncMessage(
            "AboutLogins:MasterPasswordResponse",
            true
          );
          return;
        }

        // If a master password prompt is already open, just exit early and return false.
        // The user can re-trigger it after responding to the already open dialog.
        if (Services.logins.uiBusy) {
          messageManager.sendAsyncMessage(
            "AboutLogins:MasterPasswordResponse",
            false
          );
          return;
        }

        // So there's a master password. But since checkPassword didn't succeed, we're logged out (per nsIPK11Token.idl).
        try {
          // Relogin and ask for the master password.
          token.login(true); // 'true' means always prompt for token password. User will be prompted until
          // clicking 'Cancel' or entering the correct password.
        } catch (e) {
          // An exception will be thrown if the user cancels the login prompt dialog.
          // User is also logged out of Software Security Device.
        }

        messageManager.sendAsyncMessage(
          "AboutLogins:MasterPasswordResponse",
          token.isLoggedIn()
        );
        break;
      }
      case "AboutLogins:Subscribe": {
        if (
          !ChromeUtils.nondeterministicGetWeakSetKeys(this._subscribers).length
        ) {
          Services.obs.addObserver(this, "passwordmgr-crypto-login");
          Services.obs.addObserver(this, "passwordmgr-crypto-loginCanceled");
          Services.obs.addObserver(this, "passwordmgr-storage-changed");
          Services.obs.addObserver(this, UIState.ON_UPDATE);
        }
        this._subscribers.add(message.target);

        let messageManager = message.target.messageManager;

        const logins = await this.getAllLogins();
        try {
          messageManager.sendAsyncMessage("AboutLogins:AllLogins", logins);

          let syncState = this.getSyncState();
          messageManager.sendAsyncMessage("AboutLogins:SyncState", syncState);
          this.updatePasswordSyncNotificationState();

          if (BREACH_ALERTS_ENABLED) {
            const breachesByLoginGUID = await LoginHelper.getBreachesForLogins(
              logins
            );
            messageManager.sendAsyncMessage(
              "AboutLogins:UpdateBreaches",
              breachesByLoginGUID
            );
          }

          messageManager.sendAsyncMessage(
            "AboutLogins:SendFavicons",
            await this.getAllFavicons(logins)
          );
        } catch (ex) {
          if (ex.result != Cr.NS_ERROR_NOT_INITIALIZED) {
            throw ex;
          }

          // The message manager may be destroyed before the replies can be sent.
          log.debug(
            "AboutLogins:Subscribe: exception when replying with logins",
            ex
          );
        }

        break;
      }
      case "AboutLogins:UpdateLogin": {
        let loginUpdates = message.data.login;
        let logins = LoginHelper.searchLoginsWithObject({
          guid: loginUpdates.guid,
        });
        if (logins.length != 1) {
          log.warn(
            `AboutLogins:UpdateLogin: expected to find a login for guid: ${
              loginUpdates.guid
            } but found ${logins.length}`
          );
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

  async observe(subject, topic, type) {
    if (!ChromeUtils.nondeterministicGetWeakSetKeys(this._subscribers).length) {
      Services.obs.removeObserver(this, "passwordmgr-crypto-login");
      Services.obs.removeObserver(this, "passwordmgr-crypto-loginCanceled");
      Services.obs.removeObserver(this, "passwordmgr-storage-changed");
      Services.obs.removeObserver(this, UIState.ON_UPDATE);
      return;
    }

    if (topic == "passwordmgr-crypto-login") {
      this.removeNotifications(MASTER_PASSWORD_NOTIFICATION_ID);
      this.messageSubscribers(
        "AboutLogins:AllLogins",
        await this.getAllLogins()
      );
      return;
    }

    if (topic == "passwordmgr-crypto-loginCanceled") {
      this.showMasterPasswordLoginNotifications();
      return;
    }

    if (topic == UIState.ON_UPDATE) {
      this.messageSubscribers("AboutLogins:SyncState", this.getSyncState());
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
        break;
      }
    }
  },

  async getFavicon(login) {
    try {
      const faviconData = await PlacesUtils.promiseFaviconData(login.hostname);
      return {
        faviconData,
        guid: login.guid,
      };
    } catch (ex) {
      return null;
    }
  },

  async getAllFavicons(logins) {
    let favicons = await Promise.all(
      logins.map(login => this.getFavicon(login))
    );
    let vanillaFavicons = {};
    for (let favicon of favicons) {
      if (!favicon) {
        continue;
      }
      try {
        vanillaFavicons[favicon.guid] = {
          data: favicon.faviconData.data,
          dataLen: favicon.faviconData.dataLen,
          mimeType: favicon.faviconData.mimeType,
        };
      } catch (ex) {}
    }
    return vanillaFavicons;
  },

  showMasterPasswordLoginNotifications() {
    this.showNotifications({
      id: MASTER_PASSWORD_NOTIFICATION_ID,
      priority: "PRIORITY_WARNING_MEDIUM",
      iconURL: "chrome://browser/skin/login.svg",
      messageId: "master-password-notification-message",
      buttonId: "master-password-reload-button",
      onClick(browser) {
        browser.reload();
      },
    });
  },

  showPasswordSyncNotifications() {
    this.showNotifications({
      id: PASSWORD_SYNC_NOTIFICATION_ID,
      priority: "PRIORITY_INFO_MEDIUM",
      iconURL: "chrome://browser/skin/login.svg",
      messageId: "enable-password-sync-notification-message",
      buttonId: "enable-password-sync-preferences-button",
      onClick(browser) {
        browser.ownerGlobal.gSync.openPrefs("password-manager");
      },
      extraFtl: ["branding/brand.ftl", "browser/branding/sync-brand.ftl"],
    });
  },

  showNotifications({
    id,
    priority,
    iconURL,
    messageId,
    buttonId,
    onClick,
    extraFtl = [],
  } = {}) {
    for (let subscriber of this._subscriberIterator()) {
      let MozXULElement = subscriber.ownerGlobal.MozXULElement;
      MozXULElement.insertFTLIfNeeded("browser/aboutLogins.ftl");
      for (let ftl of extraFtl) {
        MozXULElement.insertFTLIfNeeded(ftl);
      }

      // If there's already an existing notification bar, don't do anything.
      let { gBrowser } = subscriber.ownerGlobal;
      let browser = subscriber;
      let notificationBox = gBrowser.getNotificationBox(browser);
      let notification = notificationBox.getNotificationWithValue(id);
      if (notification) {
        continue;
      }

      // Configure the notification bar
      let doc = subscriber.ownerDocument;
      let messageFragment = doc.createDocumentFragment();
      let message = doc.createElement("span");
      doc.l10n.setAttributes(message, messageId);
      messageFragment.appendChild(message);

      let buttons = [
        {
          "l10n-id": buttonId,
          popup: null,
          callback: () => {
            onClick(browser);
          },
        },
      ];

      notification = notificationBox.appendNotification(
        messageFragment,
        id,
        iconURL,
        notificationBox[priority],
        buttons
      );
    }
  },

  removeNotifications(notificationId) {
    for (let subscriber of this._subscriberIterator()) {
      let { gBrowser } = subscriber.ownerGlobal;
      let browser = subscriber;
      let notificationBox = gBrowser.getNotificationBox(browser);
      let notification = notificationBox.getNotificationWithValue(
        notificationId
      );
      if (!notification) {
        continue;
      }
      notificationBox.removeNotification(notification);
    }
  },

  *_subscriberIterator() {
    let subscribers = ChromeUtils.nondeterministicGetWeakSetKeys(
      this._subscribers
    );
    for (let subscriber of subscribers) {
      if (
        subscriber.remoteType != EXPECTED_ABOUTLOGINS_REMOTE_TYPE ||
        !subscriber.contentPrincipal ||
        subscriber.contentPrincipal.originNoSuffix != ABOUT_LOGINS_ORIGIN
      ) {
        this._subscribers.delete(subscriber);
        continue;
      }
      yield subscriber;
    }
  },

  messageSubscribers(name, details) {
    for (let subscriber of this._subscriberIterator()) {
      try {
        subscriber.messageManager.sendAsyncMessage(name, details);
      } catch (ex) {
        if (ex.result != Cr.NS_ERROR_NOT_INITIALIZED) {
          throw ex;
        }

        // The message manager may be destroyed before the message is sent.
        log.debug(
          "messageSubscribers: exception when calling sendAsyncMessage",
          ex
        );
      }
    }
  },

  async getAllLogins() {
    try {
      let logins = await LoginHelper.getAllUserFacingLogins();
      return logins
        .map(LoginHelper.loginToVanillaObject)
        .map(augmentVanillaLoginObject);
    } catch (e) {
      if (e.result == Cr.NS_ERROR_ABORT) {
        // If the user cancels the MP prompt then return no logins.
        return [];
      }
      throw e;
    }
  },

  getSyncState() {
    const state = UIState.get();
    // As long as Sync is configured, about:logins will treat it as
    // authenticated. More diagnostics and error states can be handled
    // by other more Sync-specific pages.
    const loggedIn = state.status != UIState.STATUS_NOT_CONFIGURED;
    return {
      loggedIn,
      email: state.email,
      avatarURL: state.avatarURL,
    };
  },

  updatePasswordSyncNotificationState() {
    const state = this.getSyncState();
    // Need to explicitly call the getter on lazy preference getters
    // to activate their observer.
    let passwordSyncEnabled = PASSWORD_SYNC_ENABLED;
    if (state.loggedIn && !passwordSyncEnabled) {
      this.showPasswordSyncNotifications();
      return;
    }
    this.removeNotifications(PASSWORD_SYNC_NOTIFICATION_ID);
  },
};

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "PASSWORD_SYNC_ENABLED",
  "services.sync.engine.passwords",
  false,
  AboutLoginsParent.updatePasswordSyncNotificationState.bind(AboutLoginsParent)
);
