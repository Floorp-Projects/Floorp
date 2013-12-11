// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

Cu.import("resource://gre/modules/Services.jsm");
let gAppUpdater;

let AboutFlyoutPanel = {
  init: function() {
    if (this._isInitialized) {
      Cu.reportError("Attempted to initialize AboutFlyoutPanel more than once");
    }

    this._isInitialized = true;

    let self = this;
    this._elements = {};
    [
      ['versionLabel', 'about-version-label'],
      ['AboutFlyoutPanel',  'about-flyoutpanel'],
    ].forEach(function(aElement) {
      let [name, id] = aElement;
      XPCOMUtils.defineLazyGetter(self._elements, name, function() {
        return document.getElementById(id);
      });
    });

    this._topmostElement = this._elements.AboutFlyoutPanel;

    // Include the build ID if this is an "a#" (nightly or aurora) build
    let version = Services.appinfo.version;
    if (/a\d+$/.test(version)) {
      let buildID = Services.appinfo.appBuildID;
      let buildDate = buildID.slice(0,4) + "-" + buildID.slice(4,6) +
                      "-" + buildID.slice(6,8);
      this._elements.versionLabel.textContent +=" (" + buildDate + ")";
    }

    window.addEventListener('MozFlyoutPanelShowing', this, false);
    window.addEventListener('MozFlyoutPanelHiding', this, false);

#if MOZ_UPDATE_CHANNEL != release
    let defaults = Services.prefs.getDefaultBranch("");
    let channelLabel = document.getElementById("currentChannel");
    channelLabel.value = defaults.getCharPref("app.update.channel");
#endif
  },

  onPolicyClick: function(aEvent) {
    if (aEvent.button != 0) {
      return;
    }
    let url = Services.urlFormatter.formatURLPref("app.privacyURL");
    BrowserUI.addAndShowTab(url, Browser.selectedTab);
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case 'MozFlyoutPanelShowing':
#ifdef MOZ_UPDATER
        this.appUpdater = new appUpdater();
        gAppUpdater = this.appUpdater;
#endif
        break;
      case 'MozFlyoutPanelHiding':
#ifdef MOZ_UPDATER
        onUnload();
#endif
        break;
    }
  }
};

#ifdef MOZ_UPDATER
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/DownloadUtils.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");

function onUnload(aEvent) {
  if (!gAppUpdater) {
    return;
  }

  if (gAppUpdater.isChecking)
    gAppUpdater.checker.stopChecking(Components.interfaces.nsIUpdateChecker.CURRENT_CHECK);
  // Safe to call even when there isn't a download in progress.
  gAppUpdater.removeDownloadListener();
  gAppUpdater = null;
  AboutFlyoutPanel.appUpdater = null;
}

function appUpdater()
{
  this.updateDeck = document.getElementById("updateDeck");

  XPCOMUtils.defineLazyServiceGetter(this, "aus",
                                     "@mozilla.org/updates/update-service;1",
                                     "nsIApplicationUpdateService");
  XPCOMUtils.defineLazyServiceGetter(this, "checker",
                                     "@mozilla.org/updates/update-checker;1",
                                     "nsIUpdateChecker");
  XPCOMUtils.defineLazyServiceGetter(this, "um",
                                     "@mozilla.org/updates/update-manager;1",
                                     "nsIUpdateManager");

  this.bundle = Services.strings.
                createBundle("chrome://browser/locale/browser.properties");

  this.updateBtn = document.getElementById("updateButton");

  // The button label value must be set so its height is correct.
  this.setupUpdateButton("update.checkInsideButton");

  let manualURL = Services.urlFormatter.formatURLPref("app.update.url.manual");
  let manualLink = document.getElementById("manualLink");
  manualLink.value = manualURL;
  manualLink.href = manualURL;
  document.getElementById("failedLink").href = manualURL;

  if (this.updateDisabledAndLocked) {
    this.selectPanel("adminDisabled");
    return;
  }

  if (this.isPending || this.isApplied) {
    this.setupUpdateButton("update.restart." +
                           (this.isMajor ? "upgradeButton" : "updateButton"));
    return;
  }

  if (this.aus.isOtherInstanceHandlingUpdates) {
    this.selectPanel("otherInstanceHandlingUpdates");
    return;
  }

  if (this.isDownloading) {
    this.startDownload();
    return;
  }

  if (this.updateEnabled && this.updateAuto) {
    this.selectPanel("checkingForUpdates");
    this.isChecking = true;
    this.checker.checkForUpdates(this.updateCheckListener, true);
    return;
  }
}

