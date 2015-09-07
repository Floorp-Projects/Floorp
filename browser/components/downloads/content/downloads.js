/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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

/**
 * A few words on focus and focusrings
 *
 * We do quite a few hacks in the Downloads Panel for focusrings. In fact, we
 * basically suppress most if not all XUL-level focusrings, and style/draw
 * them ourselves (using :focus instead of -moz-focusring). There are a few
 * reasons for this:
 *
 * 1) Richlists on OSX don't have focusrings; instead, they are shown as
 *    selected. This makes for some ambiguity when we have a focused/selected
 *    item in the list, and the mouse is hovering a completed download (which
 *    highlights).
 * 2) Windows doesn't show focusrings until after the first time that tab is
 *    pressed (and by then you're focusing the second item in the panel).
 * 3) Richlistbox sets -moz-focusring even when we select it with a mouse.
 *
 * In general, the desired behaviour is to focus the first item after pressing
 * tab/down, and show that focus with a ring. Then, if the mouse moves over
 * the panel, to hide that focus ring; essentially resetting us to the state
 * before pressing the key.
 *
 * We end up capturing the tab/down key events, and preventing their default
 * behaviour. We then set a "keyfocus" attribute on the panel, which allows
 * us to draw a ring around the currently focused element. If the panel is
 * closed or the mouse moves over the panel, we remove the attribute.
 */

"use strict";

let { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DownloadsCommon",
                                  "resource:///modules/DownloadsCommon.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadsViewUI",
                                  "resource:///modules/DownloadsViewUI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

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
  get kStateWaitingData() 2,
  /** The panel is almost shown - we're just waiting to get a handle on the
      anchor. */
  get kStateWaitingAnchor() 3,
  /** The panel is open. */
  get kStateShown() 4,

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
  initialize(aCallback) {
    DownloadsCommon.log("Attempting to initialize DownloadsPanel for a window.");
    if (this._state != this.kStateUninitialized) {
      DownloadsCommon.log("DownloadsPanel is already initialized.");
      DownloadsOverlayLoader.ensureOverlayLoaded(this.kDownloadsOverlay,
                                                 aCallback);
      return;
    }
    this._state = this.kStateHidden;

    window.addEventListener("unload", this.onWindowUnload, false);

    // Load and resume active downloads if required.  If there are downloads to
    // be shown in the panel, they will be loaded asynchronously.
    DownloadsCommon.initializeAllDataLinks();

    // Now that data loading has eventually started, load the required XUL
    // elements and initialize our views.
    DownloadsCommon.log("Ensuring DownloadsPanel overlay loaded.");
    DownloadsOverlayLoader.ensureOverlayLoaded(this.kDownloadsOverlay, () => {
      DownloadsViewController.initialize();
      DownloadsCommon.log("Attaching DownloadsView...");
      DownloadsCommon.getData(window).addView(DownloadsView);
      DownloadsCommon.getSummary(window, DownloadsView.kItemCountLimit)
                     .addView(DownloadsSummary);
      DownloadsCommon.log("DownloadsView attached - the panel for this window",
                          "should now see download items come in.");
      DownloadsPanel._attachEventListeners();
      DownloadsCommon.log("DownloadsPanel initialized.");
      aCallback();
    });
  },

  /**
   * Closes the downloads panel and frees the internal resources related to the
   * downloads.  The downloads panel can be reopened later, even after this
   * function has been called.
   */
  terminate() {
    DownloadsCommon.log("Attempting to terminate DownloadsPanel for a window.");
    if (this._state == this.kStateUninitialized) {
      DownloadsCommon.log("DownloadsPanel was never initialized. Nothing to do.");
      return;
    }

    window.removeEventListener("unload", this.onWindowUnload, false);

    // Ensure that the panel is closed before shutting down.
    this.hidePanel();

    DownloadsViewController.terminate();
    DownloadsCommon.getData(window).removeView(DownloadsView);
    DownloadsCommon.getSummary(window, DownloadsView.kItemCountLimit)
                   .removeView(DownloadsSummary);
    this._unattachEventListeners();

    this._state = this.kStateUninitialized;

    DownloadsSummary.active = false;
    DownloadsCommon.log("DownloadsPanel terminated.");
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Panel interface

  /**
   * Main panel element in the browser window, or null if the panel overlay
   * hasn't been loaded yet.
   */
  get panel() {
    // If the downloads panel overlay hasn't loaded yet, just return null
    // without resetting this.panel.
    let downloadsPanel = document.getElementById("downloadsPanel");
    if (!downloadsPanel)
      return null;

    delete this.panel;
    return this.panel = downloadsPanel;
  },

  /**
   * Starts opening the downloads panel interface, anchored to the downloads
   * button of the browser window.  The list of downloads to display is
   * initialized the first time this method is called, and the panel is shown
   * only when data is ready.
   */
  showPanel() {
    DownloadsCommon.log("Opening the downloads panel.");

    if (this.isPanelShowing) {
      DownloadsCommon.log("Panel is already showing - focusing instead.");
      this._focusPanel();
      return;
    }

    this.initialize(() => {
      // Delay displaying the panel because this function will sometimes be
      // called while another window is closing (like the window for selecting
      // whether to save or open the file), and that would cause the panel to
      // close immediately.
      setTimeout(() => this._openPopupIfDataReady(), 0);
    });

    DownloadsCommon.log("Waiting for the downloads panel to appear.");
    this._state = this.kStateWaitingData;
  },

  /**
   * Hides the downloads panel, if visible, but keeps the internal state so that
   * the panel can be reopened quickly if required.
   */
  hidePanel() {
    DownloadsCommon.log("Closing the downloads panel.");

    if (!this.isPanelShowing) {
      DownloadsCommon.log("Downloads panel is not showing - nothing to do.");
      return;
    }

    this.panel.hidePopup();

    // Ensure that we allow the panel to be reopened.  Note that, if the popup
    // was open, then the onPopupHidden event handler has already updated the
    // current state, otherwise we must update the state ourselves.
    this._state = this.kStateHidden;
    DownloadsCommon.log("Downloads panel is now closed.");
  },

  /**
   * Indicates whether the panel is shown or will be shown.
   */
  get isPanelShowing() {
    return this._state == this.kStateWaitingData ||
           this._state == this.kStateWaitingAnchor ||
           this._state == this.kStateShown;
  },

  /**
   * Returns whether the user has started keyboard navigation.
   */
  get keyFocusing() {
    return this.panel.hasAttribute("keyfocus");
  },

  /**
   * Set to true if the user has started keyboard navigation, and we should be
   * showing focusrings in the panel. Also adds a mousemove event handler to
   * the panel which disables keyFocusing.
   */
  set keyFocusing(aValue) {
    if (aValue) {
      this.panel.setAttribute("keyfocus", "true");
      this.panel.addEventListener("mousemove", this);
    } else {
      this.panel.removeAttribute("keyfocus");
      this.panel.removeEventListener("mousemove", this);
    }
    return aValue;
  },

  /**
   * Handles the mousemove event for the panel, which disables focusring
   * visualization.
   */
  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "mousemove":
        this.keyFocusing = false;
        break;
      case "keydown":
        return this._onKeyDown(aEvent);
      case "keypress":
        return this._onKeyPress(aEvent);
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Callback functions from DownloadsView

  /**
   * Called after data loading finished.
   */
  onViewLoadCompleted() {
    this._openPopupIfDataReady();
  },

  //////////////////////////////////////////////////////////////////////////////
  //// User interface event functions

  onWindowUnload() {
    // This function is registered as an event listener, we can't use "this".
    DownloadsPanel.terminate();
  },

  onPopupShown(aEvent) {
    // Ignore events raised by nested popups.
    if (aEvent.target != aEvent.currentTarget) {
      return;
    }

    DownloadsCommon.log("Downloads panel has shown.");
    this._state = this.kStateShown;

    // Since at most one popup is open at any given time, we can set globally.
    DownloadsCommon.getIndicatorData(window).attentionSuppressed = true;

    // Ensure that the first item is selected when the panel is focused.
    if (DownloadsView.richListBox.itemCount > 0 &&
        DownloadsView.richListBox.selectedIndex == -1) {
      DownloadsView.richListBox.selectedIndex = 0;
    }

    this._focusPanel();
  },

  onPopupHidden(aEvent) {
    // Ignore events raised by nested popups.
    if (aEvent.target != aEvent.currentTarget) {
      return;
    }

    DownloadsCommon.log("Downloads panel has hidden.");

    // Removes the keyfocus attribute so that we stop handling keyboard
    // navigation.
    this.keyFocusing = false;

    // Since at most one popup is open at any given time, we can set globally.
    DownloadsCommon.getIndicatorData(window).attentionSuppressed = false;

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
  showDownloadsHistory() {
    DownloadsCommon.log("Showing download history.");
    // Hide the panel before showing another window, otherwise focus will return
    // to the browser window when the panel closes automatically.
    this.hidePanel();

    BrowserDownloadsUI();
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Internal functions

  /**
   * Attach event listeners to a panel element. These listeners should be
   * removed in _unattachEventListeners. This is called automatically after the
   * panel has successfully loaded.
   */
  _attachEventListeners() {
    // Handle keydown to support accel-V.
    this.panel.addEventListener("keydown", this, false);
    // Handle keypress to be able to preventDefault() events before they reach
    // the richlistbox, for keyboard navigation.
    this.panel.addEventListener("keypress", this, false);
  },

  /**
   * Unattach event listeners that were added in _attachEventListeners. This
   * is called automatically on panel termination.
   */
  _unattachEventListeners() {
    this.panel.removeEventListener("keydown", this, false);
    this.panel.removeEventListener("keypress", this, false);
  },

  _onKeyPress(aEvent) {
    // Handle unmodified keys only.
    if (aEvent.altKey || aEvent.ctrlKey || aEvent.shiftKey || aEvent.metaKey) {
      return;
    }

    let richListBox = DownloadsView.richListBox;

    // If the user has pressed the tab, up, or down cursor key, start keyboard
    // navigation, thus enabling focusrings in the panel.  Keyboard navigation
    // is automatically disabled if the user moves the mouse on the panel, or
    // if the panel is closed.
    if ((aEvent.keyCode == Ci.nsIDOMKeyEvent.DOM_VK_TAB ||
        aEvent.keyCode == Ci.nsIDOMKeyEvent.DOM_VK_UP ||
        aEvent.keyCode == Ci.nsIDOMKeyEvent.DOM_VK_DOWN) &&
        !this.keyFocusing) {
      this.keyFocusing = true;
      // Ensure there's a selection, we will show the focus ring around it and
      // prevent the richlistbox from changing the selection.
      if (DownloadsView.richListBox.selectedIndex == -1) {
        DownloadsView.richListBox.selectedIndex = 0;
      }
      aEvent.preventDefault();
      return;
    }

    if (aEvent.keyCode == Ci.nsIDOMKeyEvent.DOM_VK_DOWN) {
      // If the last element in the list is selected, or the footer is already
      // focused, focus the footer.
      if (richListBox.selectedItem === richListBox.lastChild ||
          document.activeElement.parentNode.id === "downloadsFooter") {
        DownloadsFooter.focus();
        aEvent.preventDefault();
        return;
      }
    }

    // Pass keypress events to the richlistbox view when it's focused.
    if (document.activeElement === richListBox) {
      DownloadsView.onDownloadKeyPress(aEvent);
    }
  },

  /**
   * Keydown listener that listens for the keys to start key focusing, as well
   * as the the accel-V "paste" event, which initiates a file download if the
   * pasted item can be resolved to a URI.
   */
  _onKeyDown(aEvent) {
    // If the footer is focused and the downloads list has at least 1 element
    // in it, focus the last element in the list when going up.
    if (aEvent.keyCode == Ci.nsIDOMKeyEvent.DOM_VK_UP &&
        document.activeElement.parentNode.id === "downloadsFooter" &&
        DownloadsView.richListBox.firstChild) {
      DownloadsView.richListBox.focus();
      DownloadsView.richListBox.selectedItem = DownloadsView.richListBox.lastChild;
      aEvent.preventDefault();
      return;
    }

    let pasting = aEvent.keyCode == Ci.nsIDOMKeyEvent.DOM_VK_V &&
#ifdef XP_MACOSX
                  aEvent.metaKey;
#else
                  aEvent.ctrlKey;
#endif

    if (!pasting) {
      return;
    }

    DownloadsCommon.log("Received a paste event.");

    let trans = Cc["@mozilla.org/widget/transferable;1"]
                  .createInstance(Ci.nsITransferable);
    trans.init(null);
    let flavors = ["text/x-moz-url", "text/unicode"];
    flavors.forEach(trans.addDataFlavor);
    Services.clipboard.getData(trans, Services.clipboard.kGlobalClipboard);
    // Getting the data or creating the nsIURI might fail
    try {
      let data = {};
      trans.getAnyTransferData({}, data, {});
      let [url, name] = data.value
                            .QueryInterface(Ci.nsISupportsString)
                            .data
                            .split("\n");
      if (!url) {
        return;
      }

      let uri = NetUtil.newURI(url);
      DownloadsCommon.log("Pasted URL seems valid. Starting download.");
      DownloadURL(uri.spec, name, document);
    } catch (ex) {}
  },

  /**
   * Move focus to the main element in the downloads panel, unless another
   * element in the panel is already focused.
   */
  _focusPanel() {
    // We may be invoked while the panel is still waiting to be shown.
    if (this._state != this.kStateShown) {
      return;
    }

    let element = document.commandDispatcher.focusedElement;
    while (element && element != this.panel) {
      element = element.parentNode;
    }
    if (!element) {
      if (DownloadsView.richListBox.itemCount > 0) {
        DownloadsView.richListBox.focus();
      } else {
        DownloadsFooter.focus();
      }
    }
  },

  /**
   * Opens the downloads panel when data is ready to be displayed.
   */
  _openPopupIfDataReady() {
    // We don't want to open the popup if we already displayed it, or if we are
    // still loading data.
    if (this._state != this.kStateWaitingData || DownloadsView.loading) {
      return;
    }

    this._state = this.kStateWaitingAnchor;

    // Ensure the anchor is visible.  If that is not possible, show the panel
    // anchored to the top area of the window, near the default anchor position.
    DownloadsButton.getAnchor(anchor => {
      // If somehow we've switched states already (by getting a panel hiding
      // event before an overlay is loaded, for example), bail out.
      if (this._state != this.kStateWaitingAnchor) {
        return;
      }

      // At this point, if the window is minimized, opening the panel could fail
      // without any notification, and there would be no way to either open or
      // close the panel any more.  To prevent this, check if the window is
      // minimized and in that case force the panel to the closed state.
      if (window.windowState == Ci.nsIDOMChromeWindow.STATE_MINIMIZED) {
        DownloadsButton.releaseAnchor();
        this._state = this.kStateHidden;
        return;
      }

      if (!anchor) {
        DownloadsCommon.error("Downloads button cannot be found.");
        return;
      }

      // When the panel is opened, we check if the target files of visible items
      // still exist, and update the allowed items interactions accordingly.  We
      // do these checks on a background thread, and don't prevent the panel to
      // be displayed while these checks are being performed.
      for (let viewItem of DownloadsView._visibleViewItems.values()) {
        viewItem.download.refresh().catch(Cu.reportError);
      }

      DownloadsCommon.log("Opening downloads panel popup.");
      this.panel.openPopup(anchor, "bottomcenter topright", 0, 0, false, null);
    });
  },
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
  ensureOverlayLoaded(aOverlay, aCallback) {
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

    this._overlayLoading = true;
    DownloadsCommon.log("Loading overlay ", aOverlay);
    document.loadOverlay(aOverlay, () => {
      this._overlayLoading = false;
      this._loadedOverlays[aOverlay] = true;

      this.processPendingRequests();
    });
  },

  /**
   * Re-processes all the currently pending requests, invoking the callbacks
   * and/or loading more overlays as needed.  In most cases, there will be a
   * single request for one overlay, that will be processed immediately.
   */
  processPendingRequests() {
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
  },
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
   * Maximum number of items shown by the list at any given time.
   */
  kItemCountLimit: 3,

  /**
   * Indicates whether we are still loading downloads data asynchronously.
   */
  loading: false,

  /**
   * Ordered array of all Download objects.  We need to keep this array because
   * only a limited number of items are shown at once, and if an item that is
   * currently visible is removed from the list, we might need to take another
   * item from the array and make it appear at the bottom.
   */
  _downloads: [],

  /**
   * Associates the visible Download objects with their corresponding
   * DownloadsViewItem object.  There is a limited number of view items in the
   * panel at any given time.
   */
  _visibleViewItems: new Map(),

  /**
   * Called when the number of items in the list changes.
   */
  _itemCountChanged() {
    DownloadsCommon.log("The downloads item count has changed - we are tracking",
                        this._downloads.length, "downloads in total.");
    let count = this._downloads.length;
    let hiddenCount = count - this.kItemCountLimit;

    if (count > 0) {
      DownloadsCommon.log("Setting the panel's hasdownloads attribute to true.");
      DownloadsPanel.panel.setAttribute("hasdownloads", "true");
    } else {
      DownloadsCommon.log("Removing the panel's hasdownloads attribute.");
      DownloadsPanel.panel.removeAttribute("hasdownloads");
    }

    // If we've got some hidden downloads, we should activate the
    // DownloadsSummary. The DownloadsSummary will determine whether or not
    // it's appropriate to actually display the summary.
    DownloadsSummary.active = hiddenCount > 0;
  },

  /**
   * Element corresponding to the list of downloads.
   */
  get richListBox() {
    delete this.richListBox;
    return this.richListBox = document.getElementById("downloadsListBox");
  },

  /**
   * Element corresponding to the button for showing more downloads.
   */
  get downloadsHistory() {
    delete this.downloadsHistory;
    return this.downloadsHistory = document.getElementById("downloadsHistory");
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Callback functions from DownloadsData

  /**
   * Called before multiple downloads are about to be loaded.
   */
  onDataLoadStarting() {
    DownloadsCommon.log("onDataLoadStarting called for DownloadsView.");
    this.loading = true;
  },

  /**
   * Called after data loading finished.
   */
  onDataLoadCompleted() {
    DownloadsCommon.log("onDataLoadCompleted called for DownloadsView.");

    this.loading = false;

    // We suppressed item count change notifications during the batch load, at
    // this point we should just call the function once.
    this._itemCountChanged();

    // Notify the panel that all the initially available downloads have been
    // loaded.  This ensures that the interface is visible, if still required.
    DownloadsPanel.onViewLoadCompleted();
  },

  /**
   * Called when a new download data item is available, either during the
   * asynchronous data load or when a new download is started.
   *
   * @param aDownload
   *        Download object that was just added.
   * @param aNewest
   *        When true, indicates that this item is the most recent and should be
   *        added in the topmost position.  This happens when a new download is
   *        started.  When false, indicates that the item is the least recent
   *        and should be appended.  The latter generally happens during the
   *        asynchronous data load.
   */
  onDownloadAdded(download, aNewest) {
    DownloadsCommon.log("A new download data item was added - aNewest =",
                        aNewest);

    if (aNewest) {
      this._downloads.unshift(download);
    } else {
      this._downloads.push(download);
    }

    let itemsNowOverflow = this._downloads.length > this.kItemCountLimit;
    if (aNewest || !itemsNowOverflow) {
      // The newly added item is visible in the panel and we must add the
      // corresponding element.  This is either because it is the first item, or
      // because it was added at the bottom but the list still doesn't overflow.
      this._addViewItem(download, aNewest);
    }
    if (aNewest && itemsNowOverflow) {
      // If the list overflows, remove the last item from the panel to make room
      // for the new one that we just added at the top.
      this._removeViewItem(this._downloads[this.kItemCountLimit]);
    }

    // For better performance during batch loads, don't update the count for
    // every item, because the interface won't be visible until load finishes.
    if (!this.loading) {
      this._itemCountChanged();
    }
  },

  onDownloadStateChanged(download) {
    let viewItem = this._visibleViewItems.get(download);
    if (viewItem) {
      viewItem.onStateChanged();
    }
  },

  onDownloadChanged(download) {
    let viewItem = this._visibleViewItems.get(download);
    if (viewItem) {
      viewItem.onChanged();
    }
  },

  /**
   * Called when a data item is removed.  Ensures that the widget associated
   * with the view item is removed from the user interface.
   *
   * @param download
   *        Download object that is being removed.
   */
  onDownloadRemoved(download) {
    DownloadsCommon.log("A download data item was removed.");

    let itemIndex = this._downloads.indexOf(download);
    this._downloads.splice(itemIndex, 1);

    if (itemIndex < this.kItemCountLimit) {
      // The item to remove is visible in the panel.
      this._removeViewItem(download);
      if (this._downloads.length >= this.kItemCountLimit) {
        // Reinsert the next item into the panel.
        this._addViewItem(this._downloads[this.kItemCountLimit - 1], false);
      }
    }

    this._itemCountChanged();
  },

  /**
   * Associates each richlistitem for a download with its corresponding
   * DownloadsViewItemController object.
   */
  _controllersForElements: new Map(),

  controllerForElement(element) {
    return this._controllersForElements.get(element);
  },

  /**
   * Creates a new view item associated with the specified data item, and adds
   * it to the top or the bottom of the list.
   */
  _addViewItem(download, aNewest)
  {
    DownloadsCommon.log("Adding a new DownloadsViewItem to the downloads list.",
                        "aNewest =", aNewest);

    let element = document.createElement("richlistitem");
    let viewItem = new DownloadsViewItem(download, element);
    this._visibleViewItems.set(download, viewItem);
    let viewItemController = new DownloadsViewItemController(download);
    this._controllersForElements.set(element, viewItemController);
    if (aNewest) {
      this.richListBox.insertBefore(element, this.richListBox.firstChild);
    } else {
      this.richListBox.appendChild(element);
    }
  },

  /**
   * Removes the view item associated with the specified data item.
   */
  _removeViewItem(download) {
    DownloadsCommon.log("Removing a DownloadsViewItem from the downloads list.");
    let element = this._visibleViewItems.get(download).element;
    let previousSelectedIndex = this.richListBox.selectedIndex;
    this.richListBox.removeChild(element);
    if (previousSelectedIndex != -1) {
      this.richListBox.selectedIndex = Math.min(previousSelectedIndex,
                                                this.richListBox.itemCount - 1);
    }
    this._visibleViewItems.delete(download);
    this._controllersForElements.delete(element);
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
  onDownloadCommand(aEvent, aCommand) {
    let target = aEvent.target;
    while (target.nodeName != "richlistitem") {
      target = target.parentNode;
    }
    DownloadsView.controllerForElement(target).doCommand(aCommand);
  },

  onDownloadClick(aEvent) {
    // Handle primary clicks only, and exclude the action button.
    if (aEvent.button == 0 &&
        !aEvent.originalTarget.hasAttribute("oncommand")) {
      goDoCommand("downloadsCmd_open");
    }
  },

  /**
   * Handles keypress events on a download item.
   */
  onDownloadKeyPress(aEvent) {
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

    if (aEvent.keyCode == KeyEvent.DOM_VK_RETURN) {
      goDoCommand("downloadsCmd_doDefault");
    }
  },

  /**
   * Mouse listeners to handle selection on hover.
   */
  onDownloadMouseOver(aEvent) {
    if (aEvent.originalTarget.parentNode == this.richListBox) {
      this.richListBox.selectedItem = aEvent.originalTarget;
    }
  },

  onDownloadMouseOut(aEvent) {
    if (aEvent.originalTarget.parentNode == this.richListBox) {
      // If the destination element is outside of the richlistitem, clear the
      // selection.
      let element = aEvent.relatedTarget;
      while (element && element != aEvent.originalTarget) {
        element = element.parentNode;
      }
      if (!element) {
        this.richListBox.selectedIndex = -1;
      }
    }
  },

  onDownloadContextMenu(aEvent) {
    let element = this.richListBox.selectedItem;
    if (!element) {
      return;
    }

    DownloadsViewController.updateCommands();

    // Set the state attribute so that only the appropriate items are displayed.
    let contextMenu = document.getElementById("downloadsContextMenu");
    contextMenu.setAttribute("state", element.getAttribute("state"));
    contextMenu.classList.toggle("temporary-block",
                                 element.classList.contains("temporary-block"));
  },

  onDownloadDragStart(aEvent) {
    let element = this.richListBox.selectedItem;
    if (!element) {
      return;
    }

    // We must check for existence synchronously because this is a DOM event.
    let file = new FileUtils.File(DownloadsView.controllerForElement(element)
                                               .download.target.path);
    if (!file.exists()) {
      return;
    }

    let dataTransfer = aEvent.dataTransfer;
    dataTransfer.mozSetDataAt("application/x-moz-file", file, 0);
    dataTransfer.effectAllowed = "copyMove";
    let spec = NetUtil.newURI(file).spec;
    dataTransfer.setData("text/uri-list", spec);
    dataTransfer.setData("text/plain", spec);
    dataTransfer.addElement(element);

    aEvent.stopPropagation();
  },
}

////////////////////////////////////////////////////////////////////////////////
//// DownloadsViewItem

/**
 * Builds and updates a single item in the downloads list widget, responding to
 * changes in the download state and real-time data.
 *
 * @param download
 *        Download object to be associated with the view item.
 * @param aElement
 *        XUL element corresponding to the single download item in the view.
 */
function DownloadsViewItem(download, aElement) {
  this.download = download;
  this.element = aElement;
  this.element._shell = this;

  this.element.setAttribute("type", "download");
  this.element.classList.add("download-state");

  this._updateState();
}

DownloadsViewItem.prototype = {
  __proto__: DownloadsViewUI.DownloadElementShell.prototype,

  /**
   * The XUL element corresponding to the associated richlistbox item.
   */
  _element: null,

  onStateChanged() {
    this.element.setAttribute("image", this.image);
    this.element.setAttribute("state",
                              DownloadsCommon.stateOfDownload(this.download));
  },

  onChanged() {
    // This cannot be placed within onStateChanged because
    // when a download goes from hasBlockedData to !hasBlockedData
    // it will still remain in the same state.
    this.element.classList.toggle("temporary-block",
                                  !!this.download.hasBlockedData);
    this._updateProgress();
  },
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

  initialize() {
    window.controllers.insertControllerAt(0, this);
  },

  terminate() {
    window.controllers.removeController(this);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIController

  supportsCommand(aCommand) {
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

  isCommandEnabled(aCommand) {
    // Handle commands that are not selection-specific.
    if (aCommand == "downloadsCmd_clearList") {
      return DownloadsCommon.getData(window).canRemoveFinished;
    }

    // Other commands are selection-specific.
    let element = DownloadsView.richListBox.selectedItem;
    return element && DownloadsView.controllerForElement(element)
                                   .isCommandEnabled(aCommand);
  },

  doCommand(aCommand) {
    // If this command is not selection-specific, execute it.
    if (aCommand in this.commands) {
      this.commands[aCommand].apply(this);
      return;
    }

    // Other commands are selection-specific.
    let element = DownloadsView.richListBox.selectedItem;
    if (element) {
      // The doCommand function also checks if the command is enabled.
      DownloadsView.controllerForElement(element).doCommand(aCommand);
    }
  },

  onEvent() {},

  //////////////////////////////////////////////////////////////////////////////
  //// Other functions

  updateCommands() {
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
    downloadsCmd_clearList() {
      DownloadsCommon.getData(window).removeFinished();
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadsViewItemController

/**
 * Handles all the user interaction events, in particular the "commands",
 * related to a single item in the downloads list widgets.
 */
function DownloadsViewItemController(download) {
  this.download = download;
}

DownloadsViewItemController.prototype = {
  isCommandEnabled(aCommand) {
    switch (aCommand) {
      case "downloadsCmd_open": {
        if (!this.download.succeeded) {
          return false;
        }

        let file = new FileUtils.File(this.download.target.path);
        return file.exists();
      }
      case "downloadsCmd_show": {
        let file = new FileUtils.File(this.download.target.path);
        if (file.exists()) {
          return true;
        }

        if (!this.download.target.partFilePath) {
          return false;
        }

        let partFile = new FileUtils.File(this.download.target.partFilePath);
        return partFile.exists();
      }
      case "downloadsCmd_pauseResume":
        return this.download.hasPartialData && !this.download.error;
      case "downloadsCmd_retry":
        return this.download.canceled || this.download.error;
      case "downloadsCmd_openReferrer":
        return !!this.download.source.referrer;
      case "cmd_delete":
      case "downloadsCmd_cancel":
      case "downloadsCmd_copyLocation":
      case "downloadsCmd_doDefault":
        return true;
      case "downloadsCmd_unblock":
      case "downloadsCmd_confirmBlock":
        return this.download.hasBlockedData;
    }
    return false;
  },

  doCommand(aCommand) {
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
    cmd_delete() {
      DownloadsCommon.removeAndFinalizeDownload(this.download);
      PlacesUtils.bhistory.removePage(
                             NetUtil.newURI(this.download.source.url));
    },

    downloadsCmd_cancel() {
      this.download.cancel().catch(() => {});
      this.download.removePartialData().catch(Cu.reportError);
    },

    downloadsCmd_unblock() {
      DownloadsPanel.hidePanel();
      DownloadsCommon.confirmUnblockDownload(DownloadsCommon.BLOCK_VERDICT_MALWARE,
                                             window).then((confirmed) => {
        if (confirmed) {
          return this.download.unblock();
        }
      }).catch(Cu.reportError);
    },

    downloadsCmd_confirmBlock() {
      this.download.confirmBlock().catch(Cu.reportError);
    },

    downloadsCmd_open() {
      this.download.launch().catch(Cu.reportError);

      // We explicitly close the panel here to give the user the feedback that
      // their click has been received, and we're handling the action.
      // Otherwise, we'd have to wait for the file-type handler to execute
      // before the panel would close. This also helps to prevent the user from
      // accidentally opening a file several times.
      DownloadsPanel.hidePanel();
    },

    downloadsCmd_show() {
      let file = new FileUtils.File(this.download.target.path);
      DownloadsCommon.showDownloadedFile(file);

      // We explicitly close the panel here to give the user the feedback that
      // their click has been received, and we're handling the action.
      // Otherwise, we'd have to wait for the operating system file manager
      // window to open before the panel closed. This also helps to prevent the
      // user from opening the containing folder several times.
      DownloadsPanel.hidePanel();
    },

    downloadsCmd_pauseResume() {
      if (this.download.stopped) {
        this.download.start();
      } else {
        this.download.cancel();
      }
    },

    downloadsCmd_retry() {
      this.download.start().catch(() => {});
    },

    downloadsCmd_openReferrer() {
      openURL(this.download.source.referrer);
    },

    downloadsCmd_copyLocation() {
      let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"]
                      .getService(Ci.nsIClipboardHelper);
      clipboard.copyString(this.download.source.url);
    },

    downloadsCmd_doDefault() {
      const nsIDM = Ci.nsIDownloadManager;

      // Determine the default command for the current item.
      let defaultCommand = function () {
        switch (DownloadsCommon.stateOfDownload(this.download)) {
          case nsIDM.DOWNLOAD_NOTSTARTED:       return "downloadsCmd_cancel";
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
        return "";
      }.apply(this);
      if (defaultCommand && this.isCommandEnabled(defaultCommand)) {
        this.doCommand(defaultCommand);
      }
    },
  },
};


////////////////////////////////////////////////////////////////////////////////
//// DownloadsSummary

/**
 * Manages the summary at the bottom of the downloads panel list if the number
 * of items in the list exceeds the panels limit.
 */
const DownloadsSummary = {

  /**
   * Sets the active state of the summary. When active, the summary subscribes
   * to the DownloadsCommon DownloadsSummaryData singleton.
   *
   * @param aActive
   *        Set to true to activate the summary.
   */
  set active(aActive) {
    if (aActive == this._active || !this._summaryNode) {
      return this._active;
    }
    if (aActive) {
      DownloadsCommon.getSummary(window, DownloadsView.kItemCountLimit)
                     .refreshView(this);
    } else {
      DownloadsFooter.showingSummary = false;
    }

    return this._active = aActive;
  },

  /**
   * Returns the active state of the downloads summary.
   */
  get active() this._active,

  _active: false,

  /**
   * Sets whether or not we show the progress bar.
   *
   * @param aShowingProgress
   *        True if we should show the progress bar.
   */
  set showingProgress(aShowingProgress) {
    if (aShowingProgress) {
      this._summaryNode.setAttribute("inprogress", "true");
    } else {
      this._summaryNode.removeAttribute("inprogress");
    }
    // If progress isn't being shown, then we simply do not show the summary.
    return DownloadsFooter.showingSummary = aShowingProgress;
  },

  /**
   * Sets the amount of progress that is visible in the progress bar.
   *
   * @param aValue
   *        A value between 0 and 100 to represent the progress of the
   *        summarized downloads.
   */
  set percentComplete(aValue) {
    if (this._progressNode) {
      this._progressNode.setAttribute("value", aValue);
    }
    return aValue;
  },

  /**
   * Sets the description for the download summary.
   *
   * @param aValue
   *        A string representing the description of the summarized
   *        downloads.
   */
  set description(aValue) {
    if (this._descriptionNode) {
      this._descriptionNode.setAttribute("value", aValue);
      this._descriptionNode.setAttribute("tooltiptext", aValue);
    }
    return aValue;
  },

  /**
   * Sets the details for the download summary, such as the time remaining,
   * the amount of bytes transferred, etc.
   *
   * @param aValue
   *        A string representing the details of the summarized
   *        downloads.
   */
  set details(aValue) {
    if (this._detailsNode) {
      this._detailsNode.setAttribute("value", aValue);
      this._detailsNode.setAttribute("tooltiptext", aValue);
    }
    return aValue;
  },

  /**
   * Focuses the root element of the summary.
   */
  focus() {
    if (this._summaryNode) {
      this._summaryNode.focus();
    }
  },

  /**
   * Respond to keydown events on the Downloads Summary node.
   *
   * @param aEvent
   *        The keydown event being handled.
   */
  onKeyDown(aEvent) {
    if (aEvent.charCode == " ".charCodeAt(0) ||
        aEvent.keyCode == KeyEvent.DOM_VK_RETURN) {
      DownloadsPanel.showDownloadsHistory();
    }
  },

  /**
   * Respond to click events on the Downloads Summary node.
   *
   * @param aEvent
   *        The click event being handled.
   */
  onClick(aEvent) {
    DownloadsPanel.showDownloadsHistory();
  },

  /**
   * Element corresponding to the root of the downloads summary.
   */
  get _summaryNode() {
    let node = document.getElementById("downloadsSummary");
    if (!node) {
      return null;
    }
    delete this._summaryNode;
    return this._summaryNode = node;
  },

  /**
   * Element corresponding to the progress bar in the downloads summary.
   */
  get _progressNode() {
    let node = document.getElementById("downloadsSummaryProgress");
    if (!node) {
      return null;
    }
    delete this._progressNode;
    return this._progressNode = node;
  },

  /**
   * Element corresponding to the main description of the downloads
   * summary.
   */
  get _descriptionNode() {
    let node = document.getElementById("downloadsSummaryDescription");
    if (!node) {
      return null;
    }
    delete this._descriptionNode;
    return this._descriptionNode = node;
  },

  /**
   * Element corresponding to the secondary description of the downloads
   * summary.
   */
  get _detailsNode() {
    let node = document.getElementById("downloadsSummaryDetails");
    if (!node) {
      return null;
    }
    delete this._detailsNode;
    return this._detailsNode = node;
  }
}

////////////////////////////////////////////////////////////////////////////////
//// DownloadsFooter

/**
 * Manages events sent to to the footer vbox, which contains both the
 * DownloadsSummary as well as the "Show All Downloads" button.
 */
const DownloadsFooter = {

  /**
   * Focuses the appropriate element within the footer. If the summary
   * is visible, focus it. If not, focus the "Show All Downloads"
   * button.
   */
  focus() {
    if (this._showingSummary) {
      DownloadsSummary.focus();
    } else {
      DownloadsView.downloadsHistory.focus();
    }
  },

  _showingSummary: false,

  /**
   * Sets whether or not the Downloads Summary should be displayed in the
   * footer. If not, the "Show All Downloads" button is shown instead.
   */
  set showingSummary(aValue) {
    if (this._footerNode) {
      if (aValue) {
        this._footerNode.setAttribute("showingsummary", "true");
      } else {
        this._footerNode.removeAttribute("showingsummary");
      }
      this._showingSummary = aValue;
    }
    return aValue;
  },

  /**
   * Element corresponding to the footer of the downloads panel.
   */
  get _footerNode() {
    let node = document.getElementById("downloadsFooter");
    if (!node) {
      return null;
    }
    delete this._footerNode;
    return this._footerNode = node;
  }
};
