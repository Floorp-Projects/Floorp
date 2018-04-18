/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.importGlobalProperties(["NodeFilter"]);

const UNORDERED_TYPE = 8; // XPathResult.ANY_UNORDERED_NODE_TYPE

/**
 * Determine if the data node has only ignorable white-space.
 *
 * @return NodeFilter.FILTER_SKIP if it does.
 * @return NodeFilter.FILTER_ACCEPT otherwise.
 */
function isWhitespace(aNode) {
  return ((/\S/).test(aNode.nodeValue)) ?
         NodeFilter.FILTER_SKIP :
         NodeFilter.FILTER_ACCEPT;
}

/**
 * Create a DocumentFragment with cloned children equaling a node's children.
 *
 * @param aNode The node to copy from.
 *
 * @return DocumentFragment node.
 */
function getFragment(aNode) {
  var frag = aNode.ownerDocument.createDocumentFragment();
  for (var i = 0; i < aNode.childNodes.length; i++) {
    frag.appendChild(aNode.childNodes.item(i).cloneNode(true));
  }
  return frag;
}

// Goodies from head_content.js
const serializer = new DOMSerializer();
const parser = new DOMParser();

/**
 * Dump the contents of a document fragment to the console.
 *
 * @param aFragment The fragment to serialize.
 */
function dumpFragment(aFragment) {
  dump(serializer.serializeToString(aFragment) + "\n\n");
}

/**
 * Translate an XPath to a DOM node. This method uses a document
 * fragment as context node.
 *
 * @param aContextNode The context node to apply the XPath to.
 * @param aPath        The XPath to use.
 *
 * @return nsIDOMNode  The target node retrieved from the XPath.
 */
function evalXPathInDocumentFragment(aContextNode, aPath) {
  Assert.equal(ChromeUtils.getClassName(aContextNode), "DocumentFragment");
  Assert.ok(aContextNode.childNodes.length > 0);
  if (aPath == ".") {
    return aContextNode;
  }

  // Separate the fragment's xpath lookup from the rest.
  var firstSlash = aPath.indexOf("/");
  if (firstSlash == -1) {
    firstSlash = aPath.length;
  }
  var prefix = aPath.substr(0, firstSlash);
  var realPath = aPath.substr(firstSlash + 1);
  if (!realPath) {
    realPath = ".";
  }

  // Set up a special node filter to look among the fragment's child nodes.
  var childIndex = 1;
  var bracketIndex = prefix.indexOf("[");
  if (bracketIndex != -1) {
    childIndex = Number(prefix.substring(bracketIndex + 1, prefix.indexOf("]")));
    Assert.ok(childIndex > 0);
    prefix = prefix.substr(0, bracketIndex);
  }

  var targetType = NodeFilter.SHOW_ELEMENT;
  var targetNodeName = prefix;
  if (prefix.indexOf("processing-instruction(") == 0) {
    targetType = NodeFilter.SHOW_PROCESSING_INSTRUCTION;
    targetNodeName = prefix.substring(prefix.indexOf("(") + 2, prefix.indexOf(")") - 1);
  }
  switch (prefix) {
    case "text()":
      targetType = NodeFilter.SHOW_TEXT | NodeFilter.SHOW_CDATA_SECTION;
      targetNodeName = null;
      break;
    case "comment()":
      targetType = NodeFilter.SHOW_COMMENT;
      targetNodeName = null;
      break;
    case "node()":
      targetType = NodeFilter.SHOW_ALL;
      targetNodeName = null;
  }

  var filter = {
    count: 0,

    // NodeFilter
    acceptNode: function acceptNode(aNode) {
      if (aNode.parentNode != aContextNode) {
        // Don't bother looking at kids either.
        return NodeFilter.FILTER_REJECT;
      }

      if (targetNodeName && targetNodeName != aNode.nodeName) {
        return NodeFilter.FILTER_SKIP;
      }

      this.count++;
      if (this.count != childIndex) {
        return NodeFilter.FILTER_SKIP;
      }

      return NodeFilter.FILTER_ACCEPT;
    }
  };

  // Look for the node matching the step from the document fragment.
  var walker = aContextNode.ownerDocument.createTreeWalker(
                 aContextNode,
                 targetType,
                 filter);
  var targetNode = walker.nextNode();
  Assert.notEqual(targetNode, null);

  // Apply our remaining xpath to the found node.
  var expr = aContextNode.ownerDocument.createExpression(realPath, null);
  var result = expr.evaluate(targetNode, UNORDERED_TYPE, null);
  return result.singleNodeValue;
}

