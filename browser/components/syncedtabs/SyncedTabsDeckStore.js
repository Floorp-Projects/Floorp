/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

let { EventEmitter } = Cu.import("resource:///modules/syncedtabs/EventEmitter.jsm", {});

this.EXPORTED_SYMBOLS = [
  "SyncedTabsDeckStore"
];

/**
 * SyncedTabsDeckStore
 *
 * This store keeps track of the deck view state, including the panels and which
 * one is selected. The view listens for change events on the store, which are
 * triggered whenever the state changes. If it's a small change, the state
 * will have `isUpdatable` set to true so the view can skip rerendering the whole
 * DOM.
 */
function SyncedTabsDeckStore() {
  EventEmitter.call(this);
  this._panels = [];
}

Object.assign(SyncedTabsDeckStore.prototype, EventEmitter.prototype, {
  _change(isUpdatable = false) {
    let panels = this._panels.map(panel => {
      return {id: panel, selected: panel === this._selectedPanel};
    });
    this.emit("change", {panels, isUpdatable});
  },

  /**
   * Sets the selected panelId and triggers a change event.
   * @param {String} panelId - ID of the panel to select.
   */
  selectPanel(panelId) {
    if (this._panels.indexOf(panelId) === -1 || this._selectedPanel === panelId) {
      return;
    }
    this._selectedPanel = panelId;
    this._change(true);
  },

  /**
   * Update the set of panels in the deck and trigger a change event.
   * @param {Array} panels - an array of IDs for each panel in the deck.
   */
  setPanels(panels) {
    if (panels === this._panels) {
      return;
    }
    this._panels = panels || [];
    this._change();
  }
});
