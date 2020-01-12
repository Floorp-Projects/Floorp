/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global ExtensionAPI */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppMenuNotifications: "resource://gre/modules/AppMenuNotifications.jsm",
  AppUpdater: "resource:///modules/AppUpdater.jsm",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",
  ProfileAge: "resource://gre/modules/ProfileAge.jsm",
  Services: "resource://gre/modules/Services.jsm",
  ResetProfile: "resource://gre/modules/ResetProfile.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  Sanitizer: "resource:///modules/Sanitizer.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "updateService",
  "@mozilla.org/updates/update-service;1",
  "nsIApplicationUpdateService"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "updateManager",
  "@mozilla.org/updates/update-manager;1",
  "nsIUpdateManager"
);

XPCOMUtils.defineLazyGetter(this, "appUpdater", () => new AppUpdater());

XPCOMUtils.defineLazyGetter(this, "appUpdaterStatusToStringMap", () => {
  // The AppUpdater.STATUS values have uppercase, underscored names like
  // READY_FOR_RESTART.  The statuses we return from this API are camel-cased
  // versions of those names, like "readyForRestart".  Here we convert those
  // AppUpdater.STATUS names to camel-cased names and store them in a map.
  let map = new Map();
  for (let name in AppUpdater.STATUS) {
    let parts = name.split("_").map(p => p.toLowerCase());
    let string =
      parts[0] +
      parts
        .slice(1)
        .map(p => p[0].toUpperCase() + p.substring(1))
        .join("");
    map.set(AppUpdater.STATUS[name], string);
  }
  return map;
});

XPCOMUtils.defineLazyGetter(
  this,
  "defaultPreferences",
  () => new Preferences({ defaultBranch: true })
);

this.experiments_urlbar = class extends ExtensionAPI {
  getAPI() {
    return {
      experiments: {
        urlbar: {
          checkForBrowserUpdate() {
            appUpdater.check();
          },

          clearInput() {
            let window = BrowserWindowTracker.getTopWindow();
            window.gURLBar.value = "";
            window.SetPageProxyState("invalid");
          },

          engagementTelemetry: this._getDefaultSettingsAPI(
            "browser.urlbar.eventTelemetry.enabled"
          ),

          getBrowserUpdateStatus() {
            return appUpdaterStatusToStringMap.get(appUpdater.status);
          },

          installBrowserUpdateAndRestart() {
            if (appUpdater.status != AppUpdater.STATUS.DOWNLOAD_AND_INSTALL) {
              return Promise.resolve();
            }
            return new Promise(resolve => {
              let listener = () => {
                // Once we call startDownload, there are two possible end
                // states: DOWNLOAD_FAILED and READY_FOR_RESTART.
                if (
                  appUpdater.status != AppUpdater.STATUS.READY_FOR_RESTART &&
                  appUpdater.status != AppUpdater.STATUS.DOWNLOAD_FAILED
                ) {
                  return;
                }
                appUpdater.removeListener(listener);
                if (appUpdater.status == AppUpdater.STATUS.READY_FOR_RESTART) {
                  restartBrowser();
                }
                resolve();
              };
              appUpdater.addListener(listener);
              appUpdater.startDownload();
            });
          },

          isBrowserShowingNotification() {
            let window = BrowserWindowTracker.getTopWindow();

            // urlbar view and notification box (info bar)
            if (
              window.gURLBar.view.isOpen ||
              window.gBrowser.getNotificationBox().currentNotification
            ) {
              return true;
            }

            // app menu notification doorhanger
            if (
              AppMenuNotifications.activeNotification &&
              !AppMenuNotifications.activeNotification.dismissed &&
              !AppMenuNotifications.activeNotification.options.badgeOnly
            ) {
              return true;
            }

            // tracking protection and identity box doorhangers
            if (
              ["tracking-protection-icon-container", "identity-box"].some(
                id =>
                  window.document.getElementById(id).getAttribute("open") ==
                  "true"
              )
            ) {
              return true;
            }

            // page action button panels
            let pageActions = window.document.getElementById(
              "page-action-buttons"
            );
            if (pageActions) {
              for (let child of pageActions.childNodes) {
                if (child.getAttribute("open") == "true") {
                  return true;
                }
              }
            }

            // toolbar button panels
            let navbar = window.document.getElementById(
              "nav-bar-customization-target"
            );
            for (let node of navbar.querySelectorAll("toolbarbutton")) {
              if (node.getAttribute("open") == "true") {
                return true;
              }
            }

            return false;
          },

          async lastBrowserUpdateDate() {
            // Get the newest update in the update history.  This isn't perfect
            // because these dates are when updates are applied, not when the
            // user restarts with the update.  See bug 1595328.
            if (updateManager.updateCount) {
              let update = updateManager.getUpdateAt(0);
              return update.installDate;
            }
            // Fall back to the profile age.
            let age = await ProfileAge();
            return (await age.firstUse) || age.created;
          },

          openViewOnFocus: this._getDefaultSettingsAPI(
            "browser.urlbar.openViewOnFocus"
          ),

          openClearHistoryDialog() {
            let window = BrowserWindowTracker.getTopWindow();
            // The behaviour of the Clear Recent History dialog in PBM does
            // not have the expected effect (bug 463607).
            if (PrivateBrowsingUtils.isWindowPrivate(window)) {
              return;
            }
            Sanitizer.showUI(window);
          },

          restartBrowser() {
            restartBrowser();
          },

          resetBrowser() {
            if (!ResetProfile.resetSupported()) {
              return;
            }
            let window = BrowserWindowTracker.getTopWindow();
            ResetProfile.openConfirmationDialog(window);
          },
        },
      },
    };
  }

  onShutdown() {
    // Reset the default prefs.  This is necessary because
    // ExtensionPreferencesManager doesn't properly reset prefs set on the
    // default branch.  See bug 1586543, bug 1578513, bug 1578508.
    if (this._initialDefaultPrefs) {
      for (let [pref, value] of this._initialDefaultPrefs.entries()) {
        defaultPreferences.set(pref, value);
      }
    }
  }

  _getDefaultSettingsAPI(pref) {
    return {
      get: details => {
        return {
          value: Preferences.get(pref),

          // Nothing actually uses this, but on debug builds there are extra
          // checks enabled in Schema.jsm that fail if it's not present.  The
          // value doesn't matter.
          levelOfControl: "controllable_by_this_extension",
        };
      },
      set: details => {
        if (!this._initialDefaultPrefs) {
          this._initialDefaultPrefs = new Map();
        }
        if (!this._initialDefaultPrefs.has(pref)) {
          this._initialDefaultPrefs.set(pref, defaultPreferences.get(pref));
        }
        defaultPreferences.set(pref, details.value);
        return true;
      },
      clear: details => {
        if (this._initialDefaultPrefs && this._initialDefaultPrefs.has(pref)) {
          defaultPreferences.set(pref, this._initialDefaultPrefs.get(pref));
          return true;
        }
        return false;
      },
    };
  }
};

function restartBrowser() {
  // Notify all windows that an application quit has been requested.
  let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
    Ci.nsISupportsPRBool
  );
  Services.obs.notifyObservers(
    cancelQuit,
    "quit-application-requested",
    "restart"
  );
  // Something aborted the quit process.
  if (cancelQuit.data) {
    return;
  }
  // If already in safe mode restart in safe mode.
  if (Services.appinfo.inSafeMode) {
    Services.startup.restartInSafeMode(Ci.nsIAppStartup.eAttemptQuit);
  } else {
    Services.startup.quit(
      Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart
    );
  }
}
