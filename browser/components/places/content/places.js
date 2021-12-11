/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from editBookmark.js */
/* import-globals-from ../../../../toolkit/content/contentAreaUtils.js */
/* import-globals-from ../../downloads/content/allDownloadsView.js */

/* Shared Places Import - change other consumers if you change this: */
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.jsm",
  PlacesTransactions: "resource://gre/modules/PlacesTransactions.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
});
XPCOMUtils.defineLazyScriptGetter(
  this,
  "PlacesTreeView",
  "chrome://browser/content/places/treeView.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  ["PlacesInsertionPoint", "PlacesController", "PlacesControllerDragHelper"],
  "chrome://browser/content/places/controller.js"
);
/* End Shared Places Import */

var { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "MigrationUtils",
  "resource:///modules/MigrationUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "BookmarkJSONUtils",
  "resource://gre/modules/BookmarkJSONUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PlacesBackups",
  "resource://gre/modules/PlacesBackups.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "DownloadUtils",
  "resource://gre/modules/DownloadUtils.jsm"
);

const RESTORE_FILEPICKER_FILTER_EXT = "*.json;*.jsonlz4";
const HISTORY_LIBRARY_SEARCH_TELEMETRY =
  "PLACES_HISTORY_LIBRARY_SEARCH_TIME_MS";

