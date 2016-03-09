/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://testing-common/Assert.jsm");
Cu.import("resource://gre/modules/Task.jsm");

var { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
var { gDevTools } = require("devtools/client/framework/devtools");
var { BrowserLoader } = Cu.import("resource://devtools/client/shared/browser-loader.js", {});
var promise = require("promise");
var Services = require("Services");
var { DebuggerServer } = require("devtools/server/main");
var { DebuggerClient } = require("devtools/shared/client/main");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var { TargetFactory } = require("devtools/client/framework/target");
var { Toolbox } = require("devtools/client/framework/toolbox");

DevToolsUtils.testing = true;
var { require: browserRequire } = BrowserLoader({
  baseURI: "resource://devtools/client/shared/",
  window: this
});

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
  var deferred = promise.defer();
  component.setState(newState, onNextAnimationFrame(deferred.resolve));
  return deferred.promise;
}

function setProps(component, newState) {
  var deferred = promise.defer();
  component.setProps(newState, onNextAnimationFrame(deferred.resolve));
  return deferred.promise;
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
  renderItem: (x, depth, focused, arrow) => "-".repeat(depth) + x + ":" + focused + "\n",
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
function checkFrameString (component, file, line, column) {
  let el = component.getDOMNode();
  let $ = selector => el.querySelector(selector);

  let $line = $(".frame-link-line");
  let $column = $(".frame-link-column");

  is($(".frame-link-filename").textContent, file);

  is(el.getAttribute("data-line"), line ? `${line}` : null, "Expected `data-line` found");
  is(el.getAttribute("data-column"), column ? `${column}` : null, "Expected `data-column` found");

  if (line != null) {
    is(+$line.textContent, +line);
  } else {
    ok(!$line, "Should not have an element for `line`");
  }

  if (column != null) {
    is(+$column.textContent, +column);
  } else {
    ok(!$column, "Should not have an element for `column`");
  }
}

function checkFrameTooltips (component, mainTooltip, linkTooltip) {
  let el = component.getDOMNode();
  is(el.getAttribute("title"), mainTooltip);
  is(el.querySelector("a.frame-link-filename").getAttribute("title"), linkTooltip);
}
