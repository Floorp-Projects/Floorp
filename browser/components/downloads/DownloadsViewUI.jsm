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

XPCOMUtils.defineLazyModuleGetter(this, "DownloadUtils",
                                  "resource://gre/modules/DownloadUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadsCommon",
                                  "resource:///modules/DownloadsCommon.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

this.DownloadsViewUI = {};

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
  get statusTextAndTip() this.rawStatusTextAndTip,

  /**
   * Derived objects may call this to get the status text.
   */
  get rawStatusTextAndTip() {
    const nsIDM = Ci.nsIDownloadManager;
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
        stateLabel = s.stateDirty;
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
};
