/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

let { EventEmitter } = Cu.import("resource:///modules/syncedtabs/EventEmitter.jsm", {});

this.EXPORTED_SYMBOLS = [
  "SyncedTabsListStore"
];

/**
 * SyncedTabsListStore
 *
 * Instances of this store encapsulate all of the state associated with a synced tabs list view.
 * The state includes the clients, their tabs, the row that is currently selected,
 * and the filtered query.
 */
function SyncedTabsListStore(SyncedTabs) {
  EventEmitter.call(this);
  this._SyncedTabs = SyncedTabs;
  this.data = [];
  this._closedClients = {};
  this._selectedRow = [-1, -1];
  this.filter = "";
  this.inputFocused = false;
}

Object.assign(SyncedTabsListStore.prototype, EventEmitter.prototype, {
  // This internal method triggers the "change" event that views
  // listen for. It denormalizes the state so that it's easier for
  // the view to deal with. updateType hints to the view what
  // actually needs to be rerendered or just updated, and can be
  // empty (to (re)render everything), "searchbox" (to rerender just the tab list),
  // or "all" (to skip rendering and just update all attributes of existing nodes).
  _change(updateType) {
    let selectedParent = this._selectedRow[0];
    let selectedChild = this._selectedRow[1];
    let rowSelected = false;
    // clone the data so that consumers can't mutate internal storage
    let data = Cu.cloneInto(this.data, {});
    let tabCount = 0;

    data.forEach((client, index) => {
      client.closed = !!this._closedClients[client.id];

      if (rowSelected || selectedParent < 0) {
        return;
      }
      if (this.filter) {
        if (selectedParent < tabCount + client.tabs.length) {
          client.tabs[selectedParent - tabCount].selected = true;
          client.tabs[selectedParent - tabCount].focused = !this.inputFocused;
          rowSelected = true;
        } else {
          tabCount += client.tabs.length;
        }
        return;
      }
      if (selectedParent === index && selectedChild === -1) {
        client.selected = true;
        client.focused = !this.inputFocused;
        rowSelected = true;
      } else if (selectedParent === index) {
        client.tabs[selectedChild].selected = true;
        client.tabs[selectedChild].focused = !this.inputFocused;
        rowSelected = true;
      }
    });

    // If this were React the view would be smart enough
    // to not re-render the whole list unless necessary. But it's
    // not, so updateType is a hint to the view of what actually
    // needs to be rerendered.
    this.emit("change", {
      clients: data,
      canUpdateAll: updateType === "all",
      canUpdateInput: updateType === "searchbox",
      filter: this.filter,
      inputFocused: this.inputFocused
    });
  },

  /**
   * Moves the row selection from a child to its parent,
   * which occurs when the parent of a selected row closes.
   */
  _selectParentRow() {
    this._selectedRow[1] = -1;
  },

  _toggleBranch(id, closed) {
    this._closedClients[id] = closed;
    if (this._closedClients[id]) {
      this._selectParentRow();
    }
    this._change("all");
  },

  _isOpen(client) {
    return !this._closedClients[client.id];
  },

  moveSelectionDown() {
    let branchRow = this._selectedRow[0];
    let childRow = this._selectedRow[1];
    let branch = this.data[branchRow];

    if (this.filter) {
      this.selectRow(branchRow + 1);
      return;
    }

    if (branchRow < 0) {
      this.selectRow(0, -1);
    } else if ((!branch.tabs.length || childRow >= branch.tabs.length - 1 || !this._isOpen(branch)) && branchRow < this.data.length) {
      this.selectRow(branchRow + 1, -1);
    } else if (childRow < branch.tabs.length) {
      this.selectRow(branchRow, childRow + 1);
    }
  },

  moveSelectionUp() {
    let branchRow = this._selectedRow[0];
    let childRow = this._selectedRow[1];
    let branch = this.data[branchRow];

    if (this.filter) {
      this.selectRow(branchRow - 1);
      return;
    }

    if (branchRow < 0) {
      this.selectRow(0, -1);
    } else if (childRow < 0 && branchRow > 0) {
      let prevBranch = this.data[branchRow - 1];
      let newChildRow = this._isOpen(prevBranch) ? prevBranch.tabs.length - 1 : -1;
      this.selectRow(branchRow - 1, newChildRow);
    } else if (childRow >= 0) {
      this.selectRow(branchRow, childRow - 1);
    }
  },

  // Selects a row and makes sure the selection is within bounds
  selectRow(parent, child) {
    let maxParentRow = this.filter ? this._tabCount() : this.data.length;
    let parentRow = parent;
    if (parent <= -1) {
      parentRow = 0;
    } else if (parent >= maxParentRow) {
      return;
    }

    let childRow = child;
    if (parentRow === -1 || this.filter || typeof child === "undefined" || child < -1) {
      childRow = -1;
    } else if (child >= this.data[parentRow].tabs.length) {
      childRow = this.data[parentRow].tabs.length - 1;
    }

    if (this._selectedRow[0] === parentRow && this._selectedRow[1] === childRow) {
      return;
    }

    this._selectedRow = [parentRow, childRow];
    this.inputFocused = false;
    this._change("all");
  },

  _tabCount() {
    return this.data.reduce((prev, curr) => curr.tabs.length + prev, 0);
  },

  toggleBranch(id) {
    this._toggleBranch(id, !this._closedClients[id]);
  },

  closeBranch(id) {
    this._toggleBranch(id, true);
  },

  openBranch(id) {
    this._toggleBranch(id, false);
  },

  focusInput() {
    this.inputFocused = true;
    // A change type of "all" updates rather than rebuilds, which is what we
    // want here - only the selection/focus has changed.
    this._change("all");
  },

  blurInput() {
    this.inputFocused = false;
    // A change type of "all" updates rather than rebuilds, which is what we
    // want here - only the selection/focus has changed.
    this._change("all");
  },

  clearFilter() {
    this.filter = "";
    this._selectedRow = [-1, -1];
    return this.getData();
  },

  // Fetches data from the SyncedTabs module and triggers
  // and update
  getData(filter) {
    let updateType;
    let hasFilter = typeof filter !== "undefined";
    if (hasFilter) {
      this.filter = filter;
      this._selectedRow = [-1, -1];

      // When a filter is specified we tell the view that only the list
      // needs to be rerendered so that it doesn't disrupt the input
      // field's focus.
      updateType = "searchbox";
    }

    // return promise for tests
    return this._SyncedTabs.getTabClients(this.filter)
      .then(result => {
        if (!hasFilter) {
          // Only sort clients and tabs if we're rendering the whole list.
          this._SyncedTabs.sortTabClientsByLastUsed(result);
        }
        this.data = result;
        this._change(updateType);
      })
      .catch(Cu.reportError);
  }
});

