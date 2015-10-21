/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global XPCOMUtils, Services, BinarySearch, PlacesUtils, gPrincipal, EventEmitter */
/* global gLinks */
/* exported PlacesProvider */

"use strict";

this.EXPORTED_SYMBOLS = ["PlacesProvider"];

const {interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "BinarySearch",
  "resource://gre/modules/BinarySearch.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "EventEmitter", function() {
  const {EventEmitter} = Cu.import("resource://devtools/shared/event-emitter.js", {});
  return EventEmitter;
});

XPCOMUtils.defineLazyGetter(this, "gPrincipal", function() {
  let uri = Services.io.newURI("about:newtab", null, null);
  return Services.scriptSecurityManager.getNoAppCodebasePrincipal(uri);
});

// The maximum number of results PlacesProvider retrieves from history.
const HISTORY_RESULTS_LIMIT = 100;

/**
 * Singleton that checks if a given link should be displayed on about:newtab
 * or if we should rather not do it for security reasons. URIs that inherit
 * their caller's principal will be filtered.
 */
let LinkChecker = {
  _cache: new Map(),

  get flags() {
    return Ci.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL |
           Ci.nsIScriptSecurityManager.DONT_REPORT_ERRORS;
  },

  checkLoadURI: function LinkChecker_checkLoadURI(aURI) {
    if (!this._cache.has(aURI)) {
      this._cache.set(aURI, this._doCheckLoadURI(aURI));
    }

    return this._cache.get(aURI);
  },

  _doCheckLoadURI: function LinkChecker_doCheckLoadURI(aURI) {
    let result = false;
    try {
      Services.scriptSecurityManager.
        checkLoadURIStrWithPrincipal(gPrincipal, aURI, this.flags);
      result = true;
    } catch (e) {
      // We got a weird URI or one that would inherit the caller's principal.
      Cu.reportError(e);
    }
    return result;
  }
};

/**
 * Singleton that provides utility functions for links.
 * A link is a plain object that looks like this:
 *
 * {
 *   url: "http://www.mozilla.org/",
 *   title: "Mozilla",
 *   frecency: 1337,
 *   lastVisitDate: 1394678824766431,
 * }
 */
const LinkUtils = {
  _sortProperties: [
    "frecency",
    "lastVisitDate",
    "url",
  ],

  /**
   * Compares two links.
   *
   * @param {String} aLink1 The first link.
   * @param {String} aLink2 The second link.
   * @return {Number} A negative number if aLink1 is ordered before aLink2, zero if
   *         aLink1 and aLink2 have the same ordering, or a positive number if
   *         aLink1 is ordered after aLink2.
   *         Order is ascending.
   */
  compareLinks: function LinkUtils_compareLinks(aLink1, aLink2) {
    for (let prop of LinkUtils._sortProperties) {
      if (!aLink1.hasOwnProperty(prop) || !aLink2.hasOwnProperty(prop)) {
        throw new Error("Comparable link missing required property: " + prop);
      }
    }
    return aLink2.frecency - aLink1.frecency ||
           aLink2.lastVisitDate - aLink1.lastVisitDate ||
           aLink1.url.localeCompare(aLink2.url);
  },
};

/* Queries history to retrieve the most visited sites. Emits events when the
 * history changes.
 * Implements the EventEmitter interface.
 */
let Links = function Links() {
  EventEmitter.decorate(this);
};