appUpdater.prototype =
{
  // true when there is an update check in progress.
  isChecking: false,

  // true when there is an update already staged / ready to be applied.
  get isPending() {
    if (this.update) {
      return this.update.state == "pending" ||
             this.update.state == "pending-service";
    }
    return this.um.activeUpdate &&
           (this.um.activeUpdate.state == "pending" ||
            this.um.activeUpdate.state == "pending-service");
  },

  // true when there is an update already installed in the background.
  get isApplied() {
    if (this.update)
      return this.update.state == "applied" ||
             this.update.state == "applied-service";
    return this.um.activeUpdate &&
           (this.um.activeUpdate.state == "applied" ||
            this.um.activeUpdate.state == "applied-service");
  },

  // true when there is an update download in progress.
  get isDownloading() {
    if (this.update)
      return this.update.state == "downloading";
    return this.um.activeUpdate &&
           this.um.activeUpdate.state == "downloading";
  },

  // true when the update type is major.
  get isMajor() {
    if (this.update)
      return this.update.type == "major";
    return this.um.activeUpdate.type == "major";
  },

  // true when updating is disabled by an administrator.
  get updateDisabledAndLocked() {
    return !this.updateEnabled &&
           Services.prefs.prefIsLocked("app.update.enabled");
  },

  // true when updating is enabled.
  get updateEnabled() {
    let updatesEnabled = true;
    try {
      updatesEnabled = Services.prefs.getBoolPref("app.update.metro.enabled");
    }
    catch (e) { }
    if (!updatesEnabled) {
      return false;
    }

    try {
      updatesEnabled = Services.prefs.getBoolPref("app.update.enabled")
    }
    catch (e) { }

    return updatesEnabled;
  },

  // true when updating in background is enabled.
  get backgroundUpdateEnabled() {
    return this.updateEnabled &&
           gAppUpdater.aus.canStageUpdates;
  },

  // true when updating is automatic.
  get updateAuto() {
    try {
      return Services.prefs.getBoolPref("app.update.auto");
    }
    catch (e) { }
    return true; // Firefox default is true
  },

  /**
   * Sets the deck's selected panel.
   *
   * @param  aChildID
   *         The id of the deck's child to select.
   */
  selectPanel: function(aChildID) {
    this.updateDeck.selectedPanel = document.getElementById(aChildID);
    this.updateBtn.disabled = (aChildID != "updateButtonBox");
  },

  /**
   * Sets the update button's label and accesskey.
   *
   * @param  aKeyPrefix
   *         The prefix for the properties file entry to use for setting the
   *         label and accesskey.
   */
  setupUpdateButton: function(aKeyPrefix) {
    this.updateBtn.label = this.bundle.GetStringFromName(aKeyPrefix + ".label");
    this.updateBtn.accessKey = this.bundle.GetStringFromName(aKeyPrefix + ".accesskey");
    if (!document.commandDispatcher.focusedElement ||
        document.commandDispatcher.focusedElement == this.updateBtn)
      this.updateBtn.focus();
  },

  /**
   * Handles oncommand for the update button.
   */
  buttonOnCommand: function() {
    if (this.isPending || this.isApplied) {
      // Notify all windows that an application quit has been requested.
      let cancelQuit = Components.classes["@mozilla.org/supports-PRBool;1"].
                       createInstance(Components.interfaces.nsISupportsPRBool);
      Services.obs.notifyObservers(cancelQuit, "quit-application-requested", "restart");

      // Something aborted the quit process.
      if (cancelQuit.data)
        return;

      let appStartup = Components.classes["@mozilla.org/toolkit/app-startup;1"].
                       getService(Components.interfaces.nsIAppStartup);

      // If already in safe mode restart in safe mode (bug 327119)
      if (Services.appinfo.inSafeMode) {
        appStartup.restartInSafeMode(Components.interfaces.nsIAppStartup.eAttemptQuit);
        return;
      }

      appStartup.quit(Components.interfaces.nsIAppStartup.eAttemptQuit |
                      Components.interfaces.nsIAppStartup.eRestartTouchEnvironment);
      return;
    }

    const URI_UPDATE_PROMPT_DIALOG = "chrome://mozapps/content/update/updates.xul";
    // Firefox no longer displays a license for updates and the licenseURL check
    // is just in case a distibution does.
    if (this.update && (this.update.billboardURL || this.update.licenseURL ||
        this.addons.length != 0)) {
      var ary = null;
      ary = Components.classes["@mozilla.org/supports-array;1"].
            createInstance(Components.interfaces.nsISupportsArray);
      ary.AppendElement(this.update);
      var openFeatures = "chrome,centerscreen,dialog=no,resizable=no,titlebar,toolbar=no";
      Services.ww.openWindow(null, URI_UPDATE_PROMPT_DIALOG, "", openFeatures, ary);
      window.close();
      return;
    }

    this.selectPanel("checkingForUpdates");
    this.isChecking = true;
    this.checker.checkForUpdates(this.updateCheckListener, true);
  },

  /**
   * Implements nsIUpdateCheckListener. The methods implemented by
   * nsIUpdateCheckListener are in a different scope from nsIIncrementalDownload
   * to make it clear which are used by each interface.
   */
  updateCheckListener: {
    /**
     * See nsIUpdateService.idl
     */
    onCheckComplete: function(aRequest, aUpdates, aUpdateCount) {
      gAppUpdater.isChecking = false;
      gAppUpdater.update = gAppUpdater.aus.
                           selectUpdate(aUpdates, aUpdates.length);
      if (!gAppUpdater.update) {
        gAppUpdater.selectPanel("noUpdatesFound");
        return;
      }

      if (!gAppUpdater.aus.canApplyUpdates) {
        gAppUpdater.selectPanel("manualUpdate");
        return;
      }

      // Firefox no longer displays a license for updates and the licenseURL
      // check is just in case a distibution does.
      if (gAppUpdater.update.billboardURL || gAppUpdater.update.licenseURL) {
        gAppUpdater.selectPanel("updateButtonBox");
        gAppUpdater.setupUpdateButton("update.openUpdateUI." +
                                      (this.isMajor ? "upgradeButton"
                                                    : "applyButton"));
        return;
      }

      if (!gAppUpdater.update.appVersion ||
          Services.vc.compare(gAppUpdater.update.appVersion,
                              Services.appinfo.version) == 0) {
        gAppUpdater.startDownload();
        return;
      }

      gAppUpdater.checkAddonCompatibility();
    },

    /**
     * See nsIUpdateService.idl
     */
    onError: function(aRequest, aUpdate) {
      // Errors in the update check are treated as no updates found. If the
      // update check fails repeatedly without a success the user will be
      // notified with the normal app update user interface so this is safe.
      gAppUpdater.isChecking = false;
      gAppUpdater.selectPanel("noUpdatesFound");
    },

    /**
     * See nsISupports.idl
     */
    QueryInterface: function(aIID) {
      if (!aIID.equals(Components.interfaces.nsIUpdateCheckListener) &&
          !aIID.equals(Components.interfaces.nsISupports))
        throw Components.results.NS_ERROR_NO_INTERFACE;
      return this;
    }
  },

  /**
   * Checks the compatibility of add-ons for the application update.
   */
  checkAddonCompatibility: function() {
    try {
      var hotfixID = Services.prefs.getCharPref(PREF_EM_HOTFIX_ID);
    }
    catch (e) { }

    var self = this;
    AddonManager.getAllAddons(function(aAddons) {
      self.addons = [];
      self.addonsCheckedCount = 0;
      aAddons.forEach(function(aAddon) {
        // Protect against code that overrides the add-ons manager and doesn't
        // implement the isCompatibleWith or the findUpdates method.
        if (!("isCompatibleWith" in aAddon) || !("findUpdates" in aAddon)) {
          let errMsg = "Add-on doesn't implement either the isCompatibleWith " +
                       "or the findUpdates method!";
          if (aAddon.id)
            errMsg += " Add-on ID: " + aAddon.id;
          Components.utils.reportError(errMsg);
          return;
        }

        // If an add-on isn't appDisabled and isn't userDisabled then it is
        // either active now or the user expects it to be active after the
        // restart. If that is the case and the add-on is not installed by the
        // application and is not compatible with the new application version
        // then the user should be warned that the add-on will become
        // incompatible. If an addon's type equals plugin it is skipped since
        // checking plugins compatibility information isn't supported and
        // getting the scope property of a plugin breaks in some environments
        // (see bug 566787). The hotfix add-on is also ignored as it shouldn't
        // block the user from upgrading.
        try {
          if (aAddon.type != "plugin" && aAddon.id != hotfixID &&
              !aAddon.appDisabled && !aAddon.userDisabled &&
              aAddon.scope != AddonManager.SCOPE_APPLICATION &&
              aAddon.isCompatible &&
              !aAddon.isCompatibleWith(self.update.appVersion,
                                       self.update.platformVersion))
            self.addons.push(aAddon);
        }
        catch (e) {
          Components.utils.reportError(e);
        }
      });
      self.addonsTotalCount = self.addons.length;
      if (self.addonsTotalCount == 0) {
        self.startDownload();
        return;
      }

      self.checkAddonsForUpdates();
    });
  },

  /**
   * Checks if there are updates for add-ons that are incompatible with the
   * application update.
   */
  checkAddonsForUpdates: function() {
    this.addons.forEach(function(aAddon) {
      aAddon.findUpdates(this, AddonManager.UPDATE_WHEN_NEW_APP_DETECTED,
                         this.update.appVersion,
                         this.update.platformVersion);
    }, this);
  },

  /**
   * See XPIProvider.jsm
   */
  onCompatibilityUpdateAvailable: function(aAddon) {
    for (var i = 0; i < this.addons.length; ++i) {
      if (this.addons[i].id == aAddon.id) {
        this.addons.splice(i, 1);
        break;
      }
    }
  },

  /**
   * See XPIProvider.jsm
   */
  onUpdateAvailable: function(aAddon, aInstall) {
    if (!Services.blocklist.isAddonBlocklisted(aAddon.id, aInstall.version,
                                               this.update.appVersion,
                                               this.update.platformVersion)) {
      // Compatibility or new version updates mean the same thing here.
      this.onCompatibilityUpdateAvailable(aAddon);
    }
  },

  /**
   * See XPIProvider.jsm
   */
  onUpdateFinished: function(aAddon) {
    ++this.addonsCheckedCount;

    if (this.addonsCheckedCount < this.addonsTotalCount)
      return;

    if (this.addons.length == 0) {
      // Compatibility updates or new version updates were found for all add-ons
      this.startDownload();
      return;
    }

    this.selectPanel("updateButtonBox");
    this.setupUpdateButton("update.openUpdateUI." +
                           (this.isMajor ? "upgradeButton" : "applyButton"));
  },

  /**
   * Starts the download of an update mar.
   */
  startDownload: function() {
    if (!this.update)
      this.update = this.um.activeUpdate;
    this.update.QueryInterface(Components.interfaces.nsIWritablePropertyBag);
    this.update.setProperty("foregroundDownload", "true");

    this.aus.pauseDownload();
    let state = this.aus.downloadUpdate(this.update, false);
    if (state == "failed") {
      this.selectPanel("downloadFailed");
      return;
    }

    this.setupDownloadingUI();
  },

  /**
   * Switches to the UI responsible for tracking the download.
   */
  setupDownloadingUI: function() {
    this.downloadStatus = document.getElementById("downloadStatus");
    this.downloadStatus.value =
      DownloadUtils.getTransferTotal(0, this.update.selectedPatch.size);
    this.selectPanel("downloading");
    this.aus.addDownloadListener(this);
  },

  removeDownloadListener: function() {
    if (this.aus) {
      this.aus.removeDownloadListener(this);
    }
  },

  /**
   * See nsIRequestObserver.idl
   */
  onStartRequest: function(aRequest, aContext) {
  },

  /**
   * See nsIRequestObserver.idl
   */
  onStopRequest: function(aRequest, aContext, aStatusCode) {
    switch (aStatusCode) {
    case Components.results.NS_ERROR_UNEXPECTED:
      if (this.update.selectedPatch.state == "download-failed" &&
          (this.update.isCompleteUpdate || this.update.patchCount != 2)) {
        // Verification error of complete patch, informational text is held in
        // the update object.
        this.removeDownloadListener();
        this.selectPanel("downloadFailed");
        break;
      }
      // Verification failed for a partial patch, complete patch is now
      // downloading so return early and do NOT remove the download listener!
      break;
    case Components.results.NS_BINDING_ABORTED:
      // Do not remove UI listener since the user may resume downloading again.
      break;
    case Components.results.NS_OK:
      this.removeDownloadListener();
      if (this.backgroundUpdateEnabled) {
        this.selectPanel("applying");
        let update = this.um.activeUpdate;
        let self = this;
        Services.obs.addObserver(function updateStaged(aSubject, aTopic, aData) {
          // Update the UI when the background updater is finished
          let status = aData;
          if (status == "applied" || status == "applied-service" ||
              status == "pending" || status == "pending-service") {
            // If the update is successfully applied, or if the updater has
            // fallen back to non-staged updates, show the Restart to Update
            // button.
            self.selectPanel("updateButtonBox");
            self.setupUpdateButton("update.restart." +
                                   (self.isMajor ? "upgradeButton" : "updateButton"));
          } else if (status == "failed") {
            // Background update has failed, let's show the UI responsible for
            // prompting the user to update manually.
            self.selectPanel("downloadFailed");
          } else if (status == "downloading") {
            // We've fallen back to downloading the full update because the
            // partial update failed to get staged in the background.
            // Therefore we need to keep our observer.
            self.setupDownloadingUI();
            return;
          }
          Services.obs.removeObserver(updateStaged, "update-staged");
        }, "update-staged", false);
      } else {
        this.selectPanel("updateButtonBox");
        this.setupUpdateButton("update.restart." +
                               (this.isMajor ? "upgradeButton" : "updateButton"));
      }
      break;
    default:
      this.removeDownloadListener();
      this.selectPanel("downloadFailed");
      break;
    }

  },

  /**
   * See nsIProgressEventSink.idl
   */
  onStatus: function(aRequest, aContext, aStatus, aStatusArg) {
  },

  /**
   * See nsIProgressEventSink.idl
   */
  onProgress: function(aRequest, aContext, aProgress, aProgressMax) {
    this.downloadStatus.value =
      DownloadUtils.getTransferTotal(aProgress, aProgressMax);
  },

  /**
   * See nsISupports.idl
   */
  QueryInterface: function(aIID) {
    if (!aIID.equals(Components.interfaces.nsIProgressEventSink) &&
        !aIID.equals(Components.interfaces.nsIRequestObserver) &&
        !aIID.equals(Components.interfaces.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};
#endif
