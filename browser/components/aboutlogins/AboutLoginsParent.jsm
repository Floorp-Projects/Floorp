/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// _AboutLogins is only exported for testing
var EXPORTED_SYMBOLS = ["AboutLoginsParent", "_AboutLogins"];

const { setTimeout, clearTimeout } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
  LoginBreaches: "resource:///modules/LoginBreaches.jsm",
  LoginHelper: "resource://gre/modules/LoginHelper.jsm",
  LoginExport: "resource://gre/modules/LoginExport.jsm",
  LoginCSVImport: "resource://gre/modules/LoginCSVImport.jsm",
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
  "FXA_ENABLED",
  "identity.fxaccounts.enabled",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "OS_AUTH_ENABLED",
  "signon.management.page.os-auth.enabled",
  true
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "VULNERABLE_PASSWORDS_ENABLED",
  "signon.management.page.vulnerable-passwords.enabled",
  false
);
XPCOMUtils.defineLazyGetter(this, "AboutLoginsL10n", () => {
  return new Localization(["branding/brand.ftl", "browser/aboutLogins.ftl"]);
});

const ABOUT_LOGINS_ORIGIN = "about:logins";
const MASTER_PASSWORD_NOTIFICATION_ID = "master-password-login-required";

// about:logins will always use the privileged content process,
// even if it is disabled for other consumers such as about:newtab.
const EXPECTED_ABOUTLOGINS_REMOTE_TYPE = E10SUtils.PRIVILEGEDABOUT_REMOTE_TYPE;
let _passwordRemaskTimeout;
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
        if (!Services.policies.isAllowed("removeMasterPassword")) {
          if (!LoginHelper.isMasterPasswordSet()) {
            ownerGlobal.openDialog(
              "chrome://mozapps/content/preferences/changemp.xhtml",
              "",
              "centerscreen,chrome,modal,titlebar"
            );
            if (!LoginHelper.isMasterPasswordSet()) {
              return;
            }
          }
        }
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

      case "AboutLogins:ImportReportInit": {
        let reportData = LoginCSVImport.lastImportReport;
        this.sendAsyncMessage("AboutLogins:ImportReportData", reportData);
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
        let messageText = { value: "NOT SUPPORTED" };
        let captionText = { value: "" };

        // This feature is only supported on Windows and macOS
        // but we still call in to OSKeyStore on Linux to get
        // the proper auth_details for Telemetry.
        // See bug 1614874 for Linux support.
        if (OS_AUTH_ENABLED && OSKeyStore.canReauth()) {
          messageId += "-" + AppConstants.platform;
          [messageText, captionText] = await AboutLoginsL10n.formatMessages([
            {
              id: messageId,
            },
            {
              id: "about-logins-os-auth-dialog-caption",
            },
          ]);
        }

        let { isAuthorized, telemetryEvent } = await LoginHelper.requestReauth(
          this.browsingContext.embedderElement,
          OS_AUTH_ENABLED,
          AboutLogins._authExpirationTime,
          messageText.value,
          captionText.value
        );
        this.sendAsyncMessage("AboutLogins:MasterPasswordResponse", {
          result: isAuthorized,
          telemetryEvent,
        });
        if (isAuthorized) {
          const AUTH_TIMEOUT_MS = 5 * 60 * 1000; // 5 minutes
          AboutLogins._authExpirationTime = Date.now() + AUTH_TIMEOUT_MS;
          const remaskPasswords = () => {
            this.sendAsyncMessage("AboutLogins:RemaskPassword");
          };
          clearTimeout(_passwordRemaskTimeout);
          _passwordRemaskTimeout = setTimeout(remaskPasswords, AUTH_TIMEOUT_MS);
        }
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
      case "AboutLogins:ExportPasswords": {
        let messageText = { value: "NOT SUPPORTED" };
        let captionText = { value: "" };

        // This feature is only supported on Windows and macOS
        // but we still call in to OSKeyStore on Linux to get
        // the proper auth_details for Telemetry.
        // See bug 1614874 for Linux support.
        if (OSKeyStore.canReauth()) {
          let messageId =
            "about-logins-export-password-os-auth-dialog-message-" +
            AppConstants.platform;
          [messageText, captionText] = await AboutLoginsL10n.formatMessages([
            {
              id: messageId,
            },
            {
              id: "about-logins-os-auth-dialog-caption",
            },
          ]);
        }

        let { isAuthorized, telemetryEvent } = await LoginHelper.requestReauth(
          this.browsingContext.embedderElement,
          true,
          null, // Prompt regardless of a recent prompt
          messageText.value,
          captionText.value
        );

        let { method, object, extra = {}, value = null } = telemetryEvent;
        Services.telemetry.recordEvent("pwmgr", method, object, value, extra);

        if (!isAuthorized) {
          return;
        }

        let fp = Cc["@mozilla.org/filepicker;1"].createInstance(
          Ci.nsIFilePicker
        );
        function fpCallback(aResult) {
          if (aResult != Ci.nsIFilePicker.returnCancel) {
            LoginExport.exportAsCSV(fp.file.path);
            Services.telemetry.recordEvent(
              "pwmgr",
              "mgmt_menu_item_used",
              "export_complete"
            );
          }
        }
        let [
          title,
          defaultFilename,
          okButtonLabel,
          csvFilterTitle,
        ] = await AboutLoginsL10n.formatValues([
          {
            id: "about-logins-export-file-picker-title",
          },
          {
            id: "about-logins-export-file-picker-default-filename",
          },
          {
            id: "about-logins-export-file-picker-export-button",
          },
          {
            id: "about-logins-export-file-picker-csv-filter-title",
          },
        ]);

        fp.init(ownerGlobal, title, Ci.nsIFilePicker.modeSave);
        fp.appendFilter(csvFilterTitle, "*.csv");
        fp.appendFilters(Ci.nsIFilePicker.filterAll);
        fp.defaultString = defaultFilename;
        fp.defaultExtension = "csv";
        fp.okButtonLabel = okButtonLabel;
        fp.open(fpCallback);
        break;
      }
      case "AboutLogins:ImportPasswords": {
        let [
          title,
          okButtonLabel,
          csvFilterTitle,
          tsvFilterTitle,
        ] = await AboutLoginsL10n.formatValues([
          {
            id: "about-logins-import-file-picker-title",
          },
          {
            id: "about-logins-import-file-picker-import-button",
          },
          {
            id: "about-logins-import-file-picker-csv-filter-title",
          },
          {
            id: "about-logins-import-file-picker-tsv-filter-title",
          },
        ]);
        let { result, path } = await this.openFilePickerDialog(
          title,
          okButtonLabel,
          [
            {
              title: csvFilterTitle,
              extensionPattern: "*.csv",
            },
            {
              title: tsvFilterTitle,
              extensionPattern: "*.tsv",
            },
          ],
          ownerGlobal
        );

        if (result != Ci.nsIFilePicker.returnCancel) {
          let summary;
          try {
            summary = await LoginCSVImport.importFromCSV(path);
          } catch (e) {
            Cu.reportError(e);
            this.sendAsyncMessage(
              "AboutLogins:ImportPasswordsErrorDialog",
              e.errorType
            );
          }
          if (summary) {
            this.sendAsyncMessage("AboutLogins:ImportPasswordsDialog", summary);
            Services.telemetry.recordEvent(
              "pwmgr",
              "mgmt_menu_item_used",
              "import_csv_complete"
            );
          }
        }
        break;
      }
      case "AboutLogins:RemoveAllLogins": {
        Services.logins.removeAllUserFacingLogins();
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

  async openFilePickerDialog(title, okButtonLabel, appendFilters, ownerGlobal) {
    return new Promise(resolve => {
      let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
      fp.init(ownerGlobal, title, Ci.nsIFilePicker.modeOpen);
      for (const appendFilter of appendFilters) {
        fp.appendFilter(appendFilter.title, appendFilter.extensionPattern);
      }
      fp.appendFilters(Ci.nsIFilePicker.filterAll);
      fp.okButtonLabel = okButtonLabel;
      fp.open(async result => {
        resolve({ result, path: fp.file.path });
      });
    });
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

        this.messageSubscribers("AboutLogins:LoginAdded", login);
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
        this.messageSubscribers("AboutLogins:RemoveAllLogins", []);
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
      messageId: "about-logins-primary-password-notification-message",
      buttonIds: ["master-password-reload-button"],
      onClicks: [
        function onReloadClick(browser) {
          browser.reload();
        },
      ],
    });
    this.messageSubscribers("AboutLogins:MasterPasswordAuthRequired");
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
    const passwordSyncEnabled = state.syncEnabled && PASSWORD_SYNC_ENABLED;

    return {
      loggedIn,
      email: state.email,
      avatarURL: state.avatarURL,
      fxAccountsEnabled: FXA_ENABLED,
      passwordSyncEnabled,
    };
  },

  onPasswordSyncEnabledPreferenceChange(data, previous, latest) {
    this.messageSubscribers("AboutLogins:SyncState", this.getSyncState());
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
