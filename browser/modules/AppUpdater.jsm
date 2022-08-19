/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AppUpdater"];

var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

var gLogfileOutputStream;

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);
const PREF_APP_UPDATE_LOG = "app.update.log";
const PREF_APP_UPDATE_LOG_FILE = "app.update.log.file";
const KEY_PROFILE_DIR = "ProfD";
const FILE_UPDATE_MESSAGES = "update_messages.log";
const lazy = {};
XPCOMUtils.defineLazyModuleGetters(lazy, {
  UpdateUtils: "resource://gre/modules/UpdateUtils.jsm",
});
XPCOMUtils.defineLazyGetter(lazy, "gLogEnabled", function aus_gLogEnabled() {
  return (
    Services.prefs.getBoolPref(PREF_APP_UPDATE_LOG, false) ||
    Services.prefs.getBoolPref(PREF_APP_UPDATE_LOG_FILE, false)
  );
});
XPCOMUtils.defineLazyGetter(
  lazy,
  "gLogfileEnabled",
  function aus_gLogfileEnabled() {
    return Services.prefs.getBoolPref(PREF_APP_UPDATE_LOG_FILE, false);
  }
);

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
    this.QueryInterface = ChromeUtils.generateQI([
      "nsIObserver",
      "nsIProgressEventSink",
      "nsIRequestObserver",
      "nsISupportsWeakReference",
    ]);
    Services.obs.addObserver(this, "update-swap", /* ownsWeak */ true);

    // This one call observes PREF_APP_UPDATE_LOG and PREF_APP_UPDATE_LOG_FILE
    Services.prefs.addObserver(PREF_APP_UPDATE_LOG, this);
  }

  /**
   * The main entry point for checking for updates.  As different stages of the
   * check and possible subsequent update occur, the updater's status is set and
   * listeners are called.
   */
  check() {
    if (!AppConstants.MOZ_UPDATER || this.updateDisabledByPackage) {
      LOG(
        "AppUpdater:check -" +
          "AppConstants.MOZ_UPDATER=" +
          AppConstants.MOZ_UPDATER +
          "this.updateDisabledByPackage: " +
          this.updateDisabledByPackage
      );
      this._setStatus(AppUpdater.STATUS.NO_UPDATER);
      return;
    }

    if (this.updateDisabledByPolicy) {
      LOG("AppUpdater:check - this.updateDisabledByPolicy");
      this._setStatus(AppUpdater.STATUS.UPDATE_DISABLED_BY_POLICY);
      return;
    }

    if (this.isReadyForRestart) {
      LOG("AppUpdater:check - this.isReadyForRestart");
      this._setStatus(AppUpdater.STATUS.READY_FOR_RESTART);
      return;
    }

    if (this.aus.isOtherInstanceHandlingUpdates) {
      LOG("AppUpdater:check - this.aus.isOtherInstanceHandlingUpdates");
      this._setStatus(AppUpdater.STATUS.OTHER_INSTANCE_HANDLING_UPDATES);
      return;
    }

    if (this.isDownloading) {
      LOG("AppUpdater:check - this.isDownloading");
      this.startDownload();
      return;
    }

    if (this.isStaging) {
      LOG("AppUpdater:check - this.isStaging");
      this._waitForUpdateToStage();
      return;
    }

    // We might need this value later, so start loading it from the disk now.
    this.promiseAutoUpdateSetting = lazy.UpdateUtils.getAppUpdateAutoEnabled();

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
      this.um.readyUpdate &&
      (this.um.readyUpdate.state == "pending" ||
        this.um.readyUpdate.state == "pending-service" ||
        this.um.readyUpdate.state == "pending-elevate")
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
      this.um.readyUpdate &&
      (this.um.readyUpdate.state == "applied" ||
        this.um.readyUpdate.state == "applied-service")
    );
  }

  get isStaging() {
    if (!this.updateStagingEnabled) {
      return false;
    }
    let errorCode;
    if (this.update) {
      errorCode = this.update.errorCode;
    } else if (this.um.readyUpdate) {
      errorCode = this.um.readyUpdate.errorCode;
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
      } else if (this.um.readyUpdate) {
        errorCode = this.um.readyUpdate.errorCode;
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
    return (
      this.um.downloadingUpdate &&
      this.um.downloadingUpdate.state == "downloading"
    );
  }

  // true when updating has been disabled by enterprise policy
  get updateDisabledByPolicy() {
    return Services.policies && !Services.policies.isAllowed("appUpdate");
  }

  // true if updating is disabled because we're running in an app package.
  // This is distinct from updateDisabledByPolicy because we need to avoid
  // messages being shown to the user about an "administrator" handling
  // updates; packaged apps may be getting updated by an administrator or they
  // may not be, and we don't have a good way to tell the difference from here,
  // so we err to the side of less confusion for unmanaged users.
  get updateDisabledByPackage() {
    try {
      return Services.sysinfo.getProperty("hasWinPackageId");
    } catch (_ex) {
      // The hasWinPackageId property doesn't exist; assume it would be false.
    }
    return false;
  }

  // true when updating in background is enabled.
  get updateStagingEnabled() {
    LOG(
      "AppUpdater:updateStagingEnabled" +
        "canStageUpdates: " +
        this.aus.canStageUpdates
    );
    return (
      !this.updateDisabledByPolicy &&
      !this.updateDisabledByPackage &&
      this.aus.canStageUpdates
    );
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
    LOG("AppUpdater:checkForUpdates - waiting for onCheckComplete()");
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
        onCheckComplete: async (aRequest, aUpdates) => {
          LOG("AppUpdater:_updateCheckListener:onCheckComplete - reached.");
          this.update = this.aus.selectUpdate(aUpdates);
          if (!this.update) {
            LOG(
              "AppUpdater:_updateCheckListener:onCheckComplete - result: " +
                "NO_UPDATES_FOUND"
            );
            this._setStatus(AppUpdater.STATUS.NO_UPDATES_FOUND);
            return;
          }

          if (this.update.unsupported) {
            LOG(
              "AppUpdater:_updateCheckListener:onCheckComplete - result: " +
                "UNSUPPORTED SYSTEM"
            );
            this._setStatus(AppUpdater.STATUS.UNSUPPORTED_SYSTEM);
            return;
          }

          if (!this.aus.canApplyUpdates) {
            LOG(
              "AppUpdater:_updateCheckListener:onCheckComplete - result: " +
                "MANUAL_UPDATE"
            );
            this._setStatus(AppUpdater.STATUS.MANUAL_UPDATE);
            return;
          }

          if (!this.promiseAutoUpdateSetting) {
            this.promiseAutoUpdateSetting = lazy.UpdateUtils.getAppUpdateAutoEnabled();
          }
          this.promiseAutoUpdateSetting.then(updateAuto => {
            if (updateAuto && !this.aus.manualUpdateOnly) {
              LOG(
                "AppUpdater:_updateCheckListener:onCheckComplete - " +
                  "updateAuto is active and " +
                  "manualUpdateOnlydateOnly is inactive." +
                  "start the download."
              );
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
        onError: async (aRequest, aUpdate) => {
          // Errors in the update check are treated as no updates found. If the
          // update check fails repeatedly without a success the user will be
          // notified with the normal app update user interface so this is safe.
          LOG("AppUpdater:_updateCheckListener:onError: NO_UPDATES_FOUND");
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
      this.update = this.um.readyUpdate;
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
      this.update = this.um.downloadingUpdate;
    }
    this.update.QueryInterface(Ci.nsIWritablePropertyBag);
    this.update.setProperty("foregroundDownload", "true");

    let success = this.aus.downloadUpdate(this.update, false);
    if (!success) {
      LOG("AppUpdater:startDownload - downloadUpdate failed.");
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
    LOG("AppUpdater:_setupDownloadListener - registered a download listener");
  }

  /**
   * See nsIRequestObserver.idl
   */
  onStartRequest(aRequest) {
    LOG("AppUpdater:onStartRequest - aRequest: " + aRequest);
  }

  /**
   * See nsIRequestObserver.idl
   */
  onStopRequest(aRequest, aStatusCode) {
    LOG(
      "AppUpdater:onStopRequest " +
        "- aRequest: " +
        aRequest +
        ", aStatusCode: " +
        aStatusCode
    );
    switch (aStatusCode) {
      case Cr.NS_ERROR_UNEXPECTED:
        if (
          this.update.selectedPatch.state == "download-failed" &&
          (this.update.isCompleteUpdate || this.update.patchCount != 2)
        ) {
          // Verification error of complete patch, informational text is held in
          // the update object.
          this.aus.removeDownloadListener(this);
          LOG(
            "AppUpdater:onStopRequest " +
              "- download failed with unexpected error" +
              ", removed download listener"
          );
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
        LOG(
          "AppUpdater:onStopRequest " +
            "- download ok" +
            ", removed download listener"
        );
        if (this.updateStagingEnabled) {
          // It could be that another instance was started during the download,
          // and if that happened, then we actually should not advance to the
          // STAGING status because the staging process isn't really happening
          // until that instance exits (or we time out waiting).
          if (this.aus.isOtherInstanceHandlingUpdates) {
            LOG(
              "AppUpdater:onStopRequest " +
                "- aStatusCode=Cr.NS_OK" +
                ", another instance is handling updates"
            );
            this._setStatus(AppUpdater.OTHER_INSTANCE_HANDLING_UPDATES);
          } else {
            LOG(
              "AppUpdater:onStopRequest " +
                "- aStatusCode=Cr.NS_OK" +
                ", no competitive instance found."
            );
            this._setStatus(AppUpdater.STATUS.STAGING);
          }
          // But we should register the staging observer in either case, because
          // if we do time out waiting for the other instance to exit, then
          // staging really will start at that point.
          this._awaitStagingComplete();
        } else {
          this._awaitDownloadComplete();
        }
        break;
      default:
        this.aus.removeDownloadListener(this);
        LOG(
          "AppUpdater:onStopRequest " +
            "- case default" +
            ", removing download listener" +
            ", because the download failed."
        );
        this._setStatus(AppUpdater.STATUS.DOWNLOAD_FAILED);
        break;
    }
  }

  /**
   * See nsIProgressEventSink.idl
   */
  onStatus(aRequest, aStatus, aStatusArg) {
    LOG(
      "AppUpdater:onStatus " +
        "- aRequest: " +
        aRequest +
        ", aStatus: " +
        aStatus +
        ", aStatusArg: " +
        aStatusArg
    );
  }

  /**
   * See nsIProgressEventSink.idl
   */
  onProgress(aRequest, aProgress, aProgressMax) {
    LOG(
      "AppUpdater:onProgress " +
        "- aRequest: " +
        aRequest +
        ", aProgress: " +
        aProgress +
        ", aProgressMax: " +
        aProgressMax
    );
    this._setStatus(AppUpdater.STATUS.DOWNLOADING, aProgress, aProgressMax);
  }

  /**
   * This function registers an observer that watches for the download
   * to complete. Once it does, it updates the status accordingly.
   */
  _awaitDownloadComplete() {
    let observer = (aSubject, aTopic, aData) => {
      // Update the UI when the download is finished
      LOG(
        "AppUpdater:_awaitStagingComplete - observer reached" +
          ", status changes to READY_FOR_RESTART"
      );
      this._setStatus(AppUpdater.STATUS.READY_FOR_RESTART);
      Services.obs.removeObserver(observer, "update-downloaded");
    };
    Services.obs.addObserver(observer, "update-downloaded");
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
      LOG(
        "AppUpdater:_awaitStagingComplete:observer" +
          "- aSubject: " +
          aSubject +
          "- aTopic: " +
          aTopic +
          "- aData (=status): " +
          aData
      );
      // Update the UI when the background updater is finished
      switch (aTopic) {
        case "update-staged":
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
          break;
        case "update-error":
          this._setStatus(AppUpdater.STATUS.DOWNLOAD_FAILED);
          break;
      }
      Services.obs.removeObserver(observer, "update-staged");
      Services.obs.removeObserver(observer, "update-error");
    };
    Services.obs.addObserver(observer, "update-staged");
    Services.obs.addObserver(observer, "update-error");
  }

  /**
   * Stops the current check for updates and any ongoing download.
   */
  stop() {
    LOG("AppUpdater:stop called, remove download listener");
    this.checker.stopCurrentCheck();
    this.aus.removeDownloadListener(this);
  }

  /**
   * {AppUpdater.STATUS} The status of the current check or update.
   */
  get status() {
    if (!this._status) {
      if (!AppConstants.MOZ_UPDATER || this.updateDisabledByPackage) {
        LOG("AppUpdater:status - no updater or updates disabled by package.");
        this._status = AppUpdater.STATUS.NO_UPDATER;
      } else if (this.updateDisabledByPolicy) {
        LOG("AppUpdater:status - updateDisabledByPolicy");
        this._status = AppUpdater.STATUS.UPDATE_DISABLED_BY_POLICY;
      } else if (this.isReadyForRestart) {
        LOG("AppUpdater:status - isReadyForRestart");
        this._status = AppUpdater.STATUS.READY_FOR_RESTART;
      } else if (this.aus.isOtherInstanceHandlingUpdates) {
        LOG("AppUpdater:status - another instance is handling updates");
        this._status = AppUpdater.STATUS.OTHER_INSTANCE_HANDLING_UPDATES;
      } else if (this.isDownloading) {
        LOG("AppUpdater:status - isDownloading");
        this._status = AppUpdater.STATUS.DOWNLOADING;
      } else if (this.isStaging) {
        LOG("AppUpdater:status - isStaging");
        this._status = AppUpdater.STATUS.STAGING;
      } else {
        LOG("AppUpdater:status - NEVER_CHECKED");
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

  observe(subject, topic, status) {
    LOG(
      "AppUpdater:observe " +
        "- subject: " +
        subject +
        ", topic: " +
        topic +
        ", status: " +
        status
    );
    switch (topic) {
      case "update-swap":
        this._handleUpdateSwap();
        break;
      case "nsPref:changed":
        if (
          status == PREF_APP_UPDATE_LOG ||
          status == PREF_APP_UPDATE_LOG_FILE
        ) {
          lazy.gLogEnabled; // Assigning this before it is lazy-loaded is an error.
          lazy.gLogEnabled =
            Services.prefs.getBoolPref(PREF_APP_UPDATE_LOG, false) ||
            Services.prefs.getBoolPref(PREF_APP_UPDATE_LOG_FILE, false);
        }
        break;
      case "quit-application":
        Services.prefs.removeObserver(PREF_APP_UPDATE_LOG, this);
        Services.obs.removeObserver(this, topic);
    }
  }

  _handleUpdateSwap() {
    // This function exists to deal with the fact that we support handling 2
    // updates at once: a ready update and a downloading update. But AppUpdater
    // only ever really considers a single update at a time.
    // We see an update swap just when the downloading update has finished
    // downloading and is being swapped into UpdateManager.readyUpdate. At this
    // point, we are in one of two states. Either:
    //  a) The update that is being swapped in is the update that this
    //     AppUpdater has already been tracking, or
    //  b) We've been tracking the ready update. Now that the downloading
    //     update is about to be swapped into the place of the ready update, we
    //     need to switch over to tracking the new update.
    if (
      this._status == AppUpdater.STATUS.DOWNLOADING ||
      this._status == AppUpdater.STATUS.STAGING
    ) {
      // We are already tracking the correct update.
      return;
    }

    if (this.updateStagingEnabled) {
      LOG("AppUpdater:_handleUpdateSwap - updateStagingEnabled");
      this._setStatus(AppUpdater.STATUS.STAGING);
      this._awaitStagingComplete();
    } else {
      LOG("AppUpdater:_handleUpdateSwap - updateStagingDisabled");
      this._setStatus(AppUpdater.STATUS.DOWNLOADING);
      this._awaitDownloadComplete();
    }
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

  /**
   * Is the given `status` a terminal state in the update state machine?
   *
   * A terminal state means that the `check()` method has completed.
   *
   * N.b.: `DOWNLOAD_AND_INSTALL` is not considered terminal because the normal
   * flow is that Firefox will show UI prompting the user to install, and when
   * the user interacts, the `check()` method will continue through the update
   * state machine.
   *
   * @returns {boolean} `true` if `status` is terminal.
   */
  isTerminalStatus(status) {
    return ![
      AppUpdater.STATUS.CHECKING,
      AppUpdater.STATUS.DOWNLOAD_AND_INSTALL,
      AppUpdater.STATUS.DOWNLOADING,
      AppUpdater.STATUS.NEVER_CHECKED,
      AppUpdater.STATUS.STAGING,
    ].includes(status);
  },

  /**
   * Turn the given `status` into a string for debugging.
   *
   * @returns {?string} representation of given numerical `status`.
   */
  debugStringFor(status) {
    for (let [k, v] of Object.entries(AppUpdater.STATUS)) {
      if (v == status) {
        return k;
      }
    }
    return null;
  },
};

/**
 * Logs a string to the error console. If enabled, also logs to the update
 * messages file.
 * @param   string
 *          The string to write to the error console.
 */
function LOG(string) {
  if (lazy.gLogEnabled) {
    dump("*** AUS:AUM " + string + "\n");
    if (!Cu.isInAutomation) {
      Services.console.logStringMessage("AUS:AUM " + string);
    }

    if (lazy.gLogfileEnabled) {
      if (!gLogfileOutputStream) {
        let logfile = Services.dirsvc.get(KEY_PROFILE_DIR, Ci.nsIFile);
        logfile.append(FILE_UPDATE_MESSAGES);
        gLogfileOutputStream = FileUtils.openAtomicFileOutputStream(logfile);
      }

      try {
        let encoded = new TextEncoder().encode(string + "\n");
        gLogfileOutputStream.write(encoded, encoded.length);
        gLogfileOutputStream.flush();
      } catch (e) {
        dump("*** AUS:AUM Unable to write to messages file: " + e + "\n");
        Services.console.logStringMessage(
          "AUS:AUM Unable to write to messages file: " + e
        );
      }
    }
  }
}
