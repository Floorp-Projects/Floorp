/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, createElement } = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

loader.lazyRequireGetter(this, "ChangesContextMenu", "devtools/client/inspector/changes/ChangesContextMenu");
loader.lazyRequireGetter(this, "clipboardHelper", "devtools/shared/platform/clipboard");

const ChangesApp = createFactory(require("./components/ChangesApp"));
const { getChangesStylesheet } = require("./selectors/changes");

const {
  TELEMETRY_SCALAR_CONTEXTMENU,
  TELEMETRY_SCALAR_CONTEXTMENU_COPY,
  TELEMETRY_SCALAR_COPY,
} = require("./constants");

const {
  resetChanges,
  trackChange,
} = require("./actions/changes");

class ChangesView {
  constructor(inspector, window) {
    this.document = window.document;
    this.inspector = inspector;
    this.store = this.inspector.store;
    this.telemetry = this.inspector.telemetry;
    this.window = window;

    this.onAddChange = this.onAddChange.bind(this);
    this.onClearChanges = this.onClearChanges.bind(this);
    this.onChangesFront = this.onChangesFront.bind(this);
    this.onContextMenu = this.onContextMenu.bind(this);
    this.onCopy = this.onCopy.bind(this);
    this.destroy = this.destroy.bind(this);

    this.init();
  }

  get contextMenu() {
    if (!this._contextMenu) {
      this._contextMenu = new ChangesContextMenu(this);
    }

    return this._contextMenu;
  }

  init() {
    const changesApp = ChangesApp({
      onContextMenu: this.onContextMenu,
      onCopy: this.onCopy,
    });

    // listen to the front for initialization, add listeners
    // when it is ready
    this._getChangesFront();

    // Expose the provider to let inspector.js use it in setupSidebar.
    this.provider = createElement(Provider, {
      id: "changesview",
      key: "changesview",
      store: this.store,
    }, changesApp);

    this.inspector.target.on("will-navigate", this.onClearChanges);
  }

  _getChangesFront() {
    if (this.changesFrontPromise) {
      return this.changesFrontPromise;
    }
    this.changesFrontPromise = new Promise(async resolve => {
      const target = this.inspector.target;
      const front = await target.getFront("changes");
      this.onChangesFront(front);
      resolve(front);
    });
    return this.changesFrontPromise;
  }

  async onChangesFront(changesFront) {
    changesFront.on("add-change", this.onAddChange);
    changesFront.on("clear-changes", this.onClearChanges);
    try {
      // Get all changes collected up to this point by the ChangesActor on the server,
      // then push them to the Redux store here on the client.
      const changes = await changesFront.allChanges();
      changes.forEach(change => {
        this.onAddChange(change);
      });
    } catch (e) {
      // The connection to the server may have been cut, for
      // example during test
      // teardown. Here we just catch the error and silently
      // ignore it.
    }
  }

  /**
   * Handler for the "Copy Changes" option from the context menu.
   * Builds a CSS text with the aggregated changes and copies it to the clipboard.
   *
   * Optional rule and source ids can be used to filter the scope of the operation:
   * - if both a rule id and source id are provided, copy only the changes to the
   * matching rule within the matching source.
   * - if only a source id is provided, copy the changes to all rules within the
   * matching source.
   * - if neither rule id nor source id are provided, copy the changes too all rules
   * within all sources.
   *
   * @param {String|null} ruleId
   *        Optional rule id.
   * @param {String|null} sourceId
   *        Optional source id.
   */
  copyChanges(ruleId, sourceId) {
    const state = this.store.getState().changes || {};
    const filter = {};
    if (ruleId) {
      filter.ruleIds = [ruleId];
    }
    if (sourceId) {
      filter.sourceIds = [sourceId];
    }

    const text = getChangesStylesheet(state, filter);
    clipboardHelper.copyString(text);
  }

  /**
   * Handler for the "Copy" option from the context menu.
   * Copies the current text selection to the clipboard.
   */
  copySelection() {
    clipboardHelper.copyString(this.window.getSelection().toString());
    this.telemetry.scalarAdd(TELEMETRY_SCALAR_CONTEXTMENU_COPY, 1);
  }

  onAddChange(change) {
    // Turn data into a suitable change to send to the store.
    this.store.dispatch(trackChange(change));
  }

  onClearChanges() {
    this.store.dispatch(resetChanges());
  }

  /**
   * Event handler for the "contextmenu" event fired when the context menu is requested.
   * @param {Event} e
   */
  onContextMenu(e) {
    this.contextMenu.show(e);
    this.telemetry.scalarAdd(TELEMETRY_SCALAR_CONTEXTMENU, 1);
  }

  /**
   * Event handler for the "copy" event fired when content is copied to the clipboard.
   * We don't change the default behavior. We only log the increment count of this action.
   */
  onCopy() {
    this.telemetry.scalarAdd(TELEMETRY_SCALAR_COPY, 1);
  }

  /**
   * Destruction function called when the inspector is destroyed.
   */
  async destroy() {
    this.store.dispatch(resetChanges());

    // ensure we finish waiting for the front before destroying.
    const changesFront = await this.changesFrontPromise;
    changesFront.off("add-change", this.onAddChange);
    changesFront.off("clear-changes", this.onClearChanges);

    this.document = null;
    this.inspector = null;
    this.store = null;

    if (this._contextMenu) {
      this._contextMenu.destroy();
      this._contextMenu = null;
    }
  }
}

module.exports = ChangesView;
