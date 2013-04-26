// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';


function HistoryView(aSet) {
  this._set = aSet;
  this._set.controller = this;
  this._inBatch = false;

  let history = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
  history.addObserver(this, false);
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

      this.addItemToSet(uri, title, node.icon);
    }

    rootNode.containerOpen = false;
  },

  destruct: function destruct() {
  },

  // nsINavHistoryObserver & helpers

  addItemToSet: function addItemToSet(uri, title, icon) {
    let item = this._set.appendItem(title, uri, this._inBatch);
    item.setAttribute("iconURI", icon);
  },

  // TODO rebase/merge Alert: bug 831916 's patch merges in,
  // this can be replaced with the updated calls to populateGrid()
  refreshAndRepopulate: function() {
    this._set.clearAll();
    this.populateGrid();
  },

  onBeginUpdateBatch: function() {
    // Avoid heavy grid redraws while a batch is in process
    this._inBatch = true;
  },

  onEndUpdateBatch: function() {
    this._inBatch = false;
    this.refreshAndRepopulate();
  },

  onVisit: function(aURI, aVisitID, aTime, aSessionID,
                    aReferringID, aTransitionType) {
    if (!this._inBatch) {
      this.refreshAndRepopulate();
    }
  },

  onTitleChanged: function(aURI, aPageTitle) {
    let changedItems = this._set.getItemsByUrl(aURI.spec);
    for (let item of changedItems) {
      item.setAttribute("label", aPageTitle);
    }
  },

  onDeleteURI: function(aURI) {
    for (let item of this._set.getItemsByUrl(aURI.spec)) {
      this._set.removeItem(item, this._inBatch);
    }
  },

  onClearHistory: function() {
    this._set.clearAll();
  },

  onPageChanged: function(aURI, aWhat, aValue) {
    if (aWhat ==  Ci.nsINavHistoryObserver.ATTRIBUTE_FAVICON) {
      let changedItems = this._set.getItemsByUrl(aURI.spec);
      for (let item of changedItems) {
        let currIcon = item.getAttribute("iconURI");
        if (currIcon != aValue) {
          item.setAttribute("iconURI", aValue);
        }
      }
    }
  },

  onDeleteVisits: function (aURI, aVisitTime, aGUID, aReason, aTransitionType) {
    if ((aReason ==  Ci.nsINavHistoryObserver.REASON_DELETED) && !this._inBatch) {
      this.refreshAndRepopulate();
    }
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsINavHistoryObserver) ||
        iid.equals(Components.interfaces.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
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
