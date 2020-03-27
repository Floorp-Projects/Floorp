/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AppUpdater"];

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  Services: "resource://gre/modules/Services.jsm",
  UpdateUtils: "resource://gre/modules/UpdateUtils.jsm",
});

const PREF_APP_UPDATE_CANCELATIONS_OSX = "app.update.cancelations.osx";
const PREF_APP_UPDATE_ELEVATE_NEVER = "app.update.elevate.never";

/**
 * This class checks for app updates in the foreground.  It has several public
 * methods for checking for updates, downloading updates, stopping the current
 * update, and getting the current update status.  It can also register
 * listeners that will be called back as different stages of updates occur.
 */
class AppUpdater {
  constructor() {
    this._listeners = new Set();
    XPCOMUtils.defineLazyServiceGetter(
      this,
      "aus",
      "@mozilla.org/updates/update-service;1",
      "nsIApplicationUpdateService"
    );
    XPCOMUtils.defineLazyServiceGetter(
      this,
      "checker",
      "@mozilla.org/updates/update-checker;1",
      "nsIUpdateChecker"
    );
    XPCOMUtils.defineLazyServiceGetter(
      this,
      "um",
      "@mozilla.org/updates/update-manager;1",
      "nsIUpdateManager"
    );
  }

  /**
   * The main entry point for checking for updates.  As different stages of the
   * check and possible subsequent update occur, the updater's status is set and
   * listeners are called.
   */
  check() {
    if (!AppConstants.MOZ_UPDATER) {
      this._setStatus(AppUpdater.STATUS.NO_UPDATER);
      return;
    }

    if (this.updateDisabledByPolicy) {
      this._setStatus(AppUpdater.STATUS.UPDATE_DISABLED_BY_POLICY);
      return;
    }

    if (this.isReadyForRestart) {
      this._setStatus(AppUpdater.STATUS.READY_FOR_RESTART);
      return;
    }

    if (this.aus.isOtherInstanceHandlingUpdates) {
      this._setStatus(AppUpdater.STATUS.OTHER_INSTANCE_HANDLING_UPDATES);
      return;
    }

    if (this.isDownloading) {
      this.startDownload();
      return;
    }

    if (this.isStaging) {
      this._waitForUpdateToStage();
      return;
    }

    // We might need this value later, so start loading it from the disk now.
    this.promiseAutoUpdateSetting = UpdateUtils.getAppUpdateAutoEnabled();

    // That leaves the options
    // "Check for updates, but let me choose whether to install them", and
    // "Automatically install updates".
    // In both cases, we check for updates without asking.
    // In the "let me choose" case, we ask before downloading though, in onCheckComplete.
    this.checkForUpdates();
  }

  // true when there is an update ready to be applied on restart or staged.
  get isPending() {
    if (this.update) {
      return (
        this.update.state == "pending" ||
        this.update.state == "pending-service" ||
        this.update.state == "pending-elevate"
      );
    }
    return (
      this.um.activeUpdate &&
      (this.um.activeUpdate.state == "pending" ||
        this.um.activeUpdate.state == "pending-service" ||
        this.um.activeUpdate.state == "pending-elevate")
    );
  }

  // true when there is an update already staged.
  get isApplied() {
    if (this.update) {
      return (
        this.update.state == "applied" || this.update.state == "applied-service"
      );
    }
    return (
      this.um.activeUpdate &&
      (this.um.activeUpdate.state == "applied" ||
        this.um.activeUpdate.state == "applied-service")
    );
  }

  get isStaging() {
    if (!this.updateStagingEnabled) {
      return false;
    }
    let errorCode;
    if (this.update) {
      errorCode = this.update.errorCode;
    } else if (this.um.activeUpdate) {
      errorCode = this.um.activeUpdate.errorCode;
    }
    // If the state is pending and the error code is not 0, staging must have
    // failed.
    return this.isPending && errorCode == 0;
  }

