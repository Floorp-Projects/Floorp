/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
/* exported Cr, CC, Match, Census, Task, DevToolsUtils, HeapAnalysesClient,
  assertThrows, getFilePath, saveHeapSnapshotAndTakeCensus,
  saveHeapSnapshotAndComputeDominatorTree, compareCensusViewData, assertDiff,
  assertLabelAndShallowSize, makeTestDominatorTreeNode,
  assertDominatorTreeNodeInsertion, assertDeduplicatedPaths,
  assertCountToBucketBreakdown, pathEntry */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;
var CC = Components.Constructor;

const { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
const { Match } = Cu.import("resource://test/Match.jsm", {});
const { Census } = Cu.import("resource://test/Census.jsm", {});
const { addDebuggerToGlobal } =
  Cu.import("resource://gre/modules/jsdebugger.jsm", {});

const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const flags = require("devtools/shared/flags");
const HeapAnalysesClient =
  require("devtools/shared/heapsnapshot/HeapAnalysesClient");
const Services = require("Services");
const { censusReportToCensusTreeNode } = require("devtools/shared/heapsnapshot/census-tree-node");
const CensusUtils = require("devtools/shared/heapsnapshot/CensusUtils");
const DominatorTreeNode = require("devtools/shared/heapsnapshot/DominatorTreeNode");
const { deduplicatePaths } = require("devtools/shared/heapsnapshot/shortest-paths");
const { LabelAndShallowSizeVisitor } = DominatorTreeNode;

// Always log packets when running tests. runxpcshelltests.py will throw
// the output away anyway, unless you give it the --verbose flag.
if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_DEFAULT) {
  Services.prefs.setBoolPref("devtools.debugger.log", true);
}
flags.wantLogging = true;

const SYSTEM_PRINCIPAL = Cc["@mozilla.org/systemprincipal;1"]
  .createInstance(Ci.nsIPrincipal);

function dumpn(msg) {
  dump("HEAPSNAPSHOT-TEST: " + msg + "\n");
}

function addTestingFunctionsToGlobal(global) {
  global.eval(
    `
    const testingFunctions = Components.utils.getJSTestingFunctions();
    for (let k in testingFunctions) {
      this[k] = testingFunctions[k];
    }
    `
  );
  if (!global.print) {
    global.print = info;
  }
  if (!global.newGlobal) {
    global.newGlobal = newGlobal;
  }
  if (!global.Debugger) {
    addDebuggerToGlobal(global);
  }
}

addTestingFunctionsToGlobal(this);

/**
 * Create a new global, with all the JS shell testing functions. Similar to the
 * newGlobal function exposed to JS shells, and useful for porting JS shell
 * tests to xpcshell tests.
 */
function newGlobal() {
  const global = new Cu.Sandbox(SYSTEM_PRINCIPAL, { freshZone: true });
  addTestingFunctionsToGlobal(global);
  return global;
}

function assertThrows(f, val, msg) {
  let fullmsg;
  try {
    f();
  } catch (exc) {
    if ((exc === val) && (val !== 0 || 1 / exc === 1 / val)) {
      return;
    } else if (exc instanceof Error && exc.message === val) {
      return;
    }
    fullmsg = "Assertion failed: expected exception " + val + ", got " + exc;
  }
  if (fullmsg === undefined) {
    fullmsg = "Assertion failed: expected exception " + val + ", no exception thrown";
  }
  if (msg !== undefined) {
    fullmsg += " - " + msg;
  }
  throw new Error(fullmsg);
}

/**
 * Returns the full path of the file with the specified name in a
 * platform-independent and URL-like form.
 */
