// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var BookmarksTestHelper = {
  _originalNavHistoryService: null,
  _startView:  null,
  MockNavHistoryService: {
    getNewQueryOptions: function () {
      return {};
    },
    getNewQuery: function () {
      return {
        setFolders: function(){}
      };
    },
    executeQuery: function () {
      return {
        root: {
          get childCount() {
            return Object.keys(BookmarksTestHelper._nodes).length;
          },

          getChild: function (aIndex) BookmarksTestHelper._nodes[Object.keys(BookmarksTestHelper._nodes)[aIndex]]
        }
      }
    }
  },

  _originalBookmarkService: null,
  MockBookmarkService: {
    getItemIndex: function (aIndex) aIndex,
    getBookmarkURI: function (aId) BookmarksTestHelper._nodes[aId].uri,
    getItemTitle: function (aId) BookmarksTestHelper._nodes[aId].title,
    removeItem: function (aId) {
      delete BookmarksTestHelper._nodes[aId];

      // Simulate observer notification
      BookmarksTestHelper._startView._changes.onItemRemoved(aId, BookmarksTestHelper._startView._root);
    },
  },

  Node: function (aTitle, aId) {
    this.type = this.RESULT_TYPE_URI = 0;
    this.title = aTitle;
    this.itemId = aId;
    this.uri = "http://" + aTitle + ".com.br";
    this.pinned = true
  },

  _nodes: null,
  createNodes: function (aMany) {
    this._nodes = {};
    for (let i=0; i<aMany; i++) {
      this._nodes[i] = new this.Node("Mock-Bookmark" + i, i);
    }
  },

  _originalPinHelper: null,
  MockPinHelper: {
    isPinned: function (aItem) BookmarksTestHelper._nodes[aItem].pinned,
    setUnpinned: function (aItem) BookmarksTestHelper._nodes[aItem].pinned = false,
    setPinned: function (aItem) BookmarksTestHelper._nodes[aItem].pinned = true,
  },

  _originalUpdateFavicon: null,
  setup: function setup() {
    this._startView = Browser.selectedBrowser.contentWindow.BookmarksStartView._view;

    // Just enough items so that there will be one less then the limit
    // after removing 4 items.
    this.createNodes(this._startView.maxTiles + 3);

    this._originalNavHistoryService = this._startView._navHistoryService;
    this._startView._navHistoryService = this.MockNavHistoryService;

    this._originalBookmarkService = this._startView._bookmarkService;
    this._startView._bookmarkService= this.MockBookmarkService;

    this._originalPinHelper = this._startView._pinHelper;
    this._startView._pinHelper = this.MockPinHelper;

    this._originalUpdateFavicon = this._startView._updateFavicon;
    this._startView._updateFavicon = function () {};

    this._startView.clearBookmarks();
    this._startView.getBookmarks();
  },

  restore: function () {
    this._startView._navHistoryService = this._originalNavHistoryService;
    this._startView._bookmarkService= this._originalBookmarkService;
    this._startView._pinHelper = this._originalPinHelper;
    this._startView._updateFavicon = this._originalUpdateFavicon;

    this._startView.clearBookmarks();
    this._startView.getBookmarks();
  }
};
