/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const { utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

"use strict";

let gSiteDataRemoveSelected = {

  _tree: null,

  init() {
    let bundlePreferences = document.getElementById("bundlePreferences");
    let acceptBtn = document.getElementById("SiteDataRemoveSelectedDialog")
                            .getButton("accept");
    acceptBtn.label = bundlePreferences.getString("acceptRemove");

    // Organize items for the tree from the argument
    let hostsTable = window.arguments[0].hostsTable;
    let visibleItems = [];
    let itemsTable = new Map();
    for (let [ baseDomain, hosts ] of hostsTable) {
      // In the beginning, only display base domains in the topmost level.
      visibleItems.push({
        level: 0,
        opened: false,
        host: baseDomain
      });
      // Other hosts are in the second level.
      let items = hosts.map(host => {
        return { host, level: 1 };
      });
      items.sort(sortByHost);
      itemsTable.set(baseDomain, items);
    }
    visibleItems.sort(sortByHost);
    this._view.itemsTable = itemsTable;
    this._view.visibleItems = visibleItems;
    this._tree = document.getElementById("sitesTree");
    this._tree.view = this._view;

    function sortByHost(a, b) {
      let aHost = a.host.toLowerCase();
      let bHost = b.host.toLowerCase();
      return aHost.localeCompare(bHost);
    }
  },

  ondialogaccept() {
    window.arguments[0].allowed = true;
  },

  ondialogcancel() {
    window.arguments[0].allowed = false;
  },

  _view: {
    _selection: null,

    itemsTable: null,

    visibleItems: null,

    get rowCount() {
      return this.visibleItems.length;
    },

    getCellText(index, column) {
      let item = this.visibleItems[index];
      return item ? item.host : "";
    },

    isContainer(index) {
      let item = this.visibleItems[index];
      if (item && item.level === 0) {
        return true;
      }
      return false;
    },

    isContainerEmpty() {
      return false;
    },

    isContainerOpen(index) {
      let item = this.visibleItems[index];
      if (item && item.level === 0) {
        return item.opened;
      }
      return false;
    },

    getLevel(index) {
      let item = this.visibleItems[index];
      return item ? item.level : 0;
    },

    hasNextSibling(index, afterIndex) {
      let item = this.visibleItems[index];
      if (item) {
        let thisLV = this.getLevel(index);
        for (let i = afterIndex + 1; i < this.rowCount; ++i) {
          let nextLV = this.getLevel(i);
          if (nextLV == thisLV) {
            return true;
          }
          if (nextLV < thisLV) {
            break;
          }
        }
      }
      return false;
    },

    getParentIndex(index) {
      if (!this.isContainer(index)) {
        for (let i = index - 1; i >= 0; --i) {
          if (this.isContainer(i)) {
            return i;
          }
        }
      }
      return -1;
    },

    toggleOpenState(index) {
      let item = this.visibleItems[index];
      if (!this.isContainer(index)) {
        return;
      }

      if (item.opened) {
        item.opened = false;

        let deleteCount = 0;
        for (let i = index + 1; i < this.visibleItems.length; ++i) {
          if (!this.isContainer(i)) {
            ++deleteCount;
          } else {
            break;
          }
        }

        if (deleteCount) {
          this.visibleItems.splice(index + 1, deleteCount);
          this.treeBox.rowCountChanged(index + 1, -deleteCount);
        }
      } else {
        item.opened = true;

        let childItems = this.itemsTable.get(item.host);
        for (let i = 0; i < childItems.length; ++i) {
          this.visibleItems.splice(index + i + 1, 0, childItems[i]);
        }
        this.treeBox.rowCountChanged(index + 1, childItems.length);
      }
      this.treeBox.invalidateRow(index);
    },

    get selection() {
      return this._selection;
    },
    set selection(v) {
      this._selection = v;
      return v;
    },
    setTree(treeBox) {
      this.treeBox = treeBox;
    },
    isSeparator(index) {
      return false;
    },
    isSorted(index) {
      return false;
    },
    canDrop() {
      return false;
    },
    drop() {},
    getRowProperties() {},
    getCellProperties() {},
    getColumnProperties() {},
    hasPreviousSibling(index) {},
    getImageSrc() {},
    getCellValue() {},
    cycleHeader() {},
    selectionChanged() {},
    cycleCell() {},
    isEditable() {},
    isSelectable() {},
    setCellValue() {},
    setCellText() {},
    performAction() {},
    performActionOnRow() {},
    performActionOnCell() {}
  }
};
