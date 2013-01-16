/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource:///modules/MigrationUtils.jsm");

var PlacesOrganizer = {
  _places: null,

  // IDs of fields from editBookmarkOverlay that should be hidden when infoBox
  // is minimal. IDs should be kept in sync with the IDs of the elements
  // observing additionalInfoBroadcaster.
  _additionalInfoFields: [
    "editBMPanel_descriptionRow",
    "editBMPanel_loadInSidebarCheckbox",
    "editBMPanel_keywordRow",
  ],

  _initFolderTree: function() {
    var leftPaneRoot = PlacesUIUtils.leftPaneFolderId;
    this._places.place = "place:excludeItems=1&expandQueries=0&folder=" + leftPaneRoot;
  },

  selectLeftPaneQuery: function PO_selectLeftPaneQuery(aQueryName) {
    var itemId = PlacesUIUtils.leftPaneQueries[aQueryName];
    this._places.selectItems([itemId]);
    // Forcefully expand all-bookmarks
    if (aQueryName == "AllBookmarks")
      PlacesUtils.asContainer(this._places.selectedNode).containerOpen = true;
  },

  init: function PO_init() {
    ContentArea.init();

    this._places = document.getElementById("placesList");
    this._initFolderTree();

    var leftPaneSelection = "AllBookmarks"; // default to all-bookmarks
    if (window.arguments && window.arguments[0])
      leftPaneSelection = window.arguments[0];

    this.selectLeftPaneQuery(leftPaneSelection);
    // clear the back-stack
    this._backHistory.splice(0, this._backHistory.length);
    document.getElementById("OrganizerCommand:Back").setAttribute("disabled", true);

    // Set up the search UI.
    PlacesSearchBox.init();

    window.addEventListener("AppCommand", this, true);
#ifdef XP_MACOSX
    // 1. Map Edit->Find command to OrganizerCommand_find:all.  Need to map
    // both the menuitem and the Find key.
    var findMenuItem = document.getElementById("menu_find");
    findMenuItem.setAttribute("command", "OrganizerCommand_find:all");
    var findKey = document.getElementById("key_find");
    findKey.setAttribute("command", "OrganizerCommand_find:all");

    // 2. Disable some keybindings from browser.xul
    var elements = ["cmd_handleBackspace", "cmd_handleShiftBackspace"];
    for (var i=0; i < elements.length; i++) {
      document.getElementById(elements[i]).setAttribute("disabled", "true");
    }
    
    // 3. Disable the keyboard shortcut for the History menu back/forward
    // in order to support those in the Library
    var historyMenuBack = document.getElementById("historyMenuBack");
    historyMenuBack.removeAttribute("key");
    var historyMenuForward = document.getElementById("historyMenuForward");
    historyMenuForward.removeAttribute("key");
#endif

    // remove the "Properties" context-menu item, we've our own details pane
    document.getElementById("placesContext")
            .removeChild(document.getElementById("placesContext_show:info"));

#ifndef MOZ_PER_WINDOW_PRIVATE_BROWSING
    gPrivateBrowsingListener.init();
#endif

    ContentArea.focus();
  },

  QueryInterface: function PO_QueryInterface(aIID) {
    if (aIID.equals(Components.interfaces.nsIDOMEventListener) ||
        aIID.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_NOINTERFACE;
  },

  handleEvent: function PO_handleEvent(aEvent) {
    if (aEvent.type != "AppCommand")
      return;

    aEvent.stopPropagation();
    switch (aEvent.command) {
      case "Back":
        if (this._backHistory.length > 0)
          this.back();
        break;
      case "Forward":
        if (this._forwardHistory.length > 0)
          this.forward();
        break;
      case "Search":
        PlacesSearchBox.findAll();
        break;
    }
  },

  destroy: function PO_destroy() {
#ifndef MOZ_PER_WINDOW_PRIVATE_BROWSING
    gPrivateBrowsingListener.uninit();
#endif
  },

  _location: null,
  get location() {
    return this._location;
  },

  set location(aLocation) {
    if (!aLocation || this._location == aLocation)
      return aLocation;

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
    if (this._backHistory.length == 0)
      document.getElementById("OrganizerCommand:Back").setAttribute("disabled", true);
    else
      document.getElementById("OrganizerCommand:Back").removeAttribute("disabled");
    if (this._forwardHistory.length == 0)
      document.getElementById("OrganizerCommand:Forward").setAttribute("disabled", true);
    else
      document.getElementById("OrganizerCommand:Forward").removeAttribute("disabled");

    return aLocation;
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
    if (!this._places.hasSelection)
      return;

    var node = this._places.selectedNode;
    var queries = PlacesUtils.asQuery(node).getQueries();

    // Items are only excluded on the left pane.
    var options = node.queryOptions.clone();
    options.excludeItems = false;
    var placeURI = PlacesUtils.history.queriesToQueryString(queries,
                                                            queries.length,
                                                            options);

    // If either the place of the content tree in the right pane has changed or
    // the user cleared the search box, update the place, hide the search UI,
    // and update the back/forward buttons by setting location.
    if (ContentArea.currentPlace != placeURI || !resetSearchBox) {
      ContentArea.currentPlace = placeURI;
      this.location = node.uri;
    }

    // When we invalidate a container we use suppressSelectionEvent, when it is
    // unset a select event is fired, in many cases the selection did not really
    // change, so we should check for it, and return early in such a case. Note
    // that we cannot return any earlier than this point, because when
    // !resetSearchBox, we need to update location and hide the UI as above,
    // even though the selection has not changed.
    if (node.uri == this._cachedLeftPaneSelectedURI)
      return;
    this._cachedLeftPaneSelectedURI = node.uri;

    // At this point, resetSearchBox is true, because the left pane selection
    // has changed; otherwise we would have returned earlier.

    PlacesSearchBox.searchFilter.reset();
    this._setSearchScopeForNode(node);
    this.updateDetailsPane();
  },

  /**
   * Sets the search scope based on aNode's properties.
   * @param   aNode
   *          the node to set up scope from
   */
  _setSearchScopeForNode: function PO__setScopeForNode(aNode) {
    let itemId = aNode.itemId;

    if (PlacesUtils.nodeIsHistoryContainer(aNode) ||
        itemId == PlacesUIUtils.leftPaneQueries["History"]) {
      PlacesQueryBuilder.setScope("history");
    }
    else if (itemId == PlacesUIUtils.leftPaneQueries["Downloads"]) {
      PlacesQueryBuilder.setScope("downloads");
    }
    else {
      // Default to All Bookmarks for all other nodes, per bug 469437.
      PlacesQueryBuilder.setScope("bookmarks");
    }
  },

  /**
   * Handle clicks on the places list.
   * Single Left click, right click or modified click do not result in any
   * special action, since they're related to selection.
   * @param   aEvent
   *          The mouse event.
   */
  onPlacesListClick: function PO_onPlacesListClick(aEvent) {
    // Only handle clicks on tree children.
    if (aEvent.target.localName != "treechildren")
      return;

    let node = this._places.selectedNode;
    if (node) {
      let middleClick = aEvent.button == 1 && aEvent.detail == 1;
      if (middleClick && PlacesUtils.nodeIsContainer(node)) {
        // The command execution function will take care of seeing if the
        // selection is a folder or a different container type, and will
        // load its contents in tabs.
        PlacesUIUtils.openContainerNodeInTabs(selectedNode, aEvent, this._places);
      }
    }
  },

  /**
   * Handle focus changes on the places list and the current content view.
   */
  updateDetailsPane: function PO_updateDetailsPane() {
    if (!ContentArea.currentViewOptions.showDetailsPane)
      return;
    let view = PlacesUIUtils.getViewForNode(document.activeElement);
    if (view) {
      let selectedNodes = view.selectedNode ?
                          [view.selectedNode] : view.selectedNodes;
      this._fillDetailsPane(selectedNodes);
    }
  },

  openFlatContainer: function PO_openFlatContainerFlatContainer(aContainer) {
    if (aContainer.itemId != -1)
      this._places.selectItems([aContainer.itemId]);
    else if (PlacesUtils.nodeIsQuery(aContainer))
      this._places.selectPlaceURI(aContainer.uri);
  },

  /**
   * Returns the options associated with the query currently loaded in the
   * main places pane.
   */
  getCurrentOptions: function PO_getCurrentOptions() {
    return PlacesUtils.asQuery(ContentArea.currentView.result.root).queryOptions;
  },

  /**
   * Returns the queries associated with the query currently loaded in the
   * main places pane.
   */
  getCurrentQueries: function PO_getCurrentQueries() {
    return PlacesUtils.asQuery(ContentArea.currentView.result.root).getQueries();
  },

  /**
   * Show the migration wizard for importing passwords,
   * cookies, history, preferences, and bookmarks.
   */
  importFromBrowser: function PO_importFromBrowser() {
    MigrationUtils.showMigrationWizard(window);
  },

  /**
   * Open a file-picker and import the selected file into the bookmarks store
   */
  importFromFile: function PO_importFromFile() {
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    let fpCallback = function fpCallback_done(aResult) {
      if (aResult != Ci.nsIFilePicker.returnCancel && fp.fileURL) {
        Components.utils.import("resource://gre/modules/BookmarkHTMLUtils.jsm");
        BookmarkHTMLUtils.importFromURL(fp.fileURL.spec, false)
                         .then(null, Components.utils.reportError);
      }
    };

    fp.init(window, PlacesUIUtils.getString("SelectImport"),
            Ci.nsIFilePicker.modeOpen);
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
        Components.utils.import("resource://gre/modules/BookmarkHTMLUtils.jsm");
        BookmarkHTMLUtils.exportToFile(fp.file)
                         .then(null, Components.utils.reportError);
      }
    };

    fp.init(window, PlacesUIUtils.getString("EnterExport"),
            Ci.nsIFilePicker.modeSave);
    fp.appendFilters(Ci.nsIFilePicker.filterHTML);
    fp.defaultString = "bookmarks.html";
    fp.open(fpCallback);
  },

  /**
   * Populates the restore menu with the dates of the backups available.
   */
  populateRestoreMenu: function PO_populateRestoreMenu() {
    let restorePopup = document.getElementById("fileRestorePopup");

    let dateSvc = Cc["@mozilla.org/intl/scriptabledateformat;1"].
                  getService(Ci.nsIScriptableDateFormat);

    // Remove existing menu items.  Last item is the restoreFromFile item.
    while (restorePopup.childNodes.length > 1)
      restorePopup.removeChild(restorePopup.firstChild);

    let backupFiles = PlacesUtils.backups.entries;
    if (backupFiles.length == 0)
      return;

    // Populate menu with backups.
    for (let i = 0; i < backupFiles.length; i++) {
      let backupDate = PlacesUtils.backups.getDateForFile(backupFiles[i]);
      let m = restorePopup.insertBefore(document.createElement("menuitem"),
                                        document.getElementById("restoreFromFile"));
      m.setAttribute("label",
                     dateSvc.FormatDate("",
                                        Ci.nsIScriptableDateFormat.dateFormatLong,
                                        backupDate.getFullYear(),
                                        backupDate.getMonth() + 1,
                                        backupDate.getDate()));
      m.setAttribute("value", backupFiles[i].leafName);
      m.setAttribute("oncommand",
                     "PlacesOrganizer.onRestoreMenuItemClick(this);");
    }

    // Add the restoreFromFile item.
    restorePopup.insertBefore(document.createElement("menuseparator"),
                              document.getElementById("restoreFromFile"));
  },

  /**
   * Called when a menuitem is selected from the restore menu.
   */
  onRestoreMenuItemClick: function PO_onRestoreMenuItemClick(aMenuItem) {
    let backupName = aMenuItem.getAttribute("value");
    let backupFiles = PlacesUtils.backups.entries;
    for (let i = 0; i < backupFiles.length; i++) {
      if (backupFiles[i].leafName == backupName) {
        this.restoreBookmarksFromFile(backupFiles[i]);
        break;
      }
    }
  },

  /**
   * Called when 'Choose File...' is selected from the restore menu.
   * Prompts for a file and restores bookmarks to those in the file.
   */
  onRestoreBookmarksFromFile: function PO_onRestoreBookmarksFromFile() {
    let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
                 getService(Ci.nsIProperties);
    let backupsDir = dirSvc.get("Desk", Ci.nsILocalFile);
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    let fpCallback = function fpCallback_done(aResult) {
      if (aResult != Ci.nsIFilePicker.returnCancel) {
        this.restoreBookmarksFromFile(fp.file);
      }
    }.bind(this);

    fp.init(window, PlacesUIUtils.getString("bookmarksRestoreTitle"),
            Ci.nsIFilePicker.modeOpen);
    fp.appendFilter(PlacesUIUtils.getString("bookmarksRestoreFilterName"),
                    PlacesUIUtils.getString("bookmarksRestoreFilterExtension"));
    fp.appendFilters(Ci.nsIFilePicker.filterAll);
    fp.displayDirectory = backupsDir;
    fp.open(fpCallback);
  },

  /**
   * Restores bookmarks from a JSON file.
   */
  restoreBookmarksFromFile: function PO_restoreBookmarksFromFile(aFile) {
    // check file extension
    if (!aFile.leafName.match(/\.json$/)) {
      this._showErrorAlert(PlacesUIUtils.getString("bookmarksRestoreFormatError"));
      return;
    }

    // confirm ok to delete existing bookmarks
    var prompts = Cc["@mozilla.org/embedcomp/prompt-service;1"].
                  getService(Ci.nsIPromptService);
    if (!prompts.confirm(null,
                         PlacesUIUtils.getString("bookmarksRestoreAlertTitle"),
                         PlacesUIUtils.getString("bookmarksRestoreAlert")))
      return;

    try {
      PlacesUtils.restoreBookmarksFromJSONFile(aFile);
    }
    catch(ex) {
      this._showErrorAlert(PlacesUIUtils.getString("bookmarksRestoreParseError"));
    }
  },

  _showErrorAlert: function PO__showErrorAlert(aMsg) {
    var brandShortName = document.getElementById("brandStrings").
                                  getString("brandShortName");

    Cc["@mozilla.org/embedcomp/prompt-service;1"].
      getService(Ci.nsIPromptService).
      alert(window, brandShortName, aMsg);
  },

  /**
   * Backup bookmarks to desktop, auto-generate a filename with a date.
   * The file is a JSON serialization of bookmarks, tags and any annotations
   * of those items.
   */
  backupBookmarks: function PO_backupBookmarks() {
    let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
                 getService(Ci.nsIProperties);
    let backupsDir = dirSvc.get("Desk", Ci.nsILocalFile);
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    let fpCallback = function fpCallback_done(aResult) {
      if (aResult != Ci.nsIFilePicker.returnCancel) {
        PlacesUtils.backups.saveBookmarksToJSONFile(fp.file);
      }
    };

    fp.init(window, PlacesUIUtils.getString("bookmarksBackupTitle"),
            Ci.nsIFilePicker.modeSave);
    fp.appendFilter(PlacesUIUtils.getString("bookmarksRestoreFilterName"),
                    PlacesUIUtils.getString("bookmarksRestoreFilterExtension"));
    fp.defaultString = PlacesUtils.backups.getFilenameForDate();
    fp.displayDirectory = backupsDir;
    fp.open(fpCallback);
  },

  _paneDisabled: false,
  _setDetailsFieldsDisabledState:
  function PO__setDetailsFieldsDisabledState(aDisabled) {
    if (aDisabled) {
      document.getElementById("paneElementsBroadcaster")
              .setAttribute("disabled", "true");
    }
    else {
      document.getElementById("paneElementsBroadcaster")
              .removeAttribute("disabled");
    }
  },

  _detectAndSetDetailsPaneMinimalState:
  function PO__detectAndSetDetailsPaneMinimalState(aNode) {
    /**
     * The details of simple folder-items (as opposed to livemarks) or the
     * of livemark-children are not likely to fill the infoBox anyway,
     * thus we remove the "More/Less" button and show all details.
     *
     * the wasminimal attribute here is used to persist the "more/less"
     * state in a bookmark->folder->bookmark scenario.
     */
    var infoBox = document.getElementById("infoBox");
    var infoBoxExpander = document.getElementById("infoBoxExpander");
    var infoBoxExpanderWrapper = document.getElementById("infoBoxExpanderWrapper");
    var additionalInfoBroadcaster = document.getElementById("additionalInfoBroadcaster");

    if (!aNode) {
      infoBoxExpanderWrapper.hidden = true;
      return;
    }
    if (aNode.itemId != -1 &&
        PlacesUtils.nodeIsFolder(aNode) && !aNode._feedURI) {
      if (infoBox.getAttribute("minimal") == "true")
        infoBox.setAttribute("wasminimal", "true");
      infoBox.removeAttribute("minimal");
      infoBoxExpanderWrapper.hidden = true;
    }
    else {
      if (infoBox.getAttribute("wasminimal") == "true")
        infoBox.setAttribute("minimal", "true");
      infoBox.removeAttribute("wasminimal");
      infoBoxExpanderWrapper.hidden =
        this._additionalInfoFields.every(function (id)
          document.getElementById(id).collapsed);
    }
    additionalInfoBroadcaster.hidden = infoBox.getAttribute("minimal") == "true";
  },

  // NOT YET USED
  updateThumbnailProportions: function PO_updateThumbnailProportions() {
    var previewBox = document.getElementById("previewBox");
    var canvas = document.getElementById("itemThumbnail");
    var height = previewBox.boxObject.height;
    var width = height * (screen.width / screen.height);
    canvas.width = width;
    canvas.height = height;
  },

  _fillDetailsPane: function PO__fillDetailsPane(aNodeList) {
    var infoBox = document.getElementById("infoBox");
    var detailsDeck = document.getElementById("detailsDeck");

    // Make sure the infoBox UI is visible if we need to use it, we hide it
    // below when we don't.
    infoBox.hidden = false;
    var aSelectedNode = aNodeList.length == 1 ? aNodeList[0] : null;
    // If a textbox within a panel is focused, force-blur it so its contents
    // are saved
    if (gEditItemOverlay.itemId != -1) {
      var focusedElement = document.commandDispatcher.focusedElement;
      if ((focusedElement instanceof HTMLInputElement ||
           focusedElement instanceof HTMLTextAreaElement) &&
          /^editBMPanel.*/.test(focusedElement.parentNode.parentNode.id))
        focusedElement.blur();

      // don't update the panel if we are already editing this node unless we're
      // in multi-edit mode
      if (aSelectedNode) {
        var concreteId = PlacesUtils.getConcreteItemId(aSelectedNode);
        var nodeIsSame = gEditItemOverlay.itemId == aSelectedNode.itemId ||
                         gEditItemOverlay.itemId == concreteId ||
                         (aSelectedNode.itemId == -1 && gEditItemOverlay.uri &&
                          gEditItemOverlay.uri == aSelectedNode.uri);
        if (nodeIsSame && detailsDeck.selectedIndex == 1 &&
            !gEditItemOverlay.multiEdit)
          return;
      }
    }

    // Clean up the panel before initing it again.
    gEditItemOverlay.uninitPanel(false);

    if (aSelectedNode && !PlacesUtils.nodeIsSeparator(aSelectedNode)) {
      detailsDeck.selectedIndex = 1;
      // Using the concrete itemId is arguably wrong.  The bookmarks API
      // does allow setting properties for folder shortcuts as well, but since
      // the UI does not distinct between the couple, we better just show
      // the concrete item properties for shortcuts to root nodes.
      var concreteId = PlacesUtils.getConcreteItemId(aSelectedNode);
      var isRootItem = concreteId != -1 && PlacesUtils.isRootItem(concreteId);
      var readOnly = isRootItem ||
                     aSelectedNode.parent.itemId == PlacesUIUtils.leftPaneFolderId;
      var useConcreteId = isRootItem ||
                          PlacesUtils.nodeIsTagQuery(aSelectedNode);
      var itemId = -1;
      if (concreteId != -1 && useConcreteId)
        itemId = concreteId;
      else if (aSelectedNode.itemId != -1)
        itemId = aSelectedNode.itemId;
      else
        itemId = PlacesUtils._uri(aSelectedNode.uri);

      gEditItemOverlay.initPanel(itemId, { hiddenRows: ["folderPicker"]
                                         , forceReadOnly: readOnly
                                         , titleOverride: aSelectedNode.title
                                         });

      // Dynamically generated queries, like history date containers, have
      // itemId !=0 and do not exist in history.  For them the panel is
      // read-only, but empty, since it can't get a valid title for the object.
      // In such a case we force the title using the selectedNode one, for UI
      // polishness.
      if (aSelectedNode.itemId == -1 &&
          (PlacesUtils.nodeIsDay(aSelectedNode) ||
           PlacesUtils.nodeIsHost(aSelectedNode)))
        gEditItemOverlay._element("namePicker").value = aSelectedNode.title;

      this._detectAndSetDetailsPaneMinimalState(aSelectedNode);
    }
    else if (!aSelectedNode && aNodeList[0]) {
      var itemIds = [];
      for (var i = 0; i < aNodeList.length; i++) {
        if (!PlacesUtils.nodeIsBookmark(aNodeList[i]) &&
            !PlacesUtils.nodeIsURI(aNodeList[i])) {
          detailsDeck.selectedIndex = 0;
          var selectItemDesc = document.getElementById("selectItemDescription");
          var itemsCountLabel = document.getElementById("itemsCountText");
          selectItemDesc.hidden = false;
          itemsCountLabel.value =
            PlacesUIUtils.getPluralString("detailsPane.itemsCountLabel",
                                          aNodeList.length, [aNodeList.length]);
          infoBox.hidden = true;
          return;
        }
        itemIds[i] = aNodeList[i].itemId != -1 ? aNodeList[i].itemId :
                     PlacesUtils._uri(aNodeList[i].uri);
      }
      detailsDeck.selectedIndex = 1;
      gEditItemOverlay.initPanel(itemIds,
                                 { hiddenRows: ["folderPicker",
                                                "loadInSidebar",
                                                "location",
                                                "keyword",
                                                "description",
                                                "name"]});
      this._detectAndSetDetailsPaneMinimalState(aSelectedNode);
    }
    else {
      detailsDeck.selectedIndex = 0;
      infoBox.hidden = true;
      let selectItemDesc = document.getElementById("selectItemDescription");
      let itemsCountLabel = document.getElementById("itemsCountText");
      let itemsCount = 0;
      if (ContentArea.currentView.result) {
        let rootNode = ContentArea.currentView.result.root;
        if (rootNode.containerOpen)
          itemsCount = rootNode.childCount;
      }
      if (itemsCount == 0) {
        selectItemDesc.hidden = true;
        itemsCountLabel.value = PlacesUIUtils.getString("detailsPane.noItems");
      }
      else {
        selectItemDesc.hidden = false;
        itemsCountLabel.value =
          PlacesUIUtils.getPluralString("detailsPane.itemsCountLabel",
                                        itemsCount, [itemsCount]);
      }
    }
  },

  // NOT YET USED
  _updateThumbnail: function PO__updateThumbnail() {
    var bo = document.getElementById("previewBox").boxObject;
    var width  = bo.width;
    var height = bo.height;

    var canvas = document.getElementById("itemThumbnail");
    var ctx = canvas.getContext('2d');
    var notAvailableText = canvas.getAttribute("notavailabletext");
    ctx.save();
    ctx.fillStyle = "-moz-Dialog";
    ctx.fillRect(0, 0, width, height);
    ctx.translate(width/2, height/2);

    ctx.fillStyle = "GrayText";
    ctx.mozTextStyle = "12pt sans serif";
    var len = ctx.mozMeasureText(notAvailableText);
    ctx.translate(-len/2,0);
    ctx.mozDrawText(notAvailableText);
    ctx.restore();
  },

  toggleAdditionalInfoFields: function PO_toggleAdditionalInfoFields() {
    var infoBox = document.getElementById("infoBox");
    var infoBoxExpander = document.getElementById("infoBoxExpander");
    var infoBoxExpanderLabel = document.getElementById("infoBoxExpanderLabel");
    var additionalInfoBroadcaster = document.getElementById("additionalInfoBroadcaster");

    if (infoBox.getAttribute("minimal") == "true") {
      infoBox.removeAttribute("minimal");
      infoBoxExpanderLabel.value = infoBoxExpanderLabel.getAttribute("lesslabel");
      infoBoxExpanderLabel.accessKey = infoBoxExpanderLabel.getAttribute("lessaccesskey");
      infoBoxExpander.className = "expander-up";
      additionalInfoBroadcaster.removeAttribute("hidden");
    }
    else {
      infoBox.setAttribute("minimal", "true");
      infoBoxExpanderLabel.value = infoBoxExpanderLabel.getAttribute("morelabel");
      infoBoxExpanderLabel.accessKey = infoBoxExpanderLabel.getAttribute("moreaccesskey");
      infoBoxExpander.className = "expander-down";
      additionalInfoBroadcaster.setAttribute("hidden", "true");
    }
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
    if (this._folders.length == 0) {
      this._folders.push(PlacesUtils.bookmarksMenuFolderId,
                         PlacesUtils.unfiledBookmarksFolderId,
                         PlacesUtils.toolbarFolderId);
    }
    return this._folders;
  },
  set folders(aFolders) {
    this._folders = aFolders;
    return aFolders;
  },

  /**
   * Run a search for the specified text, over the collection specified by
   * the dropdown arrow. The default is all bookmarks, but can be
   * localized to the active collection.
   * @param   filterString
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
    let currentOptions = PO.getCurrentOptions();

    // Search according to the current scope, which was set by
    // PQB_setScope()
    switch (PlacesSearchBox.filterCollection) {
      case "bookmarks":
        currentView.applyFilter(filterString, this.folders);
        break;
      case "history":
        if (currentOptions.queryType != Ci.nsINavHistoryQueryOptions.QUERY_TYPE_HISTORY) {
          var query = PlacesUtils.history.getNewQuery();
          query.searchTerms = filterString;
          var options = currentOptions.clone();
          // Make sure we're getting uri results.
          options.resultType = currentOptions.RESULT_TYPE_URI;
          options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_HISTORY;
          options.includeHidden = true;
          currentView.load([query], options);
        }
        else {
          currentView.applyFilter(filterString, null, true);
        }
        break;
      case "downloads":
        if (currentView == ContentTree.view) {
          let query = PlacesUtils.history.getNewQuery();
          query.searchTerms = filterString;
          query.setTransitions([Ci.nsINavHistoryService.TRANSITION_DOWNLOAD], 1);
          let options = currentOptions.clone();
          // Make sure we're getting uri results.
          options.resultType = currentOptions.RESULT_TYPE_URI;
          options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_HISTORY;
          options.includeHidden = true;
          currentView.load([query], options);
        }
        else {
          // The new downloads view doesn't use places for searching downloads.
          currentView.searchTerm = filterString;
        }
        break;
      default:
        throw "Invalid filterCollection on search";
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
   * @param   aTitle
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
    if (collectionName == this.filterCollection)
      return collectionName;

    this.searchFilter.setAttribute("collection", collectionName);
    this.updateCollectionTitle();

    return collectionName;
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
    return this.searchFilter.value = value;
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
   * @param   aScope
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
        folders.push(PlacesUtils.bookmarksMenuFolderId,
                     PlacesUtils.toolbarFolderId,
                     PlacesUtils.unfiledBookmarksFolderId);
        break;
      case "downloads":
        filterCollection = "downloads";
        break;
      default:
        throw "Invalid search scope";
        break;
    }

    // Update the search box.  Re-search if there's an active search.
    PlacesSearchBox.filterCollection = filterCollection;
    PlacesSearchBox.folders = folders;
    var searchStr = PlacesSearchBox.searchFilter.value;
    if (searchStr)
      PlacesSearchBox.search(searchStr);
  }
};

/**
 * Population and commands for the View Menu.
 */