  // true when an update ready to restart to finish the update process.
  get isReadyForRestart() {
    if (this.updateStagingEnabled) {
      let errorCode;
      if (this.update) {
        errorCode = this.update.errorCode;
      } else if (this.um.activeUpdate) {
        errorCode = this.um.activeUpdate.errorCode;
      }
      // If the state is pending and the error code is not 0, staging must have
      // failed and Firefox should be restarted to try to apply the update
      // without staging.
      return this.isApplied || (this.isPending && errorCode != 0);
    }
    return this.isPending;
  }

  // true when there is an update download in progress.
  get isDownloading() {
    if (this.update) {
      return this.update.state == "downloading";
    }
    return this.um.activeUpdate && this.um.activeUpdate.state == "downloading";
  }

  // true when updating has been disabled by enterprise policy
  get updateDisabledByPolicy() {
    return Services.policies && !Services.policies.isAllowed("appUpdate");
  }

  // true when updating in background is enabled.
  get updateStagingEnabled() {
    return !this.updateDisabledByPolicy && this.aus.canStageUpdates;
  }

  /**
   * Check for updates
   */
  checkForUpdates() {
    // Clear prefs that could prevent a user from discovering available updates.
    if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_CANCELATIONS_OSX)) {
      Services.prefs.clearUserPref(PREF_APP_UPDATE_CANCELATIONS_OSX);
    }
    if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_ELEVATE_NEVER)) {
      Services.prefs.clearUserPref(PREF_APP_UPDATE_ELEVATE_NEVER);
    }
    this._setStatus(AppUpdater.STATUS.CHECKING);
    this.checker.checkForUpdates(this._updateCheckListener, true);
    // after checking, onCheckComplete() is called
  }

  /**
   * Implements nsIUpdateCheckListener. The methods implemented by
   * nsIUpdateCheckListener are in a different scope from nsIIncrementalDownload
   * to make it clear which are used by each interface.
   */
  get _updateCheckListener() {
    if (!this.__updateCheckListener) {
      this.__updateCheckListener = {
        /**
         * See nsIUpdateService.idl
         */
        onCheckComplete: (aRequest, aUpdates) => {
          this.update = this.aus.selectUpdate(aUpdates);
          if (!this.update) {
            this._setStatus(AppUpdater.STATUS.NO_UPDATES_FOUND);
            return;
          }

          if (this.update.unsupported) {
            this._setStatus(AppUpdater.STATUS.UNSUPPORTED_SYSTEM);
            return;
          }

          if (!this.aus.canApplyUpdates) {
            this._setStatus(AppUpdater.STATUS.MANUAL_UPDATE);
            return;
          }

          if (!this.promiseAutoUpdateSetting) {
            this.promiseAutoUpdateSetting = UpdateUtils.getAppUpdateAutoEnabled();
          }
          this.promiseAutoUpdateSetting.then(updateAuto => {
            if (updateAuto) {
              // automatically download and install
              this.startDownload();
            } else {
              // ask
              this._setStatus(AppUpdater.STATUS.DOWNLOAD_AND_INSTALL);
            }
          });
        },

        /**
         * See nsIUpdateService.idl
         */
        onError: (aRequest, aUpdate) => {
          // Errors in the update check are treated as no updates found. If the
          // update check fails repeatedly without a success the user will be
          // notified with the normal app update user interface so this is safe.
          this._setStatus(AppUpdater.STATUS.NO_UPDATES_FOUND);
        },

        /**
         * See nsISupports.idl
         */
        QueryInterface: ChromeUtils.generateQI(["nsIUpdateCheckListener"]),
      };
    }
    return this.__updateCheckListener;
  }

  /**
   * Sets the status to STAGING.  The status will then be set again when the
   * update finishes staging.
   */
  _waitForUpdateToStage() {
    if (!this.update) {
      this.update = this.um.activeUpdate;
    }
    this.update.QueryInterface(Ci.nsIWritablePropertyBag);
    this.update.setProperty("foregroundDownload", "true");
    this._setStatus(AppUpdater.STATUS.STAGING);
    this._awaitStagingComplete();
  }

  /**
   * Starts the download of an update mar.
   */
  startDownload() {
    if (!this.update) {
      this.update = this.um.activeUpdate;
    }
    this.update.QueryInterface(Ci.nsIWritablePropertyBag);
    this.update.setProperty("foregroundDownload", "true");

    let state = this.aus.downloadUpdate(this.update, false);
    if (state == "failed") {
      this._setStatus(AppUpdater.STATUS.DOWNLOAD_FAILED);
      return;
    }

    this._setupDownloadListener();
  }

  /**
   * Starts tracking the download.
   */
  _setupDownloadListener() {
    this._setStatus(AppUpdater.STATUS.DOWNLOADING);
    this.aus.addDownloadListener(this);
  }

  /**
   * See nsIRequestObserver.idl
   */
  onStartRequest(aRequest) {}

  /**
   * See nsIRequestObserver.idl
   */
  onStopRequest(aRequest, aStatusCode) {
    switch (aStatusCode) {
      case Cr.NS_ERROR_UNEXPECTED:
        if (
          this.update.selectedPatch.state == "download-failed" &&
          (this.update.isCompleteUpdate || this.update.patchCount != 2)
        ) {
          // Verification error of complete patch, informational text is held in
          // the update object.
          this.aus.removeDownloadListener(this);
          this._setStatus(AppUpdater.STATUS.DOWNLOAD_FAILED);
          break;
        }
        // Verification failed for a partial patch, complete patch is now
        // downloading so return early and do NOT remove the download listener!
        break;
      case Cr.NS_BINDING_ABORTED:
        // Do not remove UI listener since the user may resume downloading again.
        break;
      case Cr.NS_OK:
        this.aus.removeDownloadListener(this);
        if (this.updateStagingEnabled) {
          this._setStatus(AppUpdater.STATUS.STAGING);
          this._awaitStagingComplete();
        } else {
          this._setStatus(AppUpdater.STATUS.READY_FOR_RESTART);
        }
        break;
      default:
        this.aus.removeDownloadListener(this);
        this._setStatus(AppUpdater.STATUS.DOWNLOAD_FAILED);
        break;
    }
  }

  /**
   * See nsIProgressEventSink.idl
   */
  onStatus(aRequest, aContext, aStatus, aStatusArg) {}

  /**
   * See nsIProgressEventSink.idl
   */
  onProgress(aRequest, aContext, aProgress, aProgressMax) {
    this._setStatus(AppUpdater.STATUS.DOWNLOADING, aProgress, aProgressMax);
  }

  /**
   * This function registers an observer that watches for the staging process
   * to complete. Once it does, it sets the status to either request that the
   * user restarts to install the update on success, request that the user
   * manually download and install the newer version, or automatically download
   * a complete update if applicable.
   */
  _awaitStagingComplete() {
    let observer = (aSubject, aTopic, aData) => {
      // Update the UI when the background updater is finished
      let status = aData;
      if (
        status == "applied" ||
        status == "applied-service" ||
        status == "pending" ||
        status == "pending-service" ||
        status == "pending-elevate"
      ) {
        // If the update is successfully applied, or if the updater has
        // fallen back to non-staged updates, show the "Restart to Update"
        // button.
        this._setStatus(AppUpdater.STATUS.READY_FOR_RESTART);
      } else if (status == "failed") {
        // Background update has failed, let's show the UI responsible for
        // prompting the user to update manually.
        this._setStatus(AppUpdater.STATUS.DOWNLOAD_FAILED);
      } else if (status == "downloading") {
        // We've fallen back to downloading the complete update because the
        // partial update failed to get staged in the background.
        // Therefore we need to keep our observer.
        this._setupDownloadListener();
        return;
      }
      Services.obs.removeObserver(observer, "update-staged");
    };
    Services.obs.addObserver(observer, "update-staged");
  }

  /**
   * Stops the current check for updates and any ongoing download.
   */
  stop() {
    this.checker.stopCurrentCheck();
    this.aus.removeDownloadListener(this);
  }

  /**
   * {AppUpdater.STATUS} The status of the current check or update.
   */
  get status() {
    if (!this._status) {
      if (!AppConstants.MOZ_UPDATER) {
        this._status = AppUpdater.STATUS.NO_UPDATER;
      } else if (this.updateDisabledByPolicy) {
        this._status = AppUpdater.STATUS.UPDATE_DISABLED_BY_POLICY;
      } else if (this.isReadyForRestart) {
        this._status = AppUpdater.STATUS.READY_FOR_RESTART;
      } else if (this.aus.isOtherInstanceHandlingUpdates) {
        this._status = AppUpdater.STATUS.OTHER_INSTANCE_HANDLING_UPDATES;
      } else if (this.isDownloading) {
        this._status = AppUpdater.STATUS.DOWNLOADING;
      } else if (this.isStaging) {
        this._status = AppUpdater.STATUS.STAGING;
      } else {
        this._status = AppUpdater.STATUS.NEVER_CHECKED;
      }
    }
    return this._status;
  }

  /**
   * Adds a listener function that will be called back on status changes as
   * different stages of updates occur.  The function will be called without
   * arguments for most status changes; see the comments around the STATUS value
   * definitions below.  This is safe to call multiple times with the same
   * function.  It will be added only once.
   *
   * @param {function} listener
   *   The listener function to add.
   */
  addListener(listener) {
    this._listeners.add(listener);
  }

  /**
   * Removes a listener.  This is safe to call multiple times with the same
   * function, or with a function that was never added.
   *
   * @param {function} listener
   *   The listener function to remove.
   */
  removeListener(listener) {
    this._listeners.delete(listener);
  }

  /**
   * Sets the updater's current status and calls listeners.
   *
   * @param {AppUpdater.STATUS} status
   *   The new updater status.
   * @param {*} listenerArgs
   *   Arguments to pass to listeners.
   */
  _setStatus(status, ...listenerArgs) {
    this._status = status;
    for (let listener of this._listeners) {
      listener(status, ...listenerArgs);
    }
    return status;
  }
}

