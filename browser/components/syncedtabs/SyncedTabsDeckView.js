/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

let log = Cu.import("resource://gre/modules/Log.jsm", {})
            .Log.repository.getLogger("Sync.RemoteTabs");

this.EXPORTED_SYMBOLS = [
  "SyncedTabsDeckView"
];

/**
 * SyncedTabsDeckView
 *
 * Instances of SyncedTabsDeckView render DOM nodes from a given state.
 * No state is kept internaly and the DOM will completely
 * rerender unless the state flags `isUpdatable`, which helps
 * make small changes without the overhead of a full rerender.
 */
const SyncedTabsDeckView = function(window, tabListComponent, props) {
  this.props = props;

  this._window = window;
  this._doc = window.document;

  this._tabListComponent = tabListComponent;
  this._deckTemplate = this._doc.getElementById("deck-template");
  this.container = this._doc.createElement("div");
};

SyncedTabsDeckView.prototype = {
  render(state) {
    if (state.isUpdatable) {
      this.update(state);
    } else {
      this.create(state);
    }
  },

  create(state) {
    let deck = this._doc.importNode(this._deckTemplate.content, true).firstElementChild;
    this._clearChilden();

    let tabListWrapper = this._doc.createElement("div");
    tabListWrapper.className = "tabs-container sync-state";
    this._tabListComponent.init();
    tabListWrapper.appendChild(this._tabListComponent.container);
    deck.appendChild(tabListWrapper);
    this.container.appendChild(deck);

    this._attachListeners();
    this.update(state);
  },

  destroy() {
    this._tabListComponent.uninit();
    this.container.remove();
  },

  update(state) {
    // Note that we may also want to update elements that are outside of the
    // deck, so use the document to find the class names rather than our
    // container.
    for (let panel of state.panels) {
      if (panel.selected) {
        Array.prototype.map.call(this._doc.getElementsByClassName(panel.id),
                                 item => item.classList.add("selected"));
      } else {
        Array.prototype.map.call(this._doc.getElementsByClassName(panel.id),
                                 item => item.classList.remove("selected"));
      }
    }
  },

  _clearChilden() {
    while (this.container.firstChild) {
      this.container.firstChild.remove();
    }
  },

  _attachListeners() {
    let syncPrefLinks = this.container.querySelectorAll(".sync-prefs");
    for (let link of syncPrefLinks) {
      link.addEventListener("click", this.props.onSyncPrefClick);
    }
    this.container.querySelector(".connect-device").addEventListener("click", this.props.onConnectDeviceClick);
  },
};

