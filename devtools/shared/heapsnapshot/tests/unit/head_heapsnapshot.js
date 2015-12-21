/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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
const { Task } = Cu.import("resource://gre/modules/Task.jsm", {});

const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const HeapAnalysesClient =
  require("devtools/shared/heapsnapshot/HeapAnalysesClient");
const Services = require("Services");
const { censusReportToCensusTreeNode } = require("devtools/shared/heapsnapshot/census-tree-node");
const CensusUtils = require("devtools/shared/heapsnapshot/CensusUtils");
const DominatorTreeNode = require("devtools/shared/heapsnapshot/DominatorTreeNode");
const { LabelAndShallowSizeVisitor } = DominatorTreeNode;


// Always log packets when running tests. runxpcshelltests.py will throw
// the output away anyway, unless you give it the --verbose flag.
if (Services.appInfo &&
    Services.appInfo.processType == Services.appInfo.PROCESS_TYPE_DEFAULT) {
  Services.prefs.setBoolPref("devtools.debugger.log", true);
}
DevToolsUtils.dumpn.wantLogging = true;

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
    global.print = do_print;
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

function assertThrowsValue(f, val, msg) {
  var fullmsg;
  try {
    f();
  } catch (exc) {
    if ((exc === val) === (val === val) && (val !== 0 || 1 / exc === 1 / val))
      return;
    fullmsg = "Assertion failed: expected exception " + val + ", got " + exc;
  }
  if (fullmsg === undefined)
    fullmsg = "Assertion failed: expected exception " + val + ", no exception thrown";
  if (msg !== undefined)
    fullmsg += " - " + msg;
  throw new Error(fullmsg);
}

/**
 * Returns the full path of the file with the specified name in a
 * platform-independent and URL-like form.
 */
function getFilePath(aName, aAllowMissing=false, aUsePlatformPathSeparator=false)
{
  let file = do_get_file(aName, aAllowMissing);
  let path = Services.io.newFileURI(file).spec;
  let filePrePath = "file://";
  if ("nsILocalFileWin" in Ci &&
      file instanceof Ci.nsILocalFileWin) {
    filePrePath += "/";
  }

  path = path.slice(filePrePath.length);

  if (aUsePlatformPathSeparator && path.match(/^\w:/)) {
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
function saveHeapSnapshotAndTakeCensus(dbg=null, censusOptions=undefined) {
  const snapshotOptions = dbg ? { debugger: dbg } : { runtime: true };
  const filePath = saveNewHeapSnapshot(snapshotOptions);
  const snapshot = readHeapSnapshot(filePath);

  equal(typeof snapshot.takeCensus, "function", "snapshot should have a takeCensus method");

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
  } else {
    return val;
  }
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
function compareCensusViewData (breakdown, report, expected, options) {
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
function assertStructurallyEquivalent(actual, expected, path="root") {
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
           `${path}: every key in actual should exist in expected: ${String(key).slice(0, 10)}`);
        expectedKeys.delete(key);

        assertStructurallyEquivalent(actual.get(key), expected.get(key),
                                     path + ".get(" + String(key).slice(0, 20) + ")");
      }

      equal(expectedKeys.size, 0,
            `${path}: every key in expected should also exist in actual, did not see ${[...expectedKeys]}`);
    } else {
      const expectedKeys = new Set(Object.keys(expected));

      for (let key of Object.keys(actual)) {
        ok(expectedKeys.has(key),
           `${path}: every key in actual should exist in expected: ${key}`);
        expectedKeys.delete(key);

        assertStructurallyEquivalent(actual[key], expected[key], path + "." + key);
      }

      equal(expectedKeys.size, 0,
            `${path}: every key in expected should also exist in actual, did not see ${[...expectedKeys]}`);
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
function assertLabelAndShallowSize(breakdown, givenDescription, expectedShallowSize, expectedLabel) {
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
