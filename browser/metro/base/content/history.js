/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

function HistoryView(aSet, aLimit, aFilterUnpinned) {
  this._set = aSet;
  this._set.controller = this;
  this._inBatch = 0;

  this._limit = aLimit;
  this._filterUnpinned = aFilterUnpinned;
  this._historyService = PlacesUtils.history;
  this._navHistoryService = gHistSvc;

  this._pinHelper = new ItemPinHelper("metro.history.unpinned");
  this._historyService.addObserver(this, false);
  window.addEventListener('MozAppbarDismissing', this, false);
  window.addEventListener('HistoryNeedsRefresh', this, false);
}

HistoryView.prototype = Util.extend(Object.create(View.prototype), {
  _set: null,
  _toRemove: null,

  handleItemClick: function tabview_handleItemClick(aItem) {
    let url = aItem.getAttribute("value");
    BrowserUI.goToURI(url);
  },

  populateGrid: function populateGrid(aRefresh) {
    this._inBatch++; // always batch up grid updates to avoid redundant arrangeItems calls
    let query = this._navHistoryService.getNewQuery();
    let options = this._navHistoryService.getNewQueryOptions();
    options.excludeQueries = true;
    options.queryType = options.QUERY_TYPE_HISTORY;
    options.resultType = options.RESULTS_AS_URI;
    options.sortingMode = options.SORT_BY_DATE_DESCENDING;

    let limit = this._limit || Infinity;
    let result = this._navHistoryService.executeQuery(query, options);
    let rootNode = result.root;
    rootNode.containerOpen = true;
    let childCount = rootNode.childCount;

    for (let i = 0, addedCount = 0; i < childCount && addedCount < limit; i++) {
      let node = rootNode.getChild(i);
      let uri = node.uri;
      let title = (node.title && node.title.length) ? node.title : uri;

      // If item is marked for deletion, skip it.
      if (this._toRemove && this._toRemove.indexOf(uri) !== -1)
        continue;

      let items = this._set.getItemsByUrl(uri);

      // Item has been unpinned, skip if filterUnpinned set.
      if (this._filterUnpinned && !this._pinHelper.isPinned(uri)) {
        if (items.length > 0)
          this.removeHistory(uri);

        continue;
      }

      if (!aRefresh || items.length === 0) {
        // If we're not refreshing or the item is not in the grid, add it.
        this.addItemToSet(uri, title, node.icon, addedCount);
      } else if (aRefresh && items.length > 0) {
        // Update context action in case it changed in another view.
        for (let item of items) {
          this._setContextActions(item);
        }
      }

      addedCount++;
    }

    // Remove extra items in case a refresh added more than the limit.
    // This can happen when undoing a delete.
    if (aRefresh) {
      while (this._set.itemCount > limit)
        this._set.removeItemAt(this._set.itemCount - 1);
    }

    rootNode.containerOpen = false;
    this._set.arrangeItems();
    if (this._inBatch > 0)
      this._inBatch--;
  },

  destruct: function destruct() {
    this._historyService.removeObserver(this);
    window.removeEventListener('MozAppbarDismissing', this, false);
    window.removeEventListener('HistoryNeedsRefresh', this, false);
  },

  addItemToSet: function addItemToSet(aURI, aTitle, aIcon, aPos) {
    let item = this._set.insertItemAt(aPos || 0, aTitle, aURI, this._inBatch);
    this._setContextActions(item);
    this._updateFavicon(item, aURI);
  },

  _setContextActions: function bv__setContextActions(aItem) {
    let uri = aItem.getAttribute("value");
    aItem.setAttribute("data-contextactions", "delete," + (this._pinHelper.isPinned(uri) ? "unpin" : "pin"));
    if (aItem.refresh) aItem.refresh();
  },

  _sendNeedsRefresh: function bv__sendNeedsRefresh(){
    // Event sent when all views need to refresh.
    let event = document.createEvent("Events");
    event.initEvent("HistoryNeedsRefresh", true, false);
    window.dispatchEvent(event);
  },

  removeHistory: function (aUri) {
    let items = this._set.getItemsByUrl(aUri);
    for (let item of items)
      this._set.removeItem(item, true);
    if (!this._inBatch)
      this._set.arrangeItems();
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

          let uri = aNode.getAttribute("value");

          this._toRemove.push(uri);
          this.removeHistory(uri);
        }, this);

        // stop the appbar from dismissing
        aEvent.preventDefault();

        // at next tick, re-populate the context appbar.
        setTimeout(function(){
          // fire a MozContextActionsChange event to update the context appbar
          let event = document.createEvent("Events");
          // we need the restore button to show (the tile node will go away though)
          event.actions = ["restore"];
          event.noun = tileGroup.contextNoun;
          event.qty = selectedTiles.length;
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
          let uri = aNode.getAttribute("value");

          if (this._filterUnpinned)
            this.removeHistory(uri);

          this._pinHelper.setUnpinned(uri);
        }, this);
        break;

      case "pin":
        Array.forEach(selectedTiles, function(aNode) {
          let uri = aNode.getAttribute("value");

          this._pinHelper.setPinned(uri);
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
          for (let uri of this._toRemove) {
            this._historyService.removePage(NetUtil.newURI(uri));
          }

          // Clear context app bar
          let event = document.createEvent("Events");
          event.initEvent("MozContextActionsChange", true, false);
          this._set.dispatchEvent(event);

          this._toRemove = null;
        }
        break;

      case "HistoryNeedsRefresh":
        this.populateGrid(true);
        break;
    }
  },

  // nsINavHistoryObserver & helpers

  onBeginUpdateBatch: function() {
    // Avoid heavy grid redraws while a batch is in process
    this._inBatch++;
  },

  onEndUpdateBatch: function() {
    this.populateGrid(true);
    if (this._inBatch > 0) {
      this._inBatch--;
      this._set.arrangeItems();
    }
  },

  onVisit: function(aURI, aVisitID, aTime, aSessionID,
                    aReferringID, aTransitionType) {
    if (!this._inBatch) {
      this.populateGrid(true);
    }
  },

  onTitleChanged: function(aURI, aPageTitle) {
    let changedItems = this._set.getItemsByUrl(aURI.spec);
    for (let item of changedItems) {
      item.setAttribute("label", aPageTitle);
    }
  },

  onDeleteURI: function(aURI) {
    this.removeHistory(aURI.spec);
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
      this.populateGrid(true);
    }
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsINavHistoryObserver) ||
        iid.equals(Components.interfaces.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
});

let HistoryStartView = {
  _view: null,
  get _grid() { return document.getElementById("start-history-grid"); },

  show: function show() {
    this._grid.arrangeItems();
  },

  init: function init() {
    this._view = new HistoryView(this._grid, StartUI.maxResultsPerSection, true);
    this._view.populateGrid();
  },

  uninit: function uninit() {
    this._view.destruct();
  }
};