var PlacesOrganizer = {
  _places: null,

  _initFolderTree() {
    this._places.place = `place:type=${Ci.nsINavHistoryQueryOptions.RESULTS_AS_LEFT_PANE_QUERY}&excludeItems=1&expandQueries=0`;
  },

  /**
   * Selects a left pane built-in item.
   *
   * @param {string} item The built-in item to select, may be one of (case sensitive):
   *                      AllBookmarks, BookmarksMenu, BookmarksToolbar,
   *                      History, Downloads, Tags, UnfiledBookmarks.
   */
  selectLeftPaneBuiltIn(item) {
    switch (item) {
      case "AllBookmarks":
        this._places.selectItems([PlacesUtils.virtualAllBookmarksGuid]);
        PlacesUtils.asContainer(this._places.selectedNode).containerOpen = true;
        break;
      case "History":
        this._places.selectItems([PlacesUtils.virtualHistoryGuid]);
        PlacesUtils.asContainer(this._places.selectedNode).containerOpen = true;
        break;
      case "Downloads":
        this._places.selectItems([PlacesUtils.virtualDownloadsGuid]);
        break;
      case "Tags":
        this._places.selectItems([PlacesUtils.virtualTagsGuid]);
        break;
      case "BookmarksMenu":
        this.selectLeftPaneContainerByHierarchy([
          PlacesUtils.virtualAllBookmarksGuid,
          PlacesUtils.bookmarks.virtualMenuGuid,
        ]);
        break;
      case "BookmarksToolbar":
        this.selectLeftPaneContainerByHierarchy([
          PlacesUtils.virtualAllBookmarksGuid,
          PlacesUtils.bookmarks.virtualToolbarGuid,
        ]);
        break;
      case "UnfiledBookmarks":
        this.selectLeftPaneContainerByHierarchy([
          PlacesUtils.virtualAllBookmarksGuid,
          PlacesUtils.bookmarks.virtualUnfiledGuid,
        ]);
        break;
      default:
        throw new Error(
          `Unrecognized item ${item} passed to selectLeftPaneRootItem`
        );
    }
  },

  /**
   * Opens a given hierarchy in the left pane, stopping at the last reachable
   * container. Note: item ids should be considered deprecated.
   *
   * @param {array|string|number} aHierarchy
   *        A single container or an array of containers, sorted from
   *        the outmost to the innermost in the hierarchy. Each
   *        container may be either an item id, a Places URI string,
   *        or a named query, like:
   *        "BookmarksMenu", "BookmarksToolbar", "UnfiledBookmarks", "AllBookmarks".
   */
  selectLeftPaneContainerByHierarchy(aHierarchy) {
    if (!aHierarchy) {
      throw new Error("Containers hierarchy not specified");
    }
    let hierarchy = [].concat(aHierarchy);
    let selectWasSuppressed = this._places.view.selection
      .selectEventsSuppressed;
    if (!selectWasSuppressed) {
      this._places.view.selection.selectEventsSuppressed = true;
    }
    try {
      for (let container of hierarchy) {
        if (typeof container != "string") {
          throw new Error("Invalid container type found: " + container);
        }

        try {
          this.selectLeftPaneBuiltIn(container);
        } catch (ex) {
          if (container.substr(0, 6) == "place:") {
            this._places.selectPlaceURI(container);
          } else {
            // Must be a guid.
            this._places.selectItems([container], false);
          }
        }
        PlacesUtils.asContainer(this._places.selectedNode).containerOpen = true;
      }
    } finally {
      if (!selectWasSuppressed) {
        this._places.view.selection.selectEventsSuppressed = false;
      }
    }
  },

  init: function PO_init() {
    // Register the downloads view.
    const DOWNLOADS_QUERY =
      "place:transition=" +
      Ci.nsINavHistoryService.TRANSITION_DOWNLOAD +
      "&sort=" +
      Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING;

    ContentArea.setContentViewForQueryString(
      DOWNLOADS_QUERY,
      () =>
        new DownloadsPlacesView(
          document.getElementById("downloadsRichListBox"),
          false
        ),
      {
        showDetailsPane: false,
        toolbarSet:
          "back-button, forward-button, organizeButton, clearDownloadsButton, libraryToolbarSpacer, searchFilter",
      }
    );

    ContentArea.init();

    this._places = document.getElementById("placesList");
    this._initFolderTree();

    var leftPaneSelection = "AllBookmarks"; // default to all-bookmarks
    if (window.arguments && window.arguments[0]) {
      leftPaneSelection = window.arguments[0];
    }

    this.selectLeftPaneContainerByHierarchy(leftPaneSelection);
    if (leftPaneSelection === "History") {
      let historyNode = this._places.selectedNode;
      if (historyNode.childCount > 0) {
        this._places.selectNode(historyNode.getChild(0));
      }
    }

    // clear the back-stack
    this._backHistory.splice(0, this._backHistory.length);
    document
      .getElementById("OrganizerCommand:Back")
      .setAttribute("disabled", true);

    // Set up the search UI.
    PlacesSearchBox.init();

    window.addEventListener("AppCommand", this, true);

    let placeContentElement = document.getElementById("placeContent");
    placeContentElement.addEventListener("onOpenFlatContainer", function(e) {
      PlacesOrganizer.openFlatContainer(e.detail);
    });

    if (AppConstants.platform === "macosx") {
      // 1. Map Edit->Find command to OrganizerCommand_find:all.  Need to map
      // both the menuitem and the Find key.
      let findMenuItem = document.getElementById("menu_find");
      findMenuItem.setAttribute("command", "OrganizerCommand_find:all");
      let findKey = document.getElementById("key_find");
      findKey.setAttribute("command", "OrganizerCommand_find:all");

      // 2. Disable some keybindings from browser.xhtml
      let elements = ["cmd_handleBackspace", "cmd_handleShiftBackspace"];
      for (let i = 0; i < elements.length; i++) {
        document.getElementById(elements[i]).setAttribute("disabled", "true");
      }
    }

    // remove the "Edit" and "Edit Bookmark" context-menu item, we're in our own details pane
    let contextMenu = document.getElementById("placesContext");
    contextMenu.removeChild(document.getElementById("placesContext_show:info"));
    contextMenu.removeChild(
      document.getElementById("placesContext_show_bookmark:info")
    );
    contextMenu.removeChild(
      document.getElementById("placesContext_show_folder:info")
    );

    if (!Services.policies.isAllowed("profileImport")) {
      document
        .getElementById("OrganizerCommand_browserImport")
        .setAttribute("disabled", true);
    }

    ContentArea.focus();
  },

  QueryInterface: ChromeUtils.generateQI([]),

  handleEvent: function PO_handleEvent(aEvent) {
    if (aEvent.type != "AppCommand") {
      return;
    }

    aEvent.stopPropagation();
    switch (aEvent.command) {
      case "Back":
        if (this._backHistory.length) {
          this.back();
        }
        break;
      case "Forward":
        if (this._forwardHistory.length) {
          this.forward();
        }
        break;
      case "Search":
        PlacesSearchBox.findAll();
        break;
    }
  },

  destroy: function PO_destroy() {},

  _location: null,
  get location() {
    return this._location;
  },

  set location(aLocation) {
    if (!aLocation || this._location == aLocation) {
      return;
    }

    if (this.location) {
      this._backHistory.unshift(this.location);
      this._forwardHistory.splice(0, this._forwardHistory.length);
    }

    this._location = aLocation;
    this._places.selectPlaceURI(aLocation);

    if (!this._places.hasSelection) {
      // If no node was found for the given place: uri, just load it directly
      ContentArea.currentPlace = aLocation;
    }
    this.updateDetailsPane();

    // update navigation commands
    if (!this._backHistory.length) {
      document
        .getElementById("OrganizerCommand:Back")
        .setAttribute("disabled", true);
    } else {
      document
        .getElementById("OrganizerCommand:Back")
        .removeAttribute("disabled");
    }
    if (!this._forwardHistory.length) {
      document
        .getElementById("OrganizerCommand:Forward")
        .setAttribute("disabled", true);
    } else {
      document
        .getElementById("OrganizerCommand:Forward")
        .removeAttribute("disabled");
    }
  },

  _backHistory: [],
  _forwardHistory: [],

  back: function PO_back() {
    this._forwardHistory.unshift(this.location);
    var historyEntry = this._backHistory.shift();
    this._location = null;
    this.location = historyEntry;
  },
  forward: function PO_forward() {
    this._backHistory.unshift(this.location);
    var historyEntry = this._forwardHistory.shift();
    this._location = null;
    this.location = historyEntry;
  },

  /**
   * Called when a place folder is selected in the left pane.
   * @param   resetSearchBox
   *          true if the search box should also be reset, false otherwise.
   *          The search box should be reset when a new folder in the left
   *          pane is selected; the search scope and text need to be cleared in
   *          preparation for the new folder.  Note that if the user manually
   *          resets the search box, either by clicking its reset button or by
   *          deleting its text, this will be false.
   */
  _cachedLeftPaneSelectedURI: null,
  onPlaceSelected: function PO_onPlaceSelected(resetSearchBox) {
    // Don't change the right-hand pane contents when there's no selection.
    if (!this._places.hasSelection) {
      return;
    }

    let node = this._places.selectedNode;
    let placeURI = node.uri;

    // If either the place of the content tree in the right pane has changed or
    // the user cleared the search box, update the place, hide the search UI,
    // and update the back/forward buttons by setting location.
    if (ContentArea.currentPlace != placeURI || !resetSearchBox) {
      ContentArea.currentPlace = placeURI;
      this.location = placeURI;
    }

    // When we invalidate a container we use suppressSelectionEvent, when it is
    // unset a select event is fired, in many cases the selection did not really
    // change, so we should check for it, and return early in such a case. Note
    // that we cannot return any earlier than this point, because when
    // !resetSearchBox, we need to update location and hide the UI as above,
    // even though the selection has not changed.
    if (placeURI == this._cachedLeftPaneSelectedURI) {
      return;
    }
    this._cachedLeftPaneSelectedURI = placeURI;

    // At this point, resetSearchBox is true, because the left pane selection
    // has changed; otherwise we would have returned earlier.

    let input = PlacesSearchBox.searchFilter;
    input.value = "";
    try {
      input.editor.transactionManager.clear();
    } catch (e) {}
    this._setSearchScopeForNode(node);
    this.updateDetailsPane();
  },

  /**
   * Sets the search scope based on aNode's properties.
   * @param {object} aNode
   *          the node to set up scope from
   */
  _setSearchScopeForNode: function PO__setScopeForNode(aNode) {
    let itemGuid = aNode.bookmarkGuid;

    if (
      PlacesUtils.nodeIsHistoryContainer(aNode) ||
      itemGuid == PlacesUtils.virtualHistoryGuid
    ) {
      PlacesQueryBuilder.setScope("history");
    } else if (itemGuid == PlacesUtils.virtualDownloadsGuid) {
      PlacesQueryBuilder.setScope("downloads");
    } else {
      // Default to All Bookmarks for all other nodes, per bug 469437.
      PlacesQueryBuilder.setScope("bookmarks");
    }
  },

  /**
   * Handle clicks on the places list.
   * Single Left click, right click or modified click do not result in any
   * special action, since they're related to selection.
   * @param {object} aEvent
   *          The mouse event.
   */
  onPlacesListClick: function PO_onPlacesListClick(aEvent) {
    // Only handle clicks on tree children.
    if (aEvent.target.localName != "treechildren") {
      return;
    }

    let node = this._places.selectedNode;
    if (node) {
      let middleClick = aEvent.button == 1 && aEvent.detail == 1;
      if (middleClick && PlacesUtils.nodeIsContainer(node)) {
        // The command execution function will take care of seeing if the
        // selection is a folder or a different container type, and will
        // load its contents in tabs.
        PlacesUIUtils.openMultipleLinksInTabs(node, aEvent, this._places);
      }
    }
  },

  /**
   * Handle focus changes on the places list and the current content view.
   */
  updateDetailsPane: function PO_updateDetailsPane() {
    if (!ContentArea.currentViewOptions.showDetailsPane) {
      return;
    }
    let view = PlacesUIUtils.getViewForNode(document.activeElement);
    if (view) {
      let selectedNodes = view.selectedNode
        ? [view.selectedNode]
        : view.selectedNodes;
      this._fillDetailsPane(selectedNodes);
    }
  },

  /**
   * Handle openFlatContainer events.
   * @param {object} aContainer
   *        The node the event was dispatched on.
   */
  openFlatContainer(aContainer) {
    if (aContainer.bookmarkGuid) {
      PlacesUtils.asContainer(this._places.selectedNode).containerOpen = true;
      this._places.selectItems([aContainer.bookmarkGuid], false);
    } else if (PlacesUtils.nodeIsQuery(aContainer)) {
      this._places.selectPlaceURI(aContainer.uri);
    }
  },

  /**
   * @returns {object}
   * Returns the options associated with the query currently loaded in the
   * main places pane.
   */
  getCurrentOptions: function PO_getCurrentOptions() {
    return PlacesUtils.asQuery(ContentArea.currentView.result.root)
      .queryOptions;
  },

  /**
   * Show the migration wizard for importing passwords,
   * cookies, history, preferences, and bookmarks.
   */
  importFromBrowser: function PO_importFromBrowser() {
    // We pass in the type of source we're using for use in telemetry:
    MigrationUtils.showMigrationWizard(window, [
      MigrationUtils.MIGRATION_ENTRYPOINT_PLACES,
    ]);
  },

  /**
   * Open a file-picker and import the selected file into the bookmarks store
   */
  importFromFile: function PO_importFromFile() {
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    let fpCallback = function fpCallback_done(aResult) {
      if (aResult != Ci.nsIFilePicker.returnCancel && fp.fileURL) {
        var { BookmarkHTMLUtils } = ChromeUtils.import(
          "resource://gre/modules/BookmarkHTMLUtils.jsm"
        );
        BookmarkHTMLUtils.importFromURL(fp.fileURL.spec).catch(Cu.reportError);
      }
    };

    fp.init(
      window,
      PlacesUIUtils.getString("SelectImport"),
      Ci.nsIFilePicker.modeOpen
    );
    fp.appendFilters(Ci.nsIFilePicker.filterHTML);
    fp.open(fpCallback);
  },

  /**
   * Allows simple exporting of bookmarks.
   */
  exportBookmarks: function PO_exportBookmarks() {
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    let fpCallback = function fpCallback_done(aResult) {
      if (aResult != Ci.nsIFilePicker.returnCancel) {
        var { BookmarkHTMLUtils } = ChromeUtils.import(
          "resource://gre/modules/BookmarkHTMLUtils.jsm"
        );
        BookmarkHTMLUtils.exportToFile(fp.file.path).catch(Cu.reportError);
      }
    };

    fp.init(
      window,
      PlacesUIUtils.getString("EnterExport"),
      Ci.nsIFilePicker.modeSave
    );
    fp.appendFilters(Ci.nsIFilePicker.filterHTML);
    fp.defaultString = "bookmarks.html";
    fp.open(fpCallback);
  },

  /**
   * Populates the restore menu with the dates of the backups available.
   */
  populateRestoreMenu: function PO_populateRestoreMenu() {
    let restorePopup = document.getElementById("fileRestorePopup");

    const dtOptions = {
      dateStyle: "long",
    };
    let dateFormatter = new Services.intl.DateTimeFormat(undefined, dtOptions);

    // Remove existing menu items.  Last item is the restoreFromFile item.
    while (restorePopup.childNodes.length > 1) {
      restorePopup.firstChild.remove();
    }

    (async function() {
      let backupFiles = await PlacesBackups.getBackupFiles();
      if (!backupFiles.length) {
        return;
      }

      // Populate menu with backups.
      for (let i = 0; i < backupFiles.length; i++) {
        let fileSize = (await IOUtils.stat(backupFiles[i])).size;
        let [size, unit] = DownloadUtils.convertByteUnits(fileSize);
        let sizeString = PlacesUtils.getFormattedString("backupFileSizeText", [
          size,
          unit,
        ]);
        let sizeInfo;
        let bookmarkCount = PlacesBackups.getBookmarkCountForFile(
          backupFiles[i]
        );
        if (bookmarkCount != null) {
          sizeInfo =
            " (" +
            sizeString +
            " - " +
            PlacesUIUtils.getPluralString(
              "detailsPane.itemsCountLabel",
              bookmarkCount,
              [bookmarkCount]
            ) +
            ")";
        } else {
          sizeInfo = " (" + sizeString + ")";
        }

        let backupDate = PlacesBackups.getDateForFile(backupFiles[i]);
        let m = restorePopup.insertBefore(
          document.createXULElement("menuitem"),
          document.getElementById("restoreFromFile")
        );
        m.setAttribute("label", dateFormatter.format(backupDate) + sizeInfo);
        m.setAttribute("value", OS.Path.basename(backupFiles[i]));
        m.setAttribute(
          "oncommand",
          "PlacesOrganizer.onRestoreMenuItemClick(this);"
        );
      }

      // Add the restoreFromFile item.
      restorePopup.insertBefore(
        document.createXULElement("menuseparator"),
        document.getElementById("restoreFromFile")
      );
    })();
  },

  /**
   * Called when a menuitem is selected from the restore menu.
   *
   * @param {object} aMenuItem
   */
  async onRestoreMenuItemClick(aMenuItem) {
    let backupName = aMenuItem.getAttribute("value");
    let backupFilePaths = await PlacesBackups.getBackupFiles();
    for (let backupFilePath of backupFilePaths) {
      if (OS.Path.basename(backupFilePath) == backupName) {
        PlacesOrganizer.restoreBookmarksFromFile(backupFilePath);
        break;
      }
    }
  },

  /**
   * Called when 'Choose File...' is selected from the restore menu.
   * Prompts for a file and restores bookmarks to those in the file.
   */
  onRestoreBookmarksFromFile: function PO_onRestoreBookmarksFromFile() {
    let backupsDir = Services.dirsvc.get("Desk", Ci.nsIFile);
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    let fpCallback = aResult => {
      if (aResult != Ci.nsIFilePicker.returnCancel) {
        this.restoreBookmarksFromFile(fp.file.path);
      }
    };

    fp.init(
      window,
      PlacesUIUtils.getString("bookmarksRestoreTitle"),
      Ci.nsIFilePicker.modeOpen
    );
    fp.appendFilter(
      PlacesUIUtils.getString("bookmarksRestoreFilterName"),
      RESTORE_FILEPICKER_FILTER_EXT
    );
    fp.appendFilters(Ci.nsIFilePicker.filterAll);
    fp.displayDirectory = backupsDir;
    fp.open(fpCallback);
  },

  /**
   * Restores bookmarks from a JSON file.
   *
   * @param {string} aFilePath
   *   The path of the file to restore from.
   */
  restoreBookmarksFromFile: function PO_restoreBookmarksFromFile(aFilePath) {
    // check file extension
    if (
      !aFilePath.toLowerCase().endsWith("json") &&
      !aFilePath.toLowerCase().endsWith("jsonlz4")
    ) {
      this._showErrorAlert(
        PlacesUIUtils.getString("bookmarksRestoreFormatError")
      );
      return;
    }

    // confirm ok to delete existing bookmarks
    if (
      !Services.prompt.confirm(
        null,
        PlacesUIUtils.getString("bookmarksRestoreAlertTitle"),
        PlacesUIUtils.getString("bookmarksRestoreAlert")
      )
    ) {
      return;
    }

    (async function() {
      try {
        await BookmarkJSONUtils.importFromFile(aFilePath, {
          replace: true,
        });
      } catch (ex) {
        PlacesOrganizer._showErrorAlert(
          PlacesUIUtils.getString("bookmarksRestoreParseError")
        );
      }
    })();
  },

  _showErrorAlert: function PO__showErrorAlert(aMsg) {
    var brandShortName = document
      .getElementById("brandStrings")
      .getString("brandShortName");

    Services.prompt.alert(window, brandShortName, aMsg);
  },

  /**
   * Backup bookmarks to desktop, auto-generate a filename with a date.
   * The file is a JSON serialization of bookmarks, tags and any annotations
   * of those items.
   */
  backupBookmarks: function PO_backupBookmarks() {
    let backupsDir = Services.dirsvc.get("Desk", Ci.nsIFile);
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    let fpCallback = function fpCallback_done(aResult) {
      if (aResult != Ci.nsIFilePicker.returnCancel) {
        // There is no OS.File version of the filepicker yet (Bug 937812).
        PlacesBackups.saveBookmarksToJSONFile(fp.file.path).catch(
          Cu.reportError
        );
      }
    };

    fp.init(
      window,
      PlacesUIUtils.getString("bookmarksBackupTitle"),
      Ci.nsIFilePicker.modeSave
    );
    fp.appendFilter(
      PlacesUIUtils.getString("bookmarksRestoreFilterName"),
      RESTORE_FILEPICKER_FILTER_EXT
    );
    fp.defaultString = PlacesBackups.getFilenameForDate();
    fp.defaultExtension = "json";
    fp.displayDirectory = backupsDir;
    fp.open(fpCallback);
  },

  _fillDetailsPane: function PO__fillDetailsPane(aNodeList) {
    var infoBox = document.getElementById("infoBox");
    var itemsCountBox = document.getElementById("itemsCountBox");

    // Make sure the infoBox UI is visible if we need to use it, we hide it
    // below when we don't.
    infoBox.hidden = false;
    itemsCountBox.hidden = true;

    let selectedNode = aNodeList.length == 1 ? aNodeList[0] : null;

    // If an input within a panel is focused, force-blur it so its contents
    // are saved
    if (gEditItemOverlay.itemId != -1) {
      var focusedElement = document.commandDispatcher.focusedElement;
      if (
        (focusedElement instanceof HTMLInputElement ||
          focusedElement instanceof HTMLTextAreaElement) &&
        /^editBMPanel.*/.test(focusedElement.parentNode.parentNode.id)
      ) {
        focusedElement.blur();
      }

      // don't update the panel if we are already editing this node unless we're
      // in multi-edit mode
      if (selectedNode) {
        let concreteId = PlacesUtils.getConcreteItemId(selectedNode);
        var nodeIsSame =
          gEditItemOverlay.itemId == selectedNode.itemId ||
          gEditItemOverlay.itemId == concreteId ||
          (selectedNode.itemId == -1 &&
            gEditItemOverlay.uri &&
            gEditItemOverlay.uri == selectedNode.uri);
        if (nodeIsSame && !infoBox.hidden && !gEditItemOverlay.multiEdit) {
          return;
        }
      }
    }

    // Clean up the panel before initing it again.
    gEditItemOverlay.uninitPanel(false);

    if (selectedNode && !PlacesUtils.nodeIsSeparator(selectedNode)) {
      gEditItemOverlay.initPanel({
        node: selectedNode,
        hiddenRows: ["folderPicker"],
      });
    } else if (!selectedNode && aNodeList[0]) {
      if (aNodeList.every(PlacesUtils.nodeIsURI)) {
        let uris = aNodeList.map(node => Services.io.newURI(node.uri));
        gEditItemOverlay.initPanel({
          uris,
          hiddenRows: ["folderPicker", "location", "keyword", "name"],
        });
      } else {
        let selectItemDesc = document.getElementById("selectItemDescription");
        let itemsCountLabel = document.getElementById("itemsCountText");
        selectItemDesc.hidden = false;
        itemsCountLabel.value = PlacesUIUtils.getPluralString(
          "detailsPane.itemsCountLabel",
          aNodeList.length,
          [aNodeList.length]
        );
        infoBox.hidden = true;
      }
    } else {
      infoBox.hidden = true;
      let selectItemDesc = document.getElementById("selectItemDescription");
      let itemsCountLabel = document.getElementById("itemsCountText");
      let itemsCount = 0;
      if (ContentArea.currentView.result) {
        let rootNode = ContentArea.currentView.result.root;
        if (rootNode.containerOpen) {
          itemsCount = rootNode.childCount;
        }
      }
      if (itemsCount == 0) {
        selectItemDesc.hidden = true;
        itemsCountLabel.value = PlacesUIUtils.getString("detailsPane.noItems");
      } else {
        selectItemDesc.hidden = false;
        itemsCountLabel.value = PlacesUIUtils.getPluralString(
          "detailsPane.itemsCountLabel",
          itemsCount,
          [itemsCount]
        );
      }
    }
    itemsCountBox.hidden = !infoBox.hidden;
  },
};

