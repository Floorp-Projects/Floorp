/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

let log = Cu.import("resource://gre/modules/Log.jsm", {})
            .Log.repository.getLogger("Sync.RemoteTabs");

XPCOMUtils.defineLazyModuleGetter(this, "BrowserUITelemetry",
  "resource:///modules/BrowserUITelemetry.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUIUtils",
  "resource:///modules/PlacesUIUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

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

function TabListComponent({window, store, View, SyncedTabs, clipboardHelper,
                           getChromeWindow}) {
  this._window = window;
  this._store = store;
  this._View = View;
  this._clipboardHelper = clipboardHelper;
  this._getChromeWindow = getChromeWindow;
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
      onOpenTabs: (...args) => this.onOpenTabs(...args),
      onMoveSelectionDown: (...args) => this.onMoveSelectionDown(...args),
      onMoveSelectionUp: (...args) => this.onMoveSelectionUp(...args),
      onToggleBranch: (...args) => this.onToggleBranch(...args),
      onBookmarkTab: (...args) => this.onBookmarkTab(...args),
      onCopyTabLocation: (...args) => this.onCopyTabLocation(...args),
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

  onSelectRow(position) {
    this._store.selectRow(position[0], position[1]);
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
      .bookmarkLink(this._window.top.PlacesUtils.bookmarksMenuFolderId, uri, title)
      .catch(Cu.reportError);
  },

  onOpenTab(url, where, params) {
    this._window.openUILinkIn(url, where, params);
    BrowserUITelemetry.countSyncedTabEvent("open", "sidebar");
  },

  onOpenTabs(urls, where) {
    if (!PlacesUIUtils.confirmOpenInTabs(urls.length, this._window)) {
      return;
    }
    if (where == "window") {
      this._window.openDialog(this._window.getBrowserURL(), "_blank",
                              "chrome,dialog=no,all", urls.join("|"));
    } else {
      let loadInBackground = where == "tabshifted";
      this._getChromeWindow(this._window).gBrowser.loadTabs(urls, {
        inBackground: loadInBackground,
        replace: false,
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      });
    }
    BrowserUITelemetry.countSyncedTabEvent("openmultiple", "sidebar");
  },

  onCopyTabLocation(url) {
    this._clipboardHelper.copyString(url);
  },

  onSyncRefresh() {
    this._SyncedTabs.syncTabs(true);
  }
};
