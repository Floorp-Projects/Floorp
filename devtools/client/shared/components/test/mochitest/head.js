/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint no-unused-vars: [2, {"vars": "local"}] */

"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

var { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
var { Assert } = require("resource://testing-common/Assert.jsm");
var { gDevTools } = require("devtools/client/framework/devtools");
var { BrowserLoader } = Cu.import("resource://devtools/client/shared/browser-loader.js", {});
var promise = require("promise");
var defer = require("devtools/shared/defer");
var Services = require("Services");
var { DebuggerServer } = require("devtools/server/main");
var { DebuggerClient } = require("devtools/shared/client/debugger-client");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var flags = require("devtools/shared/flags");
var { Task } = require("devtools/shared/task");
var { TargetFactory } = require("devtools/client/framework/target");
var { Toolbox } = require("devtools/client/framework/toolbox");

flags.testing = true;
var { require: browserRequire } = BrowserLoader({
  baseURI: "resource://devtools/client/shared/",
  window
});

let React = browserRequire("devtools/client/shared/vendor/react");
let ReactDOM = browserRequire("devtools/client/shared/vendor/react-dom");
let dom = browserRequire("devtools/client/shared/vendor/react-dom-factories");
let TestUtils = ReactDOM.TestUtils;

var EXAMPLE_URL = "http://example.com/browser/browser/devtools/shared/test/";

function forceRender(comp) {
  return setState(comp, {})
    .then(() => setState(comp, {}));
}

// All tests are asynchronous.
SimpleTest.waitForExplicitFinish();

function onNextAnimationFrame(fn) {
  return () =>
    requestAnimationFrame(() =>
      requestAnimationFrame(fn));
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
 * Tree
 */

var TEST_TREE_INTERFACE = {
  getParent: x => TEST_TREE.parent[x],
  getChildren: x => TEST_TREE.children[x],
  renderItem: (x, depth, focused) => "-".repeat(depth) + x + ":" + focused + "\n",
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
    ok(treeNode.hasAttribute("aria-activedescendant"),
       "Tree has an active descendant set");
  }

  const treeNodes = [...treeNode.querySelectorAll(".tree-node")];
  for (let node of treeNodes) {
    ok(node.id, "TreeNode has an id");
    is(node.getAttribute("role"), "treeitem", "Tree item semantics is present");
    ok(node.hasAttribute("aria-level"), "Aria level attribute is set");
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
    O: []
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
    O: "N"
  },
  expanded: new Set(),
};

/**
 * Frame
 */
function checkFrameString({
  el, file, line, column, source, functionName, shouldLink, tooltip
}) {
  let $ = selector => el.querySelector(selector);

  let $func = $(".frame-link-function-display-name");
  let $source = $(".frame-link-source");
  let $sourceInner = $(".frame-link-source-inner");
  let $filename = $(".frame-link-filename");
  let $line = $(".frame-link-line");

  is($filename.textContent, file, "Correct filename");
  is(el.getAttribute("data-line"), line ? `${line}` : null, "Expected `data-line` found");
  is(el.getAttribute("data-column"),
     column ? `${column}` : null, "Expected `data-column` found");
  is($sourceInner.getAttribute("title"), tooltip, "Correct tooltip");
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
  const renderer = TestUtils.createRenderer();
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
    $: (s) => container.querySelector(s),
  };
}
