/* -*- Mode: Java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const VERBOSE = 1;
let log =
  VERBOSE ?
  function log_dump(msg) { dump("UpdatePrompt: "+ msg +"\n"); } :
  function log_noop(msg) { };

const APPLY_PROMPT_TIMEOUT =
      Services.prefs.getIntPref("b2g.update.apply-prompt-timeout");
const APPLY_IDLE_TIMEOUT =
      Services.prefs.getIntPref("b2g.update.apply-idle-timeout");
const SELF_DESTRUCT_TIMEOUT =
      Services.prefs.getIntPref("b2g.update.self-destruct-timeout");

const APPLY_IDLE_TIMEOUT_SECONDS = APPLY_IDLE_TIMEOUT / 1000;
const NETWORK_ERROR_OFFLINE = 111;

XPCOMUtils.defineLazyServiceGetter(Services, "aus",
                                   "@mozilla.org/updates/update-service;1",
                                   "nsIApplicationUpdateService");

XPCOMUtils.defineLazyServiceGetter(Services, "um",
                                   "@mozilla.org/updates/update-manager;1",
                                   "nsIUpdateManager");

XPCOMUtils.defineLazyServiceGetter(Services, "idle",
                                   "@mozilla.org/widget/idleservice;1",
                                   "nsIIdleService");

XPCOMUtils.defineLazyServiceGetter(Services, "settings",
                                   "@mozilla.org/settingsService;1",
                                   "nsISettingsService");

function UpdatePrompt() {
  this.wrappedJSObject = this;
}

UpdatePrompt.prototype = {
  classID: Components.ID("{88b3eb21-d072-4e3b-886d-f89d8c49fe59}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIUpdatePrompt,
                                         Ci.nsIUpdateCheckListener,
                                         Ci.nsIRequestObserver,
                                         Ci.nsIProgressEventSink,
                                         Ci.nsIObserver]),
  _xpcom_factory: XPCOMUtils.generateSingletonFactory(UpdatePrompt),

  _update: null,
  _applyPromptTimer: null,
  _waitingForIdle: false,

  // nsIUpdatePrompt

  // FIXME/bug 737601: we should have users opt-in to downloading
  // updates when on a billed pipe.  Initially, opt-in for 3g, but
  // that doesn't cover all cases.
  checkForUpdates: function UP_checkForUpdates() { },

  showUpdateAvailable: function UP_showUpdateAvailable(aUpdate) {
    if (!this.sendUpdateEvent("update-available", aUpdate)) {

      log("Unable to prompt for available update, forcing download");
      this.downloadUpdate(aUpdate);
    }
  },

  showUpdateDownloaded: function UP_showUpdateDownloaded(aUpdate, aBackground) {
    // The update has been downloaded and staged. We send the update-downloaded
    // event right away. After the user has been idle for a while, we send the
    // update-prompt-restart event, increasing the chances that we can apply the
    // update quietly without user intervention.
    this.sendUpdateEvent("update-downloaded", aUpdate);

    if (Services.idle.idleTime >= APPLY_IDLE_TIMEOUT) {
      this.showApplyPrompt(aUpdate);
      return;
    }

    // We haven't been idle long enough, so register an observer
    log("Update is ready to apply, registering idle timeout of " +
        APPLY_IDLE_TIMEOUT_SECONDS + " seconds before prompting.");

    this._update = aUpdate;
    this.waitForIdle();
  },

  showUpdateError: function UP_showUpdateError(aUpdate) {
    log("Update error, state: " + aUpdate.state + ", errorCode: " +
        aUpdate.errorCode);

    this.sendUpdateEvent("update-error", aUpdate);
    this.setUpdateStatus(aUpdate.statusText);
  },

  showUpdateHistory: function UP_showUpdateHistory(aParent) { },
  showUpdateInstalled: function UP_showUpdateInstalled() { },

  // nsIUpdateCheckListener

  onCheckComplete: function UP_onCheckComplete(request, updates, updateCount) {
    if (Services.um.activeUpdate) {
      return;
    }

    if (updateCount == 0) {
      this.setUpdateStatus("no-updates");
      return;
    }

    let update = Services.aus.selectUpdate(updates, updateCount);
    if (!update) {
      this.setUpdateStatus("already-latest-version");
      return;
    }

    this.setUpdateStatus("check-complete");
    this.showUpdateAvailable(update);
  },

  onError: function UP_onError(request, update) {
    if (update.errorCode == NETWORK_ERROR_OFFLINE) {
      this.setUpdateStatus("retry-when-online");
    }

    Services.aus.QueryInterface(Ci.nsIUpdateCheckListener);
    Services.aus.onError(request, update);
  },

  onProgress: function UP_onProgress(request, position, totalSize) {
    Services.aus.QueryInterface(Ci.nsIUpdateCheckListener);
    Services.aus.onProgress(request, position, totalSize);
  },

  // Custom functions

  waitForIdle: function UP_waitForIdle() {
    if (this._waitingForIdle) {
      return;
    }

    this._waitingForIdle = true;
    Services.idle.addIdleObserver(this, APPLY_IDLE_TIMEOUT_SECONDS);
    Services.obs.addObserver(this, "quit-application", false);
  },

  setUpdateStatus: function UP_setUpdateStatus(aStatus) {
    log("Setting gecko.updateStatus: " + aStatus);

    let lock = Services.settings.createLock();
    lock.set("gecko.updateStatus", aStatus, null);
  },

  showApplyPrompt: function UP_showApplyPrompt(aUpdate) {
    if (!this.sendUpdateEvent("update-prompt-apply", aUpdate)) {
      log("Unable to prompt, forcing restart");
      this.restartProcess();
      return;
    }

    // Schedule a fallback timeout in case the UI is unable to respond or show
    // a prompt for some reason.
    this._applyPromptTimer = this.createTimer(APPLY_PROMPT_TIMEOUT);
  },

  sendUpdateEvent: function UP_sendUpdateEvent(aType, aUpdate) {
    let detail = {
      displayVersion: aUpdate.displayVersion,
      detailsURL: aUpdate.detailsURL,
      statusText: aUpdate.statusText,
      state: aUpdate.state,
      errorCode: aUpdate.errorCode,
      isOSUpdate: aUpdate.isOSUpdate
    };

    let patch = aUpdate.selectedPatch;
    if (!patch && aUpdate.patchCount > 0) {
      // For now we just check the first patch to get size information if a
      // patch hasn't been selected yet.
      patch = aUpdate.getPatchAt(0);
    }

    if (patch) {
      detail.size = patch.size;
      detail.updateType = patch.type;
    } else {
      log("Warning: no patches available in update");
    }

    this._update = aUpdate;
    return this.sendChromeEvent(aType, detail);
  },

  sendChromeEvent: function UP_sendChromeEvent(aType, aDetail) {
    let browser = Services.wm.getMostRecentWindow("navigator:browser");
    if (!browser) {
      log("Warning: Couldn't send update event " + aType +
          ": no content browser");
      return false;
    }

    let detail = aDetail || {};
    detail.type = aType;

    browser.shell.sendChromeEvent(detail);
    return true;
  },

  handleAvailableResult: function UP_handleAvailableResult(aDetail) {
    // If the user doesn't choose "download", the updater will implicitly call
    // showUpdateAvailable again after a certain period of time
    switch (aDetail.result) {
      case "download":
        this.downloadUpdate(this._update);
        break;
    }
  },

  handleApplyPromptResult: function UP_handleApplyPromptResult(aDetail) {
    if (this._applyPromptTimer) {
      this._applyPromptTimer.cancel();
      this._applyPromptTimer = null;
    }

    switch (aDetail.result) {
      case "wait":
        // Wait until the user is idle before prompting to apply the update
        this.waitForIdle();
        break;
      case "restart":
        this.finishUpdate();
        this._update = null;
        break;
    }
  },

  downloadUpdate: function UP_downloadUpdate(aUpdate) {
    Services.aus.downloadUpdate(aUpdate, true);
    Services.aus.addDownloadListener(this);
  },

  finishUpdate: function UP_finishUpdate() {
    if (!this._update.isOSUpdate) {
      // Standard gecko+gaia updates will just need to restart the process
      this.restartProcess();
      return;
    }

    let osApplyToDir;
    try {
      this._update.QueryInterface(Ci.nsIWritablePropertyBag);
      osApplyToDir = this._update.getProperty("osApplyToDir");
    } catch (e) {}

    if (!osApplyToDir) {
      log("Error: Update has no osApplyToDir");
      return;
    }

    let updateFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    updateFile.initWithPath(osApplyToDir + "/update.zip");
    if (!updateFile.exists()) {
      log("Error: FOTA update not found at " + updateFile.path);
      return;
    }

    this.finishOSUpdate(updateFile.path);
  },

  restartProcess: function UP_restartProcess() {
    log("Update downloaded, restarting to apply it");

#ifndef MOZ_WIDGET_GONK
    let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"]
                     .getService(Ci.nsIAppStartup);
    appStartup.quit(appStartup.eForceQuit | appStartup.eRestart);
#else
    // NB: on Gonk, we rely on the system process manager to restart us.
    let pmService = Cc["@mozilla.org/power/powermanagerservice;1"]
                    .getService(Ci.nsIPowerManagerService);
    pmService.restart();
#endif
  },

  finishOSUpdate: function UP_finishOSUpdate(aOsUpdatePath) {
    let recoveryService = Cc["@mozilla.org/recovery-service;1"]
                            .getService(Ci.nsIRecoveryService);

    log("Rebooting into recovery to apply FOTA update: " + aOsUpdatePath);

    try {
      recoveryService.installFotaUpdate(aOsUpdatePath);
    } catch(e) {
      log("Error: Couldn't reboot into recovery to apply FOTA update " +
          aOsUpdatePath);
    }
  },

  forceUpdateCheck: function UP_forceUpdateCheck() {
    log("Forcing update check");

    let checker = Cc["@mozilla.org/updates/update-checker;1"]
                    .createInstance(Ci.nsIUpdateChecker);
    checker.checkForUpdates(this, true);
  },

  handleEvent: function UP_handleEvent(evt) {
    if (evt.type !== "mozContentEvent") {
      return;
    }

    let detail = evt.detail;
    if (!detail) {
      return;
    }

    switch (detail.type) {
      case "force-update-check":
        this.forceUpdateCheck();
        break;
      case "update-available-result":
        this.handleAvailableResult(detail);
        this._update = null;
        break;
      case "update-prompt-apply-result":
        this.handleApplyPromptResult(detail);
        break;
    }
  },

  // nsIObserver

  observe: function UP_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "idle":
        this._waitingForIdle = false;
        this.showApplyPrompt(this._update);
        // Fall through
      case "quit-application":
        Services.idle.removeIdleObserver(this, APPLY_IDLE_TIMEOUT_SECONDS);
        Services.obs.removeObserver(this, "quit-application");
        break;
    }
  },

  // nsITimerCallback

  notify: function UP_notify(aTimer) {
    if (aTimer == this._applyPromptTimer) {
      log("Timed out waiting for result, restarting");
      this._applyPromptTimer = null;
      this.finishUpdate();
    }
  },

  createTimer: function UP_createTimer(aTimeoutMs) {
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(this, aTimeoutMs, timer.TYPE_ONE_SHOT);
    return timer;
  },

  // nsIRequestObserver

  onStartRequest: function UP_onStartRequest(aRequest, aContext) {
    this.sendChromeEvent("update-downloading");
  },

  onStopRequest: function UP_onStopRequest(aRequest, aContext, aStatusCode) {
    Services.aus.removeDownloadListener(this);
  },

  // nsIProgressEventSink

  onProgress: function UP_onProgress(aRequest, aContext, aProgress,
                                     aProgressMax) {
    this.sendChromeEvent("update-progress", {
      progress: aProgress,
      total: aProgressMax
    });
  },

  onStatus: function UP_onStatus(aRequest, aUpdate, aStatus, aStatusArg) { }
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([UpdatePrompt]);
