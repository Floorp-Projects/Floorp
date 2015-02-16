/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file is loaded in every window that uses the "download.xml" binding, and
 * provides prototypes for objects that handle input and display information.
 *
 * This file lazily imports common JavaScript modules in the window scope. Most
 * of these modules are generally already declared before this file is included.
 */

"use strict";

let { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DownloadUtils",
                                  "resource://gre/modules/DownloadUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadsCommon",
                                  "resource:///modules/DownloadsCommon.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

/**
 * A download element shell is responsible for handling the commands and the
 * displayed data for a single element that uses the "download.xml" binding.
 *
 * The information to display is obtained through the associated Download object
 * from the JavaScript API for downloads, and commands are executed using a
 * combination of Download methods and DownloadsCommon.jsm helper functions.
 *
 * Specialized versions of this shell must be defined, currently they are the
 * HistoryDownloadElementShell and the DownloadsViewItem for the panel. The
 * history view may use a HistoryDownload object in place of a Download object.
 */
function DownloadElementShell() {}

DownloadElementShell.prototype = {
  /**
   * The richlistitem for the download, initialized by the derived object.
   */
  element: null,

  /**
   * The DownloadsDataItem for the download, overridden by the derived object.
   */
  dataItem: null,

  /**
   * Download or HistoryDownload object to use for displaying information and
   * for executing commands in the user interface.
   */
  get download() this.dataItem.download,

  /**
   * URI string for the file type icon displayed in the download element.
   */
  get image() {
    if (this.download.target.path) {
      return "moz-icon://" + this.download.target.path + "?size=32";
    }

    // Old history downloads may not have a target path.
    return "moz-icon://.unknown?size=32";
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
      this.__progressElement = document.getAnonymousElementByAttribute(
                               this.element, "anonid", "progressmeter");
    }
    return this.__progressElement;
  },

  /**
   * Processes a major state change in the user interface, then proceeds with
   * the normal progress update. This function is not called for every progress
   * update in order to improve performance.
   */
  _updateState() {
    this.element.setAttribute("state", this.dataItem.state);
    this.element.setAttribute("displayName", this.displayName);
    this.element.setAttribute("image", this.image);

    // Since state changed, reset the time left estimation.
    this.lastEstimatedSecondsLeft = Infinity;

    this._updateProgress();
  },

  /**
   * Updates the elements that change regularly for in-progress downloads,
   * namely the progress bar and the status line.
   */
  _updateProgress() {
    if (this.dataItem.starting) {
      // Before the download starts, the progress meter has its initial value.
      this.element.setAttribute("progressmode", "normal");
      this.element.setAttribute("progress", "0");
    } else if (this.dataItem.state == Ci.nsIDownloadManager.DOWNLOAD_SCANNING ||
               this.dataItem.percentComplete == -1) {
      // We might not know the progress of a running download, and we don't know
      // the remaining time during the malware scanning phase.
      this.element.setAttribute("progressmode", "undetermined");
    } else {
      // This is a running download of which we know the progress.
      this.element.setAttribute("progressmode", "normal");
      this.element.setAttribute("progress", this.dataItem.percentComplete);
    }

    // Dispatch the ValueChange event for accessibility, if possible.
    if (this._progressElement) {
      let event = document.createEvent("Events");
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

    if (this.dataItem.paused) {
      let transfer = DownloadUtils.getTransferTotal(this.download.currentBytes,
                                                    this.dataItem.maxBytes);

      // We use the same XUL label to display both the state and the amount
      // transferred, for example "Paused -  1.1 MB".
      text = s.statusSeparatorBeforeNumber(s.statePaused, transfer);
    } else if (this.dataItem.state == nsIDM.DOWNLOAD_DOWNLOADING) {
      // By default, extended status information including the individual
      // download rate is displayed in the tooltip. The history view overrides
      // the getter and displays the detials in the main area instead.
      [text] = DownloadUtils.getDownloadStatusNoRate(
                                          this.download.currentBytes,
                                          this.dataItem.maxBytes,
                                          this.download.speed,
                                          this.lastEstimatedSecondsLeft);
      let newEstimatedSecondsLeft;
      [tip, newEstimatedSecondsLeft] = DownloadUtils.getDownloadStatus(
                                          this.download.currentBytes,
                                          this.dataItem.maxBytes,
                                          this.download.speed,
                                          this.lastEstimatedSecondsLeft);
      this.lastEstimatedSecondsLeft = newEstimatedSecondsLeft;
    } else if (this.dataItem.starting) {
      text = s.stateStarting;
    } else if (this.dataItem.state == nsIDM.DOWNLOAD_SCANNING) {
      text = s.stateScanning;
    } else {
      let stateLabel;
      switch (this.dataItem.state) {
        case nsIDM.DOWNLOAD_FAILED:
          stateLabel = s.stateFailed;
          break;
        case nsIDM.DOWNLOAD_CANCELED:
          stateLabel = s.stateCanceled;
          break;
        case nsIDM.DOWNLOAD_BLOCKED_PARENTAL:
          stateLabel = s.stateBlockedParentalControls;
          break;
        case nsIDM.DOWNLOAD_BLOCKED_POLICY:
          stateLabel = s.stateBlockedPolicy;
          break;
        case nsIDM.DOWNLOAD_DIRTY:
          stateLabel = s.stateDirty;
          break;
        case nsIDM.DOWNLOAD_FINISHED:
          // For completed downloads, show the file size (e.g. "1.5 MB")
          if (this.dataItem.maxBytes !== undefined &&
              this.dataItem.maxBytes >= 0) {
            let [size, unit] =
                DownloadUtils.convertByteUnits(this.dataItem.maxBytes);
            stateLabel = s.sizeWithUnits(size, unit);
            break;
          }
          // Fallback to default unknown state.
        default:
          stateLabel = s.sizeUnknown;
          break;
      }

      let referrer = this.download.source.referrer ||
                     this.download.source.url;
      let [displayHost, fullHost] = DownloadUtils.getURIHost(referrer);

      let date = new Date(this.dataItem.endTime);
      let [displayDate, fullDate] = DownloadUtils.getReadableDates(date);

      let firstPart = s.statusSeparator(stateLabel, displayHost);
      text = s.statusSeparator(firstPart, displayDate);
      tip = s.statusSeparator(fullHost, fullDate);
    }

    return { text, tip: tip || text };
  },
};
