/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env mozilla/browser-window */

var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  Downloads: "resource://gre/modules/Downloads.jsm",
  DownloadsCommon: "resource:///modules/DownloadsCommon.jsm",
  DownloadsViewUI: "resource:///modules/DownloadsViewUI.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
});

/**
 * A download element shell is responsible for handling the commands and the
 * displayed data for a single download view element.
 *
 * The shell may contain a session download, a history download, or both.  When
 * both a history and a session download are present, the session download gets
 * priority and its information is displayed.
 *
 * On construction, a new richlistitem is created, and can be accessed through
 * the |element| getter. The shell doesn't insert the item in a richlistbox, the
 * caller must do it and remove the element when it's no longer needed.
 *
 * The caller is also responsible for forwarding status notifications, calling
 * the onChanged method.
 *
 * @param download
 *        The Download object from the DownloadHistoryList.
 */
function HistoryDownloadElementShell(download) {
  this._download = download;

  this.element = document.createXULElement("richlistitem");
  this.element._shell = this;

  this.element.classList.add("download");
  this.element.classList.add("download-state");
}

HistoryDownloadElementShell.prototype = {
  __proto__: DownloadsViewUI.DownloadElementShell.prototype,

  /**
   * Overrides the base getter to return the Download or HistoryDownload object
   * for displaying information and executing commands in the user interface.
   */
  get download() {
    return this._download;
  },

  onStateChanged() {
    // Since the state changed, we may need to check the target file again.
    this._targetFileChecked = false;

    this._updateState();

    if (this.element.selected) {
      goUpdateDownloadCommands();
    } else {
      // If a state change occurs in an item that is not currently selected,
      // this is the only command that may be affected.
      goUpdateCommand("downloadsCmd_clearDownloads");
    }
  },

  onChanged() {
    // There is nothing to do if the item has always been invisible.
    if (!this.active) {
      return;
    }

    let newState = DownloadsCommon.stateOfDownload(this.download);
    if (this._downloadState !== newState) {
      this._downloadState = newState;
      this.onStateChanged();
    } else {
      this._updateStateInner();
    }
  },
  _downloadState: null,

  isCommandEnabled(aCommand) {
    // The only valid command for inactive elements is cmd_delete.
    if (!this.active && aCommand != "cmd_delete") {
      return false;
    }
    return DownloadsViewUI.DownloadElementShell.prototype.isCommandEnabled.call(
      this,
      aCommand
    );
  },

  downloadsCmd_unblock() {
    this.confirmUnblock(window, "unblock");
  },
  downloadsCmd_unblockAndSave() {
    this.confirmUnblock(window, "unblock");
  },

  downloadsCmd_chooseUnblock() {
    this.confirmUnblock(window, "chooseUnblock");
  },

  downloadsCmd_chooseOpen() {
    this.confirmUnblock(window, "chooseOpen");
  },

  // Returns whether or not the download handled by this shell should
  // show up in the search results for the given term.  Both the display
  // name for the download and the url are searched.
  matchesSearchTerm(aTerm) {
    if (!aTerm) {
      return true;
    }
    aTerm = aTerm.toLowerCase();
    let displayName = DownloadsViewUI.getDisplayName(this.download);
    return (
      displayName.toLowerCase().includes(aTerm) ||
      (this.download.source.originalUrl || this.download.source.url)
        .toLowerCase()
        .includes(aTerm)
    );
  },

  // Handles double-click and return keypress on the element (the keypress
  // listener is set in the DownloadsPlacesView object).
  doDefaultCommand(event) {
    let command = this.currentDefaultCommandName;
    if (
      command == "downloadsCmd_open" &&
      event &&
      (event.shiftKey || event.ctrlKey || event.metaKey || event.button == 1)
    ) {
      // We adjust the command for supported modifiers to suggest where the download may
      // be opened.
      let browserWin = BrowserWindowTracker.getTopWindow();
      let openWhere = browserWin
        ? browserWin.whereToOpenLink(event, false, true)
        : "window";
      if (["window", "tabshifted", "tab"].includes(openWhere)) {
        command += ":" + openWhere;
      }
    }

    if (command && this.isCommandEnabled(command)) {
      this.doCommand(command);
    }
  },

  /**
   * This method is called by the outer download view, after the controller
   * commands have already been updated. In case we did not check for the
   * existence of the target file already, we can do it now and then update
   * the commands as needed.
   */
  onSelect() {
    if (!this.active) {
      return;
    }

    // If this is a history download for which no target file information is
    // available, we cannot retrieve information about the target file.
    if (!this.download.target.path) {
      return;
    }

    // Start checking for existence.  This may be done twice if onSelect is
    // called again before the information is collected.
    if (!this._targetFileChecked) {
      this.download
        .refresh()
        .catch(Cu.reportError)
        .then(() => {
          // Do not try to check for existence again even if this failed.
          this._targetFileChecked = true;
        });
    }
  },
};

