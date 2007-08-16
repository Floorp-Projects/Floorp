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

const LAST_USED_ANNO = "bookmarkPropertiesDialog/lastUsed";
const MAX_FOLDER_ITEM_IN_MENU_LIST = 5;

var gAddBookmarksPanel = {
  /**
   * The Microsummary Service for displaying microsummaries.
   */
  __mss: null,
  get _mss() {
    if (!this.__mss)
      this.__mss = Cc["@mozilla.org/microsummary/service;1"].
                  getService(Ci.nsIMicrosummaryService);
    return this.__mss;
  },

  _uri: null,
  _itemId: -1,
  _itemType: -1,
  _microsummaries: null,
  _doneCallback: null,
  _currentTags: [],
  _hiddenRows: [],

  /**
   * Determines the initial data for the item edited or added by this dialog
   */
  _determineInfo: function ABP__determineInfo(aInfo) {
    const bms = PlacesUtils.bookmarks;
    this._itemType = bms.getItemType(this._itemId);
    if (this._itemType == Ci.nsINavBookmarksService.TYPE_BOOKMARK)
      this._currentTags = PlacesUtils.tagging.getTagsForURI(this._uri);
    else
      this._currentTags.splice(0);

    // hidden rows
    if (aInfo && aInfo.hiddenRows)
      this._hiddenRows = aInfo.hiddenRows;
    else
      this._hiddenRows.splice(0);
  },

  _showHideRows: function EBP__showHideRows() {
    this._element("nameRow").hidden = this._hiddenRows.indexOf("name") != -1;
    this._element("folderRow").hidden =
      this._hiddenRows.indexOf("folderPicker") != -1;
    this._element("tagsRow").hidden = this._hiddenRows.indexOf("tags") != -1 ||
      this._itemType != Ci.nsINavBookmarksService.TYPE_BOOKMARK;
    this._element("descriptionRow").hidden =
      this._hiddenRows.indexOf("description") != -1;
  },

  /**
   * Initialize the panel
   */
  initPanel: function ABP_initPanel(aItemId, aTm, aDoneCallback, aInfo) {
    this._folderMenuList = this._element("folderMenuList");
    this._folderTree = this._element("folderTree");
    this._tm = aTm;
    this._itemId = aItemId;
    this._uri = PlacesUtils.bookmarks.getBookmarkURI(this._itemId);
    this._doneCallback = aDoneCallback;
    this._determineInfo(aInfo);

    // folder picker
    this._initFolderMenuList();
    
    // name picker
    this._initNamePicker();

    // tags field
    this._element("tagsField").value = this._currentTags.join(", ");

    // description field
    this._element("descriptionField").value =
      PlacesUtils.getItemDescription(this._itemId);

    this._showHideRows();
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
  function BPP__appendFolderItemToMenuList(aMenupopup, aFolderId) {
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

  _initFolderMenuList: function BPP__initFolderMenuList() {
    // clean up first
    var menupopup = this._folderMenuList.menupopup;
    while (menupopup.childNodes.length > 4)
      menupopup.removeChild(menupopup.lastChild);

    var container = PlacesUtils.bookmarks.getFolderIdForItem(this._itemId);

    // only show "All Bookmarks" if the url isn't bookmarked somewhere else
    this._element("placesRootItem").hidden = container != PlacesUtils.placesRootId;

    // List of recently used folders:
    var annos = PlacesUtils.annotations;
    var folderIds = annos.getItemsWithAnnotation(LAST_USED_ANNO, { });

    /**
     * The value of the LAST_USED_ANNO annotation is the time (in the form of
     * Date.getTime) at which the folder has been last used.
     *
     * First we build the annotated folders array, each item has both the
     * folder identifier and the time at which it was last-used by this dialog
     * set. Then we sort it descendingly based on the time field.
     */
    var folders = [];
    for (var i=0; i < folderIds.length; i++) {
      var lastUsed = annos.getItemAnnotation(folderIds[i], LAST_USED_ANNO);
      folders.push({ folderId: folderIds[i], lastUsed: lastUsed });
    }
    folders.sort(function(a, b) {
      if (b.lastUsed < a.lastUsed)
        return -1;
      if (b.lastUsed > a.lastUsed)
        return 1;
      return 0;
    });

    var numberOfItems = Math.min(MAX_FOLDER_ITEM_IN_MENU_LIST, folders.length);
    for (i=0; i < numberOfItems; i++) {
      this._appendFolderItemToMenupopup(menupopup, folders[i].folderId);
    }

    var defaultItem = this._getFolderMenuItem(container, true);
    this._folderMenuList.selectedItem = defaultItem;

    // Hide the folders-separator if no folder is annotated as recently-used
    this._element("foldersSeparator").hidden = (menupopup.childNodes.length <= 4);
  },

  QueryInterface: function BPP_QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIMicrosummaryObserver) ||
        aIID.equals(Ci.nsIDOMEventListener) ||
        aIID.eqauls(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  _element: function BPP__element(aID) {
    return document.getElementById("editBMPanel_" + aID);
  },

  _createMicrosummaryMenuItem:
  function BPP__createMicrosummaryMenuItem(aMicrosummary) {
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

  _initNamePicker: function ABP_initNamePicker() {
    var userEnteredNameField = this._element("userEnteredName");
    var namePicker = this._element("namePicker");
    var droppable = false;

    userEnteredNameField.label =
      PlacesUtils.bookmarks.getItemTitle(this._itemId);

    // clean up old entries
    var menupopup = namePicker.menupopup;
    while (menupopup.childNodes.length > 2)
      menupopup.removeChild(menupopup.lastChild);

    var itemToSelect = userEnteredNameField;
    try {
      this._microsummaries = this._mss.getMicrosummaries(this._uri, -1);
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
  },

  // nsIMicrosummaryObserver
  onContentLoaded: function ABP_onContentLoaded(aMicrosummary) {
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

  onElementAppended: function BPP_onElementAppended(aMicrosummary) {
    var namePicker = this._element("namePicker");
    namePicker.menupopup
              .appendChild(this._createMicrosummaryMenuItem(aMicrosummary));

    // Make sure the drop-marker is shown
    namePicker.setAttribute("droppable", "true");
  },

  uninitPanel: function ABP_uninitPanel() {
    if (this._microsummaries)
      this._microsummaries.removeObserver(this);

    // hide the folder tree if it was previously visible
    if (!this._folderTree.collapsed)
      this.toggleFolderTreeVisibility();

    // hide the tag selector if it was previously visible
    var tagsSelector = this._element("tagsSelector");
    if (!tagsSelector.collapsed)
      tagsSelector.collapsed = true;
  },

  saveItem: function ABP_saveItem() {
    var container = this._getFolderIdFromMenuList();
    const bms = PlacesUtils.bookmarks;
    const ptm = PlacesUtils.ptm;
    var txns = [];

    // container
    if (bms.getFolderIdForItem(this._itemId) != container)
      txns.push(ptm.moveItem(this._itemId, container, -1));

    // title
    var newTitle = this._element("userEnteredName").label;
    if (bms.getItemTitle(this._itemId) != newTitle)
      txns.push(ptm.editItemTitle(this._itemId, newTitle));

    // description
    var newDescription = this._element("descriptionField").value;
    if (newDescription != PlacesUtils.getItemDescription(this._itemId))
      txns.push(ptm.editItemDescription(this._itemId, newDescription));

    // Tags, NOT YET UNDOABLE
    var tags = this._getTagsArrayFromTagField();
    if (tags.length > 0 || this._currentTags.length > 0) {
      var tagsToRemove = [];
      var tagsToAdd = [];
      var t;
      for each (t in this._currentTags) {
        if (tags.indexOf(t) == -1)
          tagsToRemove.push(t);
      }
      for each (t in tags) {
        if (this._currentTags.indexOf(t) == -1)
          tagsToAdd.push(t);
      }

      if (tagsToAdd.length > 0)
        PlacesUtils.tagging.tagURI(this._uri, tagsToAdd);
      if (tagsToRemove.length > 0)
        PlacesUtils.tagging.untagURI(this._uri, tagsToRemove);
    }

    if (txns.length > 0) {
      // Mark the containing folder as recently-used if it isn't the
      // "All Bookmarks" root
      if (container != PlacesUtils.placesRootId)
        this._markFolderAsRecentlyUsed(container);
    }

    if (txns.length > 0)
      ptm.commitTransaction(ptm.aggregateTransactions("Edit Item", txns));
  },

  onNamePickerInput: function ABP_onNamePickerInput() {
    this._element("userEnteredName").label = this._element("namePicker").value;
  },

  toggleFolderTreeVisibility: function ABP_toggleFolderTreeVisibility() {
    var expander = this._element("foldersExpander");
    if (!this._folderTree.collapsed) {
      expander.className = "expander-down";
      expander.setAttribute("tooltiptext",
                            expander.getAttribute("tooltiptextdown"));
      this._folderTree.collapsed = true;
    }
    else {
      expander.className = "expander-up"
      expander.setAttribute("tooltiptext",
                            expander.getAttribute("tooltiptextup"));
      if (!this._folderTree.treeBoxObject.view.isContainerOpen(0))
        this._folderTree.treeBoxObject.view.toggleOpenState(0);
      this._folderTree.selectFolders([this._getFolderIdFromMenuList()]);
      this._folderTree.collapsed = false;
      this._folderTree.focus();
    }
  },

  _getFolderIdFromMenuList:
  function BPP__getFolderIdFromMenuList() {
    var selectedItem = this._folderMenuList.selectedItem
    switch (selectedItem.id) {
      case "editBMPanel_placesRootItem":
        return PlacesUtils.placesRootId;
      case "editBMPanel_bmRootItem":
        return PlacesUtils.bookmarksRootId;
      case "editBMPanel_toolbarFolderItem":
        return PlacesUtils.toolbarFolderId;
    }

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
   *        The identifier of the bookmarks folder
   * @param aCheckStaticFolderItems
   *        whether or not to also treat the static items at the top of
   *        menu-list. Note dynamic items get precedence even if this is set to
   *        true.
   */
  _getFolderMenuItem:
  function BPP__getFolderMenuItem(aFolderId, aCheckStaticFolderItems) {
    var menupopup = this._folderMenuList.menupopup;

    // 0: All Bookmarks, 1: Bookmarks root, 2: toolbar folder, 3: separator
    for (var i=4;  i < menupopup.childNodes.length; i++) {
      if (menupopup.childNodes[i].folderId == aFolderId)
        return menupopup.childNodes[i];
    }

    if (aCheckStaticFolderItems) {
      if (aFolderId == PlacesUtils.placesRootId)
        return this._element("placesRootItem");
      if (aFolderId == PlacesUtils.bookmarksRootId)
        return this._element("bmRootItem")
      if (aFolderId == PlacesUtils.toolbarFolderId)
        return this._element("toolbarFolderItem")
    }

    // 3 special folders + separator + folder-items-count limit
    if (menupopup.childNodes.length == 4 + MAX_FOLDER_ITEM_IN_MENU_LIST)
      menupopup.removeChild(menupopup.lastChild);

    return this._appendFolderItemToMenupopup(menupopup, aFolderId);
  },

  onMenuListFolderSelect: function BPP_onMenuListFolderSelect(aEvent) {
    if (this._folderTree.hidden)
      return;

    this._folderTree.selectFolders([this._getFolderIdFromMenuList()]);
  },

  onFolderTreeSelect: function BPP_onFolderTreeSelect() {
    var selectedNode = this._folderTree.selectedNode;
    if (!selectedNode)
      return;

    var folderId = selectedNode.itemId;
    // Don't set the selected item if the static item for the folder is
    // already selected
    var oldSelectedItem = this._folderMenuList.selectedItem;
    if ((oldSelectedItem.id == "editBMPanel_toolbarFolderItem" &&
         folderId == PlacesUtils.bookmarks.toolbarFolder) ||
        (oldSelectedItem.id == "editBMPanel_bmRootItem" &&
         folderId == PlacesUtils.bookmarks.bookmarksRoot))
      return;

    var folderItem = this._getFolderMenuItem(folderId, false);
    this._folderMenuList.selectedItem = folderItem;
  },

  _markFolderAsRecentlyUsed:
  function ABP__markFolderAsRecentlyUsed(aFolderId) {
    // We'll figure out when/if to expire the annotation if it turns out
    // we keep this recently-used-folders implementation
    PlacesUtils.annotations
               .setItemAnnotation(aFolderId, LAST_USED_ANNO,
                                  new Date().getTime(), 0,
                                  Ci.nsIAnnotationService.EXPIRE_NEVER);
  },

  accept: function ABP_accept() {
    this.saveItem();
    if (typeof(this._doneCallback) == "function")
      this._doneCallback();
  },

  deleteAndClose: function ABP_deleteAndClose() {
    // remove the item
    if (this._itemId != -1)
      PlacesUtils.bookmarks.removeItem(this._itemId);

    // remove all tags for the associated url
    PlacesUtils.tagging.untagURI(this._uri, null);

    if (typeof(this._doneCallback) == "function")
      this._doneCallback();
  },

  toggleTagsSelector: function ABP_toggleTagsSelector() {
    var tagsSelector = this._element("tagsSelector");
    var expander = this._element("tagsSelectorExpander");
    if (tagsSelector.collapsed) {
      expander.className = "expander-down";
      expander.setAttribute("tooltiptext",
                            expander.getAttribute("tooltiptextdown"));

      // rebuild the tag list
      while (tagsSelector.hasChildNodes())
        tagsSelector.removeChild(tagsSelector.lastChild);

      var tagsInField = this._getTagsArrayFromTagField();
      var allTags = PlacesUtils.tagging.allTags;
      for each (var tag in allTags) {
        var elt = document.createElement("listitem");
        elt.setAttribute("type", "checkbox");
        elt.setAttribute("label", tag);
        if (tagsInField.indexOf(tag) != -1)
          elt.setAttribute("checked", "true");

        tagsSelector.appendChild(elt);
      }

      // This is a no-op if we've added the listener.
      tagsSelector.addEventListener("CheckboxStateChange", this, false);
    }
    else {
      expander.className = "expander-down";
      expander.setAttribute("tooltiptext",
                            expander.getAttribute("tooltiptextdown"));
    }

    tagsSelector.collapsed = !tagsSelector.collapsed;
  },

  _getTagsArrayFromTagField: function() {
    // we don't require the leading space (after each comma)
    var tags = this._element("tagsField").value.split(",");
    for (var i=0; i < tags.length; i++) {
      // remove trailing and leading spaces
      tags[i] = tags[i].replace(/^\s+/, "").replace(/\s+$/, "");

      // remove empty entries from the array.
      if (tags[i] == "") {
        tags.splice(i, 1);
        i--;
      }
    }
    return tags;
  },

  // nsIDOMEventListener
  handleEvent: function ABP_nsIDOMEventListener(aEvent) {
    if (aEvent.type == "CheckboxStateChange") {
      // Update the tags field when items are checked/unchecked in the listbox
      var tags = this._getTagsArrayFromTagField();

      if (aEvent.target.checked)
        tags.push(aEvent.target.label);
      else {
        var indexOfItem = tags.indexOf(aEvent.target.label);
        if (indexOfItem != -1)
          tags.splice(indexOfItem, 1);
      }
      this._element("tagsField").value = tags.join(", ");
    }
  }
};