/**
 * A set of utilities relating to search within Bookmarks and History.
 */
var PlacesSearchBox = {
  /**
   * The Search text field
   */
  get searchFilter() {
    return document.getElementById("searchFilter");
  },

  /**
   * Folders to include when searching.
   */
  _folders: [],
  get folders() {
    if (!this._folders.length) {
      this._folders = PlacesUtils.bookmarks.userContentRoots;
    }
    return this._folders;
  },
  set folders(aFolders) {
    this._folders = aFolders;
  },

  /**
   * Run a search for the specified text, over the collection specified by
   * the dropdown arrow. The default is all bookmarks, but can be
   * localized to the active collection.
   * @param {string} filterString
   *          The text to search for.
   */
  search: function PSB_search(filterString) {
    var PO = PlacesOrganizer;
    // If the user empties the search box manually, reset it and load all
    // contents of the current scope.
    // XXX this might be to jumpy, maybe should search for "", so results
    // are ungrouped, and search box not reset
    if (filterString == "") {
      PO.onPlaceSelected(false);
      return;
    }

    let currentView = ContentArea.currentView;

    // Search according to the current scope, which was set by
    // PQB_setScope()
    switch (PlacesSearchBox.filterCollection) {
      case "bookmarks":
        currentView.applyFilter(filterString, this.folders);
        break;
      case "history": {
        let currentOptions = PO.getCurrentOptions();
        if (
          currentOptions.queryType !=
          Ci.nsINavHistoryQueryOptions.QUERY_TYPE_HISTORY
        ) {
          let query = PlacesUtils.history.getNewQuery();
          query.searchTerms = filterString;
          let options = currentOptions.clone();
          // Make sure we're getting uri results.
          options.resultType = currentOptions.RESULTS_AS_URI;
          options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_HISTORY;
          options.includeHidden = true;
          currentView.load([query], options);
        } else {
          TelemetryStopwatch.start(HISTORY_LIBRARY_SEARCH_TELEMETRY);
          currentView.applyFilter(filterString, null, true);
          TelemetryStopwatch.finish(HISTORY_LIBRARY_SEARCH_TELEMETRY);
        }
        break;
      }
      case "downloads": {
        // The new downloads view doesn't use places for searching downloads.
        currentView.searchTerm = filterString;
        break;
      }
      default:
        throw new Error("Invalid filterCollection on search");
    }

    // Update the details panel
    PlacesOrganizer.updateDetailsPane();
  },

  /**
   * Finds across all history, downloads or all bookmarks.
   */
  findAll: function PSB_findAll() {
    switch (this.filterCollection) {
      case "history":
        PlacesQueryBuilder.setScope("history");
        break;
      case "downloads":
        PlacesQueryBuilder.setScope("downloads");
        break;
      default:
        PlacesQueryBuilder.setScope("bookmarks");
        break;
    }
    this.focus();
  },

  /**
   * Updates the display with the title of the current collection.
   * @param {string} aTitle
   *          The title of the current collection.
   */
  updateCollectionTitle: function PSB_updateCollectionTitle(aTitle) {
    let title = "";
    switch (this.filterCollection) {
      case "history":
        title = PlacesUIUtils.getString("searchHistory");
        break;
      case "downloads":
        title = PlacesUIUtils.getString("searchDownloads");
        break;
      default:
        title = PlacesUIUtils.getString("searchBookmarks");
    }
    this.searchFilter.placeholder = title;
  },

  /**
   * Gets/sets the active collection from the dropdown menu.
   */
  get filterCollection() {
    return this.searchFilter.getAttribute("collection");
  },
  set filterCollection(collectionName) {
    if (collectionName == this.filterCollection) {
      return;
    }

    this.searchFilter.setAttribute("collection", collectionName);
    this.updateCollectionTitle();
  },

  /**
   * Focus the search box
   */
  focus: function PSB_focus() {
    this.searchFilter.focus();
  },

  /**
   * Set up the gray text in the search bar as the Places View loads.
   */
  init: function PSB_init() {
    this.updateCollectionTitle();
  },

  /**
   * Gets or sets the text shown in the Places Search Box
   */
  get value() {
    return this.searchFilter.value;
  },
  set value(value) {
    this.searchFilter.value = value;
  },
};

