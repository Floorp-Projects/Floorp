/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SiteDataManager",
                                  "resource:///modules/SiteDataManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadUtils",
                                  "resource://gre/modules/DownloadUtils.jsm");

"use strict";

let gSiteDataSettings = {

  // Array of meatdata of sites. Each array element is object holding:
  // - uri: uri of site; instance of nsIURI
  // - status: persistent-storage permission status
  // - usage: disk usage which site uses
  _sites: null,

  _list: null,

  init() {
    function setEventListener(id, eventType, callback) {
      document.getElementById(id)
              .addEventListener(eventType, callback.bind(gSiteDataSettings));
    }

    this._list = document.getElementById("sitesList");
    SiteDataManager.getSites().then(sites => {
      this._sites = sites;
      let sortCol = document.getElementById("hostCol");
      this._sortSites(this._sites, sortCol);
      this._buildSitesList(this._sites);
    });

    setEventListener("hostCol", "click", this.onClickTreeCol);
    setEventListener("usageCol", "click", this.onClickTreeCol);
    setEventListener("statusCol", "click", this.onClickTreeCol);
  },

  /**
   * @param sites {Array}
   * @param col {XULElement} the <treecol> being sorted on
   */
  _sortSites(sites, col) {
    let isCurrentSortCol = col.getAttribute("data-isCurrentSortCol")
    let sortDirection = col.getAttribute("data-last-sortDirection") || "ascending";
    if (isCurrentSortCol) {
      // Sort on the current column, flip the sorting direction
      sortDirection = sortDirection === "ascending" ? "descending" : "ascending";
    }

    let sortFunc = null;
    switch (col.id) {
      case "hostCol":
        sortFunc = (a, b) => {
          let aHost = a.uri.host.toLowerCase();
          let bHost = b.uri.host.toLowerCase();
          return aHost.localeCompare(bHost);
        }
        break;

      case "statusCol":
        sortFunc = (a, b) => a.status - b.status;
        break;

      case "usageCol":
        sortFunc = (a, b) => a.usage - b.usage;
        break;
    }
    if (sortDirection === "descending") {
      sites.sort((a, b) => sortFunc(b, a));
    } else {
      sites.sort(sortFunc);
    }

    let cols = this._list.querySelectorAll("treecol");
    cols.forEach(c => {
      c.removeAttribute("sortDirection");
      c.removeAttribute("data-isCurrentSortCol");
    });
    col.setAttribute("data-isCurrentSortCol", true);
    col.setAttribute("sortDirection", sortDirection);
    col.setAttribute("data-last-sortDirection", sortDirection);
  },

  _buildSitesList(sites) {
    // Clear old entries.
    while (this._list.childNodes.length > 1) {
      this._list.removeChild(this._list.lastChild);
    }

    let prefStrBundle = document.getElementById("bundlePreferences");
    for (let data of sites) {
      let statusStrId = data.status === Ci.nsIPermissionManager.ALLOW_ACTION ? "important" : "default";
      let size = DownloadUtils.convertByteUnits(data.usage);
      let item = document.createElement("richlistitem");
      item.setAttribute("data-origin", data.uri.spec);
      item.setAttribute("host", data.uri.host);
      item.setAttribute("status", prefStrBundle.getString(statusStrId));
      item.setAttribute("usage", prefStrBundle.getFormattedString("siteUsage", size));
      this._list.appendChild(item);
    }
  },

  onClickTreeCol(e) {
    this._sortSites(this._sites, e.target);
    this._buildSitesList(this._sites);
  }
};
