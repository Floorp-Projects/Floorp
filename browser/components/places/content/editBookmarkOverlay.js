/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Places Bookmark Properties dialog.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Asaf Romano <mano@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const LAST_USED_ANNO = "bookmarkPropertiesDialog/folderLastUsed";
const STATIC_TITLE_ANNO = "bookmarks/staticTitle";
const MAX_FOLDER_ITEM_IN_MENU_LIST = 5;

var gEditItemOverlay = {
  _uri: null,
  _itemId: -1,
  _itemIds: [],
  _uris: [],
  _tags: [],
  _allTags: [],
  _multiEdit: false,
  _itemType: -1,
  _readOnly: false,
  _microsummaries: null,
  _hiddenRows: [],
  _observersAdded: false,
  _staticFoldersListBuilt: false,
  _initialized: false,

  // the first field which was edited after this panel was initialized for
  // a certain item
  _firstEditedField: "",

  get itemId() {
    return this._itemId;
  },

  get uri() {
    return this._uri;
  },

  get multiEdit() {
    return this._multiEdit;
  },

  /**
   * Determines the initial data for the item edited or added by this dialog
   */
  _determineInfo: function EIO__determineInfo(aInfo) {
    // hidden rows
    if (aInfo && aInfo.hiddenRows)
      this._hiddenRows = aInfo.hiddenRows;
    else
      this._hiddenRows.splice(0);
    // force-read-only
    this._readOnly = aInfo && aInfo.forceReadOnly;
  },

  _showHideRows: function EIO__showHideRows() {
    var isBookmark = this._itemId != -1 &&
                     this._itemType == Ci.nsINavBookmarksService.TYPE_BOOKMARK;
    var isQuery = false;
    if (this._uri)
      isQuery = this._uri.schemeIs("place");

    this._element("nameRow").collapsed = this._hiddenRows.indexOf("name") != -1;
    this._element("folderRow").collapsed =
      this._hiddenRows.indexOf("folderPicker") != -1 || this._readOnly;
    this._element("tagsRow").collapsed = !this._uri ||
      this._hiddenRows.indexOf("tags") != -1 || isQuery;
    // Collapse the tag selector if the item does not accept tags.
    if (!this._element("tagsSelectorRow").collapsed &&
        this._element("tagsRow").collapsed)
      this.toggleTagsSelector();
    this._element("descriptionRow").collapsed =
      this._hiddenRows.indexOf("description") != -1 || this._readOnly;
    this._element("keywordRow").collapsed = !isBookmark || this._readOnly ||
      this._hiddenRows.indexOf("keyword") != -1 || isQuery;
    this._element("locationRow").collapsed = !(this._uri && !isQuery) ||
      this._hiddenRows.indexOf("location") != -1;
    this._element("loadInSidebarCheckbox").collapsed = !isBookmark || isQuery ||
      this._readOnly || this._hiddenRows.indexOf("loadInSidebar") != -1;
    this._element("feedLocationRow").collapsed = !this._isLivemark ||
      this._hiddenRows.indexOf("feedLocation") != -1;
    this._element("siteLocationRow").collapsed = !this._isLivemark ||
      this._hiddenRows.indexOf("siteLocation") != -1;
    this._element("selectionCount").hidden = !this._multiEdit;
  },

  /**
   * Initialize the panel
   * @param aFor
   *        Either a places-itemId (of a bookmark, folder or a live bookmark),
   *        an array of itemIds (used for bulk tagging), or a URI object (in 
   *        which case, the panel would be initialized in read-only mode).
   * @param [optional] aInfo
   *        JS object which stores additional info for the panel
   *        initialization. The following properties may bet set:
   *        * hiddenRows (Strings array): list of rows to be hidden regardless
   *          of the item edited. Possible values: "title", "location",
   *          "description", "keyword", "loadInSidebar", "feedLocation",
   *          "siteLocation", folderPicker"
   *        * forceReadOnly - set this flag to initialize the panel to its
   *          read-only (view) mode even if the given item is editable.
   */
  initPanel: function EIO_initPanel(aFor, aInfo) {
    // For sanity ensure that the implementer has uninited the panel before
    // trying to init it again, or we could end up leaking due to observers.
    if (this._initialized)
      this.uninitPanel(false);

    var aItemIdList;
    if (aFor.length) {
      aItemIdList = aFor;
      aFor = aItemIdList[0];
    }
    else if (this._multiEdit) {
      this._multiEdit = false;
      this._tags = [];
      this._uris = [];
      this._allTags = [];
      this._itemIds = [];
      this._element("selectionCount").hidden = true;
    }

    this._folderMenuList = this._element("folderMenuList");
    this._folderTree = this._element("folderTree");

    this._determineInfo(aInfo);
    if (aFor instanceof Ci.nsIURI) {
      this._itemId = -1;
      this._uri = aFor;
      this._readOnly = true;
    }
    else {
      this._itemId = aFor;
      var containerId = PlacesUtils.bookmarks.getFolderIdForItem(this._itemId);
      this._itemType = PlacesUtils.bookmarks.getItemType(this._itemId);
      if (this._itemType == Ci.nsINavBookmarksService.TYPE_BOOKMARK) {
        this._uri = PlacesUtils.bookmarks.getBookmarkURI(this._itemId);
        if (!this._readOnly) // If readOnly wasn't forced through aInfo
          this._readOnly = PlacesUtils.itemIsLivemark(containerId);
        this._initTextField("keywordField",
                            PlacesUtils.bookmarks
                                       .getKeywordForBookmark(this._itemId));
        // Load In Sidebar checkbox
        this._element("loadInSidebarCheckbox").checked =
          PlacesUtils.annotations.itemHasAnnotation(this._itemId,
                                                    PlacesUIUtils.LOAD_IN_SIDEBAR_ANNO);
      }
      else {
        if (!this._readOnly) // If readOnly wasn't forced through aInfo
          this._readOnly = false;

        this._uri = null;
        this._isLivemark = PlacesUtils.itemIsLivemark(this._itemId);
        if (this._isLivemark) {
          var feedURI = PlacesUtils.livemarks.getFeedURI(this._itemId);
          var siteURI = PlacesUtils.livemarks.getSiteURI(this._itemId);
          this._initTextField("feedLocationField", feedURI.spec);
          this._initTextField("siteLocationField", siteURI ? siteURI.spec : "");
        }
      }

      // folder picker
      this._initFolderMenuList(containerId);

      // description field
      this._initTextField("descriptionField", 
                          PlacesUIUtils.getItemDescription(this._itemId));
    }

    if (this._itemId == -1 ||
        this._itemType == Ci.nsINavBookmarksService.TYPE_BOOKMARK) {
      this._isLivemark = false;

      this._initTextField("locationField", this._uri.spec);
      if (!aItemIdList) {
        var tags = PlacesUtils.tagging.getTagsForURI(this._uri).join(", ");
        this._initTextField("tagsField", tags, false);
      }
      else {
        this._multiEdit = true;
        this._allTags = [];
        this._itemIds = aItemIdList;
        var nodeToCheck = 0;
        for (var i = 0; i < aItemIdList.length; i++) {
          if (aItemIdList[i] instanceof Ci.nsIURI) {
            this._uris[i] = aItemIdList[i];
            this._itemIds[i] = -1;
          }
          else
            this._uris[i] = PlacesUtils.bookmarks.getBookmarkURI(this._itemIds[i]);
          this._tags[i] = PlacesUtils.tagging.getTagsForURI(this._uris[i]);
          if (this._tags[i].length < this._tags[nodeToCheck].length)
            nodeToCheck =  i;
        }
        this._getCommonTags(nodeToCheck);
        this._initTextField("tagsField", this._allTags.join(", "), false);
        this._element("itemsCountText").value =
          PlacesUIUtils.getFormattedString("detailsPane.multipleItems",
                                           [this._itemIds.length]);
      }

      // tags selector
      this._rebuildTagsSelectorList();
    }

    // name picker
    this._initNamePicker();
    
    this._showHideRows();

    // observe changes
    if (!this._observersAdded) {
      if (this._itemId != -1)
        PlacesUtils.bookmarks.addObserver(this, false);
      window.addEventListener("unload", this, false);
      this._observersAdded = true;
    }

    this._initialized = true;
  },

  _getCommonTags: function(aArrIndex) {
    var tempArray = this._tags[aArrIndex];
    var isAllTag;
    for (var k = 0; k < tempArray.length; k++) {
      isAllTag = true;
      for (var j = 0; j < this._tags.length; j++) {
        if (j == aArrIndex)
          continue;
        if (this._tags[j].indexOf(tempArray[k]) == -1) {
          isAllTag = false;
          break;
        }
      }
      if (isAllTag)
        this._allTags.push(tempArray[k]);
    }
  },

  _initTextField: function(aTextFieldId, aValue, aReadOnly) {
    var field = this._element(aTextFieldId);
    field.readOnly = aReadOnly !== undefined ? aReadOnly : this._readOnly;

    if (field.value != aValue) {
      field.value = aValue;

      // clear the undo stack
      var editor = field.editor;
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
  _appendFolderItemToMenupopup:
  function EIO__appendFolderItemToMenuList(aMenupopup, aFolderId) {
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

  QueryInterface: function EIO_QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIMicrosummaryObserver) ||
        aIID.equals(Ci.nsIDOMEventListener) ||
        aIID.equals(Ci.nsINavBookmarkObserver) ||
        aIID.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  _element: function EIO__element(aID) {
    return document.getElementById("editBMPanel_" + aID);
  },

  _createMicrosummaryMenuItem:
  function EIO__createMicrosummaryMenuItem(aMicrosummary) {
    var menuItem = document.createElement("menuitem");

    // Store a reference to the microsummary in the menu item, so we know
    // which microsummary this menu item represents when it's time to
    // save changes or load its content.
    menuItem.microsummary = aMicrosummary;

    // Content may have to be generated asynchronously; we don't necessarily
    // have it now.  If we do, great; otherwise, fall back to the generator
    // name, then the URI, and we trigger a microsummary content update. Once
    // the update completes, the microsummary will notify our observer to
    // update the corresponding menu-item.
    // XXX Instead of just showing the generator name or (heaven forbid)
    // its URI when we don't have content, we should tell the user that
    // we're loading the microsummary, perhaps with some throbbing to let
    // her know it is in progress.
    if (aMicrosummary.content)
      menuItem.setAttribute("label", aMicrosummary.content);
    else {
      menuItem.setAttribute("label", aMicrosummary.generator.name ||
                                     aMicrosummary.generator.uri.spec);
      aMicrosummary.update();
    }

    return menuItem;
  },

  _getItemStaticTitle: function EIO__getItemStaticTitle() {
    if (this._itemId == -1)
      return PlacesUtils.history.getPageTitle(this._uri);

    const annos = PlacesUtils.annotations;
    if (annos.itemHasAnnotation(this._itemId, STATIC_TITLE_ANNO))
      return annos.getItemAnnotation(this._itemId, STATIC_TITLE_ANNO);

    return PlacesUtils.bookmarks.getItemTitle(this._itemId);
  },

  _initNamePicker: function EIO_initNamePicker() {
    var userEnteredNameField = this._element("userEnteredName");
    var namePicker = this._element("namePicker");
    var droppable = false;

    userEnteredNameField.label = this._getItemStaticTitle();

    // clean up old entries
    var menupopup = namePicker.menupopup;
    while (menupopup.childNodes.length > 2)
      menupopup.removeChild(menupopup.lastChild);

    if (this._microsummaries) {
      this._microsummaries.removeObserver(this);
      this._microsummaries = null;
    }

    var itemToSelect = userEnteredNameField;
    try {
      if (this._itemId != -1 &&
          this._itemType == Ci.nsINavBookmarksService.TYPE_BOOKMARK &&
          !this._readOnly)
        this._microsummaries = PlacesUtils.microsummaries
                                          .getMicrosummaries(this._uri, -1);
    }
    catch(ex) {
      // getMicrosummaries will throw an exception in at least two cases:
      // 1. the bookmarked URI contains a scheme that the service won't
      //    download for security reasons (currently it only handles http,
      //    https, and file);
      // 2. the page to which the URI refers isn't HTML or XML (the only two
      //    content types the service knows how to summarize).
      this._microsummaries = null;
    }

    if (this._microsummaries) {
      var enumerator = this._microsummaries.Enumerate();

      if (enumerator.hasMoreElements()) {
        // Show the drop marker if there are microsummaries
        droppable = true;
        while (enumerator.hasMoreElements()) {
          var microsummary = enumerator.getNext()
                                       .QueryInterface(Ci.nsIMicrosummary);
          var menuItem = this._createMicrosummaryMenuItem(microsummary);
          if (PlacesUtils.microsummaries
                         .isMicrosummary(this._itemId, microsummary))
            itemToSelect = menuItem;

          menupopup.appendChild(menuItem);
        }
      }

      this._microsummaries.addObserver(this);
    }

    if (namePicker.selectedItem == itemToSelect)
      namePicker.value = itemToSelect.label;
    else
      namePicker.selectedItem = itemToSelect;

    namePicker.setAttribute("droppable", droppable);
    namePicker.readOnly = this._readOnly;

    // clear the undo stack
    var editor = namePicker.editor;
    if (editor)
      editor.transactionManager.clear();
  },

  // nsIMicrosummaryObserver
  onContentLoaded: function EIO_onContentLoaded(aMicrosummary) {
    var namePicker = this._element("namePicker");
    var childNodes = namePicker.menupopup.childNodes;

    // 0: user-entered item; 1: separator
    for (var i = 2; i < childNodes.length; i++) {
      if (childNodes[i].microsummary == aMicrosummary) {
        var newLabel = aMicrosummary.content;
        // XXXmano: non-editable menulist would do this for us, see bug 360220
        // We should fix editable-menulists to set the DOMAttrModified handler
        // as well.
        //
        // Also note the order importance: if the label of the menu-item is
        // set to something different than the menulist's current value,
        // the menulist no longer has selectedItem set
        if (namePicker.selectedItem == childNodes[i])
          namePicker.value = newLabel;

        childNodes[i].label = newLabel;
        return;
      }
    }
  },

  onElementAppended: function EIO_onElementAppended(aMicrosummary) {
    var namePicker = this._element("namePicker");
    namePicker.menupopup
              .appendChild(this._createMicrosummaryMenuItem(aMicrosummary));

    // Make sure the drop-marker is shown
    namePicker.setAttribute("droppable", "true");
  },

  uninitPanel: function EIO_uninitPanel(aHideCollapsibleElements) {
    if (aHideCollapsibleElements) {
      // hide the folder tree if it was previously visible
      var folderTreeRow = this._element("folderTreeRow");
      if (!folderTreeRow.collapsed)
        this.toggleFolderTreeVisibility();

      // hide the tag selector if it was previously visible
      var tagsSelectorRow = this._element("tagsSelectorRow");
      if (!tagsSelectorRow.collapsed)
        this.toggleTagsSelector();
    }

    if (this._observersAdded) {
      if (this._itemId != -1)
        PlacesUtils.bookmarks.removeObserver(this);

      this._observersAdded = false;
    }
    if (this._microsummaries) {
      this._microsummaries.removeObserver(this);
      this._microsummaries = null;
    }
    this._itemId = -1;
    this._uri = null;
    this._uris = [];
    this._tags = [];
    this._allTags = [];
    this._itemIds = [];
    this._multiEdit = false;
    this._firstEditedField = "";
    this._initialized = false;
  },

  onTagsFieldBlur: function EIO_onTagsFieldBlur() {
    if (this._updateTags()) // if anything has changed
      this._mayUpdateFirstEditField("tagsField");
  },

  _updateTags: function EIO__updateTags() {
    if (this._multiEdit)
      return this._updateMultipleTagsForItems();
    return this._updateSingleTagForItem();
  },

  _updateSingleTagForItem: function EIO__updateSingleTagForItem() {
    var currentTags = PlacesUtils.tagging.getTagsForURI(this._uri);
    var tags = this._getTagsArrayFromTagField();
    if (tags.length > 0 || currentTags.length > 0) {
      var tagsToRemove = [];
      var tagsToAdd = [];
      var txns = []; 
      for (var i = 0; i < currentTags.length; i++) {
        if (tags.indexOf(currentTags[i]) == -1)
          tagsToRemove.push(currentTags[i]);
      }
      for (var i = 0; i < tags.length; i++) {
        if (currentTags.indexOf(tags[i]) == -1)
          tagsToAdd.push(tags[i]);
      }

      if (tagsToRemove.length > 0)
        txns.push(PlacesUIUtils.ptm.untagURI(this._uri, tagsToRemove));
      if (tagsToAdd.length > 0)
        txns.push(PlacesUIUtils.ptm.tagURI(this._uri, tagsToAdd));

      if (txns.length > 0) {
        var aggregate = PlacesUIUtils.ptm.aggregateTransactions("Update tags",
                                                                txns);
        PlacesUIUtils.ptm.doTransaction(aggregate);

        // Ensure the tagsField is in sync, clean it up from empty tags
        var tags = PlacesUtils.tagging.getTagsForURI(this._uri).join(", ");
        this._initTextField("tagsField", tags, false);
        return true;
      }
    }
    return false;
  },

   /**
    * Stores the first-edit field for this dialog, if the passed-in field
    * is indeed the first edited field
    * @param aNewField
    *        the id of the field that may be set (without the "editBMPanel_"
    *        prefix)
    */
  _mayUpdateFirstEditField: function EIO__mayUpdateFirstEditField(aNewField) {
    // * The first-edit-field behavior is not applied in the multi-edit case
    // * if this._firstEditedField is already set, this is not the first field,
    //   so there's nothing to do
    if (this._multiEdit || this._firstEditedField)
      return;

    this._firstEditedField = aNewField;

    // set the pref
    var prefs = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefBranch);
    prefs.setCharPref("browser.bookmarks.editDialog.firstEditField", aNewField);
  },

  _updateMultipleTagsForItems: function EIO__updateMultipleTagsForItems() {
    var tags = this._getTagsArrayFromTagField();
    if (tags.length > 0 || this._allTags.length > 0) {
      var tagsToRemove = [];
      var tagsToAdd = [];
      var txns = []; 
      for (var i = 0; i < this._allTags.length; i++) {
        if (tags.indexOf(this._allTags[i]) == -1)
          tagsToRemove.push(this._allTags[i]);
      }
      for (var i = 0; i < this._tags.length; i++) {
        tagsToAdd[i] = [];
        for (var j = 0; j < tags.length; j++) {
          if (this._tags[i].indexOf(tags[j]) == -1)
            tagsToAdd[i].push(tags[j]);
        }
      }

      if (tagsToAdd.length > 0) {
        for (i = 0; i < this._uris.length; i++) {
          if (tagsToAdd[i].length > 0)
            txns.push(PlacesUIUtils.ptm.tagURI(this._uris[i], tagsToAdd[i]));
        }
      }
      if (tagsToRemove.length > 0) {
        for (var i = 0; i < this._uris.length; i++)
          txns.push(PlacesUIUtils.ptm.untagURI(this._uris[i], tagsToRemove));
      }

      if (txns.length > 0) {
        var aggregate = PlacesUIUtils.ptm.aggregateTransactions("Update tags",
                                                                txns);
        PlacesUIUtils.ptm.doTransaction(aggregate);

        this._allTags = tags;
        this._tags = [];
        for (i = 0; i < this._uris.length; i++)
          this._tags[i] = PlacesUtils.tagging.getTagsForURI(this._uris[i]);

        // Ensure the tagsField is in sync, clean it up from empty tags
        this._initTextField("tagsField", tags, false);
        return true;
      }
    }
    return false;
  },

  onNamePickerInput: function EIO_onNamePickerInput() {
    var title = this._element("namePicker").value;
    this._element("userEnteredName").label = title;
  },

  onNamePickerChange: function EIO_onNamePickerChange() {
    if (this._itemId == -1)
      return;

    var namePicker = this._element("namePicker")
    var txns = [];
    const ptm = PlacesUIUtils.ptm;

    // Here we update either the item title or its cached static title
    var newTitle = this._element("userEnteredName").label;
    if (!newTitle &&
        PlacesUtils.bookmarks.getFolderIdForItem(this._itemId) == PlacesUtils.tagsFolderId) {
      // We don't allow setting an empty title for a tag, restore the old one.
      this._initNamePicker();
    }
    else if (this._getItemStaticTitle() != newTitle) {
      this._mayUpdateFirstEditField("namePicker");
      if (PlacesUtils.microsummaries.hasMicrosummary(this._itemId)) {
        // Note: this implicitly also takes care of the microsummary->static
        // title case, the removeMicorosummary method in the service will set
        // the item-title to the value of this annotation.
        //
        // XXXmano: use a transaction
        PlacesUtils.setAnnotationsForItem(this._itemId,
                                          [{name: STATIC_TITLE_ANNO,
                                            value: newTitle}]);
      }
      else
        txns.push(ptm.editItemTitle(this._itemId, newTitle));
    }

    var newMicrosummary = namePicker.selectedItem.microsummary;

    // Only add a microsummary update to the transaction if the microsummary
    // has actually changed, i.e. the user selected no microsummary, but the
    // bookmark previously had one, or the user selected a microsummary which
    // is not the one the bookmark previously had
    if ((newMicrosummary == null &&
         PlacesUtils.microsummaries.hasMicrosummary(this._itemId)) ||
        (newMicrosummary != null &&
         !PlacesUtils.microsummaries
                     .isMicrosummary(this._itemId, newMicrosummary))) {
      txns.push(ptm.editBookmarkMicrosummary(this._itemId, newMicrosummary));
    }

    var aggregate = ptm.aggregateTransactions("Edit Item Title", txns);
    ptm.doTransaction(aggregate);
  },

  onDescriptionFieldBlur: function EIO_onDescriptionFieldBlur() {
    var description = this._element("descriptionField").value;
    if (description != PlacesUIUtils.getItemDescription(this._itemId)) {
      var txn = PlacesUIUtils.ptm
                             .editItemDescription(this._itemId, description);
      PlacesUIUtils.ptm.doTransaction(txn);
    }
  },

  onLocationFieldBlur: function EIO_onLocationFieldBlur() {
    var uri;
    try {
      uri = PlacesUIUtils.createFixedURI(this._element("locationField").value);
    }
    catch(ex) { return; }

    if (!this._uri.equals(uri)) {
      var txn = PlacesUIUtils.ptm.editBookmarkURI(this._itemId, uri);
      PlacesUIUtils.ptm.doTransaction(txn);
      this._uri = uri;
    }
  },

  onKeywordFieldBlur: function EIO_onKeywordFieldBlur() {
    var keyword = this._element("keywordField").value;
    if (keyword != PlacesUtils.bookmarks.getKeywordForBookmark(this._itemId)) {
      var txn = PlacesUIUtils.ptm.editBookmarkKeyword(this._itemId, keyword);
      PlacesUIUtils.ptm.doTransaction(txn);
    }
  },

  onFeedLocationFieldBlur: function EIO_onFeedLocationFieldBlur() {
    var uri;
    try {
      uri = PlacesUIUtils.createFixedURI(this._element("feedLocationField").value);
    }
    catch(ex) { return; }

    var currentFeedURI = PlacesUtils.livemarks.getFeedURI(this._itemId);
    if (!currentFeedURI.equals(uri)) {
      var txn = PlacesUIUtils.ptm.editLivemarkFeedURI(this._itemId, uri);
      PlacesUIUtils.ptm.doTransaction(txn);
    }
  },

  onSiteLocationFieldBlur: function EIO_onSiteLocationFieldBlur() {
    var uri = null;
    try {
      uri = PlacesUIUtils.createFixedURI(this._element("siteLocationField").value);
    }
    catch(ex) {  }

    var currentSiteURI = PlacesUtils.livemarks.getSiteURI(this._itemId);
    if (!uri || !currentSiteURI.equals(uri)) {
      var txn = PlacesUIUtils.ptm.editLivemarkSiteURI(this._itemId, uri);
      PlacesUIUtils.ptm.doTransaction(txn);
    }
  },

  onLoadInSidebarCheckboxCommand:
  function EIO_onLoadInSidebarCheckboxCommand() {
    var loadInSidebarChecked = this._element("loadInSidebarCheckbox").checked;
    var txn = PlacesUIUtils.ptm.setLoadInSidebar(this._itemId,
                                                 loadInSidebarChecked);
    PlacesUIUtils.ptm.doTransaction(txn);
  },

  toggleFolderTreeVisibility: function EIO_toggleFolderTreeVisibility() {
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

  _getFolderIdFromMenuList:
  function EIO__getFolderIdFromMenuList() {
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
  _getFolderMenuItem:
  function EIO__getFolderMenuItem(aFolderId) {
    var menupopup = this._folderMenuList.menupopup;

    for (let i = 0; i < menupopup.childNodes.length; i++) {
      if ("folderId" in menupopup.childNodes[i] &&
          menupopup.childNodes[i].folderId == aFolderId)
        return menupopup.childNodes[i];
    }

    // 3 special folders + separator + folder-items-count limit
    if (menupopup.childNodes.length == 4 + MAX_FOLDER_ITEM_IN_MENU_LIST)
      menupopup.removeChild(menupopup.lastChild);

    return this._appendFolderItemToMenupopup(menupopup, aFolderId);
  },

  onFolderMenuListCommand: function EIO_onFolderMenuListCommand(aEvent) {
    // Set a selectedIndex attribute to show special icons
    this._folderMenuList.setAttribute("selectedIndex",
                                      this._folderMenuList.selectedIndex);

    if (aEvent.target.id == "editBMPanel_chooseFolderMenuItem") {
      // reset the selection back to where it was and expand the tree
      // (this menu-item is hidden when the tree is already visible
      var container = PlacesUtils.bookmarks.getFolderIdForItem(this._itemId);
      var item = this._getFolderMenuItem(container);
      this._folderMenuList.selectedItem = item;
      // XXXmano HACK: setTimeout 100, otherwise focus goes back to the
      // menulist right away
      setTimeout(function(self) self.toggleFolderTreeVisibility(), 100, this);
      return;
    }

    // Move the item
    var container = this._getFolderIdFromMenuList();
    if (PlacesUtils.bookmarks.getFolderIdForItem(this._itemId) != container) {
      var txn = PlacesUIUtils.ptm.moveItem(this._itemId, container, -1);
      PlacesUIUtils.ptm.doTransaction(txn);

      // Mark the containing folder as recently-used if it isn't in the
      // static list
      if (container != PlacesUtils.unfiledBookmarksFolderId &&
          container != PlacesUtils.toolbarFolderId &&
          container != PlacesUtils.bookmarksMenuFolderId)
        this._markFolderAsRecentlyUsed(container);
    }

    // Update folder-tree selection
    var folderTreeRow = this._element("folderTreeRow");
    if (!folderTreeRow.collapsed) {
      var selectedNode = this._folderTree.selectedNode;
      if (!selectedNode ||
          PlacesUtils.getConcreteItemId(selectedNode) != container)
        this._folderTree.selectItems([container]);
    }
  },

  onFolderTreeSelect: function EIO_onFolderTreeSelect() {
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

  _markFolderAsRecentlyUsed:
  function EIO__markFolderAsRecentlyUsed(aFolderId) {
    var txns = [];

    // Expire old unused recent folders
    var anno = this._getLastUsedAnnotationObject(false);
    while (this._recentFolders.length > MAX_FOLDER_ITEM_IN_MENU_LIST) {
      var folderId = this._recentFolders.pop().folderId;
      txns.push(PlacesUIUtils.ptm.setItemAnnotation(folderId, anno));
    }

    // Mark folder as recently used
    anno = this._getLastUsedAnnotationObject(true);
    txns.push(PlacesUIUtils.ptm.setItemAnnotation(aFolderId, anno));

    var aggregate = PlacesUIUtils.ptm.aggregateTransactions("Update last used folders", txns);
    PlacesUIUtils.ptm.doTransaction(aggregate);
  },

  /**
   * Returns an object which could then be used to set/unset the
   * LAST_USED_ANNO annotation for a folder.
   *
   * @param aLastUsed
   *        Whether to set or unset the LAST_USED_ANNO annotation.
   * @returns an object representing the annotation which could then be used
   *          with the transaction manager.
   */
  _getLastUsedAnnotationObject:
  function EIO__getLastUsedAnnotationObject(aLastUsed) {
    var anno = { name: LAST_USED_ANNO,
                 type: Ci.nsIAnnotationService.TYPE_INT32,
                 flags: 0,
                 value: aLastUsed ? new Date().getTime() : null,
                 expires: Ci.nsIAnnotationService.EXPIRE_NEVER };

    return anno;
  },

  _rebuildTagsSelectorList: function EIO__rebuildTagsSelectorList() {
    var tagsSelector = this._element("tagsSelector");
    var tagsSelectorRow = this._element("tagsSelectorRow");
    if (tagsSelectorRow.collapsed)
      return;

    while (tagsSelector.hasChildNodes())
      tagsSelector.removeChild(tagsSelector.lastChild);

    var tagsInField = this._getTagsArrayFromTagField();
    var allTags = PlacesUtils.tagging.allTags;
    for (var i = 0; i < allTags.length; i++) {
      var tag = allTags[i];
      var elt = document.createElement("listitem");
      elt.setAttribute("type", "checkbox");
      elt.setAttribute("label", tag);
      if (tagsInField.indexOf(tag) != -1)
        elt.setAttribute("checked", "true");

      tagsSelector.appendChild(elt);
    }
  },

  toggleTagsSelector: function EIO_toggleTagsSelector() {
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
      tagsSelector.addEventListener("CheckboxStateChange", this, false);
    }
    else {
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
  _getTagsArrayFromTagField: function EIO__getTagsArrayFromTagField() {
    let tags = this._element("tagsField").value;
    return tags.trim()
               .split(/\s*,\s*/) // Split on commas and remove spaces.
               .filter(function (tag) tag.length > 0); // Kill empty tags.
  },

  newFolder: function EIO_newFolder() {
    var ip = this._folderTree.insertionPoint;

    // default to the bookmarks menu folder
    if (!ip || ip.itemId == PlacesUIUtils.allBookmarksFolderId) {
        ip = new InsertionPoint(PlacesUtils.bookmarksMenuFolderId,
                                PlacesUtils.bookmarks.DEFAULT_INDEX,
                                Ci.nsITreeView.DROP_ON);
    }

    // XXXmano: add a separate "New Folder" string at some point...
    var defaultLabel = this._element("newFolderButton").label;
    var txn = PlacesUIUtils.ptm.createFolder(defaultLabel, ip.itemId, ip.index);
    PlacesUIUtils.ptm.doTransaction(txn);
    this._folderTree.focus();
    this._folderTree.selectItems([this._lastNewItem]);
    this._folderTree.startEditing(this._folderTree.view.selection.currentIndex,
                                  this._folderTree.columns.getFirstColumn());
  },

  // nsIDOMEventListener
  handleEvent: function EIO_nsIDOMEventListener(aEvent) {
    switch (aEvent.type) {
    case "CheckboxStateChange":
      // Update the tags field when items are checked/unchecked in the listbox
      var tags = this._getTagsArrayFromTagField();

      if (aEvent.target.checked) {
        if (tags.indexOf(aEvent.target.label) == -1)
          tags.push(aEvent.target.label);
      }
      else {
        var indexOfItem = tags.indexOf(aEvent.target.label);
        if (indexOfItem != -1)
          tags.splice(indexOfItem, 1);
      }
      this._element("tagsField").value = tags.join(", ");
      this._updateTags();
      break;
    case "unload":
      this.uninitPanel(false);
      break;
    }
  },

  // nsINavBookmarkObserver
  onItemChanged: function EIO_onItemChanged(aItemId, aProperty,
                                            aIsAnnotationProperty, aValue,
                                            aLastModified, aItemType) {
    if (this._itemId != aItemId) {
      if (aProperty == "title") {
        // If the title of a folder which is listed within the folders
        // menulist has been changed, we need to update the label of its
        // representing element.
        var menupopup = this._folderMenuList.menupopup;
        for (let i = 0; i < menupopup.childNodes.length; i++) {
          if ("folderId" in menupopup.childNodes[i] &&
              menupopup.childNodes[i].folderId == aItemId) {
            menupopup.childNodes[i].label = aValue;
            break;
          }
        }
      }

      return;
    }

    switch (aProperty) {
    case "title":
      if (PlacesUtils.annotations.itemHasAnnotation(this._itemId,
                                                    STATIC_TITLE_ANNO))
        return;  // onContentLoaded updates microsummary-items

      var userEnteredNameField = this._element("userEnteredName");
      if (userEnteredNameField.value != aValue) {
        userEnteredNameField.value = aValue;
        var namePicker = this._element("namePicker");
        if (namePicker.selectedItem == userEnteredNameField) {
          namePicker.label = aValue;

          // clear undo stack
          namePicker.editor.transactionManager.clear();
        }
      }
      break;
    case "uri":
      var locationField = this._element("locationField");
      if (locationField.value != aValue) {
        this._uri = Cc["@mozilla.org/network/io-service;1"].
                    getService(Ci.nsIIOService).
                    newURI(aValue, null, null);
        this._initTextField("locationField", this._uri.spec);
        this._initNamePicker(); // for microsummaries
        this._initTextField("tagsField",
                             PlacesUtils.tagging
                                        .getTagsForURI(this._uri).join(", "),
                            false);
        this._rebuildTagsSelectorList();
      }
      break;
    case "keyword":
      this._initTextField("keywordField",
                          PlacesUtils.bookmarks
                                     .getKeywordForBookmark(this._itemId));
      break;
    case PlacesUIUtils.DESCRIPTION_ANNO:
      this._initTextField("descriptionField",
                          PlacesUIUtils.getItemDescription(this._itemId));
      break;
    case PlacesUIUtils.LOAD_IN_SIDEBAR_ANNO:
      this._element("loadInSidebarCheckbox").checked =
        PlacesUtils.annotations.itemHasAnnotation(this._itemId,
                                                  PlacesUIUtils.LOAD_IN_SIDEBAR_ANNO);
      break;
    case PlacesUtils.LMANNO_FEEDURI:
      var feedURISpec = PlacesUtils.livemarks.getFeedURI(this._itemId).spec;
      this._initTextField("feedLocationField", feedURISpec);
      break;
    case PlacesUtils.LMANNO_SITEURI:
      var siteURISpec = "";
      var siteURI = PlacesUtils.livemarks.getSiteURI(this._itemId);
      if (siteURI)
        siteURISpec = siteURI.spec;
      this._initTextField("siteLocationField", siteURISpec);
      break;
    }
  },

  onItemMoved: function EIO_onItemMoved(aItemId, aOldParent, aOldIndex,
                                        aNewParent, aNewIndex, aItemType) {
    if (aItemId != this._itemId ||
        aNewParent == this._getFolderIdFromMenuList())
      return;

    var folderItem = this._getFolderMenuItem(aNewParent);

    // just setting selectItem _does not_ trigger oncommand, so we don't
    // recurse
    this._folderMenuList.selectedItem = folderItem;
  },

  onItemAdded: function EIO_onItemAdded(aItemId, aFolder, aIndex, aItemType,
                                        aURI) {
    this._lastNewItem = aItemId;
  },

  onBeginUpdateBatch: function() { },
  onEndUpdateBatch: function() { },
  onBeforeItemRemoved: function() { },
  onItemRemoved: function() { },
  onItemVisited: function() { },
};
