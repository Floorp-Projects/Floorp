/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* global EVENTS */

const nodeConstants = require("devtools/shared/dom-node-constants");

// React & Redux
const {
  createFactory,
  createElement,
} = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

// Accessibility Panel
const MainFrame = createFactory(
  require("devtools/client/accessibility/components/MainFrame")
);

// Store
const createStore = require("devtools/client/shared/redux/create-store");

// Reducers
const { reducers } = require("devtools/client/accessibility/reducers/index");
const store = createStore(reducers);

// Actions
const { reset } = require("devtools/client/accessibility/actions/ui");
const {
  select,
  highlight,
} = require("devtools/client/accessibility/actions/accessibles");

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
   * @param {Object}
   *        Object that contains the following properties:
   * - supports                              {JSON}
   *                                         a collection of flags indicating
   *                                         which accessibility panel features
   *                                         are supported by the current
   *                                         serverside version.
   * - fluentBundles                         {Array}
   *                                         array of FluentBundles elements for
   *                                         localization
   * - toolbox                               {Object}
   *                                         devtools toolbox.
   * - getAccessibilityTreeRoot              {Function}
   *                                         Returns the topmost accessibiliity
   *                                         walker that is used as the root of
   *                                         the accessibility tree.
   * - startListeningForAccessibilityEvents  {Function}
   *                                         Add listeners for specific
   *                                         accessibility events.
   * - stopListeningForAccessibilityEvents   {Function}
   *                                         Remove listeners for specific
   *                                         accessibility events.
   * - audit                                 {Function}
   *                                         Audit function that will start
   *                                         accessibility audit for given types
   *                                         of accessibility issues.
   * - simulate                              {null|Function}
   *                                         Apply simulation of a given type
   *                                         (by setting color matrices in
   *                                         docShell).
   * - enableAccessibility                   {Function}
   *                                         Enable accessibility services.
   * - disableAccessibility                  {Function}
   *                                         Disable accessibility services.
   * - resetAccessiblity                     {Function}
   *                                         Reset the state of the
   *                                         accessibility services.
   * - startListeningForLifecycleEvents      {Function}
   *                                         Add listeners for accessibility
   *                                         service lifecycle events.
   * - stopListeningForLifecycleEvents       {Function}
   *                                         Remove listeners for accessibility
   *                                         service lifecycle events.
   */
  async initialize({
    supports,
    fluentBundles,
    toolbox,
    getAccessibilityTreeRoot,
    startListeningForAccessibilityEvents,
    stopListeningForAccessibilityEvents,
    audit,
    simulate,
    enableAccessibility,
    disableAccessibility,
    resetAccessiblity,
    startListeningForLifecycleEvents,
    stopListeningForLifecycleEvents,
  }) {
    // Make sure state is reset every time accessibility panel is initialized.
    await this.store.dispatch(reset(resetAccessiblity, supports));
    const container = document.getElementById("content");
    const mainFrame = MainFrame({
      fluentBundles,
      toolbox,
      getAccessibilityTreeRoot,
      startListeningForAccessibilityEvents,
      stopListeningForAccessibilityEvents,
      audit,
      simulate,
      enableAccessibility,
      disableAccessibility,
      resetAccessiblity,
      startListeningForLifecycleEvents,
      stopListeningForLifecycleEvents,
    });
    // Render top level component
    const provider = createElement(Provider, { store: this.store }, mainFrame);
    this.mainFrame = ReactDOM.render(provider, container);
  },

  destroy() {
    const container = document.getElementById("content");
    ReactDOM.unmountComponentAtNode(container);
  },

  async selectAccessible(accessible) {
    await this.store.dispatch(select(accessible));
    window.emit(EVENTS.NEW_ACCESSIBLE_FRONT_INSPECTED);
  },

  async highlightAccessible(accessible) {
    await this.store.dispatch(highlight(accessible));
    window.emit(EVENTS.NEW_ACCESSIBLE_FRONT_HIGHLIGHTED);
  },

  async selectNodeAccessible(node) {
    if (!node) {
      return;
    }

    const accessibilityFront = await node.targetFront.getFront("accessibility");
    const accessibleWalkerFront = await accessibilityFront.getWalker();
    let accessible = await accessibleWalkerFront.getAccessibleFor(node);
    if (accessible) {
      await accessible.hydrate();
    }

    // If node does not have an accessible object, try to find node's child text node and
    // try to retrieve an accessible object for that child instead. This is the best
    // effort approach until there's accessibility API to retrieve accessible object at
    // point.
    if (!accessible || accessible.indexInParent < 0) {
      const { nodes: children } = await node.walkerFront.children(node);
      for (const child of children) {
        if (child.nodeType === nodeConstants.TEXT_NODE) {
          accessible = await accessibleWalkerFront.getAccessibleFor(child);
          // indexInParent property is only available with additional request
          // for data (hydration) about the accessible object.
          if (accessible) {
            await accessible.hydrate();
            if (accessible.indexInParent >= 0) {
              break;
            }
          }
        }
      }
    }

    // Attempt to find closest accessible ancestor for a given node.
    if (!accessible || accessible.indexInParent < 0) {
      let parentNode = node.parentNode();
      while (parentNode) {
        accessible = await accessibleWalkerFront.getAccessibleFor(parentNode);
        if (accessible) {
          await accessible.hydrate();
          if (accessible.indexInParent >= 0) {
            break;
          }
        }

        parentNode = parentNode.parentNode();
      }
    }

    // Do not set the selected state if there is no corresponding accessible.
    if (!accessible) {
      console.warn(
        `No accessible object found for a node or a node in its ancestry: ${node.actorID}`
      );
      return;
    }

    await this.store.dispatch(select(accessible));
    window.emit(EVENTS.NEW_ACCESSIBLE_FRONT_INSPECTED);
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
