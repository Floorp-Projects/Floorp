/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Utility singleton for manipulating bookmarks.
 */
var Bookmarks = {
  get metroRoot() {
    return PlacesUtils.annotations.getItemsWithAnnotation('metro/bookmarksRoot', {})[0];
  },

  logging: false,
  log: function(msg) {
    if (this.logging) {
      Services.console.logStringMessage(msg);
    }
  },

  /*
   * fixupColorFormat - convert a decimal color value to a valid
   * css color string of the format '#123456'
   */ 
  fixupColorFormat: function bv_fixupColorFormat(aColor) {
    let color = aColor.toString(16);
    while (color.length < 6)
      color = "0" + color;
    return "#" + color;
  },

  getFaveIconPrimaryColor: function bh_getFaveIconPrimaryColor(aBookmarkId) {
    if (PlacesUtils.annotations.itemHasAnnotation(aBookmarkId, 'metro/faveIconColor'))
      return PlacesUtils.annotations.getItemAnnotation(aBookmarkId, 'metro/faveIconColor');
    return "";
  },

  setFaveIconPrimaryColor: function bh_setFaveIconPrimaryColor(aBookmarkId, aColorStr) {
    var anno = [{ name: "metro/faveIconColor",
                  type: Ci.nsIAnnotationService.TYPE_STRING,
                  flags: 0,
                  value: aColorStr,
                  expires: Ci.nsIAnnotationService.EXPIRE_NEVER }];
    PlacesUtils.setAnnotationsForItem(aBookmarkId, anno);
  },

  addForURI: function bh_addForURI(aURI, aTitle, callback) {
    this.isURIBookmarked(aURI, function (isBookmarked) {
      if (isBookmarked)
        return;

      let bookmarkTitle = aTitle || aURI.spec;
      let bookmarkService = PlacesUtils.bookmarks;
      let bookmarkId = bookmarkService.insertBookmark(Bookmarks.metroRoot,
                                                      aURI,
                                                      bookmarkService.DEFAULT_INDEX,
                                                      bookmarkTitle);
  
      // XXX Used for browser-chrome tests
      let event = document.createEvent("Events");
      event.initEvent("BookmarkCreated", true, false);
      window.dispatchEvent(event);

      if (callback)
        callback(bookmarkId);
    });
  },

  isURIBookmarked: function bh_isURIBookmarked(aURI, callback) {
    if (!callback)
      return;
    PlacesUtils.asyncGetBookmarkIds(aURI, function(aItemIds) {
      callback(aItemIds && aItemIds.length > 0);
    }, this);
  },

  removeForURI: function bh_removeForURI(aURI, callback) {
    // XXX blargle xpconnect! might not matter, but a method on
    // nsINavBookmarksService that takes an array of items to
    // delete would be faster. better yet, a method that takes a URI!
    PlacesUtils.asyncGetBookmarkIds(aURI, function(aItemIds) {
      aItemIds.forEach(PlacesUtils.bookmarks.removeItem);
      if (callback)
        callback(aURI, aItemIds);

      // XXX Used for browser-chrome tests
      let event = document.createEvent("Events");
      event.initEvent("BookmarkRemoved", true, false);
      window.dispatchEvent(event);
    });
  }
};

/**
 * Wraps a list/grid control implementing nsIDOMXULSelectControlElement and
 * fills it with the user's bookmarks.
 * 
 * @param           aSet    Control implementing nsIDOMXULSelectControlElement.
 * @param {Number}  aLimit  Maximum number of items to show in the view.
 * @param           aRoot   Bookmark root to show in the view.
 */
function BookmarksView(aSet, aLimit, aRoot) {
  this._set = aSet;
  this._set.controller = this;

  this._limit = aLimit;

  this._changes = new BookmarkChangeListener(this);
  PlacesUtils.bookmarks.addObserver(this._changes, false);

  // This also implicitly calls `getBookmarks`
  this.root = aRoot;
}

