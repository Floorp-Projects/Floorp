/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global MozXULElement */

// This is defined in browser.js and only used in the star UI.
/* global setToolbarVisibility */

/* import-globals-from controller.js */

var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  CustomizableUI: "resource:///modules/CustomizableUI.sys.mjs",
  PlacesTransactions: "resource://gre/modules/PlacesTransactions.sys.mjs",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  PromiseUtils: "resource://gre/modules/PromiseUtils.sys.mjs",
});

var gEditItemOverlay = {
  // Array of PlacesTransactions accumulated by internal changes. It can be used
  // to wait for completion.
  transactionPromises: null,
  _staticFoldersListBuilt: false,
  _didChangeFolder: false,
  // Tracks bookmark properties changes in the dialog, allowing external consumers
  // to either confirm or discard them.
  _bookmarkState: null,
  _allTags: null,
  _initPanelDeferred: null,
  _updateTagsDeferred: null,
  _paneInfo: null,
  _setPaneInfo(aInitInfo) {
    if (!aInitInfo) {
      return (this._paneInfo = null);
    }

    if ("uris" in aInitInfo && "node" in aInitInfo) {
      throw new Error("ambiguous pane info");
    }
    if (!("uris" in aInitInfo) && !("node" in aInitInfo)) {
      throw new Error("Neither node nor uris set for pane info");
    }

    // We either pass a node or uris.
    let node = "node" in aInitInfo ? aInitInfo.node : null;

    // Since there's no true UI for folder shortcuts (they show up just as their target
    // folders), when the pane shows for them it's opened in read-only mode, showing the
    // properties of the target folder.
    let itemGuid = node ? PlacesUtils.getConcreteItemGuid(node) : null;
    let isItem = !!itemGuid;
    let isFolderShortcut =
      isItem &&
      node.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT;
    let isTag = node && PlacesUtils.nodeIsTagQuery(node);
    let tag = null;
    if (isTag) {
      tag =
        PlacesUtils.asQuery(node).query.tags.length == 1
          ? node.query.tags[0]
          : node.title;
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
      if (!node.parent) {
        throw new Error(
          "Cannot use an incomplete node to initialize the edit bookmark panel"
        );
      }
      let parent = node.parent;
      isParentReadOnly = !PlacesUtils.nodeIsFolder(parent);
      // Note this may be an empty string, that'd the case for the root node
      // of a search, or a virtual root node, like the Library left pane.
      parentGuid = parent.bookmarkGuid;
    }

    let focusedElement = aInitInfo.focusedElement;
    let onPanelReady = aInitInfo.onPanelReady;

    return (this._paneInfo = {
      itemGuid,
      parentGuid,
      isItem,
      isURI,
      uri,
      title,
      isBookmark,
      isFolderShortcut,
      isParentReadOnly,
      bulkTagging,
      uris,
      visibleRows,
      postData,
      isTag,
      focusedElement,
      onPanelReady,
      tag,
    });
  },

  get initialized() {
    return this._paneInfo != null;
  },

  /**
   * The concrete bookmark GUID is either the bookmark one or, for folder
   * shortcuts, the target one.
   *
   * @returns {string} GUID of the loaded bookmark, or null if not a bookmark.
   */
  get concreteGuid() {
    if (
      !this.initialized ||
      this._paneInfo.isTag ||
      this._paneInfo.bulkTagging
    ) {
      return null;
    }
    return this._paneInfo.itemGuid;
  },

  get uri() {
    if (!this.initialized) {
      return null;
    }
    if (this._paneInfo.bulkTagging) {
      return this._paneInfo.uris[0];
    }
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
    return (
      !this.initialized ||
      this._paneInfo.isFolderShortcut ||
      (!this._paneInfo.isItem && !this._paneInfo.isTag) ||
      (this._paneInfo.isParentReadOnly &&
        !this._paneInfo.isBookmark &&
        !this._paneInfo.isTag)
    );
  },

  get didChangeFolder() {
    return this._didChangeFolder;
  },

  // the first field which was edited after this panel was initialized for
  // a certain item
  _firstEditedField: "",

  _initNamePicker() {
    if (this._paneInfo.bulkTagging) {
      throw new Error("_initNamePicker called unexpectedly");
    }

    // title may by null, which, for us, is the same as an empty string.
    this._initTextField(
      this._namePicker,
      this._paneInfo.title || this._paneInfo.tag || ""
    );
  },

  _initLocationField() {
    if (!this._paneInfo.isURI) {
      throw new Error("_initLocationField called unexpectedly");
    }
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
      await PlacesUtils.keywords.fetch({ url: this._paneInfo.uri.spec }, e =>
        entries.push(e)
      );
      if (entries.length) {
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

  async _initAllTags() {
    this._allTags = new Map();
    const fetchedTags = await PlacesUtils.bookmarks.fetchTags();
    for (const tag of fetchedTags) {
      this._allTags?.set(tag.name.toLowerCase(), tag.name);
    }
  },

  /**
   * Initialize the panel.
   *
   * @param {object} aInfo
   *   The initialization info.
   * @param {object} [aInfo.node]
   *   If aInfo.uris is not specified, this must be specified.
   *   Either a result node or a node-like object representing the item to be edited.
   *   A node-like object must have the following properties (with values that
   *   match exactly those a result node would have):
   *   bookmarkGuid, uri, title, type, …
   * @param {nsIURI[]} [aInfo.uris]
   *   If aInfo.node is not specified, this must be specified.
   *   An array of uris for bulk tagging.
   * @param {string[]} [aInfo.hiddenRows]
   *   List of rows to be hidden regardless of the item edited. Possible values:
   *   "title", "location", "keyword", "folderPicker".
   */
  async initPanel(aInfo) {
    const deferred = (this._initPanelDeferred = PromiseUtils.defer());
    try {
      if (typeof aInfo != "object" || aInfo === null) {
        throw new Error("aInfo must be an object.");
      }
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
      if (this.initialized) {
        this.uninitPanel(false);
      }

      this._didChangeFolder = false;
      this.transactionPromises = [];

      let {
        parentGuid,
        isItem,
        isURI,
        isBookmark,
        bulkTagging,
        uris,
        visibleRows,
        focusedElement,
        onPanelReady,
      } = this._setPaneInfo(aInfo);

      // initPanel can be called multiple times in a row,
      // and awaits Promises. If the reference to `instance`
      // changes, it must mean another caller has called
      // initPanel again, so bail out of the initialization.
      let instance = (this._instance = {});

      // If we're creating a new item on the toolbar, show it:
      if (
        aInfo.isNewBookmark &&
        parentGuid == PlacesUtils.bookmarks.toolbarGuid
      ) {
        this._autoshowBookmarksToolbar();
      }

      let showOrCollapse = (
        rowId,
        isAppropriateForInput,
        nameInHiddenRows = null
      ) => {
        let visible = isAppropriateForInput;
        if (visible && "hiddenRows" in aInfo && nameInHiddenRows) {
          visible &= !aInfo.hiddenRows.includes(nameInHiddenRows);
        }
        if (visible) {
          visibleRows.add(rowId);
        }
        const cells = document.getElementsByClassName("editBMPanel_" + rowId);
        for (const cell of cells) {
          cell.hidden = !visible;
        }
        return visible;
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
        await this._initKeywordField().catch(console.error);
        // paneInfo can be null if paneInfo is uninitialized while
        // the process above is awaiting initialization
        if (instance != this._instance || this._paneInfo == null) {
          return;
        }
        this._keywordField.readOnly = this.readOnly;
      }

      // Collapse the tag selector if the item does not accept tags.
      if (showOrCollapse("tagsRow", isBookmark || bulkTagging, "tags")) {
        this._initTagsField();
      } else if (!this._element("tagsSelectorRow").hidden) {
        this.toggleTagsSelector().catch(console.error);
      }

      // Folder picker.
      // Technically we should check that the item is not moveable, but that's
      // not cheap (we don't always have the parent), and there's no use case for
      // this (it's only the Star UI that shows the folderPicker)
      if (showOrCollapse("folderRow", isItem, "folderPicker")) {
        await this._initFolderMenuList(parentGuid).catch(console.error);
        if (instance != this._instance || this._paneInfo == null) {
          return;
        }
      }

      // Selection count.
      if (showOrCollapse("selectionCount", bulkTagging)) {
        document.l10n.setAttributes(
          this._element("itemsCountText"),
          "places-details-pane-items-count",
          { count: uris.length }
        );
      }

      // Observe changes.
      if (!this._observersAdded) {
        this.handlePlacesEvents = this.handlePlacesEvents.bind(this);
        PlacesUtils.observers.addListener(
          ["bookmark-title-changed"],
          this.handlePlacesEvents
        );
        window.addEventListener("unload", this);
        this._observersAdded = true;
      }

      let focusElement = () => {
        // The focusedElement possible values are:
        //  * preferred: focus the field that the user touched first the last
        //    time the pane was shown (either namePicker or tagsField)
        //  * first: focus the first non hidden input
        // Note: since all controls are hidden by default, we don't get the
        // default XUL dialog behavior, that selects the first control, so we set
        // the focus explicitly.

        let elt;
        if (focusedElement === "preferred") {
          elt = this._element(
            Services.prefs.getCharPref(
              "browser.bookmarks.editDialog.firstEditField"
            )
          );
          if (elt.parentNode.hidden) {
            focusedElement = "first";
          }
        }
        if (focusedElement === "first") {
          elt = document
            .getElementById("editBookmarkPanelContent")
            .querySelector('input:not([hidden="true"])');
        }

        if (elt) {
          elt.focus({ preventScroll: true });
          elt.select();
        }
      };

      if (onPanelReady) {
        onPanelReady(focusElement);
      } else {
        focusElement();
      }

      if (this._updateTagsDeferred) {
        await this._updateTagsDeferred.promise;
      }

      this._bookmarkState = this.makeNewStateObject({
        children: aInfo.node?.children,
        index: aInfo.node?.index,
        isFolder: aInfo.node != null && PlacesUtils.nodeIsFolder(aInfo.node),
      });
      if (isBookmark || bulkTagging) {
        await this._initAllTags();
        await this._rebuildTagsSelectorList();
      }
    } finally {
      deferred.resolve();
      if (this._initPanelDeferred === deferred) {
        // Since change listeners check _initPanelDeferred for truthiness, we
        // can prevent unnecessary awaits by setting it back to null.
        this._initPanelDeferred = null;
      }
    }
  },

  /**
   * Finds tags that are in common among this._currentInfo.uris;
   *
   * @returns {string[]}
   */
  _getCommonTags() {
    if ("_cachedCommonTags" in this._paneInfo) {
      return this._paneInfo._cachedCommonTags;
    }

    let uris = [...this._paneInfo.uris];
    let firstURI = uris.shift();
    let commonTags = new Set(PlacesUtils.tagging.getTagsForURI(firstURI));
    if (commonTags.size == 0) {
      return (this._cachedCommonTags = []);
    }

    for (let uri of uris) {
      let curentURITags = PlacesUtils.tagging.getTagsForURI(uri);
      for (let tag of commonTags) {
        if (!curentURITags.includes(tag)) {
          commonTags.delete(tag);
          if (commonTags.size == 0) {
            return (this._paneInfo.cachedCommonTags = []);
          }
        }
      }
    }
    return (this._paneInfo._cachedCommonTags = [...commonTags]);
  },

  _initTextField(aElement, aValue) {
    if (aElement.value != aValue) {
      aElement.value = aValue;

      // Clear the editor's undo stack
      // FYI: editor may be null.
      aElement.editor?.clearUndoRedo();
    }
  },

  /**
   * Appends a menu-item representing a bookmarks folder to a menu-popup.
   *
   * @param {DOMElement} aMenupopup
   *   The popup to which the menu-item should be added.
   * @param {string} aFolderGuid
   *   The identifier of the bookmarks folder.
   * @param {string} aTitle
   *   The title to use as a label.
   * @returns {DOMElement}
   *   The new menu item.
   */
  _appendFolderItemToMenupopup(aMenupopup, aFolderGuid, aTitle) {
    // First make sure the folders-separator is visible
    this._element("foldersSeparator").hidden = false;

    var folderMenuItem = document.createXULElement("menuitem");
    folderMenuItem.folderGuid = aFolderGuid;
    folderMenuItem.setAttribute("label", aTitle);
    folderMenuItem.className = "menuitem-iconic folder-icon";
    aMenupopup.appendChild(folderMenuItem);
    return folderMenuItem;
  },

  async _initFolderMenuList(aSelectedFolderGuid) {
    // clean up first
    var menupopup = this._folderMenuList.menupopup;
    while (menupopup.children.length > 6) {
      menupopup.removeChild(menupopup.lastElementChild);
    }

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
    let lastUsedFolderGuids = await PlacesUtils.metadata.get(
      PlacesUIUtils.LAST_USED_FOLDERS_META_KEY,
      []
    );

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

    var numberOfItems = Math.min(
      PlacesUIUtils.maxRecentFolders,
      this._recentFolders.length
    );
    for (let i = 0; i < numberOfItems; i++) {
      await this._appendFolderItemToMenupopup(
        menupopup,
        this._recentFolders[i].guid,
        this._recentFolders[i].title
      );
    }

    let title = (await PlacesUtils.bookmarks.fetch(aSelectedFolderGuid)).title;
    var defaultItem = this._getFolderMenuItem(aSelectedFolderGuid, title);
    this._folderMenuList.selectedItem = defaultItem;
    // Ensure the selectedGuid attribute is set correctly (the above line wouldn't
    // necessary trigger a select event, so handle it manually, then add the
    // listener).
    this._onFolderListSelected();

    this._folderMenuList.addEventListener("select", this);
    this._folderMenuListListenerAdded = true;

    // Hide the folders-separator if no folder is annotated as recently-used
    this._element("foldersSeparator").hidden = menupopup.children.length <= 6;
    this._folderMenuList.disabled = this.readOnly;
  },

  _onFolderListSelected() {
    // Set a selectedGuid attribute to show special icons
    let folderGuid = this.selectedFolderGuid;
    if (folderGuid) {
      this._folderMenuList.setAttribute("selectedGuid", folderGuid);
    } else {
      this._folderMenuList.removeAttribute("selectedGuid");
    }
  },

  _element(aID) {
    return document.getElementById("editBMPanel_" + aID);
  },

  uninitPanel(aHideCollapsibleElements) {
    if (aHideCollapsibleElements) {
      // Hide the folder tree if it was previously visible.
      var folderTreeRow = this._element("folderTreeRow");
      if (!folderTreeRow.hidden) {
        this.toggleFolderTreeVisibility();
      }

      // Hide the tag selector if it was previously visible.
      var tagsSelectorRow = this._element("tagsSelectorRow");
      if (!tagsSelectorRow.hidden) {
        this.toggleTagsSelector().catch(console.error);
      }
    }

    if (this._observersAdded) {
      PlacesUtils.observers.removeListener(
        ["bookmark-title-changed"],
        this.handlePlacesEvents
      );
      window.removeEventListener("unload", this);
      this._observersAdded = false;
    }

    if (this._folderMenuListListenerAdded) {
      this._folderMenuList.removeEventListener("select", this);
      this._folderMenuListListenerAdded = false;
    }

    this._setPaneInfo(null);
    this._firstEditedField = "";
    this._didChangeFolder = false;
    this.transactionPromises = [];
    this._bookmarkState = null;
    this._allTags = null;
  },

  get selectedFolderGuid() {
    return (
      this._folderMenuList.selectedItem &&
      this._folderMenuList.selectedItem.folderGuid
    );
  },

  makeNewStateObject(extraOptions) {
    if (
      this._paneInfo.isItem ||
      this._paneInfo.isTag ||
      this._paneInfo.bulkTagging
    ) {
      const isLibraryWindow =
        document.documentElement.getAttribute("windowtype") ===
        "Places:Organizer";
      const options = {
        autosave: isLibraryWindow,
        info: this._paneInfo,
        ...extraOptions,
      };

      if (this._paneInfo.isBookmark) {
        options.tags = this._element("tagsField").value;
        options.keyword = this._keyword;
      }

      if (this._paneInfo.bulkTagging) {
        options.tags = this._element("tagsField").value;
      }

      return new PlacesUIUtils.BookmarkState(options);
    }
    return null;
  },

  async onTagsFieldChange() {
    // Check for _paneInfo existing as the dialog may be closing but receiving
    // async updates from unresolved promises.
    if (
      this._paneInfo &&
      (this._paneInfo.isURI || this._paneInfo.bulkTagging)
    ) {
      if (this._initPanelDeferred) {
        await this._initPanelDeferred.promise;
      }
      this._updateTags().then(() => {
        // Check _paneInfo here as we might be closing the dialog.
        if (this._paneInfo) {
          this._mayUpdateFirstEditField("tagsField");
        }
      }, console.error);
    }
  },

  /**
   * Handle tag list updates from the input field or selector box.
   */
  async _updateTags() {
    const deferred = (this._updateTagsDeferred = PromiseUtils.defer());
    try {
      const inputTags = this._getTagsArrayFromTagsInputField();
      const isLibraryWindow =
        document.documentElement.getAttribute("windowtype") ===
        "Places:Organizer";
      await this._bookmarkState._tagsChanged(inputTags);

      if (isLibraryWindow) {
        // Ensure the tagsField is in sync, clean it up from empty tags
        delete this._paneInfo._cachedCommonTags;
        const currentTags = this._paneInfo.bulkTagging
          ? this._getCommonTags()
          : PlacesUtils.tagging.getTagsForURI(this._paneInfo.uri);
        this._initTextField(this._tagsField, currentTags.join(", "), false);
        await this._initAllTags();
      } else {
        // Autosave is disabled. Update _allTags in memory so that the selector
        // list shows any new tags that haven't been saved yet.
        inputTags.forEach(tag => this._allTags?.set(tag.toLowerCase(), tag));
      }
      await this._rebuildTagsSelectorList();
    } finally {
      deferred.resolve();
      if (this._updateTagsDeferred === deferred) {
        // Since initPanel() checks _updateTagsDeferred for truthiness, we can
        // prevent unnecessary awaits by setting it back to null.
        this._updateTagsDeferred = null;
      }
    }
  },

  /**
   * Stores the first-edit field for this dialog, if the passed-in field
   * is indeed the first edited field.
   *
   * @param {string} aNewField
   *   The id of the field that may be set (without the "editBMPanel_" prefix).
   */
  _mayUpdateFirstEditField(aNewField) {
    // * The first-edit-field behavior is not applied in the multi-edit case
    // * if this._firstEditedField is already set, this is not the first field,
    //   so there's nothing to do
    if (this._paneInfo.bulkTagging || this._firstEditedField) {
      return;
    }

    this._firstEditedField = aNewField;

    // set the pref
    Services.prefs.setCharPref(
      "browser.bookmarks.editDialog.firstEditField",
      aNewField
    );
  },

  async onNamePickerChange() {
    if (this.readOnly || !(this._paneInfo.isItem || this._paneInfo.isTag)) {
      return;
    }
    if (this._initPanelDeferred) {
      await this._initPanelDeferred.promise;
    }

    // Here we update either the item title or its cached static title
    if (this._paneInfo.isTag) {
      let tag = this._namePicker.value;
      if (!tag || tag.includes("&")) {
        // We don't allow setting an empty title for a tag, restore the old one.
        this._initNamePicker();
        return;
      }

      this._bookmarkState._titleChanged(tag);
      return;
    }
    this._mayUpdateFirstEditField("namePicker");
    this._bookmarkState._titleChanged(this._namePicker.value);
  },

  async onLocationFieldChange() {
    if (this.readOnly || !this._paneInfo.isBookmark) {
      return;
    }
    if (this._initPanelDeferred) {
      await this._initPanelDeferred.promise;
    }

    let newURI;
    try {
      newURI = Services.uriFixup.getFixupURIInfo(
        this._locationField.value
      ).preferredURI;
    } catch (ex) {
      // TODO: Bug 1089141 - Provide some feedback about the invalid url.
      return;
    }

    if (this._paneInfo.uri.equals(newURI)) {
      return;
    }
    this._bookmarkState._locationChanged(newURI.spec);
  },

  async onKeywordFieldChange() {
    if (this.readOnly || !this._paneInfo.isBookmark) {
      return;
    }
    if (this._initPanelDeferred) {
      await this._initPanelDeferred.promise;
    }
    this._bookmarkState._keywordChanged(this._keywordField.value);
  },

  toggleFolderTreeVisibility() {
    let expander = this._element("foldersExpander");
    let folderTreeRow = this._element("folderTreeRow");
    let wasHidden = folderTreeRow.hidden;
    expander.classList.toggle("expander-up", wasHidden);
    expander.classList.toggle("expander-down", !wasHidden);
    if (!wasHidden) {
      document.l10n.setAttributes(
        expander,
        "bookmark-overlay-folders-expander2"
      );
      folderTreeRow.hidden = true;
      this._element("chooseFolderSeparator").hidden = this._element(
        "chooseFolderMenuItem"
      ).hidden = false;
      // Stop editing if we were (will no-op if not). This avoids permanently
      // breaking the tree if/when it is reshown.
      this._folderTree.stopEditing(false);
      // Unlinking the view will break the connection with the result. We don't
      // want to pay for live updates while the view is not visible.
      this._folderTree.view = null;
    } else {
      document.l10n.setAttributes(
        expander,
        "bookmark-overlay-folders-expander-hide"
      );
      folderTreeRow.hidden = false;

      // XXXmano: Ideally we would only do this once, but for some odd reason,
      // the editable mode set on this tree, together with its hidden state
      // breaks the view.
      const FOLDER_TREE_PLACE_URI =
        "place:excludeItems=1&excludeQueries=1&type=" +
        Ci.nsINavHistoryQueryOptions.RESULTS_AS_ROOTS_QUERY;
      this._folderTree.place = FOLDER_TREE_PLACE_URI;

      this._element("chooseFolderSeparator").hidden = this._element(
        "chooseFolderMenuItem"
      ).hidden = true;
      this._folderTree.selectItems([this._bookmarkState.parentGuid]);
      this._folderTree.focus();
    }
  },

  /**
   * Get the corresponding menu-item in the folder-menu-list for a bookmarks
   * folder if such an item exists. Otherwise, this creates a menu-item for the
   * folder. If the items-count limit (see
   * browser.bookmarks.editDialog.maxRecentFolders preference) is reached, the
   * new item replaces the last menu-item.
   *
   * @param {string} aFolderGuid
   *   The identifier of the bookmarks folder.
   * @param {string} aTitle
   *   The title to use in case of menuitem creation.
   * @returns {DOMElement}
   *   The handle to the menuitem.
   */
  _getFolderMenuItem(aFolderGuid, aTitle) {
    let menupopup = this._folderMenuList.menupopup;
    let menuItem = Array.prototype.find.call(
      menupopup.children,
      item => item.folderGuid === aFolderGuid
    );
    if (menuItem !== undefined) {
      return menuItem;
    }

    // 3 special folders + separator + folder-items-count limit
    if (menupopup.children.length == 4 + PlacesUIUtils.maxRecentFolders) {
      menupopup.removeChild(menupopup.lastElementChild);
    }

    return this._appendFolderItemToMenupopup(menupopup, aFolderGuid, aTitle);
  },

  async onFolderMenuListCommand(aEvent) {
    // Check for _paneInfo existing as the dialog may be closing but receiving
    // async updates from unresolved promises.
    if (!this._paneInfo) {
      return;
    }

    if (aEvent.target.id == "editBMPanel_chooseFolderMenuItem") {
      // reset the selection back to where it was and expand the tree
      // (this menu-item is hidden when the tree is already visible
      let item = this._getFolderMenuItem(
        this._bookmarkState._originalState.parentGuid,
        this._bookmarkState._originalState.title
      );
      this._folderMenuList.selectedItem = item;
      // XXXmano HACK: setTimeout 100, otherwise focus goes back to the
      // menulist right away
      setTimeout(() => this.toggleFolderTreeVisibility(), 100);
      return;
    }

    // Move the item
    let containerGuid = this._folderMenuList.selectedItem.folderGuid;
    if (this._bookmarkState.parentGuid != containerGuid) {
      this._bookmarkState._parentGuidChanged(containerGuid);

      // Auto-show the bookmarks toolbar when adding / moving an item there.
      if (containerGuid == PlacesUtils.bookmarks.toolbarGuid) {
        this._autoshowBookmarksToolbar();
      }

      // Unless the user cancels the panel, we'll use the chosen folder as
      // the default for new bookmarks.
      this._didChangeFolder = true;
    }

    // Update folder-tree selection
    var folderTreeRow = this._element("folderTreeRow");
    if (!folderTreeRow.hidden) {
      var selectedNode = this._folderTree.selectedNode;
      if (
        !selectedNode ||
        PlacesUtils.getConcreteItemGuid(selectedNode) != containerGuid
      ) {
        this._folderTree.selectItems([containerGuid]);
      }
    }
  },

  _autoshowBookmarksToolbar() {
    let neverShowToolbar =
      Services.prefs.getCharPref(
        "browser.toolbars.bookmarks.visibility",
        "newtab"
      ) == "never";
    let toolbar = document.getElementById("PersonalToolbar");
    if (!toolbar.collapsed || neverShowToolbar) {
      return;
    }

    let placement = CustomizableUI.getPlacementOfWidget("personal-bookmarks");
    let area = placement && placement.area;
    if (area != CustomizableUI.AREA_BOOKMARKS) {
      return;
    }

    // Show the toolbar but don't persist it permanently open
    setToolbarVisibility(toolbar, true, false);
  },

  onFolderTreeSelect() {
    // Ignore this event when the folder tree is hidden, even if the tree is
    // alive, it's clearly not a user activated action.
    if (this._element("folderTreeRow").hidden) {
      return;
    }

    var selectedNode = this._folderTree.selectedNode;

    // Disable the "New Folder" button if we cannot create a new folder
    this._element("newFolderButton").disabled =
      !this._folderTree.insertionPoint || !selectedNode;

    if (!selectedNode) {
      return;
    }

    var folderGuid = PlacesUtils.getConcreteItemGuid(selectedNode);
    if (this._folderMenuList.selectedItem.folderGuid == folderGuid) {
      return;
    }

    var folderItem = this._getFolderMenuItem(folderGuid, selectedNode.title);
    this._folderMenuList.selectedItem = folderItem;
    folderItem.doCommand();
  },

  async _rebuildTagsSelectorList() {
    let tagsSelector = this._element("tagsSelector");
    let tagsSelectorRow = this._element("tagsSelectorRow");
    if (tagsSelectorRow.hidden) {
      return;
    }

    let selectedIndex = tagsSelector.selectedIndex;
    let selectedTag =
      selectedIndex >= 0 ? tagsSelector.selectedItem.label : null;

    while (tagsSelector.hasChildNodes()) {
      tagsSelector.removeChild(tagsSelector.lastElementChild);
    }

    let tagsInField = this._getTagsArrayFromTagsInputField();

    let fragment = document.createDocumentFragment();
    let sortedTags = this._allTags ? [...this._allTags.values()].sort() : [];

    for (let i = 0; i < sortedTags.length; i++) {
      let tag = sortedTags[i];
      let elt = document.createXULElement("richlistitem");
      elt.appendChild(document.createXULElement("image"));
      let label = document.createXULElement("label");
      label.setAttribute("value", tag);
      elt.appendChild(label);
      if (tagsInField.includes(tag)) {
        elt.setAttribute("checked", "true");
      }
      fragment.appendChild(elt);
      if (selectedTag === tag) {
        selectedIndex = i;
      }
    }
    tagsSelector.appendChild(fragment);

    if (selectedIndex >= 0 && tagsSelector.itemCount > 0) {
      selectedIndex = Math.min(selectedIndex, tagsSelector.itemCount - 1);
      tagsSelector.selectedIndex = selectedIndex;
      tagsSelector.ensureIndexIsVisible(selectedIndex);
    }
    let event = new CustomEvent("BookmarkTagsSelectorUpdated", {
      bubbles: true,
    });
    tagsSelector.dispatchEvent(event);
  },

  async toggleTagsSelector() {
    var tagsSelector = this._element("tagsSelector");
    var tagsSelectorRow = this._element("tagsSelectorRow");
    var expander = this._element("tagsSelectorExpander");
    expander.classList.toggle("expander-up", tagsSelectorRow.hidden);
    expander.classList.toggle("expander-down", !tagsSelectorRow.hidden);
    if (tagsSelectorRow.hidden) {
      document.l10n.setAttributes(
        expander,
        "bookmark-overlay-tags-expander-hide"
      );
      tagsSelectorRow.hidden = false;
      await this._rebuildTagsSelectorList();

      // This is a no-op if we've added the listener.
      tagsSelector.addEventListener("mousedown", this);
      tagsSelector.addEventListener("keypress", this);
    } else {
      document.l10n.setAttributes(expander, "bookmark-overlay-tags-expander2");
      tagsSelectorRow.hidden = true;

      // This is a no-op if we've removed the listener.
      tagsSelector.removeEventListener("mousedown", this);
      tagsSelector.removeEventListener("keypress", this);
    }
  },

  /**
   * Splits "tagsField" element value, returning an array of valid tag strings.
   *
   * @returns {string[]}
   *   Array of tag strings found in the field value.
   */
  _getTagsArrayFromTagsInputField() {
    let tags = this._element("tagsField").value;
    return tags
      .trim()
      .split(/\s*,\s*/) // Split on commas and remove spaces.
      .filter(tag => !!tag.length); // Kill empty tags.
  },

  async newFolder() {
    let ip = this._folderTree.insertionPoint;

    // default to the bookmarks menu folder
    if (!ip) {
      ip = new PlacesInsertionPoint({
        parentGuid: PlacesUtils.bookmarks.menuGuid,
      });
    }

    // XXXmano: add a separate "New Folder" string at some point...
    let title = this._element("newFolderButton").label;
    let promise = PlacesTransactions.NewFolder({
      parentGuid: ip.guid,
      title,
      index: await ip.getIndex(),
    }).transact();
    this.transactionPromises.push(promise.catch(console.error));
    let guid = await promise;

    this._folderTree.focus();
    this._folderTree.selectItems([ip.guid]);
    PlacesUtils.asContainer(this._folderTree.selectedNode).containerOpen = true;
    this._folderTree.selectItems([guid]);
    this._folderTree.startEditing(
      this._folderTree.view.selection.currentIndex,
      this._folderTree.columns.getFirstColumn()
    );
  },

  // EventListener
  handleEvent(event) {
    switch (event.type) {
      case "mousedown":
        if (event.button == 0) {
          // Make sure the event is triggered on an item and not the empty space.
          let item = event.target.closest("richlistbox,richlistitem");
          if (item.localName == "richlistitem") {
            this.toggleItemCheckbox(item);
          }
        }
        break;
      case "keypress":
        if (event.key == " ") {
          let item = event.target.currentItem;
          if (item) {
            this.toggleItemCheckbox(item);
          }
        }
        break;
      case "unload":
        this.uninitPanel(false);
        break;
      case "select":
        this._onFolderListSelected();
        break;
    }
  },

  async handlePlacesEvents(events) {
    for (const event of events) {
      switch (event.type) {
        case "bookmark-title-changed":
          if (this._paneInfo.isItem || this._paneInfo.isTag) {
            // This also updates titles of folders in the folder menu list.
            this._onItemTitleChange(event.id, event.title, event.guid);
          }
          break;
      }
    }
  },

  toggleItemCheckbox(item) {
    // Update the tags field when items are checked/unchecked in the listbox
    let tags = this._getTagsArrayFromTagsInputField();

    let curTagIndex = tags.indexOf(item.label);
    let tagsSelector = this._element("tagsSelector");
    tagsSelector.selectedItem = item;

    if (!item.hasAttribute("checked")) {
      item.setAttribute("checked", "true");
      if (curTagIndex == -1) {
        tags.push(item.label);
      }
    } else {
      item.removeAttribute("checked");
      if (curTagIndex != -1) {
        tags.splice(curTagIndex, 1);
      }
    }
    this._element("tagsField").value = tags.join(", ");
    this._updateTags();
  },

  _initTagsField() {
    let tags;
    if (this._paneInfo.isURI) {
      tags = PlacesUtils.tagging.getTagsForURI(this._paneInfo.uri);
    } else if (this._paneInfo.bulkTagging) {
      tags = this._getCommonTags();
    } else {
      throw new Error("_promiseTagsStr called unexpectedly");
    }

    this._initTextField(this._tagsField, tags.join(", "));
  },

  _onItemTitleChange(aItemId, aNewTitle, aGuid) {
    if (this._paneInfo.visibleRows.has("folderRow")) {
      // If the title of a folder which is listed within the folders
      // menulist has been changed, we need to update the label of its
      // representing element.
      let menupopup = this._folderMenuList.menupopup;
      for (let menuitem of menupopup.children) {
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

  /**
   * State object for the bookmark(s) currently being edited.
   *
   * @returns {BookmarkState} The bookmark state.
   */
  get bookmarkState() {
    return this._bookmarkState;
  },
};

ChromeUtils.defineLazyGetter(gEditItemOverlay, "_folderTree", () => {
  if (!customElements.get("places-tree")) {
    Services.scriptloader.loadSubScript(
      "chrome://browser/content/places/places-tree.js",
      window
    );
  }
  gEditItemOverlay._element("folderTreeRow").prepend(
    MozXULElement.parseXULToFragment(`
    <tree id="editBMPanel_folderTree"
          class="placesTree"
          is="places-tree"
          editable="true"
          onselect="gEditItemOverlay.onFolderTreeSelect();"
          disableUserActions="true"
          hidecolumnpicker="true">
      <treecols>
        <treecol anonid="title" flex="1" primary="true" hideheader="true"/>
      </treecols>
      <treechildren flex="1"/>
    </tree>
  `)
  );
  return gEditItemOverlay._element("folderTree");
});

for (let elt of [
  "folderMenuList",
  "namePicker",
  "locationField",
  "keywordField",
  "tagsField",
]) {
  let eltScoped = elt;
  ChromeUtils.defineLazyGetter(gEditItemOverlay, `_${eltScoped}`, () =>
    gEditItemOverlay._element(eltScoped)
  );
}
