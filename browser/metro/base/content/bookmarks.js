/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

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
