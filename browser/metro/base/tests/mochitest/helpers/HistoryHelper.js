// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var HistoryTestHelper = {
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
            return Object.keys(HistoryTestHelper._nodes).length;
          },

          getChild: function (aIndex) HistoryTestHelper._nodes[Object.keys(HistoryTestHelper._nodes)[aIndex]]
        }
      }
    }
  },

  _originalHistoryService: null,
  MockHistoryService: {
    removePage: function (aURI) {
      delete HistoryTestHelper._nodes[aURI.spec];

      // Simulate observer notification
      HistoryTestHelper._startView.onDeleteURI(aURI);
    },
  },

  Node: function (aTitle, aURISpec) {
    this.title = aTitle;
    this.uri = aURISpec;
    this.pinned = true
  },

  _nodes: null,
  createNodes: function (aMany) {
    this._nodes = {};
    for (let i=0; i<aMany; i++) {
      let title = "mock-history-" + i;
      let uri = "http://" + title + ".com.br/";

      this._nodes[uri] = new this.Node(title, uri);
    }
  },

  _originalPinHelper: null,
  MockPinHelper: {
    isPinned: function (aItem) HistoryTestHelper._nodes[aItem].pinned,
    setUnpinned: function (aItem) HistoryTestHelper._nodes[aItem].pinned = false,
    setPinned: function (aItem) HistoryTestHelper._nodes[aItem].pinned = true,
  },

  setup: function setup() {
    this._startView = Browser.selectedBrowser.contentWindow.HistoryStartView._view;

    // Just enough items so that there will be one less then the limit
    // after removing 4 items.
    this.createNodes(this._startView._limit + 3);

    this._originalNavHistoryService = this._startView._navHistoryService;
    this._startView._navHistoryService = this.MockNavHistoryService;

    this._originalHistoryService = this._startView._historyService;
    this._startView._historyService= this.MockHistoryService;

    this._originalPinHelper = this._startView._pinHelper;
    this._startView._pinHelper = this.MockPinHelper;

    this._startView._set.clearAll();
    this._startView.populateGrid();
  },

  restore: function () {
    this._startView._navHistoryService = this._originalNavHistoryService;
    this._startView._historyService= this._originalHistoryService;
    this._startView._pinHelper = this._originalPinHelper;

    this._startView._set.clearAll();
    this._startView.populateGrid();
  }
};