/**
 * Functions and data for advanced query builder
 */
var PlacesQueryBuilder = {
  queries: [],
  queryOptions: null,

  /**
   * Sets the search scope.  This can be called when no search is active, and
   * in that case, when the user does begin a search aScope will be used (see
   * PSB_search()).  If there is an active search, it's performed again to
   * update the content tree.
   * @param {string} aScope
   *          The search scope: "bookmarks", "collection", "downloads" or
   *          "history".
   */
  setScope: function PQB_setScope(aScope) {
    // Determine filterCollection, folders, and scopeButtonId based on aScope.
    var filterCollection;
    var folders = [];
    switch (aScope) {
      case "history":
        filterCollection = "history";
        break;
      case "bookmarks":
        filterCollection = "bookmarks";
        folders = PlacesUtils.bookmarks.userContentRoots;
        break;
      case "downloads":
        filterCollection = "downloads";
        break;
      default:
        throw new Error("Invalid search scope");
    }

    // Update the search box.  Re-search if there's an active search.
    PlacesSearchBox.filterCollection = filterCollection;
    PlacesSearchBox.folders = folders;
    var searchStr = PlacesSearchBox.searchFilter.value;
    if (searchStr) {
      PlacesSearchBox.search(searchStr);
    }
  },
};

/**
 * Population and commands for the View Menu.
 */
