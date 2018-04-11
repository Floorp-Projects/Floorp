/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module is imported by code that uses the "download.xml" binding, and
 * provides prototypes for objects that handle input and display information.
 */

"use strict";

var EXPORTED_SYMBOLS = [
  "DownloadsViewUI",
];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  Downloads: "resource://gre/modules/Downloads.jsm",
  DownloadUtils: "resource://gre/modules/DownloadUtils.jsm",
  DownloadsCommon: "resource:///modules/DownloadsCommon.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm"
});

var DownloadsViewUI = {
  /**
   * Returns true if the given string is the name of a command that can be
   * handled by the Downloads user interface, including standard commands.
   */
  isCommandName(name) {
    return name.startsWith("cmd_") || name.startsWith("downloadsCmd_");
  },
};

this.DownloadsViewUI.BaseView = class {
  canClearDownloads(nodeContainer) {
    // Downloads can be cleared if there's at least one removable download in
    // the list (either a history download or a completed session download).
    // Because history downloads are always removable and are listed after the
    // session downloads, check from bottom to top.
    for (let elt = nodeContainer.lastChild; elt; elt = elt.previousSibling) {
      // Stopped, paused, and failed downloads with partial data are removed.
      let download = elt._shell.download;
      if (download.stopped && !(download.canceled && download.hasPartialData)) {
        return true;
      }
    }
    return false;
  }
};

/**
 * A download element shell is responsible for handling the commands and the
 * displayed data for a single element that uses the "download.xml" binding.
 *
 * The information to display is obtained through the associated Download object
 * from the JavaScript API for downloads, and commands are executed using a
 * combination of Download methods and DownloadsCommon.jsm helper functions.
 *
 * Specialized versions of this shell must be defined, and they are required to
 * implement the "download" property or getter. Currently these objects are the
 * HistoryDownloadElementShell and the DownloadsViewItem for the panel. The
 * history view may use a HistoryDownload object in place of a Download object.
 */
this.DownloadsViewUI.DownloadElementShell = function() {};

