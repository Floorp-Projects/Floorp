/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// _AboutLogins is only exported for testing
import { setTimeout, clearTimeout } from "resource://gre/modules/Timer.sys.mjs";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { E10SUtils } from "resource://gre/modules/E10SUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  LoginBreaches: "resource:///modules/LoginBreaches.sys.mjs",
  LoginCSVImport: "resource://gre/modules/LoginCSVImport.sys.mjs",
  LoginExport: "resource://gre/modules/LoginExport.sys.mjs",
  LoginHelper: "resource://gre/modules/LoginHelper.sys.mjs",
  MigrationUtils: "resource:///modules/MigrationUtils.sys.mjs",
  OSKeyStore: "resource://gre/modules/OSKeyStore.sys.mjs",
  UIState: "resource://services-sync/UIState.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "log", () => {
  return lazy.LoginHelper.createLogger("AboutLoginsParent");
});
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "BREACH_ALERTS_ENABLED",
  "signon.management.page.breach-alerts.enabled",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "FXA_ENABLED",
  "identity.fxaccounts.enabled",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "OS_AUTH_ENABLED",
  "signon.management.page.os-auth.enabled",
  true
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "VULNERABLE_PASSWORDS_ENABLED",
  "signon.management.page.vulnerable-passwords.enabled",
  false
);
ChromeUtils.defineLazyGetter(lazy, "AboutLoginsL10n", () => {
  return new Localization(["branding/brand.ftl", "browser/aboutLogins.ftl"]);
});

const ABOUT_LOGINS_ORIGIN = "about:logins";
const AUTH_TIMEOUT_MS = 5 * 60 * 1000; // 5 minutes
const PRIMARY_PASSWORD_NOTIFICATION_ID = "primary-password-login-required";

