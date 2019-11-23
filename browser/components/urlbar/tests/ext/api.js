/* global ExtensionAPI */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppMenuNotifications: "resource://gre/modules/AppMenuNotifications.jsm",
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

XPCOMUtils.defineLazyGetter(
  this,
  "defaultPreferences",
  () => new Preferences({ defaultBranch: true })
);

function updateStateIs(prefix) {
  let update = updateManager.activeUpdate;
  return !!(update && update.state.startsWith(prefix));
}

this.experiments_urlbar = class extends ExtensionAPI {
  getAPI() {
    return {
      experiments: {
        urlbar: {
          engagementTelemetry: this._getDefaultSettingsAPI(
            "browser.urlbar.eventTelemetry.enabled"
          ),

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

          isBrowserUpdateReadyToInstall() {
            if (
              !updateService.canStageUpdates ||
              !Services.policies.isAllowed("appUpdate")
            ) {
              return updateStateIs("pending");
            }
            if (updateStateIs("applied")) {
              return true;
            }
            // If the state is pending and there is an error, staging failed and
            // Firefox can be restarted to apply the update without staging.
            let update = updateManager.activeUpdate;
            let errorCode = update ? update.errorCode : 0;
            return updateStateIs("pending") && errorCode != 0;
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
            // Notify all windows that an application quit has been requested.
            let cancelQuit = Cc[
              "@mozilla.org/supports-PRBool;1"
            ].createInstance(Ci.nsISupportsPRBool);
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