BookmarksView.prototype = {
  _limit: null,
  _set: null,
  _changes: null,
  _root: null,
  _sort: 0, // Natural bookmark order.

  get sort() {
    return this._sort;
  },

  set sort(aSort) {
    this._sort = aSort;
    this.getBookmarks();
  },

  get root() {
    return this._root;
  },

  set root(aRoot) {
    this._root = aRoot;
    this.getBookmarks();
  },

  handleItemClick: function bv_handleItemClick(aItem) {
    let url = aItem.getAttribute("value");
    BrowserUI.goToURI(url);
  },

  _getItemForBookmarkId: function bv__getItemForBookmark(aBookmarkId) {
    return this._set.querySelector("richgriditem[bookmarkId='" + aBookmarkId + "']");
  },

  _getBookmarkIdForItem: function bv__getBookmarkForItem(aItem) {
    return aItem.getAttribute("bookmarkId");
  },

  _updateItemWithAttrs: function dv__updateItemWithAttrs(anItem, aAttrs) {
    for (let name in aAttrs)
      anItem.setAttribute(name, aAttrs[name]);
  },

  getBookmarks: function bv_getBookmarks() {
    let options = gHistSvc.getNewQueryOptions();
    options.queryType = options.QUERY_TYPE_BOOKMARKS;
    options.excludeQueries = true; // Don't include "smart folders"
    options.maxResults = this._limit;
    options.sortingMode = this._sort;

    let query = gHistSvc.getNewQuery();
    query.setFolders([Bookmarks.metroRoot], 1);

    let result = gHistSvc.executeQuery(query, options);
    let rootNode = result.root;
    rootNode.containerOpen = true;
    let childCount = rootNode.childCount;

    for (let i = 0; i < childCount; i++) {
      let node = rootNode.getChild(i);

      // Ignore folders, separators, undefined item types, etc.
      if (node.type != node.RESULT_TYPE_URI)
        continue;

      this.addBookmark(node.itemId);
    }

    rootNode.containerOpen = false;
  },

  inCurrentView: function bv_inCurrentView(aParentId, aIndex, aItemType) {
    if (this._root && aParentId != this._root)
      return false;

    if (this._limit && aIndex >= this._limit)
      return false;

    if (aItemType != PlacesUtils.bookmarks.TYPE_BOOKMARK)
      return false;

    return true;
  },

  clearBookmarks: function bv_clearBookmarks() {
    while (this._set.itemCount > 0)
      this._set.removeItemAt(0);
  },

  addBookmark: function bv_addBookmark(aBookmarkId) {
    let bookmarks = PlacesUtils.bookmarks;

    let index = bookmarks.getItemIndex(aBookmarkId);
    let uri = bookmarks.getBookmarkURI(aBookmarkId);
    let title = bookmarks.getItemTitle(aBookmarkId) || uri.spec;
    let item = this._set.insertItemAt(index, title, uri.spec);
    item.setAttribute("bookmarkId", aBookmarkId);
    this._updateFavicon(aBookmarkId, item, uri);
  },

  _updateFavicon: function _updateFavicon(aBookmarkId, aItem, aUri) {
    PlacesUtils.favicons.getFaviconURLForPage(aUri, this._gotIcon.bind(this, aBookmarkId, aItem));
  },

  _gotIcon: function _gotIcon(aBookmarkId, aItem, aIconUri) {
    aItem.setAttribute("iconURI", aIconUri ? aIconUri.spec : "");

    let color = Bookmarks.getFaveIconPrimaryColor(aBookmarkId);
    if (color) {
      aItem.color = color;
      return;
    }
    if (!aIconUri) {
      return;
    }
    let url = Services.io.newURI(aIconUri.spec.replace("moz-anno:favicon:",""), "", null)
    let ca = Components.classes["@mozilla.org/places/colorAnalyzer;1"]
                       .getService(Components.interfaces.mozIColorAnalyzer);
    ca.findRepresentativeColor(url, function (success, color) {
      let colorStr = Bookmarks.fixupColorFormat(color);
      Bookmarks.setFaveIconPrimaryColor(aBookmarkId, colorStr);
      aItem.color = colorStr;
    }, this);
  },

  updateBookmark: function bv_updateBookmark(aBookmarkId) {
    let item = this._getItemForBookmarkId(aBookmarkId);

    if (!item)
      return;
    
    let bookmarks = PlacesUtils.bookmarks;
    let oldIndex = this._set.getIndexOfItem(item);
    let index = bookmarks.getItemIndex(aBookmarkId);

    if (oldIndex != index) {
      this.removeBookmark(aBookmarkId);
      this.addBookmark(aBookmarkId);
      return;
    }

    let uri = bookmarks.getBookmarkURI(aBookmarkId);
    let title = bookmarks.getItemTitle(aBookmarkId) || uri.spec;

    item.setAttribute("value", uri.spec);
    item.setAttribute("label", title);

    this._updateFavicon(aBookmarkId, item, uri);
  },

  removeBookmark: function bv_removeBookmark(aBookmarkId) {
    let item = this._getItemForBookmarkId(aBookmarkId);
    let index = this._set.getIndexOfItem(item);
    this._set.removeItemAt(index);
  },

  destruct: function bv_destruct() {
    PlacesUtils.bookmarks.removeObserver(this._changes);
  }
};