var ViewMenu = {
  /**
   * Removes content generated previously from a menupopup.
   * @param   popup
   *          The popup that contains the previously generated content.
   * @param   startID
   *          The id attribute of an element that is the start of the
   *          dynamically generated region - remove elements after this
   *          item only.
   *          Must be contained by popup. Can be null (in which case the
   *          contents of popup are removed).
   * @param   endID
   *          The id attribute of an element that is the end of the
   *          dynamically generated region - remove elements up to this
   *          item only.
   *          Must be contained by popup. Can be null (in which case all
   *          items until the end of the popup will be removed). Ignored
   *          if startID is null.
   * @returns The element for the caller to insert new items before,
   *          null if the caller should just append to the popup.
   */
  _clean: function VM__clean(popup, startID, endID) {
    if (endID)
      NS_ASSERT(startID, "meaningless to have valid endID and null startID");
    if (startID) {
      var startElement = document.getElementById(startID);
      NS_ASSERT(startElement.parentNode ==
                popup, "startElement is not in popup");
      NS_ASSERT(startElement,
                "startID does not correspond to an existing element");
      var endElement = null;
      if (endID) {
        endElement = document.getElementById(endID);
        NS_ASSERT(endElement.parentNode == popup,
                  "endElement is not in popup");
        NS_ASSERT(endElement,
                  "endID does not correspond to an existing element");
      }
      while (startElement.nextSibling != endElement)
        popup.removeChild(startElement.nextSibling);
      return endElement;
    }
    else {
      while(popup.hasChildNodes())
        popup.removeChild(popup.firstChild);
    }
    return null;
  },

  /**
   * Fills a menupopup with a list of columns
   * @param   event
   *          The popupshowing event that invoked this function.
   * @param   startID
   *          see _clean
   * @param   endID
   *          see _clean
   * @param   type
   *          the type of the menuitem, e.g. "radio" or "checkbox".
   *          Can be null (no-type).
   *          Checkboxes are checked if the column is visible.
   * @param   propertyPrefix
   *          If propertyPrefix is non-null:
   *          propertyPrefix + column ID + ".label" will be used to get the
   *          localized label string.
   *          propertyPrefix + column ID + ".accesskey" will be used to get the
   *          localized accesskey.
   *          If propertyPrefix is null, the column label is used as label and
   *          no accesskey is assigned.
   */
  fillWithColumns: function VM_fillWithColumns(event, startID, endID, type, propertyPrefix) {
    var popup = event.target;
    var pivot = this._clean(popup, startID, endID);

    // If no column is "sort-active", the "Unsorted" item needs to be checked,
    // so track whether or not we find a column that is sort-active.
    var isSorted = false;
    var content = document.getElementById("placeContent");
    var columns = content.columns;
    for (var i = 0; i < columns.count; ++i) {
      var column = columns.getColumnAt(i).element;
      var menuitem = document.createElement("menuitem");
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
          isSorted = true;
        }
      }
      else if (type == "checkbox") {
        menuitem.setAttribute("type", "checkbox");
        // Cannot uncheck the primary column.
        if (column.getAttribute("primary") == "true")
          menuitem.setAttribute("disabled", "true");
        // Items for visible columns are checked.
        if (!column.hidden)
          menuitem.setAttribute("checked", "true");
      }
      if (pivot)
        popup.insertBefore(menuitem, pivot);
      else
        popup.appendChild(menuitem);
    }
    event.stopPropagation();
  },

  /**
   * Set up the content of the view menu.
   */
  populateSortMenu: function VM_populateSortMenu(event) {
    this.fillWithColumns(event, "viewUnsorted", "directionSeparator", "radio", "view.sortBy.");

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
    }
    else if (sortColumn.getAttribute("sortDirection") == "ascending") {
      viewSortAscending.setAttribute("checked", "true");
      viewSortDescending.removeAttribute("checked");
      viewUnsorted.removeAttribute("checked");
    }
    else if (sortColumn.getAttribute("sortDirection") == "descending") {
      viewSortDescending.setAttribute("checked", "true");
      viewSortAscending.removeAttribute("checked");
      viewUnsorted.removeAttribute("checked");
    }
  },

  /**
   * Shows/Hides a tree column.
   * @param   element
   *          The menuitem element for the column
   */
  showHideColumn: function VM_showHideColumn(element) {
    var column = element.column;

    var splitter = column.nextSibling;
    if (splitter && splitter.localName != "splitter")
      splitter = null;

    if (element.getAttribute("checked") == "true") {
      column.setAttribute("hidden", "false");
      if (splitter)
        splitter.removeAttribute("hidden");
    }
    else {
      column.setAttribute("hidden", "true");
      if (splitter)
        splitter.setAttribute("hidden", "true");
    }
  },

  /**
   * Gets the last column that was sorted.
   * @returns  the currently sorted column, null if there is no sorted column.
   */
  _getSortColumn: function VM__getSortColumn() {
    var content = document.getElementById("placeContent");
    var cols = content.columns;
    for (var i = 0; i < cols.count; ++i) {
      var column = cols.getColumnAt(i).element;
      var sortDirection = column.getAttribute("sortDirection");
      if (sortDirection == "ascending" || sortDirection == "descending")
        return column;
    }
    return null;
  },

  /**
   * Sorts the view by the specified column.
   * @param   aColumn
   *          The colum that is the sort key. Can be null - the
   *          current sort column or the title column will be used.
   * @param   aDirection
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
        var sortColumn = this._getSortColumn();
        if (sortColumn)
          aDirection = sortColumn.getAttribute("sortDirection");
      }
    }
    else {
      var sortColumn = this._getSortColumn();
      columnId = sortColumn ? sortColumn.getAttribute("anonid") : "title";
    }

    // This maps the possible values of columnId (i.e., anonid's of treecols in
    // placeContent) to the default sortingMode and sortingAnnotation values for
    // each column.
    //   key:  Sort key in the name of one of the
    //         nsINavHistoryQueryOptions.SORT_BY_* constants
    //   dir:  Default sort direction to use if none has been specified
    //   anno: The annotation to sort by, if key is "ANNOTATION"
    var colLookupTable = {
      title:        { key: "TITLE",        dir: "ascending"  },
      tags:         { key: "TAGS",         dir: "ascending"  },
      url:          { key: "URI",          dir: "ascending"  },
      date:         { key: "DATE",         dir: "descending" },
      visitCount:   { key: "VISITCOUNT",   dir: "descending" },
      keyword:      { key: "KEYWORD",      dir: "ascending"  },
      dateAdded:    { key: "DATEADDED",    dir: "descending" },
      lastModified: { key: "LASTMODIFIED", dir: "descending" },
      description:  { key: "ANNOTATION",
                      dir: "ascending",
                      anno: PlacesUIUtils.DESCRIPTION_ANNO }
    };

    // Make sure we have a valid column.
    if (!colLookupTable.hasOwnProperty(columnId))
      throw("Invalid column");

    // Use a default sort direction if none has been specified.  If aDirection
    // is invalid, result.sortingMode will be undefined, which has the effect
    // of unsorting the tree.
    aDirection = (aDirection || colLookupTable[columnId].dir).toUpperCase();

    var sortConst = "SORT_BY_" + colLookupTable[columnId].key + "_" + aDirection;
    result.sortingAnnotation = colLookupTable[columnId].anno || "";
    result.sortingMode = Ci.nsINavHistoryQueryOptions[sortConst];
  }
}

#ifndef MOZ_PER_WINDOW_PRIVATE_BROWSING
/**
 * Disables the "Import and Backup->Import From Another Browser" menu item
 * in private browsing mode.
 */
