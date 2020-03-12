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
  "browsersDataset",
  "devtools/client/inspector/compatibility/lib/dataset/browsers.json"
);

const {
  updateSelectedNode,
  updateTargetBrowsers,
} = require("devtools/client/inspector/compatibility/actions/compatibility");

const CompatibilityApp = createFactory(
  require("devtools/client/inspector/compatibility/components/CompatibilityApp")
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

    this._initTargetBrowsers();
  }

  async _initTargetBrowsers() {
    this.inspector.store.dispatch(
      updateTargetBrowsers(_getDefaultTargetBrowsers())
    );
  }

  _isAvailable() {
    return (
      this.inspector &&
      this.inspector.sidebar &&
      this.inspector.sidebar.getCurrentTabID() === "compatibilityview" &&
      this.inspector.selection &&
      this.inspector.selection.isConnected()
    );
  }

  _onNewNode() {
    if (!this._isAvailable()) {
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

/**
 * Return target browsers that will be checked as default.
 * The default target browsers includes major browsers that have been releasing as `esr`,
 * `release`, `beta` or `nightly`.
 *
 * @return e.g, [{ id: "firefox", name: "Firefox", version: "70", status: "nightly"},...]
 */
function _getDefaultTargetBrowsers() {
  const TARGET_BROWSER_ID = [
    "firefox",
    "firefox_android",
    "chrome",
    "chrome_android",
    "safari",
    "safari_ios",
    "edge",
  ];
  const TARGET_BROWSER_STATUS = ["esr", "current", "beta", "nightly"];

  // Retrieve the information that matches to the browser id and the status
  // from the browsersDataset.
  // For the structure of then browsersDataset,
  // see https://github.com/mdn/browser-compat-data/blob/master/browsers/firefox.json
  const targets = [];
  for (const id of TARGET_BROWSER_ID) {
    const { name, releases } = browsersDataset[id];

    for (const version in releases) {
      const { status } = releases[version];

      if (TARGET_BROWSER_STATUS.includes(status)) {
        targets.push({ id, name, version, status });
      }
    }
  }
  return targets;
}

module.exports = CompatibilityView;