/**
 * Get a DOM range corresponding to the test's source node.
 *
 * @param aSourceNode <source/> element with range information.
 * @param aFragment   DocumentFragment generated with getFragment().
 *
 * @return Range object.
 */
function getRange(aSourceNode, aFragment) {
  Assert.ok(aSourceNode instanceof Ci.nsIDOMElement);
  Assert.equal(ChromeUtils.getClassName(aFragment), "DocumentFragment");
  var doc = aSourceNode.ownerDocument;

  var containerPath = aSourceNode.getAttribute("startContainer");
  var startContainer = evalXPathInDocumentFragment(aFragment, containerPath);
  var startOffset = Number(aSourceNode.getAttribute("startOffset"));

  containerPath = aSourceNode.getAttribute("endContainer");
  var endContainer = evalXPathInDocumentFragment(aFragment, containerPath);
  var endOffset = Number(aSourceNode.getAttribute("endOffset"));

  var range = doc.createRange();
  range.setStart(startContainer, startOffset);
  range.setEnd(endContainer, endOffset);
  return range;
}

/**
 * Get the document for a given path, and clean it up for our tests.
 *
 * @param aPath The path to the local document.
 */
function getParsedDocument(aPath) {
  return do_parse_document(aPath, "application/xml").then(processParsedDocument);
}

function processParsedDocument(doc) {
  Assert.ok(doc.documentElement.localName != "parsererror");
  Assert.ok(doc instanceof Ci.nsIDOMDocument);

  // Clean out whitespace.
  var walker = doc.createTreeWalker(doc,
                                    NodeFilter.SHOW_TEXT |
                                    NodeFilter.SHOW_CDATA_SECTION,
                                    isWhitespace);
  while (walker.nextNode()) {
    var parent = walker.currentNode.parentNode;
    parent.removeChild(walker.currentNode);
    walker.currentNode = parent;
  }

  // Clean out mandatory splits between nodes.
  var splits = doc.getElementsByTagName("split");
  var i;
  for (i = splits.length - 1; i >= 0; i--) {
    var node = splits.item(i);
    node.remove();
  }
  splits = null;

  // Replace empty CDATA sections.
  var emptyData = doc.getElementsByTagName("empty-cdata");
  for (i = emptyData.length - 1; i >= 0; i--) {
    var node = emptyData.item(i);
    var cdata = doc.createCDATASection("");
    node.parentNode.replaceChild(cdata, node);
  }

  return doc;
}

/**
 * Run the extraction tests.
 */
function run_extract_test() {
  var filePath = "test_delete_range.xml";
  getParsedDocument(filePath).then(do_extract_test);
}