var BookmarksStartView = {
  _view: null,
  get _grid() { return document.getElementById("start-bookmarks-grid"); },

  init: function init() {
    this._view = new BookmarksView(this._grid, StartUI.maxResultsPerSection, Bookmarks.metroRoot);
  },

  uninit: function uninit() {
    this._view.destruct();
  },

  show: function show() {
    this._grid.arrangeItems();
  }
};

var BookmarksPanelView = {
  _view: null,

  get _grid() { return document.getElementById("bookmarks-list"); },
  get visible() { return PanelUI.isPaneVisible("bookmarks-container"); },

  init: function init() {
    this._view = new BookmarksView(this._grid, null, Bookmarks.metroRoot);
  },

  show: function show() {
    this._grid.arrangeItems();
  },

  uninit: function uninit() {
    this._view.destruct();
  }
};

/**
 * Observes bookmark changes and keeps a linked BookmarksView updated.
 *
 * @param aView An instance of BookmarksView.
 */
function BookmarkChangeListener(aView) {
  this._view = aView;
};

BookmarkChangeListener.prototype = {
  //////////////////////////////////////////////////////////////////////////////
  //// nsINavBookmarkObserver
  onBeginUpdateBatch: function () { },
  onEndUpdateBatch: function () { },

  onItemAdded: function bCL_onItemAdded(aItemId, aParentId, aIndex, aItemType, aURI, aTitle, aDateAdded, aGUID, aParentGUID) {
    if (!this._view.inCurrentView(aParentId, aIndex, aItemType))
      return;

    this._view.addBookmark(aItemId);
  },

  onItemChanged: function bCL_onItemChanged(aItemId, aProperty, aIsAnnotationProperty, aNewValue, aLastModified, aItemType, aParentId, aGUID, aParentGUID) {
    let itemIndex = PlacesUtils.bookmarks.getItemIndex(aItemId);
    if (!this._view.inCurrentView(aParentId, itemIndex, aItemType))
      return;
    
    this._view.updateBookmark(aItemId);
  },

  onItemMoved: function bCL_onItemMoved(aItemId, aOldParentId, aOldIndex, aNewParentId, aNewIndex, aItemType, aGUID, aOldParentGUID, aNewParentGUID) {
    let wasInView = this._view.inCurrentView(aOldParentId, aOldIndex, aItemType);
    let nowInView = this._view.inCurrentView(aNewParentId, aNewIndex, aItemType);

    if (!wasInView && nowInView)
      this._view.addBookmark(aItemId, aParentId, aIndex, aItemType, aURI, aTitle, aDateAdded);

    if (wasInView && !nowInView)
      this._view.removeBookmark(aItemId);
  },

  onBeforeItemRemoved: function (aItemId, aItemType, aParentId, aGUID, aParentGUID) { },
  onItemRemoved: function bCL_onItemRemoved(aItemId, aParentId, aIndex, aItemType, aURI, aGUID, aParentGUID) {
    if (!this._view.inCurrentView(aParentId, aIndex, aItemType))
      return;

    this._view.removeBookmark(aItemId);
  },

  onItemVisited: function(aItemId, aVisitId, aTime, aTransitionType, aURI, aParentId, aGUID, aParentGUID) { },

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINavBookmarkObserver])
};
