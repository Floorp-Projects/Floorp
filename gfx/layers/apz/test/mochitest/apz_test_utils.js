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
