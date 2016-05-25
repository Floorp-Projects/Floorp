// Utilities for writing APZ tests using the framework added in bug 961289

// ----------------------------------------------------------------------
// Functions that convert the APZ test data into a more usable form.
// Every place we have a WebIDL sequence whose elements are dictionaries
// with two elements, a key, and a value, we convert this into a JS
// object with a property for each key/value pair. (This is the structure
// we really want, but we can't express in directly in WebIDL.)
// ----------------------------------------------------------------------

function convertEntries(entries) {
  var result = {};
  for (var i = 0; i < entries.length; ++i) {
    result[entries[i].key] = entries[i].value;
  }
  return result;
}

function convertScrollFrameData(scrollFrames) {
  var result = {};
  for (var i = 0; i < scrollFrames.length; ++i) {
    result[scrollFrames[i].scrollId] = convertEntries(scrollFrames[i].entries);
  }
  return result;
}

function convertBuckets(buckets) {
  var result = {};
  for (var i = 0; i < buckets.length; ++i) {
    result[buckets[i].sequenceNumber] = convertScrollFrameData(buckets[i].scrollFrames);
  }
  return result;
}

function convertTestData(testData) {
  var result = {};
  result.paints = convertBuckets(testData.paints);
  result.repaintRequests = convertBuckets(testData.repaintRequests);
  return result;
}

// Given APZ test data for a single paint on the compositor side,
// reconstruct the APZC tree structure from the 'parentScrollId'
// entries that were logged. More specifically, the subset of the
// APZC tree structure corresponding to the layer subtree for the
// content process that triggered the paint, is reconstructed (as
// the APZ test data only contains information abot this subtree).
function buildApzcTree(paint) {
  // The APZC tree can potentially have multiple root nodes,
  // so we invent a node that is the parent of all roots.
  // This 'root' does not correspond to an APZC.
  var root = {scrollId: -1, children: []};
  for (var scrollId in paint) {
    paint[scrollId].children = [];
    paint[scrollId].scrollId = scrollId;
  }
  for (var scrollId in paint) {
    var parentNode = null;
    if ("hasNoParentWithSameLayersId" in paint[scrollId]) {
      parentNode = root;
    } else if ("parentScrollId" in paint[scrollId]) {
      parentNode = paint[paint[scrollId].parentScrollId];
    }
    parentNode.children.push(paint[scrollId]);
  }
  return root;
}

// Given an APZC tree produced by buildApzcTree, return the RCD node in
// the tree, or null if there was none.
function findRcdNode(apzcTree) {
  if (!!apzcTree.isRootContent) { // isRootContent will be undefined or "1"
    return apzcTree;
  }
  for (var i = 0; i < apzcTree.children.length; i++) {
    var rcd = findRcdNode(apzcTree.children[i]);
    if (rcd != null) {
      return rcd;
    }
  }
  return null;
}

function flushApzRepaints(aCallback, aWindow = window) {
  if (!aCallback) {
    throw "A callback must be provided!";
  }
  var repaintDone = function() {
    SpecialPowers.Services.obs.removeObserver(repaintDone, "apz-repaints-flushed", false);
    setTimeout(aCallback, 0);
  };
  SpecialPowers.Services.obs.addObserver(repaintDone, "apz-repaints-flushed", false);
  if (SpecialPowers.getDOMWindowUtils(aWindow).flushApzRepaints()) {
    dump("Flushed APZ repaints, waiting for callback...\n");
  } else {
    dump("Flushing APZ repaints was a no-op, triggering callback directly...\n");
    repaintDone();
  }
}