function do_extract_test(doc) {
  var tests = doc.getElementsByTagName("test");

  // Run our deletion, extraction tests.
  for (var i = 0; i < tests.length; i++) {
    dump("Configuring for test " + i + "\n");
    var currentTest = tests.item(i);

    // Validate the test is properly formatted for what this harness expects.
    var baseSource = currentTest.firstChild;
    Assert.equal(baseSource.nodeName, "source");
    var baseResult = baseSource.nextSibling;
    Assert.equal(baseResult.nodeName, "result");
    var baseExtract = baseResult.nextSibling;
    Assert.equal(baseExtract.nodeName, "extract");
    Assert.equal(baseExtract.nextSibling, null);

    /* We do all our tests on DOM document fragments, derived from the test
       element's children.  This lets us rip the various fragments to shreds,
       while preserving the original elements so we can make more copies of
       them.

       After the range's extraction or deletion is done, we use
       nsIDOMNode.isEqualNode() between the altered source fragment and the
       result fragment.  We also run isEqualNode() between the extracted
       fragment and the fragment from the baseExtract node.  If they are not
       equal, we have failed a test.

       We also have to ensure the original nodes on the end points of the
       range are still in the source fragment.  This is bug 332148.  The nodes
       may not be replaced with equal but separate nodes.  The range extraction
       may alter these nodes - in the case of text containers, they will - but
       the nodes must stay there, to preserve references such as user data,
       event listeners, etc.

       First, an extraction test.
     */

    var resultFrag = getFragment(baseResult);
    var extractFrag = getFragment(baseExtract);

    dump("Extract contents test " + i + "\n\n");
    var baseFrag = getFragment(baseSource);
    var baseRange = getRange(baseSource, baseFrag);
    var startContainer = baseRange.startContainer;
    var endContainer = baseRange.endContainer;

    var cutFragment = baseRange.extractContents();
    dump("cutFragment: " + cutFragment + "\n");
    if (cutFragment) {
      Assert.ok(extractFrag.isEqualNode(cutFragment));
    } else {
      Assert.equal(extractFrag.firstChild, null);
    }
    Assert.ok(baseFrag.isEqualNode(resultFrag));

    dump("Ensure the original nodes weren't extracted - test " + i + "\n\n");
    var walker = doc.createTreeWalker(baseFrag,
                                      NodeFilter.SHOW_ALL,
                                      null);
    var foundStart = false;
    var foundEnd = false;
    do {
      if (walker.currentNode == startContainer) {
        foundStart = true;
      }

      if (walker.currentNode == endContainer) {
        // An end container node should not come before the start container node.
        Assert.ok(foundStart);
        foundEnd = true;
        break;
      }
    } while (walker.nextNode())
    Assert.ok(foundEnd);

    /* Now, we reset our test for the deleteContents case.  This one differs
       from the extractContents case only in that there is no extracted document
       fragment to compare against.  So we merely compare the starting fragment,
       minus the extracted content, against the result fragment.
     */
    dump("Delete contents test " + i + "\n\n");
    baseFrag = getFragment(baseSource);
    baseRange = getRange(baseSource, baseFrag);
    var startContainer = baseRange.startContainer;
    var endContainer = baseRange.endContainer;
    baseRange.deleteContents();
    Assert.ok(baseFrag.isEqualNode(resultFrag));

    dump("Ensure the original nodes weren't deleted - test " + i + "\n\n");
    walker = doc.createTreeWalker(baseFrag,
                                  NodeFilter.SHOW_ALL,
                                  null);
    foundStart = false;
    foundEnd = false;
    do {
      if (walker.currentNode == startContainer) {
        foundStart = true;
      }

      if (walker.currentNode == endContainer) {
        // An end container node should not come before the start container node.
        Assert.ok(foundStart);
        foundEnd = true;
        break;
      }
    } while (walker.nextNode())
    Assert.ok(foundEnd);

    // Clean up after ourselves.
    walker = null;
  }
}

/**
 * Miscellaneous tests not covered above.
 */
function run_miscellaneous_tests() {
  var filePath = "test_delete_range.xml";
  getParsedDocument(filePath).then(do_miscellaneous_tests);
}

function isText(node) {
  return node.nodeType == node.TEXT_NODE ||
         node.nodeType == node.CDATA_SECTION_NODE;
}

