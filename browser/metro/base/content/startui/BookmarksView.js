/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Wraps a list/grid control implementing nsIDOMXULSelectControlElement and
 * fills it with the user's bookmarks.
 *
 * @param           aSet    Control implementing nsIDOMXULSelectControlElement.
 * @param {Number}  aLimit  Maximum number of items to show in the view.
 * @param           aRoot   Bookmark root to show in the view.
 */
function BookmarksView(aSet, aLimit, aRoot, aFilterUnpinned) {
  View.call(this, aSet);

  this._inBatch = false; // batch up grid updates to avoid redundant arrangeItems calls

  this._limit = aLimit;
  this._filterUnpinned = aFilterUnpinned;
  this._bookmarkService = PlacesUtils.bookmarks;
  this._navHistoryService = gHistSvc;

  this._changes = new BookmarkChangeListener(this);
  this._pinHelper = new ItemPinHelper("metro.bookmarks.unpinned");
  this._bookmarkService.addObserver(this._changes, false);
  StartUI.chromeWin.addEventListener('MozAppbarDismissing', this, false);
  StartUI.chromeWin.addEventListener('BookmarksNeedsRefresh', this, false);
  window.addEventListener("TabClose", this, true);

  this.root = aRoot;
}

BookmarksView.prototype = Util.extend(Object.create(View.prototype), {
  _limit: null,
  _set: null,
  _changes: null,
  _root: null,
  _sort: 0, // Natural bookmark order.
  _toRemove: null,

  get sort() {
    return this._sort;
  },

  set sort(aSort) {
    this._sort = aSort;
    this.clearBookmarks();
    this.getBookmarks();
  },

  get root() {
    return this._root;
  },

  set root(aRoot) {
    this._root = aRoot;
  },

  destruct: function bv_destruct() {
    this._bookmarkService.removeObserver(this._changes);
    if (StartUI.chromeWin) {
      StartUI.chromeWin.removeEventListener('MozAppbarDismissing', this, false);
      StartUI.chromeWin.removeEventListener('BookmarksNeedsRefresh', this, false);
    }
    View.prototype.destruct.call(this);
  },

  handleItemClick: function bv_handleItemClick(aItem) {
    let url = aItem.getAttribute("value");
    StartUI.goToURI(url);
  },

  _getItemForBookmarkId: function bv__getItemForBookmark(aBookmarkId) {
    return this._set.querySelector("richgriditem[anonid='" + aBookmarkId + "']");
  },

  _getBookmarkIdForItem: function bv__getBookmarkForItem(aItem) {
    return +aItem.getAttribute("anonid");
  },

  _updateItemWithAttrs: function dv__updateItemWithAttrs(anItem, aAttrs) {
    for (let name in aAttrs)
      anItem.setAttribute(name, aAttrs[name]);
  },

  getBookmarks: function bv_getBookmarks(aRefresh) {
    let options = this._navHistoryService.getNewQueryOptions();
    options.queryType = options.QUERY_TYPE_BOOKMARKS;
    options.excludeQueries = true; // Don't include "smart folders"
    options.sortingMode = this._sort;

    let limit = this._limit || Infinity;

    let query = this._navHistoryService.getNewQuery();
    query.setFolders([Bookmarks.metroRoot], 1);

    let result = this._navHistoryService.executeQuery(query, options);
    let rootNode = result.root;
    rootNode.containerOpen = true;
    let childCount = rootNode.childCount;

    this._inBatch = true; // batch up grid updates to avoid redundant arrangeItems calls

    for (let i = 0, addedCount = 0; i < childCount && addedCount < limit; i++) {
      let node = rootNode.getChild(i);

      // Ignore folders, separators, undefined item types, etc.
      if (node.type != node.RESULT_TYPE_URI)
        continue;

      // If item is marked for deletion, skip it.
      if (this._toRemove && this._toRemove.indexOf(node.itemId) !== -1)
        continue;

      let item = this._getItemForBookmarkId(node.itemId);

      // Item has been unpinned.
      if (this._filterUnpinned && !this._pinHelper.isPinned(node.itemId)) {
        if (item)
          this.removeBookmark(node.itemId);

        continue;
      }

      if (!aRefresh || !item) {
        // If we're not refreshing or the item is not in the grid, add it.
        this.addBookmark(node.itemId, addedCount);
      } else if (aRefresh && item) {
        // Update context action in case it changed in another view.
        this._setContextActions(item);
      }

      addedCount++;
    }

    // Remove extra items in case a refresh added more than the limit.
    // This can happen when undoing a delete.
    if (aRefresh) {
      while (this._set.itemCount > limit)
        this._set.removeItemAt(this._set.itemCount - 1, true);
    }
    this._set.arrangeItems();
    this._set.removeAttribute("fade");
    this._inBatch = false;
    rootNode.containerOpen = false;
  },

  inCurrentView: function bv_inCurrentView(aParentId, aItemId) {
    if (this._root && aParentId != this._root)
      return false;

    return !!this._getItemForBookmarkId(aItemId);
  },

  clearBookmarks: function bv_clearBookmarks() {
    if ('clearAll' in this._set)
      this._set.clearAll();
  },

  addBookmark: function bv_addBookmark(aBookmarkId, aPos) {
    let index = this._bookmarkService.getItemIndex(aBookmarkId);
    let uri = this._bookmarkService.getBookmarkURI(aBookmarkId);
    let title = this._bookmarkService.getItemTitle(aBookmarkId) || uri.spec;
    let item = this._set.insertItemAt(aPos || index, title, uri.spec, this._inBatch);
    item.setAttribute("anonid", aBookmarkId);
    this._setContextActions(item);
    this._updateFavicon(item, uri);
  },

  _setContextActions: function bv__setContextActions(aItem) {
    let itemId = this._getBookmarkIdForItem(aItem);
    aItem.setAttribute("data-contextactions", "delete," + (this._pinHelper.isPinned(itemId) ? "unpin" : "pin"));
    if (aItem.refresh) aItem.refresh();
  },

  _sendNeedsRefresh: function bv__sendNeedsRefresh(){
    // Event sent when all view instances need to refresh.
    let event = document.createEvent("Events");
    event.initEvent("BookmarksNeedsRefresh", true, false);
    window.dispatchEvent(event);
  },

  updateBookmark: function bv_updateBookmark(aBookmarkId) {
    let item = this._getItemForBookmarkId(aBookmarkId);

    if (!item)
      return;

    let oldIndex = this._set.getIndexOfItem(item);
    let index = this._bookmarkService.getItemIndex(aBookmarkId);

    if (oldIndex != index) {
      this.removeBookmark(aBookmarkId);
      this.addBookmark(aBookmarkId);
      return;
    }

    let uri = this._bookmarkService.getBookmarkURI(aBookmarkId);
    let title = this._bookmarkService.getItemTitle(aBookmarkId) || uri.spec;

    item.setAttribute("anonid", aBookmarkId);
    item.setAttribute("value", uri.spec);
    item.setAttribute("label", title);

    this._updateFavicon(item, uri);
  },

  removeBookmark: function bv_removeBookmark(aBookmarkId) {
    let item = this._getItemForBookmarkId(aBookmarkId);
    let index = this._set.getIndexOfItem(item);
    this._set.removeItemAt(index, this._inBatch);
  },

  doActionOnSelectedTiles: function bv_doActionOnSelectedTiles(aActionName, aEvent) {
    let tileGroup = this._set;
    let selectedTiles = tileGroup.selectedItems;

    switch (aActionName){
      case "delete":
        Array.forEach(selectedTiles, function(aNode) {
          if (!this._toRemove) {
            this._toRemove = [];
          }

          let itemId = this._getBookmarkIdForItem(aNode);

          this._toRemove.push(itemId);
          this.removeBookmark(itemId);
        }, this);

        // stop the appbar from dismissing
        aEvent.preventDefault();

        // at next tick, re-populate the context appbar.
        setTimeout(function(){
          // fire a MozContextActionsChange event to update the context appbar
          let event = document.createEvent("Events");
          // we need the restore button to show (the tile node will go away though)
          event.actions = ["restore"];
          event.initEvent("MozContextActionsChange", true, false);
          tileGroup.dispatchEvent(event);
        }, 0);
        break;

      case "restore":
        // clear toRemove and let _sendNeedsRefresh update the items.
        this._toRemove = null;
        break;

      case "unpin":
        Array.forEach(selectedTiles, function(aNode) {
          let itemId = this._getBookmarkIdForItem(aNode);

          if (this._filterUnpinned)
            this.removeBookmark(itemId);

          this._pinHelper.setUnpinned(itemId);
        }, this);
        break;

      case "pin":
        Array.forEach(selectedTiles, function(aNode) {
          let itemId = this._getBookmarkIdForItem(aNode);

          this._pinHelper.setPinned(itemId);
        }, this);
        break;

      default:
        return;
    }

    // Send refresh event so all view are in sync.
    this._sendNeedsRefresh();
  },

  handleEvent: function bv_handleEvent(aEvent) {
    switch (aEvent.type){
      case "MozAppbarDismissing":
        // If undo wasn't pressed, time to do definitive actions.
        if (this._toRemove) {
          for (let bookmarkId of this._toRemove) {
            this._bookmarkService.removeItem(bookmarkId);
          }
          this._toRemove = null;
        }
        break;

      case "BookmarksNeedsRefresh":
        this.getBookmarks(true);
        break;

      case "TabClose":
        // Flush any pending actions - appbar will call us back
        // before this returns with 'MozAppbarDismissing' above.
        StartUI.chromeWin.ContextUI.dismissContextAppbar();
      break;
    }
  }
});

