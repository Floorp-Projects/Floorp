/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  createElement,
} = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

loader.lazyRequireGetter(
  this,
  "ChangesContextMenu",
  "devtools/client/inspector/changes/ChangesContextMenu"
);
loader.lazyRequireGetter(
  this,
  "clipboardHelper",
  "devtools/shared/platform/clipboard"
);

const ChangesApp = createFactory(require("./components/ChangesApp"));
const { getChangesStylesheet } = require("./selectors/changes");

const {
  TELEMETRY_SCALAR_CONTEXTMENU_COPY_DECLARATION,
  TELEMETRY_SCALAR_CONTEXTMENU_COPY_RULE,
  TELEMETRY_SCALAR_COPY_ALL_CHANGES,
  TELEMETRY_SCALAR_COPY_RULE,
} = require("./constants");

const { resetChanges, trackChange } = require("./actions/changes");

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
    this.onCopyAllChanges = this.copyAllChanges.bind(this);
    this.onCopyRule = this.copyRule.bind(this);
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
      onCopyAllChanges: this.onCopyAllChanges,
      onCopyRule: this.onCopyRule,
    });

    // listen to the front for initialization, add listeners
    // when it is ready
    this._getChangesFront();

    // Expose the provider to let inspector.js use it in setupSidebar.
    this.provider = createElement(
      Provider,
      {
        id: "changesview",
        key: "changesview",
        store: this.store,
      },
      changesApp
    );

    this.inspector.currentTarget.on("will-navigate", this.onClearChanges);
  }

  _getChangesFront() {
    if (this.changesFrontPromise) {
      return this.changesFrontPromise;
    }
    this.changesFrontPromise = (async () => {
      const target = this.inspector.currentTarget;
      const front = await target.getFront("changes");
      this.onChangesFront(front);
      return front;
    })();
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
   * Handler for the "Copy All Changes" button. Simple wrapper that just calls
   * |this.copyChanges()| with no filters in order to trigger default operation.
   */
  copyAllChanges() {
    this.copyChanges();
    this.telemetry.scalarAdd(TELEMETRY_SCALAR_COPY_ALL_CHANGES, 1);
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
   * Handler for the "Copy Declaration" option from the context menu.
   * Builds a CSS declaration string with the property name and value, and copies it
   * to the clipboard. The declaration is commented out if it is marked as removed.
   *
   * @param {DOMElement} element
   *        Host element of a CSS declaration rendered the Changes panel.
   */
  copyDeclaration(element) {
    const name = element.querySelector(".changes__declaration-name")
      .textContent;
    const value = element.querySelector(".changes__declaration-value")
      .textContent;
    const isRemoved = element.classList.contains("diff-remove");
    const text = isRemoved ? `/* ${name}: ${value}; */` : `${name}: ${value};`;
    clipboardHelper.copyString(text);
    this.telemetry.scalarAdd(TELEMETRY_SCALAR_CONTEXTMENU_COPY_DECLARATION, 1);
  }

  /**
   * Handler for the "Copy Rule" option from the context menu and "Copy Rule" button.
   * Gets the full content of the target CSS rule (including any changes applied)
   * and copies it to the clipboard.
   *
   * @param {String} ruleId
   *        Rule id of the target CSS rule.
   * @param {Boolean} usingContextMenu
   *        True if the handler is invoked from the context menu.
   *        (Default) False if invoked from the button.
   */
  async copyRule(ruleId, usingContextMenu = false) {
    const rule = await this.inspector.pageStyle.getRule(ruleId);
    const text = await rule.getRuleText();
    clipboardHelper.copyString(text);

    if (usingContextMenu) {
      this.telemetry.scalarAdd(TELEMETRY_SCALAR_CONTEXTMENU_COPY_RULE, 1);
    } else {
      this.telemetry.scalarAdd(TELEMETRY_SCALAR_COPY_RULE, 1);
    }
  }

  /**
   * Handler for the "Copy" option from the context menu.
   * Copies the current text selection to the clipboard.
   */
  copySelection() {
    clipboardHelper.copyString(this.window.getSelection().toString());
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
  }

  /**
   * Destruction function called when the inspector is destroyed.
   */
  destroy() {
    this.store.dispatch(resetChanges());

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
