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
const STATIC_TITLE_ANNO = "bookmarks/staticTitle";
const MAX_FOLDER_ITEM_IN_MENU_LIST = 5;

var gEditItemOverlay = {
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
  _hiddenRows: [],
  _observersAdded: false,

  get itemId() {
    return this._itemId;
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
  },

  _showHideRows: function EIO__showHideRows() {
    var isBookmark = this._itemType == Ci.nsINavBookmarksService.TYPE_BOOKMARK;
    this._element("nameRow").hidden = this._hiddenRows.indexOf("name") != -1;
    this._element("folderRow").hidden =
      this._hiddenRows.indexOf("folderPicker") != -1;
    this._element("tagsRow").hidden =
      this._hiddenRows.indexOf("tags") != -1 || !isBookmark;
    this._element("descriptionRow").hidden =
      this._hiddenRows.indexOf("description") != -1;
    this._element("locationRow").hidden =
      this._hiddenRows.indexOf("location") != -1 || !isBookmark;
  },

  /**
   * Initialize the panel
   */
  initPanel: function EIO_initPanel(aItemId, aInfo) {
    this._folderMenuList = this._element("folderMenuList");
    this._folderTree = this._element("folderTree");
    this._itemId = aItemId;
    this._itemType = PlacesUtils.bookmarks.getItemType(this._itemId);
    this._determineInfo(aInfo);

    if (this._itemType == Ci.nsINavBookmarksService.TYPE_BOOKMARK) {
      this._uri = PlacesUtils.bookmarks.getBookmarkURI(this._itemId);
      // tags field
      this._element("tagsField").value =
        PlacesUtils.tagging.getTagsForURI(this._uri).join(", ");

      this._element("locationField").value = this._uri.spec;
    }

    // folder picker
    this._initFolderMenuList();

    // name picker
    this._initNamePicker();

    // description field
    this._element("descriptionField").value =
      PlacesUtils.getItemDescription(this._itemId);

    this._showHideRows();

    // observe changes
    if (!this._observersAdded) {
      PlacesUtils.bookmarks.addObserver(this, false);
      window.addEventListener("unload", this, false);
      this._observersAdded = true;
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

  _initFolderMenuList: function EIO__initFolderMenuList() {
    // clean up first
    var menupopup = this._folderMenuList.menupopup;
    while (menupopup.childNodes.length > 4)
      menupopup.removeChild(menupopup.lastChild);

    var container = PlacesUtils.bookmarks.getFolderIdForItem(this._itemId);

    // only show "All Bookmarks" if the url isn't bookmarked somewhere else
    this._element("unfiledRootItem").hidden = container != PlacesUtils.unfiledRootId;

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

  QueryInterface: function EIO_QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIMicrosummaryObserver) ||
        aIID.equals(Ci.nsIDOMEventListener) ||
        aIID.equals(Ci.nsINavBookmarkObserver) ||
        aIID.eqauls(Ci.nsISupports))
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
      if (this._itemType == Ci.nsINavBookmarksService.TYPE_BOOKMARK)
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
          if (this._mss.isMicrosummary(this._itemId, microsummary))
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
      if (!this._folderTree.collapsed)
        this.toggleFolderTreeVisibility();

      // hide the tag selector if it was previously visible
      var tagsSelector = this._element("tagsSelector");
      if (!tagsSelector.collapsed)
        this._toggleTagsSelector();
    }

    if (this._observersAdded) {
      PlacesUtils.bookmarks.removeObserver(this);
      this._observersAdded = false;
    }
    if (this._microsummaries) {
      this._microsummaries.removeObserver(this);
      this._microsummaries = null;
    }
    this._itemId = -1;
  },

  onTagsFieldBlur: function EIO_onTagsFieldBlur() {
    this._updateTags();
  },

  _updateTags: function EIO__updateTags() {
    var currentTags = PlacesUtils.tagging.getTagsForURI(this._uri);
    var tags = this._getTagsArrayFromTagField();
    if (tags.length > 0 || currentTags.length > 0) {
      var tagsToRemove = [];
      var tagsToAdd = [];
      var t;
      for each (t in currentTags) {
        if (tags.indexOf(t) == -1)
          tagsToRemove.push(t);
      }
      for each (t in tags) {
        if (currentTags.indexOf(t) == -1)
          tagsToAdd.push(t);
      }

      if (tagsToAdd.length > 0)
        PlacesUtils.tagging.tagURI(this._uri, tagsToAdd);
      if (tagsToRemove.length > 0)
        PlacesUtils.tagging.untagURI(this._uri, tagsToRemove);
    }
  },

  onNamePickerInput: function EIO_onNamePickerInput() {
    var title = this._element("namePicker").value;
    this._element("userEnteredName").label = title;
  },

  onNamePickerChange: function EIO_onNamePickerChange() {
    var namePicker = this._element("namePicker")
    var txns = [];
    const ptm = PlacesUtils.ptm;

    // Here we update either the item title or its cached static title
    var newTitle = this._element("userEnteredName").label;
    if (this._getItemStaticTitle() != newTitle) {
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
    if ((newMicrosummary == null && this._mss.hasMicrosummary(this._itemId)) ||
        (newMicrosummary != null &&
         !this._mss.isMicrosummary(this._itemId, newMicrosummary))) {
      txns.push(ptm.editBookmarkMicrosummary(this._itemId, newMicrosummary));
    }

    var aggregate = ptm.aggregateTransactions("Edit Item Title", txns);
    ptm.commitTransaction(aggregate);
  },

  onDescriptionFieldBlur: function EIO_onDescriptionFieldInput() {
    var description = this._element("descriptionField").value;
    if (description != PlacesUtils.getItemDescription(this._itemId)) {
      var txn = PlacesUtils.ptm
                           .editItemDescription(this._itemId, description);
      PlacesUtils.ptm.commitTransaction(txn);
    }
  },

  onLocationFieldBlur: function EIO_onLocationFieldBlur() {
    // XXXmano: uri fixup
    var uri;
    try {
      uri = IO.newURI(this._element("locationField").value);
    }
    catch(ex) { return; }

    if (!this._uri.equals(uri)) {
      var txn = PlacesUtils.ptm.editBookmarkURI(this._itemId, uri);
      PlacesUtils.ptm.commitTransaction(txn);
    }
  },

  toggleFolderTreeVisibility: function EIO_toggleFolderTreeVisibility() {
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
  function EIO__getFolderIdFromMenuList() {
    var selectedItem = this._folderMenuList.selectedItem
    switch (selectedItem.id) {
      case "editBMPanel_unfiledRootItem":
        return PlacesUtils.unfiledRootId;
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
  function EIO__getFolderMenuItem(aFolderId, aCheckStaticFolderItems) {
    var menupopup = this._folderMenuList.menupopup;

    // 0: All Bookmarks, 1: Bookmarks root, 2: toolbar folder, 3: separator
    for (var i=4;  i < menupopup.childNodes.length; i++) {
      if (menupopup.childNodes[i].folderId == aFolderId)
        return menupopup.childNodes[i];
    }

    if (aCheckStaticFolderItems) {
      if (aFolderId == PlacesUtils.unfiledRootId)
        return this._element("unfiledRootItem");
      if (aFolderId == PlacesUtils.bookmarksRootId)
        return this._element("bmRootItem");
      if (aFolderId == PlacesUtils.toolbarFolderId)
        return this._element("toolbarFolderItem");
    }

    // 3 special folders + separator + folder-items-count limit
    if (menupopup.childNodes.length == 4 + MAX_FOLDER_ITEM_IN_MENU_LIST)
      menupopup.removeChild(menupopup.lastChild);

    return this._appendFolderItemToMenupopup(menupopup, aFolderId);
  },

  onFolderMenuListCommand: function EIO_onFolderMenuListCommand(aEvent) {
    var container = this._getFolderIdFromMenuList();

    // Move the item
    if (PlacesUtils.bookmarks.getFolderIdForItem(this._itemId) != container) {
      var txn = PlacesUtils.ptm.moveItem(this._itemId, container, -1);
      PlacesUtils.ptm.commitTransaction(txn);

      // Mark the containing folder as recently-used if it isn't the
      // "All Bookmarks" root
      if (container != PlacesUtils.unfiledRootId)
        this._markFolderAsRecentlyUsed(container);
    }

    // Update folder-tree selection
    if (isElementVisible(this._folderTree)) {
      var selectedNode = this._folderTree.selectedNode;
      if (!selectedNode || selectedNode.itemId != container)
        this._folderTree.selectFolders([container]);
    }
  },

  onFolderTreeSelect: function EIO_onFolderTreeSelect() {
    var selectedNode = this._folderTree.selectedNode;
    if (!selectedNode)
      return;

    var folderId = selectedNode.itemId;
    if (this._getFolderIdFromMenuList() == folderId)
      return;

    var folderItem = this._getFolderMenuItem(folderId, false);
    this._folderMenuList.selectedItem = folderItem;
    folderItem.doCommand();
  },

  _markFolderAsRecentlyUsed:
  function EIO__markFolderAsRecentlyUsed(aFolderId) {
    // We'll figure out when/if to expire the annotation if it turns out
    // we keep this recently-used-folders implementation
    PlacesUtils.annotations
               .setItemAnnotation(aFolderId, LAST_USED_ANNO,
                                  new Date().getTime(), 0,
                                  Ci.nsIAnnotationService.EXPIRE_NEVER);
  },

  _rebuildTagsSelectorList: function EIO__rebuildTagsSelectorList() {
    var tagsSelector = this._element("tagsSelector");

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
  },

  toggleTagsSelector: function EIO_toggleTagsSelector() {
    var tagsSelector = this._element("tagsSelector");
    var expander = this._element("tagsSelectorExpander");
    if (!isElementVisible(tagsSelector)) {
      expander.className = "expander-up";
      expander.setAttribute("tooltiptext",
                            expander.getAttribute("tooltiptextup"));

      this._rebuildTagsSelectorList();

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

  _getTagsArrayFromTagField: function EIO__getTagsArrayFromTagField() {
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
  handleEvent: function EIO_nsIDOMEventListener(aEvent) {
    switch (aEvent.type) {
    case "CheckboxStateChange":
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
      this._updateTags();
      break;
    case "unload":
      this.uninitPanel(false);
      break;
    }
  },

  // nsINavBookmarkObserver
  onItemChanged: function EIO_onItemChanged(aItemId, aProperty,
                                            aIsAnnotationProperty, aValue) {
    if (this._itemId != aItemId)
      return;

    switch (aProperty) {
    case "title":
      if (PlacesUtils.annotations.itemHasAnnotation(this._itemId,
                                                    STATIC_TITLE_ANNO))
        return;  // onContentLoaded updates microsummary-items

      var userEnteredNameField = this._element("userEnteredName");
      if (userEnteredNameField.value != aValue) {
          userEnteredNameField.value = aValue;
        var namePicker = this._element("namePicker");
        if (namePicker.selectedItem == userEnteredNameField)
          namePicker.label = aValue;
      }
      break;
    case "uri":
      var locationField = this._element("locationField");
      if (locationField.value != aValue) {
        locationField.value = aValue;
        this._uri = IO.newURI(aValue);
        this._initNamePicker(); // for microsummaries
        this._element("tagsField").value =
          PlacesUtils.tagging.getTagsForURI(this._uri).join(", ");
        this._rebuildTagsSelectorList();
      }
      break;
    case DESCRIPTION_ANNO:
      this._element("descriptionField").value =
        PlacesUtils.annotations.getItemDescription(this._itemId);
      break;
    }
  },

  onItemMoved: function EIO_onItemMoved(aItemId, aOldParent, aOldIndex,
                                        aNewParent, aNewIndex) {
    if (aItemId != this._itemId ||
        aNewParent == this._getFolderIdFromMenuList())
      return;

    var folderItem = this._getFolderMenuItem(aNewParent, false);

    // just setting selectItem _does not_ trigger oncommand, so we don't
    // recurse
    this._folderMenuList.selectedItem = folderItem;
  },

  onBeginUpdateBatch: function() { },
  onEndUpdateBatch: function() { },
  onItemAdded: function() { },
  onItemRemoved: function() { },
  onItemVisited: function() { },
};
