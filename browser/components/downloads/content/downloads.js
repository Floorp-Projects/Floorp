/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Handles the Downloads panel user interface for each browser window.
 *
 * This file includes the following constructors and global objects:
 *
 * DownloadsPanel
 * Main entry point for the downloads panel interface.
 *
 * DownloadsOverlayLoader
 * Allows loading the downloads panel and the status indicator interfaces on
 * demand, to improve startup performance.
 *
 * DownloadsView
 * Builds and updates the downloads list widget, responding to changes in the
 * download state and real-time data.  In addition, handles part of the user
 * interaction events raised by the downloads list widget.
 *
 * DownloadsViewItem
 * Builds and updates a single item in the downloads list widget, responding to
 * changes in the download state and real-time data.
 *
 * DownloadsViewController
 * Handles part of the user interaction events raised by the downloads list
 * widget, in particular the "commands" that apply to multiple items, and
 * dispatches the commands that apply to individual items.
 *
 * DownloadsViewItemController
 * Handles all the user interaction events, in particular the "commands",
 * related to a single item in the downloads list widgets.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Globals

XPCOMUtils.defineLazyModuleGetter(this, "DownloadUtils",
                                  "resource://gre/modules/DownloadUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadsCommon",
                                  "resource:///modules/DownloadsCommon.jsm");

////////////////////////////////////////////////////////////////////////////////
//// DownloadsPanel

/**
 * Main entry point for the downloads panel interface.
 */