var ViewMenu = {
  /**
   * Removes content generated previously from a menupopup.
   * @param {object} popup
   *          The popup that contains the previously generated content.
   * @param {string} startID
   *          The id attribute of an element that is the start of the
   *          dynamically generated region - remove elements after this
   *          item only.
   *          Must be contained by popup. Can be null (in which case the
   *          contents of popup are removed).
   * @param {string} endID
   *          The id attribute of an element that is the end of the
   *          dynamically generated region - remove elements up to this
   *          item only.
   *          Must be contained by popup. Can be null (in which case all
   *          items until the end of the popup will be removed). Ignored
   *          if startID is null.
   * @returns {object|null} The element for the caller to insert new items before,
   *          null if the caller should just append to the popup.
   */
  _clean: function VM__clean(popup, startID, endID) {
    if (endID && !startID) {
      throw new Error("meaningless to have valid endID and null startID");
    }
    if (startID) {
      var startElement = document.getElementById(startID);
      if (startElement.parentNode != popup) {
        throw new Error("startElement is not in popup");
      }
      if (!startElement) {
        throw new Error("startID does not correspond to an existing element");
      }
      var endElement = null;
      if (endID) {
        endElement = document.getElementById(endID);
        if (endElement.parentNode != popup) {
          throw new Error("endElement is not in popup");
        }
        if (!endElement) {
          throw new Error("endID does not correspond to an existing element");
        }
      }
      while (startElement.nextSibling != endElement) {
        popup.removeChild(startElement.nextSibling);
      }
      return endElement;
    }
    while (popup.hasChildNodes()) {
      popup.firstChild.remove();
    }
    return null;
  },

  /**
   * Fills a menupopup with a list of columns
   * @param {object} event
   *          The popupshowing event that invoked this function.
   * @param {string} startID
   *          see _clean
   * @param {string} endID
   *          see _clean
   * @param {string} type
   *          the type of the menuitem, e.g. "radio" or "checkbox".
   *          Can be null (no-type).
   *          Checkboxes are checked if the column is visible.
   * @param {string} propertyPrefix
   *          If propertyPrefix is non-null:
   *          propertyPrefix + column ID + ".label" will be used to get the
   *          localized label string.
   *          propertyPrefix + column ID + ".accesskey" will be used to get the
   *          localized accesskey.
   *          If propertyPrefix is null, the column label is used as label and
   *          no accesskey is assigned.
   */
  fillWithColumns: function VM_fillWithColumns(
    event,
    startID,
    endID,
    type,
    propertyPrefix
  ) {
    var popup = event.target;
    var pivot = this._clean(popup, startID, endID);

    var content = document.getElementById("placeContent");
    var columns = content.columns;
    for (var i = 0; i < columns.count; ++i) {
      var column = columns.getColumnAt(i).element;
      var menuitem = document.createXULElement("menuitem");
      menuitem.id = "menucol_" + column.id;
      menuitem.column = column;
      var label = column.getAttribute("label");
      if (propertyPrefix) {
        var menuitemPrefix = propertyPrefix;
        // for string properties, use "name" as the id, instead of "title"
        // see bug #386287 for details
        var columnId = column.getAttribute("anonid");
        menuitemPrefix += columnId == "title" ? "name" : columnId;
        label = PlacesUIUtils.getString(menuitemPrefix + ".label");
        var accesskey = PlacesUIUtils.getString(menuitemPrefix + ".accesskey");
        menuitem.setAttribute("accesskey", accesskey);
      }
      menuitem.setAttribute("label", label);
      if (type == "radio") {
        menuitem.setAttribute("type", "radio");
        menuitem.setAttribute("name", "columns");
        // This column is the sort key. Its item is checked.
        if (column.getAttribute("sortDirection") != "") {
          menuitem.setAttribute("checked", "true");
        }
      } else if (type == "checkbox") {
        menuitem.setAttribute("type", "checkbox");
        // Cannot uncheck the primary column.
        if (column.getAttribute("primary") == "true") {
          menuitem.setAttribute("disabled", "true");
        }
        // Items for visible columns are checked.
        if (!column.hidden) {
          menuitem.setAttribute("checked", "true");
        }
      }
      if (pivot) {
        popup.insertBefore(menuitem, pivot);
      } else {
        popup.appendChild(menuitem);
      }
    }
    event.stopPropagation();
  },

  /**
   * Set up the content of the view menu.
   *
   * @param {object} event
   */
  populateSortMenu: function VM_populateSortMenu(event) {
    this.fillWithColumns(
      event,
      "viewUnsorted",
      "directionSeparator",
      "radio",
      "view.sortBy.1."
    );

    var sortColumn = this._getSortColumn();
    var viewSortAscending = document.getElementById("viewSortAscending");
    var viewSortDescending = document.getElementById("viewSortDescending");
    // We need to remove an existing checked attribute because the unsorted
    // menu item is not rebuilt every time we open the menu like the others.
    var viewUnsorted = document.getElementById("viewUnsorted");
    if (!sortColumn) {
      viewSortAscending.removeAttribute("checked");
      viewSortDescending.removeAttribute("checked");
      viewUnsorted.setAttribute("checked", "true");
    } else if (sortColumn.getAttribute("sortDirection") == "ascending") {
      viewSortAscending.setAttribute("checked", "true");
      viewSortDescending.removeAttribute("checked");
      viewUnsorted.removeAttribute("checked");
    } else if (sortColumn.getAttribute("sortDirection") == "descending") {
      viewSortDescending.setAttribute("checked", "true");
      viewSortAscending.removeAttribute("checked");
      viewUnsorted.removeAttribute("checked");
    }
  },

  /**
   * Shows/Hides a tree column.
   * @param {object} element
   *          The menuitem element for the column
   */
  showHideColumn: function VM_showHideColumn(element) {
    var column = element.column;

    var splitter = column.nextSibling;
    if (splitter && splitter.localName != "splitter") {
      splitter = null;
    }

    const isChecked = element.getAttribute("checked") == "true";
    column.hidden = !isChecked;
    if (splitter) {
      splitter.hidden = !isChecked;
    }
  },

  /**
   * Gets the last column that was sorted.
   * @returns {object|null} the currently sorted column, null if there is no sorted column.
   */
  _getSortColumn: function VM__getSortColumn() {
    var content = document.getElementById("placeContent");
    var cols = content.columns;
    for (var i = 0; i < cols.count; ++i) {
      var column = cols.getColumnAt(i).element;
      var sortDirection = column.getAttribute("sortDirection");
      if (sortDirection == "ascending" || sortDirection == "descending") {
        return column;
      }
    }
    return null;
  },

  /**
   * Sorts the view by the specified column.
   * @param {object} aColumn
   *          The colum that is the sort key. Can be null - the
   *          current sort column or the title column will be used.
   * @param {string} aDirection
   *          The direction to sort - "ascending" or "descending".
   *          Can be null - the last direction or descending will be used.
   *
   * If both aColumnID and aDirection are null, the view will be unsorted.
   */
  setSortColumn: function VM_setSortColumn(aColumn, aDirection) {
    var result = document.getElementById("placeContent").result;
    if (!aColumn && !aDirection) {
      result.sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_NONE;
      return;
    }

    var columnId;
    if (aColumn) {
      columnId = aColumn.getAttribute("anonid");
      if (!aDirection) {
        let sortColumn = this._getSortColumn();
        if (sortColumn) {
          aDirection = sortColumn.getAttribute("sortDirection");
        }
      }
    } else {
      let sortColumn = this._getSortColumn();
      columnId = sortColumn ? sortColumn.getAttribute("anonid") : "title";
    }

    // This maps the possible values of columnId (i.e., anonid's of treecols in
    // placeContent) to the default sortingMode for each column.
    //   key:  Sort key in the name of one of the
    //         nsINavHistoryQueryOptions.SORT_BY_* constants
    //   dir:  Default sort direction to use if none has been specified
    const colLookupTable = {
      title: { key: "TITLE", dir: "ascending" },
      tags: { key: "TAGS", dir: "ascending" },
      url: { key: "URI", dir: "ascending" },
      date: { key: "DATE", dir: "descending" },
      visitCount: { key: "VISITCOUNT", dir: "descending" },
      dateAdded: { key: "DATEADDED", dir: "descending" },
      lastModified: { key: "LASTMODIFIED", dir: "descending" },
    };

    // Make sure we have a valid column.
    if (!colLookupTable.hasOwnProperty(columnId)) {
      throw new Error("Invalid column");
    }

    // Use a default sort direction if none has been specified.  If aDirection
    // is invalid, result.sortingMode will be undefined, which has the effect
    // of unsorting the tree.
    aDirection = (aDirection || colLookupTable[columnId].dir).toUpperCase();

    var sortConst =
      "SORT_BY_" + colLookupTable[columnId].key + "_" + aDirection;
    result.sortingMode = Ci.nsINavHistoryQueryOptions[sortConst];
  },
};

