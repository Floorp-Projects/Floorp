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

  // Array of metadata of sites. Each array element is object holding:
  // - uri: uri of site; instance of nsIURI
  // - status: persistent-storage permission status
  // - usage: disk usage which site uses
  // - userAction: "remove" or "update-permission"; the action user wants to take.
  //               If not specified, means no action to take
  _sites: null,

  _list: null,
  _searchBox: null,
  _prefStrBundle: null,

  init() {
    function setEventListener(id, eventType, callback) {
      document.getElementById(id)
              .addEventListener(eventType, callback.bind(gSiteDataSettings));
    }

    this._list = document.getElementById("sitesList");
    this._searchBox = document.getElementById("searchBox");
    this._prefStrBundle = document.getElementById("bundlePreferences");
    SiteDataManager.getSites().then(sites => {
      this._sites = sites;
      let sortCol = document.querySelector("treecol[data-isCurrentSortCol=true]");
      this._sortSites(this._sites, sortCol);
      this._buildSitesList(this._sites);
      Services.obs.notifyObservers(null, "sitedata-settings-init");
    });

    let brandShortName = document.getElementById("bundle_brand").getString("brandShortName");
    let settingsDescription = document.getElementById("settingsDescription");
    settingsDescription.textContent = this._prefStrBundle.getFormattedString("siteDataSettings.description", [brandShortName]);

    setEventListener("hostCol", "click", this.onClickTreeCol);
    setEventListener("usageCol", "click", this.onClickTreeCol);
    setEventListener("statusCol", "click", this.onClickTreeCol);
    setEventListener("cancel", "command", this.close);
    setEventListener("save", "command", this.saveChanges);
    setEventListener("searchBox", "command", this.onCommandSearch);
    setEventListener("removeAll", "command", this.onClickRemoveAll);
    setEventListener("removeSelected", "command", this.onClickRemoveSelected);
  },

  _updateButtonsState() {
    let items = this._list.getElementsByTagName("richlistitem");
    let removeSelectedBtn = document.getElementById("removeSelected");
    let removeAllBtn = document.getElementById("removeAll");
    removeSelectedBtn.disabled = items.length == 0;
    removeAllBtn.disabled = removeSelectedBtn.disabled;

    let removeAllBtnLabelStringID = "removeAllSiteData.label";
    let removeAllBtnAccesskeyStringID = "removeAllSiteData.accesskey";
    if (this._searchBox.value) {
      removeAllBtnLabelStringID = "removeAllSiteDataShown.label";
      removeAllBtnAccesskeyStringID = "removeAllSiteDataShown.accesskey";
    }
    removeAllBtn.setAttribute("label", this._prefStrBundle.getString(removeAllBtnLabelStringID));
    removeAllBtn.setAttribute("accesskey", this._prefStrBundle.getString(removeAllBtnAccesskeyStringID));
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
          let aHost = a.host.toLowerCase();
          let bHost = b.host.toLowerCase();
          return aHost.localeCompare(bHost);
        }
        break;

      case "statusCol":
        sortFunc = (a, b) => {
          return a.persisted && !b.persisted ? 1 :
                 !a.persisted && b.persisted ? -1 : 0;
        };
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

  /**
   * @param sites {Array} array of metadata of sites
   */
  _buildSitesList(sites) {
    // Clear old entries.
    let oldItems = this._list.querySelectorAll("richlistitem");
    for (let item of oldItems) {
      item.remove();
    }

    let keyword = this._searchBox.value.toLowerCase().trim();
    for (let site of sites) {
      let host = site.host;
      if (keyword && !host.includes(keyword)) {
        continue;
      }

      if (site.userAction === "remove") {
        continue;
      }

      let size = DownloadUtils.convertByteUnits(site.usage);
      let item = document.createElement("richlistitem");
      item.setAttribute("host", host);
      item.setAttribute("usage", this._prefStrBundle.getFormattedString("siteUsage", size));
      if (site.persisted) {
        item.setAttribute("status", this._prefStrBundle.getString("persistent"));
      }
      this._list.appendChild(item);
    }
    this._updateButtonsState();
  },

  _removeSiteItems(items) {
    for (let i = items.length - 1; i >= 0; --i) {
      let item = items[i];
      let host = item.getAttribute("host");
      let siteForHost = this._sites.find(site => site.host == host);
      if (siteForHost) {
        siteForHost.userAction = "remove";
      }
      item.remove();
    }
    this._updateButtonsState();
  },

  saveChanges() {
    let allowed = true;

    // Confirm user really wants to remove site data starts
    let removals = [];
    this._sites = this._sites.filter(site => {
      if (site.userAction === "remove") {
        removals.push(site.host);
        return false;
      }
      return true;
    });

    if (removals.length > 0) {
      if (this._sites.length == 0) {
        // User selects all sites so equivalent to clearing all data
        let flags =
          Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_0 +
          Services.prompt.BUTTON_TITLE_CANCEL * Services.prompt.BUTTON_POS_1 +
          Services.prompt.BUTTON_POS_0_DEFAULT;
        let prefStrBundle = document.getElementById("bundlePreferences");
        let title = prefStrBundle.getString("clearSiteDataPromptTitle");
        let text = prefStrBundle.getString("clearSiteDataPromptText");
        let btn0Label = prefStrBundle.getString("clearSiteDataNow");
        let result = Services.prompt.confirmEx(window, title, text, flags, btn0Label, null, null, null, {});
        allowed = result == 0;
        if (allowed) {
          SiteDataManager.removeAll();
        }
      } else {
        // User only removes partial sites.
        // We will remove cookies based on base domain, say, user selects "news.foo.com" to remove.
        // The cookies under "music.foo.com" will be removed together.
        // We have to prompt user about this action.
        let hostsTable = new Map();
        // Group removed sites by base domain
        for (let host of removals) {
          let baseDomain = Services.eTLD.getBaseDomainFromHost(host);
          let hosts = hostsTable.get(baseDomain);
          if (!hosts) {
            hosts = [];
            hostsTable.set(baseDomain, hosts);
          }
          hosts.push(host);
        }
        // Pick out sites with the same base domain as removed sites
        for (let site of this._sites) {
          let baseDomain = Services.eTLD.getBaseDomainFromHost(site.host);
          let hosts = hostsTable.get(baseDomain);
          if (hosts) {
            hosts.push(site.host);
          }
        }

        let args = {
          hostsTable,
          allowed: false
        };
        let features = "centerscreen,chrome,modal,resizable=no";
        window.openDialog("chrome://browser/content/preferences/siteDataRemoveSelected.xul", "", features, args);
        allowed = args.allowed;
        if (allowed) {
          try {
            SiteDataManager.remove(removals);
          } catch (e) {
            // Hit error, maybe remove unknown site.
            // Let's print out the error, then proceed to close this settings dialog.
            // When we next open again we will once more get sites from the SiteDataManager and refresh the list.
            Cu.reportError(e);
          }
        }
      }
    }
    // Confirm user really wants to remove site data ends

    this.close();
  },

  close() {
    window.close();
  },

  onClickTreeCol(e) {
    this._sortSites(this._sites, e.target);
    this._buildSitesList(this._sites);
  },

  onCommandSearch() {
    this._buildSitesList(this._sites);
  },

  onClickRemoveSelected() {
    let selected = this._list.selectedItem;
    if (selected) {
      this._removeSiteItems([selected]);
    }
  },

  onClickRemoveAll() {
    let siteItems = this._list.getElementsByTagName("richlistitem");
    if (siteItems.length > 0) {
      this._removeSiteItems(siteItems);
    }
  }
};
