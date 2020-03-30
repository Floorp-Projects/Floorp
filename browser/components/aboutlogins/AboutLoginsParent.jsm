/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// _AboutLogins is only exported for testing
var EXPORTED_SYMBOLS = ["AboutLoginsParent", "_AboutLogins"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
  LoginBreaches: "resource:///modules/LoginBreaches.jsm",
  LoginHelper: "resource://gre/modules/LoginHelper.jsm",
  MigrationUtils: "resource:///modules/MigrationUtils.jsm",
  OSKeyStore: "resource://gre/modules/OSKeyStore.jsm",
  Services: "resource://gre/modules/Services.jsm",
  UIState: "resource://services-sync/UIState.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "log", () => {
  return LoginHelper.createLogger("AboutLoginsParent");
});
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "BREACH_ALERTS_ENABLED",
  "signon.management.page.breach-alerts.enabled",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "VULNERABLE_PASSWORDS_ENABLED",
  "signon.management.page.vulnerable-passwords.enabled",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "FXA_ENABLED",
  "identity.fxaccounts.enabled",
  false
);
XPCOMUtils.defineLazyGetter(this, "AboutLoginsL10n", () => {
  return new Localization(["branding/brand.ftl", "browser/aboutLogins.ftl"]);
});

const ABOUT_LOGINS_ORIGIN = "about:logins";
const MASTER_PASSWORD_NOTIFICATION_ID = "master-password-login-required";
const PASSWORD_SYNC_NOTIFICATION_ID = "enable-password-sync";

const HIDE_MOBILE_FOOTER_PREF = "signon.management.page.hideMobileFooter";
const SHOW_PASSWORD_SYNC_NOTIFICATION_PREF =
  "signon.management.page.showPasswordSyncNotification";

// about:logins will always use the privileged content process,
// even if it is disabled for other consumers such as about:newtab.
const EXPECTED_ABOUTLOGINS_REMOTE_TYPE = E10SUtils.PRIVILEGEDABOUT_REMOTE_TYPE;

// App store badges sourced from https://developer.apple.com/app-store/marketing/guidelines/#section-badges.
// This array mirrors the file names from the App store directory (./content/third-party/app-store)
const APP_STORE_LOCALES = [
  "az",
  "ar",
  "bg",
  "cs",
  "da",
  "de",
  "el",
  "en",
  "es-mx",
  "es",
  "et",
  "fi",
  "fr",
  "he",
  "hu",
  "id",
  "it",
  "ja",
  "ko",
  "lt",
  "lv",
  "my",
  "nb",
  "nl",
  "nn",
  "pl",
  "pt-br",
  "pt-pt",
  "ro",
  "ru",
  "si",
  "sk",
  "sv",
  "th",
  "tl",
  "tr",
  "vi",
  "zh-hans",
  "zh-hant",
];

// Google play badges sourced from https://play.google.com/intl/en_us/badges/
// This array mirrors the file names from the play store directory (./content/third-party/play-store)
const PLAY_STORE_LOCALES = [
  "af",
  "ar",
  "az",
  "be",
  "bg",
  "bn",
  "bs",
  "ca",
  "cs",
  "da",
  "de",
  "el",
  "en",
  "es",
  "et",
  "eu",
  "fa",
  "fr",
  "gl",
  "gu",
  "he",
  "hi",
  "hr",
  "hu",
  "hy",
  "id",
  "is",
  "it",
  "ja",
  "ka",
  "kk",
  "km",
  "kn",
  "ko",
  "lo",
  "lt",
  "lv",
  "mk",
  "mr",
  "ms",
  "my",
  "nb",
  "ne",
  "nl",
  "nn",
  "pa",
  "pl",
  "pt-br",
  "pt",
  "ro",
  "ru",
  "si",
  "sk",
  "sl",
  "sq",
  "sr",
  "sv",
  "ta",
  "te",
  "th",
  "tl",
  "tr",
  "uk",
  "ur",
  "uz",
  "vi",
  "zh-cn",
  "zh-tw",
];

const convertSubjectToLogin = subject => {
  subject.QueryInterface(Ci.nsILoginMetaInfo).QueryInterface(Ci.nsILoginInfo);
  const login = LoginHelper.loginToVanillaObject(subject);
  if (!LoginHelper.isUserFacingLogin(login)) {
    return null;
  }
  return augmentVanillaLoginObject(login);
};

