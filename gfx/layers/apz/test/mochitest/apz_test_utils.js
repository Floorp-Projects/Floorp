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

// ----------------------------------------------------------------
// Utilities for reconstructing the structure of the APZC tree from
// 'parentScrollId' entries in the APZ test data.
// ----------------------------------------------------------------

// Create a node with scroll id 'id' in the APZC tree.
function makeNode(id) {
  return {scrollId: id, children: []};
}

// Find a node with scroll id 'id' in the APZC tree rooted at 'root'.
function findNode(root, id) {
  if (root.scrollId == id) {
    return root;
  }
  for (var i = 0; i < root.children.length; ++i) {
    var subtreeResult = findNode(root.children[i], id);
    if (subtreeResult != null) {
      return subtreeResult;
    }
  }
  return null;
}

// Add a child -> parent link to the APZC tree rooted at 'root'.
function addLink(root, child, parent) {
  var parentNode = findNode(root, parent);
  if (parentNode == null) {
    parentNode = makeNode(parent);
    root.children.push(parentNode);
  }
  parentNode.children.push(makeNode(child));
}

// Add a root node to the APZC tree. It will become a direct
// child of 'root'.
function addRoot(root, id) {
  root.children.push(makeNode(id));
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
  var root = makeNode(-1);
  for (var scrollId in paint) {
    if ("hasNoParentWithSameLayersId" in paint[scrollId]) {
      addRoot(root, scrollId);
    } else if ("parentScrollId" in paint[scrollId]) {
      addLink(root, scrollId, paint[scrollId]["parentScrollId"]);
    }
  }
  return root;
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
