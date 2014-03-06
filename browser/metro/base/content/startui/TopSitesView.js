/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

Cu.import("resource://gre/modules/PageThumbs.jsm");

function TopSitesView(aGrid) {
  View.call(this, aGrid);
  // View monitors this for maximum tile display counts
  this.tilePrefName = "browser.display.startUI.topsites.maxresults";
  this.showing = this.maxTiles > 0 && !this.isFirstRun();

  // clean up state when the appbar closes
  StartUI.chromeWin.addEventListener('MozAppbarDismissing', this, false);
  let history = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
  history.addObserver(this, false);

  Services.obs.addObserver(this, "Metro:RefreshTopsiteThumbnail", false);

  NewTabUtils.allPages.register(this);
  TopSites.prepareCache().then(function(){
    this.populateGrid();
  }.bind(this));
}

TopSitesView.prototype = Util.extend(Object.create(View.prototype), {
  _set:null,
  // _lastSelectedSites used to temporarily store blocked/removed sites for undo/restore-ing
  _lastSelectedSites: null,
  // isUpdating used only for testing currently
  isUpdating: false,

  // For View's showing property
  get vbox() {
    return document.getElementById("start-topsites");
  },

  destruct: function destruct() {
    Services.obs.removeObserver(this, "Metro:RefreshTopsiteThumbnail");
    NewTabUtils.allPages.unregister(this);
    if (StartUI.chromeWin) {
      StartUI.chromeWin.removeEventListener('MozAppbarDismissing', this, false);
    }
    View.prototype.destruct.call(this);
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
          pinIndices.push( Array.indexOf(aNode.control.items, aNode) );
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
        break;
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
      this.populateGrid();
    }
  },

  updateTile: function(aTileNode, aSite, aArrangeGrid) {
    if (!(aSite && aSite.url)) {
      throw new Error("Invalid Site object passed to TopSitesView updateTile");
    }
    this._updateFavicon(aTileNode, Util.makeURI(aSite.url));

    Task.spawn(function() {
      let filepath = PageThumbsStorage.getFilePathForURL(aSite.url);
      if (yield OS.File.exists(filepath)) {
        aSite.backgroundImage = 'url("'+PageThumbs.getThumbnailURL(aSite.url)+'")';
        // use the setter when available to update the backgroundImage value
        if ('backgroundImage' in aTileNode &&
            aTileNode.backgroundImage != aSite.backgroundImage) {
          aTileNode.backgroundImage = aSite.backgroundImage;
        } else {
          // just update the attribute for when the node gets the binding applied
          aTileNode.setAttribute("customImage", aSite.backgroundImage);
        }
      }
    });

    aSite.applyToTileNode(aTileNode);
    if (aTileNode.refresh) {
      aTileNode.refresh();
    }
    if (aArrangeGrid) {
      this._set.arrangeItems();
    }
  },

  populateGrid: function populateGrid() {
    this.isUpdating = true;

    let sites = TopSites.getSites();

    let tileset = this._set;
    tileset.clearAll(true);

    if (!this.maxTiles) {
      this.isUpdating = false;
      return;
    } else {
      sites = sites.slice(0, this.maxTiles);
    }

    for (let site of sites) {
      let slot = tileset.nextSlot();
      this.updateTile(slot, site);
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

  isFirstRun: function isFirstRun() {
    return Services.prefs.getBoolPref("browser.firstrun.show.localepicker");
  },

  _adjustDOMforViewState: function _adjustDOMforViewState(aState) {
    if (!this._set)
      return;
    if (!aState)
      aState = this._set.getAttribute("viewstate");

    View.prototype._adjustDOMforViewState.call(this, aState);

    // Don't show thumbnails in snapped view.
    if (aState == "snapped") {
      document.getElementById("start-topsites-grid").removeAttribute("tiletype");
    } else {
      document.getElementById("start-topsites-grid").setAttribute("tiletype", "thumbnail");
    }

    // propogate tiletype changes down to tile children
    let tileType = this._set.getAttribute("tiletype");
    for (let item of this._set.children) {
      if (tileType) {
        item.setAttribute("tiletype", tileType);
      } else {
        item.removeAttribute("tiletype");
      }
    }
  },

  refreshView: function () {
    this.populateGrid();
  },

  // nsIObservers
  observe: function (aSubject, aTopic, aState) {
    switch (aTopic) {
      case "Metro:RefreshTopsiteThumbnail":
        this.forceReloadOfThumbnail(aState);
        break;
    }
    View.prototype.observe.call(this, aSubject, aTopic, aState);
    this.showing = this.maxTiles > 0 && !this.isFirstRun();
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
    if ('clearAll' in this._set)
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
    this._view = new TopSitesView(this._grid);
    this._grid.removeAttribute("fade");
  },

  uninit: function uninit() {
    if (this._view) {
      this._view.destruct();
    }
  },
};
