// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';


// TODO <jwilde>: Observe changes in history with nsINavHistoryObserver
function HistoryView(aSet) {
  this._set = aSet;
  this._set.controller = this;

  //add observers here
}

HistoryView.prototype = {
  _set:null,

  handleItemClick: function tabview_handleItemClick(aItem) {
    let url = aItem.getAttribute("value");
    BrowserUI.goToURI(url);
  },

  populateGrid: function populateGrid() {
    let query = gHistSvc.getNewQuery();
    let options = gHistSvc.getNewQueryOptions();
    options.excludeQueries = true;
    options.queryType = options.QUERY_TYPE_HISTORY;
    options.maxResults = StartUI.maxResultsPerSection;
    options.resultType = options.RESULTS_AS_URI;
    options.sortingMode = options.SORT_BY_DATE_DESCENDING;

    let result = gHistSvc.executeQuery(query, options);
    let rootNode = result.root;
    rootNode.containerOpen = true;
    let childCount = rootNode.childCount;

    for (let i = 0; i < childCount; i++) {
      let node = rootNode.getChild(i);
      let uri = node.uri;
      let title = node.title || uri;

      let item = this._set.appendItem(title, uri);
      item.setAttribute("iconURI", node.icon);
    }

    rootNode.containerOpen = false;
  },

  destruct: function destruct() {
  },
};

let HistoryStartView = {
  _view: null,
  get _grid() { return document.getElementById("start-history-grid"); },

  show: function show() {
    this._grid.arrangeItems();
  },

  init: function init() {
    this._view = new HistoryView(this._grid);
    this._view.populateGrid();
  },

  uninit: function uninit() {
    this._view.destruct();
  }
};

let HistoryPanelView = {
  _view: null,
  get _grid() { return document.getElementById("history-list"); },
  get visible() { return PanelUI.isPaneVisible("history-container"); },

  show: function show() {
    this._grid.arrangeItems();
  },

  init: function init() {
    this._view = new HistoryView(this._grid);
    this._view.populateGrid();
  },

  uninit: function uninit() {
    this._view.destruct();
  }
};

