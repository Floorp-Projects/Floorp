// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';
 let prefs = Components.classes["@mozilla.org/preferences-service;1"].
      getService(Components.interfaces.nsIPrefBranch);
Cu.import("resource://gre/modules/PageThumbs.jsm");
Cu.import("resource:///modules/colorUtils.jsm");

/**
 * singleton to provide data-level functionality to the views
 */
let TopSites = {
  _initialized: false,

  Site: Site,

  prepareCache: function(aForce){
    // front to the NewTabUtils' links cache
    //  -ensure NewTabUtils.links links are pre-cached

    // avoid re-fetching links data while a fetch is in flight
    if (this._promisedCache && !aForce) {
      return this._promisedCache;
    }
    let deferred = Promise.defer();
    this._promisedCache = deferred.promise;

    NewTabUtils.links.populateCache(function () {
      deferred.resolve();
      this._promisedCache = null;
      this._sites = null;  // reset our sites cache so they are built anew
      this._sitesDirty.clear();
    }.bind(this), true);
    return this._promisedCache;
  },

  _sites: null,
  _sitesDirty: new Set(),
  getSites: function() {
    if (this._sites) {
      return this._sites;
    }

    let links = NewTabUtils.links.getLinks();
    let sites = links.map(function(aLink){
      let site = new Site(aLink);
      return site;
    });

    // reset state
    this._sites = sites;
    this._sitesDirty.clear();
    return this._sites;
  },

  /**
   * Get list of top site as in need of update/re-render
   * @param aSite Optionally add Site arguments to be refreshed/updated
   */
  dirty: function() {
    // add any arguments for more fine-grained updates rather than invalidating the whole collection
    for (let i=0; i<arguments.length; i++) {
      this._sitesDirty.add(arguments[i]);
    }
    return this._sitesDirty;
  },

  /**
   * Cause update of top sites
   */
  update: function() {
    NewTabUtils.allPages.update();
    // then clear all the dirty flags
    this._sitesDirty.clear();
  },

  /**
   * Pin top site at a given index
   * @param aSites array of sites to be pinned
   * @param aSlotIndices indices corresponding to the site to be pinned
   */
  pinSites: function(aSites, aSlotIndices) {
    if (aSites.length !== aSlotIndices.length)
        throw new Error("TopSites.pinSites: Mismatched sites/indices arguments");

    for (let i=0; i<aSites.length && i<aSlotIndices.length; i++){
      let site = aSites[i],
          idx = aSlotIndices[i];
      if (!(site && site.url)) {
        throw Cr.NS_ERROR_INVALID_ARG
      }
      // pinned state is a pref, using Storage apis therefore sync
      NewTabUtils.pinnedLinks.pin(site, idx);
      this.dirty(site);
    }
    this.update();
  },

  /**
   * Unpin top sites
   * @param aSites array of sites to be unpinned
   */
  unpinSites: function(aSites) {
    for (let site of aSites) {
      if (!(site && site.url)) {
        throw Cr.NS_ERROR_INVALID_ARG
      }
      // pinned state is a pref, using Storage apis therefore sync
      NewTabUtils.pinnedLinks.unpin(site);
      this.dirty(site);
    }
    this.update();
  },

  /**
   * Hide (block) top sites
   * @param aSites array of sites to be unpinned
   */
  hideSites: function(aSites) {
    for (let site of aSites) {
      if (!(site && site.url)) {
        throw Cr.NS_ERROR_INVALID_ARG
      }

      site._restorePinIndex = NewTabUtils.pinnedLinks._indexOfLink(site);
      // blocked state is a pref, using Storage apis therefore sync
      NewTabUtils.blockedLinks.block(site);
    }
    // clear out the cache, we'll fetch and re-render
    this._sites = null;
    this._sitesDirty.clear();
    this.update();
  },

  /**
   * Un-hide (restore) top sites
   * @param aSites array of sites to be restored
   */
  restoreSites: function(aSites) {
    for (let site of aSites) {
      if (!(site && site.url)) {
        throw Cr.NS_ERROR_INVALID_ARG
      }
      NewTabUtils.blockedLinks.unblock(site);
      let pinIndex = site._restorePinIndex;

      if (!isNaN(pinIndex) && pinIndex > -1) {
        NewTabUtils.pinnedLinks.pin(site, pinIndex);
      }
    }
    // clear out the cache, we'll fetch and re-render
    this._sites = null;
    this._sitesDirty.clear();
    this.update();
  },
  _linkFromNode: function _linkFromNode(aNode) {
    return {
      url: aNode.getAttribute("value"),
      title: aNode.getAttribute("label")
    };
  }
};
// The value of useThumbs should not be changed over the lifetime of
//   the object.
function TopSitesView(aGrid, aMaxSites, aUseThumbnails) {
  this._set = aGrid;
  this._set.controller = this;
  this._topSitesMax = aMaxSites;
  this._useThumbs = aUseThumbnails;
  // clean up state when the appbar closes
  window.addEventListener('MozAppbarDismissing', this, false);
  let history = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
  history.addObserver(this, false);
  if (this._useThumbs) {
    PageThumbs.addExpirationFilter(this);
    Services.obs.addObserver(this, "Metro:RefreshTopsiteThumbnail", false);
  }

  NewTabUtils.allPages.register(this);
  TopSites.prepareCache().then(function(){
    this.populateGrid();
  }.bind(this));
}

