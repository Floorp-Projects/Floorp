/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Manages breach alerts for saved logins using data from Firefox Monitor via
 * RemoteSettings.
 */

"use strict";

const EXPORTED_SYMBOLS = ["LoginBreaches"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  LoginHelper: "resource://gre/modules/LoginHelper.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
  RemoteSettingsClient: "resource://services-settings/RemoteSettingsClient.jsm",
});

this.LoginBreaches = {
  REMOTE_SETTINGS_COLLECTION: "fxmonitor-breaches",

  async recordDismissal(loginGuid) {
    await Services.logins.initializationPromise;
    const storageJSON =
      Services.logins.wrappedJSObject._storage.wrappedJSObject;

    return storageJSON.recordBreachAlertDismissal(loginGuid);
  },

  async update(breaches = null) {
    const logins = await LoginHelper.getAllUserFacingLogins();
    await this.getBreachesForLogins(logins, breaches);
  },

  /**
   * Return a Map of login GUIDs to a potential breach affecting that login
   * by considering only breaches affecting passwords.
   *
   * This only uses the breach `Domain` and `timePasswordChanged` to determine
   * if a login may be breached which means it may contain false-positives if
   * login timestamps are incorrect, the user didn't save their password change
   * in Firefox, or the breach didn't contain all accounts, etc. As a result,
   * consumers should avoid making stronger claims than the data supports.
   *
   * @param {nsILoginInfo[]} logins Saved logins to check for potential breaches.
   * @param {object[]} [breaches = null] Only ones involving passwords will be used.
   * @returns {Map} with a key for each login GUID potentially in a breach.
   */
  async getPotentialBreachesByLoginGUID(logins, breaches = null) {
    const breachesByLoginGUID = new Map();
    if (!breaches) {
      try {
        breaches = await RemoteSettings(this.REMOTE_SETTINGS_COLLECTION).get();
      } catch (ex) {
        if (ex instanceof RemoteSettingsClient.UnknownCollectionError) {
          log.warn(
            "Could not get Remote Settings collection.",
            this.REMOTE_SETTINGS_COLLECTION,
            ex
          );
          return breachesByLoginGUID;
        }
        throw ex;
      }
    }
    const BREACH_ALERT_URL = Services.prefs.getStringPref(
      "signon.management.page.breachAlertUrl"
    );
    const baseBreachAlertURL = new URL(BREACH_ALERT_URL);

    await Services.logins.initializationPromise;
    const storageJSON =
      Services.logins.wrappedJSObject._storage.wrappedJSObject;
    const dismissedBreachAlertsByLoginGUID = storageJSON.getBreachAlertDismissalsByLoginGUID();

    // Determine potentially breached logins by checking their origin and the last time
    // they were changed. It's important to note here that we are NOT considering the
    // username and password of that login.
    for (const login of logins) {
      const loginURI = Services.io.newURI(login.origin);
      let loginHost;
      try {
        // nsIURI.host can throw if the URI scheme doesn't have a host.
        loginHost = loginURI.host;
      } catch (ex) {
        continue;
      }
      for (const breach of breaches) {
        if (
          !breach.Domain ||
          !Services.eTLD.hasRootDomain(loginHost, breach.Domain) ||
          !this._breachInvolvedPasswords(breach) ||
          !this._breachWasAfterPasswordLastChanged(breach, login)
        ) {
          continue;
        }

        if (!storageJSON.isPotentiallyVulnerablePassword(login)) {
          storageJSON.addPotentiallyVulnerablePassword(login);
        }

        if (
          this._breachAlertIsDismissed(
            login,
            breach,
            dismissedBreachAlertsByLoginGUID
          )
        ) {
          continue;
        }

        let breachAlertURL = new URL(breach.Name, baseBreachAlertURL);
        breach.breachAlertURL = breachAlertURL.href;
        breachesByLoginGUID.set(login.guid, breach);
      }
    }
    return breachesByLoginGUID;
  },

  /**
   * Return information about logins using passwords that were potentially in a
   * breach.
   * @see the caveats in the documentation for `getPotentialBreachesByLoginGUID`.
   *
   * @param {nsILoginInfo[]} logins to check the passwords of.
   * @returns {Map} from login GUID to `true` for logins that have a password
   *                that may be vulnerable.
   */
  getPotentiallyVulnerablePasswordsByLoginGUID(logins) {
    const vulnerablePasswordsByLoginGUID = new Map();
    const storageJSON =
      Services.logins.wrappedJSObject._storage.wrappedJSObject;
    for (const login of logins) {
      if (storageJSON.isPotentiallyVulnerablePassword(login)) {
        vulnerablePasswordsByLoginGUID.set(login.guid, true);
      }
    }
    return vulnerablePasswordsByLoginGUID;
  },

  _breachAlertIsDismissed(login, breach, dismissedBreachAlerts) {
    const breachAddedDate = new Date(breach.AddedDate).getTime();
    const breachAlertIsDismissed =
      dismissedBreachAlerts[login.guid] &&
      dismissedBreachAlerts[login.guid].timeBreachAlertDismissed >
        breachAddedDate;
    return breachAlertIsDismissed;
  },

  _breachInvolvedPasswords(breach) {
    return (
      breach.hasOwnProperty("DataClasses") &&
      breach.DataClasses.includes("Passwords")
    );
  },

  _breachWasAfterPasswordLastChanged(breach, login) {
    const breachDate = new Date(breach.BreachDate).getTime();
    return login.timePasswordChanged < breachDate;
  },
};

XPCOMUtils.defineLazyGetter(this, "log", () => {
  return LoginHelper.createLogger("LoginBreaches");
});