let BookmarksStartView = {
  _view: null,
  get _grid() { return document.getElementById("start-bookmarks-grid"); },

  init: function init() {
    this._view = new BookmarksView(this._grid, StartUI.maxResultsPerSection, Bookmarks.metroRoot, true);
    this._view.getBookmarks();
  },

  uninit: function uninit() {
    if (this._view) {
      this._view.destruct();
    }
  },
};

/**
 * Observes bookmark changes and keeps a linked BookmarksView updated.
 *
 * @param aView An instance of BookmarksView.
 */
function BookmarkChangeListener(aView) {
  this._view = aView;
}

BookmarkChangeListener.prototype = {
  //////////////////////////////////////////////////////////////////////////////
  //// nsINavBookmarkObserver
  onBeginUpdateBatch: function () { },
  onEndUpdateBatch: function () { },

  onItemAdded: function bCL_onItemAdded(aItemId, aParentId, aIndex, aItemType, aURI, aTitle, aDateAdded, aGUID, aParentGUID) {
    this._view.getBookmarks(true);
  },

  onItemChanged: function bCL_onItemChanged(aItemId, aProperty, aIsAnnotationProperty, aNewValue, aLastModified, aItemType, aParentId, aGUID, aParentGUID) {
    let itemIndex = PlacesUtils.bookmarks.getItemIndex(aItemId);
    if (!this._view.inCurrentView(aParentId, aItemId))
      return;

    this._view.updateBookmark(aItemId);
  },

  onItemMoved: function bCL_onItemMoved(aItemId, aOldParentId, aOldIndex, aNewParentId, aNewIndex, aItemType, aGUID, aOldParentGUID, aNewParentGUID) {
    let wasInView = this._view.inCurrentView(aOldParentId, aItemId);
    let nowInView = this._view.inCurrentView(aNewParentId, aItemId);

    if (!wasInView && nowInView)
      this._view.addBookmark(aItemId);

    if (wasInView && !nowInView)
      this._view.removeBookmark(aItemId);

    this._view.getBookmarks(true);
  },

  onBeforeItemRemoved: function (aItemId, aItemType, aParentId, aGUID, aParentGUID) { },
  onItemRemoved: function bCL_onItemRemoved(aItemId, aParentId, aIndex, aItemType, aURI, aGUID, aParentGUID) {
    if (!this._view.inCurrentView(aParentId, aItemId))
      return;

    this._view.removeBookmark(aItemId);
    this._view.getBookmarks(true);
  },

  onItemVisited: function(aItemId, aVisitId, aTime, aTransitionType, aURI, aParentId, aGUID, aParentGUID) { },

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINavBookmarkObserver])
};