function do_miscellaneous_tests(doc) {
  var tests = doc.getElementsByTagName("test");

  // Let's try some invalid inputs to our DOM range and see what happens.
  var currentTest = tests.item(0);
  var baseSource = currentTest.firstChild;
  var baseResult = baseSource.nextSibling;
  var baseExtract = baseResult.nextSibling;

  var baseFrag = getFragment(baseSource);

  var baseRange = getRange(baseSource, baseFrag);
  var startContainer = baseRange.startContainer;
  var endContainer = baseRange.endContainer;
  var startOffset = baseRange.startOffset;
  var endOffset = baseRange.endOffset;

  // Text range manipulation.
  if ((endOffset > startOffset) &&
      (startContainer == endContainer) &&
      isText(startContainer)) {
    // Invalid start node
    try {
      baseRange.setStart(null, 0);
      do_throw("Should have thrown NOT_OBJECT_ERR!");
    } catch (e) {
      Assert.equal(e.constructor.name, "TypeError");
    }

    // Invalid start node
    try {
      baseRange.setStart({}, 0);
      do_throw("Should have thrown SecurityError!");
    } catch (e) {
      Assert.equal(e.constructor.name, "TypeError");
    }

    // Invalid index
    try {
      baseRange.setStart(startContainer, -1);
      do_throw("Should have thrown IndexSizeError!");
    } catch (e) {
      Assert.equal(e.name, "IndexSizeError");
    }
  
    // Invalid index
    var newOffset = isText(startContainer) ?
                      startContainer.nodeValue.length + 1 :
                      startContainer.childNodes.length + 1;
    try {
      baseRange.setStart(startContainer, newOffset);
      do_throw("Should have thrown IndexSizeError!");
    } catch (e) {
      Assert.equal(e.name, "IndexSizeError");
    }
  
    newOffset--;
    // Valid index
    baseRange.setStart(startContainer, newOffset);
    Assert.equal(baseRange.startContainer, baseRange.endContainer);
    Assert.equal(baseRange.startOffset, newOffset);
    Assert.ok(baseRange.collapsed);

    // Valid index
    baseRange.setEnd(startContainer, 0);
    Assert.equal(baseRange.startContainer, baseRange.endContainer);
    Assert.equal(baseRange.startOffset, 0);
    Assert.ok(baseRange.collapsed);
  } else {
    do_throw("The first test should be a text-only range test.  Test is invalid.")
  }

  /* See what happens when a range has a startContainer in one fragment, and an
     endContainer in another.  According to the DOM spec, section 2.4, the range
     should collapse to the new container and offset. */
  baseRange = getRange(baseSource, baseFrag);
  startContainer = baseRange.startContainer;
  var startOffset = baseRange.startOffset;
  endContainer = baseRange.endContainer;
  var endOffset = baseRange.endOffset;

  dump("External fragment test\n\n");

  var externalTest = tests.item(1);
  var externalSource = externalTest.firstChild;
  var externalFrag = getFragment(externalSource);
  var externalRange = getRange(externalSource, externalFrag);

  baseRange.setEnd(externalRange.endContainer, 0);
  Assert.equal(baseRange.startContainer, externalRange.endContainer);
  Assert.equal(baseRange.startOffset, 0);
  Assert.ok(baseRange.collapsed);

  /*
  // XXX ajvincent if rv == WRONG_DOCUMENT_ERR, return false?
  do_check_false(baseRange.isPointInRange(startContainer, startOffset));
  do_check_false(baseRange.isPointInRange(startContainer, startOffset + 1));
  do_check_false(baseRange.isPointInRange(endContainer, endOffset));
  */

  // Requested by smaug:  A range involving a comment as a document child.
  doc = parser.parseFromString("<!-- foo --><foo/>", "application/xml");
  Assert.ok(doc instanceof Ci.nsIDOMDocument);
  Assert.equal(doc.childNodes.length, 2);
  baseRange = doc.createRange();
  baseRange.setStart(doc.firstChild, 1);
  baseRange.setEnd(doc.firstChild, 2);
  var frag = baseRange.extractContents();
  Assert.equal(frag.childNodes.length, 1);
  Assert.ok(ChromeUtils.getClassName(frag.firstChild) == "Comment");
  Assert.equal(frag.firstChild.nodeType, frag.COMMENT_NODE);
  Assert.equal(frag.firstChild.nodeValue, "f");

  /* smaug also requested attribute tests.  Sadly, those are not yet supported
     in ranges - see https://bugzilla.mozilla.org/show_bug.cgi?id=302775.
   */
}

function run_test() {
  run_extract_test();
  run_miscellaneous_tests();
}