var ContentArea = {
  _specialViews: new Map(),

  init: function CA_init() {
    this._box = document.getElementById("placesViewsBox");
    this._toolbar = document.getElementById("placesToolbar");
    ContentTree.init();
    this._setupView();
  },

  /**
   * Gets the content view to be used for loading the given query.
   * If a custom view was set by setContentViewForQueryString, that
   * view would be returned, else the default tree view is returned
   *
   * @param {string} aQueryString
   *        a query string
   * @returns {object} the view to be used for loading aQueryString.
   */
  getContentViewForQueryString: function CA_getContentViewForQueryString(
    aQueryString
  ) {
    try {
      if (this._specialViews.has(aQueryString)) {
        let { view, options } = this._specialViews.get(aQueryString);
        if (typeof view == "function") {
          view = view();
          this._specialViews.set(aQueryString, { view, options });
        }
        return view;
      }
    } catch (ex) {
      Cu.reportError(ex);
    }
    return ContentTree.view;
  },

  /**
   * Sets a custom view to be used rather than the default places tree
   * whenever the given query is selected in the left pane.
   * @param {string} aQueryString
   *        a query string
   * @param {object} aView
   *        Either the custom view or a function that will return the view
   *        the first (and only) time it's called.
   * @param {object} [aOptions]
   *        Object defining special options for the view.
   * @see ContentTree.viewOptions for supported options and default values.
   */
  setContentViewForQueryString: function CA_setContentViewForQueryString(
    aQueryString,
    aView,
    aOptions
  ) {
    if (
      !aQueryString ||
      (typeof aView != "object" && typeof aView != "function")
    ) {
      throw new Error("Invalid arguments");
    }

    this._specialViews.set(aQueryString, {
      view: aView,
      options: aOptions || {},
    });
  },

  get currentView() {
    let selectedPane = [...this._box.children].filter(
      child => !child.hidden
    )[0];
    return PlacesUIUtils.getViewForNode(selectedPane);
  },
  set currentView(aNewView) {
    let oldView = this.currentView;
    if (oldView != aNewView) {
      oldView.associatedElement.hidden = true;
      aNewView.associatedElement.hidden = false;

      // If the content area inactivated view was focused, move focus
      // to the new view.
      if (document.activeElement == oldView.associatedElement) {
        aNewView.associatedElement.focus();
      }
    }
  },

  get currentPlace() {
    return this.currentView.place;
  },
  set currentPlace(aQueryString) {
    let oldView = this.currentView;
    let newView = this.getContentViewForQueryString(aQueryString);
    newView.place = aQueryString;
    if (oldView != newView) {
      oldView.active = false;
      this.currentView = newView;
      this._setupView();
      newView.active = true;
    }
  },

  /**
   * Applies view options.
   */
  _setupView: function CA__setupView() {
    let options = this.currentViewOptions;

    // showDetailsPane.
    let detailsPane = document.getElementById("detailsPane");
    detailsPane.hidden = !options.showDetailsPane;

    // toolbarSet.
    for (let elt of this._toolbar.childNodes) {
      // On Windows and Linux the menu buttons are menus wrapped in a menubar.
      if (elt.id == "placesMenu") {
        for (let menuElt of elt.childNodes) {
          menuElt.hidden = !options.toolbarSet.includes(menuElt.id);
        }
      } else {
        elt.hidden = !options.toolbarSet.includes(elt.id);
      }
    }
  },

  /**
   * Options for the current view.
   *
   * @see ContentTree.viewOptions for supported options and default values.
   */
  get currentViewOptions() {
    // Use ContentTree options as default.
    let viewOptions = ContentTree.viewOptions;
    if (this._specialViews.has(this.currentPlace)) {
      let { options } = this._specialViews.get(this.currentPlace);
      for (let option in options) {
        viewOptions[option] = options[option];
      }
    }
    return viewOptions;
  },

  focus() {
    this.currentView.associatedElement.focus();
  },
};