this.DownloadsViewUI.DownloadElementShell.prototype = {
  /**
   * The richlistitem for the download, initialized by the derived object.
   */
  element: null,

  /**
   * URI string for the file type icon displayed in the download element.
   */
  get image() {
    if (!this.download.target.path) {
      // Old history downloads may not have a target path.
      return "moz-icon://.unknown?size=32";
    }

    // When a download that was previously in progress finishes successfully, it
    // means that the target file now exists and we can extract its specific
    // icon, for example from a Windows executable. To ensure that the icon is
    // reloaded, however, we must change the URI used by the XUL image element,
    // for example by adding a query parameter. This only works if we add one of
    // the parameters explicitly supported by the nsIMozIconURI interface.
    return "moz-icon://" + this.download.target.path + "?size=32" +
           (this.download.succeeded ? "&state=normal" : "");
  },

  /**
   * The user-facing label for the download. This is normally the leaf name of
   * the download target file. In case this is a very old history download for
   * which the target file is unknown, the download source URI is displayed.
   */
  get displayName() {
    if (!this.download.target.path) {
      return this.download.source.url;
    }
    return OS.Path.basename(this.download.target.path);
  },

  /**
   * The user-facing label for the size (if any) of the download. The return value
   * is an object 'sizeStrings' with 2 strings:
   *   1. stateLabel - The size with the units (e.g. "1.5 MB").
   *   2. status - The status of the download (e.g. "Completed");
   */
  get sizeStrings() {
    let s = DownloadsCommon.strings;
    let sizeStrings = {};

    if (this.download.target.size !== undefined) {
      let [size, unit] = DownloadUtils.convertByteUnits(this.download.target.size);
      sizeStrings.stateLabel = s.sizeWithUnits(size, unit);
      sizeStrings.status = s.statusSeparator(s.stateCompleted, sizeStrings.stateLabel);
    } else {
      // History downloads may not have a size defined.
      sizeStrings.stateLabel = s.sizeUnknown;
      sizeStrings.status = s.stateCompleted;
    }
    return sizeStrings;
  },

  get browserWindow() {
    return BrowserWindowTracker.getTopWindow();
  },

  /**
   * The progress element for the download, or undefined in case the XBL binding
   * has not been applied yet.
   */
  get _progressElement() {
    if (!this.__progressElement) {
      // If the element is not available now, we will try again the next time.
      this.__progressElement =
           this.element.ownerDocument.getAnonymousElementByAttribute(
                                         this.element, "anonid",
                                         "progressmeter");
    }
    return this.__progressElement;
  },

  /**
   * Processes a major state change in the user interface, then proceeds with
   * the normal progress update. This function is not called for every progress
   * update in order to improve performance.
   */
  _updateState() {
    this.element.setAttribute("displayName", this.displayName);
    this.element.setAttribute("image", this.image);
    this.element.setAttribute("state",
                              DownloadsCommon.stateOfDownload(this.download));

    if (!this.download.succeeded && this.download.error &&
        this.download.error.becauseBlockedByReputationCheck) {
      this.element.setAttribute("verdict",
                                this.download.error.reputationCheckVerdict);
    } else {
      this.element.removeAttribute("verdict");
    }

    // Since state changed, reset the time left estimation.
    this.lastEstimatedSecondsLeft = Infinity;

    this._updateProgress();
  },

  /**
   * Updates the elements that change regularly for in-progress downloads,
   * namely the progress bar and the status line.
   */
  _updateProgress() {
    if (this.download.succeeded) {
      // We only need to add or remove this attribute for succeeded downloads.
      if (this.download.target.exists) {
        this.element.setAttribute("exists", "true");
      } else {
        this.element.removeAttribute("exists");
      }
    }

    // When a block is confirmed, the removal of blocked data will not trigger a
    // state change for the download, so this class must be updated here.
    this.element.classList.toggle("temporary-block",
                                  !!this.download.hasBlockedData);

    // The progress bar is only displayed for in-progress downloads.
    if (this.download.hasProgress) {
      this.element.setAttribute("progressmode", "normal");
      this.element.setAttribute("progress", this.download.progress);
    } else {
      this.element.setAttribute("progressmode", "undetermined");
    }

    if (this.download.stopped && this.download.canceled &&
        this.download.hasPartialData) {
      this.element.setAttribute("progresspaused", "true");
    } else {
      this.element.removeAttribute("progresspaused");
    }

    // Dispatch the ValueChange event for accessibility, if possible.
    if (this._progressElement) {
      let event = this.element.ownerDocument.createEvent("Events");
      event.initEvent("ValueChange", true, true);
      this._progressElement.dispatchEvent(event);
    }

    let labels = this.statusLabels;
    this.element.setAttribute("status", labels.status);
    this.element.setAttribute("hoverStatus", labels.hoverStatus);
    this.element.setAttribute("fullStatus", labels.fullStatus);
  },

  lastEstimatedSecondsLeft: Infinity,

  /**
   * Returns the labels for the status of normal, full, and hovering cases. These
   * are returned by a single property because they are computed together.
   */
  get statusLabels() {
    let s = DownloadsCommon.strings;

    let status = "";
    let hoverStatus = "";
    let fullStatus = "";

    if (!this.download.stopped) {
      let totalBytes = this.download.hasProgress ? this.download.totalBytes
                                                 : -1;
      let newEstimatedSecondsLeft;
      [status, newEstimatedSecondsLeft] = DownloadUtils.getDownloadStatus(
                                          this.download.currentBytes,
                                          totalBytes,
                                          this.download.speed,
                                          this.lastEstimatedSecondsLeft);
      this.lastEstimatedSecondsLeft = newEstimatedSecondsLeft;
      hoverStatus = status;
    } else if (this.download.canceled && this.download.hasPartialData) {
      let totalBytes = this.download.hasProgress ? this.download.totalBytes
                                                 : -1;
      let transfer = DownloadUtils.getTransferTotal(this.download.currentBytes,
                                                    totalBytes);

      // We use the same XUL label to display both the state and the amount
      // transferred, for example "Paused -  1.1 MB".
      status = s.statusSeparatorBeforeNumber(s.statePaused, transfer);
      hoverStatus = status;
    } else if (!this.download.succeeded && !this.download.canceled &&
               !this.download.error) {
      status = s.stateStarting;
      hoverStatus = status;
    } else {
      let stateLabel;

      if (this.download.succeeded && !this.download.target.exists) {
        stateLabel = s.fileMovedOrMissing;
        hoverStatus = stateLabel;
      } else if (this.download.succeeded) {
        // For completed downloads, show the file size
        let sizeStrings = this.sizeStrings;
        stateLabel = sizeStrings.stateLabel;
        status = sizeStrings.status;
        hoverStatus = status;
      } else if (this.download.canceled) {
        stateLabel = s.stateCanceled;
      } else if (this.download.error.becauseBlockedByParentalControls) {
        stateLabel = s.stateBlockedParentalControls;
      } else if (this.download.error.becauseBlockedByReputationCheck) {
        stateLabel = this.rawBlockedTitleAndDetails[0];
      } else {
        stateLabel = s.stateFailed;
      }

      let referrer = this.download.source.referrer || this.download.source.url;
      let [displayHost /* ,fullHost */] = DownloadUtils.getURIHost(referrer);

      let date = new Date(this.download.endTime);
      let [displayDate /* ,fullDate */] = DownloadUtils.getReadableDates(date);

      let firstPart = s.statusSeparator(stateLabel, displayHost);
      fullStatus = s.statusSeparator(firstPart, displayDate);
      status = status || stateLabel;
    }

    return {
      status,
      hoverStatus: hoverStatus || fullStatus,
      fullStatus: fullStatus || status,
    };
  },

  /**
   * Returns [title, [details1, details2]] for blocked downloads.
   */
  get rawBlockedTitleAndDetails() {
    let s = DownloadsCommon.strings;
    if (!this.download.error ||
        !this.download.error.becauseBlockedByReputationCheck) {
      return [null, null];
    }
    switch (this.download.error.reputationCheckVerdict) {
      case Downloads.Error.BLOCK_VERDICT_UNCOMMON:
        return [s.blockedUncommon2, [s.unblockTypeUncommon2, s.unblockTip2]];
      case Downloads.Error.BLOCK_VERDICT_POTENTIALLY_UNWANTED:
        return [s.blockedPotentiallyUnwanted,
                [s.unblockTypePotentiallyUnwanted2, s.unblockTip2]];
      case Downloads.Error.BLOCK_VERDICT_MALWARE:
        return [s.blockedMalware, [s.unblockTypeMalware, s.unblockTip2]];
    }
    throw new Error("Unexpected reputationCheckVerdict: " +
                    this.download.error.reputationCheckVerdict);
  },

  /**
   * Shows the appropriate unblock dialog based on the verdict, and executes the
   * action selected by the user in the dialog, which may involve unblocking,
   * opening or removing the file.
   *
   * @param window
   *        The window to which the dialog should be anchored.
   * @param dialogType
   *        Can be "unblock", "chooseUnblock", or "chooseOpen".
   */
  confirmUnblock(window, dialogType) {
    DownloadsCommon.confirmUnblockDownload({
      verdict: this.download.error.reputationCheckVerdict,
      window,
      dialogType,
    }).then(action => {
      if (action == "open") {
        return this.unblockAndOpenDownload();
      } else if (action == "unblock") {
        return this.download.unblock();
      } else if (action == "confirmBlock") {
        return this.download.confirmBlock();
      }
      return Promise.resolve();
    }).catch(Cu.reportError);
  },

  /**
   * Unblocks the downloaded file and opens it.
   *
   * @return A promise that's resolved after the file has been opened.
   */
  unblockAndOpenDownload() {
    return this.download.unblock().then(() => this.downloadsCmd_open());
  },

  /**
   * Returns the name of the default command to use for the current state of the
   * download, when there is a double click or another default interaction. If
   * there is no default command for the current state, returns an empty string.
   * The commands are implemented as functions on this object or derived ones.
   */
  get currentDefaultCommandName() {
    switch (DownloadsCommon.stateOfDownload(this.download)) {
      case DownloadsCommon.DOWNLOAD_NOTSTARTED:
        return "downloadsCmd_cancel";
      case DownloadsCommon.DOWNLOAD_FAILED:
      case DownloadsCommon.DOWNLOAD_CANCELED:
        return "downloadsCmd_retry";
      case DownloadsCommon.DOWNLOAD_PAUSED:
        return "downloadsCmd_pauseResume";
      case DownloadsCommon.DOWNLOAD_FINISHED:
        return "downloadsCmd_open";
      case DownloadsCommon.DOWNLOAD_BLOCKED_PARENTAL:
        return "downloadsCmd_openReferrer";
      case DownloadsCommon.DOWNLOAD_DIRTY:
        return "downloadsCmd_showBlockedInfo";
    }
    return "";
  },

  /**
   * Returns true if the specified command can be invoked on the current item.
   * The commands are implemented as functions on this object or derived ones.
   *
   * @param aCommand
   *        Name of the command to check, for example "downloadsCmd_retry".
   */
  isCommandEnabled(aCommand) {
    switch (aCommand) {
      case "downloadsCmd_retry":
        return this.download.canceled || this.download.error;
      case "downloadsCmd_pauseResume":
        return this.download.hasPartialData && !this.download.error;
      case "downloadsCmd_openReferrer":
        return !!this.download.source.referrer;
      case "downloadsCmd_confirmBlock":
      case "downloadsCmd_chooseUnblock":
      case "downloadsCmd_chooseOpen":
      case "downloadsCmd_unblock":
      case "downloadsCmd_unblockAndOpen":
        return this.download.hasBlockedData;
      case "downloadsCmd_cancel":
        return this.download.hasPartialData || !this.download.stopped;
      case "downloadsCmd_open":
        // This property is false if the download did not succeed.
        return this.download.target.exists;
      case "downloadsCmd_show":
        // TODO: Bug 827010 - Handle part-file asynchronously.
        if (this.download.target.partFilePath) {
          let partFile = new FileUtils.File(this.download.target.partFilePath);
          if (partFile.exists()) {
            return true;
          }
        }

        // This property is false if the download did not succeed.
        return this.download.target.exists;
      case "downloadsCmd_delete":
      case "cmd_delete":
        // We don't want in-progress downloads to be removed accidentally.
        return this.download.stopped;
    }
    return DownloadsViewUI.isCommandName(aCommand) && !!this[aCommand];
  },

  doCommand(aCommand) {
    if (DownloadsViewUI.isCommandName(aCommand)) {
      this[aCommand]();
    }
  },

  downloadsCmd_cancel() {
    // This is the correct way to avoid race conditions when cancelling.
    this.download.cancel().catch(() => {});
    this.download.removePartialData().catch(Cu.reportError);
  },

  downloadsCmd_confirmBlock() {
    this.download.confirmBlock().catch(Cu.reportError);
  },

  downloadsCmd_open() {
    let file = new FileUtils.File(this.download.target.path);
    DownloadsCommon.openDownloadedFile(file, null, this.element.ownerGlobal);
  },

  downloadsCmd_openReferrer() {
    this.element.ownerGlobal.openURL(this.download.source.referrer);
  },

  downloadsCmd_pauseResume() {
    if (this.download.stopped) {
      this.download.start();
    } else {
      this.download.cancel();
    }
  },

  downloadsCmd_show() {
    let file = new FileUtils.File(this.download.target.path);
    DownloadsCommon.showDownloadedFile(file);
  },

  downloadsCmd_retry() {
    if (this.download.start) {
      // Errors when retrying are already reported as download failures.
      this.download.start().catch(() => {});
      return;
    }

    let window = this.browserWindow || this.element.ownerGlobal;
    let document = window.document;

    // Do not suggest a file name if we don't know the original target.
    let targetPath = this.download.target.path ?
                     OS.Path.basename(this.download.target.path) : null;
    window.DownloadURL(this.download.source.url, targetPath, document);
  },

  downloadsCmd_delete() {
    // Alias for the 'cmd_delete' command, because it may clash with another
    // controller which causes unexpected behavior as different codepaths claim
    // ownership.
    this.cmd_delete();
  },

  cmd_delete() {
    (async () => {
      // Remove the associated history element first, if any, so that the views
      // that combine history and session downloads won't resurrect the history
      // download into the view just before it is deleted permanently.
      try {
        await PlacesUtils.history.remove(this.download.source.url);
      } catch (ex) {
        Cu.reportError(ex);
      }
      let list = await Downloads.getList(Downloads.ALL);
      await list.remove(this.download);
      await this.download.finalize(true);
    })().catch(Cu.reportError);
  },
};