AppUpdater.STATUS = {
  // Updates are allowed and there's no downloaded or staged update, but the
  // AppUpdater hasn't checked for updates yet, so it doesn't know more than
  // that.
  NEVER_CHECKED: 0,

  // The updater isn't available (AppConstants.MOZ_UPDATER is falsey).
  NO_UPDATER: 1,

  // "appUpdate" is not allowed by policy.
  UPDATE_DISABLED_BY_POLICY: 2,

  // Another app instance is handling updates.
  OTHER_INSTANCE_HANDLING_UPDATES: 3,

  // There's an update, but it's not supported on this system.
  UNSUPPORTED_SYSTEM: 4,

  // The user must apply updates manually.
  MANUAL_UPDATE: 5,

  // The AppUpdater is checking for updates.
  CHECKING: 6,

  // The AppUpdater checked for updates and none were found.
  NO_UPDATES_FOUND: 7,

  // The AppUpdater is downloading an update.  Listeners are notified of this
  // status as a download starts.  They are also notified on download progress,
  // and in that case they are passed two arguments: the current download
  // progress and the total download size.
  DOWNLOADING: 8,

  // The AppUpdater tried to download an update but it failed.
  DOWNLOAD_FAILED: 9,

  // There's an update available, but the user wants us to ask them to download
  // and install it.
  DOWNLOAD_AND_INSTALL: 10,

  // An update is staging.
  STAGING: 11,

  // An update is downloaded and staged and will be applied on restart.
  READY_FOR_RESTART: 12,
};
