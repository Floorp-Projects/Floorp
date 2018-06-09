/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* global EVENTS, gToolbox */

const nodeConstants = require("devtools/shared/dom-node-constants");

// React & Redux
const { createFactory, createElement } = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { Provider } = require("devtools/client/shared/vendor/react-redux");
const { combineReducers } = require("devtools/client/shared/vendor/redux");

// Accessibility Panel
const MainFrame = createFactory(require("./components/MainFrame"));
const OldVersionDescription =
  createFactory(require("./components/Description").OldVersionDescription);

// Store
const createStore = require("devtools/client/shared/redux/create-store")();

// Reducers
const { reducers } = require("./reducers/index");
const store = createStore(combineReducers(reducers));

// Actions
const { reset } = require("./actions/ui");
const { select, highlight } = require("./actions/accessibles");

/**
 * This object represents view of the Accessibility panel and is responsible
 * for rendering the content. It renders the top level ReactJS
 * component: the MainFrame.
 */
function AccessibilityView(localStore) {
  addEventListener("devtools/chrome/message", this.onMessage.bind(this), true);
  this.store = localStore;
}

AccessibilityView.prototype = {
  /**
   * Initialize accessibility view, create its top level component and set the
   * data store.
   *
   * @param {Object} accessibility  front that can initialize accessibility
   *                                walker and enable/disable accessibility
   *                                services.
   */
  async initialize(accessibility, walker, supportsLatestAccessibility) {
    // Make sure state is reset every time accessibility panel is initialized.
    await this.store.dispatch(reset(accessibility));
    const container = document.getElementById("content");

    if (!supportsLatestAccessibility) {
      ReactDOM.render(OldVersionDescription(), container);
      return;
    }

    const mainFrame = MainFrame({ accessibility, walker });
    // Render top level component
    const provider = createElement(Provider, { store: this.store }, mainFrame);
    this.mainFrame = ReactDOM.render(provider, container);
  },

  async selectAccessible(walker, accessible) {
    await this.store.dispatch(select(walker, accessible));
    window.emit(EVENTS.NEW_ACCESSIBLE_FRONT_INSPECTED);
  },

  async highlightAccessible(walker, accessible) {
    await this.store.dispatch(highlight(walker, accessible));
    window.emit(EVENTS.NEW_ACCESSIBLE_FRONT_HIGHLIGHTED);
  },

  async selectNodeAccessible(walker, node) {
    let accessible = await walker.getAccessibleFor(node);
    // If node does not have an accessible object, try to find node's child text node and
    // try to retrieve an accessible object for that child instead. This is the best
    // effort approach until there's accessibility API to retrieve accessible object at
    // point.
    if (!accessible || accessible.indexInParent < 0) {
      const { nodes: children } = await gToolbox.walker.children(node);
      for (const child of children) {
        if (child.nodeType === nodeConstants.TEXT_NODE) {
          accessible = await walker.getAccessibleFor(child);
          if (accessible && accessible.indexInParent >= 0) {
            break;
          }
        }
      }
    }

    await this.store.dispatch(select(walker, accessible));
    window.emit(EVENTS.NEW_ACCESSIBLE_FRONT_HIGHLIGHTED);
  },

  /**
   * Process message from accessibility panel.
   *
   * @param {Object} event  message type and data.
   */
  onMessage(event) {
    const data = event.data;
    const method = data.type;

    if (typeof this[method] === "function") {
      this[method](...data.args);
    }
  },
};

window.view = new AccessibilityView(store);