// about:logins will always use the privileged content process,
// even if it is disabled for other consumers such as about:newtab.
const EXPECTED_ABOUTLOGINS_REMOTE_TYPE = E10SUtils.PRIVILEGEDABOUT_REMOTE_TYPE;
let _gPasswordRemaskTimeout = null;
const convertSubjectToLogin = subject => {
  subject.QueryInterface(Ci.nsILoginMetaInfo).QueryInterface(Ci.nsILoginInfo);
  const login = lazy.LoginHelper.loginToVanillaObject(subject);
  if (!lazy.LoginHelper.isUserFacingLogin(login)) {
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

export class AboutLoginsParent extends JSWindowActorParent {
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

    AboutLogins.subscribers.add(this.browsingContext);

    switch (message.name) {
      case "AboutLogins:CreateLogin": {
        await this.#createLogin(message.data.login);
        break;
      }
      case "AboutLogins:DeleteLogin": {
        this.#deleteLogin(message.data.login);
        break;
      }
      case "AboutLogins:SortChanged": {
        this.#sortChanged(message.data);
        break;
      }
      case "AboutLogins:SyncEnable": {
        this.#syncEnable();
        break;
      }
      case "AboutLogins:SyncOptions": {
        this.#syncOptions();
        break;
      }
      case "AboutLogins:ImportFromBrowser": {
        this.#importFromBrowser();
        break;
      }
      case "AboutLogins:ImportReportInit": {
        this.#importReportInit();
        break;
      }
      case "AboutLogins:GetHelp": {
        this.#getHelp();
        break;
      }
      case "AboutLogins:OpenPreferences": {
        this.#openPreferences();
        break;
      }
      case "AboutLogins:PrimaryPasswordRequest": {
        await this.#primaryPasswordRequest(message.data);
        break;
      }
      case "AboutLogins:Subscribe": {
        await this.#subscribe();
        break;
      }
      case "AboutLogins:UpdateLogin": {
        this.#updateLogin(message.data.login);
        break;
      }
      case "AboutLogins:ExportPasswords": {
        await this.#exportPasswords();
        break;
      }
      case "AboutLogins:ImportFromFile": {
        await this.#importFromFile();
        break;
      }
      case "AboutLogins:RemoveAllLogins": {
        this.#removeAllLogins();
        break;
      }
    }
  }

  get #ownerGlobal() {
    return this.browsingContext.embedderElement?.ownerGlobal;
  }

  async #createLogin(newLogin) {
    if (!Services.policies.isAllowed("removeMasterPassword")) {
      if (!lazy.LoginHelper.isPrimaryPasswordSet()) {
        this.#ownerGlobal.openDialog(
          "chrome://mozapps/content/preferences/changemp.xhtml",
          "",
          "centerscreen,chrome,modal,titlebar"
        );
        if (!lazy.LoginHelper.isPrimaryPasswordSet()) {
          return;
        }
      }
    }
    // Remove the path from the origin, if it was provided.
    let origin = lazy.LoginHelper.getLoginOrigin(newLogin.origin);
    if (!origin) {
      console.error(
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
    newLogin = lazy.LoginHelper.vanillaObjectToLogin(newLogin);
    try {
      await Services.logins.addLoginAsync(newLogin);
    } catch (error) {
      this.#handleLoginStorageErrors(newLogin, error);
    }
  }

  get preselectedLogin() {
    const preselectedLogin =
      this.#ownerGlobal?.gBrowser.selectedTab.getAttribute("preselect-login") ||
      this.browsingContext.currentURI?.ref;
    this.#ownerGlobal?.gBrowser.selectedTab.removeAttribute("preselect-login");
    return preselectedLogin || null;
  }

  #deleteLogin(loginObject) {
    let login = lazy.LoginHelper.vanillaObjectToLogin(loginObject);
    Services.logins.removeLogin(login);
  }

  #sortChanged(sort) {
    Services.prefs.setCharPref("signon.management.page.sort", sort);
  }

  #syncEnable() {
    this.#ownerGlobal.gSync.openFxAEmailFirstPage("password-manager");
  }

  #syncOptions() {
    this.#ownerGlobal.gSync.openFxAManagePage("password-manager");
  }

  #importFromBrowser() {
    try {
      lazy.MigrationUtils.showMigrationWizard(this.#ownerGlobal, {
        entrypoint: lazy.MigrationUtils.MIGRATION_ENTRYPOINTS.PASSWORDS,
      });
    } catch (ex) {
      console.error(ex);
    }
  }

  #importReportInit() {
    let reportData = lazy.LoginCSVImport.lastImportReport;
    this.sendAsyncMessage("AboutLogins:ImportReportData", reportData);
  }

  #getHelp() {
    const SUPPORT_URL =
      Services.urlFormatter.formatURLPref("app.support.baseURL") +
      "password-manager-remember-delete-edit-logins";
    this.#ownerGlobal.openWebLinkIn(SUPPORT_URL, "tab", {
      relatedToCurrent: true,
    });
  }

  #openPreferences() {
    this.#ownerGlobal.openPreferences("privacy-logins");
  }

  async #primaryPasswordRequest(messageId) {
    if (!messageId) {
      throw new Error("AboutLogins:PrimaryPasswordRequest: no messageId.");
    }
    let messageText = { value: "NOT SUPPORTED" };
    let captionText = { value: "" };

    // This feature is only supported on Windows and macOS
    // but we still call in to OSKeyStore on Linux to get
    // the proper auth_details for Telemetry.
    // See bug 1614874 for Linux support.
    if (lazy.OS_AUTH_ENABLED && lazy.OSKeyStore.canReauth()) {
      messageId += "-" + AppConstants.platform;
      [messageText, captionText] = await lazy.AboutLoginsL10n.formatMessages([
        {
          id: messageId,
        },
        {
          id: "about-logins-os-auth-dialog-caption",
        },
      ]);
    }

    let { isAuthorized, telemetryEvent } = await lazy.LoginHelper.requestReauth(
      this.browsingContext.embedderElement,
      lazy.OS_AUTH_ENABLED,
      AboutLogins._authExpirationTime,
      messageText.value,
      captionText.value
    );
    this.sendAsyncMessage("AboutLogins:PrimaryPasswordResponse", {
      result: isAuthorized,
      telemetryEvent,
    });
    if (isAuthorized) {
      AboutLogins._authExpirationTime = Date.now() + AUTH_TIMEOUT_MS;
      const remaskPasswords = () => {
        this.sendAsyncMessage("AboutLogins:RemaskPassword");
      };
      clearTimeout(_gPasswordRemaskTimeout);
      _gPasswordRemaskTimeout = setTimeout(remaskPasswords, AUTH_TIMEOUT_MS);
    }
  }

  async #subscribe() {
    AboutLogins._authExpirationTime = Number.NEGATIVE_INFINITY;
    AboutLogins.addObservers();

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
        primaryPasswordEnabled: lazy.LoginHelper.isPrimaryPasswordSet(),
        passwordRevealVisible: Services.policies.isAllowed("passwordReveal"),
        importVisible:
          Services.policies.isAllowed("profileImport") &&
          AppConstants.platform != "linux",
        preselectedLogin: this.preselectedLogin,
      });

      await AboutLogins.sendAllLoginRelatedObjects(
        logins,
        this.browsingContext
      );
    } catch (ex) {
      if (ex.result != Cr.NS_ERROR_NOT_INITIALIZED) {
        throw ex;
      }

      // The message manager may be destroyed before the replies can be sent.
      lazy.log.debug(
        "AboutLogins:Subscribe: exception when replying with logins",
        ex
      );
    }
  }

  #updateLogin(loginUpdates) {
    let logins = lazy.LoginHelper.searchLoginsWithObject({
      guid: loginUpdates.guid,
    });
    if (logins.length != 1) {
      lazy.log.warn(
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
      this.#handleLoginStorageErrors(modifiedLogin, error);
    }
  }

  async #exportPasswords() {
    let messageText = { value: "NOT SUPPORTED" };
    let captionText = { value: "" };

    // This feature is only supported on Windows and macOS
    // but we still call in to OSKeyStore on Linux to get
    // the proper auth_details for Telemetry.
    // See bug 1614874 for Linux support.
    if (lazy.OSKeyStore.canReauth()) {
      let messageId =
        "about-logins-export-password-os-auth-dialog-message-" +
        AppConstants.platform;
      [messageText, captionText] = await lazy.AboutLoginsL10n.formatMessages([
        {
          id: messageId,
        },
        {
          id: "about-logins-os-auth-dialog-caption",
        },
      ]);
    }

    let { isAuthorized, telemetryEvent } = await lazy.LoginHelper.requestReauth(
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

    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    function fpCallback(aResult) {
      if (aResult != Ci.nsIFilePicker.returnCancel) {
        lazy.LoginExport.exportAsCSV(fp.file.path);
        Services.telemetry.recordEvent(
          "pwmgr",
          "mgmt_menu_item_used",
          "export_complete"
        );
      }
    }
    let [title, defaultFilename, okButtonLabel, csvFilterTitle] =
      await lazy.AboutLoginsL10n.formatValues([
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

    fp.init(this.#ownerGlobal, title, Ci.nsIFilePicker.modeSave);
    fp.appendFilter(csvFilterTitle, "*.csv");
    fp.appendFilters(Ci.nsIFilePicker.filterAll);
    fp.defaultString = defaultFilename;
    fp.defaultExtension = "csv";
    fp.okButtonLabel = okButtonLabel;
    fp.open(fpCallback);
  }

  async #importFromFile() {
    let [title, okButtonLabel, csvFilterTitle, tsvFilterTitle] =
      await lazy.AboutLoginsL10n.formatValues([
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
      this.#ownerGlobal
    );

    if (result != Ci.nsIFilePicker.returnCancel) {
      let summary;
      try {
        summary = await lazy.LoginCSVImport.importFromCSV(path);
      } catch (e) {
        console.error(e);
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
  }

  #removeAllLogins() {
    Services.logins.removeAllUserFacingLogins();
  }

  #handleLoginStorageErrors(login, error) {
    let messageObject = {
      login: augmentVanillaLoginObject(
        lazy.LoginHelper.loginToVanillaObject(login)
      ),
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

class AboutLoginsInternal {
  subscribers = new WeakSet();
  #observersAdded = false;
  authExpirationTime = Number.NEGATIVE_INFINITY;

  async observe(subject, topic, type) {
    if (!ChromeUtils.nondeterministicGetWeakSetKeys(this.subscribers).length) {
      this.#removeObservers();
      return;
    }

    switch (topic) {
      case "passwordmgr-reload-all": {
        await this.#reloadAllLogins();
        break;
      }
      case "passwordmgr-crypto-login": {
        this.#removeNotifications(PRIMARY_PASSWORD_NOTIFICATION_ID);
        await this.#reloadAllLogins();
        break;
      }
      case "passwordmgr-crypto-loginCanceled": {
        this.#showPrimaryPasswordLoginNotifications();
        break;
      }
      case lazy.UIState.ON_UPDATE: {
        this.#messageSubscribers("AboutLogins:SyncState", this.getSyncState());
        break;
      }
      case "passwordmgr-storage-changed": {
        switch (type) {
          case "addLogin": {
            await this.#addLogin(subject);
            break;
          }
          case "modifyLogin": {
            this.#modifyLogin(subject);
            break;
          }
          case "removeLogin": {
            this.#removeLogin(subject);
            break;
          }
          case "removeAllLogins": {
            this.#removeAllLogins();
            break;
          }
        }
      }
    }
  }

  async #addLogin(subject) {
    const login = convertSubjectToLogin(subject);
    if (!login) {
      return;
    }

    if (lazy.BREACH_ALERTS_ENABLED) {
      this.#messageSubscribers(
        "AboutLogins:UpdateBreaches",
        await lazy.LoginBreaches.getPotentialBreachesByLoginGUID([login])
      );
      if (lazy.VULNERABLE_PASSWORDS_ENABLED) {
        this.#messageSubscribers(
          "AboutLogins:UpdateVulnerableLogins",
          await lazy.LoginBreaches.getPotentiallyVulnerablePasswordsByLoginGUID(
            [login]
          )
        );
      }
    }

    this.#messageSubscribers("AboutLogins:LoginAdded", login);
  }

  async #modifyLogin(subject) {
    subject.QueryInterface(Ci.nsIArrayExtensions);
    const login = convertSubjectToLogin(subject.GetElementAt(1));
    if (!login) {
      return;
    }

    if (lazy.BREACH_ALERTS_ENABLED) {
      let breachesForThisLogin =
        await lazy.LoginBreaches.getPotentialBreachesByLoginGUID([login]);
      let breachData = breachesForThisLogin.size
        ? breachesForThisLogin.get(login.guid)
        : false;
      this.#messageSubscribers(
        "AboutLogins:UpdateBreaches",
        new Map([[login.guid, breachData]])
      );
      if (lazy.VULNERABLE_PASSWORDS_ENABLED) {
        let vulnerablePasswordsForThisLogin =
          await lazy.LoginBreaches.getPotentiallyVulnerablePasswordsByLoginGUID(
            [login]
          );
        let isLoginVulnerable = !!vulnerablePasswordsForThisLogin.size;
        this.#messageSubscribers(
          "AboutLogins:UpdateVulnerableLogins",
          new Map([[login.guid, isLoginVulnerable]])
        );
      }
    }

    this.#messageSubscribers("AboutLogins:LoginModified", login);
  }

  #removeLogin(subject) {
    const login = convertSubjectToLogin(subject);
    if (!login) {
      return;
    }
    this.#messageSubscribers("AboutLogins:LoginRemoved", login);
  }

  #removeAllLogins() {
    this.#messageSubscribers("AboutLogins:RemoveAllLogins", []);
  }

  async #reloadAllLogins() {
    let logins = await this.getAllLogins();
    this.#messageSubscribers("AboutLogins:AllLogins", logins);
    await this.sendAllLoginRelatedObjects(logins);
  }

  #showPrimaryPasswordLoginNotifications() {
    this.#showNotifications({
      id: PRIMARY_PASSWORD_NOTIFICATION_ID,
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
    this.#messageSubscribers("AboutLogins:PrimaryPasswordAuthRequired");
  }

  #showNotifications({
    id,
    priority,
    iconURL,
    messageId,
    buttonIds,
    onClicks,
    extraFtl = [],
  } = {}) {
    for (let subscriber of this.#subscriberIterator()) {
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
        id,
        {
          label: { "l10n-id": messageId },
          image: iconURL,
          priority: notificationBox[priority],
        },
        buttons
      );
    }
  }

  #removeNotifications(notificationId) {
    for (let subscriber of this.#subscriberIterator()) {
      let browser = subscriber.embedderElement;
      let { gBrowser } = browser.ownerGlobal;
      let notificationBox = gBrowser.getNotificationBox(browser);
      let notification =
        notificationBox.getNotificationWithValue(notificationId);
      if (!notification) {
        continue;
      }
      notificationBox.removeNotification(notification);
    }
  }

  *#subscriberIterator() {
    let subscribers = ChromeUtils.nondeterministicGetWeakSetKeys(
      this.subscribers
    );
    for (let subscriber of subscribers) {
      let browser = subscriber.embedderElement;
      if (
        browser?.remoteType != EXPECTED_ABOUTLOGINS_REMOTE_TYPE ||
        browser?.contentPrincipal?.originNoSuffix != ABOUT_LOGINS_ORIGIN
      ) {
        this.subscribers.delete(subscriber);
        continue;
      }
      yield subscriber;
    }
  }

  #messageSubscribers(name, details) {
    for (let subscriber of this.#subscriberIterator()) {
      try {
        if (subscriber.currentWindowGlobal) {
          let actor = subscriber.currentWindowGlobal.getActor("AboutLogins");
          actor.sendAsyncMessage(name, details);
        }
      } catch (ex) {
        if (ex.result == Cr.NS_ERROR_NOT_INITIALIZED) {
          // The actor may be destroyed before the message is sent.
          lazy.log.debug(
            "messageSubscribers: exception when calling sendAsyncMessage",
            ex
          );
        } else {
          throw ex;
        }
      }
    }
  }

  async getAllLogins() {
    try {
      let logins = await lazy.LoginHelper.getAllUserFacingLogins();
      return logins
        .map(lazy.LoginHelper.loginToVanillaObject)
        .map(augmentVanillaLoginObject);
    } catch (e) {
      if (e.result == Cr.NS_ERROR_ABORT) {
        // If the user cancels the MP prompt then return no logins.
        return [];
      }
      throw e;
    }
  }

  async sendAllLoginRelatedObjects(logins, browsingContext) {
    let sendMessageFn = (name, details) => {
      if (browsingContext?.currentWindowGlobal) {
        let actor = browsingContext.currentWindowGlobal.getActor("AboutLogins");
        actor.sendAsyncMessage(name, details);
      } else {
        this.#messageSubscribers(name, details);
      }
    };

    if (lazy.BREACH_ALERTS_ENABLED) {
      sendMessageFn(
        "AboutLogins:SetBreaches",
        await lazy.LoginBreaches.getPotentialBreachesByLoginGUID(logins)
      );
      if (lazy.VULNERABLE_PASSWORDS_ENABLED) {
        sendMessageFn(
          "AboutLogins:SetVulnerableLogins",
          await lazy.LoginBreaches.getPotentiallyVulnerablePasswordsByLoginGUID(
            logins
          )
        );
      }
    }
  }

  getSyncState() {
    const state = lazy.UIState.get();
    // As long as Sync is configured, about:logins will treat it as
    // authenticated. More diagnostics and error states can be handled
    // by other more Sync-specific pages.
    const loggedIn = state.status != lazy.UIState.STATUS_NOT_CONFIGURED;
    const passwordSyncEnabled = state.syncEnabled && lazy.PASSWORD_SYNC_ENABLED;

    return {
      loggedIn,
      email: state.email,
      avatarURL: state.avatarURL,
      fxAccountsEnabled: lazy.FXA_ENABLED,
      passwordSyncEnabled,
    };
  }

  onPasswordSyncEnabledPreferenceChange(data, previous, latest) {
    this.#messageSubscribers("AboutLogins:SyncState", this.getSyncState());
  }

  #observedTopics = [
    "passwordmgr-crypto-login",
    "passwordmgr-crypto-loginCanceled",
    "passwordmgr-storage-changed",
    "passwordmgr-reload-all",
    lazy.UIState.ON_UPDATE,
  ];

  addObservers() {
    if (!this.#observersAdded) {
      for (const topic of this.#observedTopics) {
        Services.obs.addObserver(this, topic);
      }
      this.#observersAdded = true;
    }
  }

  #removeObservers() {
    for (const topic of this.#observedTopics) {
      Services.obs.removeObserver(this, topic);
    }
    this.#observersAdded = false;
  }
}

let AboutLogins = new AboutLoginsInternal();
export var _AboutLogins = AboutLogins;

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "PASSWORD_SYNC_ENABLED",
  "services.sync.engine.passwords",
  false,
  AboutLogins.onPasswordSyncEnabledPreferenceChange
);