/**
 * Relays commands from the download.xml binding to the selected items.
 */
var DownloadsView = {
  onDownloadButton(event) {
    event.target.closest("richlistitem")._shell.onButton();
  },

  onDownloadClick() {},
};

/**
 * A Downloads Places View is a places view designed to show a places query
 * for history downloads alongside the session downloads.
 *
 * As we don't use the places controller, some methods implemented by other
 * places views are not implemented by this view.
 *
 * A richlistitem in this view can represent either a past download or a session
 * download, or both. Session downloads are shown first in the view, and as long
 * as they exist they "collapses" their history "counterpart" (So we don't show two
 * items for every download).
 */
function DownloadsPlacesView(
  aRichListBox,
  aActive = true,
  aSuppressionFlag = DownloadsCommon.SUPPRESS_ALL_DOWNLOADS_OPEN
) {
  this._richlistbox = aRichListBox;
  this._richlistbox._placesView = this;
  window.controllers.insertControllerAt(0, this);

  // Map downloads to their element shells.
  this._viewItemsForDownloads = new WeakMap();

  this._searchTerm = "";

  this._active = aActive;

  // Register as a downloads view. The places data will be initialized by
  // the places setter.
  this._initiallySelectedElement = null;
  this._downloadsData = DownloadsCommon.getData(window.opener || window, true);
  this._waitingForInitialData = true;
  this._downloadsData.addView(this);

  // Pause the download indicator as user is interacting with downloads. This is
  // skipped on about:downloads because it handles this by itself.
  if (aSuppressionFlag === DownloadsCommon.SUPPRESS_ALL_DOWNLOADS_OPEN) {
    DownloadsCommon.getIndicatorData(
      window
    ).attentionSuppressed |= aSuppressionFlag;
  }

  // Make sure to unregister the view if the window is closed.
  window.addEventListener(
    "unload",
    () => {
      window.controllers.removeController(this);
      // Unpause the main window's download indicator.
      DownloadsCommon.getIndicatorData(
        window
      ).attentionSuppressed &= ~aSuppressionFlag;
      this._downloadsData.removeView(this);
      this.result = null;
    },
    true
  );
  // Resizing the window may change items visibility.
  window.addEventListener(
    "resize",
    () => {
      this._ensureVisibleElementsAreActive(true);
    },
    true
  );
}

