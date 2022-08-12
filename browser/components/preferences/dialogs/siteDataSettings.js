/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "SiteDataManager",
  "resource:///modules/SiteDataManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "DownloadUtils",
  "resource://gre/modules/DownloadUtils.jsm"
);

let gSiteDataSettings = {
  // Array of metadata of sites. Each array element is object holding:
  // - uri: uri of site; instance of nsIURI
  // - baseDomain: base domain of the site
  // - cookies: array of cookies of that site
  // - usage: disk usage which site uses
  // - userAction: "remove" or "update-permission"; the action user wants to take.
  _sites: null,

  _list: null,
  _searchBox: null,

  _createSiteListItem(site) {
    let item = document.createXULElement("richlistitem");
    item.setAttribute("host", site.baseDomain);
    let container = document.createXULElement("hbox");

    // Creates a new column item with the specified relative width.
    function addColumnItem(l10n, flexWidth, tooltipText) {
      let box = document.createXULElement("hbox");
      box.className = "item-box";
      box.setAttribute("style", `-moz-box-flex: ${flexWidth}`);
      let label = document.createXULElement("label");
      label.setAttribute("crop", "end");
      if (l10n) {
        if (l10n.hasOwnProperty("raw")) {
          box.setAttribute("tooltiptext", l10n.raw);
          label.setAttribute("value", l10n.raw);
        } else {
          document.l10n.setAttributes(label, l10n.id, l10n.args);
        }
      }
      if (tooltipText) {
        box.setAttribute("tooltiptext", tooltipText);
      }
      box.appendChild(label);
      container.appendChild(box);
    }

    // Add "Host" column.
    let hostData = site.baseDomain
      ? { raw: site.baseDomain }
      : { id: "site-data-local-file-host" };
    addColumnItem(hostData, "4");

    // Add "Cookies" column.
    addColumnItem({ raw: site.cookies.length }, "1");

    // Add "Storage" column
    if (site.usage > 0 || site.persisted) {
      let [value, unit] = DownloadUtils.convertByteUnits(site.usage);
      let strName = site.persisted
        ? "site-storage-persistent"
        : "site-storage-usage";
      addColumnItem(
        {
          id: strName,
          args: { value, unit },
        },
        "2"
      );
    } else {
      // Pass null to avoid showing "0KB" when there is no site data stored.
      addColumnItem(null, "2");
    }

    // Add "Last Used" column.
    let formattedLastAccessed =
      site.lastAccessed > 0
        ? this._relativeTimeFormat.formatBestUnit(site.lastAccessed)
        : null;
    let formattedFullDate =
      site.lastAccessed > 0
        ? this._absoluteTimeFormat.format(site.lastAccessed)
        : null;
    addColumnItem(
      site.lastAccessed > 0 ? { raw: formattedLastAccessed } : null,
      "2",
      formattedFullDate
    );

    item.appendChild(container);
    return item;
  },

  init() {
    function setEventListener(id, eventType, callback) {
      document
        .getElementById(id)
        .addEventListener(eventType, callback.bind(gSiteDataSettings));
    }

    this._absoluteTimeFormat = new Services.intl.DateTimeFormat(undefined, {
      dateStyle: "short",
      timeStyle: "short",
    });

    this._relativeTimeFormat = new Services.intl.RelativeTimeFormat(
      undefined,
      {}
    );

    this._list = document.getElementById("sitesList");
    this._searchBox = document.getElementById("searchBox");
    SiteDataManager.getSites().then(sites => {
      this._sites = sites;
      let sortCol = document.querySelector(
        "treecol[data-isCurrentSortCol=true]"
      );
      this._sortSites(this._sites, sortCol);
      this._buildSitesList(this._sites);
      Services.obs.notifyObservers(null, "sitedata-settings-init");
    });

    setEventListener("sitesList", "select", this.onSelect);
    setEventListener("hostCol", "click", this.onClickTreeCol);
    setEventListener("usageCol", "click", this.onClickTreeCol);
    setEventListener("lastAccessedCol", "click", this.onClickTreeCol);
    setEventListener("cookiesCol", "click", this.onClickTreeCol);
    setEventListener("searchBox", "command", this.onCommandSearch);
    setEventListener("removeAll", "command", this.onClickRemoveAll);
    setEventListener("removeSelected", "command", this.removeSelected);

    document.addEventListener("dialogaccept", e => this.saveChanges(e));
  },

  _updateButtonsState() {
    let items = this._list.getElementsByTagName("richlistitem");
    let removeSelectedBtn = document.getElementById("removeSelected");
    let removeAllBtn = document.getElementById("removeAll");
    removeSelectedBtn.disabled = !this._list.selectedItems.length;
    removeAllBtn.disabled = !items.length;

    let l10nId = this._searchBox.value
      ? "site-data-remove-shown"
      : "site-data-remove-all";
    document.l10n.setAttributes(removeAllBtn, l10nId);
  },

  /**
   * @param sites {Array}
   * @param col {XULElement} the <treecol> being sorted on
   */
  _sortSites(sites, col) {
    let isCurrentSortCol = col.getAttribute("data-isCurrentSortCol");
    let sortDirection =
      col.getAttribute("data-last-sortDirection") || "ascending";
    if (isCurrentSortCol) {
      // Sort on the current column, flip the sorting direction
      sortDirection =
        sortDirection === "ascending" ? "descending" : "ascending";
    }

    let sortFunc = null;
    switch (col.id) {
      case "hostCol":
        sortFunc = (a, b) => {
          let aHost = a.baseDomain.toLowerCase();
          let bHost = b.baseDomain.toLowerCase();
          return aHost.localeCompare(bHost);
        };
        break;

      case "cookiesCol":
        sortFunc = (a, b) => a.cookies.length - b.cookies.length;
        break;

      case "usageCol":
        sortFunc = (a, b) => a.usage - b.usage;
        break;

      case "lastAccessedCol":
        sortFunc = (a, b) => a.lastAccessed - b.lastAccessed;
        break;
    }
    if (sortDirection === "descending") {
      sites.sort((a, b) => sortFunc(b, a));
    } else {
      sites.sort(sortFunc);
    }

    let cols = this._list.previousElementSibling.querySelectorAll("treecol");
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
    let fragment = document.createDocumentFragment();
    for (let site of sites) {
      if (keyword && !site.baseDomain.includes(keyword)) {
        continue;
      }

      if (site.userAction === "remove") {
        continue;
      }

      let item = this._createSiteListItem(site);
      fragment.appendChild(item);
    }
    this._list.appendChild(fragment);
    this._updateButtonsState();
  },

  _removeSiteItems(items) {
    for (let i = items.length - 1; i >= 0; --i) {
      let item = items[i];
      let baseDomain = item.getAttribute("host");
      let siteForBaseDomain = this._sites.find(
        site => site.baseDomain == baseDomain
      );
      if (siteForBaseDomain) {
        siteForBaseDomain.userAction = "remove";
      }
      item.remove();
    }
    this._updateButtonsState();
  },

  async saveChanges(event) {
    let removals = this._sites
      .filter(site => site.userAction == "remove")
      .map(site => site.baseDomain);

    if (removals.length) {
      let removeAll = removals.length == this._sites.length;
      let promptArg = removeAll ? undefined : removals;
      if (!SiteDataManager.promptSiteDataRemoval(window, promptArg)) {
        // If the user cancelled the confirm dialog keep the site data window open,
        // they can still press cancel again to exit.
        event.preventDefault();
        return;
      }
      try {
        if (removeAll) {
          await SiteDataManager.removeAll();
        } else {
          await SiteDataManager.remove(removals);
        }
      } catch (e) {
        Cu.reportError(e);
      }
    }
  },

  removeSelected() {
    let lastIndex = this._list.selectedItems.length - 1;
    let lastSelectedItem = this._list.selectedItems[lastIndex];
    let lastSelectedItemPosition = this._list.getIndexOfItem(lastSelectedItem);
    let nextSelectedItem = this._list.getItemAtIndex(
      lastSelectedItemPosition + 1
    );

    this._removeSiteItems(this._list.selectedItems);
    this._list.clearSelection();

    if (nextSelectedItem) {
      this._list.selectedItem = nextSelectedItem;
    } else {
      this._list.selectedIndex = this._list.itemCount - 1;
    }
  },

  onClickTreeCol(e) {
    this._sortSites(this._sites, e.target);
    this._buildSitesList(this._sites);
    this._list.clearSelection();
  },

  onCommandSearch() {
    this._buildSitesList(this._sites);
    this._list.clearSelection();
  },

  onClickRemoveAll() {
    let siteItems = this._list.getElementsByTagName("richlistitem");
    if (siteItems.length) {
      this._removeSiteItems(siteItems);
    }
  },

  onKeyPress(e) {
    if (
      e.keyCode == KeyEvent.DOM_VK_DELETE ||
      (AppConstants.platform == "macosx" &&
        e.keyCode == KeyEvent.DOM_VK_BACK_SPACE)
    ) {
      if (!e.target.closest("#sitesList")) {
        // The user is typing or has not selected an item from the list to remove
        return;
      }
      // The users intention is to delete site data
      this.removeSelected();
    }
  },

  onSelect() {
    this._updateButtonsState();
  },
};
