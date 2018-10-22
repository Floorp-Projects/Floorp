/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, createElement } = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const ChangesApp = createFactory(require("./components/ChangesApp"));

const {
  resetChanges,
  trackChange,
} = require("./actions/changes");

class ChangesView {
  constructor(inspector) {
    this.inspector = inspector;
    this.store = this.inspector.store;
    this.toolbox = this.inspector.toolbox;

    this.onAddChange = this.onAddChange.bind(this);
    this.onClearChanges = this.onClearChanges.bind(this);
    this.destroy = this.destroy.bind(this);

    // Get the Changes front, and listen to it.
    this.changesFront = this.toolbox.target.getFront("changes");
    this.changesFront.on("add-change", this.onAddChange);
    this.changesFront.on("clear-changes", this.onClearChanges);

    this.init();
  }

  init() {
    const changesApp = ChangesApp({});

    // Expose the provider to let inspector.js use it in setupSidebar.
    this.provider = createElement(Provider, {
      id: "changesview",
      key: "changesview",
      store: this.store,
    }, changesApp);

    // TODO: save store and restore/replay on refresh.
    // Bug 1478439 - https://bugzilla.mozilla.org/show_bug.cgi?id=1478439
    this.inspector.target.once("will-navigate", this.destroy);

    // Sync the store to the changes stored on the server. The
    // syncChangesToServer() method is async, but we don't await it since
    // this method itself is NOT async. The call will be made in its own
    // time, which is fine since it definitionally brings us up-to-date
    // with the server at that moment.
    this.syncChangesToServer();
  }

  async syncChangesToServer() {
    // Empty the store.
    this.onClearChanges();

    // Add back in all the changes from the changesFront.
    const changes = await this.changesFront.allChanges();
    changes.forEach((change) => {
      this.onAddChange(change);
    });
  }

  onAddChange(change) {
    // Turn data into a suitable change to send to the store.
    this.store.dispatch(trackChange(change));
  }

  onClearChanges() {
    this.store.dispatch(resetChanges());
  }

  /**
   * Destruction function called when the inspector is destroyed.
   */
  destroy() {
    this.store.dispatch(resetChanges());

    this.changesFront.off("add-change", this.onAddChange);
    this.changesFront.off("clear-changes", this.onClearChanges);
    this.changesFront = null;

    this.inspector = null;
    this.store = null;
    this.toolbox = null;
  }
}

module.exports = ChangesView;