const SUBDOMAIN_REGEX = new RegExp(/^www\d*\./);
const augmentVanillaLoginObject = login => {
  // Note that `displayOrigin` can also include a httpRealm.
  let title = login.displayOrigin.replace(SUBDOMAIN_REGEX, "");
  return Object.assign({}, login, {
    title,
  });
};

class AboutLoginsParent extends JSWindowActorParent {
  async receiveMessage(message) {
    if (!this.browsingContext.embedderElement) {
      return;
    }
    // Only respond to messages sent from a privlegedabout process. Ideally
    // we would also check the contentPrincipal.originNoSuffix but this
    // check has been removed due to bug 1576722.
    if (
      this.browsingContext.embedderElement.remoteType !=
      EXPECTED_ABOUTLOGINS_REMOTE_TYPE
    ) {
      throw new Error(
        `AboutLoginsParent: Received ${message.name} message the remote type didn't match expectations: ${this.browsingContext.embedderElement.remoteType} == ${EXPECTED_ABOUTLOGINS_REMOTE_TYPE}`
      );
    }

    AboutLogins._subscribers.add(this.browsingContext);

    let ownerGlobal = this.browsingContext.embedderElement.ownerGlobal;
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
        newLogin = LoginHelper.vanillaObjectToLogin(newLogin);
        try {
          Services.logins.addLogin(newLogin);
        } catch (error) {
          this.handleLoginStorageErrors(newLogin, error, message);
        }
        break;
      }
      case "AboutLogins:DeleteLogin": {
        let login = LoginHelper.vanillaObjectToLogin(message.data.login);
        Services.logins.removeLogin(login);
        break;
      }
      case "AboutLogins:HideFooter": {
        Services.prefs.setBoolPref(HIDE_MOBILE_FOOTER_PREF, true);
        break;
      }
      case "AboutLogins:SortChanged": {
        Services.prefs.setCharPref("signon.management.page.sort", message.data);
        break;
      }
      case "AboutLogins:SyncEnable": {
        ownerGlobal.gSync.openFxAEmailFirstPage("password-manager");
        break;
      }
      case "AboutLogins:SyncOptions": {
        ownerGlobal.gSync.openFxAManagePage("password-manager");
        break;
      }
      case "AboutLogins:Import": {
        try {
          MigrationUtils.showMigrationWizard(ownerGlobal, [
            MigrationUtils.MIGRATION_ENTRYPOINT_PASSWORDS,
          ]);
        } catch (ex) {
          Cu.reportError(ex);
        }
        break;
      }
      case "AboutLogins:GetHelp": {
        const SUPPORT_URL =
          Services.urlFormatter.formatURLPref("app.support.baseURL") +
          "firefox-lockwise";
        ownerGlobal.openWebLinkIn(SUPPORT_URL, "tab", {
          relatedToCurrent: true,
        });
        break;
      }
      case "AboutLogins:OpenMobileAndroid": {
        const MOBILE_ANDROID_URL_PREF =
          "signon.management.page.mobileAndroidURL";
        const linkTrackingSource = message.data.source;
        let MOBILE_ANDROID_URL = Services.prefs.getStringPref(
          MOBILE_ANDROID_URL_PREF
        );
        // Append the `utm_creative` query parameter value:
        MOBILE_ANDROID_URL += linkTrackingSource;
        ownerGlobal.openWebLinkIn(MOBILE_ANDROID_URL, "tab", {
          relatedToCurrent: true,
        });
        break;
      }
      case "AboutLogins:OpenMobileIos": {
        const MOBILE_IOS_URL_PREF = "signon.management.page.mobileAppleURL";
        const linkTrackingSource = message.data.source;
        let MOBILE_IOS_URL = Services.prefs.getStringPref(MOBILE_IOS_URL_PREF);
        // Append the `utm_creative` query parameter value:
        MOBILE_IOS_URL += linkTrackingSource;
        ownerGlobal.openWebLinkIn(MOBILE_IOS_URL, "tab", {
          relatedToCurrent: true,
        });
        break;
      }
      case "AboutLogins:OpenPreferences": {
        ownerGlobal.openPreferences("privacy-logins");
        break;
      }
      case "AboutLogins:MasterPasswordRequest": {
        let messageId = message.data;
        if (!messageId) {
          throw new Error(
            "AboutLogins:MasterPasswordRequest: Message ID required for MasterPasswordRequest."
          );
        }

        if (Date.now() < AboutLogins._authExpirationTime) {
          this.sendAsyncMessage("AboutLogins:MasterPasswordResponse", true);
          return;
        }

        // This does no harm if master password isn't set.
        let tokendb = Cc["@mozilla.org/security/pk11tokendb;1"].createInstance(
          Ci.nsIPK11TokenDB
        );
        let token = tokendb.getInternalKeyToken();

        let loggedIn = false;
        // Use the OS auth dialog if there is no master password
        if (token.checkPassword("")) {
          if (AppConstants.platform == "macosx") {
            // OS Auth dialogs on macOS must only provide the "reason" that the prompt
            // is being displayed.
            messageId += "-macosx";
          }
          let [messageText, captionText] = await AboutLoginsL10n.formatMessages(
            [
              {
                id: messageId,
              },
              {
                id: "about-logins-os-auth-dialog-caption",
              },
            ]
          );
          loggedIn = await OSKeyStore.ensureLoggedIn(
            messageText.value,
            captionText.value,
            ownerGlobal,
            false
          );
        } else {
          // If a master password prompt is already open, just exit early and return false.
          // The user can re-trigger it after responding to the already open dialog.
          if (Services.logins.uiBusy) {
            this.sendAsyncMessage("AboutLogins:MasterPasswordResponse", false);
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
          loggedIn = token.isLoggedIn();
        }

        if (loggedIn) {
          const AUTH_TIMEOUT_MS = 5 * 60 * 1000; // 5 minutes
          AboutLogins._authExpirationTime = Date.now() + AUTH_TIMEOUT_MS;
        }
        this.sendAsyncMessage("AboutLogins:MasterPasswordResponse", loggedIn);
        break;
      }
      case "AboutLogins:Subscribe": {
        AboutLogins._authExpirationTime = Number.NEGATIVE_INFINITY;
        if (!AboutLogins._observersAdded) {
          Services.obs.addObserver(AboutLogins, "passwordmgr-crypto-login");
          Services.obs.addObserver(
            AboutLogins,
            "passwordmgr-crypto-loginCanceled"
          );
          Services.obs.addObserver(AboutLogins, "passwordmgr-storage-changed");
          Services.obs.addObserver(AboutLogins, UIState.ON_UPDATE);
          AboutLogins._observersAdded = true;
        }

        const logins = await AboutLogins.getAllLogins();
        try {
          let syncState = AboutLogins.getSyncState();
          if (FXA_ENABLED) {
            AboutLogins.updatePasswordSyncNotificationState(syncState);
          }

          const playStoreBadgeLanguage = Services.locale.negotiateLanguages(
            Services.locale.appLocalesAsBCP47,
            PLAY_STORE_LOCALES,
            "en-us",
            Services.locale.langNegStrategyLookup
          )[0];

          const appStoreBadgeLanguage = Services.locale.negotiateLanguages(
            Services.locale.appLocalesAsBCP47,
            APP_STORE_LOCALES,
            "en-us",
            Services.locale.langNegStrategyLookup
          )[0];

          const selectedBadgeLanguages = {
            appStoreBadgeLanguage,
            playStoreBadgeLanguage,
          };

          let selectedSort = Services.prefs.getCharPref(
            "signon.management.page.sort",
            "name"
          );
          if (selectedSort == "breached") {
            // The "breached" value was used since Firefox 70 and
            // replaced with "alerts" in Firefox 76.
            selectedSort = "alerts";
          }
          this.sendAsyncMessage("AboutLogins:Setup", {
            logins,
            selectedSort,
            syncState,
            selectedBadgeLanguages,
            masterPasswordEnabled: LoginHelper.isMasterPasswordSet(),
            passwordRevealVisible: Services.policies.isAllowed(
              "passwordReveal"
            ),
            importVisible:
              Services.policies.isAllowed("profileImport") &&
              AppConstants.platform != "linux",
          });

          await AboutLogins._sendAllLoginRelatedObjects(
            logins,
            this.browsingContext
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
            `AboutLogins:UpdateLogin: expected to find a login for guid: ${loginUpdates.guid} but found ${logins.length}`
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
        try {
          Services.logins.modifyLogin(logins[0], modifiedLogin);
        } catch (error) {
          this.handleLoginStorageErrors(modifiedLogin, error, message);
        }
        break;
      }
    }
  }

  handleLoginStorageErrors(login, error) {
    let messageObject = {
      login: augmentVanillaLoginObject(LoginHelper.loginToVanillaObject(login)),
      errorMessage: error.message,
    };

    if (error.message.includes("This login already exists")) {
      // See comment in LoginHelper.createLoginAlreadyExistsError as to
      // why we need to call .toString() on the nsISupportsString.
      messageObject.existingLoginGuid = error.data.toString();
    }

    this.sendAsyncMessage("AboutLogins:ShowLoginItemError", messageObject);
  }
}

var AboutLogins = {
  _subscribers: new WeakSet(),
  _observersAdded: false,
  _authExpirationTime: Number.NEGATIVE_INFINITY,

  async observe(subject, topic, type) {
    if (!ChromeUtils.nondeterministicGetWeakSetKeys(this._subscribers).length) {
      Services.obs.removeObserver(this, "passwordmgr-crypto-login");
      Services.obs.removeObserver(this, "passwordmgr-crypto-loginCanceled");
      Services.obs.removeObserver(this, "passwordmgr-storage-changed");
      Services.obs.removeObserver(this, UIState.ON_UPDATE);
      this._observersAdded = false;
      return;
    }

    if (topic == "passwordmgr-crypto-login") {
      this.removeNotifications(MASTER_PASSWORD_NOTIFICATION_ID);
      let logins = await this.getAllLogins();
      this.messageSubscribers("AboutLogins:AllLogins", logins);
      await this._sendAllLoginRelatedObjects(logins);
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

        if (BREACH_ALERTS_ENABLED) {
          this.messageSubscribers(
            "AboutLogins:UpdateBreaches",
            await LoginBreaches.getPotentialBreachesByLoginGUID([login])
          );
          if (VULNERABLE_PASSWORDS_ENABLED) {
            this.messageSubscribers(
              "AboutLogins:UpdateVulnerableLogins",
              await LoginBreaches.getPotentiallyVulnerablePasswordsByLoginGUID([
                login,
              ])
            );
          }
        }
        break;
      }
      case "modifyLogin": {
        subject.QueryInterface(Ci.nsIArrayExtensions);
        const login = convertSubjectToLogin(subject.GetElementAt(1));
        if (!login) {
          return;
        }

        if (BREACH_ALERTS_ENABLED) {
          let breachesForThisLogin = await LoginBreaches.getPotentialBreachesByLoginGUID(
            [login]
          );
          let breachData = breachesForThisLogin.size
            ? breachesForThisLogin.get(login.guid)
            : false;
          this.messageSubscribers(
            "AboutLogins:UpdateBreaches",
            new Map([[login.guid, breachData]])
          );
          if (VULNERABLE_PASSWORDS_ENABLED) {
            let vulnerablePasswordsForThisLogin = await LoginBreaches.getPotentiallyVulnerablePasswordsByLoginGUID(
              [login]
            );
            let isLoginVulnerable = !!vulnerablePasswordsForThisLogin.size;
            this.messageSubscribers(
              "AboutLogins:UpdateVulnerableLogins",
              new Map([[login.guid, isLoginVulnerable]])
            );
          }
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
      case "removeAllLogins": {
        this.messageSubscribers("AboutLogins:AllLogins", []);
        break;
      }
    }
  },

  async getFavicon(login) {
    try {
      const faviconData = await PlacesUtils.promiseFaviconData(login.origin);
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
      buttonIds: ["master-password-reload-button"],
      onClicks: [
        function onReloadClick(browser) {
          browser.reload();
        },
      ],
    });
    this.messageSubscribers("AboutLogins:MasterPasswordAuthRequired");
  },

  showPasswordSyncNotifications() {
    if (
      !Services.prefs.getBoolPref(SHOW_PASSWORD_SYNC_NOTIFICATION_PREF, true)
    ) {
      return;
    }

    this.showNotifications({
      id: PASSWORD_SYNC_NOTIFICATION_ID,
      priority: "PRIORITY_INFO_MEDIUM",
      iconURL: "chrome://browser/skin/login.svg",
      messageId: "enable-password-sync-notification-message",
      buttonIds: [
        "enable-password-sync-preferences-button",
        "about-logins-enable-password-sync-dont-ask-again-button",
      ],
      onClicks: [
        function onSyncPreferencesClick(browser) {
          browser.ownerGlobal.gSync.openPrefs("password-manager");
        },
        function onDontAskAgainClick(browser) {
          Services.prefs.setBoolPref(
            SHOW_PASSWORD_SYNC_NOTIFICATION_PREF,
            false
          );
        },
      ],
      extraFtl: ["branding/brand.ftl", "browser/branding/sync-brand.ftl"],
    });
  },

  showNotifications({
    id,
    priority,
    iconURL,
    messageId,
    buttonIds,
    onClicks,
    extraFtl = [],
  } = {}) {
    for (let subscriber of this._subscriberIterator()) {
      let browser = subscriber.embedderElement;
      let MozXULElement = browser.ownerGlobal.MozXULElement;
      MozXULElement.insertFTLIfNeeded("browser/aboutLogins.ftl");
      for (let ftl of extraFtl) {
        MozXULElement.insertFTLIfNeeded(ftl);
      }

      // If there's already an existing notification bar, don't do anything.
      let { gBrowser } = browser.ownerGlobal;
      let notificationBox = gBrowser.getNotificationBox(browser);
      let notification = notificationBox.getNotificationWithValue(id);
      if (notification) {
        continue;
      }

      // Configure the notification bar
      let doc = browser.ownerDocument;
      let messageFragment = doc.createDocumentFragment();
      let message = doc.createElement("span");
      doc.l10n.setAttributes(message, messageId);
      messageFragment.appendChild(message);

      let buttons = [];
      for (let i = 0; i < buttonIds.length; i++) {
        buttons[i] = {
          "l10n-id": buttonIds[i],
          popup: null,
          callback: () => {
            onClicks[i](browser);
          },
        };
      }

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
      let browser = subscriber.embedderElement;
      let { gBrowser } = browser.ownerGlobal;
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
      let browser = subscriber.embedderElement;
      if (
        !browser ||
        browser.remoteType != EXPECTED_ABOUTLOGINS_REMOTE_TYPE ||
        !browser.contentPrincipal ||
        browser.contentPrincipal.originNoSuffix != ABOUT_LOGINS_ORIGIN
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
        if (subscriber.currentWindowGlobal) {
          let actor = subscriber.currentWindowGlobal.getActor("AboutLogins");
          actor.sendAsyncMessage(name, details);
        }
      } catch (ex) {
        if (ex.result != Cr.NS_ERROR_NOT_INITIALIZED) {
          throw ex;
        }

        // The actor may be destroyed before the message is sent.
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

  async _sendAllLoginRelatedObjects(logins, browsingContext) {
    let sendMessageFn = (name, details) => {
      if (browsingContext && browsingContext.currentWindowGlobal) {
        let actor = browsingContext.currentWindowGlobal.getActor("AboutLogins");
        actor.sendAsyncMessage(name, details);
      } else {
        this.messageSubscribers(name, details);
      }
    };

    if (BREACH_ALERTS_ENABLED) {
      sendMessageFn(
        "AboutLogins:SetBreaches",
        await LoginBreaches.getPotentialBreachesByLoginGUID(logins)
      );
      if (VULNERABLE_PASSWORDS_ENABLED) {
        sendMessageFn(
          "AboutLogins:SetVulnerableLogins",
          await LoginBreaches.getPotentiallyVulnerablePasswordsByLoginGUID(
            logins
          )
        );
      }
    }

    sendMessageFn(
      "AboutLogins:SendFavicons",
      await AboutLogins.getAllFavicons(logins)
    );
  },

  getSyncState() {
    const state = UIState.get();
    // As long as Sync is configured, about:logins will treat it as
    // authenticated. More diagnostics and error states can be handled
    // by other more Sync-specific pages.
    const loggedIn = state.status != UIState.STATUS_NOT_CONFIGURED;

    // Pass the pref set if user has dismissed mobile promo footer
    const dismissedMobileFooter = Services.prefs.getBoolPref(
      HIDE_MOBILE_FOOTER_PREF
    );

    return {
      loggedIn,
      email: state.email,
      avatarURL: state.avatarURL,
      hideMobileFooter: !loggedIn || dismissedMobileFooter,
      fxAccountsEnabled: FXA_ENABLED,
    };
  },

  updatePasswordSyncNotificationState(
    syncState,
    // Need to explicitly call the getter on lazy preference getters
    // to activate their observer.
    passwordSyncEnabled = PASSWORD_SYNC_ENABLED
  ) {
    if (syncState.loggedIn && !passwordSyncEnabled) {
      this.showPasswordSyncNotifications();
      return;
    }
    this.removeNotifications(PASSWORD_SYNC_NOTIFICATION_ID);
  },

  onPasswordSyncEnabledPreferenceChange(data, previous, latest) {
    Services.prefs.clearUserPref(SHOW_PASSWORD_SYNC_NOTIFICATION_PREF);
    this.updatePasswordSyncNotificationState(this.getSyncState(), latest);
  },
};
var _AboutLogins = AboutLogins;

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "PASSWORD_SYNC_ENABLED",
  "services.sync.engine.passwords",
  false,
  AboutLogins.onPasswordSyncEnabledPreferenceChange.bind(AboutLogins)
);