// Flush repaints, APZ pending repaints, and any repaints resulting from that
// flush. This is particularly useful if the test needs to reach some sort of
// "idle" state in terms of repaints. Usually just doing waitForAllPaints
// followed by flushApzRepaints is sufficient to flush all APZ state back to
// the main thread, but it can leave a paint scheduled which will get triggered
// at some later time. For tests that specifically test for painting at
// specific times, this method is the way to go. Even if in doubt, this is the
// preferred method as the extra step is "safe" and shouldn't interfere with
// most tests.
function waitForApzFlushedRepaints(aCallback) {
  // First flush the main-thread paints and send transactions to the APZ
  waitForAllPaints(function() {
    // Then flush the APZ to make sure any repaint requests have been sent
    // back to the main thread
    flushApzRepaints(function() {
      // Then flush the main-thread again to process the repaint requests.
      // Once this is done, we should be in a stable state with nothing
      // pending, so we can trigger the callback.
      waitForAllPaints(aCallback);
    });
  });
}

// This function takes a set of subtests to run one at a time in new top-level
// windows, and returns a Promise that is resolved once all the subtests are
// done running.
//
// The aSubtests array is an array of objects with the following keys:
//   file: required, the filename of the subtest.
//   prefs: optional, an array of arrays containing key-value prefs to set.
//   dp_suppression: optional, a boolean on whether or not to respect displayport
//                   suppression during the test.
// An example of an array is:
//   aSubtests = [
//     { 'file': 'test_file_name.html' },
//     { 'file': 'test_file_2.html', 'prefs': [['pref.name', true], ['other.pref', 1000]], 'dp_suppression': false }
//   ];
//
// Each subtest should call the subtestDone() function when it is done, to
// indicate that the window should be torn down and the next text should run.
// The subtestDone() function is injected into the subtest's window by this
// function prior to loading the subtest. For convenience, the |is| and |ok|
// functions provided by SimpleTest are also mapped into the subtest's window.
// For other things from the parent, the subtest can use window.opener.<whatever>
// to access objects.
function runSubtestsSeriallyInFreshWindows(aSubtests) {
  return new Promise(function(resolve, reject) {
    var testIndex = -1;
    var w = null;

    function advanceSubtestExecution() {
      var test = aSubtests[testIndex];
      if (w) {
        if (typeof test.dp_suppression != 'undefined') {
          // We modified the suppression when starting the test, so now undo that.
          SpecialPowers.getDOMWindowUtils(window).respectDisplayPortSuppression(!test.dp_suppression);
        }
        if (test.prefs) {
          // We pushed some prefs for this test, pop them, and re-invoke
          // advanceSubtestExecution() after that's been processed
          SpecialPowers.popPrefEnv(function() {
            w.close();
            w = null;
            advanceSubtestExecution();
          });
          return;
        }

        w.close();
      }

      testIndex++;
      if (testIndex >= aSubtests.length) {
        resolve();
        return;
      }

      test = aSubtests[testIndex];
      if (typeof test.dp_suppression != 'undefined') {
        // Normally during a test, the displayport will get suppressed during page
        // load, and unsuppressed at a non-deterministic time during the test. The
        // unsuppression can trigger a repaint which interferes with the test, so
        // to avoid that we can force the displayport to be unsuppressed for the
        // entire test which is more deterministic.
        SpecialPowers.getDOMWindowUtils(window).respectDisplayPortSuppression(test.dp_suppression);
      }

      function spawnTest(aFile) {
        w = window.open('', "_blank");
        w.subtestDone = advanceSubtestExecution;
        w.SimpleTest = SimpleTest;
        w.is = function(a, b, msg) { return is(a, b, aFile + " | " + msg); };
        w.ok = function(cond, name, diag) { return ok(cond, aFile + " | " + name, diag); };
        w.location = location.href.substring(0, location.href.lastIndexOf('/') + 1) + aFile;
        return w;
      }

      if (test.prefs) {
        // Got some prefs for this subtest, push them
        SpecialPowers.pushPrefEnv({"set": test.prefs}, function() {
          w = spawnTest(test.file);
        });
      } else {
        w = spawnTest(test.file);
      }
    }

    advanceSubtestExecution();
  });
}

function pushPrefs(prefs) {
  return SpecialPowers.pushPrefEnv({'set': prefs});
}

function waitUntilApzStable() {
  return new Promise(function(resolve, reject) {
    SimpleTest.waitForFocus(function() {
      waitForAllPaints(function() {
        flushApzRepaints(resolve);
      });
    }, window);
  });
}
