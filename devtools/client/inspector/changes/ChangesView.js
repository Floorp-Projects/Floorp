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
} = require("./actions/changes");

class ChangesView {
  constructor(inspector) {
    this.inspector = inspector;
    this.store = this.inspector.store;

    this.destroy = this.destroy.bind(this);

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
  }

  /**
   * Destruction function called when the inspector is destroyed.
   */
  destroy() {
    this.store.dispatch(resetChanges());
    this.inspector = null;
    this.store = null;
  }
}

module.exports = ChangesView;
