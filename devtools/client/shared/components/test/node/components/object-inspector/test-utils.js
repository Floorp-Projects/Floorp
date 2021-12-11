/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { mount } = require("enzyme");
const { createFactory } = require("devtools/client/shared/vendor/react");

const { Provider } = require("devtools/client/shared/vendor/react-redux");
const {
  combineReducers,
  createStore,
  applyMiddleware,
} = require("devtools/client/shared/vendor/redux");

const { thunk } = require("devtools/client/shared/redux/middleware/thunk");
const {
  waitUntilService,
} = require("devtools/client/shared/redux/middleware/wait-service");

/**
 * Redux store utils
 * @module utils/create-store
 */
const objectInspector = require("devtools/client/shared/components/object-inspector/index");
const {
  getLoadedProperties,
  getLoadedPropertyKeys,
  getExpandedPaths,
  getExpandedPathKeys,
} = require("devtools/client/shared/components/object-inspector/reducer");

const ObjectInspector = createFactory(objectInspector.ObjectInspector);

const {
  NAME: WAIT_UNTIL_TYPE,
} = require("devtools/client/shared/redux/middleware/wait-service");

/*
 * Takes an Enzyme wrapper (obtained with mount/shallow/…) and
 * returns a stringified version of the ObjectInspector, e.g.
 *
 *   ▼ Map { "a" → "value-a", "b" → "value-b" }
 *   |    size : 2
 *   |  ▼ <entries>
 *   |  |  ▼ 0 : "a" → "value-a"
 *   |  |  |    <key> : "a"
 *   |  |  |    <value> : "value-a"
 *   |  |  ▼ 1 : "b" → "value-b"
 *   |  |  |    <key> : "b"
 *   |  |  |    <value> : "value-b"
 *   |  ▼ <prototype> : Object { … }
 *
 */
function formatObjectInspector(wrapper) {
  const hasFocusedNode = wrapper.find(".tree-node.focused").length > 0;
  const textTree = wrapper
    .find(".tree-node")
    .map(node => {
      const indentStr = "|  ".repeat((node.prop("aria-level") || 1) - 1);
      // Need to target .arrow or Enzyme will also match the ArrowExpander
      // component.
      const arrow = node.find(".arrow");
      let arrowStr = "  ";
      if (arrow.exists()) {
        arrowStr = arrow.hasClass("expanded") ? "▼ " : "▶︎ ";
      } else {
        arrowStr = "  ";
      }

      const icon = node
        .find(".node")
        .first()
        .hasClass("block")
        ? "☲ "
        : "";
      let text = `${indentStr}${arrowStr}${icon}${getSanitizedNodeText(node)}`;

      if (node.find("button.invoke-getter").exists()) {
        text = `${text}(>>)`;
      }

      if (!hasFocusedNode) {
        return text;
      }
      return node.hasClass("focused") ? `[ ${text} ]` : `  ${text}`;
    })
    .join("\n");
  // Wrap the text representation in new lines so it keeps alignment between
  // tree nodes.
  return `\n${textTree}\n`;
}

function getSanitizedNodeText(node) {
  // Stripping off the invisible space used in the indent.
  return node.text().replace(/^\u200B+/, "");
}

/**
 * Wait for a specific action type to be dispatched.
 *
 * @param {Object} store: Redux store
 * @param {String} type: type of the actin to wait for
 * @return {Promise}
 */
function waitForDispatch(store, type) {
  return new Promise(resolve => {
    store.dispatch({
      type: WAIT_UNTIL_TYPE,
      predicate: action => action.type === type,
      run: (dispatch, getState, action) => {
        resolve(action);
      },
    });
  });
}

/**
 * Wait until the condition evaluates to something truthy
 * @param {function} condition: function that we need for returning something
 *                              truthy.
 * @param {int} interval: Time to wait before trying to evaluate condition again
 * @param {int} maxTries: Number of evaluation to try.
 */
async function waitFor(condition, interval = 50, maxTries = 100) {
  let res = condition();
  while (!res) {
    await new Promise(done => setTimeout(done, interval));
    maxTries--;

    if (maxTries <= 0) {
      throw new Error("waitFor - maxTries limit hit");
    }

    res = condition();
  }
  return res;
}

/**
 * Wait until the state has all the expected keys for the loadedProperties
 * state prop.
 * @param {Redux Store} store: function that we need for returning something
 *                             truthy.
 * @param {Array} expectedKeys: Array of stringified keys.
 * @param {int} interval: Time to wait before trying to evaluate condition again
 * @param {int} maxTries: Number of evaluation to try.
 */
function waitForLoadedProperties(store, expectedKeys, interval, maxTries) {
  return waitFor(
    () => storeHasLoadedPropertiesKeys(store, expectedKeys),
    interval,
    maxTries
  );
}

function storeHasLoadedPropertiesKeys(store, expectedKeys) {
  return expectedKeys.every(key => storeHasLoadedProperty(store, key));
}

function storeHasLoadedProperty(store, key) {
  return getLoadedPropertyKeys(store.getState()).some(
    k => k.toString() === key
  );
}

function storeHasExactLoadedProperties(store, expectedKeys) {
  return (
    expectedKeys.length === getLoadedProperties(store.getState()).size &&
    expectedKeys.every(key => storeHasLoadedProperty(store, key))
  );
}

function storeHasExpandedPaths(store, expectedKeys) {
  return expectedKeys.every(key => storeHasExpandedPath(store, key));
}

function storeHasExpandedPath(store, key) {
  return getExpandedPathKeys(store.getState()).some(k => k.toString() === key);
}

function storeHasExactExpandedPaths(store, expectedKeys) {
  return (
    expectedKeys.length === getExpandedPaths(store.getState()).size &&
    expectedKeys.every(key => storeHasExpandedPath(store, key))
  );
}

function createOiStore(client, initialState = {}) {
  const reducers = { objectInspector: objectInspector.reducer.default };
  return configureStore({
    thunkArgs: { client },
  })(combineReducers(reducers), initialState);
}

const configureStore = (opts = {}) => {
  const middleware = [thunk(opts.thunkArgs), waitUntilService];
  return applyMiddleware(...middleware)(createStore);
};

function mountObjectInspector({ props, client, initialState = {} }) {
  if (initialState.objectInspector) {
    initialState.objectInspector = {
      expandedPaths: new Set(),
      loadedProperties: new Map(),
      ...initialState.objectInspector,
    };
  }
  const store = createOiStore(client, initialState);
  const wrapper = mount(
    createFactory(Provider)({ store }, ObjectInspector(props))
  );

  const tree = wrapper.find(".tree");

  return { store, tree, wrapper, client };
}

module.exports = {
  formatObjectInspector,
  storeHasExpandedPaths,
  storeHasExpandedPath,
  storeHasExactExpandedPaths,
  storeHasLoadedPropertiesKeys,
  storeHasLoadedProperty,
  storeHasExactLoadedProperties,
  waitFor,
  waitForDispatch,
  waitForLoadedProperties,
  mountObjectInspector,
  createStore: createOiStore,
};
