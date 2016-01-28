/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

let log = Cu.import("resource://gre/modules/Log.jsm", {})
            .Log.repository.getLogger("Sync.RemoteTabs");

this.EXPORTED_SYMBOLS = [
  "TabListComponent"
];

/**
 * TabListComponent
 *
 * The purpose of this component is to compose the view, state, and actions.
 * It defines high level actions that act on the state and passes them to the
 * view for it to trigger during user interaction. It also subscribes the view
 * to state changes so it can rerender.
 */

function TabListComponent({window, store, View, SyncedTabs}) {
  this._window = window;
  this._store = store;
  this._View = View;
  // used to trigger Sync from context menu
  this._SyncedTabs = SyncedTabs;
}

TabListComponent.prototype = {
  get container() {
    return this._view.container;
  },

  init() {
    log.debug("Initializing TabListComponent");

    this._view = new this._View(this._window, {
      onSelectRow: (...args) => this.onSelectRow(...args),
      onOpenTab: (...args) => this.onOpenTab(...args),
      onMoveSelectionDown: (...args) => this.onMoveSelectionDown(...args),
      onMoveSelectionUp: (...args) => this.onMoveSelectionUp(...args),
      onToggleBranch: (...args) => this.onToggleBranch(...args),
      onBookmarkTab: (...args) => this.onBookmarkTab(...args),
      onSyncRefresh: (...args) => this.onSyncRefresh(...args),
      onFilter: (...args) => this.onFilter(...args),
      onClearFilter: (...args) => this.onClearFilter(...args),
      onFilterFocus: (...args) => this.onFilterFocus(...args),
      onFilterBlur: (...args) => this.onFilterBlur(...args)
    });

    this._store.on("change", state => this._view.render(state));
    this._view.render({clients: []});
    // get what's already available...
    this._store.getData();
    this._store.focusInput();
  },

  uninit() {
    this._view.destroy();
  },

  onFilter(query) {
    this._store.getData(query);
  },

  onClearFilter() {
    this._store.clearFilter();
  },

  onFilterFocus() {
    this._store.focusInput();
  },

  onFilterBlur() {
    this._store.blurInput();
  },

  onSelectRow(position, id) {
    this._store.selectRow(position[0], position[1]);
    if (id) {
      this._store.toggleBranch(id);
    }
  },

  onMoveSelectionDown() {
    this._store.moveSelectionDown();
  },

  onMoveSelectionUp() {
    this._store.moveSelectionUp();
  },

  onToggleBranch(id) {
    this._store.toggleBranch(id);
  },

  onBookmarkTab(uri, title) {
    this._window.top.PlacesCommandHook
      .bookmarkLink(this._window.PlacesUtils.bookmarksMenuFolderId, uri, title)
      .catch(Cu.reportError);
  },

  onOpenTab(url, event) {
    this._window.openUILink(url, event);
  },

  onSyncRefresh() {
    this._SyncedTabs.syncTabs(true);
  }
};
