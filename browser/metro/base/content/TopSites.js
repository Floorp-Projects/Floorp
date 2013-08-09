// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

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