let gPrivateBrowsingListener = {
  _cmd_import: null,

  init: function PO_PB_init() {
    this._cmd_import = document.getElementById("OrganizerCommand_browserImport");

    let pbs = Cc["@mozilla.org/privatebrowsing;1"].
              getService(Ci.nsIPrivateBrowsingService);
    if (pbs.privateBrowsingEnabled)
      this.updateUI(true);

    Services.obs.addObserver(this, "private-browsing", false);
  },

  uninit: function PO_PB_uninit() {
    Services.obs.removeObserver(this, "private-browsing");
  },

  observe: function PO_PB_observe(aSubject, aTopic, aData) {
    if (aData == "enter")
      this.updateUI(true);
    else if (aData == "exit")
      this.updateUI(false);
  },

  updateUI: function PO_PB_updateUI(PBmode) {
    if (PBmode)
      this._cmd_import.setAttribute("disabled", "true");
    else
      this._cmd_import.removeAttribute("disabled");
  }
};
#endif

let ContentArea = {
  _specialViews: new Map(),

  init: function CA_init() {
    this._deck = document.getElementById("placesViewsDeck");
    this._toolbar = document.getElementById("placesToolbar");
    ContentTree.init();
    this._setupView();
  },

  /**
   * Gets the content view to be used for loading the given query.
   * If a custom view was set by setContentViewForQueryString, that
   * view would be returned, else the default tree view is returned
   *
   * @param aQueryString
   *        a query string
   * @return the view to be used for loading aQueryString.
   */
  getContentViewForQueryString:
  function CA_getContentViewForQueryString(aQueryString) {
    try {
      if (this._specialViews.has(aQueryString)) {
        let { view, options } = this._specialViews.get(aQueryString);
        if (typeof view == "function") {
          view = view();
          this._specialViews.set(aQueryString, { view: view, options: options });
        }
        return view;
      }
    }
    catch(ex) {
      Components.utils.reportError(ex);
    }
    return ContentTree.view;
  },

  /**
   * Sets a custom view to be used rather than the default places tree
   * whenever the given query is selected in the left pane.
   * @param aQueryString
   *        a query string
   * @param aView
   *        Either the custom view or a function that will return the view
   *        the first (and only) time it's called.
   * @param [optional] aOptions
   *        Object defining special options for the view.
   * @see ContentTree.viewOptions for supported options and default values.
   */
  setContentViewForQueryString:
  function CA_setContentViewForQueryString(aQueryString, aView, aOptions) {
    if (!aQueryString ||
        typeof aView != "object" && typeof aView != "function")
      throw new Error("Invalid arguments");

    this._specialViews.set(aQueryString, { view: aView,
                                           options: aOptions || new Object() });
  },

  get currentView() PlacesUIUtils.getViewForNode(this._deck.selectedPanel),
  set currentView(aView) {
    if (this.currentView != aView)
      this._deck.selectedPanel = aView.associatedElement;
    return aView;
  },

  get currentPlace() this.currentView.place,
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
    return aQueryString;
  },

  /**
   * Applies view options.
   */
  _setupView: function CA__setupView() {
    let options = this.currentViewOptions;

    // showDetailsPane.
    let detailsDeck = document.getElementById("detailsDeck");
    detailsDeck.hidden = !options.showDetailsPane;

    // toolbarSet.
    for (let elt of this._toolbar.childNodes) {
      // On Windows and Linux the menu buttons are menus wrapped in a menubar.
      if (elt.id == "placesMenu") {
        for (let menuElt of elt.childNodes) {
          menuElt.hidden = options.toolbarSet.indexOf(menuElt.id) == -1;
        }
      }
      else {
        elt.hidden = options.toolbarSet.indexOf(elt.id) == -1;
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
      let { view, options } = this._specialViews.get(this.currentPlace);
      for (let option in options) {
        viewOptions[option] = options[option];
      }
    }
    return viewOptions;
  },

  focus: function() {
    this._deck.selectedPanel.focus();
  }
};

