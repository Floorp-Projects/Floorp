/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  createElement,
} = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const { updateSelectedNode } = require("./actions/compatibility");

const CompatibilityApp = createFactory(
  require("./components/CompatibilityApp")
);

class CompatibilityView {
  constructor(inspector, window) {
    this._onNewNode = this._onNewNode.bind(this);
    this.inspector = inspector;

    this._init();
  }

  destroy() {
    this.inspector.selection.off("new-node-front", this._onNewNode);
    this.inspector.sidebar.off("compatibilityview-selected", this._onNewNode);

    if (this._ruleView) {
      this._ruleView.off("ruleview-changed", this._onNewNode);
    }

    this.inspector = null;
  }

  _init() {
    const compatibilityApp = new CompatibilityApp();

    this.provider = createElement(
      Provider,
      {
        id: "compatibilityview",
        store: this.inspector.store,
      },
      compatibilityApp
    );

    this.inspector.selection.on("new-node-front", this._onNewNode);
    this.inspector.sidebar.on("compatibilityview-selected", this._onNewNode);

    if (this._ruleView) {
      this._ruleView.on("ruleview-changed", this._onNewNode);
    } else {
      this.inspector.on(
        "ruleview-added",
        () => {
          this._ruleView.on("ruleview-changed", this._onNewNode);
        },
        { once: true }
      );
    }
  }

  _isVisible() {
    return (
      this.inspector &&
      this.inspector.sidebar &&
      this.inspector.sidebar.getCurrentTabID() === "compatibilityview"
    );
  }

  _onNewNode() {
    if (!this._isVisible()) {
      return;
    }

    this.inspector.store.dispatch(
      updateSelectedNode(this.inspector.selection.nodeFront)
    );
  }

  get _ruleView() {
    return (
      this.inspector.hasPanel("ruleview") &&
      this.inspector.getPanel("ruleview").view
    );
  }
}

module.exports = CompatibilityView;