DownloadsPlacesView.prototype = {
  __proto__: DownloadsViewUI.BaseView.prototype,

  get associatedElement() {
    return this._richlistbox;
  },

  get active() {
    return this._active;
  },
  set active(val) {
    this._active = val;
    if (this._active) {
      this._ensureVisibleElementsAreActive(true);
    }
  },

  /**
   * Ensure the custom element contents are created and shown for each
   * visible element in the list.
   *
   * @param debounce whether to use a short timeout rather than running
   *                 immediately. The default is running immediately. If you
   *                 pass `true`, we'll run on a 10ms timeout. This is used to
   *                 avoid running this code lots while scrolling or resizing.
   */
  _ensureVisibleElementsAreActive(debounce = false) {
    if (
      !this.active ||
      (debounce && this._ensureVisibleTimer) ||
      !this._richlistbox.firstChild
    ) {
      return;
    }

    if (debounce) {
      this._ensureVisibleTimer = setTimeout(() => {
        this._internalEnsureVisibleElementsAreActive();
      }, 10);
    } else {
      this._internalEnsureVisibleElementsAreActive();
    }
  },

  _internalEnsureVisibleElementsAreActive() {
    // If there are no children, we can't do anything so bail out.
    // However, avoid clearing the timer because there may be children
    // when the timer fires.
    if (!this._richlistbox.firstChild) {
      // If we were called asynchronously (debounced), we need to delete
      // the timer variable to ensure we are called again if another
      // debounced call comes in.
      delete this._ensureVisibleTimer;
      return;
    }

    if (this._ensureVisibleTimer) {
      clearTimeout(this._ensureVisibleTimer);
      delete this._ensureVisibleTimer;
    }

    let rlbRect = this._richlistbox.getBoundingClientRect();
    let winUtils = window.windowUtils;
    let nodes = winUtils.nodesFromRect(
      rlbRect.left,
      rlbRect.top,
      0,
      rlbRect.width,
      rlbRect.height,
      0,
      true,
      false,
      false
    );
    // nodesFromRect returns nodes in z-index order, and for the same z-index
    // sorts them in inverted DOM order, thus starting from the one that would
    // be on top.
    let firstVisibleNode, lastVisibleNode;
    for (let node of nodes) {
      if (node.localName === "richlistitem" && node._shell) {
        node._shell.ensureActive();
        // The first visible node is the last match.
        firstVisibleNode = node;
        // While the last visible node is the first match.
        if (!lastVisibleNode) {
          lastVisibleNode = node;
        }
      }
    }

    // Also activate the first invisible nodes in both boundaries (that is,
    // above and below the visible area) to ensure proper keyboard navigation
    // in both directions.
    let nodeBelowVisibleArea = lastVisibleNode && lastVisibleNode.nextSibling;
    if (nodeBelowVisibleArea && nodeBelowVisibleArea._shell) {
      nodeBelowVisibleArea._shell.ensureActive();
    }

    let nodeAboveVisibleArea =
      firstVisibleNode && firstVisibleNode.previousSibling;
    if (nodeAboveVisibleArea && nodeAboveVisibleArea._shell) {
      nodeAboveVisibleArea._shell.ensureActive();
    }
  },

  _place: "",
  get place() {
    return this._place;
  },
  set place(val) {
    if (this._place == val) {
      // XXXmano: places.js relies on this behavior (see Bug 822203).
      this.searchTerm = "";
    } else {
      this._place = val;
    }
  },

  get selectedNodes() {
    return Array.prototype.filter.call(
      this._richlistbox.selectedItems,
      element => element._shell.download.placesNode
    );
  },

  get selectedNode() {
    let selectedNodes = this.selectedNodes;
    return selectedNodes.length == 1 ? selectedNodes[0] : null;
  },

  get hasSelection() {
    return !!this.selectedNodes.length;
  },

  get controller() {
    return this._richlistbox.controller;
  },

  get searchTerm() {
    return this._searchTerm;
  },
  set searchTerm(aValue) {
    if (this._searchTerm != aValue) {
      // Always clear selection on a new search, since the user is starting a
      // different workflow. This also solves the fact we could end up
      // retaining selection on hidden elements.
      this._richlistbox.clearSelection();
      for (let element of this._richlistbox.childNodes) {
        element.hidden = !element._shell.matchesSearchTerm(aValue);
      }
      this._ensureVisibleElementsAreActive();
    }
    this._searchTerm = aValue;
  },

  /**
   * When the view loads, we want to select the first item.
   * However, because session downloads, for which the data is loaded
   * asynchronously, always come first in the list, and because the list
   * may (or may not) already contain history downloads at that point, it
   * turns out that by the time we can select the first item, the user may
   * have already started using the view.
   * To make things even more complicated, in other cases, the places data
   * may be loaded after the session downloads data.  Thus we cannot rely on
   * the order in which the data comes in.
   * We work around this by attempting to select the first element twice,
   * once after the places data is loaded and once when the session downloads
   * data is done loading.  However, if the selection has changed in-between,
   * we assume the user has already started using the view and give up.
   */
  _ensureInitialSelection() {
    // Either they're both null, or the selection has not changed in between.
    if (this._richlistbox.selectedItem == this._initiallySelectedElement) {
      let firstDownloadElement = this._richlistbox.firstChild;
      if (firstDownloadElement != this._initiallySelectedElement) {
        // We may be called before _ensureVisibleElementsAreActive,
        // therefore, ensure the first item is activated.
        firstDownloadElement._shell.ensureActive();
        this._richlistbox.selectedItem = firstDownloadElement;
        this._richlistbox.currentItem = firstDownloadElement;
        this._initiallySelectedElement = firstDownloadElement;
      }
    }
  },

  /**
   * DocumentFragment object that contains all the new elements added during a
   * batch operation, or null if no batch is in progress.
   *
   * Since newest downloads are displayed at the top, elements are normally
   * prepended to the fragment, and then the fragment is prepended to the list.
   */
  batchFragment: null,

  onDownloadBatchStarting() {
    this.batchFragment = document.createDocumentFragment();

    this.oldSuppressOnSelect = this._richlistbox.suppressOnSelect;
    this._richlistbox.suppressOnSelect = true;
  },

  onDownloadBatchEnded() {
    this._richlistbox.suppressOnSelect = this.oldSuppressOnSelect;
    delete this.oldSuppressOnSelect;

    if (this.batchFragment.childElementCount) {
      this._prependBatchFragment();
    }
    this.batchFragment = null;

    this._ensureInitialSelection();
    this._ensureVisibleElementsAreActive();
    goUpdateDownloadCommands();
    if (this._waitingForInitialData) {
      this._waitingForInitialData = false;
      this._richlistbox.dispatchEvent(
        new CustomEvent("InitialDownloadsLoaded")
      );
    }
  },

  _prependBatchFragment() {
    // Workaround multiple reflows hang by removing the richlistbox
    // and adding it back when we're done.

    // Hack for bug 836283: reset xbl fields to their old values after the
    // binding is reattached to avoid breaking the selection state
    let xblFields = new Map();
    for (let key of Object.getOwnPropertyNames(this._richlistbox)) {
      let value = this._richlistbox[key];
      xblFields.set(key, value);
    }

    let oldActiveElement = document.activeElement;
    let parentNode = this._richlistbox.parentNode;
    let nextSibling = this._richlistbox.nextSibling;
    parentNode.removeChild(this._richlistbox);
    this._richlistbox.prepend(this.batchFragment);
    parentNode.insertBefore(this._richlistbox, nextSibling);
    if (oldActiveElement && oldActiveElement != document.activeElement) {
      oldActiveElement.focus();
    }

    for (let [key, value] of xblFields) {
      this._richlistbox[key] = value;
    }
  },

  onDownloadAdded(download, { insertBefore } = {}) {
    let shell = new HistoryDownloadElementShell(download);
    this._viewItemsForDownloads.set(download, shell);

    // Since newest downloads are displayed at the top, either prepend the new
    // element or insert it after the one indicated by the insertBefore option.
    if (insertBefore) {
      this._viewItemsForDownloads
        .get(insertBefore)
        .element.insertAdjacentElement("afterend", shell.element);
    } else {
      (this.batchFragment || this._richlistbox).prepend(shell.element);
    }

    if (this.searchTerm) {
      shell.element.hidden = !shell.matchesSearchTerm(this.searchTerm);
    }

    // Don't update commands and visible elements during a batch change.
    if (!this.batchFragment) {
      this._ensureVisibleElementsAreActive();
      goUpdateCommand("downloadsCmd_clearDownloads");
    }
  },

  onDownloadChanged(download) {
    this._viewItemsForDownloads.get(download).onChanged();
  },

  onDownloadRemoved(download) {
    let element = this._viewItemsForDownloads.get(download).element;

    // If the element was selected exclusively, select its next
    // sibling first, if not, try for previous sibling, if any.
    if (
      (element.nextSibling || element.previousSibling) &&
      this._richlistbox.selectedItems &&
      this._richlistbox.selectedItems.length == 1 &&
      this._richlistbox.selectedItems[0] == element
    ) {
      this._richlistbox.selectItem(
        element.nextSibling || element.previousSibling
      );
    }

    this._richlistbox.removeItemFromSelection(element);
    element.remove();

    // Don't update commands and visible elements during a batch change.
    if (!this.batchFragment) {
      this._ensureVisibleElementsAreActive();
      goUpdateCommand("downloadsCmd_clearDownloads");
    }
  },

  // nsIController
  supportsCommand(aCommand) {
    // Firstly, determine if this is a command that we can handle.
    if (!DownloadsViewUI.isCommandName(aCommand)) {
      return false;
    }
    if (
      !(aCommand in this) &&
      !(aCommand in HistoryDownloadElementShell.prototype)
    ) {
      return false;
    }
    // If this function returns true, other controllers won't get a chance to
    // process the command even if isCommandEnabled returns false, so it's
    // important to check if the list is focused here to handle common commands
    // like copy and paste correctly. The clear downloads command, instead, is
    // specific to the downloads list but can be invoked from the toolbar, so we
    // can just return true unconditionally.
    return (
      aCommand == "downloadsCmd_clearDownloads" ||
      document.activeElement == this._richlistbox
    );
  },

  // nsIController
  isCommandEnabled(aCommand) {
    switch (aCommand) {
      case "cmd_copy":
        return Array.prototype.some.call(
          this._richlistbox.selectedItems,
          element => {
            const { source } = element._shell.download;
            return !!(source?.originalUrl || source?.url);
          }
        );
      case "downloadsCmd_openReferrer":
      case "downloadShowMenuItem":
        return this._richlistbox.selectedItems.length == 1;
      case "cmd_selectAll":
        return true;
      case "cmd_paste":
        return this._canDownloadClipboardURL();
      case "downloadsCmd_clearDownloads":
        return this.canClearDownloads(this._richlistbox);
      default:
        return Array.prototype.every.call(
          this._richlistbox.selectedItems,
          element => element._shell.isCommandEnabled(aCommand)
        );
    }
  },

  _copySelectedDownloadsToClipboard() {
    let urls = Array.from(this._richlistbox.selectedItems, element => {
      const { source } = element._shell.download;
      return source?.originalUrl || source?.url;
    }).filter(Boolean);

    Cc["@mozilla.org/widget/clipboardhelper;1"]
      .getService(Ci.nsIClipboardHelper)
      .copyString(urls.join("\n"));
  },

  _getURLFromClipboardData() {
    let trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
      Ci.nsITransferable
    );
    trans.init(null);

    let flavors = ["text/x-moz-url", "text/unicode"];
    flavors.forEach(trans.addDataFlavor);

    Services.clipboard.getData(trans, Services.clipboard.kGlobalClipboard);

    // Getting the data or creating the nsIURI might fail.
    try {
      let data = {};
      trans.getAnyTransferData({}, data);
      let [url, name] = data.value
        .QueryInterface(Ci.nsISupportsString)
        .data.split("\n");
      if (url) {
        return [NetUtil.newURI(url).spec, name];
      }
    } catch (ex) {}

    return ["", ""];
  },

  _canDownloadClipboardURL() {
    let [url /* ,name */] = this._getURLFromClipboardData();
    return url != "";
  },

  _downloadURLFromClipboard() {
    let [url, name] = this._getURLFromClipboardData();
    let browserWin = BrowserWindowTracker.getTopWindow();
    let initiatingDoc = browserWin ? browserWin.document : document;
    DownloadURL(url, name, initiatingDoc);
  },

  // nsIController
  doCommand(aCommand) {
    // Commands may be invoked with keyboard shortcuts even if disabled.
    if (!this.isCommandEnabled(aCommand)) {
      return;
    }

    // If this command is not selection-specific, execute it.
    if (aCommand in this) {
      this[aCommand]();
      return;
    }

    // Cloning the nodelist into an array to get a frozen list of selected items.
    // Otherwise, the selectedItems nodelist is live and doCommand may alter the
    // selection while we are trying to do one particular action, like removing
    // items from history.
    let selectedElements = [...this._richlistbox.selectedItems];
    for (let element of selectedElements) {
      element._shell.doCommand(aCommand);
    }
  },

  // nsIController
  onEvent() {},

  cmd_copy() {
    this._copySelectedDownloadsToClipboard();
  },

  cmd_selectAll() {
    if (!this.searchTerm) {
      this._richlistbox.selectAll();
      return;
    }
    // If there is a filtering search term, some rows are hidden and should not
    // be selected.
    let oldSuppressOnSelect = this._richlistbox.suppressOnSelect;
    this._richlistbox.suppressOnSelect = true;
    this._richlistbox.clearSelection();
    var item = this._richlistbox.getItemAtIndex(0);
    while (item) {
      if (!item.hidden) {
        this._richlistbox.addItemToSelection(item);
      }
      item = this._richlistbox.getNextItem(item, 1);
    }
    this._richlistbox.suppressOnSelect = oldSuppressOnSelect;
  },

  cmd_paste() {
    this._downloadURLFromClipboard();
  },

  downloadsCmd_clearDownloads() {
    this._downloadsData.removeFinished();
    if (this._place) {
      PlacesUtils.history
        .removeVisitsByFilter({
          transition: PlacesUtils.history.TRANSITIONS.DOWNLOAD,
        })
        .catch(Cu.reportError);
    }
    // There may be no selection or focus change as a result
    // of these change, and we want the command updated immediately.
    goUpdateCommand("downloadsCmd_clearDownloads");
  },

  onContextMenu(aEvent) {
    let element = this._richlistbox.selectedItem;
    if (!element || !element._shell) {
      return false;
    }

    let contextMenu = document.getElementById("downloadsContextMenu");
    DownloadsViewUI.updateContextMenuForElement(contextMenu, element);
    // Hide the copy location item if there is somehow no URL. We have to do
    // this here instead of in DownloadsViewUI because DownloadsView doesn't
    // allow selecting multiple downloads, so in that view the menuitem will be
    // shown according to whether just the selected item has a source URL.
    contextMenu.querySelector(
      ".downloadCopyLocationMenuItem"
    ).hidden = !Array.prototype.some.call(
      this._richlistbox.selectedItems,
      el => !!el._shell.download.source?.url
    );

    let download = element._shell.download;
    if (!download.stopped) {
      // The hasPartialData property of a download may change at any time after
      // it has started, so ensure we update the related command now.
      goUpdateCommand("downloadsCmd_pauseResume");
    }

    return true;
  },

  onKeyPress(aEvent) {
    let selectedElements = this._richlistbox.selectedItems;
    if (aEvent.keyCode == KeyEvent.DOM_VK_RETURN) {
      // In the content tree, opening bookmarks by pressing return is only
      // supported when a single item is selected. To be consistent, do the
      // same here.
      if (selectedElements.length == 1) {
        let element = selectedElements[0];
        if (element._shell) {
          element._shell.doDefaultCommand(aEvent);
        }
      }
    } else if (aEvent.charCode == " ".charCodeAt(0)) {
      let atLeastOneDownloadToggled = false;
      // Pause/Resume every selected download
      for (let element of selectedElements) {
        if (element._shell.isCommandEnabled("downloadsCmd_pauseResume")) {
          element._shell.doCommand("downloadsCmd_pauseResume");
          atLeastOneDownloadToggled = true;
        }
      }

      if (atLeastOneDownloadToggled) {
        aEvent.preventDefault();
      }
    }
  },

  onDoubleClick(aEvent) {
    if (aEvent.button != 0) {
      return;
    }

    let selectedElements = this._richlistbox.selectedItems;
    if (selectedElements.length != 1) {
      return;
    }

    let element = selectedElements[0];
    if (element._shell) {
      element._shell.doDefaultCommand(aEvent);
    }
  },

  onScroll() {
    this._ensureVisibleElementsAreActive(true);
  },

  onSelect() {
    goUpdateDownloadCommands();

    let selectedElements = this._richlistbox.selectedItems;
    for (let elt of selectedElements) {
      if (elt._shell) {
        elt._shell.onSelect();
      }
    }
  },

  onDragStart(aEvent) {
    // TODO Bug 831358: Support d&d for multiple selection.
    // For now, we just drag the first element.
    let selectedItem = this._richlistbox.selectedItem;
    if (!selectedItem) {
      return;
    }

    let targetPath = selectedItem._shell.download.target.path;
    if (!targetPath) {
      return;
    }

    // We must check for existence synchronously because this is a DOM event.
    let file = new FileUtils.File(targetPath);
    if (!file.exists()) {
      return;
    }

    let dt = aEvent.dataTransfer;
    dt.mozSetDataAt("application/x-moz-file", file, 0);
    let url = Services.io.newFileURI(file).spec;
    dt.setData("text/uri-list", url);
    dt.setData("text/plain", url);
    dt.effectAllowed = "copyMove";
    dt.addElement(selectedItem);
  },

  onDragOver(aEvent) {
    let types = aEvent.dataTransfer.types;
    if (
      types.includes("text/uri-list") ||
      types.includes("text/x-moz-url") ||
      types.includes("text/plain")
    ) {
      aEvent.preventDefault();
    }
  },

  onDrop(aEvent) {
    let dt = aEvent.dataTransfer;
    // If dragged item is from our source, do not try to
    // redownload already downloaded file.
    if (dt.mozGetDataAt("application/x-moz-file", 0)) {
      return;
    }

    let links = Services.droppedLinkHandler.dropLinks(aEvent);
    if (!links.length) {
      return;
    }
    aEvent.preventDefault();
    let browserWin = BrowserWindowTracker.getTopWindow();
    let initiatingDoc = browserWin ? browserWin.document : document;
    for (let link of links) {
      if (link.url.startsWith("about:")) {
        continue;
      }
      DownloadURL(link.url, link.name, initiatingDoc);
    }
  },
};

