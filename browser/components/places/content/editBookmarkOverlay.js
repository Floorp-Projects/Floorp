/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

const LAST_USED_ANNO = "bookmarkPropertiesDialog/folderLastUsed";
const MAX_FOLDER_ITEM_IN_MENU_LIST = 5;

let gEditItemOverlay = {
  _observersAdded: false,
  _staticFoldersListBuilt: false,

  _paneInfo: null,
  _setPaneInfo(aInitInfo) {
    if (!aInitInfo)
      return this._paneInfo = null;

    if ("uris" in aInitInfo && "node" in aInitInfo)
      throw new Error("ambiguous pane info");
    if (!("uris" in aInitInfo) && !("node" in aInitInfo))
      throw new Error("Neither node nor uris set for pane info");

    let node = "node" in aInitInfo ? aInitInfo.node : null;

    // Since there's no true UI for folder shortcuts (they show up just as their target
    // folders), when the pane shows for them it's opened in read-only mode, showing the
    // properties of the target folder.
    let itemId = node ? node.itemId : -1;
    let itemGuid = PlacesUIUtils.useAsyncTransactions && node ?
                     PlacesUtils.getConcreteItemGuid(node) : null;
    let isItem = itemId != -1;
    let isFolderShortcut = isItem &&
      node.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT;
    let isURI = node && PlacesUtils.nodeIsURI(node);
    let uri = isURI ? NetUtil.newURI(node.uri) : null;
    let title = node ? node.title : null;
    let isBookmark = isItem && isURI;
    let bulkTagging = !node;
    let uris = bulkTagging ? aInitInfo.uris : null;
    let visibleRows = new Set();
    let isParentReadOnly = false;
    if (node && "parent" in node) {
      let parent = node.parent;
      if (parent) {
        isParentReadOnly = !PlacesUtils.nodeIsFolder(parent) ||
                            PlacesUIUtils.isContentsReadOnly(parent);
      }
    }

    return this._paneInfo = { itemId, itemGuid, isItem,
                              isURI, uri, title,
                              isBookmark, isFolderShortcut, isParentReadOnly,
                              bulkTagging, uris,
                              visibleRows };
  },

  get initialized() {
    return this._paneInfo != null;
  },

  // Backwards-compatibility getters
  get itemId() {
    if (!this.initialized || this._paneInfo.bulkTagging)
      return -1;
    return this._paneInfo.itemId;
  },

  get uri() {
    if (!this.initialized)
      return null;
    if (this._paneInfo.bulkTagging)
      return this._paneInfo.uris[0];
    return this._paneInfo.uri;
  },

  get multiEdit() {
    return this.initialized && this._paneInfo.bulkTagging;
  },

  // Check if the pane is initialized to show only read-only fields.
  get readOnly() {
    // Bug 1120314 - Folder shortcuts are read-only due to some quirky implementation
    // details (the most important being the "smart" semantics of node.title).
    return (!this.initialized ||
            (!this._paneInfo.visibleRows.has("tagsRow") &&
             (this._paneInfo.isFolderShortcut ||
              this._paneInfo.isParentReadOnly)));
  },

  // the first field which was edited after this panel was initialized for
  // a certain item
  _firstEditedField: "",

  _initNamePicker() {
    if (this._paneInfo.bulkTagging)
      throw new Error("_initNamePicker called unexpectedly");

    // title may by null, which, for us, is the same as an empty string.
    this._initTextField(this._namePicker, this._paneInfo.title || "");
  },

  _initLocationField() {
    if (!this._paneInfo.isURI)
      throw new Error("_initLocationField called unexpectedly");
    this._initTextField(this._locationField, this._paneInfo.uri.spec);
  },

  _initDescriptionField() {
    if (!this._paneInfo.isItem)
      throw new Error("_initDescriptionField called unexpectedly");

    this._initTextField(this._descriptionField,
                        PlacesUIUtils.getItemDescription(this._paneInfo.itemId));
  },

  _initKeywordField: Task.async(function* (aNewKeyword) {
    if (!this._paneInfo.isBookmark)
      throw new Error("_initKeywordField called unexpectedly");

    let newKeyword = aNewKeyword;
    if (newKeyword === undefined) {
      let itemId = this._paneInfo.itemId;
      newKeyword = PlacesUtils.bookmarks.getKeywordForBookmark(itemId);
    }
    this._initTextField(this._keywordField, newKeyword);
  }),

  _initLoadInSidebar: Task.async(function* () {
    if (!this._paneInfo.isBookmark)
      throw new Error("_initLoadInSidebar called unexpectedly");

    this._loadInSidebarCheckbox.checked =
      PlacesUtils.annotations.itemHasAnnotation(
        this._paneInfo.itemId, PlacesUIUtils.LOAD_IN_SIDEBAR_ANNO);
  }),

  /**
   * Initialize the panel.
   *
   * @param aInfo
   *        An object having:
   *        1. one of the following properties:
   *        - node: either a result node or a node-like object representing the
   *          item to be edited. A node-like object must have the following
   *          properties (with values that match exactly those a result node
   *          would have): itemId, bookmarkGuid, uri, title, type.
   *        - uris: an array of uris for bulk tagging.
   *
   *        2. any of the following optional properties:
   *          - hiddenRows (Strings array): list of rows to be hidden regardless
   *            of the item edited. Possible values: "title", "location",
   *            "description", "keyword", "loadInSidebar", "feedLocation",
   *            "siteLocation", folderPicker"
   */
  initPanel(aInfo) {
    if (typeof(aInfo) != "object" || aInfo === null)
      throw new Error("aInfo must be an object.");

    // For sanity ensure that the implementer has uninited the panel before
    // trying to init it again, or we could end up leaking due to observers.
    if (this.initialized)
      this.uninitPanel(false);

    let { itemId, itemGuid, isItem,
          isURI, uri, title,
          isBookmark, bulkTagging, uris,
          visibleRows } = this._setPaneInfo(aInfo);

    let showOrCollapse =
      (rowId, isAppropriateForInput, nameInHiddenRows = null) => {
        let visible = isAppropriateForInput;
        if (visible && "hiddenRows" in aInfo && nameInHiddenRows)
          visible &= aInfo.hiddenRows.indexOf(nameInHiddenRows) == -1;
        if (visible)
          visibleRows.add(rowId);
        return !(this._element(rowId).collapsed = !visible);
      };

    if (showOrCollapse("nameRow", !bulkTagging, "name")) {
      this._initNamePicker();
      this._namePicker.readOnly = this.readOnly;
    }

    if (showOrCollapse("locationRow", isURI, "location")) {
      this._initLocationField();
      this._locationField.readOnly = !this._paneInfo.isItem;
    }

    if (showOrCollapse("descriptionRow",
                       this._paneInfo.isItem && !this.readOnly,
                       "description")) {
      this._initDescriptionField();
    }

    if (showOrCollapse("keywordRow", isBookmark, "keyword"))
      this._initKeywordField();

    // Collapse the tag selector if the item does not accept tags.
    if (showOrCollapse("tagsRow", isURI || bulkTagging, "tags"))
      this._initTagsField().catch(Components.utils.reportError);
    else if (!this._element("tagsSelectorRow").collapsed)
      this.toggleTagsSelector().catch(Components.utils.reportError);

    // Load in sidebar.
    if (showOrCollapse("loadInSidebarCheckbox", isBookmark, "loadInSidebar")) {
      this._initLoadInSidebar();
    }

    // Folder picker.
    // Technically we should check that the item is not moveable, but that's
    // not cheap (we don't always have the parent), and there's no use case for
    // this (it's only the Star UI that shows the folderPicker)
    if (showOrCollapse("folderRow", isItem, "folderPicker")) {
      let containerId = PlacesUtils.bookmarks.getFolderIdForItem(itemId);
      this._initFolderMenuList(containerId);
    }

    // Selection count.
    if (showOrCollapse("selectionCount", bulkTagging)) {
      this._element("itemsCountText").value =
        PlacesUIUtils.getPluralString("detailsPane.itemsCountLabel",
                                      uris.length,
                                      [uris.length]);
    }

    // Observe changes.
    if (!this._observersAdded) {
      PlacesUtils.bookmarks.addObserver(this, false);
      window.addEventListener("unload", this, false);
      this._observersAdded = true;
    }
  },

  /**
   * Finds tags that are in common among this._currentInfo.uris;
   */
  _getCommonTags() {
    if ("_cachedCommonTags" in this._paneInfo)
      return this._paneInfo._cachedCommonTags;

    let uris = [...this._paneInfo.uris];
    let firstURI = uris.shift();
    let commonTags = new Set(PlacesUtils.tagging.getTagsForURI(firstURI));
    if (commonTags.size == 0)
      return this._cachedCommonTags = [];

    for (let uri of uris) {
      let curentURITags = PlacesUtils.tagging.getTagsForURI(uri);
      for (let tag of commonTags) {
        if (curentURITags.indexOf(tag) == -1) {
          commonTags.delete(tag)
          if (commonTags.size == 0)
            return this._paneInfo.cachedCommonTags = [];
        }
      }
    }
    return this._paneInfo._cachedCommonTags = [...commonTags];
  },

  _initTextField(aElement, aValue) {
    if (aElement.value != aValue) {
      aElement.value = aValue;

      // Clear the undo stack
      let editor = aElement.editor;
      if (editor)
        editor.transactionManager.clear();
    }
  },

  /**
   * Appends a menu-item representing a bookmarks folder to a menu-popup.
   * @param aMenupopup
   *        The popup to which the menu-item should be added.
   * @param aFolderId
   *        The identifier of the bookmarks folder.
   * @return the new menu item.
   */
  _appendFolderItemToMenupopup(aMenupopup, aFolderId) {
    // First make sure the folders-separator is visible
    this._element("foldersSeparator").hidden = false;

    var folderMenuItem = document.createElement("menuitem");
    var folderTitle = PlacesUtils.bookmarks.getItemTitle(aFolderId)
    folderMenuItem.folderId = aFolderId;
    folderMenuItem.setAttribute("label", folderTitle);
    folderMenuItem.className = "menuitem-iconic folder-icon";
    aMenupopup.appendChild(folderMenuItem);
    return folderMenuItem;
  },

  _initFolderMenuList: function EIO__initFolderMenuList(aSelectedFolder) {
    // clean up first
    var menupopup = this._folderMenuList.menupopup;
    while (menupopup.childNodes.length > 6)
      menupopup.removeChild(menupopup.lastChild);

    const bms = PlacesUtils.bookmarks;
    const annos = PlacesUtils.annotations;

    // Build the static list
    var unfiledItem = this._element("unfiledRootItem");
    if (!this._staticFoldersListBuilt) {
      unfiledItem.label = bms.getItemTitle(PlacesUtils.unfiledBookmarksFolderId);
      unfiledItem.folderId = PlacesUtils.unfiledBookmarksFolderId;
      var bmMenuItem = this._element("bmRootItem");
      bmMenuItem.label = bms.getItemTitle(PlacesUtils.bookmarksMenuFolderId);
      bmMenuItem.folderId = PlacesUtils.bookmarksMenuFolderId;
      var toolbarItem = this._element("toolbarFolderItem");
      toolbarItem.label = bms.getItemTitle(PlacesUtils.toolbarFolderId);
      toolbarItem.folderId = PlacesUtils.toolbarFolderId;
      this._staticFoldersListBuilt = true;
    }

    // List of recently used folders:
    var folderIds = annos.getItemsWithAnnotation(LAST_USED_ANNO);

    /**
     * The value of the LAST_USED_ANNO annotation is the time (in the form of
     * Date.getTime) at which the folder has been last used.
     *
     * First we build the annotated folders array, each item has both the
     * folder identifier and the time at which it was last-used by this dialog
     * set. Then we sort it descendingly based on the time field.
     */
    this._recentFolders = [];
    for (var i = 0; i < folderIds.length; i++) {
      var lastUsed = annos.getItemAnnotation(folderIds[i], LAST_USED_ANNO);
      this._recentFolders.push({ folderId: folderIds[i], lastUsed: lastUsed });
    }
    this._recentFolders.sort(function(a, b) {
      if (b.lastUsed < a.lastUsed)
        return -1;
      if (b.lastUsed > a.lastUsed)
        return 1;
      return 0;
    });

    var numberOfItems = Math.min(MAX_FOLDER_ITEM_IN_MENU_LIST,
                                 this._recentFolders.length);
    for (var i = 0; i < numberOfItems; i++) {
      this._appendFolderItemToMenupopup(menupopup,
                                        this._recentFolders[i].folderId);
    }

    var defaultItem = this._getFolderMenuItem(aSelectedFolder);
    this._folderMenuList.selectedItem = defaultItem;

    // Set a selectedIndex attribute to show special icons
    this._folderMenuList.setAttribute("selectedIndex",
                                      this._folderMenuList.selectedIndex);

    // Hide the folders-separator if no folder is annotated as recently-used
    this._element("foldersSeparator").hidden = (menupopup.childNodes.length <= 6);
    this._folderMenuList.disabled = this._readOnly;
  },

  QueryInterface:
  XPCOMUtils.generateQI([Components.interfaces.nsIDOMEventListener,
                         Components.interfaces.nsINavBookmarkObserver]),

  _element(aID) document.getElementById("editBMPanel_" + aID),

  uninitPanel(aHideCollapsibleElements) {
    if (aHideCollapsibleElements) {
      // Hide the folder tree if it was previously visible.
      var folderTreeRow = this._element("folderTreeRow");
      if (!folderTreeRow.collapsed)
        this.toggleFolderTreeVisibility();

      // Hide the tag selector if it was previously visible.
      var tagsSelectorRow = this._element("tagsSelectorRow");
      if (!tagsSelectorRow.collapsed)
        this.toggleTagsSelector();
    }

    if (this._observersAdded) {
      PlacesUtils.bookmarks.removeObserver(this);
      this._observersAdded = false;
    }

    this._setPaneInfo(null);
    this._firstEditedField = "";
  },

  onTagsFieldChange() {
    if (!this.readOnly) {
      this._updateTags().then(
        anyChanges => {
          if (anyChanges)
            this._mayUpdateFirstEditField("tagsField");
        }, Components.utils.reportError);
    }
  },

  /**
   * For a given array of currently-set tags and the tags-input-field
   * value, returns which tags should be removed and which should be added in
   * the form of { removedTags: [...], newTags: [...] }.
   */
  _getTagsChanges(aCurrentTags) {
    let inputTags = this._getTagsArrayFromTagsInputField();

    // Optimize the trivial cases (which are actually the most common).
    if (inputTags.length == 0 && aCurrentTags.length == 0)
      return { newTags: [], removedTags: [] };
    if (inputTags.length == 0)
      return { newTags: [], removedTags: aCurrentTags };
    if (aCurrentTags.length == 0)
      return { newTags: inputTags, removedTags: [] };

    let removedTags = aCurrentTags.filter(t => inputTags.indexOf(t) == -1);
    let newTags = inputTags.filter(t => aCurrentTags.indexOf(t) == -1);
    return { removedTags, newTags };
  },

  // Adds and removes tags for one or more uris.
  _setTagsFromTagsInputField: Task.async(function* (aCurrentTags, aURIs) {
    let { removedTags, newTags } = this._getTagsChanges(aCurrentTags);
    if (removedTags.length + newTags.length == 0)
      return false;

    if (!PlacesUIUtils.useAsyncTransactions) {
      let txns = [];
      for (let uri of aURIs) {
        if (removedTags.length > 0)
          txns.push(new PlacesUntagURITransaction(uri, removedTags));
        if (newTags.length > 0)
          txns.push(new PlacesTagURITransaction(uri, newTags));
      }

      PlacesUtils.transactionManager.doTransaction(
        new PlacesAggregatedTransaction("Update tags", txns));
      return true;
    }

    let setTags = function* () {
      if (newTags.length > 0) {
        yield PlacesTransactions.Tag({ urls: aURIs, tags: newTags })
                                .transact();
      }
      if (removedTags.length > 0) {
        yield PlacesTransactions.Untag({ urls: aURIs, tags: removedTags })
                          .transact();
      }
    };

    // Only in the library info-pane it's safe (and necessary) to batch these.
    // TODO bug 1093030: cleanup this mess when the bookmarksProperties dialog
    // and star UI code don't "run a batch in the background".
    if (window.document.documentElement.id == "places")
      PlacesTransactions.batch(setTags).catch(Components.utils.reportError);
    else
      Task.spawn(setTags).catch(Components.utils.reportError);
    return true;
  }),

  _updateTags: Task.async(function*() {
    let uris = this._paneInfo.bulkTagging ?
                 this._paneInfo.uris : [this._paneInfo.uri];
    let currentTags = this._paneInfo.bulkTagging ?
                        yield this._getCommonTags() :
                        PlacesUtils.tagging.getTagsForURI(uris[0]);
    let anyChanges = yield this._setTagsFromTagsInputField(currentTags, uris);
    if (!anyChanges)
      return false;

    // The panel could have been closed in the meanwhile.
    if (!this._paneInfo)
      return false;

    // Ensure the tagsField is in sync, clean it up from empty tags
    currentTags = this._paneInfo.bulkTagging ?
                    this._getCommonTags() :
                    PlacesUtils.tagging.getTagsForURI(this._paneInfo.uri);
    this._initTextField(this._tagsField, currentTags.join(", "), false);
    return true;
  }),

  /**
   * Stores the first-edit field for this dialog, if the passed-in field
   * is indeed the first edited field
   * @param aNewField
   *        the id of the field that may be set (without the "editBMPanel_"
   *        prefix)
   */
  _mayUpdateFirstEditField(aNewField) {
    // * The first-edit-field behavior is not applied in the multi-edit case
    // * if this._firstEditedField is already set, this is not the first field,
    //   so there's nothing to do
    if (this._paneInfo.bulkTagging || this._firstEditedField)
      return;

    this._firstEditedField = aNewField;

    // set the pref
    var prefs = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefBranch);
    prefs.setCharPref("browser.bookmarks.editDialog.firstEditField", aNewField);
  },

  onNamePickerChange() {
    if (this.readOnly || !this._paneInfo.isItem)
      return;

    // Here we update either the item title or its cached static title
    let newTitle = this._namePicker.value;
    if (!newTitle &&
        PlacesUtils.bookmarks.getFolderIdForItem(this._paneInfo.itemId) == PlacesUtils.tagsFolderId) {
      // We don't allow setting an empty title for a tag, restore the old one.
      this._initNamePicker();
    }
    else {
      this._mayUpdateFirstEditField("namePicker");
      if (!PlacesUIUtils.useAsyncTransactions) {
        let txn = new PlacesEditItemTitleTransaction(this._paneInfo.itemId,
                                                     newTitle);
        PlacesUtils.transactionManager.doTransaction(txn);
        return;
      }
      let guid = this._paneInfo.itemGuid;
      PlacesTransactions.EditTitle({ guid, title: newTitle })
                        .transact().catch(Components.utils.reportError);
    }
  },

  onDescriptionFieldChange() {
    if (this.readOnly || !this._paneInfo.isItem)
      return;

    let itemId = this._paneInfo.itemId;
    let description = this._element("descriptionField").value;
    if (description != PlacesUIUtils.getItemDescription(this._paneInfo.itemId)) {
      let annotation =
        { name: PlacesUIUtils.DESCRIPTION_ANNO, value: description };
      if (!PlacesUIUtils.useAsyncTransactions) {
        let txn = new PlacesSetItemAnnotationTransaction(itemId,
                                                         annotation);
        PlacesUtils.transactionManager.doTransaction(txn);
        return;
      }
      let guid = this._paneInfo.itemGuid;
      PlacesTransactions.Annotate({ guid, annotation })
                        .transact().catch(Components.utils.reportError);
    }
  },

  onLocationFieldChange() {
    if (this.readOnly || !this._paneInfo.isBookmark)
      return;

    let newURI;
    try {
      newURI = PlacesUIUtils.createFixedURI(this._locationField.value);
    }
    catch(ex) {
      // TODO: Bug 1089141 - Provide some feedback about the invalid url.
      return;
    }

    if (this._paneInfo.uri.equals(newURI))
      return;

    if (!PlacesUIUtils.useAsyncTransactions) {
      let itemId = this._paneInfo.itemId;
      let txn = new PlacesEditBookmarkURITransaction(this._paneInfo.itemId, newURI);
      PlacesUtils.transactionManager.doTransaction(txn);
      return;
    }
    let guid = this._paneInfo.itemGuid;
    PlacesTransactions.EditUrl({ guid, url: newURI })
                      .transact().catch(Components.utils.reportError);
  },

  onKeywordFieldChange() {
    if (this.readOnly || !this._paneInfo.isBookmark)
      return;

    let itemId = this._paneInfo.itemId;
    let newKeyword = this._keywordField.value;
    if (!PlacesUIUtils.useAsyncTransactions) {
      let txn = new PlacesEditBookmarkKeywordTransaction(itemId, newKeyword);
      PlacesUtils.transactionManager.doTransaction(txn);
      return;
    }
    let guid = this._paneInfo.itemGuid;
    PlacesTransactions.EditKeyword({ guid, keyword: newKeyword })
                      .transact().catch(Components.utils.reportError);
  },

  onLoadInSidebarCheckboxCommand() {
    if (!this.initialized || !this._paneInfo.isBookmark)
      return;

    let annotation = { name : PlacesUIUtils.LOAD_IN_SIDEBAR_ANNO };
    if (this._loadInSidebarCheckbox.checked)
      annotation.value = true;

    if (!PlacesUIUtils.useAsyncTransactions) {
      let itemId = this._paneInfo.itemId;
      let txn = new PlacesSetItemAnnotationTransaction(itemId,
                                                       annotation);
      PlacesUtils.transactionManager.doTransaction(txn);
      return;
    }
    let guid = this._paneInfo.itemGuid;
    PlacesTransactions.Annotate({ guid, annotation })
                      .transact().catch(Components.utils.reportError);
  },

  toggleFolderTreeVisibility() {
    var expander = this._element("foldersExpander");
    var folderTreeRow = this._element("folderTreeRow");
    if (!folderTreeRow.collapsed) {
      expander.className = "expander-down";
      expander.setAttribute("tooltiptext",
                            expander.getAttribute("tooltiptextdown"));
      folderTreeRow.collapsed = true;
      this._element("chooseFolderSeparator").hidden =
        this._element("chooseFolderMenuItem").hidden = false;
    }
    else {
      expander.className = "expander-up"
      expander.setAttribute("tooltiptext",
                            expander.getAttribute("tooltiptextup"));
      folderTreeRow.collapsed = false;

      // XXXmano: Ideally we would only do this once, but for some odd reason,
      // the editable mode set on this tree, together with its collapsed state
      // breaks the view.
      const FOLDER_TREE_PLACE_URI =
        "place:excludeItems=1&excludeQueries=1&excludeReadOnlyFolders=1&folder=" +
        PlacesUIUtils.allBookmarksFolderId;
      this._folderTree.place = FOLDER_TREE_PLACE_URI;

      this._element("chooseFolderSeparator").hidden =
        this._element("chooseFolderMenuItem").hidden = true;
      var currentFolder = this._getFolderIdFromMenuList();
      this._folderTree.selectItems([currentFolder]);
      this._folderTree.focus();
    }
  },

  _getFolderIdFromMenuList() {
    var selectedItem = this._folderMenuList.selectedItem;
    NS_ASSERT("folderId" in selectedItem,
              "Invalid menuitem in the folders-menulist");
    return selectedItem.folderId;
  },

  /**
   * Get the corresponding menu-item in the folder-menu-list for a bookmarks
   * folder if such an item exists. Otherwise, this creates a menu-item for the
   * folder. If the items-count limit (see MAX_FOLDERS_IN_MENU_LIST) is reached,
   * the new item replaces the last menu-item.
   * @param aFolderId
   *        The identifier of the bookmarks folder.
   */
  _getFolderMenuItem(aFolderId) {
    let menupopup = this._folderMenuList.menupopup;
    let menuItem = Array.prototype.find.call(
      menupopup.childNodes, menuItem => menuItem.folderId === aFolderId);
    if (menuItem !== undefined)
      return menuItem;

    // 3 special folders + separator + folder-items-count limit
    if (menupopup.childNodes.length == 4 + MAX_FOLDER_ITEM_IN_MENU_LIST)
      menupopup.removeChild(menupopup.lastChild);

    return this._appendFolderItemToMenupopup(menupopup, aFolderId);
  },

  onFolderMenuListCommand(aEvent) {
    // Set a selectedIndex attribute to show special icons
    this._folderMenuList.setAttribute("selectedIndex",
                                      this._folderMenuList.selectedIndex);

    if (aEvent.target.id == "editBMPanel_chooseFolderMenuItem") {
      // reset the selection back to where it was and expand the tree
      // (this menu-item is hidden when the tree is already visible
      let containerId = PlacesUtils.bookmarks.getFolderIdForItem(this._paneInfo.itemId);
      let item = this._getFolderMenuItem(containerId);
      this._folderMenuList.selectedItem = item;
      // XXXmano HACK: setTimeout 100, otherwise focus goes back to the
      // menulist right away
      setTimeout(function(self) self.toggleFolderTreeVisibility(), 100, this);
      return;
    }

    // Move the item
    let containerId = this._getFolderIdFromMenuList();
    if (PlacesUtils.bookmarks.getFolderIdForItem(this._paneInfo.itemId) != containerId &&
        this._paneInfo.itemId != containerId) {
      if (PlacesUIUtils.useAsyncTransactions) {
        Task.spawn(function* () {
          let newParentGuid = yield PlacesUtils.promiseItemGuid(containerId);
          let guid = this._paneInfo.itemGuid;
          yield PlacesTransactions.Move({ guid, newParentGuid }).transact();
        }.bind(this));
      }
      else {
        let txn = new PlacesMoveItemTransaction(this._paneInfo.itemId,
                                                containerId,
                                                PlacesUtils.bookmarks.DEFAULT_INDEX);
        PlacesUtils.transactionManager.doTransaction(txn);
      }

      // Mark the containing folder as recently-used if it isn't in the
      // static list
      if (containerId != PlacesUtils.unfiledBookmarksFolderId &&
          containerId != PlacesUtils.toolbarFolderId &&
          containerId != PlacesUtils.bookmarksMenuFolderId) {
        this._markFolderAsRecentlyUsed(containerId)
            .catch(Components.utils.reportError);
      }
    }

    // Update folder-tree selection
    var folderTreeRow = this._element("folderTreeRow");
    if (!folderTreeRow.collapsed) {
      var selectedNode = this._folderTree.selectedNode;
      if (!selectedNode ||
          PlacesUtils.getConcreteItemId(selectedNode) != containerId)
        this._folderTree.selectItems([containerId]);
    }
  },

  onFolderTreeSelect() {
    var selectedNode = this._folderTree.selectedNode;

    // Disable the "New Folder" button if we cannot create a new folder
    this._element("newFolderButton")
        .disabled = !this._folderTree.insertionPoint || !selectedNode;

    if (!selectedNode)
      return;

    var folderId = PlacesUtils.getConcreteItemId(selectedNode);
    if (this._getFolderIdFromMenuList() == folderId)
      return;

    var folderItem = this._getFolderMenuItem(folderId);
    this._folderMenuList.selectedItem = folderItem;
    folderItem.doCommand();
  },

  _markFolderAsRecentlyUsed: Task.async(function* (aFolderId) {
    if (!PlacesUIUtils.useAsyncTransactions) {
      let txns = [];

      // Expire old unused recent folders.
      let annotation = this._getLastUsedAnnotationObject(false);
      while (this._recentFolders.length > MAX_FOLDER_ITEM_IN_MENU_LIST) {
        let folderId = this._recentFolders.pop().folderId;
        let annoTxn = new PlacesSetItemAnnotationTransaction(folderId,
                                                             annotation);
        txns.push(annoTxn);
      }

      // Mark folder as recently used
      annotation = this._getLastUsedAnnotationObject(true);
      let annoTxn = new PlacesSetItemAnnotationTransaction(aFolderId,
                                                           annotation);
      txns.push(annoTxn);

      let aggregate =
        new PlacesAggregatedTransaction("Update last used folders", txns);
      PlacesUtils.transactionManager.doTransaction(aggregate);
      return;
    }

    // Expire old unused recent folders.
    let guids = [];
    while (this._recentFolders.length > MAX_FOLDER_ITEM_IN_MENU_LIST) {
      let folderId = this._recentFolders.pop().folderId;
      let guid = yield PlacesUtils.promiseItemGuid(folderId);
      guids.push(guid);
    }
    if (guids.length > 0) {
      let annotation = this._getLastUsedAnnotationObject(false);
      PlacesTransactions.Annotate({ guids, annotation  })
                        .transact().catch(Components.utils.reportError);
    }

    // Mark folder as recently used
    let annotation = this._getLastUsedAnnotationObject(true);
    let guid = yield PlacesUtils.promiseItemGuid(aFolderId);
    PlacesTransactions.Annotate({ guid, annotation })
                      .transact().catch(Components.utils.reportError);
  }),

  /**
   * Returns an object which could then be used to set/unset the
   * LAST_USED_ANNO annotation for a folder.
   *
   * @param aLastUsed
   *        Whether to set or unset the LAST_USED_ANNO annotation.
   * @returns an object representing the annotation which could then be used
   *          with the transaction manager.
   */
  _getLastUsedAnnotationObject(aLastUsed) {
    return { name: LAST_USED_ANNO,
             value: aLastUsed ? new Date().getTime() : null };
  },

  _rebuildTagsSelectorList: Task.async(function* () {
    let tagsSelector = this._element("tagsSelector");
    let tagsSelectorRow = this._element("tagsSelectorRow");
    if (tagsSelectorRow.collapsed)
      return;

    // Save the current scroll position and restore it after the rebuild.
    let firstIndex = tagsSelector.getIndexOfFirstVisibleRow();
    let selectedIndex = tagsSelector.selectedIndex;
    let selectedTag = selectedIndex >= 0 ? tagsSelector.selectedItem.label
                                         : null;

    while (tagsSelector.hasChildNodes()) {
      tagsSelector.removeChild(tagsSelector.lastChild);
    }

    let tagsInField = this._getTagsArrayFromTagsInputField();
    let allTags = PlacesUtils.tagging.allTags;
    for (tag of allTags) {
      let elt = document.createElement("listitem");
      elt.setAttribute("type", "checkbox");
      elt.setAttribute("label", tag);
      if (tagsInField.indexOf(tag) != -1)
        elt.setAttribute("checked", "true");
      tagsSelector.appendChild(elt);
      if (selectedTag === tag)
        selectedIndex = tagsSelector.getIndexOfItem(elt);
    }

    // Restore position.
    // The listbox allows to scroll only if the required offset doesn't
    // overflow its capacity, thus need to adjust the index for removals.
    firstIndex =
      Math.min(firstIndex,
               tagsSelector.itemCount - tagsSelector.getNumberOfVisibleRows());
    tagsSelector.scrollToIndex(firstIndex);
    if (selectedIndex >= 0 && tagsSelector.itemCount > 0) {
      selectedIndex = Math.min(selectedIndex, tagsSelector.itemCount - 1);
      tagsSelector.selectedIndex = selectedIndex;
      tagsSelector.ensureIndexIsVisible(selectedIndex);
    }
  }),

  toggleTagsSelector: Task.async(function* () {
    var tagsSelector = this._element("tagsSelector");
    var tagsSelectorRow = this._element("tagsSelectorRow");
    var expander = this._element("tagsSelectorExpander");
    if (tagsSelectorRow.collapsed) {
      expander.className = "expander-up";
      expander.setAttribute("tooltiptext",
                            expander.getAttribute("tooltiptextup"));
      tagsSelectorRow.collapsed = false;
      yield this._rebuildTagsSelectorList();

      // This is a no-op if we've added the listener.
      tagsSelector.addEventListener("CheckboxStateChange", this, false);
    }
    else {
      expander.className = "expander-down";
      expander.setAttribute("tooltiptext",
                            expander.getAttribute("tooltiptextdown"));
      tagsSelectorRow.collapsed = true;
    }
  }),

  /**
   * Splits "tagsField" element value, returning an array of valid tag strings.
   *
   * @return Array of tag strings found in the field value.
   */
  _getTagsArrayFromTagsInputField() {
    let tags = this._element("tagsField").value;
    return tags.trim()
               .split(/\s*,\s*/) // Split on commas and remove spaces.
               .filter(function (tag) tag.length > 0); // Kill empty tags.
  },

  newFolder: Task.async(function* () {
    let ip = this._folderTree.insertionPoint;

    // default to the bookmarks menu folder
    if (!ip || ip.itemId == PlacesUIUtils.allBookmarksFolderId) {
      ip = new InsertionPoint(PlacesUtils.bookmarksMenuFolderId,
                              PlacesUtils.bookmarks.DEFAULT_INDEX,
                              Ci.nsITreeView.DROP_ON);
    }

    // XXXmano: add a separate "New Folder" string at some point...
    let title = this._element("newFolderButton").label;
    if (PlacesUIUtils.useAsyncTransactions) {
      let parentGuid = yield ip.promiseGuid();
      yield PlacesTransactions.NewFolder({ parentGuid, title, index: ip.index })
                              .transact().catch(Components.utils.reportError);
    }
    else {
      let txn = new PlacesCreateFolderTransaction(title, ip.itemId, ip.index);
      PlacesUtils.transactionManager.doTransaction(txn);
    }

    this._folderTree.focus();
    this._folderTree.selectItems([ip.itemId]);
    PlacesUtils.asContainer(this._folderTree.selectedNode).containerOpen = true;
    this._folderTree.selectItems([this._lastNewItem]);
    this._folderTree.startEditing(this._folderTree.view.selection.currentIndex,
                                  this._folderTree.columns.getFirstColumn());
  }),

  // nsIDOMEventListener
  handleEvent(aEvent) {
    switch (aEvent.type) {
    case "CheckboxStateChange":
      // Update the tags field when items are checked/unchecked in the listbox
      let tags = this._getTagsArrayFromTagsInputField();
      let tagCheckbox = aEvent.target;

      let curTagIndex = tags.indexOf(tagCheckbox.label);
      let tagsSelector = this._element("tagsSelector");
      tagsSelector.selectedItem = tagCheckbox;

      if (tagCheckbox.checked) {
        if (curTagIndex == -1)
          tags.push(tagCheckbox.label);
      }
      else {
        if (curTagIndex != -1)
          tags.splice(curTagIndex, 1);
      }
      this._element("tagsField").value = tags.join(", ");
      this._updateTags();
      break;
    case "unload":
      this.uninitPanel(false);
      break;
    }
  },

  _initTagsField: Task.async(function* () {
    let tags;
    if (this._paneInfo.isURI)
      tags = PlacesUtils.tagging.getTagsForURI(this._paneInfo.uri);
    else if (this._paneInfo.bulkTagging)
      tags = this._getCommonTags();
    else
      throw new Error("_promiseTagsStr called unexpectedly");

    this._initTextField(this._tagsField, tags.join(", "));
  }),

  _onTagsChange(aItemId) {
    let paneInfo = this._paneInfo;
    let updateTagsField = false;
    if (paneInfo.isURI) {
      if (paneInfo.isBookmark && aItemId == paneInfo.itemId) {
        updateTagsField = true;
      }
      else if (!paneInfo.isBookmark) {
        let changedURI = PlacesUtils.bookmarks.getBookmarkURI(aItemId);
        updateTagsField = changedURI.equals(paneInfo.uri);
      }
    }
    else if (paneInfo.bulkTagging) {
      let changedURI = PlacesUtils.bookmarks.getBookmarkURI(aItemId);
      if (paneInfo.uris.some(uri => uri.equals(changedURI))) {
        updateTagsField = true;
        delete this._paneInfo._cachedCommonTags;
      }
    }
    else {
      throw new Error("_onTagsChange called unexpectedly");
    }

    if (updateTagsField)
      this._initTagsField().catch(Components.utils.reportError);

    // Any tags change should be reflected in the tags selector.
    if (this._element("tagsSelector"))
      this._rebuildTagsSelectorList().catch(Components.utils.reportError);
  },

  _onItemTitleChange(aItemId, aNewTitle) {
    if (!this._paneInfo.isBookmark)
      return;
    if (aItemId == this._paneInfo.itemId) {
      this._paneInfo.title = aNewTitle;
      this._initTextField(this._namePicker, aNewTitle);
    }
    else if (this._paneInfo.visibleRows.has("folderRow")) {
      // If the title of a folder which is listed within the folders
      // menulist has been changed, we need to update the label of its
      // representing element.
      let menupopup = this._folderMenuList.menupopup;
      for (menuitem of menupopup.childNodes) {
        if ("folderId" in menuitem && menuitem.folderId == aItemId) {
          menuitem.label = aNewTitle;
          break;
        }
      }
    }
  },

  // nsINavBookmarkObserver
  onItemChanged(aItemId, aProperty, aIsAnnotationProperty, aValue,
                aLastModified, aItemType) {
    if (aProperty == "tags" && this._paneInfo.visibleRows.has("tagsRow")) {
      this._onTagsChange(aItemId);
    }
    else if (aProperty == "title" && this._paneInfo.isItem) {
      // This also updates titles of folders in the folder menu list.
      this._onItemTitleChange(aItemId, aValue);
    }
    else if (!this._paneInfo.isItem || this._paneInfo.itemId != aItemId) {
      return;
    }

    switch (aProperty) {
    case "uri":
      let newURI = NetUtil.newURI(aValue);
      if (!newURI.equals(this._paneInfo.uri)) {
        this._paneInfo.uri = newURI;
        if (this._paneInfo.visibleRows.has("locationRow"))
          this._initLocationField();

        if (this._paneInfo.visibleRows.has("tagsRow")) {
          delete this._paneInfo._cachedCommonTags;
          this._onTagsChange(aItemId);
        }
      }
      break;
    case "keyword":
      if (this._paneInfo.visibleRows.has("keywordRow"))
        this._initKeywordField(aValue);
      break;
    case PlacesUIUtils.DESCRIPTION_ANNO:
      if (this._paneInfo.visibleRows.has("descriptionRow"))
        this._initDescriptionField();
      break;
    case PlacesUIUtils.LOAD_IN_SIDEBAR_ANNO:
      if (this._paneInfo.visibleRows.has("loadInSidebarCheckbox"))
        this._initLoadInSidebar();
      break;
    }
  },

  onItemMoved(aItemId, aOldParent, aOldIndex,
              aNewParent, aNewIndex, aItemType) {
    if (!this._paneInfo.isItem ||
        !this._paneInfo.visibleRows.has("folderPicker") ||
        this._paneInfo.itemId != aItemOd ||
        aNewParent == this._getFolderIdFromMenuList()) {
      return;
    }

    // Just setting selectItem _does not_ trigger oncommand, so we don't
    // recurse.
    this._folderMenuList.selectedItem = this._getFolderMenuItem(aNewParent);
  },

  onItemAdded(aItemId, aParentId, aIndex, aItemType, aURI) {
    this._lastNewItem = aItemId;
  },

  onItemRemoved() { },
  onBeginUpdateBatch() { },
  onEndUpdateBatch() { },
  onItemVisited() { },
};


for (let elt of ["folderMenuList", "folderTree", "namePicker",
                 "locationField", "descriptionField", "keywordField",
                 "tagsField", "loadInSidebarCheckbox"]) {
  let eltScoped = elt;
  XPCOMUtils.defineLazyGetter(gEditItemOverlay, `_${eltScoped}`,
                              () => gEditItemOverlay._element(eltScoped));
}