function getFilePath(name, allowMissing = false, usePlatformPathSeparator = false) {
  let file = do_get_file(name, allowMissing);
  let path = Services.io.newFileURI(file).spec;
  let filePrePath = "file://";
  if ("nsILocalFileWin" in Ci &&
      file instanceof Ci.nsILocalFileWin) {
    filePrePath += "/";
  }

  path = path.slice(filePrePath.length);

  if (sePlatformPathSeparator && path.match(/^\w:/)) {
    path = path.replace(/\//g, "\\");
  }

  return path;
}

function saveNewHeapSnapshot(opts = { runtime: true }) {
  const filePath = ChromeUtils.saveHeapSnapshot(opts);
  ok(filePath, "Should get a file path to save the core dump to.");
  ok(true, "Saved a heap snapshot to " + filePath);
  return filePath;
}

function readHeapSnapshot(filePath) {
  const snapshot = ChromeUtils.readHeapSnapshot(filePath);
  ok(snapshot, "Should have read a heap snapshot back from " + filePath);
  ok(snapshot instanceof HeapSnapshot, "snapshot should be an instance of HeapSnapshot");
  return snapshot;
}

/**
 * Save a heap snapshot to the file with the given name in the current
 * directory, read it back as a HeapSnapshot instance, and then take a census of
 * the heap snapshot's serialized heap graph with the provided census options.
 *
 * @param {Object|undefined} censusOptions
 *        Options that should be passed through to the takeCensus method. See
 *        js/src/doc/Debugger/Debugger.Memory.md for details.
 *
 * @param {Debugger|null} dbg
 *        If a Debugger object is given, only serialize the subgraph covered by
 *        the Debugger's debuggees. If null, serialize the whole heap graph.
 *
 * @param {String} fileName
 *        The file name to save the heap snapshot's core dump file to, within
 *        the current directory.
 *
 * @returns Census
 */
function saveHeapSnapshotAndTakeCensus(dbg = null, censusOptions = undefined) {
  const snapshotOptions = dbg ? { debugger: dbg } : { runtime: true };
  const filePath = saveNewHeapSnapshot(snapshotOptions);
  const snapshot = readHeapSnapshot(filePath);

  equal(typeof snapshot.takeCensus, "function",
    "snapshot should have a takeCensus method");

  return snapshot.takeCensus(censusOptions);
}

/**
 * Save a heap snapshot to disk, read it back as a HeapSnapshot instance, and
 * then compute its dominator tree.
 *
 * @param {Debugger|null} dbg
 *        If a Debugger object is given, only serialize the subgraph covered by
 *        the Debugger's debuggees. If null, serialize the whole heap graph.
 *
 * @returns {DominatorTree}
 */
function saveHeapSnapshotAndComputeDominatorTree(dbg = null) {
  const snapshotOptions = dbg ? { debugger: dbg } : { runtime: true };
  const filePath = saveNewHeapSnapshot(snapshotOptions);
  const snapshot = readHeapSnapshot(filePath);

  equal(typeof snapshot.computeDominatorTree, "function",
        "snapshot should have a `computeDominatorTree` method");

  const dominatorTree = snapshot.computeDominatorTree();

  ok(dominatorTree, "Should be able to compute a dominator tree");
  ok(dominatorTree instanceof DominatorTree, "Should be an instance of DominatorTree");

  return dominatorTree;
}

function isSavedFrame(obj) {
  return Object.prototype.toString.call(obj) === "[object SavedFrame]";
}

function savedFrameReplacer(key, val) {
  if (isSavedFrame(val)) {
    return `<SavedFrame '${val.toString().split(/\n/g).shift()}'>`;
  }
  return val;
}

/**
 * Assert that creating a CensusTreeNode from the given `report` with the
 * specified `breakdown` creates the given `expected` CensusTreeNode.
 *
 * @param {Object} breakdown
 *        The census breakdown.
 *
 * @param {Object} report
 *        The census report.
 *
 * @param {Object} expected
 *        The expected CensusTreeNode result.
 *
 * @param {Object} options
 *        The options to pass through to `censusReportToCensusTreeNode`.
 */
function compareCensusViewData(breakdown, report, expected, options) {
  dumpn("Generating CensusTreeNode from report:");
  dumpn("breakdown: " + JSON.stringify(breakdown, null, 4));
  dumpn("report: " + JSON.stringify(report, null, 4));
  dumpn("expected: " + JSON.stringify(expected, savedFrameReplacer, 4));

  const actual = censusReportToCensusTreeNode(breakdown, report, options);
  dumpn("actual: " + JSON.stringify(actual, savedFrameReplacer, 4));

  assertStructurallyEquivalent(actual, expected);
}

// Deep structural equivalence that can handle Map objects in addition to plain
// objects.
function assertStructurallyEquivalent(actual, expected, path = "root") {
  if (actual === expected) {
    equal(actual, expected, "actual and expected are the same");
    return;
  }

  equal(typeof actual, typeof expected, `${path}: typeof should be the same`);

  if (actual && typeof actual === "object") {
    const actualProtoString = Object.prototype.toString.call(actual);
    const expectedProtoString = Object.prototype.toString.call(expected);
    equal(actualProtoString, expectedProtoString,
          `${path}: Object.prototype.toString.call() should be the same`);

    if (actualProtoString === "[object Map]") {
      const expectedKeys = new Set([...expected.keys()]);

      for (let key of actual.keys()) {
        ok(expectedKeys.has(key),
          `${path}: every key in actual is expected: ${String(key).slice(0, 10)}`);
        expectedKeys.delete(key);

        assertStructurallyEquivalent(actual.get(key), expected.get(key),
                                     path + ".get(" + String(key).slice(0, 20) + ")");
      }

      equal(expectedKeys.size, 0,
        `${path}: every key in expected should also exist in actual,\
        did not see ${[...expectedKeys]}`);
    } else if (actualProtoString === "[object Set]") {
      const expectedItems = new Set([...expected]);

      for (let item of actual) {
        ok(expectedItems.has(item),
           `${path}: every set item in actual should exist in expected: ${item}`);
        expectedItems.delete(item);
      }

      equal(expectedItems.size, 0,
        `${path}: every set item in expected should also exist in actual,\
        did not see ${[...expectedItems]}`);
    } else {
      const expectedKeys = new Set(Object.keys(expected));

      for (let key of Object.keys(actual)) {
        ok(expectedKeys.has(key),
           `${path}: every key in actual should exist in expected: ${key}`);
        expectedKeys.delete(key);

        assertStructurallyEquivalent(actual[key], expected[key], path + "." + key);
      }

      equal(expectedKeys.size, 0,
        `${path}: every key in expected should also exist in actual,\
        did not see ${[...expectedKeys]}`);
    }
  } else {
    equal(actual, expected, `${path}: primitives should be equal`);
  }
}

/**
 * Assert that creating a diff of the `first` and `second` census reports
 * creates the `expected` delta-report.
 *
 * @param {Object} breakdown
 *        The census breakdown.
 *
 * @param {Object} first
 *        The first census report.
 *
 * @param {Object} second
 *        The second census report.
 *
 * @param {Object} expected
 *        The expected delta-report.
 */
function assertDiff(breakdown, first, second, expected) {
  dumpn("Diffing census reports:");
  dumpn("Breakdown: " + JSON.stringify(breakdown, null, 4));
  dumpn("First census report: " + JSON.stringify(first, null, 4));
  dumpn("Second census report: " + JSON.stringify(second, null, 4));
  dumpn("Expected delta-report: " + JSON.stringify(expected, null, 4));

  const actual = CensusUtils.diff(breakdown, first, second);
  dumpn("Actual delta-report: " + JSON.stringify(actual, null, 4));

  assertStructurallyEquivalent(actual, expected);
}

/**
 * Assert that creating a label and getting a shallow size from the given node
 * description with the specified breakdown is as expected.
 *
 * @param {Object} breakdown
 * @param {Object} givenDescription
 * @param {Number} expectedShallowSize
 * @param {Object} expectedLabel
 */
function assertLabelAndShallowSize(breakdown, givenDescription,
  expectedShallowSize, expectedLabel) {
  dumpn("Computing label and shallow size from node description:");
  dumpn("Breakdown: " + JSON.stringify(breakdown, null, 4));
  dumpn("Given description: " + JSON.stringify(givenDescription, null, 4));

  const visitor = new LabelAndShallowSizeVisitor();
  CensusUtils.walk(breakdown, description, visitor);

  dumpn("Expected shallow size: " + expectedShallowSize);
  dumpn("Actual shallow size: " + visitor.shallowSize());
  equal(visitor.shallowSize(), expectedShallowSize, "Shallow size should be correct");

  dumpn("Expected label: " + JSON.stringify(expectedLabel, null, 4));
  dumpn("Actual label: " + JSON.stringify(visitor.label(), null, 4));
  assertStructurallyEquivalent(visitor.label(), expectedLabel);
}

// Counter for mock DominatorTreeNode ids.
let TEST_NODE_ID_COUNTER = 0;

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
    label: undefined,
    shallowSize: 1,
    retainedSize: (children || []).reduce((size, c) => size + c.retainedSize, 1),
    parentId: undefined,
    children,
    moreChildrenAvailable: true,
  }, opts);

  if (children && children.length) {
    children.map(c => (c.parentId = node.nodeId));
  }

  return node;
}

