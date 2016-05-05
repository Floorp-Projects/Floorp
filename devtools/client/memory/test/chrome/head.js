/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://testing-common/Assert.jsm");

Cu.import("resource://devtools/client/shared/browser-loader.js");
var { require } = BrowserLoader({
  baseURI: "resource://devtools/client/memory/",
  window: this
});
var Services = require("Services");
var { Task } = require("resource://gre/modules/Task.jsm");

var EXPECTED_DTU_ASSERT_FAILURE_COUNT = 0;

SimpleTest.registerCleanupFunction(function() {
  if (DevToolsUtils.assertionFailureCount !== EXPECTED_DTU_ASSERT_FAILURE_COUNT) {
    ok(false, "Should have had the expected number of DevToolsUtils.assert() failures. Expected " +
       EXPECTED_DTU_ASSERT_FAILURE_COUNT + ", got " + DevToolsUtils.assertionFailureCount);
  }
});

var DevToolsUtils = require("devtools/shared/DevToolsUtils");
DevToolsUtils.testing = true;
var { immutableUpdate } = DevToolsUtils;

var constants = require("devtools/client/memory/constants");
var {
  censusDisplays,
  diffingState,
  labelDisplays,
  dominatorTreeState,
  snapshotState,
  viewState,
  censusState
} = constants;

const {
  L10N,
} = require("devtools/client/memory/utils");

var models = require("devtools/client/memory/models");

var Immutable = require("devtools/client/shared/vendor/immutable");
var React = require("devtools/client/shared/vendor/react");
var ReactDOM = require("devtools/client/shared/vendor/react-dom");
var Heap = React.createFactory(require("devtools/client/memory/components/heap"));
var CensusTreeItem = React.createFactory(require("devtools/client/memory/components/census-tree-item"));
var DominatorTreeComponent = React.createFactory(require("devtools/client/memory/components/dominator-tree"));
var DominatorTreeItem = React.createFactory(require("devtools/client/memory/components/dominator-tree-item"));
var ShortestPaths = React.createFactory(require("devtools/client/memory/components/shortest-paths"));
var TreeMap = React.createFactory(require("devtools/client/memory/components/tree-map"));
var SnapshotListItem = React.createFactory(require("devtools/client/memory/components/snapshot-list-item"));
var List = React.createFactory(require("devtools/client/memory/components/list"));
var Toolbar = React.createFactory(require("devtools/client/memory/components/toolbar"));

// All tests are asynchronous.
SimpleTest.waitForExplicitFinish();

var noop = () => {};

var TEST_CENSUS_TREE_ITEM_PROPS = Object.freeze({
  item: Object.freeze({
    bytes: 10,
    count: 1,
    totalBytes: 10,
    totalCount: 1,
    name: "foo",
    children: [
      Object.freeze({
        bytes: 10,
        count: 1,
        totalBytes: 10,
        totalCount: 1,
        name: "bar",
      })
    ]
  }),
  depth: 0,
  arrow: ">",
  focused: true,
  getPercentBytes: () => 50,
  getPercentCount: () => 50,
  showSign: false,
  onViewSourceInDebugger: noop,
  inverted: false,
});

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
  display: labelDisplays.coarseType,
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

var TEST_SHORTEST_PATHS_PROPS = Object.freeze({
  graph: Object.freeze({
    nodes: [
      { id: 1, label: ["other", "SomeType"] },
      { id: 2, label: ["other", "SomeType"] },
      { id: 3, label: ["other", "SomeType"] },
    ],
    edges: [
      { from: 1, to: 2, name: "1->2" },
      { from: 1, to: 3, name: "1->3" },
      { from: 2, to: 3, name: "2->3" },
    ],
  }),
});

var TEST_SNAPSHOT = Object.freeze({
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
    display: Object.freeze({
      displayName: "Test Display",
      tooltip: "Test display tooltup",
      inverted: false,
      breakdown: Object.freeze({
        by: "coarseType",
        objects: Object.freeze({ by: "count", count: true, bytes: true }),
        scripts: Object.freeze({ by: "count", count: true, bytes: true }),
        strings: Object.freeze({ by: "count", count: true, bytes: true }),
        other: Object.freeze({ by: "count", count: true, bytes: true }),
      }),
    }),
    state: censusState.SAVED,
    inverted: false,
    filter: null,
    expanded: new Set(),
    focused: null,
    parentMap: Object.freeze(Object.create(null))
  }),
  dominatorTree: TEST_DOMINATOR_TREE,
  error: null,
  imported: false,
  creationTime: 0,
  state: snapshotState.READ,
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
  view: { state: viewState.CENSUS, },
  snapshot: TEST_SNAPSHOT,
  sizes: Object.freeze({ shortestPathsSize: .5 }),
  onShortestPathsResize: noop,
});

var TEST_TOOLBAR_PROPS = Object.freeze({
  censusDisplays: [
    censusDisplays.coarseType,
    censusDisplays.allocationStack,
    censusDisplays.invertedAllocationStack,
  ],
  censusDisplay: censusDisplays.coarseType,
  onTakeSnapshotClick: noop,
  onImportClick: noop,
  onCensusDisplayChange: noop,
  onToggleRecordAllocationStacks: noop,
  allocations: models.allocations,
  onToggleInverted: noop,
  inverted: false,
  filterString: null,
  setFilterString: noop,
  diffing: null,
  onToggleDiffing: noop,
  view: { state: viewState.CENSUS, },
  onViewChange: noop,
  labelDisplays: [
    labelDisplays.coarseType,
    labelDisplays.allocationStack,
  ],
  labelDisplay: labelDisplays.coarseType,
  onLabelDisplayChange: noop,
  snapshots: [],
});

function makeTestCensusNode() {
  return {
    name: "Function",
    bytes: 100,
    totalBytes: 100,
    count: 100,
    totalCount: 100,
    children: []
  };
}

var TEST_TREE_MAP_PROPS = Object.freeze({
  treeMap: Object.freeze({
    report: {
      name: null,
      bytes: 0,
      totalBytes: 400,
      count: 0,
      totalCount: 400,
      children: [
        {
          name: "objects",
          bytes: 0,
          totalBytes: 200,
          count: 0,
          totalCount: 200,
          children: [ makeTestCensusNode(), makeTestCensusNode() ]
        },
        {
          name: "other",
          bytes: 0,
          totalBytes: 200,
          count: 0,
          totalCount: 200,
          children: [ makeTestCensusNode(), makeTestCensusNode() ],
        }
      ]
    }
  })
});

var TEST_SNAPSHOT_LIST_ITEM_PROPS = Object.freeze({
  onClick: noop,
  onSave: noop,
  onDelete: noop,
  item: TEST_SNAPSHOT,
  index: 1234,
});

function onNextAnimationFrame(fn) {
  return () =>
    requestAnimationFrame(() =>
      requestAnimationFrame(fn));
}

/**
 * Render the provided ReactElement in the provided HTML container.
 * Returns a Promise that will resolve the rendered element as a React
 * component.
 */
function renderComponent(element, container) {
  return new Promise(resolve => {
    let component = ReactDOM.render(element, container,
      onNextAnimationFrame(() => {
        dumpn("Rendered = " + container.innerHTML);
        resolve(component);
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
