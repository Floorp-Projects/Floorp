/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module is imported by code that uses the "download.xml" binding, and
 * provides prototypes for objects that handle input and display information.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "DownloadsViewUI",
];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
                                  "resource://gre/modules/Downloads.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadUtils",
                                  "resource://gre/modules/DownloadUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadsCommon",
                                  "resource:///modules/DownloadsCommon.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

this.DownloadsViewUI = {
  /**
   * Returns true if the given string is the name of a command that can be
   * handled by the Downloads user interface, including standard commands.
   */
  isCommandName(name) {
    return name.startsWith("cmd_") || name.startsWith("downloadsCmd_");
  },
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
this.DownloadsViewUI.DownloadElementShell = function () {}

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

    if (this.download.error &&
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

    // Dispatch the ValueChange event for accessibility, if possible.
    if (this._progressElement) {
      let event = this.element.ownerDocument.createEvent("Events");
      event.initEvent("ValueChange", true, true);
      this._progressElement.dispatchEvent(event);
    }

    let status = this.statusTextAndTip;
    this.element.setAttribute("status", status.text);
    this.element.setAttribute("statusTip", status.tip);
  },

  lastEstimatedSecondsLeft: Infinity,

  /**
   * Returns the text for the status line and the associated tooltip. These are
   * returned by a single property because they are computed together. The
   * result may be overridden by derived objects.
   */
  get statusTextAndTip() {
    return this.rawStatusTextAndTip;
  },

  /**
   * Derived objects may call this to get the status text.
   */
  get rawStatusTextAndTip() {
    let s = DownloadsCommon.strings;

    let text = "";
    let tip = "";

    if (!this.download.stopped) {
      let totalBytes = this.download.hasProgress ? this.download.totalBytes
                                                 : -1;
      // By default, extended status information including the individual
      // download rate is displayed in the tooltip. The history view overrides
      // the getter and displays the datails in the main area instead.
      [text] = DownloadUtils.getDownloadStatusNoRate(
                                          this.download.currentBytes,
                                          totalBytes,
                                          this.download.speed,
                                          this.lastEstimatedSecondsLeft);
      let newEstimatedSecondsLeft;
      [tip, newEstimatedSecondsLeft] = DownloadUtils.getDownloadStatus(
                                          this.download.currentBytes,
                                          totalBytes,
                                          this.download.speed,
                                          this.lastEstimatedSecondsLeft);
      this.lastEstimatedSecondsLeft = newEstimatedSecondsLeft;
    } else if (this.download.canceled && this.download.hasPartialData) {
      let totalBytes = this.download.hasProgress ? this.download.totalBytes
                                                 : -1;
      let transfer = DownloadUtils.getTransferTotal(this.download.currentBytes,
                                                    totalBytes);

      // We use the same XUL label to display both the state and the amount
      // transferred, for example "Paused -  1.1 MB".
      text = s.statusSeparatorBeforeNumber(s.statePaused, transfer);
    } else if (!this.download.succeeded && !this.download.canceled &&
               !this.download.error) {
      text = s.stateStarting;
    } else {
      let stateLabel;

      if (this.download.succeeded) {
        // For completed downloads, show the file size (e.g. "1.5 MB").
        if (this.download.target.size !== undefined) {
          let [size, unit] =
            DownloadUtils.convertByteUnits(this.download.target.size);
          stateLabel = s.sizeWithUnits(size, unit);
        } else {
          // History downloads may not have a size defined.
          stateLabel = s.sizeUnknown;
        }
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
      let [displayHost, fullHost] = DownloadUtils.getURIHost(referrer);

      let date = new Date(this.download.endTime);
      let [displayDate, fullDate] = DownloadUtils.getReadableDates(date);

      let firstPart = s.statusSeparator(stateLabel, displayHost);
      text = s.statusSeparator(firstPart, displayDate);
      tip = s.statusSeparator(fullHost, fullDate);
    }

    return { text, tip: tip || text };
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
    // return anyway to avoid a JS strict warning.
    return [null, null];
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
        return this.download.unblock().then(() => this.downloadsCmd_open());
      } else if (action == "unblock") {
        return this.download.unblock();
      } else if (action == "confirmBlock") {
        return this.download.confirmBlock();
      }
    }).catch(Cu.reportError);
  },

  /**
   * Returns the name of the default command to use for the current state of the
   * download, when there is a double click or another default interaction. If
   * there is no default command for the current state, returns an empty string.
   * The commands are implemented as functions on this object or derived ones.
   */
  get currentDefaultCommandName() {
    switch (DownloadsCommon.stateOfDownload(this.download)) {
      case Ci.nsIDownloadManager.DOWNLOAD_NOTSTARTED:
        return "downloadsCmd_cancel";
      case Ci.nsIDownloadManager.DOWNLOAD_FAILED:
      case Ci.nsIDownloadManager.DOWNLOAD_CANCELED:
        return "downloadsCmd_retry";
      case Ci.nsIDownloadManager.DOWNLOAD_PAUSED:
        return "downloadsCmd_pauseResume";
      case Ci.nsIDownloadManager.DOWNLOAD_FINISHED:
        return "downloadsCmd_open";
      case Ci.nsIDownloadManager.DOWNLOAD_BLOCKED_PARENTAL:
        return "downloadsCmd_openReferrer";
      case Ci.nsIDownloadManager.DOWNLOAD_DIRTY:
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
        return this.download.hasBlockedData;
    }
    return false;
  },

  downloadsCmd_cancel() {
    // This is the correct way to avoid race conditions when cancelling.
    this.download.cancel().catch(() => {});
    this.download.removePartialData().catch(Cu.reportError);
  },

  downloadsCmd_retry() {
    // Errors when retrying are already reported as download failures.
    this.download.start().catch(() => {});
  },

  downloadsCmd_pauseResume() {
    if (this.download.stopped) {
      this.download.start();
    } else {
      this.download.cancel();
    }
  },

  downloadsCmd_confirmBlock() {
    this.download.confirmBlock().catch(Cu.reportError);
  },
};
