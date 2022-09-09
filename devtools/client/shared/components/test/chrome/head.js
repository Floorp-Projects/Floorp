/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint no-unused-vars: [2, {"vars": "local"}] */

/* global _snapshots */

"use strict";

var { require } = ChromeUtils.import(
  "resource://devtools/shared/loader/Loader.jsm"
);
var { Assert } = require("resource://testing-common/Assert.jsm");
var { gDevTools } = require("devtools/client/framework/devtools");
var { BrowserLoader } = ChromeUtils.import(
  "resource://devtools/shared/loader/browser-loader.js"
);
var { DevToolsServer } = require("devtools/server/devtools-server");
var { DevToolsClient } = require("devtools/client/devtools-client");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var { Toolbox } = require("devtools/client/framework/toolbox");

var { require: browserRequire } = BrowserLoader({
  baseURI: "resource://devtools/client/shared/",
  window,
});

const React = browserRequire("devtools/client/shared/vendor/react");
const ReactDOM = browserRequire("devtools/client/shared/vendor/react-dom");
const dom = browserRequire("devtools/client/shared/vendor/react-dom-factories");
const TestUtils = browserRequire(
  "devtools/client/shared/vendor/react-dom-test-utils"
);

const ShallowRenderer = browserRequire(
  "devtools/client/shared/vendor/react-test-renderer-shallow"
);
const TestRenderer = browserRequire(
  "devtools/client/shared/vendor/react-test-renderer"
);

var EXAMPLE_URL = "https://example.com/browser/browser/devtools/shared/test/";

SimpleTest.registerCleanupFunction(() => {
  window._snapshots = null;
});

function forceRender(comp) {
  return setState(comp, {}).then(() => setState(comp, {}));
}

// All tests are asynchronous.
SimpleTest.waitForExplicitFinish();

function onNextAnimationFrame(fn) {
  return () => requestAnimationFrame(() => requestAnimationFrame(fn));
}

function setState(component, newState) {
  return new Promise(resolve => {
    component.setState(newState, onNextAnimationFrame(resolve));
  });
}

function dumpn(msg) {
  dump(`SHARED-COMPONENTS-TEST: ${msg}\n`);
}

/**
 * Tree View
 */

const TEST_TREE_VIEW = {
  A: { label: "A", value: "A" },
  B: { label: "B", value: "B" },
  C: { label: "C", value: "C" },
  D: { label: "D", value: "D" },
  E: { label: "E", value: "E" },
  F: { label: "F", value: "F" },
  G: { label: "G", value: "G" },
  H: { label: "H", value: "H" },
  I: { label: "I", value: "I" },
  J: { label: "J", value: "J" },
  K: { label: "K", value: "K" },
  L: { label: "L", value: "L" },
};

TEST_TREE_VIEW.children = {
  A: [TEST_TREE_VIEW.B, TEST_TREE_VIEW.C, TEST_TREE_VIEW.D],
  B: [TEST_TREE_VIEW.E, TEST_TREE_VIEW.F, TEST_TREE_VIEW.G],
  C: [TEST_TREE_VIEW.H, TEST_TREE_VIEW.I],
  D: [TEST_TREE_VIEW.J],
  E: [TEST_TREE_VIEW.K, TEST_TREE_VIEW.L],
  F: [],
  G: [],
  H: [],
  I: [],
  J: [],
  K: [],
  L: [],
};

const TEST_TREE_VIEW_INTERFACE = {
  provider: {
    getChildren: x => TEST_TREE_VIEW.children[x.label],
    hasChildren: x => !!TEST_TREE_VIEW.children[x.label].length,
    getLabel: x => x.label,
    getValue: x => x.value,
    getKey: x => x.label,
    getType: () => "string",
  },
  object: TEST_TREE_VIEW.A,
  columns: [{ id: "default" }, { id: "value" }],
};

/**
 * Tree
 */

var TEST_TREE_INTERFACE = {
  getParent: x => TEST_TREE.parent[x],
  getChildren: x => TEST_TREE.children[x],
  renderItem: (x, depth, focused) =>
    "-".repeat(depth) + x + ":" + focused + "\n",
  getRoots: () => ["A", "M"],
  getKey: x => "key-" + x,
  itemHeight: 1,
  onExpand: x => TEST_TREE.expanded.add(x),
  onCollapse: x => TEST_TREE.expanded.delete(x),
  isExpanded: x => TEST_TREE.expanded.has(x),
};

function isRenderedTree(actual, expectedDescription, msg) {
  const expected = expectedDescription.map(x => x + "\n").join("");
  dumpn(`Expected tree:\n${expected}`);
  dumpn(`Actual tree:\n${actual}`);
  is(actual, expected, msg);
}

function isAccessibleTree(tree, options = {}) {
  const treeNode = tree.refs.tree;
  is(treeNode.getAttribute("tabindex"), "0", "Tab index is set");
  is(treeNode.getAttribute("role"), "tree", "Tree semantics is present");
  if (options.hasActiveDescendant) {
    ok(
      treeNode.hasAttribute("aria-activedescendant"),
      "Tree has an active descendant set"
    );
  }

  const treeNodes = [...treeNode.querySelectorAll(".tree-node")];
  for (const node of treeNodes) {
    ok(node.id, "TreeNode has an id");
    is(node.getAttribute("role"), "treeitem", "Tree item semantics is present");
    is(
      parseInt(node.getAttribute("aria-level"), 10),
      parseInt(node.getAttribute("data-depth"), 10) + 1,
      "Aria level attribute is set correctly"
    );
  }
}

