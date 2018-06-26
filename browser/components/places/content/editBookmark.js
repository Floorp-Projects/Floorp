/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const MAX_FOLDER_ITEM_IN_MENU_LIST = 5;

var gEditItemOverlay = {
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

    // We either pass a node or uris.
    let node = "node" in aInitInfo ? aInitInfo.node : null;

    // Since there's no true UI for folder shortcuts (they show up just as their target
    // folders), when the pane shows for them it's opened in read-only mode, showing the
    // properties of the target folder.
    let itemId = node ? node.itemId : -1;
    let itemGuid = node ? PlacesUtils.getConcreteItemGuid(node) : null;
    let isItem = itemId != -1;
    let isFolderShortcut = isItem &&
                           node.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT;
    let isTag = node && PlacesUtils.nodeIsTagQuery(node);
    let tag = null;
    if (isTag) {
      tag = PlacesUtils.asQuery(node).query.tags.length == 1 ? node.query.tags[0] : node.title;
    }
    let isURI = node && PlacesUtils.nodeIsURI(node);
    let uri = isURI || isTag ? Services.io.newURI(node.uri) : null;
    let title = node ? node.title : null;
    let isBookmark = isItem && isURI;
    let bulkTagging = !node;
    let uris = bulkTagging ? aInitInfo.uris : null;
    let visibleRows = new Set();
    let isParentReadOnly = false;
    let postData = aInitInfo.postData;
    let parentGuid = null;

    if (node && isItem) {
      if (!node.parent || (node.parent.itemId > 0 && !node.parent.bookmarkGuid)) {
        throw new Error("Cannot use an incomplete node to initialize the edit bookmark panel");
      }
      let parent = node.parent;
      isParentReadOnly = !PlacesUtils.nodeIsFolder(parent);
      parentGuid = parent.bookmarkGuid;
    }

    let focusedElement = aInitInfo.focusedElement;
    let onPanelReady = aInitInfo.onPanelReady;

    return this._paneInfo = { itemId, itemGuid, parentGuid, isItem,
                              isURI, uri, title,
                              isBookmark, isFolderShortcut, isParentReadOnly,
                              bulkTagging, uris,
                              visibleRows, postData, isTag, focusedElement,
                              onPanelReady, tag };
  },

  get initialized() {
    return this._paneInfo != null;
  },

  // Backwards-compatibility getters
  get itemId() {
    if (!this.initialized || this._paneInfo.isTag || this._paneInfo.bulkTagging)
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
    // TODO (Bug 1120314): Folder shortcuts are currently read-only due to some
    // quirky implementation details (the most important being the "smart"
    // semantics of node.title that makes hard to edit the right entry).
    // This pane is read-only if:
    //  * the panel is not initialized
    //  * the node is a folder shortcut
    //  * the node is not bookmarked and not a tag container
    //  * the node is child of a read-only container and is not a bookmarked
    //    URI nor a tag container
    return !this.initialized ||
           this._paneInfo.isFolderShortcut ||
           (!this._paneInfo.isItem && !this._paneInfo.isTag) ||
           (this._paneInfo.isParentReadOnly && !this._paneInfo.isBookmark && !this._paneInfo.isTag);
  },

  // the first field which was edited after this panel was initialized for
  // a certain item
  _firstEditedField: "",

  _initNamePicker() {
    if (this._paneInfo.bulkTagging)
      throw new Error("_initNamePicker called unexpectedly");

    // title may by null, which, for us, is the same as an empty string.
    this._initTextField(this._namePicker, this._paneInfo.title || this._paneInfo.tag || "");
  },

  _initLocationField() {
    if (!this._paneInfo.isURI)
      throw new Error("_initLocationField called unexpectedly");
    this._initTextField(this._locationField, this._paneInfo.uri.spec);
  },

  async _initKeywordField(newKeyword = "") {
    if (!this._paneInfo.isBookmark) {
      throw new Error("_initKeywordField called unexpectedly");
    }

    // Reset the field status synchronously now, eventually we'll reinit it
    // later if we find an existing keyword. This way we can ensure to be in a
    // consistent status when reusing the panel across different bookmarks.
    this._keyword = newKeyword;
    this._initTextField(this._keywordField, newKeyword);

    if (!newKeyword) {
      let entries = [];
      await PlacesUtils.keywords.fetch({ url: this._paneInfo.uri.spec },
                                       e => entries.push(e));
      if (entries.length > 0) {
        // We show an existing keyword if either POST data was not provided, or
        // if the POST data is the same.
        let existingKeyword = entries[0].keyword;
        let postData = this._paneInfo.postData;
        if (postData) {
          let sameEntry = entries.find(e => e.postData === postData);
          existingKeyword = sameEntry ? sameEntry.keyword : "";
        }
        if (existingKeyword) {
          this._keyword = existingKeyword;
          // Update the text field to the existing keyword.
          this._initTextField(this._keywordField, this._keyword);
        }
      }
    }
  },

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
   *            "keyword", "feedLocation", "siteLocation", folderPicker"
   */
  initPanel(aInfo) {
    if (typeof(aInfo) != "object" || aInfo === null)
      throw new Error("aInfo must be an object.");
    if ("node" in aInfo) {
      try {
        aInfo.node.type;
      } catch (e) {
        // If the lazy loader for |type| generates an exception, it means that
        // this bookmark could not be loaded. This sometimes happens when tests
        // create a bookmark by clicking the bookmark star, then try to cleanup
        // before the bookmark panel has finished opening. Either way, if we
        // cannot retrieve the bookmark information, we cannot open the panel.
        return;
      }
    }

    // For sanity ensure that the implementer has uninited the panel before
    // trying to init it again, or we could end up leaking due to observers.
    if (this.initialized)
      this.uninitPanel(false);

    let { parentGuid, isItem, isURI,
          isBookmark, bulkTagging, uris,
          visibleRows, focusedElement,
          onPanelReady } = this._setPaneInfo(aInfo);

    let showOrCollapse =
      (rowId, isAppropriateForInput, nameInHiddenRows = null) => {
        let visible = isAppropriateForInput;
        if (visible && "hiddenRows" in aInfo && nameInHiddenRows)
          visible &= !aInfo.hiddenRows.includes(nameInHiddenRows);
        if (visible)
          visibleRows.add(rowId);
        return !(this._element(rowId).collapsed = !visible);
      };

    if (showOrCollapse("nameRow", !bulkTagging, "name")) {
      this._initNamePicker();
      this._namePicker.readOnly = this.readOnly;
    }

    // In some cases we want to hide the location field, since it's not
    // human-readable, but we still want to initialize it.
    showOrCollapse("locationRow", isURI, "location");
    if (isURI) {
      this._initLocationField();
      this._locationField.readOnly = this.readOnly;
    }

    if (showOrCollapse("keywordRow", isBookmark, "keyword")) {
      this._initKeywordField().catch(Cu.reportError);
      this._keywordField.readOnly = this.readOnly;
    }

    // Collapse the tag selector if the item does not accept tags.
    if (showOrCollapse("tagsRow", isURI || bulkTagging, "tags"))
      this._initTagsField();
    else if (!this._element("tagsSelectorRow").collapsed)
      this.toggleTagsSelector();

    // Folder picker.
    // Technically we should check that the item is not moveable, but that's
    // not cheap (we don't always have the parent), and there's no use case for
    // this (it's only the Star UI that shows the folderPicker)
    if (showOrCollapse("folderRow", isItem, "folderPicker")) {
      this._initFolderMenuList(parentGuid).catch(Cu.reportError);
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
      PlacesUtils.bookmarks.addObserver(this);
      window.addEventListener("unload", this);
      this._observersAdded = true;
    }

    let focusElement = () => {
      // The focusedElement possible values are:
      //  * preferred: focus the field that the user touched first the last
      //    time the pane was shown (either namePicker or tagsField)
      //  * first: focus the first non collapsed textbox
      // Note: since all controls are collapsed by default, we don't get the
      // default XUL dialog behavior, that selects the first control, so we set
      // the focus explicitly.
      let elt;
      if (focusedElement === "preferred") {
        elt = this._element(Services.prefs.getCharPref("browser.bookmarks.editDialog.firstEditField"));
      } else if (focusedElement === "first") {
        elt = document.querySelector("textbox:not([collapsed=true])");
      }
      if (elt) {
        elt.focus();
        elt.select();
      }
    };

    if (onPanelReady) {
      onPanelReady(focusElement);
    } else {
      focusElement();
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
        if (!curentURITags.includes(tag)) {
          commonTags.delete(tag);
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

      // Clear the editor's undo stack
      let transactionManager;
      try {
        transactionManager = aElement.editor.transactionManager;
      } catch (e) {
        // When retrieving the transaction manager, editor may be null resulting
        // in a TypeError. Additionally, the transaction manager may not
        // exist yet, which causes access to it to throw NS_ERROR_FAILURE.
        // In either event, the transaction manager doesn't exist it, so we
        // don't need to worry about clearing it.
        if (!(e instanceof TypeError) && e.result != Cr.NS_ERROR_FAILURE) {
          throw e;
        }
      }
      if (transactionManager) {
        transactionManager.clear();
      }
    }
  },

  /**
   * Appends a menu-item representing a bookmarks folder to a menu-popup.
   * @param aMenupopup
   *        The popup to which the menu-item should be added.
   * @param aFolderGuid
   *        The identifier of the bookmarks folder.
   * @param aTitle
   *        The title to use as a label.
   * @return the new menu item.
   */
  _appendFolderItemToMenupopup(aMenupopup, aFolderGuid, aTitle) {
    // First make sure the folders-separator is visible
    this._element("foldersSeparator").hidden = false;

    var folderMenuItem = document.createElement("menuitem");
    folderMenuItem.folderGuid = aFolderGuid;
    folderMenuItem.setAttribute("label", aTitle);
    folderMenuItem.className = "menuitem-iconic folder-icon";
    aMenupopup.appendChild(folderMenuItem);
    return folderMenuItem;
  },

  async _initFolderMenuList(aSelectedFolderGuid) {
    // clean up first
    var menupopup = this._folderMenuList.menupopup;
    while (menupopup.childNodes.length > 6)
      menupopup.removeChild(menupopup.lastChild);

    // Build the static list
    if (!this._staticFoldersListBuilt) {
      let unfiledItem = this._element("unfiledRootItem");
      unfiledItem.label = PlacesUtils.getString("OtherBookmarksFolderTitle");
      unfiledItem.folderGuid = PlacesUtils.bookmarks.unfiledGuid;
      let bmMenuItem = this._element("bmRootItem");
      bmMenuItem.label = PlacesUtils.getString("BookmarksMenuFolderTitle");
      bmMenuItem.folderGuid = PlacesUtils.bookmarks.menuGuid;
      let toolbarItem = this._element("toolbarFolderItem");
      toolbarItem.label = PlacesUtils.getString("BookmarksToolbarFolderTitle");
      toolbarItem.folderGuid = PlacesUtils.bookmarks.toolbarGuid;
      this._staticFoldersListBuilt = true;
    }

    // List of recently used folders:
    let lastUsedFolderGuids =
      await PlacesUtils.metadata.get(PlacesUIUtils.LAST_USED_FOLDERS_META_KEY, []);

    /**
     * The list of last used folders is sorted in most-recent first order.
     *
     * First we build the annotated folders array, each item has both the
     * folder identifier and the time at which it was last-used by this dialog
     * set. Then we sort it descendingly based on the time field.
     */
    this._recentFolders = [];
    for (let guid of lastUsedFolderGuids) {
      let bm = await PlacesUtils.bookmarks.fetch(guid);
      if (bm) {
        let title = PlacesUtils.bookmarks.getLocalizedTitle(bm);
        this._recentFolders.push({ guid, title });
      }
    }

    var numberOfItems = Math.min(MAX_FOLDER_ITEM_IN_MENU_LIST,
                                 this._recentFolders.length);
    for (let i = 0; i < numberOfItems; i++) {
      await this._appendFolderItemToMenupopup(menupopup,
                                              this._recentFolders[i].guid,
                                              this._recentFolders[i].title);
    }

    let title = (await PlacesUtils.bookmarks.fetch(aSelectedFolderGuid)).title;
    var defaultItem = this._getFolderMenuItem(aSelectedFolderGuid, title);
    this._folderMenuList.selectedItem = defaultItem;

    // Set a selectedIndex attribute to show special icons
    this._folderMenuList.setAttribute("selectedIndex",
                                      this._folderMenuList.selectedIndex);

    // Hide the folders-separator if no folder is annotated as recently-used
    this._element("foldersSeparator").hidden = (menupopup.childNodes.length <= 6);
    this._folderMenuList.disabled = this.readOnly;
  },

  QueryInterface:
  ChromeUtils.generateQI([Ci.nsINavBookmarkObserver]),

  _element(aID) {
    return document.getElementById("editBMPanel_" + aID);
  },

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

  get selectedFolderGuid() {
    return this._folderMenuList.selectedItem && this._folderMenuList.selectedItem.folderGuid;
  },

  onTagsFieldChange() {
    // Check for _paneInfo existing as the dialog may be closing but receiving
    // async updates from unresolved promises.
    if (this._paneInfo &&
        (this._paneInfo.isURI || this._paneInfo.bulkTagging)) {
      this._updateTags().then(
        anyChanges => {
          // Check _paneInfo here as we might be closing the dialog.
          if (anyChanges && this._paneInfo)
            this._mayUpdateFirstEditField("tagsField");
        }, Cu.reportError);
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

    // Do not remove tags that may be reinserted with a different
    // case, since the tagging service may handle those more efficiently.
    let lcInputTags = inputTags.map(t => t.toLowerCase());
    let removedTags = aCurrentTags.filter(t => !lcInputTags.includes(t.toLowerCase()));
    let newTags = inputTags.filter(t => !aCurrentTags.includes(t));
    return { removedTags, newTags };
  },

  // Adds and removes tags for one or more uris.
  _setTagsFromTagsInputField(aCurrentTags, aURIs) {
    let { removedTags, newTags } = this._getTagsChanges(aCurrentTags);
    if (removedTags.length + newTags.length == 0)
      return false;

    let setTags = async function() {
      if (removedTags.length > 0) {
        await PlacesTransactions.Untag({ urls: aURIs, tags: removedTags })
                                .transact();
      }
      if (newTags.length > 0) {
        await PlacesTransactions.Tag({ urls: aURIs, tags: newTags })
                                .transact();
      }
    };

    // Only in the library info-pane it's safe (and necessary) to batch these.
    // TODO bug 1093030: cleanup this mess when the bookmarksProperties dialog
    // and star UI code don't "run a batch in the background".
    if (window.document.documentElement.id == "places")
      PlacesTransactions.batch(setTags).catch(Cu.reportError);
    else
      setTags().catch(Cu.reportError);
    return true;
  },

  async _updateTags() {
    let uris = this._paneInfo.bulkTagging ?
                 this._paneInfo.uris : [this._paneInfo.uri];
    let currentTags = this._paneInfo.bulkTagging ?
                        await this._getCommonTags() :
                        PlacesUtils.tagging.getTagsForURI(uris[0]);
    let anyChanges = this._setTagsFromTagsInputField(currentTags, uris);
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
  },

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
    Services.prefs.setCharPref("browser.bookmarks.editDialog.firstEditField", aNewField);
  },

  async onNamePickerChange() {
    if (this.readOnly || !(this._paneInfo.isItem || this._paneInfo.isTag))
      return;

    // Here we update either the item title or its cached static title
    if (this._paneInfo.isTag) {
      let tag = this._namePicker.value;
      if (!tag || tag.includes("&")) {
        // We don't allow setting an empty title for a tag, restore the old one.
        this._initNamePicker();
        return;
      }
      // Get all the bookmarks for the old tag, tag them with the new tag, and
      // untag them from the old tag.
      let oldTag = this._paneInfo.tag;
      this._paneInfo.tag = tag;
      let title = this._paneInfo.title;
      if (title == oldTag) {
        this._paneInfo.title = tag;
      }
      await PlacesTransactions.RenameTag({ oldTag, tag }).transact();
      return;
    }

    this._mayUpdateFirstEditField("namePicker");
    await PlacesTransactions.EditTitle({ guid: this._paneInfo.itemGuid,
                                         title: this._namePicker.value }).transact();
  },

  onLocationFieldChange() {
    if (this.readOnly || !this._paneInfo.isBookmark)
      return;

    let newURI;
    try {
      newURI = PlacesUIUtils.createFixedURI(this._locationField.value);
    } catch (ex) {
      // TODO: Bug 1089141 - Provide some feedback about the invalid url.
      return;
    }

    if (this._paneInfo.uri.equals(newURI))
      return;

    let guid = this._paneInfo.itemGuid;
    PlacesTransactions.EditUrl({ guid, url: newURI })
                      .transact().catch(Cu.reportError);
  },

  onKeywordFieldChange() {
    if (this.readOnly || !this._paneInfo.isBookmark)
      return;

    let oldKeyword = this._keyword;
    let keyword = this._keyword = this._keywordField.value;
    let postData = this._paneInfo.postData;
    let guid = this._paneInfo.itemGuid;
    PlacesTransactions.EditKeyword({ guid, keyword, postData, oldKeyword })
                      .transact().catch(Cu.reportError);
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
    } else {
      expander.className = "expander-up";
      expander.setAttribute("tooltiptext",
                            expander.getAttribute("tooltiptextup"));
      folderTreeRow.collapsed = false;

      // XXXmano: Ideally we would only do this once, but for some odd reason,
      // the editable mode set on this tree, together with its collapsed state
      // breaks the view.
      const FOLDER_TREE_PLACE_URI =
        "place:excludeItems=1&excludeQueries=1&excludeReadOnlyFolders=1&type=" +
        Ci.nsINavHistoryQueryOptions.RESULTS_AS_ROOTS_QUERY;
      this._folderTree.place = FOLDER_TREE_PLACE_URI;

      this._element("chooseFolderSeparator").hidden =
        this._element("chooseFolderMenuItem").hidden = true;
      this._folderTree.selectItems([this._paneInfo.parentGuid]);
      this._folderTree.focus();
    }
  },

  /**
   * Get the corresponding menu-item in the folder-menu-list for a bookmarks
   * folder if such an item exists. Otherwise, this creates a menu-item for the
   * folder. If the items-count limit (see MAX_FOLDERS_IN_MENU_LIST) is reached,
   * the new item replaces the last menu-item.
   * @param aFolderGuid
   *        The identifier of the bookmarks folder.
   * @param aTitle
   *        The title to use in case of menuitem creation.
   * @return handle to the menuitem.
   */
  _getFolderMenuItem(aFolderGuid, aTitle) {
    let menupopup = this._folderMenuList.menupopup;
    let menuItem = Array.prototype.find.call(
      menupopup.childNodes, item => item.folderGuid === aFolderGuid);
    if (menuItem !== undefined)
      return menuItem;

    // 3 special folders + separator + folder-items-count limit
    if (menupopup.childNodes.length == 4 + MAX_FOLDER_ITEM_IN_MENU_LIST)
      menupopup.removeChild(menupopup.lastChild);

    return this._appendFolderItemToMenupopup(menupopup, aFolderGuid, aTitle);
  },

  async onFolderMenuListCommand(aEvent) {
    // Check for _paneInfo existing as the dialog may be closing but receiving
    // async updates from unresolved promises.
    if (!this._paneInfo) {
      return;
    }
    // Set a selectedIndex attribute to show special icons
    this._folderMenuList.setAttribute("selectedIndex",
                                      this._folderMenuList.selectedIndex);

    if (aEvent.target.id == "editBMPanel_chooseFolderMenuItem") {
      // reset the selection back to where it was and expand the tree
      // (this menu-item is hidden when the tree is already visible
      let item = this._getFolderMenuItem(this._paneInfo.parentGuid,
                                         this._paneInfo.title);
      this._folderMenuList.selectedItem = item;
      // XXXmano HACK: setTimeout 100, otherwise focus goes back to the
      // menulist right away
      setTimeout(() => this.toggleFolderTreeVisibility(), 100);
      return;
    }

    // Move the item
    let containerGuid = this._folderMenuList.selectedItem.folderGuid;
    if (this._paneInfo.parentGuid != containerGuid &&
        this._paneInfo.itemGuid != containerGuid) {
      await PlacesTransactions.Move({
        guid: this._paneInfo.itemGuid,
        newParentGuid: containerGuid
      }).transact();

      // Auto-show the bookmarks toolbar when adding / moving an item there.
      if (containerGuid == PlacesUtils.bookmarks.toolbarGuid) {
        Services.obs.notifyObservers(null, "autoshow-bookmarks-toolbar");
      }
    }

    // Update folder-tree selection
    var folderTreeRow = this._element("folderTreeRow");
    if (!folderTreeRow.collapsed) {
      var selectedNode = this._folderTree.selectedNode;
      if (!selectedNode ||
          PlacesUtils.getConcreteItemGuid(selectedNode) != containerGuid)
        this._folderTree.selectItems([containerGuid]);
    }
  },

  onFolderTreeSelect() {
    var selectedNode = this._folderTree.selectedNode;

    // Disable the "New Folder" button if we cannot create a new folder
    this._element("newFolderButton")
        .disabled = !this._folderTree.insertionPoint || !selectedNode;

    if (!selectedNode)
      return;

    var folderGuid = PlacesUtils.getConcreteItemGuid(selectedNode);
    if (this._folderMenuList.selectedItem.folderGuid == folderGuid)
      return;

    var folderItem = this._getFolderMenuItem(folderGuid, selectedNode.title);
    this._folderMenuList.selectedItem = folderItem;
    folderItem.doCommand();
  },

  _rebuildTagsSelectorList() {
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
    for (let tag of allTags) {
      let elt = document.createElement("listitem");
      elt.setAttribute("type", "checkbox");
      elt.setAttribute("label", tag);
      if (tagsInField.includes(tag))
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
  },

  toggleTagsSelector() {
    var tagsSelector = this._element("tagsSelector");
    var tagsSelectorRow = this._element("tagsSelectorRow");
    var expander = this._element("tagsSelectorExpander");
    if (tagsSelectorRow.collapsed) {
      expander.className = "expander-up";
      expander.setAttribute("tooltiptext",
                            expander.getAttribute("tooltiptextup"));
      tagsSelectorRow.collapsed = false;
      this._rebuildTagsSelectorList();

      // This is a no-op if we've added the listener.
      tagsSelector.addEventListener("CheckboxStateChange", this);
    } else {
      expander.className = "expander-down";
      expander.setAttribute("tooltiptext",
                            expander.getAttribute("tooltiptextdown"));
      tagsSelectorRow.collapsed = true;
    }
  },

  /**
   * Splits "tagsField" element value, returning an array of valid tag strings.
   *
   * @return Array of tag strings found in the field value.
   */
  _getTagsArrayFromTagsInputField() {
    let tags = this._element("tagsField").value;
    return tags.trim()
               .split(/\s*,\s*/) // Split on commas and remove spaces.
               .filter(tag => tag.length > 0); // Kill empty tags.
  },

  async newFolder() {
    let ip = this._folderTree.insertionPoint;

    // default to the bookmarks menu folder
    if (!ip) {
      ip = new PlacesInsertionPoint({
        parentId: PlacesUtils.bookmarksMenuFolderId,
        parentGuid: PlacesUtils.bookmarks.menuGuid
      });
    }

    // XXXmano: add a separate "New Folder" string at some point...
    let title = this._element("newFolderButton").label;
    let guid = await PlacesTransactions.NewFolder({
      parentGuid: ip.guid,
      title,
      index: await ip.getIndex()
    }).transact().catch(Cu.reportError);

    this._folderTree.focus();
    this._folderTree.selectItems([ip.guid]);
    PlacesUtils.asContainer(this._folderTree.selectedNode).containerOpen = true;
    this._folderTree.selectItems([guid]);
    this._folderTree.startEditing(this._folderTree.view.selection.currentIndex,
                                  this._folderTree.columns.getFirstColumn());
  },

  // EventListener
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
      } else if (curTagIndex != -1) {
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

  _initTagsField() {
    let tags;
    if (this._paneInfo.isURI)
      tags = PlacesUtils.tagging.getTagsForURI(this._paneInfo.uri);
    else if (this._paneInfo.bulkTagging)
      tags = this._getCommonTags();
    else
      throw new Error("_promiseTagsStr called unexpectedly");

    this._initTextField(this._tagsField, tags.join(", "));
  },

  async _onTagsChange(guid, changedURI = null) {
    let paneInfo = this._paneInfo;
    let updateTagsField = false;
    if (paneInfo.isURI) {
      if (paneInfo.isBookmark && guid == paneInfo.itemGuid) {
        updateTagsField = true;
      } else if (!paneInfo.isBookmark) {
        if (!changedURI) {
          let href = (await PlacesUtils.bookmarks.fetch(guid)).url.href;
          changedURI = Services.io.newURI(href);
        }
        updateTagsField = changedURI.equals(paneInfo.uri);
      }
    } else if (paneInfo.bulkTagging) {
      if (!changedURI) {
        let href = (await PlacesUtils.bookmarks.fetch(guid)).url.href;
        changedURI = Services.io.newURI(href);
      }
      if (paneInfo.uris.some(uri => uri.equals(changedURI))) {
        updateTagsField = true;
        delete this._paneInfo._cachedCommonTags;
      }
    } else {
      throw new Error("_onTagsChange called unexpectedly");
    }

    if (updateTagsField) {
      this._initTagsField();
      // Any tags change should be reflected in the tags selector.
      if (this._element("tagsSelector")) {
        this._rebuildTagsSelectorList();
      }
    }
  },

  _onItemTitleChange(aItemId, aNewTitle, aGuid) {
    if (aItemId == this._paneInfo.itemId || aGuid == this._paneInfo.itemGuid) {
      this._paneInfo.title = aNewTitle;
      this._initTextField(this._namePicker, aNewTitle);
    } else if (this._paneInfo.visibleRows.has("folderRow")) {
      // If the title of a folder which is listed within the folders
      // menulist has been changed, we need to update the label of its
      // representing element.
      let menupopup = this._folderMenuList.menupopup;
      for (let menuitem of menupopup.childNodes) {
        if ("folderGuid" in menuitem && menuitem.folderGuid == aGuid) {
          menuitem.label = aNewTitle;
          break;
        }
      }
    }
    // We need to also update title of recent folders.
    if (this._recentFolders) {
      for (let folder of this._recentFolders) {
        if (folder.folderGuid == aGuid) {
          folder.title = aNewTitle;
          break;
        }
      }
    }
  },

  // nsINavBookmarkObserver
  onItemChanged(aItemId, aProperty, aIsAnnotationProperty, aValue,
                aLastModified, aItemType, aParentId, aGuid) {
    if (aProperty == "tags" && this._paneInfo.visibleRows.has("tagsRow")) {
      this._onTagsChange(aGuid).catch(Cu.reportError);
      return;
    }
    if (aProperty == "title" && (this._paneInfo.isItem || this._paneInfo.isTag)) {
      // This also updates titles of folders in the folder menu list.
      this._onItemTitleChange(aItemId, aValue, aGuid);
      return;
    }

    if (!this._paneInfo.isItem || this._paneInfo.itemId != aItemId) {
      return;
    }

    switch (aProperty) {
    case "uri":
      let newURI = Services.io.newURI(aValue);
      if (!newURI.equals(this._paneInfo.uri)) {
        this._paneInfo.uri = newURI;
        if (this._paneInfo.visibleRows.has("locationRow"))
          this._initLocationField();

        if (this._paneInfo.visibleRows.has("tagsRow")) {
          delete this._paneInfo._cachedCommonTags;
          this._onTagsChange(aGuid, newURI).catch(Cu.reportError);
        }
      }
      break;
    case "keyword":
      if (this._paneInfo.visibleRows.has("keywordRow"))
        this._initKeywordField(aValue).catch(Cu.reportError);
      break;
    }
  },

  onItemMoved(id, oldParentId, oldIndex, newParentId, newIndex, type, guid,
              oldParentGuid, newParentGuid) {
    if (!this._paneInfo.isItem || this._paneInfo.itemId != id) {
      return;
    }

    this._paneInfo.parentGuid = newParentGuid;

    if (!this._paneInfo.visibleRows.has("folderRow") ||
        newParentGuid == this._folderMenuList.selectedItem.folderGuid) {
      return;
    }

    // Just setting selectItem _does not_ trigger oncommand, so we don't
    // recurse.
    PlacesUtils.bookmarks.fetch(newParentGuid).then(bm => {
      this._folderMenuList.selectedItem = this._getFolderMenuItem(newParentGuid,
                                                                  bm.title);
    });
  },

  onItemAdded() {},
  onItemRemoved() { },
  onBeginUpdateBatch() { },
  onEndUpdateBatch() { },
  onItemVisited() { },
};

for (let elt of ["folderMenuList", "folderTree", "namePicker",
                 "locationField", "keywordField",
                 "tagsField" ]) {
  let eltScoped = elt;
  XPCOMUtils.defineLazyGetter(gEditItemOverlay, `_${eltScoped}`,
                              () => gEditItemOverlay._element(eltScoped));
}