var ContentTree = {
  init: function CT_init() {
    this._view = document.getElementById("placeContent");
  },

  get view() {
    return this._view;
  },

  get viewOptions() {
    return Object.seal({
      showDetailsPane: true,
      toolbarSet:
        "back-button, forward-button, organizeButton, viewMenu, maintenanceButton, libraryToolbarSpacer, searchFilter",
    });
  },

  openSelectedNode: function CT_openSelectedNode(aEvent) {
    let view = this.view;
    PlacesUIUtils.openNodeWithEvent(view.selectedNode, aEvent);
  },

  onClick: function CT_onClick(aEvent) {
    let node = this.view.selectedNode;
    if (node) {
      let doubleClick = aEvent.button == 0 && aEvent.detail == 2;
      let middleClick = aEvent.button == 1 && aEvent.detail == 1;
      if (PlacesUtils.nodeIsURI(node) && (doubleClick || middleClick)) {
        // Open associated uri in the browser.
        this.openSelectedNode(aEvent);
      } else if (middleClick && PlacesUtils.nodeIsContainer(node)) {
        // The command execution function will take care of seeing if the
        // selection is a folder or a different container type, and will
        // load its contents in tabs.
        PlacesUIUtils.openMultipleLinksInTabs(node, aEvent, this.view);
      }
    }
  },

  onKeyPress: function CT_onKeyPress(aEvent) {
    if (aEvent.keyCode == KeyEvent.DOM_VK_RETURN) {
      this.openSelectedNode(aEvent);
    }
  },
};