Links.prototype = {
  /**
   * Set this to change the maximum number of links the provider will provide.
   */
  get maxNumLinks() {
    // getter, so it can't be replaced dynamically
    return HISTORY_RESULTS_LIMIT;
  },

  /**
   * A set of functions called by @mozilla.org/browser/nav-historyservice
   * All history events are emitted from this object.
   */
  historyObserver: {
    onDeleteURI: function historyObserver_onDeleteURI(aURI) {
      // let observers remove sensetive data associated with deleted visit
      gLinks.emit("deleteURI", {
        url: aURI.spec,
      });
    },

    onClearHistory: function historyObserver_onClearHistory() {
      gLinks.emit("clearHistory");
    },

    onFrecencyChanged: function historyObserver_onFrecencyChanged(aURI,
                           aNewFrecency, aGUID, aHidden, aLastVisitDate) { // jshint ignore:line
      // The implementation of the query in getLinks excludes hidden and
      // unvisited pages, so it's important to exclude them here, too.
      if (!aHidden && aLastVisitDate) {
        gLinks.emit("linkChanged", {
          url: aURI.spec,
          frecency: aNewFrecency,
          lastVisitDate: aLastVisitDate,
          type: "history",
        });
      }
    },

    onManyFrecenciesChanged: function historyObserver_onManyFrecenciesChanged() {
      // Called when frecencies are invalidated and also when clearHistory is called
      // See toolkit/components/places/tests/unit/test_frecency_observers.js
      gLinks.emit("manyLinksChanged");
    },

    onTitleChanged: function historyObserver_onTitleChanged(aURI, aNewTitle) {
      gLinks.emit("linkChanged", {
        url: aURI.spec,
        title: aNewTitle
      });
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver,
                        Ci.nsISupportsWeakReference])
  },

  /**
   * Must be called before the provider is used.
   * Makes it easy to disable under pref
   */
  init: function PlacesProvider_init() {
    PlacesUtils.history.addObserver(this.historyObserver, true);
  },

  /**
   * Must be called before the provider is unloaded.
   */
  destroy: function PlacesProvider_destroy() {
    PlacesUtils.history.removeObserver(this.historyObserver);
  },

  /**
   * Gets the current set of links delivered by this provider.
   *
   * @returns {Promise} Returns a promise with the array of links as payload.
   */
  getLinks: function PlacesProvider_getLinks() {
    let getLinksPromise = new Promise((resolve, reject) => {
      let options = PlacesUtils.history.getNewQueryOptions();
      options.maxResults = this.maxNumLinks;

      // Sort by frecency, descending.
      options.sortingMode = Ci.nsINavHistoryQueryOptions
        .SORT_BY_FRECENCY_DESCENDING;

      let links = [];

      let queryHandlers = {
        handleResult: function(aResultSet) {
          for (let row = aResultSet.getNextRow(); row; row = aResultSet.getNextRow()) {
            let url = row.getResultByIndex(1);
            if (LinkChecker.checkLoadURI(url)) {
              let link = {
                url: url,
                title: row.getResultByIndex(2),
                frecency: row.getResultByIndex(12),
                lastVisitDate: row.getResultByIndex(5),
                type: "history",
              };
              links.push(link);
            }
          }
        },

        handleError: function(aError) {
          reject(aError);
        },

        handleCompletion: function(aReason) { // jshint ignore:line
          // The Places query breaks ties in frecency by place ID descending, but
          // that's different from how Links.compareLinks breaks ties, because
          // compareLinks doesn't have access to place IDs.  It's very important
          // that the initial list of links is sorted in the same order imposed by
          // compareLinks, because Links uses compareLinks to perform binary
          // searches on the list.  So, ensure the list is so ordered.
          let i = 1;
          let outOfOrder = [];
          while (i < links.length) {
            if (LinkUtils.compareLinks(links[i - 1], links[i]) > 0) {
              outOfOrder.push(links.splice(i, 1)[0]);
            } else {
              i++;
            }
          }
          for (let link of outOfOrder) {
            i = BinarySearch.insertionIndexOf(LinkUtils.compareLinks, links, link);
            links.splice(i, 0, link);
          }

          resolve(links);
        }
      };

      // Execute the query.
      let query = PlacesUtils.history.getNewQuery();
      let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase);
      db.asyncExecuteLegacyQueries([query], 1, options, queryHandlers);
    });

    return getLinksPromise;
  }
};

/**
 * Singleton that serves as the default link provider for the grid.
 */
const gLinks = new Links(); // jshint ignore:line

let PlacesProvider = {
  LinkChecker: LinkChecker,
  LinkUtils: LinkUtils,
  links: gLinks,
};