for (let methodName of ["load", "applyFilter", "selectNode", "selectItems"]) {
  DownloadsPlacesView.prototype[methodName] = function() {
    throw new Error(
      "|" + methodName + "| is not implemented by the downloads view."
    );
  };
}

function goUpdateDownloadCommands() {
  function updateCommandsForObject(object) {
    for (let name in object) {
      if (DownloadsViewUI.isCommandName(name)) {
        goUpdateCommand(name);
      }
    }
  }
  updateCommandsForObject(DownloadsPlacesView.prototype);
  updateCommandsForObject(HistoryDownloadElementShell.prototype);
}

document.addEventListener("DOMContentLoaded", function() {
  let richListBox = document.getElementById("downloadsListBox");
  richListBox.addEventListener("scroll", function(event) {
    return this._placesView.onScroll();
  });
  richListBox.addEventListener("keypress", function(event) {
    return this._placesView.onKeyPress(event);
  });
  richListBox.addEventListener("dblclick", function(event) {
    return this._placesView.onDoubleClick(event);
  });
  richListBox.addEventListener("contextmenu", function(event) {
    return this._placesView.onContextMenu(event);
  });
  richListBox.addEventListener("dragstart", function(event) {
    this._placesView.onDragStart(event);
  });
  let dropNode = richListBox;
  // In about:downloads, also allow drops if the list is empty, by
  // adding the listener to the document, as the richlistbox is
  // hidden when it is empty.
  if (document.documentElement.id == "contentAreaDownloadsView") {
    dropNode = richListBox.parentNode;
  }
  dropNode.addEventListener("dragover", function(event) {
    richListBox._placesView.onDragOver(event);
  });
  dropNode.addEventListener("drop", function(event) {
    richListBox._placesView.onDrop(event);
  });
  richListBox.addEventListener("select", function(event) {
    this._placesView.onSelect();
  });
  richListBox.addEventListener("focus", goUpdateDownloadCommands);
  richListBox.addEventListener("blur", goUpdateDownloadCommands);
});
