/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Note: this file is included in aboutDialog.xhtml and preferences/advanced.xhtml
// if MOZ_UPDATER is defined.

/* import-globals-from aboutDialog.js */

var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  AppUpdater: "resource:///modules/AppUpdater.jsm",
  DownloadUtils: "resource://gre/modules/DownloadUtils.jsm",
  UpdateUtils: "resource://gre/modules/UpdateUtils.jsm",
});

var UPDATING_MIN_DISPLAY_TIME_MS = 1500;

var gAppUpdater;

function onUnload(aEvent) {
  if (gAppUpdater) {
    gAppUpdater.destroy();
    gAppUpdater = null;
  }
}

function appUpdater(options = {}) {
  this._appUpdater = new AppUpdater();

  this._appUpdateListener = (status, ...args) => {
    this._onAppUpdateStatus(status, ...args);
  };
  this._appUpdater.addListener(this._appUpdateListener);

  this.options = options;
  this.updatingMinDisplayTimerId = null;
  this.updateDeck = document.getElementById("updateDeck");

  this.bundle = Services.strings.createBundle(
    "chrome://browser/locale/browser.properties"
  );

  try {
    let manualURL = new URL(
      Services.urlFormatter.formatURLPref("app.update.url.manual")
    );

    let manualLink = document.getElementById("manualLink");
    // Strip hash and search parameters for display text.
    manualLink.textContent = manualURL.origin + manualURL.pathname;
    manualLink.href = manualURL.href;

    document.getElementById("failedLink").href = manualURL.href;
  } catch (e) {
    console.error("Invalid manual update url.", e);
  }

  this._appUpdater.check();
}

appUpdater.prototype = {
  destroy() {
    this.stopCurrentCheck();
    if (this.updatingMinDisplayTimerId) {
      clearTimeout(this.updatingMinDisplayTimerId);
    }
  },

  stopCurrentCheck() {
    this._appUpdater.removeListener(this._appUpdateListener);
    this._appUpdater.stop();
  },

  get update() {
    return this._appUpdater.update;
  },

  get selectedPanel() {
    return this.updateDeck.querySelector(".selected");
  },

  _onAppUpdateStatus(status, ...args) {
    switch (status) {
      case AppUpdater.STATUS.UPDATE_DISABLED_BY_POLICY:
        this.selectPanel("policyDisabled");
        break;
      case AppUpdater.STATUS.READY_FOR_RESTART:
        this.selectPanel("apply");
        break;
      case AppUpdater.STATUS.OTHER_INSTANCE_HANDLING_UPDATES:
        this.selectPanel("otherInstanceHandlingUpdates");
        break;
      case AppUpdater.STATUS.DOWNLOADING:
        this.downloadStatus = document.getElementById("downloadStatus");
        if (!args.length) {
          this.downloadStatus.textContent = DownloadUtils.getTransferTotal(
            0,
            this.update.selectedPatch.size
          );
          this.selectPanel("downloading");
        } else {
          let [progress, max] = args;
          this.downloadStatus.textContent = DownloadUtils.getTransferTotal(
            progress,
            max
          );
        }
        break;
      case AppUpdater.STATUS.STAGING:
        this.selectPanel("applying");
        break;
      case AppUpdater.STATUS.CHECKING: {
        this.checkingForUpdatesDelayPromise = new Promise(resolve => {
          this.updatingMinDisplayTimerId = setTimeout(
            resolve,
            UPDATING_MIN_DISPLAY_TIME_MS
          );
        });
        if (Services.policies.isAllowed("appUpdate")) {
          this.selectPanel("checkingForUpdates");
        } else {
          this.selectPanel("policyDisabled");
        }
        break;
      }
      case AppUpdater.STATUS.NO_UPDATES_FOUND:
        this.checkingForUpdatesDelayPromise.then(() => {
          if (Services.policies.isAllowed("appUpdate")) {
            this.selectPanel("noUpdatesFound");
          } else {
            this.selectPanel("policyDisabled");
          }
        });
        break;
      case AppUpdater.STATUS.UNSUPPORTED_SYSTEM:
        if (this.update.detailsURL) {
          let unsupportedLink = document.getElementById("unsupportedLink");
          unsupportedLink.href = this.update.detailsURL;
        }
        this.selectPanel("unsupportedSystem");
        break;
      case AppUpdater.STATUS.MANUAL_UPDATE:
        this.selectPanel("manualUpdate");
        break;
      case AppUpdater.STATUS.DOWNLOAD_AND_INSTALL:
        this.selectPanel("downloadAndInstall");
        break;
      case AppUpdater.STATUS.DOWNLOAD_FAILED:
        this.selectPanel("downloadFailed");
        break;
    }
  },

  /**
   * Sets the panel of the updateDeck and the visibility of icons
   * in the #icons element.
   *
   * @param  aChildID
   *         The id of the deck's child to select, e.g. "apply".
   */
  selectPanel(aChildID) {
    let panel = document.getElementById(aChildID);
    let icons = document.getElementById("icons");
    if (icons) {
      icons.className = aChildID;
    }

    let button = panel.querySelector("button");
    if (button) {
      if (aChildID == "downloadAndInstall") {
        let updateVersion = gAppUpdater.update.displayVersion;
        // Include the build ID if this is an "a#" (nightly or aurora) build
        if (/a\d+$/.test(updateVersion)) {
          let buildID = gAppUpdater.update.buildID;
          let year = buildID.slice(0, 4);
          let month = buildID.slice(4, 6);
          let day = buildID.slice(6, 8);
          updateVersion += ` (${year}-${month}-${day})`;
        }
        button.label = this.bundle.formatStringFromName(
          "update.downloadAndInstallButton.label",
          [updateVersion]
        );
        button.accessKey = this.bundle.GetStringFromName(
          "update.downloadAndInstallButton.accesskey"
        );
      }
      this.selectedPanel?.classList.remove("selected");
      panel.classList.add("selected");
      if (
        this.options.buttonAutoFocus &&
        (!document.commandDispatcher.focusedElement || // don't steal the focus
          document.commandDispatcher.focusedElement.localName == "button")
      ) {
        // except from the other buttons
        button.focus();
      }
    } else {
      this.selectedPanel?.classList.remove("selected");
      panel.classList.add("selected");
    }
  },

  /**
   * Check for updates
   */
  checkForUpdates() {
    this._appUpdater.checkForUpdates();
  },

  /**
   * Handles oncommand for the "Restart to Update" button
   * which is presented after the download has been downloaded.
   */
  buttonRestartAfterDownload() {
    if (!this._appUpdater.isReadyForRestart) {
      return;
    }

    gAppUpdater.selectPanel("restarting");

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
      gAppUpdater.selectPanel("apply");
      return;
    }

    // If already in safe mode restart in safe mode (bug 327119)
    if (Services.appinfo.inSafeMode) {
      Services.startup.restartInSafeMode(Ci.nsIAppStartup.eAttemptQuit);
      return;
    }

    if (
      !Services.startup.quit(
        Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart
      )
    ) {
      // Either the user or the hidden window aborted the quit process.
      gAppUpdater.selectPanel("apply");
    }
  },

  /**
   * Starts the download of an update mar.
   */
  startDownload() {
    this._appUpdater.startDownload();
  },
};