let ContentTree = {
  init: function CT_init() {
    this._view = document.getElementById("placeContent");
  },

  get view() this._view,

  get viewOptions() Object.seal({
    showDetailsPane: true,
    toolbarSet: "back-button, forward-button, organizeButton, viewMenu, maintenanceButton, libraryToolbarSpacer, searchFilter"
  }),

  openSelectedNode: function CT_openSelectedNode(aEvent) {
    let view = this.view;
    PlacesUIUtils.openNodeWithEvent(view.selectedNode, aEvent, view);
  },

  onClick: function CT_onClick(aEvent) {
    let node = this.view.selectedNode;
    if (node) {
      let doubleClick = aEvent.button == 0 && aEvent.detail == 2;
      let middleClick = aEvent.button == 1 && aEvent.detail == 1;
      if (PlacesUtils.nodeIsURI(node) && (doubleClick || middleClick)) {
        // Open associated uri in the browser.
        this.openSelectedNode(aEvent);
      }
      else if (middleClick && PlacesUtils.nodeIsContainer(node)) {
        // The command execution function will take care of seeing if the
        // selection is a folder or a different container type, and will
        // load its contents in tabs.
        PlacesUIUtils.openContainerNodeInTabs(node, aEvent, this.view);
      }
    }
  },

  onKeyPress: function CT_onKeyPress(aEvent) {
    if (aEvent.keyCode == KeyEvent.DOM_VK_RETURN)
      this.openSelectedNode(aEvent);
  }
};