const DownloadsPanel = {
  //////////////////////////////////////////////////////////////////////////////
  //// Initialization and termination

  /**
   * Internal state of the downloads panel, based on one of the kState
   * constants.  This is not the same state as the XUL panel element.
   */
  _state: 0,

  /** The panel is not linked to downloads data yet. */
  get kStateUninitialized() 0,
  /** This object is linked to data, but the panel is invisible. */
  get kStateHidden() 1,
  /** The panel will be shown as soon as possible. */
  get kStateShowing() 2,
  /** The panel is open. */
  get kStateShown() 3,

  /**
   * Location of the panel overlay.
   */
  get kDownloadsOverlay()
      "chrome://browser/content/downloads/downloadsOverlay.xul",

  /**
   * Starts loading the download data in background, without opening the panel.
   * Use showPanel instead to load the data and open the panel at the same time.
   *
   * @param aCallback
   *        Called when initialization is complete.
   */
  initialize: function DP_initialize(aCallback)
  {
    if (this._state != this.kStateUninitialized) {
      DownloadsOverlayLoader.ensureOverlayLoaded(this.kDownloadsOverlay,
                                                 aCallback);
      return;
    }
    this._state = this.kStateHidden;

    window.addEventListener("unload", this.onWindowUnload, false);

    // Ensure that the Download Manager service is running.  This resumes
    // active downloads if required.  If there are downloads to be shown in the
    // panel, starting the service will make us load their data asynchronously.
    Services.downloads;

    // Now that data loading has eventually started, load the required XUL
    // elements and initialize our views.
    DownloadsOverlayLoader.ensureOverlayLoaded(this.kDownloadsOverlay,
                                               function DP_I_callback() {
      DownloadsViewController.initialize();
      DownloadsCommon.data.addView(DownloadsView);
      aCallback();
    });
  },

  /**
   * Closes the downloads panel and frees the internal resources related to the
   * downloads.  The downloads panel can be reopened later, even after this
   * function has been called.
   */
  terminate: function DP_terminate()
  {
    if (this._state == this.kStateUninitialized) {
      return;
    }

    window.removeEventListener("unload", this.onWindowUnload, false);

    // Ensure that the panel is closed before shutting down.
    this.hidePanel();

    DownloadsViewController.terminate();
    DownloadsCommon.data.removeView(DownloadsView);

    this._state = this.kStateUninitialized;
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Panel interface

  /**
   * Main panel element in the browser window.
   */
  get panel()
  {
    delete this.panel;
    return this.panel = document.getElementById("downloadsPanel");
  },

  /**
   * Starts opening the downloads panel interface, anchored to the downloads
   * button of the browser window.  The list of downloads to display is
   * initialized the first time this method is called, and the panel is shown
   * only when data is ready.
   */
  showPanel: function DP_showPanel()
  {
    if (this.isPanelShowing) {
      this._focusPanel();
      return;
    }

    this.initialize(function DP_SP_callback() {
      // Delay displaying the panel because this function will sometimes be
      // called while another window is closing (like the window for selecting
      // whether to save or open the file), and that would cause the panel to
      // close immediately.
      setTimeout(function () DownloadsPanel._openPopupIfDataReady(), 0);
    }.bind(this));

    this._state = this.kStateShowing;
  },

  /**
   * Hides the downloads panel, if visible, but keeps the internal state so that
   * the panel can be reopened quickly if required.
   */
  hidePanel: function DP_hidePanel()
  {
    if (!this.isPanelShowing) {
      return;
    }

    this.panel.hidePopup();

    // Ensure that we allow the panel to be reopened.  Note that, if the popup
    // was open, then the onPopupHidden event handler has already updated the
    // current state, otherwise we must update the state ourselves.
    this._state = this.kStateHidden;
  },

  /**
   * Indicates whether the panel is shown or will be shown.
   */
  get isPanelShowing()
  {
    return this._state == this.kStateShowing ||
           this._state == this.kStateShown;
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Callback functions from DownloadsView

  /**
   * Called after data loading finished.
   */
  onViewLoadCompleted: function DP_onViewLoadCompleted()
  {
    this._openPopupIfDataReady();
  },

  //////////////////////////////////////////////////////////////////////////////
  //// User interface event functions

  onWindowUnload: function DP_onWindowUnload()
  {
    // This function is registered as an event listener, we can't use "this".
    DownloadsPanel.terminate();
  },

  onPopupShown: function DP_onPopupShown(aEvent)
  {
    // Ignore events raised by nested popups.
    if (aEvent.target != aEvent.currentTarget) {
      return;
    }

    this._state = this.kStateShown;

    // Since at most one popup is open at any given time, we can set globally.
    DownloadsCommon.indicatorData.attentionSuppressed = true;

    // Ensure that an item is selected when the panel is focused.
    if (DownloadsView.richListBox.itemCount > 0 &&
        !DownloadsView.richListBox.selectedItem) {
      DownloadsView.richListBox.selectedIndex = 0;
    }

    this._focusPanel();
  },

  onPopupHidden: function DP_onPopupHidden(aEvent)
  {
    // Ignore events raised by nested popups.
    if (aEvent.target != aEvent.currentTarget) {
      return;
    }

    // Since at most one popup is open at any given time, we can set globally.
    DownloadsCommon.indicatorData.attentionSuppressed = false;

    // Allow the anchor to be hidden.
    DownloadsButton.releaseAnchor();

    // Allow the panel to be reopened.
    this._state = this.kStateHidden;
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Related operations

  /**
   * Shows or focuses the user interface dedicated to downloads history.
   */
  showDownloadsHistory: function DP_showDownloadsHistory()
  {
    // Hide the panel before invoking the Library window, otherwise focus will
    // return to the browser window when the panel closes automatically.
    this.hidePanel();

    // Open the Library window and select the Downloads query.
    PlacesCommandHook.showPlacesOrganizer("Downloads");
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Internal functions

  /**
   * Move focus to the main element in the downloads panel, unless another
   * element in the panel is already focused.
   */
  _focusPanel: function DP_focusPanel()
  {
    // We may be invoked while the panel is still waiting to be shown.
    if (this._state != this.kStateShown) {
      return;
    }

    let element = document.commandDispatcher.focusedElement;
    while (element && element != this.panel) {
      element = element.parentNode;
    }
    if (!element) {
      DownloadsView.richListBox.focus();
    }
  },

  /**
   * Opens the downloads panel when data is ready to be displayed.
   */
  _openPopupIfDataReady: function DP_openPopupIfDataReady()
  {
    // We don't want to open the popup if we already displayed it, or if we are
    // still loading data.
    if (this._state != this.kStateShowing || DownloadsView.loading) {
      return;
    }

    // Ensure the anchor is visible.  If that is not possible, show the panel
    // anchored to the top area of the window, near the default anchor position.
    DownloadsButton.getAnchor(function DP_OPIDR_callback(aAnchor) {
      // At this point, if the window is minimized, opening the panel could fail
      // without any notification, and there would be no way to either open or
      // close the panel anymore.  To prevent this, check if the window is
      // minimized and in that case force the panel to the closed state.
      if (window.windowState == Ci.nsIDOMChromeWindow.STATE_MINIMIZED) {
        DownloadsButton.releaseAnchor();
        this._state = this.kStateHidden;
        return;
      }

      if (aAnchor) {
        this.panel.openPopup(aAnchor, "bottomcenter topright", 0, 0, false,
                             null);
      } else {
        this.panel.openPopup(document.getElementById("TabsToolbar"),
                             "after_end", 0, 0, false, null);
      }
    }.bind(this));
  }
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadsOverlayLoader

/**
 * Allows loading the downloads panel and the status indicator interfaces on
 * demand, to improve startup performance.
 */
const DownloadsOverlayLoader = {
  /**
   * We cannot load two overlays at the same time, thus we use a queue of
   * pending load requests.
   */
  _loadRequests: [],

  /**
   * True while we are waiting for an overlay to be loaded.
   */
  _overlayLoading: false,

  /**
   * This object has a key for each overlay URI that is already loaded.
   */
  _loadedOverlays: {},

  /**
   * Loads the specified overlay and invokes the given callback when finished.
   *
   * @param aOverlay
   *        String containing the URI of the overlay to load in the current
   *        window.  If this overlay has already been loaded using this
   *        function, then the overlay is not loaded again. 
   * @param aCallback
   *        Invoked when loading is completed.  If the overlay is already
   *        loaded, the function is called immediately.
   */
  ensureOverlayLoaded: function DOL_ensureOverlayLoaded(aOverlay, aCallback)
  {
    // The overlay is already loaded, invoke the callback immediately.
    if (aOverlay in this._loadedOverlays) {
      aCallback();
      return;
    }

    // The callback will be invoked when loading is finished.
    this._loadRequests.push({ overlay: aOverlay, callback: aCallback });
    if (this._overlayLoading) {
      return;
    }

    function DOL_EOL_loadCallback() {
      this._overlayLoading = false;
      this._loadedOverlays[aOverlay] = true;

      // Loading the overlay causes all the persisted XUL attributes to be
      // reapplied, including "iconsize" on the toolbars.  Until bug 640158 is
      // fixed, we must recalculate the correct "iconsize" attributes manually.
      retrieveToolbarIconsizesFromTheme();

      this.processPendingRequests();
    }

    this._overlayLoading = true;
    document.loadOverlay(aOverlay, DOL_EOL_loadCallback.bind(this));
  },

  /**
   * Re-processes all the currently pending requests, invoking the callbacks
   * and/or loading more overlays as needed.  In most cases, there will be a
   * single request for one overlay, that will be processed immediately.
   */
  processPendingRequests: function DOL_processPendingRequests()
  {
    // Re-process all the currently pending requests, yet allow more requests
    // to be appended at the end of the array if we're not ready for them.
    let currentLength = this._loadRequests.length;
    for (let i = 0; i < currentLength; i++) {
      let request = this._loadRequests.shift();

      // We must call ensureOverlayLoaded again for each request, to check if
      // the associated callback can be invoked now, or if we must still wait
      // for the associated overlay to load.
      this.ensureOverlayLoaded(request.overlay, request.callback);
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadsView

/**
 * Builds and updates the downloads list widget, responding to changes in the
 * download state and real-time data.  In addition, handles part of the user
 * interaction events raised by the downloads list widget.
 */
const DownloadsView = {
  //////////////////////////////////////////////////////////////////////////////
  //// Functions handling download items in the list

  /**
   * Indicates whether we are still loading downloads data asynchronously.
   */
  loading: false,

  /**
   * Object containing all the available DownloadsViewItem objects, indexed by
   * their numeric download identifier.
   */
  _viewItems: {},

  /**
   * Called when the number of items in the list changes.
   */
  _itemCountChanged: function DV_itemCountChanged()
  {
    if (Object.keys(this._viewItems).length > 0) {
      DownloadsPanel.panel.setAttribute("hasdownloads", "true");
    } else {
      DownloadsPanel.panel.removeAttribute("hasdownloads");
    }
  },

  /**
   * Element corresponding to the list of downloads.
   */
  get richListBox()
  {
    delete this.richListBox;
    return this.richListBox = document.getElementById("downloadsListBox");
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Callback functions from DownloadsData

  /**
   * Called before multiple downloads are about to be loaded.
   */
  onDataLoadStarting: function DV_onDataLoadStarting()
  {
    this.loading = true;
  },

  /**
   * Called after data loading finished.
   */
  onDataLoadCompleted: function DV_onDataLoadCompleted()
  {
    this.loading = false;

    // Notify the panel that all the initially available downloads have been
    // loaded.  This ensures that the interface is visible, if still required.
    DownloadsPanel.onViewLoadCompleted();
  },

  /**
   * Called when the downloads database becomes unavailable (for example,
   * entering Private Browsing Mode).  References to existing data should be
   * discarded.
   */
  onDataInvalidated: function DV_onDataInvalidated()
  {
    DownloadsPanel.terminate();

    // Clear the list by replacing with a shallow copy.
    let emptyView = this.richListBox.cloneNode(false);
    this.richListBox.parentNode.replaceChild(emptyView, this.richListBox);
    this.richListBox = emptyView;
    this._viewItems = {};
  },

  /**
   * Called when a new download data item is available, either during the
   * asynchronous data load or when a new download is started.
   *
   * @param aDataItem
   *        DownloadsDataItem object that was just added.
   * @param aNewest
   *        When true, indicates that this item is the most recent and should be
   *        added in the topmost position.  This happens when a new download is
   *        started.  When false, indicates that the item is the least recent
   *        and should be appended.  The latter generally happens during the
   *        asynchronous data load.
   */
  onDataItemAdded: function DV_onDataItemAdded(aDataItem, aNewest)
  {
    // Make the item and add it in the appropriate place in the list.
    let element = document.createElement("richlistitem");
    let viewItem = new DownloadsViewItem(aDataItem, element);
    this._viewItems[aDataItem.downloadId] = viewItem;
    if (aNewest) {
      this.richListBox.insertBefore(element, this.richListBox.firstChild);
    } else {
      this.richListBox.appendChild(element);
    }

    this._itemCountChanged();
  },

  /**
   * Called when a data item is removed.  Ensures that the widget associated
   * with the view item is removed from the user interface.
   *
   * @param aDataItem
   *        DownloadsDataItem object that is being removed.
   */
  onDataItemRemoved: function DV_onDataItemRemoved(aDataItem)
  {
    let element = this.getViewItem(aDataItem)._element;
    let previousSelectedIndex = this.richListBox.selectedIndex;
    this.richListBox.removeChild(element);
    this.richListBox.selectedIndex = Math.min(previousSelectedIndex,
                                              this.richListBox.itemCount - 1);
    delete this._viewItems[aDataItem.downloadId];

    this._itemCountChanged();
  },

  /**
   * Returns the view item associated with the provided data item for this view.
   *
   * @param aDataItem
   *        DownloadsDataItem object for which the view item is requested.
   *
   * @return Object that can be used to notify item status events.
   */
  getViewItem: function DV_getViewItem(aDataItem)
  {
    return this._viewItems[aDataItem.downloadId];
  },

  //////////////////////////////////////////////////////////////////////////////
  //// User interface event functions

  /**
   * Helper function to do commands on a specific download item.
   *
   * @param aEvent
   *        Event object for the event being handled.  If the event target is
   *        not a richlistitem that represents a download, this function will
   *        walk up the parent nodes until it finds a DOM node that is.
   * @param aCommand
   *        The command to be performed.
   */
  onDownloadCommand: function DV_onDownloadCommand(aEvent, aCommand)
  {
    let target = aEvent.target;
    while (target.nodeName != "richlistitem") {
      target = target.parentNode;
    }
    new DownloadsViewItemController(target).doCommand(aCommand);
  },

  onDownloadClick: function DV_onDownloadClick(aEvent)
  {
    // Handle primary clicks only.
    if (aEvent.button == 0) {
      goDoCommand("downloadsCmd_open");
    }
  },

  onDownloadKeyPress: function DV_onDownloadKeyPress(aEvent)
  {
    // Handle unmodified keys only.
    if (aEvent.altKey || aEvent.ctrlKey || aEvent.shiftKey || aEvent.metaKey) {
      return;
    }

    // Pressing the key on buttons should not invoke the action because the
    // event has already been handled by the button itself.
    if (aEvent.originalTarget.hasAttribute("command") ||
        aEvent.originalTarget.hasAttribute("oncommand")) {
      return;
    }

    if (aEvent.charCode == " ".charCodeAt(0)) {
      goDoCommand("downloadsCmd_pauseResume");
      return;
    }

    switch (aEvent.keyCode) {
      case KeyEvent.DOM_VK_ENTER:
      case KeyEvent.DOM_VK_RETURN:
        goDoCommand("downloadsCmd_doDefault");
        break;
    }
  },

  onDownloadContextMenu: function DV_onDownloadContextMenu(aEvent)
  {
    let element = this.richListBox.selectedItem;
    if (!element) {
      return;
    }

    DownloadsViewController.updateCommands();

    // Set the state attribute so that only the appropriate items are displayed.
    let contextMenu = document.getElementById("downloadsContextMenu");
    contextMenu.setAttribute("state", element.getAttribute("state"));
  },

  onDownloadDragStart: function DV_onDownloadDragStart(aEvent)
  {
    let element = this.richListBox.selectedItem;
    if (!element) {
      return;
    }

    let controller = new DownloadsViewItemController(element);
    let localFile = controller.dataItem.localFile;
    if (!localFile.exists()) {
      return;
    }

    let dataTransfer = aEvent.dataTransfer;
    dataTransfer.mozSetDataAt("application/x-moz-file", localFile, 0);
    dataTransfer.effectAllowed = "copyMove";
    dataTransfer.addElement(element);

    aEvent.stopPropagation();
  }
}

////////////////////////////////////////////////////////////////////////////////
//// DownloadsViewItem

/**
 * Builds and updates a single item in the downloads list widget, responding to
 * changes in the download state and real-time data.
 *
 * @param aDataItem
 *        DownloadsDataItem to be associated with the view item.
 * @param aElement
 *        XUL element corresponding to the single download item in the view.
 */
function DownloadsViewItem(aDataItem, aElement)
{
  this._element = aElement;
  this.dataItem = aDataItem;

  this.wasDone = this.dataItem.done;
  this.wasInProgress = this.dataItem.inProgress;
  this.lastEstimatedSecondsLeft = Infinity;

  // Set the URI that represents the correct icon for the target file.  As soon
  // as bug 239948 comment 12 is handled, the "file" property will be always a
  // file URL rather than a file name.  At that point we should remove the "//"
  // (double slash) from the icon URI specification (see test_moz_icon_uri.js).
  this.image = "moz-icon://" + this.dataItem.file + "?size=32";

  let attributes = {
    "type": "download",
    "class": "download-state",
    "id": "downloadsItem_" + this.dataItem.downloadId,
    "downloadId": this.dataItem.downloadId,
    "state": this.dataItem.state,
    "progress": this.dataItem.inProgress ? this.dataItem.percentComplete : 100,
    "target": this.dataItem.target,
    "image": this.image
  };

  for (let attributeName in attributes) {
    this._element.setAttribute(attributeName, attributes[attributeName]);
  }

  // Initialize more complex attributes.
  this._updateProgress();
  this._updateStatusLine();
}

DownloadsViewItem.prototype = {
  /**
   * The DownloadDataItem associated with this view item.
   */
  dataItem: null,

  /**
   * The XUL element corresponding to the associated richlistbox item.
   */
  _element: null,

  /**
   * The inner XUL element for the progress bar, or null if not available.
   */
  _progressElement: null,

  //////////////////////////////////////////////////////////////////////////////
  //// Callback functions from DownloadsData

  /**
   * Called when the download state might have changed.  Sometimes the state of
   * the download might be the same as before, if the data layer received
   * multiple events for the same download.
   */
  onStateChange: function DVI_onStateChange()
  {
    // If a download just finished successfully, it means that the target file
    // now exists and we can extract its specific icon.  To ensure that the icon
    // is reloaded, we must change the URI used by the XUL image element, for
    // example by adding a query parameter.  Since this URI has a "moz-icon"
    // scheme, this only works if we add one of the parameters explicitly
    // supported by the nsIMozIconURI interface.
    if (!this.wasDone && this.dataItem.openable) {
      this._element.setAttribute("image", this.image + "&state=normal");
    }

    // Update the end time using the current time if required.
    if (this.wasInProgress && !this.dataItem.inProgress) {
      this.endTime = Date.now();
    }

    this.wasDone = this.dataItem.done;
    this.wasInProgress = this.dataItem.inProgress;

    // Update the user interface after switching states.
    this._element.setAttribute("state", this.dataItem.state);
    this._updateProgress();
    this._updateStatusLine();
  },

  /**
   * Called when the download progress has changed.
   */
  onProgressChange: function DVI_onProgressChange() {
    this._updateProgress();
    this._updateStatusLine();
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Functions for updating the user interface

  /**
   * Updates the progress bar.
   */
  _updateProgress: function DVI_updateProgress() {
    if (this.dataItem.starting) {
      // Before the download starts, the progress meter has its initial value.
      this._element.setAttribute("progressmode", "normal");
      this._element.setAttribute("progress", "0");
    } else if (this.dataItem.state == Ci.nsIDownloadManager.DOWNLOAD_SCANNING ||
               this.dataItem.percentComplete == -1) {
      // We might not know the progress of a running download, and we don't know
      // the remaining time during the malware scanning phase.
      this._element.setAttribute("progressmode", "undetermined");
    } else {
      // This is a running download of which we know the progress.
      this._element.setAttribute("progressmode", "normal");
      this._element.setAttribute("progress", this.dataItem.percentComplete);
    }

    // Find the progress element as soon as the download binding is accessible.
    if (!this._progressElement) {
      this._progressElement =
           document.getAnonymousElementByAttribute(this._element, "anonid",
                                                   "progressmeter");
    }

    // Dispatch the ValueChange event for accessibility, if possible.
    if (this._progressElement) {
      let event = document.createEvent("Events");
      event.initEvent("ValueChange", true, true);
      this._progressElement.dispatchEvent(event);
    }
  },

  /**
   * Updates the main status line, including bytes transferred, bytes total,
   * download rate, and time remaining.
   */
  _updateStatusLine: function DVI_updateStatusLine() {
    const nsIDM = Ci.nsIDownloadManager;

    let status = "";
    let statusTip = "";

    if (this.dataItem.paused) {
      let transfer = DownloadUtils.getTransferTotal(this.dataItem.currBytes,
                                                    this.dataItem.maxBytes);

      // We use the same XUL label to display both the state and the amount
      // transferred, for example "Paused -  1.1 MB".
      status = DownloadsCommon.strings.statusSeparatorBeforeNumber(
                                            DownloadsCommon.strings.statePaused,
                                            transfer);
    } else if (this.dataItem.state == nsIDM.DOWNLOAD_DOWNLOADING) {
      let newEstimatedSecondsLeft;
      [status, newEstimatedSecondsLeft] =
        DownloadUtils.getDownloadStatus(this.dataItem.currBytes,
                                        this.dataItem.maxBytes,
                                        this.dataItem.speed,
                                        this.lastEstimatedSecondsLeft);
      this.lastEstimatedSecondsLeft = newEstimatedSecondsLeft;
    } else if (this.dataItem.starting) {
      status = DownloadsCommon.strings.stateStarting;
    } else if (this.dataItem.state == nsIDM.DOWNLOAD_SCANNING) {
      status = DownloadsCommon.strings.stateScanning;
    } else if (!this.dataItem.inProgress) {
      let stateLabel = function () {
        let s = DownloadsCommon.strings;
        switch (this.dataItem.state) {
          case nsIDM.DOWNLOAD_FAILED:           return s.stateFailed;
          case nsIDM.DOWNLOAD_CANCELED:         return s.stateCanceled;
          case nsIDM.DOWNLOAD_BLOCKED_PARENTAL: return s.stateBlockedParentalControls;
          case nsIDM.DOWNLOAD_BLOCKED_POLICY:   return s.stateBlockedPolicy;
          case nsIDM.DOWNLOAD_DIRTY:            return s.stateDirty;
          case nsIDM.DOWNLOAD_FINISHED:         return this._fileSizeText;
        }
        return null;
      }.apply(this);

      let [displayHost, fullHost] =
        DownloadUtils.getURIHost(this.dataItem.referrer || this.dataItem.uri);

      let end = new Date(this.dataItem.endTime);
      let [displayDate, fullDate] = DownloadUtils.getReadableDates(end);

      // We use the same XUL label to display the state, the host name, and the
      // end time, for example "Canceled - 222.net - 11:15" or "1.1 MB -
      // website2.com - Yesterday".  We show the full host and the complete date
      // in the tooltip.
      let firstPart = DownloadsCommon.strings.statusSeparator(stateLabel,
                                                              displayHost);
      status = DownloadsCommon.strings.statusSeparator(firstPart, displayDate);
      statusTip = DownloadsCommon.strings.statusSeparator(fullHost, fullDate);
    }

    this._element.setAttribute("status", status);
    this._element.setAttribute("statusTip", statusTip || status);
  },

  /**
   * Localized string representing the total size of completed downloads, for
   * example "1.5 MB" or "Unknown size".
   */
  get _fileSizeText()
  {
    // Display the file size, but show "Unknown" for negative sizes.
    let fileSize = this.dataItem.maxBytes;
    if (fileSize < 0) {
      return DownloadsCommon.strings.sizeUnknown;
    }
    let [size, unit] = DownloadUtils.convertByteUnits(fileSize);
    return DownloadsCommon.strings.sizeWithUnits(size, unit);
  }
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadsViewController

/**
 * Handles part of the user interaction events raised by the downloads list
 * widget, in particular the "commands" that apply to multiple items, and
 * dispatches the commands that apply to individual items.
 */
const DownloadsViewController = {
  //////////////////////////////////////////////////////////////////////////////
  //// Initialization and termination

  initialize: function DVC_initialize()
  {
    window.controllers.insertControllerAt(0, this);
  },

  terminate: function DVC_terminate()
  {
    window.controllers.removeController(this);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIController

  supportsCommand: function DVC_supportsCommand(aCommand)
  {
    // Firstly, determine if this is a command that we can handle.
    if (!(aCommand in this.commands) &&
        !(aCommand in DownloadsViewItemController.prototype.commands)) {
      return false;
    }
    // Secondly, determine if focus is on a control in the downloads list.
    let element = document.commandDispatcher.focusedElement;
    while (element && element != DownloadsView.richListBox) {
      element = element.parentNode;
    }
    // We should handle the command only if the downloads list is among the
    // ancestors of the focused element.
    return !!element;
  },

  isCommandEnabled: function DVC_isCommandEnabled(aCommand)
  {
    // Handle commands that are not selection-specific.
    if (aCommand == "downloadsCmd_clearList") {
      return Services.downloads.canCleanUp;
    }

    // Other commands are selection-specific.
    let element = DownloadsView.richListBox.selectedItem;
    return element &&
           new DownloadsViewItemController(element).isCommandEnabled(aCommand);
  },

  doCommand: function DVC_doCommand(aCommand)
  {
    // If this command is not selection-specific, execute it.
    if (aCommand in this.commands) {
      this.commands[aCommand].apply(this);
      return;
    }

    // Other commands are selection-specific.
    let element = DownloadsView.richListBox.selectedItem;
    if (element) {
      // The doCommand function also checks if the command is enabled.
      new DownloadsViewItemController(element).doCommand(aCommand);
    }
  },

  onEvent: function () { },

  //////////////////////////////////////////////////////////////////////////////
  //// Other functions

  updateCommands: function DVC_updateCommands()
  {
    Object.keys(this.commands).forEach(goUpdateCommand);
    Object.keys(DownloadsViewItemController.prototype.commands)
          .forEach(goUpdateCommand);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Selection-independent commands

  /**
   * This object contains one key for each command that operates regardless of
   * the currently selected item in the list.
   */
  commands: {
    downloadsCmd_clearList: function DVC_downloadsCmd_clearList()
    {
      Services.downloads.cleanUp();
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadsViewItemController

/**
 * Handles all the user interaction events, in particular the "commands",
 * related to a single item in the downloads list widgets.
 */
function DownloadsViewItemController(aElement) {
  let downloadId = aElement.getAttribute("downloadId");
  this.dataItem = DownloadsCommon.data.dataItems[downloadId];
}

DownloadsViewItemController.prototype = {
  //////////////////////////////////////////////////////////////////////////////
  //// Constants

  get kPrefBdmAlertOnExeOpen() "browser.download.manager.alertOnEXEOpen",
  get kPrefBdmScanWhenDone() "browser.download.manager.scanWhenDone",

  //////////////////////////////////////////////////////////////////////////////
  //// Command dispatching

  /**
   * The DownloadDataItem controlled by this object.
   */
  dataItem: null,

  isCommandEnabled: function DVIC_isCommandEnabled(aCommand)
  {
    switch (aCommand) {
      case "downloadsCmd_open": {
        return this.dataItem.openable && this.dataItem.localFile.exists();
      }
      case "downloadsCmd_show": {
        return this.dataItem.localFile.exists();
      }
      case "downloadsCmd_pauseResume":
        return this.dataItem.inProgress && this.dataItem.resumable;
      case "downloadsCmd_retry":
        return this.dataItem.canRetry;
      case "downloadsCmd_openReferrer":
        return !!this.dataItem.referrer;
      case "cmd_delete":
      case "downloadsCmd_cancel":
      case "downloadsCmd_copyLocation":
      case "downloadsCmd_doDefault":
        return true;
    }
    return false;
  },

  doCommand: function DVIC_doCommand(aCommand)
  {
    if (this.isCommandEnabled(aCommand)) {
      this.commands[aCommand].apply(this);
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Item commands

  /**
   * This object contains one key for each command that operates on this item.
   *
   * In commands, the "this" identifier points to the controller item.
   */
  commands: {
    cmd_delete: function DVIC_cmd_delete()
    {
      this.commands.downloadsCmd_cancel.apply(this);

      Services.downloads.removeDownload(this.dataItem.downloadId);
    },

    downloadsCmd_cancel: function DVIC_downloadsCmd_cancel()
    {
      if (this.dataItem.inProgress) {
        Services.downloads.cancelDownload(this.dataItem.downloadId);

        // It is possible that in some cases the Download Manager service
        // doesn't delete the file from disk when canceling.  See bug 732924.
        try {
          let localFile = this.dataItem.localFile;
          if (localFile.exists()) {
            localFile.remove(false);
          }
        } catch (ex) { }
      }
    },

    downloadsCmd_open: function DVIC_downloadsCmd_open()
    {
      // Confirm opening executable files if required.
      let localFile = this.dataItem.localFile;
      if (localFile.isExecutable()) {
        let showAlert = true;
        try {
          showAlert = Services.prefs.getBoolPref(this.kPrefBdmAlertOnExeOpen);
        } catch (ex) { }

        // On Vista and above, we rely on native security prompting for
        // downloaded content unless it's disabled.
        if (DownloadsCommon.isWinVistaOrHigher) {
          try {
            if (Services.prefs.getBoolPref(this.kPrefBdmScanWhenDone)) {
              showAlert = false;
            }
          } catch (ex) { }
        }

        if (showAlert) {
          let name = this.dataItem.target;
          let message =
              DownloadsCommon.strings.fileExecutableSecurityWarning(name, name);
          let title =
              DownloadsCommon.strings.fileExecutableSecurityWarningTitle;
          let dontAsk =
              DownloadsCommon.strings.fileExecutableSecurityWarningDontAsk;

          let checkbox = { value: false };
          let open = Services.prompt.confirmCheck(window, title, message,
                                                  dontAsk, checkbox);
          if (!open) {
            return;
          }

          Services.prefs.setBoolPref(this.kPrefBdmAlertOnExeOpen,
                                     !checkbox.value);
        }
      }

      // Actually open the file.
      try {
        let launched = false;
        try {
          let mimeInfo = this.dataItem.download.MIMEInfo;
          if (mimeInfo.preferredAction == mimeInfo.useHelperApp) {
            mimeInfo.launchWithFile(localFile);
            launched = true;
          }
        } catch (ex) { }
        if (!launched) {
          localFile.launch();
        }
      } catch (ex) {
        // If launch fails, try sending it through the system's external "file:"
        // URL handler.
        this._openExternal(localFile);
      }
    },

    downloadsCmd_show: function DVIC_downloadsCmd_show()
    {
      let localFile = this.dataItem.localFile;

      try {
        // Show the directory containing the file and select the file.
        localFile.reveal();
      } catch (ex) {
        // If reveal fails for some reason (e.g., it's not implemented on unix
        // or the file doesn't exist), try using the parent if we have it.
        let parent = localFile.parent.QueryInterface(Ci.nsILocalFile);
        if (parent) {
          try {
            // Open the parent directory to show where the file should be.
            parent.launch();
          } catch (ex) {
            // If launch also fails (probably because it's not implemented), let
            // the OS handler try to open the parent.
            this._openExternal(parent);
          }
        }
      }
    },

    downloadsCmd_pauseResume: function DVIC_downloadsCmd_pauseResume()
    {
      if (this.dataItem.paused) {
        Services.downloads.resumeDownload(this.dataItem.downloadId);
      } else {
        Services.downloads.pauseDownload(this.dataItem.downloadId);
      }
    },

    downloadsCmd_retry: function DVIC_downloadsCmd_retry()
    {
      Services.downloads.retryDownload(this.dataItem.downloadId);
    },

    downloadsCmd_openReferrer: function DVIC_downloadsCmd_openReferrer()
    {
      openURL(this.dataItem.referrer);
    },

    downloadsCmd_copyLocation: function DVIC_downloadsCmd_copyLocation()
    {
      let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"]
                      .getService(Ci.nsIClipboardHelper);
      clipboard.copyString(this.dataItem.uri, document);
    },

    downloadsCmd_doDefault: function DVIC_downloadsCmd_doDefault()
    {
      const nsIDM = Ci.nsIDownloadManager;

      // Determine the default command for the current item.
      let defaultCommand = function () {
        switch (this.dataItem.state) {
          case nsIDM.DOWNLOAD_NOTSTARTED:       return "downloadsCmd_cancel";
          case nsIDM.DOWNLOAD_DOWNLOADING:      return "downloadsCmd_show";
          case nsIDM.DOWNLOAD_FINISHED:         return "downloadsCmd_open";
          case nsIDM.DOWNLOAD_FAILED:           return "downloadsCmd_retry";
          case nsIDM.DOWNLOAD_CANCELED:         return "downloadsCmd_retry";
          case nsIDM.DOWNLOAD_PAUSED:           return "downloadsCmd_pauseResume";
          case nsIDM.DOWNLOAD_QUEUED:           return "downloadsCmd_cancel";
          case nsIDM.DOWNLOAD_BLOCKED_PARENTAL: return "downloadsCmd_openReferrer";
          case nsIDM.DOWNLOAD_SCANNING:         return "downloadsCmd_show";
          case nsIDM.DOWNLOAD_DIRTY:            return "downloadsCmd_openReferrer";
          case nsIDM.DOWNLOAD_BLOCKED_POLICY:   return "downloadsCmd_openReferrer";
        }
        return null;
      }.apply(this);
      // Invoke the command.
      this.doCommand(defaultCommand);
    }
  },

  /**
   * Support function to open the specified nsIFile.
   */
  _openExternal: function DVIC_openExternal(aFile)
  {
    let protocolSvc = Cc["@mozilla.org/uriloader/external-protocol-service;1"]
                      .getService(Ci.nsIExternalProtocolService);
    protocolSvc.loadUrl(makeFileURI(aFile));
  }
};
