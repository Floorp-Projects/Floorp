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
    this._list = document.getElementById("sitesList");
    SiteDataManager.getSites().then(sites => {
      this._sites = sites;
      this._sortSites(this._sites, "decending");
      this._buildSitesList(this._sites);
    });
  },

  /**
   * Sort sites by usages
   *
   * @param sites {Array}
   * @param order {String} indicate to sort in the "decending" or "ascending" order
   */
  _sortSites(sites, order) {
    sites.sort((a, b) => {
      if (order === "ascending") {
        return a.usage - b.usage;
      }
      return b.usage - a.usage;
    });
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
  }
};
