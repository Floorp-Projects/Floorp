/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://testing-common/Assert.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Services.jsm");

Cu.import("resource://devtools/client/shared/browser-loader.js");
var { require } = BrowserLoader("resource://devtools/client/memory/", this);

var DevToolsUtils = require("devtools/shared/DevToolsUtils");
DevToolsUtils.testing = true;
var { immutableUpdate } = DevToolsUtils;

var constants = require("devtools/client/memory/constants");
var {
  diffingState,
  dominatorTreeBreakdowns,
  dominatorTreeState,
  snapshotState,
  viewState
} = constants;

const {
  getBreakdownDisplayData,
  getDominatorTreeBreakdownDisplayData,
} = require("devtools/client/memory/utils");

var models = require("devtools/client/memory/models");

var React = require("devtools/client/shared/vendor/react");
var ReactDOM = require("devtools/client/shared/vendor/react-dom");
var Heap = React.createFactory(require("devtools/client/memory/components/heap"));
var DominatorTreeComponent = React.createFactory(require("devtools/client/memory/components/dominator-tree"));
var DominatorTreeItem = React.createFactory(require("devtools/client/memory/components/dominator-tree-item"));
var Toolbar = React.createFactory(require("devtools/client/memory/components/toolbar"));

// All tests are asynchronous.
SimpleTest.waitForExplicitFinish();

var noop = () => {};

// Counter for mock DominatorTreeNode ids.
var TEST_NODE_ID_COUNTER = 0;

/**
 * Create a mock DominatorTreeNode for testing, with sane defaults. Override any
 * property by providing it on `opts`. Optionally pass child nodes as well.
 *
 * @param {Object} opts
 * @param {Array<DominatorTreeNode>?} children
 *
 * @returns {DominatorTreeNode}
 */
function makeTestDominatorTreeNode(opts, children) {
  const nodeId = TEST_NODE_ID_COUNTER++;

  const node = Object.assign({
    nodeId,
    label: ["other", "SomeType"],
    shallowSize: 1,
    retainedSize: (children || []).reduce((size, c) => size + c.retainedSize, 1),
    parentId: undefined,
    children,
    moreChildrenAvailable: true,
  }, opts);

  if (children && children.length) {
    children.map(c => c.parentId = node.nodeId);
  }

  return node;
}

var TEST_DOMINATOR_TREE = Object.freeze({
  dominatorTreeId: 666,
  root: (function makeTree(depth = 0) {
    let children;
    if (depth <= 3) {
      children = [
        makeTree(depth + 1),
        makeTree(depth + 1),
        makeTree(depth + 1),
      ];
    }
    return makeTestDominatorTreeNode({}, children);
  }()),
  expanded: new Set(),
  focused: null,
  error: null,
  breakdown: dominatorTreeBreakdowns.coarseType.breakdown,
  activeFetchRequestCount: null,
  state: dominatorTreeState.LOADED,
});

var TEST_DOMINATOR_TREE_PROPS = Object.freeze({
  dominatorTree: TEST_DOMINATOR_TREE,
  onLoadMoreSiblings: noop,
  onViewSourceInDebugger: noop,
  onExpand: noop,
  onCollapse: noop,
});

var TEST_HEAP_PROPS = Object.freeze({
  onSnapshotClick: noop,
  onLoadMoreSiblings: noop,
  onCensusExpand: noop,
  onCensusCollapse: noop,
  onDominatorTreeExpand: noop,
  onDominatorTreeCollapse: noop,
  onCensusFocus: noop,
  onDominatorTreeFocus: noop,
  onViewSourceInDebugger: noop,
  diffing: null,
  view: viewState.CENSUS,
  snapshot: Object.freeze({
    id: 1337,
    selected: true,
    path: "/fake/path/to/snapshot",
    census: Object.freeze({
      report: Object.freeze({
        objects: Object.freeze({ count: 4, bytes: 400 }),
        scripts: Object.freeze({ count: 3, bytes: 300 }),
        strings: Object.freeze({ count: 2, bytes: 200 }),
        other: Object.freeze({ count: 1, bytes: 100 }),
      }),
      breakdown: Object.freeze({
        by: "coarseType",
        objects: Object.freeze({ by: "count", count: true, bytes: true }),
        scripts: Object.freeze({ by: "count", count: true, bytes: true }),
        strings: Object.freeze({ by: "count", count: true, bytes: true }),
        other: Object.freeze({ by: "count", count: true, bytes: true }),
      }),
      inverted: false,
      filter: null,
      expanded: new Set(),
      focused: null,
    }),
    dominatorTree: TEST_DOMINATOR_TREE,
    error: null,
    imported: false,
    creationTime: 0,
    state: snapshotState.SAVED_CENSUS,
  }),
});

var TEST_TOOLBAR_PROPS = Object.freeze({
  breakdowns: getBreakdownDisplayData(),
  onTakeSnapshotClick: noop,
  onImportClick: noop,
  onBreakdownChange: noop,
  onToggleRecordAllocationStacks: noop,
  allocations: models.allocations,
  onToggleInverted: noop,
  inverted: false,
  filterString: null,
  setFilterString: noop,
  diffing: null,
  onToggleDiffing: noop,
  view: viewState.CENSUS,
  onViewChange: noop,
  dominatorTreeBreakdowns: getDominatorTreeBreakdownDisplayData(),
  onDominatorTreeBreakdownChange: noop,
  snapshots: [],
});

function onNextAnimationFrame(fn) {
  return () =>
    requestAnimationFrame(() =>
      requestAnimationFrame(fn));
}

function renderComponent(component, container) {
  return new Promise(resolve => {
    ReactDOM.render(component, container, onNextAnimationFrame(() => {
      dumpn("Rendered = " + container.innerHTML);
      resolve();
    }));
  });
}

function setState(component, newState) {
  return new Promise(resolve => {
    component.setState(newState, onNextAnimationFrame(resolve));
  });
}

function setProps(component, newProps) {
  return new Promise(resolve => {
    component.setProps(newProps, onNextAnimationFrame(resolve));
  });
}

function dumpn(msg) {
  dump(`MEMORY-TEST: ${msg}\n`);
}
