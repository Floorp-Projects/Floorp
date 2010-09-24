# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla Firebird about dialog.
#
# The Initial Developer of the Original Code is
# Blake Ross (blaker@netscape.com).
# Portions created by the Initial Developer are Copyright (C) 2002
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ehsan Akhgari <ehsan.akhgari@gmail.com>
#   Margaret Leibovic <margaret.leibovic@gmail.com>
#   Robert Strong <robert.bugzilla@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the LGPL or the GPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK ***** -->

// Services = object with smart getters for common XPCOM services
Components.utils.import("resource://gre/modules/Services.jsm");

function init(aEvent)
{
  if (aEvent.target != document)
    return;

  try {
    var distroId = Services.prefs.getCharPref("distribution.id");
    if (distroId) {
      var distroVersion = Services.prefs.getCharPref("distribution.version");
      var distroAbout = Services.prefs.getComplexValue("distribution.about",
        Components.interfaces.nsISupportsString);

      var distroField = document.getElementById("distribution");
      distroField.value = distroAbout;
      distroField.style.display = "block";

      var distroIdField = document.getElementById("distributionId");
      distroIdField.value = distroId + " - " + distroVersion;
      distroIdField.style.display = "block";
    }
  }
  catch (e) {
    // Pref is unset
  }

#ifdef MOZ_UPDATER
  gAppUpdater = new appUpdater();
#endif

#ifdef XP_MACOSX
  // it may not be sized at this point, and we need its width to calculate its position
  window.sizeToContent();
  window.moveTo((screen.availWidth / 2) - (window.outerWidth / 2), screen.availHeight / 5);
#endif
}

#ifdef MOZ_UPDATER
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/DownloadUtils.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");

var gAppUpdater;

function onUnload(aEvent) {
  if (gAppUpdater.isChecking)
    gAppUpdater.checker.stopChecking(Components.interfaces.nsIUpdateChecker.CURRENT_CHECK);
  // Safe to call even when there isn't a download in progress.
  gAppUpdater.removeDownloadListener();
  gAppUpdater = null;
}


function appUpdater()
{
  this.updateDeck = document.getElementById("updateDeck");

  // Hide the update deck when there is already an update window open to avoid
  // syncing issues between them.
  if (Services.wm.getMostRecentWindow("Update:Wizard")) {
    this.updateDeck.hidden = true;
    return;
  }

  XPCOMUtils.defineLazyServiceGetter(this, "aus",
                                     "@mozilla.org/updates/update-service;1",
                                     "nsIApplicationUpdateService");
  XPCOMUtils.defineLazyServiceGetter(this, "checker",
                                     "@mozilla.org/updates/update-checker;1",
                                     "nsIUpdateChecker");
  XPCOMUtils.defineLazyServiceGetter(this, "um",
                                     "@mozilla.org/updates/update-manager;1",
                                     "nsIUpdateManager");
  XPCOMUtils.defineLazyServiceGetter(this, "bs",
                                     "@mozilla.org/extensions/blocklist;1",
                                     "nsIBlocklistService");

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

  if (this.isPending) {
    this.setupUpdateButton("update.restart." +
                           (this.isMajor ? "upgradeButton" : "applyButton"));
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
    if (this.update)
      return this.update.state == "pending";
    return this.um.activeUpdate && this.um.activeUpdate.state == "pending";
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
    try {
      return Services.prefs.getBoolPref("app.update.enabled");
    }
    catch (e) { }
    return true; // Firefox default is true
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
        document.commandDispatcher.focusedElement.isSameNode(this.updateBtn))
      this.updateBtn.focus();
  },

  /**
   * Handles oncommand for the update button.
   */
  buttonOnCommand: function() {
    if (this.isPending) {
      // Notify all windows that an application quit has been requested.
      let cancelQuit = Components.classes["@mozilla.org/supports-PRBool;1"].
                       createInstance(Components.interfaces.nsISupportsPRBool);
      Services.obs.notifyObservers(cancelQuit, "quit-application-requested", "restart");

      // Something aborted the quit process.
      if (cancelQuit.data)
        return;

      // If already in safe mode restart in safe mode (bug 327119)
      if (Services.appinfo.inSafeMode) {
        let env = Components.classes["@mozilla.org/process/environment;1"].
                  getService(Components.interfaces.nsIEnvironment);
        env.set("MOZ_SAFE_MODE_RESTART", "1");
      }

      Components.classes["@mozilla.org/toolkit/app-startup;1"].
      getService(Components.interfaces.nsIAppStartup).
      quit(Components.interfaces.nsIAppStartup.eAttemptQuit |
           Components.interfaces.nsIAppStartup.eRestart);
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
   * nsIUpdateCheckListener have to be in a different scope from
   * nsIIncrementalDownload because both nsIUpdateCheckListener and
   * nsIIncrementalDownload implement onProgress.
   */
  updateCheckListener: {
    /**
     * See nsIUpdateService.idl
     */
    onProgress: function(aRequest, aPosition, aTotalSize) {
    },

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
      return;
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
    var self = this;
    AddonManager.getAllAddons(function(aAddons) {
      self.addons = [];
      self.addonsCheckedCount = 0;
      aAddons.forEach(function(aAddon) {
        // If an add-on isn't appDisabled and isn't userDisabled then it is
        // either active now or the user expects it to be active after the
        // restart. If that is the case and the add-on is not installed by the
        // application and is not compatible with the new application version
        // then the user should be warned that the add-on will become
        // incompatible. If an addon's type equals plugin it is skipped since
        // checking plugins compatibility information isn't supported and
        // getting the scope property of a plugin breaks in some environments
        // (see bug 566787).
        if (aAddon.type != "plugin" &&
            !aAddon.appDisabled && !aAddon.userDisabled &&
            aAddon.scope != AddonManager.SCOPE_APPLICATION &&
            aAddon.isCompatible &&
            !aAddon.isCompatibleWith(self.update.appVersion,
                                     self.update.platformVersion))
          self.addons.push(aAddon);
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
    if (!this.bs.isAddonBlocklisted(aAddon.id, aInstall.version,
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

    this.downloadStatus = document.getElementById("downloadStatus");
    this.downloadStatus.value =
      DownloadUtils.getTransferTotal(0, this.update.selectedPatch.size);
    this.selectPanel("downloading");
    this.aus.addDownloadListener(this);
  },

  removeDownloadListener: function() {
    this.aus.removeDownloadListener(this);
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
      this.selectPanel("updateButtonBox");
      this.setupUpdateButton("update.restart." +
                             (this.isMajor ? "upgradeButton" : "applyButton"));
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