// Encoding of the following tree/forest:
//
// A
// |-- B
// |   |-- E
// |   |   |-- K
// |   |   `-- L
// |   |-- F
// |   `-- G
// |-- C
// |   |-- H
// |   `-- I
// `-- D
//     `-- J
// M
// `-- N
//     `-- O
var TEST_TREE = {
  children: {
    A: ["B", "C", "D"],
    B: ["E", "F", "G"],
    C: ["H", "I"],
    D: ["J"],
    E: ["K", "L"],
    F: [],
    G: [],
    H: [],
    I: [],
    J: [],
    K: [],
    L: [],
    M: ["N"],
    N: ["O"],
    O: [],
  },
  parent: {
    A: null,
    B: "A",
    C: "A",
    D: "A",
    E: "B",
    F: "B",
    G: "B",
    H: "C",
    I: "C",
    J: "D",
    K: "E",
    L: "E",
    M: null,
    N: "M",
    O: "N",
  },
  expanded: new Set(),
};

/**
 * Frame
 */
function checkFrameString({
  el,
  file,
  line,
  column,
  source,
  functionName,
  shouldLink,
  tooltip,
  locationPrefix,
}) {
  const $ = selector => el.querySelector(selector);

  const $func = $(".frame-link-function-display-name");
  const $source = $(".frame-link-source");
  const $locationPrefix = $(".frame-link-prefix");
  const $filename = $(".frame-link-filename");
  const $line = $(".frame-link-line");

  is($filename.textContent, file, "Correct filename");
  is(
    el.getAttribute("data-line"),
    line ? `${line}` : null,
    "Expected `data-line` found"
  );
  is(
    el.getAttribute("data-column"),
    column ? `${column}` : null,
    "Expected `data-column` found"
  );
  is($source.getAttribute("title"), tooltip, "Correct tooltip");
  is($source.tagName, shouldLink ? "A" : "SPAN", "Correct linkable status");
  if (shouldLink) {
    is($source.getAttribute("href"), source, "Correct source");
  }

  if (line != null) {
    let lineText = `:${line}`;
    if (column != null) {
      lineText += `:${column}`;
    }

    is($line.textContent, lineText, "Correct line number");
  } else {
    ok(!$line, "Should not have an element for `line`");
  }

  if (functionName != null) {
    is($func.textContent, functionName, "Correct function name");
  } else {
    ok(!$func, "Should not have an element for `functionName`");
  }

  if (locationPrefix != null) {
    is($locationPrefix.textContent, locationPrefix, "Correct prefix");
  } else {
    ok(!$locationPrefix, "Should not have an element for `locationPrefix`");
  }
}

function checkSmartFrameString({ el, location, functionName, tooltip }) {
  const $ = selector => el.querySelector(selector);

  const $func = $(".title");
  const $location = $(".location");

  is($location.textContent, location, "Correct filename");
  is(el.getAttribute("title"), tooltip, "Correct tooltip");
  if (functionName != null) {
    is($func.textContent, functionName, "Correct function name");
  } else {
    ok(!$func, "Should not have an element for `functionName`");
  }
}

function renderComponent(component, props) {
  const el = React.createElement(component, props, {});
  // By default, renderIntoDocument() won't work for stateless components, but
  // it will work if the stateless component is wrapped in a stateful one.
  // See https://github.com/facebook/react/issues/4839
  const wrappedEl = dom.span({}, [el]);
  const renderedComponent = TestUtils.renderIntoDocument(wrappedEl);
  return ReactDOM.findDOMNode(renderedComponent).children[0];
}

function shallowRenderComponent(component, props) {
  const el = React.createElement(component, props);
  const renderer = new ShallowRenderer();
  renderer.render(el, {});
  return renderer.getRenderOutput();
}

/**
 * Creates a React Component for testing
 *
 * @param {string} factory - factory object of the component to be created
 * @param {object} props - React props for the component
 * @returns {object} - container Node, Object with React component
 * and querySelector function with $ as name.
 */
async function createComponentTest(factory, props) {
  const container = document.createElement("div");
  document.body.appendChild(container);

  const component = ReactDOM.render(factory(props), container);
  await forceRender(component);

  return {
    container,
    component,
    $: s => container.querySelector(s),
  };
}

async function waitFor(condition = () => true, delay = 50) {
  do {
    const res = condition();
    if (res) {
      return res;
    }
    await new Promise(resolve => setTimeout(resolve, delay));
  } while (true);
}

/**
 * Matches a component tree rendererd using TestRenderer to a given expected JSON
 * snapshot.
 * @param  {String} name
 *         Name of the function derived from a test [step] name.
 * @param  {Object} el
 *         React element to be rendered using TestRenderer.
 */
function matchSnapshot(name, el) {
  if (!_snapshots) {
    is(false, "No snapshots were loaded into test.");
  }

  const snapshot = _snapshots[name];
  if (snapshot === undefined) {
    is(false, `Snapshot for "${name}" not found.`);
  }

  const renderer = TestRenderer.create(el, {});
  const tree = renderer.toJSON();

  is(
    JSON.stringify(tree, (key, value) =>
      typeof value === "function" ? value.toString() : value
    ),
    JSON.stringify(snapshot),
    name
  );
}