/**
 * Insert `newChildren` into the given dominator `tree` as specified by the
 * `path` from the root to the node the `newChildren` should be inserted
 * beneath. Assert that the resulting tree matches `expected`.
 */
function assertDominatorTreeNodeInsertion(tree, path, newChildren,
  moreChildrenAvailable, expected) {
  dumpn("Inserting new children into a dominator tree:");
  dumpn("Dominator tree: " + JSON.stringify(tree, null, 2));
  dumpn("Path: " + JSON.stringify(path, null, 2));
  dumpn("New children: " + JSON.stringify(newChildren, null, 2));
  dumpn("Expected resulting tree: " + JSON.stringify(expected, null, 2));

  const actual = DominatorTreeNode.insert(tree, path, newChildren, moreChildrenAvailable);
  dumpn("Actual resulting tree: " + JSON.stringify(actual, null, 2));

  assertStructurallyEquivalent(actual, expected);
}

function assertDeduplicatedPaths({ target, paths, expectedNodes, expectedEdges }) {
  dumpn("Deduplicating paths:");
  dumpn("target = " + target);
  dumpn("paths = " + JSON.stringify(paths, null, 2));
  dumpn("expectedNodes = " + expectedNodes);
  dumpn("expectedEdges = " + JSON.stringify(expectedEdges, null, 2));

  const { nodes, edges } = deduplicatePaths(target, paths);

  dumpn("Actual nodes = " + nodes);
  dumpn("Actual edges = " + JSON.stringify(edges, null, 2));

  equal(nodes.length, expectedNodes.length,
        "actual number of nodes is equal to the expected number of nodes");

  equal(edges.length, expectedEdges.length,
        "actual number of edges is equal to the expected number of edges");

  const expectedNodeSet = new Set(expectedNodes);
  const nodeSet = new Set(nodes);
  ok(nodeSet.size === nodes.length,
     "each returned node should be unique");

  for (let node of nodes) {
    ok(expectedNodeSet.has(node), `the ${node} node was expected`);
  }

  for (let expectedEdge of expectedEdges) {
    let count = 0;
    for (let edge of edges) {
      if (edge.from === expectedEdge.from &&
          edge.to === expectedEdge.to &&
          edge.name === expectedEdge.name) {
        count++;
      }
    }
    equal(count, 1,
      "should have exactly one matching edge for the expected edge = "
      + JSON.stringify(edge));
  }
}

function assertCountToBucketBreakdown(breakdown, expected) {
  dumpn("count => bucket breakdown");
  dumpn("Initial breakdown = ", JSON.stringify(breakdown, null, 2));
  dumpn("Expected results = ", JSON.stringify(expected, null, 2));

  const actual = CensusUtils.countToBucketBreakdown(breakdown);
  dumpn("Actual results = ", JSON.stringify(actual, null, 2));

  assertStructurallyEquivalent(actual, expected);
}

/**
 * Create a mock path entry for the given predecessor and edge.
 */
function pathEntry(predecessor, edge) {
  return { predecessor, edge };
}