TopSitesView.prototype = {
  _set:null,
  _topSitesMax: null,
  // _lastSelectedSites used to temporarily store blocked/removed sites for undo/restore-ing
  _lastSelectedSites: null,
  // isUpdating used only for testing currently
  isUpdating: false,

  handleItemClick: function tabview_handleItemClick(aItem) {
    let url = aItem.getAttribute("value");
    BrowserUI.goToURI(url);
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
        nextContextActions.add('restore');
        TopSites.hideSites(sites);
        break;
      case "restore":
        // usually restore is an undo action, so there's no tiles/selection to act on
        if (this._lastSelectedSites) {
          TopSites.restoreSites(this._lastSelectedSites);
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
      // stop the appbar from dismissing
      aEvent.preventDefault();
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
        this._set.clearSelection();
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
      // destroy and recreate all item nodes
      let item;
      while ((item = grid.firstChild)){
        grid.removeChild(item);
      }
      this.populateGrid();
    }
  },

  updateTile: function(aTileNode, aSite, aArrangeGrid) {
    PlacesUtils.favicons.getFaviconURLForPage(Util.makeURI(aSite.url), function(iconURLfromSiteURL) {
      if (!iconURLfromSiteURL) {
        return;
      }
      aTileNode.iconSrc = iconURLfromSiteURL.spec;
      let faviconURL = (PlacesUtils.favicons.getFaviconLinkForIcon(iconURLfromSiteURL)).spec;
      let xpFaviconURI = Util.makeURI(faviconURL.replace("moz-anno:favicon:",""));
      ColorUtils.getForegroundAndBackgroundIconColors(xpFaviconURI, function(foreground, background) {
        aTileNode.style.color = foreground; //color text
        aTileNode.setAttribute("customColor", background);
        if (aTileNode.refresh) {
          aTileNode.refresh();
        }
      });
    });

    if (this._useThumbs) {
      aSite.backgroundImage = 'url("'+PageThumbs.getThumbnailURL(aSite.url)+'")';
    } else {
      delete aSite.backgroundImage;
    }
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
          item = tileset.children[idx] || document.createElement("richgriditem"),
          site = sites[idx];

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
        item.refreshBackgroundImage();
      }
  },
  filterForThumbnailExpiration: function filterForThumbnailExpiration(aCallback) {
    aCallback([item.getAttribute("value") for (item of this._set.children)]);
  },

  isFirstRun: function isFirstRun() {
    return prefs.getBoolPref("browser.firstrun.show.localepicker");
  },

  destruct: function destruct() {
    if (this._useThumbs) {
      Services.obs.removeObserver(this, "Metro:RefreshTopsiteThumbnail");
      PageThumbs.removeExpirationFilter(this);
    }
    window.removeEventListener('MozAppbarDismissing', this, false);
  },

  // nsIObservers
  observe: function (aSubject, aTopic, aState) {
    switch(aTopic) {
      case "Metro:RefreshTopsiteThumbnail":
        this.forceReloadOfThumbnail(aState);
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

  onPageExpired: function(aURI, aVisitTime, aWholeEntry) {
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsINavHistoryObserver) ||
        iid.equals(Components.interfaces.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }

};

let TopSitesStartView = {
  _view: null,
  get _grid() { return document.getElementById("start-topsites-grid"); },

  init: function init() {
    this._view = new TopSitesView(this._grid, 8, true);
    if (this._view.isFirstRun()) {
      let topsitesVbox = document.getElementById("start-topsites");
      topsitesVbox.setAttribute("hidden", "true");
    }
  },
  uninit: function uninit() {
    this._view.destruct();
  },

  show: function show() {
    this._grid.arrangeItems();
  },
};

let TopSitesSnappedView = {
  get _grid() { return document.getElementById("snapped-topsites-grid"); },

  show: function show() {
    this._grid.arrangeItems();
  },

  init: function() {
    this._view = new TopSitesView(this._grid, 8);
    if (this._view.isFirstRun()) {
      let topsitesVbox = document.getElementById("snapped-topsites");
      topsitesVbox.setAttribute("hidden", "true");
    }
    Services.obs.addObserver(this, "metro_viewstate_dom_snapped", false);
  },

  uninit: function uninit() {
    this._view.destruct();
    Services.obs.removeObserver(this, "metro_viewstate_dom_snapped");
  },

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "metro_viewstate_dom_snapped":
          this._grid.arrangeItems();
        break;
    }
  },
};
