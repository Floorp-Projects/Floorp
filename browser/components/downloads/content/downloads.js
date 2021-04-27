/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env mozilla/browser-window */

/**
 * Handles the Downloads panel user interface for each browser window.
 *
 * This file includes the following constructors and global objects:
 *
 * DownloadsPanel
 * Main entry point for the downloads panel interface.
 *
 * DownloadsView
 * Builds and updates the downloads list widget, responding to changes in the
 * download state and real-time data.  In addition, handles part of the user
 * interaction events raised by the downloads list widget.
 *
 * DownloadsViewItem
 * Builds and updates a single item in the downloads list widget, responding to
 * changes in the download state and real-time data, and handles the user
 * interaction events related to a single item in the downloads list widgets.
 *
 * DownloadsViewController
 * Handles part of the user interaction events raised by the downloads list
 * widget, in particular the "commands" that apply to multiple items, and
 * dispatches the commands that apply to individual items.
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

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "DownloadsViewUI",
  "resource:///modules/DownloadsViewUI.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "DownloadsSubview",
  "resource:///modules/DownloadsSubview.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "NetUtil",
  "resource://gre/modules/NetUtil.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);

// DownloadsPanel

/**
 * Main entry point for the downloads panel interface.
 */
var DownloadsPanel = {
  // Initialization and termination

  /**
   * Internal state of the downloads panel, based on one of the kState
   * constants.  This is not the same state as the XUL panel element.
   */
  _state: 0,

  /** The panel is not linked to downloads data yet. */
  get kStateUninitialized() {
    return 0;
  },
  /** This object is linked to data, but the panel is invisible. */
  get kStateHidden() {
    return 1;
  },
  /** The panel will be shown as soon as possible. */
  get kStateWaitingData() {
    return 2;
  },
  /** The panel is open. */
  get kStateShown() {
    return 3;
  },

  /**
   * Starts loading the download data in background, without opening the panel.
   * Use showPanel instead to load the data and open the panel at the same time.
   */
  initialize() {
    DownloadsCommon.log(
      "Attempting to initialize DownloadsPanel for a window."
    );
    if (this._state != this.kStateUninitialized) {
      DownloadsCommon.log("DownloadsPanel is already initialized.");
      return;
    }
    this._state = this.kStateHidden;

    window.addEventListener("unload", this.onWindowUnload);

    // Load and resume active downloads if required.  If there are downloads to
    // be shown in the panel, they will be loaded asynchronously.
    DownloadsCommon.initializeAllDataLinks();

    // Now that data loading has eventually started, load the required XUL
    // elements and initialize our views.

    this.panel.hidden = false;
    DownloadsViewController.initialize();
    DownloadsCommon.log("Attaching DownloadsView...");
    DownloadsCommon.getData(window).addView(DownloadsView);
    DownloadsCommon.getSummary(window, DownloadsView.kItemCountLimit).addView(
      DownloadsSummary
    );
    DownloadsCommon.log(
      "DownloadsView attached - the panel for this window",
      "should now see download items come in."
    );
    DownloadsPanel._attachEventListeners();
    DownloadsCommon.log("DownloadsPanel initialized.");
  },

  /**
   * Closes the downloads panel and frees the internal resources related to the
   * downloads.  The downloads panel can be reopened later, even after this
   * function has been called.
   */
  terminate() {
    DownloadsCommon.log("Attempting to terminate DownloadsPanel for a window.");
    if (this._state == this.kStateUninitialized) {
      DownloadsCommon.log(
        "DownloadsPanel was never initialized. Nothing to do."
      );
      return;
    }

    window.removeEventListener("unload", this.onWindowUnload);

    // Ensure that the panel is closed before shutting down.
    this.hidePanel();

    DownloadsViewController.terminate();
    DownloadsCommon.getData(window).removeView(DownloadsView);
    DownloadsCommon.getSummary(
      window,
      DownloadsView.kItemCountLimit
    ).removeView(DownloadsSummary);
    this._unattachEventListeners();

    this._state = this.kStateUninitialized;

    DownloadsSummary.active = false;
    DownloadsCommon.log("DownloadsPanel terminated.");
  },

  // Panel interface

  /**
   * Main panel element in the browser window.
   */
  get panel() {
    delete this.panel;
    return (this.panel = document.getElementById("downloadsPanel"));
  },

  /**
   * Starts opening the downloads panel interface, anchored to the downloads
   * button of the browser window.  The list of downloads to display is
   * initialized the first time this method is called, and the panel is shown
   * only when data is ready.
   */
  showPanel() {
    Services.telemetry.scalarAdd("downloads.panel_shown", 1);
    DownloadsCommon.log("Opening the downloads panel.");

    if (this.isPanelShowing) {
      DownloadsCommon.log("Panel is already showing - focusing instead.");
      this._focusPanel();
      return;
    }

    // As a belt-and-suspenders check, ensure the button is not hidden.
    DownloadsButton.unhide();

    this.initialize();
    // Delay displaying the panel because this function will sometimes be
    // called while another window is closing (like the window for selecting
    // whether to save or open the file), and that would cause the panel to
    // close immediately.
    setTimeout(() => this._openPopupIfDataReady(), 0);

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

    PanelMultiView.hidePopup(this.panel);

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
    return (
      this._state == this.kStateWaitingData || this._state == this.kStateShown
    );
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
        this._onKeyDown(aEvent);
        break;
      case "keypress":
        this._onKeyPress(aEvent);
        break;
    }
  },

  // Callback functions from DownloadsView

  /**
   * Called after data loading finished.
   */
  onViewLoadCompleted() {
    this._openPopupIfDataReady();
  },

  // User interface event functions

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
    if (
      DownloadsView.richListBox.itemCount > 0 &&
      DownloadsView.richListBox.selectedIndex == -1
    ) {
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

  // Related operations

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

  // Internal functions

  /**
   * Attach event listeners to a panel element. These listeners should be
   * removed in _unattachEventListeners. This is called automatically after the
   * panel has successfully loaded.
   */
  _attachEventListeners() {
    // Handle keydown to support accel-V.
    this.panel.addEventListener("keydown", this);
    // Handle keypress to be able to preventDefault() events before they reach
    // the richlistbox, for keyboard navigation.
    this.panel.addEventListener("keypress", this);
  },

  /**
   * Unattach event listeners that were added in _attachEventListeners. This
   * is called automatically on panel termination.
   */
  _unattachEventListeners() {
    this.panel.removeEventListener("keydown", this);
    this.panel.removeEventListener("keypress", this);
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
    if (
      (aEvent.keyCode == aEvent.DOM_VK_TAB ||
        aEvent.keyCode == aEvent.DOM_VK_UP ||
        aEvent.keyCode == aEvent.DOM_VK_DOWN) &&
      !this.keyFocusing
    ) {
      this.keyFocusing = true;
      // Ensure there's a selection, we will show the focus ring around it and
      // prevent the richlistbox from changing the selection.
      if (DownloadsView.richListBox.selectedIndex == -1) {
        DownloadsView.richListBox.selectedIndex = 0;
      }
      aEvent.preventDefault();
      return;
    }

    if (aEvent.keyCode == aEvent.DOM_VK_DOWN) {
      // If the last element in the list is selected, or the footer is already
      // focused, focus the footer.
      if (
        richListBox.selectedItem === richListBox.lastElementChild ||
        document.activeElement.parentNode.id === "downloadsFooter"
      ) {
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
    if (
      aEvent.keyCode == aEvent.DOM_VK_UP &&
      document.activeElement.parentNode.id === "downloadsFooter" &&
      DownloadsView.richListBox.firstElementChild
    ) {
      DownloadsView.richListBox.focus();
      DownloadsView.richListBox.selectedItem =
        DownloadsView.richListBox.lastElementChild;
      aEvent.preventDefault();
      return;
    }

    let pasting =
      aEvent.keyCode == aEvent.DOM_VK_V && aEvent.getModifierState("Accel");

    if (!pasting) {
      return;
    }

    DownloadsCommon.log("Received a paste event.");

    let trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
      Ci.nsITransferable
    );
    trans.init(null);
    let flavors = ["text/x-moz-url", "text/unicode"];
    flavors.forEach(trans.addDataFlavor);
    Services.clipboard.getData(trans, Services.clipboard.kGlobalClipboard);
    // Getting the data or creating the nsIURI might fail
    try {
      let data = {};
      trans.getAnyTransferData({}, data);
      let [url, name] = data.value
        .QueryInterface(Ci.nsISupportsString)
        .data.split("\n");
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

    // At this point, if the window is minimized, opening the panel could fail
    // without any notification, and there would be no way to either open or
    // close the panel any more.  To prevent this, check if the window is
    // minimized and in that case force the panel to the closed state.
    if (window.windowState == window.STATE_MINIMIZED) {
      this._state = this.kStateHidden;
      return;
    }

    // Ensure the anchor is visible.  If that is not possible, show the panel
    // anchored to the top area of the window, near the default anchor position.
    let anchor = DownloadsButton.getAnchor();

    if (!anchor) {
      DownloadsCommon.error("Downloads button cannot be found.");
      return;
    }

    let onBookmarksToolbar = !!anchor.closest("#PersonalToolbar");
    this.panel.classList.toggle("bookmarks-toolbar", onBookmarksToolbar);

    // When the panel is opened, we check if the target files of visible items
    // still exist, and update the allowed items interactions accordingly.  We
    // do these checks on a background thread, and don't prevent the panel to
    // be displayed while these checks are being performed.
    for (let viewItem of DownloadsView._visibleViewItems.values()) {
      viewItem.download.refresh().catch(Cu.reportError);
    }

    DownloadsCommon.log("Opening downloads panel popup.");

    // Delay displaying the panel because this function will sometimes be
    // called while another window is closing (like the window for selecting
    // whether to save or open the file), and that would cause the panel to
    // close immediately.
    setTimeout(
      () =>
        PanelMultiView.openPopup(
          this.panel,
          anchor,
          "bottomcenter topright",
          0,
          0,
          false,
          null
        ).catch(Cu.reportError),
      0
    );
  },
};

XPCOMUtils.defineConstant(this, "DownloadsPanel", DownloadsPanel);

// DownloadsView

/**
 * Builds and updates the downloads list widget, responding to changes in the
 * download state and real-time data.  In addition, handles part of the user
 * interaction events raised by the downloads list widget.
 */
var DownloadsView = {
  // Functions handling download items in the list

  /**
   * Maximum number of items shown by the list at any given time.
   */
  kItemCountLimit: 5,

  /**
   * Indicates whether there is a DownloadsBlockedSubview open.
   */
  subViewOpen: false,

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
    DownloadsCommon.log(
      "The downloads item count has changed - we are tracking",
      this._downloads.length,
      "downloads in total."
    );
    let count = this._downloads.length;
    let hiddenCount = count - this.kItemCountLimit;

    if (count > 0) {
      DownloadsCommon.log(
        "Setting the panel's hasdownloads attribute to true."
      );
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
    return (this.richListBox = document.getElementById("downloadsListBox"));
  },

  /**
   * Element corresponding to the button for showing more downloads.
   */
  get downloadsHistory() {
    delete this.downloadsHistory;
    return (this.downloadsHistory = document.getElementById(
      "downloadsHistory"
    ));
  },

  // Callback functions from DownloadsData

  /**
   * Called before multiple downloads are about to be loaded.
   */
  onDownloadBatchStarting() {
    DownloadsCommon.log("onDownloadBatchStarting called for DownloadsView.");
    this.loading = true;
  },

  /**
   * Called after data loading finished.
   */
  onDownloadBatchEnded() {
    DownloadsCommon.log("onDownloadBatchEnded called for DownloadsView.");

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
   */
  onDownloadAdded(download) {
    DownloadsCommon.log("A new download data item was added");

    this._downloads.unshift(download);

    // The newly added item is visible in the panel and we must add the
    // corresponding element. If the list overflows, remove the last item from
    // the panel to make room for the new one that we just added at the top.
    this._addViewItem(download, true);
    if (this._downloads.length > this.kItemCountLimit) {
      this._removeViewItem(this._downloads[this.kItemCountLimit]);
    }

    // For better performance during batch loads, don't update the count for
    // every item, because the interface won't be visible until load finishes.
    if (!this.loading) {
      this._itemCountChanged();
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
   * DownloadsViewItem object.
   */
  _itemsForElements: new Map(),

  itemForElement(element) {
    return this._itemsForElements.get(element);
  },

  /**
   * Creates a new view item associated with the specified data item, and adds
   * it to the top or the bottom of the list.
   */
  _addViewItem(download, aNewest) {
    DownloadsCommon.log(
      "Adding a new DownloadsViewItem to the downloads list.",
      "aNewest =",
      aNewest
    );

    let element = document.createXULElement("richlistitem");
    let viewItem = new DownloadsViewItem(download, element);
    this._visibleViewItems.set(download, viewItem);
    this._itemsForElements.set(element, viewItem);
    if (aNewest) {
      this.richListBox.insertBefore(
        element,
        this.richListBox.firstElementChild
      );
    } else {
      this.richListBox.appendChild(element);
    }
    viewItem.ensureActive();
  },

  /**
   * Removes the view item associated with the specified data item.
   */
  _removeViewItem(download) {
    DownloadsCommon.log(
      "Removing a DownloadsViewItem from the downloads list."
    );
    let element = this._visibleViewItems.get(download).element;
    let previousSelectedIndex = this.richListBox.selectedIndex;
    this.richListBox.removeChild(element);
    if (previousSelectedIndex != -1) {
      this.richListBox.selectedIndex = Math.min(
        previousSelectedIndex,
        this.richListBox.itemCount - 1
      );
    }
    this._visibleViewItems.delete(download);
    this._itemsForElements.delete(element);
  },

  // User interface event functions

  onDownloadClick(aEvent) {
    // Handle primary clicks only, and exclude the action button.
    if (aEvent.button == 0 && aEvent.originalTarget.localName != "button") {
      let target = aEvent.target;
      while (target.nodeName != "richlistitem") {
        target = target.parentNode;
      }
      Services.telemetry.scalarAdd("downloads.file_opened", 1);
      let download = DownloadsView.itemForElement(target).download;
      let command = "downloadsCmd_open";
      if (download.hasBlockedData) {
        command = "downloadsCmd_showBlockedInfo";
      } else if (aEvent.shiftKey || aEvent.ctrlKey || aEvent.metaKey) {
        // We adjust the command for supported modifiers to suggest where the download
        // may be opened
        let openWhere = target.ownerGlobal.whereToOpenLink(aEvent, false, true);
        if (["tab", "window", "tabshifted"].includes(openWhere)) {
          command += ":" + openWhere;
        }
      }
      DownloadsCommon.log("onDownloadClick, resolved command: ", command);
      goDoCommand(command);
    }
  },

  onDownloadButton(event) {
    let target = event.target.closest("richlistitem");
    DownloadsView.itemForElement(target).onButton();
  },

  /**
   * Handles keypress events on a download item.
   */
  onDownloadKeyPress(aEvent) {
    // Pressing the key on buttons should not invoke the action because the
    // event has already been handled by the button itself.
    if (
      aEvent.originalTarget.hasAttribute("command") ||
      aEvent.originalTarget.hasAttribute("oncommand")
    ) {
      return;
    }

    if (aEvent.charCode == " ".charCodeAt(0)) {
      aEvent.preventDefault();
      goDoCommand("downloadsCmd_pauseResume");
      return;
    }

    if (aEvent.keyCode == KeyEvent.DOM_VK_RETURN) {
      goDoCommand("downloadsCmd_doDefault");
    }
  },

  get contextMenu() {
    let menu = document.getElementById("downloadsContextMenu");
    if (menu) {
      delete this.contextMenu;
      this.contextMenu = menu;
    }
    return menu;
  },

  /**
   * Indicates whether there is an open contextMenu for a download item.
   */
  get contextMenuOpen() {
    return this.contextMenu.state != "closed";
  },

  /**
   * Mouse listeners to handle selection on hover.
   */
  onDownloadMouseOver(aEvent) {
    let item = aEvent.target.closest("richlistitem,richlistbox");
    if (item.localName != "richlistitem") {
      return;
    }

    if (aEvent.target.classList.contains("downloadButton")) {
      item.classList.add("downloadHoveringButton");
    }

    if (!this.contextMenuOpen && !this.subViewOpen) {
      this.richListBox.selectedItem = item;
    }
  },

  onDownloadMouseOut(aEvent) {
    let item = aEvent.target.closest("richlistitem,richlistbox");
    if (item.localName != "richlistitem") {
      return;
    }

    if (aEvent.target.classList.contains("downloadButton")) {
      item.classList.remove("downloadHoveringButton");
    }

    // If the destination element is outside of the richlistitem, clear the
    // selection.
    if (
      !this.contextMenuOpen &&
      !this.subViewOpen &&
      !item.contains(aEvent.relatedTarget)
    ) {
      this.richListBox.selectedIndex = -1;
    }
  },

  onDownloadContextMenu(aEvent) {
    let element = this.richListBox.selectedItem;
    if (!element) {
      return;
    }

    DownloadsViewController.updateCommands();

    DownloadsViewUI.updateContextMenuForElement(this.contextMenu, element);
  },

  onDownloadDragStart(aEvent) {
    let element = this.richListBox.selectedItem;
    if (!element) {
      return;
    }

    // We must check for existence synchronously because this is a DOM event.
    let file = new FileUtils.File(
      DownloadsView.itemForElement(element).download.target.path
    );
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
};

XPCOMUtils.defineConstant(this, "DownloadsView", DownloadsView);

// DownloadsViewItem

/**
 * Builds and updates a single item in the downloads list widget, responding to
 * changes in the download state and real-time data, and handles the user
 * interaction events related to a single item in the downloads list widgets.
 *
 * @param download
 *        Download object to be associated with the view item.
 * @param aElement
 *        XUL element corresponding to the single download item in the view.
 */

class DownloadsViewItem extends DownloadsViewUI.DownloadElementShell {
  constructor(download, aElement) {
    super();

    this.download = download;
    this.element = aElement;
    this.element._shell = this;

    this.element.setAttribute("type", "download");
    this.element.classList.add("download-state");

    this.isPanel = true;
  }

  onChanged() {
    let newState = DownloadsCommon.stateOfDownload(this.download);
    if (this.downloadState !== newState) {
      this.downloadState = newState;
      this._updateState();
    } else {
      this._updateStateInner();
    }
  }

  isCommandEnabled(aCommand) {
    switch (aCommand) {
      case "downloadsCmd_open":
      case "downloadsCmd_open:current":
      case "downloadsCmd_open:tab":
      case "downloadsCmd_open:tabshifted":
      case "downloadsCmd_open:window": {
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
      case "cmd_delete":
      case "downloadsCmd_copyLocation":
      case "downloadsCmd_doDefault":
        return true;
      case "downloadsCmd_showBlockedInfo":
        return this.download.hasBlockedData;
    }
    return DownloadsViewUI.DownloadElementShell.prototype.isCommandEnabled.call(
      this,
      aCommand
    );
  }

  doCommand(aCommand) {
    if (this.isCommandEnabled(aCommand)) {
      let [command, modifier] = aCommand.split(":");
      // split off an optional command "modifier" into an argument,
      // e.g. "downloadsCmd_open:window"
      this[command](modifier);
    }
  }

  // Item commands

  downloadsCmd_unblock() {
    DownloadsPanel.hidePanel();
    this.confirmUnblock(window, "unblock");
  }

  downloadsCmd_chooseUnblock() {
    DownloadsPanel.hidePanel();
    this.confirmUnblock(window, "chooseUnblock");
  }

  downloadsCmd_unblockAndOpen() {
    DownloadsPanel.hidePanel();
    this.unblockAndOpenDownload().catch(Cu.reportError);
  }
  downloadsCmd_unblockAndSave() {
    DownloadsPanel.hidePanel();
    this.unblockAndSave();
  }

  downloadsCmd_open(openWhere) {
    super.downloadsCmd_open(openWhere);

    // We explicitly close the panel here to give the user the feedback that
    // their click has been received, and we're handling the action.
    // Otherwise, we'd have to wait for the file-type handler to execute
    // before the panel would close. This also helps to prevent the user from
    // accidentally opening a file several times.
    DownloadsPanel.hidePanel();
  }

  downloadsCmd_openInSystemViewer() {
    super.downloadsCmd_openInSystemViewer();

    // We explicitly close the panel here to give the user the feedback that
    // their click has been received, and we're handling the action.
    DownloadsPanel.hidePanel();
  }

  downloadsCmd_alwaysOpenInSystemViewer() {
    super.downloadsCmd_alwaysOpenInSystemViewer();

    // We explicitly close the panel here to give the user the feedback that
    // their click has been received, and we're handling the action.
    DownloadsPanel.hidePanel();
  }

  downloadsCmd_show() {
    let file = new FileUtils.File(this.download.target.path);
    DownloadsCommon.showDownloadedFile(file);

    // We explicitly close the panel here to give the user the feedback that
    // their click has been received, and we're handling the action.
    // Otherwise, we'd have to wait for the operating system file manager
    // window to open before the panel closed. This also helps to prevent the
    // user from opening the containing folder several times.
    DownloadsPanel.hidePanel();
  }

  downloadsCmd_showBlockedInfo() {
    DownloadsBlockedSubview.toggle(
      this.element,
      ...this.rawBlockedTitleAndDetails
    );
  }

  downloadsCmd_openReferrer() {
    openURL(this.download.source.referrerInfo.originalReferrer);
  }

  downloadsCmd_copyLocation() {
    DownloadsCommon.copyDownloadLink(this.download);
  }

  downloadsCmd_doDefault() {
    let defaultCommand = this.currentDefaultCommandName;
    if (defaultCommand && this.isCommandEnabled(defaultCommand)) {
      this.doCommand(defaultCommand);
    }
  }
}

// DownloadsViewController

/**
 * Handles part of the user interaction events raised by the downloads list
 * widget, in particular the "commands" that apply to multiple items, and
 * dispatches the commands that apply to individual items.
 */
var DownloadsViewController = {
  // Initialization and termination

  initialize() {
    window.controllers.insertControllerAt(0, this);
  },

  terminate() {
    window.controllers.removeController(this);
  },

  // nsIController

  supportsCommand(aCommand) {
    if (aCommand === "downloadsCmd_clearList") {
      return true;
    }
    // Firstly, determine if this is a command that we can handle.
    if (!DownloadsViewUI.isCommandName(aCommand)) {
      return false;
    }
    // Strip off any :modifier suffix before checking if the command name is
    // a method on our view
    let [command] = aCommand.split(":");
    if (!(command in this) && !(command in DownloadsViewItem.prototype)) {
      return false;
    }
    // The currently supported commands depend on whether the blocked subview is
    // showing.  If it is, then take the following path.
    if (DownloadsView.subViewOpen) {
      let blockedSubviewCmds = [
        "downloadsCmd_unblockAndOpen",
        "cmd_delete",
        "downloadsCmd_unblockAndSave",
      ];
      return blockedSubviewCmds.includes(aCommand);
    }
    // If the blocked subview is not showing, then determine if focus is on a
    // control in the downloads list.
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
    return (
      element &&
      DownloadsView.itemForElement(element).isCommandEnabled(aCommand)
    );
  },

  doCommand(aCommand) {
    // If this command is not selection-specific, execute it.
    if (aCommand in this) {
      this[aCommand]();
      return;
    }

    // Other commands are selection-specific.
    let element = DownloadsView.richListBox.selectedItem;
    if (element) {
      // The doCommand function also checks if the command is enabled.
      DownloadsView.itemForElement(element).doCommand(aCommand);
    }
  },

  onEvent() {},

  // Other functions

  updateCommands() {
    function updateCommandsForObject(object) {
      for (let name in object) {
        if (DownloadsViewUI.isCommandName(name)) {
          goUpdateCommand(name);
        }
      }
    }
    updateCommandsForObject(this);
    updateCommandsForObject(DownloadsViewItem.prototype);
  },

  // Selection-independent commands

  downloadsCmd_clearList() {
    DownloadsCommon.getData(window).removeFinished();
  },
};

XPCOMUtils.defineConstant(
  this,
  "DownloadsViewController",
  DownloadsViewController
);

// DownloadsSummary

/**
 * Manages the summary at the bottom of the downloads panel list if the number
 * of items in the list exceeds the panels limit.
 */
var DownloadsSummary = {
  /**
   * Sets the active state of the summary. When active, the summary subscribes
   * to the DownloadsCommon DownloadsSummaryData singleton.
   *
   * @param aActive
   *        Set to true to activate the summary.
   */
  set active(aActive) {
    if (aActive == this._active || !this._summaryNode) {
      return;
    }
    if (aActive) {
      DownloadsCommon.getSummary(
        window,
        DownloadsView.kItemCountLimit
      ).refreshView(this);
    } else {
      DownloadsFooter.showingSummary = false;
    }

    this._active = aActive;
  },

  /**
   * Returns the active state of the downloads summary.
   */
  get active() {
    return this._active;
  },

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
    DownloadsFooter.showingSummary = aShowingProgress;
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
    if (
      aEvent.charCode == " ".charCodeAt(0) ||
      aEvent.keyCode == KeyEvent.DOM_VK_RETURN
    ) {
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
    return (this._summaryNode = node);
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
    return (this._progressNode = node);
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
    return (this._descriptionNode = node);
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
    return (this._detailsNode = node);
  },
};

XPCOMUtils.defineConstant(this, "DownloadsSummary", DownloadsSummary);

// DownloadsFooter

/**
 * Manages events sent to to the footer vbox, which contains both the
 * DownloadsSummary as well as the "Show all downloads" button.
 */
var DownloadsFooter = {
  /**
   * Focuses the appropriate element within the footer. If the summary
   * is visible, focus it. If not, focus the "Show all downloads"
   * button.
   */
  focus() {
    if (this._showingSummary) {
      DownloadsSummary.focus();
    } else {
      DownloadsView.downloadsHistory.focus({ preventFocusRing: true });
    }
  },

  _showingSummary: false,

  /**
   * Sets whether or not the Downloads Summary should be displayed in the
   * footer. If not, the "Show all downloads" button is shown instead.
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
    return (this._footerNode = node);
  },
};

XPCOMUtils.defineConstant(this, "DownloadsFooter", DownloadsFooter);

// DownloadsBlockedSubview

/**
 * Manages the blocked subview that slides in when you click a blocked download.
 */
var DownloadsBlockedSubview = {
  /**
   * Elements in the subview.
   */
  get elements() {
    let idSuffixes = [
      "title",
      "details1",
      "details2",
      "unblockButton",
      "deleteButton",
    ];
    let elements = idSuffixes.reduce((memo, s) => {
      memo[s] = document.getElementById("downloadsPanel-blockedSubview-" + s);
      return memo;
    }, {});
    delete this.elements;
    return (this.elements = elements);
  },

  /**
   * The blocked-download richlistitem element that was clicked to show the
   * subview.  If the subview is not showing, this is undefined.
   */
  element: undefined,

  /**
   * Slides in the blocked subview.
   *
   * @param element
   *        The blocked-download richlistitem element that was clicked.
   * @param title
   *        The title to show in the subview.
   * @param details
   *        An array of strings with information about the block.
   */
  toggle(element, title, details) {
    DownloadsView.subViewOpen = true;
    DownloadsViewController.updateCommands();
    const { download } = DownloadsView.itemForElement(element);

    let e = this.elements;
    let s = DownloadsCommon.strings;
    e.title.textContent = title;
    e.details1.textContent = details[0];
    e.details2.textContent = details[1];

    if (download.launchWhenSucceeded) {
      e.unblockButton.label = s.unblockButtonOpen;
      e.unblockButton.command = "downloadsCmd_unblockAndOpen";
    } else {
      e.unblockButton.label = s.unblockButtonUnblock;
      e.unblockButton.command = "downloadsCmd_unblockAndSave";
    }

    e.deleteButton.label = s.unblockButtonConfirmBlock;

    let verdict = element.getAttribute("verdict");
    this.subview.setAttribute("verdict", verdict);

    this.mainView.addEventListener("ViewShown", this);
    DownloadsPanel.panel.addEventListener("popuphidden", this);
    this.panelMultiView.showSubView(this.subview);

    // Without this, the mainView is more narrow than the panel once all
    // downloads are removed from the panel.
    this.mainView.style.minWidth = window.getComputedStyle(this.subview).width;
  },

  handleEvent(event) {
    // This is called when the main view is shown or the panel is hidden.
    DownloadsView.subViewOpen = false;
    this.mainView.removeEventListener("ViewShown", this);
    DownloadsPanel.panel.removeEventListener("popuphidden", this);
    // Focus the proper element if we're going back to the main panel.
    if (event.type == "ViewShown") {
      DownloadsPanel.showPanel();
    }
  },

  /**
   * Deletes the download and hides the entire panel.
   */
  confirmBlock() {
    goDoCommand("cmd_delete");
    DownloadsPanel.hidePanel();
  },
};

XPCOMUtils.defineLazyGetter(DownloadsBlockedSubview, "panelMultiView", () =>
  document.getElementById("downloadsPanel-multiView")
);
XPCOMUtils.defineLazyGetter(DownloadsBlockedSubview, "mainView", () =>
  document.getElementById("downloadsPanel-mainView")
);
XPCOMUtils.defineLazyGetter(DownloadsBlockedSubview, "subview", () =>
  document.getElementById("downloadsPanel-blockedSubview")
);

XPCOMUtils.defineConstant(
  this,
  "DownloadsBlockedSubview",
  DownloadsBlockedSubview
);
