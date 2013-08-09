/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

let prefs = Components.classes["@mozilla.org/preferences-service;1"].
      getService(Components.interfaces.nsIPrefBranch);

Cu.import("resource://gre/modules/PageThumbs.jsm");
Cu.import("resource:///modules/colorUtils.jsm");

function TopSitesView(aGrid, aMaxSites) {
  this._set = aGrid;
  this._set.controller = this;
  this._topSitesMax = aMaxSites;

  // clean up state when the appbar closes
  StartUI.chromeWin.addEventListener('MozAppbarDismissing', this, false);
  let history = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
  history.addObserver(this, false);

  PageThumbs.addExpirationFilter(this);
  Services.obs.addObserver(this, "Metro:RefreshTopsiteThumbnail", false);
  Services.obs.addObserver(this, "metro_viewstate_changed", false);

  NewTabUtils.allPages.register(this);
  TopSites.prepareCache().then(function(){
    this.populateGrid();
  }.bind(this));
}

TopSitesView.prototype = Util.extend(Object.create(View.prototype), {
  _set:null,
  _topSitesMax: null,
  // _lastSelectedSites used to temporarily store blocked/removed sites for undo/restore-ing
  _lastSelectedSites: null,
  // isUpdating used only for testing currently
  isUpdating: false,

  destruct: function destruct() {
    Services.obs.removeObserver(this, "Metro:RefreshTopsiteThumbnail");
    Services.obs.removeObserver(this, "metro_viewstate_changed");
    PageThumbs.removeExpirationFilter(this);
    NewTabUtils.allPages.unregister(this);
    if (StartUI.chromeWin) {
      StartUI.chromeWin.removeEventListener('MozAppbarDismissing', this, false);
    }
  },

  handleItemClick: function tabview_handleItemClick(aItem) {
    let url = aItem.getAttribute("value");
    StartUI.goToURI(url);
  },

  doActionOnSelectedTiles: function(aActionName, aEvent) {
    let tileGroup = this._set;
    let selectedTiles = tileGroup.selectedItems;
    let sites = Array.map(selectedTiles, TopSites._linkFromNode);
    let nextContextActions = new Set();

    switch (aActionName){
      case "delete":
        for (let aNode of selectedTiles) {
          // add some class to transition element before deletion?
          aNode.contextActions.delete('delete');
          // we need new context buttons to show (the tile node will go away though)
        }
        this._lastSelectedSites = (this._lastSelectedSites || []).concat(sites);
        // stop the appbar from dismissing
        aEvent.preventDefault();
        nextContextActions.add('restore');
        TopSites.hideSites(sites);
        break;
      case "restore":
        // usually restore is an undo action, so we have to recreate the tiles and grid selection
        if (this._lastSelectedSites) {
          let selectedUrls = this._lastSelectedSites.map((site) => site.url);
          // re-select the tiles once the tileGroup is done populating and arranging
          tileGroup.addEventListener("arranged", function _onArranged(aEvent){
            for (let url of selectedUrls) {
              let tileNode = tileGroup.querySelector("richgriditem[value='"+url+"']");
              if (tileNode) {
                tileNode.setAttribute("selected", true);
              }
            }
            tileGroup.removeEventListener("arranged", _onArranged, false);
            // <sfoster> we can't just call selectItem n times on tileGroup as selecting means trigger the default action
            // for seltype="single" grids.
            // so we toggle the attributes and raise the selectionchange "manually"
            let event = tileGroup.ownerDocument.createEvent("Events");
            event.initEvent("selectionchange", true, true);
            tileGroup.dispatchEvent(event);
          }, false);

          TopSites.restoreSites(this._lastSelectedSites);
          // stop the appbar from dismissing,
          // the selectionchange event will trigger re-population of the context appbar
          aEvent.preventDefault();
        }
        break;
      case "pin":
        let pinIndices = [];
        Array.forEach(selectedTiles, function(aNode) {
          pinIndices.push( Array.indexOf(aNode.control.children, aNode) );
          aNode.contextActions.delete('pin');
          aNode.contextActions.add('unpin');
        });
        TopSites.pinSites(sites, pinIndices);
        break;
      case "unpin":
        Array.forEach(selectedTiles, function(aNode) {
          aNode.contextActions.delete('unpin');
          aNode.contextActions.add('pin');
        });
        TopSites.unpinSites(sites);
        break;
      // default: no action
    }
    if (nextContextActions.size) {
      // at next tick, re-populate the context appbar
      setTimeout(function(){
        // fire a MozContextActionsChange event to update the context appbar
        let event = document.createEvent("Events");
        event.actions = [...nextContextActions];
        event.initEvent("MozContextActionsChange", true, false);
        tileGroup.dispatchEvent(event);
      },0);
    }
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type){
      case "MozAppbarDismissing":
        // clean up when the context appbar is dismissed - we don't remember selections
        this._lastSelectedSites = null;
    }
  },

  update: function() {
    // called by the NewTabUtils.allPages.update, notifying us of data-change in topsites
    let grid = this._set,
        dirtySites = TopSites.dirty();

    if (dirtySites.size) {
      // we can just do a partial update and refresh the node representing each dirty tile
      for (let site of dirtySites) {
        let tileNode = grid.querySelector("[value='"+site.url+"']");
        if (tileNode) {
          this.updateTile(tileNode, new Site(site));
        }
      }
    } else {
        // flush, recreate all
      this.isUpdating = true;
      // destroy and recreate all item nodes, skip calling arrangeItems
      grid.clearAll(true);
      this.populateGrid();
    }
  },

  updateTile: function(aTileNode, aSite, aArrangeGrid) {
    this._updateFavicon(aTileNode, Util.makeURI(aSite.url));

    Task.spawn(function() {
      let filepath = PageThumbsStorage.getFilePathForURL(aSite.url);
      if (yield OS.File.exists(filepath)) {
        aSite.backgroundImage = 'url("'+PageThumbs.getThumbnailURL(aSite.url)+'")';
        aTileNode.setAttribute("customImage", aSite.backgroundImage);
        if (aTileNode.refresh) {
          aTileNode.refresh()
        }
      }
    });

    aSite.applyToTileNode(aTileNode);
    if (aArrangeGrid) {
      this._set.arrangeItems();
    }
  },

  populateGrid: function populateGrid() {
    this.isUpdating = true;

    let sites = TopSites.getSites();
    let length = Math.min(sites.length, this._topSitesMax || Infinity);
    let tileset = this._set;

    // if we're updating with a collection that is smaller than previous
    // remove any extra tiles
    while (tileset.children.length > length) {
      tileset.removeChild(tileset.children[tileset.children.length -1]);
    }

    for (let idx=0; idx < length; idx++) {
      let isNew = !tileset.children[idx],
          site = sites[idx];
      let item = isNew ? tileset.createItemElement(site.title, site.url) : tileset.children[idx];

      this.updateTile(item, site);
      if (isNew) {
        tileset.appendChild(item);
      }
    }
    tileset.arrangeItems();
    this.isUpdating = false;
  },

  forceReloadOfThumbnail: function forceReloadOfThumbnail(url) {
    let nodes = this._set.querySelectorAll('richgriditem[value="'+url+'"]');
    for (let item of nodes) {
      if ("isBound" in item && item.isBound) {
        item.refreshBackgroundImage();
      }
    }
  },

  filterForThumbnailExpiration: function filterForThumbnailExpiration(aCallback) {
    aCallback([item.getAttribute("value") for (item of this._set.children)]);
  },

  isFirstRun: function isFirstRun() {
    return prefs.getBoolPref("browser.firstrun.show.localepicker");
  },

  // nsIObservers
  observe: function (aSubject, aTopic, aState) {
    switch(aTopic) {
      case "Metro:RefreshTopsiteThumbnail":
        this.forceReloadOfThumbnail(aState);
        break;
      case "metro_viewstate_changed":
        this.onViewStateChange(aState);
        for (let item of this._set.children) {
          if (aState == "snapped") {
            item.removeAttribute("tiletype");
          } else {
            item.setAttribute("tiletype", "thumbnail");
          }
        }
        break;
    }
  },

  // nsINavHistoryObserver
  onBeginUpdateBatch: function() {
  },

  onEndUpdateBatch: function() {
  },

  onVisit: function(aURI, aVisitID, aTime, aSessionID,
                    aReferringID, aTransitionType) {
  },

  onTitleChanged: function(aURI, aPageTitle) {
  },

  onDeleteURI: function(aURI) {
  },

  onClearHistory: function() {
    this._set.clearAll();
  },

  onPageChanged: function(aURI, aWhat, aValue) {
  },

  onDeleteVisits: function (aURI, aVisitTime, aGUID, aReason, aTransitionType) {
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsINavHistoryObserver) ||
        iid.equals(Components.interfaces.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }

});

let TopSitesStartView = {
  _view: null,
  get _grid() { return document.getElementById("start-topsites-grid"); },

  init: function init() {
    this._view = new TopSitesView(this._grid, 8);
    if (this._view.isFirstRun()) {
      let topsitesVbox = document.getElementById("start-topsites");
      topsitesVbox.setAttribute("hidden", "true");
    }
  },

  uninit: function uninit() {
    if (this._view) {
      this._view.destruct();
    }
  },

  show: function show() {
    this._grid.arrangeItems();
  }
};
