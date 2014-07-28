/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["SessionMigration"];

const Cu = Components.utils;
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);

// An encoder to UTF-8.
XPCOMUtils.defineLazyGetter(this, "gEncoder", function () {
  return new TextEncoder();
});

// A decoder.
XPCOMUtils.defineLazyGetter(this, "gDecoder", function () {
  return new TextDecoder();
});

let SessionMigrationInternal = {
  /**
   * Convert the original session restore state into a minimal state. It will
   * only contain:
   * - open windows
   *   - with tabs
   *     - with history entries with only title, url
   *     - with pinned state
   *     - with tab group info (hidden + group id)
   *     - with selected tab info
   *   - with selected window info
   *   - with tabgroups info
   *
   * The complete state is then wrapped into the "about:welcomeback" page as
   * form field info to be restored when restoring the state.
   */
  convertState: function(aStateObj) {
    let state = {
      selectedWindow: aStateObj.selectedWindow,
      _closedWindows: []
    };
    state.windows = aStateObj.windows.map(function(oldWin) {
      var win = {extData: {}};
      win.tabs = oldWin.tabs.map(function(oldTab) {
        var tab = {};
        // Keep only titles and urls for history entries
        tab.entries = oldTab.entries.map(function(entry) {
          return {url: entry.url, title: entry.title};
        });
        tab.index = oldTab.index;
        tab.hidden = oldTab.hidden;
        tab.pinned = oldTab.pinned;
        // The tabgroup info is in the extData, so we need to get it out.
        if (oldTab.extData && "tabview-tab" in oldTab.extData) {
          tab.extData = {"tabview-tab": oldTab.extData["tabview-tab"]};
        }
        return tab;
      });
      // There are various tabgroup-related attributes that we need to get out
      // of the session restore data for the window, too.
      if (oldWin.extData) {
        for (let k of Object.keys(oldWin.extData)) {
          if (k.startsWith("tabview-")) {
            win.extData[k] = oldWin.extData[k];
          }
        }
      }
      win.selected = oldWin.selected;
      win._closedTabs = [];
      return win;
    });
    let wrappedState = {
      url: "about:welcomeback",
      formdata: {
        id: {"sessionData": state},
        xpath: {}
      }
    };
    return {windows: [{tabs: [{entries: [wrappedState]}]}]};
  },
  /**
   * Asynchronously read session restore state (JSON) from a path
   */
  readState: function(aPath) {
    return Task.spawn(function() {
      let bytes = yield OS.File.read(aPath);
      let text = gDecoder.decode(bytes);
      let state = JSON.parse(text);
      throw new Task.Result(state);
    });
  },
  /**
   * Asynchronously write session restore state as JSON to a path
   */
  writeState: function(aPath, aState) {
    let bytes = gEncoder.encode(JSON.stringify(aState));
    return OS.File.writeAtomic(aPath, bytes, {tmpPath: aPath + ".tmp"});
  }
}

let SessionMigration = {
  /**
   * Migrate a limited set of session data from one path to another.
   */
  migrate: function(aFromPath, aToPath) {
    return Task.spawn(function() {
      let inState = yield SessionMigrationInternal.readState(aFromPath);
      let outState = SessionMigrationInternal.convertState(inState);
      // Unfortunately, we can't use SessionStore's own SessionFile to
      // write out the data because it has a dependency on the profile dir
      // being known. When the migration runs, there is no guarantee that
      // that's true.
      yield SessionMigrationInternal.writeState(aToPath, outState);
    });
  }
};
