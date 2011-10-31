/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is DOM Range tests.
 *
 * The Initial Developer of the Original Code is
 * Alexander J. Vincent <ajvincent@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const C_i = Components.interfaces;
const C_r = Components.results;

const UNORDERED_TYPE = C_i.nsIDOMXPathResult.ANY_UNORDERED_NODE_TYPE;

const INVALID_STATE_ERR = 0x8053000b;     // NS_ERROR_DOM_INVALID_STATE_ERR
const INDEX_SIZE_ERR = 0x80530001;        // NS_ERROR_DOM_INDEX_SIZE_ERR
const INVALID_NODE_TYPE_ERR = 0x805c0002; // NS_ERROR_DOM_INVALID_NODE_TYPE_ERR
const NOT_OBJECT_ERR = 0x805303eb;        // NS_ERROR_DOM_NOT_OBJECT_ERR
const SECURITY_ERR = 0x805303e8;          // NS_ERROR_DOM_SECURITY_ERR

/**
 * Determine if the data node has only ignorable white-space.
 *
 * @return nsIDOMNodeFilter.FILTER_SKIP if it does.
 * @return nsIDOMNodeFilter.FILTER_ACCEPT otherwise.
 */
function isWhitespace(aNode) {
  return ((/\S/).test(aNode.nodeValue)) ?
         C_i.nsIDOMNodeFilter.FILTER_SKIP :
         C_i.nsIDOMNodeFilter.FILTER_ACCEPT;
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
  do_check_true(aContextNode instanceof C_i.nsIDOMDocumentFragment);
  do_check_true(aContextNode.childNodes.length > 0);
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
    do_check_true(childIndex > 0);
    prefix = prefix.substr(0, bracketIndex);
  }

  var targetType = C_i.nsIDOMNodeFilter.SHOW_ELEMENT;
  var targetNodeName = prefix;
  if (prefix.indexOf("processing-instruction(") == 0) {
    targetType = C_i.nsIDOMNodeFilter.SHOW_PROCESSING_INSTRUCTION;
    targetNodeName = prefix.substring(prefix.indexOf("(") + 2, prefix.indexOf(")") - 1);
  }
  switch (prefix) {
    case "text()":
      targetType = C_i.nsIDOMNodeFilter.SHOW_TEXT |
                   C_i.nsIDOMNodeFilter.SHOW_CDATA_SECTION;
      targetNodeName = null;
      break;
    case "comment()":
      targetType = C_i.nsIDOMNodeFilter.SHOW_COMMENT;
      targetNodeName = null;
      break;
    case "node()":
      targetType = C_i.nsIDOMNodeFilter.SHOW_ALL;
      targetNodeName = null;
  }

  var filter = {
    count: 0,

    // nsIDOMNodeFilter
    acceptNode: function acceptNode(aNode) {
      if (aNode.parentNode != aContextNode) {
        // Don't bother looking at kids either.
        return C_i.nsIDOMNodeFilter.FILTER_REJECT;
      }

      if (targetNodeName && targetNodeName != aNode.nodeName) {
        return C_i.nsIDOMNodeFilter.FILTER_SKIP;
      }

      this.count++;
      if (this.count != childIndex) {
        return C_i.nsIDOMNodeFilter.FILTER_SKIP;
      }

      return C_i.nsIDOMNodeFilter.FILTER_ACCEPT;
    }
  };

  // Look for the node matching the step from the document fragment.
  var walker = aContextNode.ownerDocument.createTreeWalker(
                 aContextNode,
                 targetType,
                 filter,
                 true);
  var targetNode = walker.nextNode();
  do_check_neq(targetNode, null);

  // Apply our remaining xpath to the found node.
  var expr = aContextNode.ownerDocument.createExpression(realPath, null);
  var result = expr.evaluate(targetNode, UNORDERED_TYPE, null);
  do_check_true(result instanceof C_i.nsIDOMXPathResult);
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
  do_check_true(aSourceNode instanceof C_i.nsIDOMElement);
  do_check_true(aFragment instanceof C_i.nsIDOMDocumentFragment);
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
  var doc = do_parse_document(aPath, "application/xml");
  do_check_true(doc.documentElement.localName != "parsererror");
  do_check_true(doc instanceof C_i.nsIDOMXPathEvaluator);
  do_check_true(doc instanceof C_i.nsIDOMDocument);

  // Clean out whitespace.
  var walker = doc.createTreeWalker(doc,
                                    C_i.nsIDOMNodeFilter.SHOW_TEXT |
                                    C_i.nsIDOMNodeFilter.SHOW_CDATA_SECTION,
                                    isWhitespace,
                                    false);
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
    node.parentNode.removeChild(node);
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
  var doc = getParsedDocument(filePath);
  var tests = doc.getElementsByTagName("test");

  // Run our deletion, extraction tests.
  for (var i = 0; i < tests.length; i++) {
    dump("Configuring for test " + i + "\n");
    var currentTest = tests.item(i);

    // Validate the test is properly formatted for what this harness expects.
    var baseSource = currentTest.firstChild;
    do_check_eq(baseSource.nodeName, "source");
    var baseResult = baseSource.nextSibling;
    do_check_eq(baseResult.nodeName, "result");
    var baseExtract = baseResult.nextSibling;
    do_check_eq(baseExtract.nodeName, "extract");
    do_check_eq(baseExtract.nextSibling, null);

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
      do_check_true(extractFrag.isEqualNode(cutFragment));
    } else {
      do_check_eq(extractFrag.firstChild, null);
    }
    do_check_true(baseFrag.isEqualNode(resultFrag));

    dump("Ensure the original nodes weren't extracted - test " + i + "\n\n");
    var walker = doc.createTreeWalker(baseFrag,
				      C_i.nsIDOMNodeFilter.SHOW_ALL,
				      null,
				      false);
    var foundStart = false;
    var foundEnd = false;
    do {
      if (walker.currentNode == startContainer) {
        foundStart = true;
      }

      if (walker.currentNode == endContainer) {
        // An end container node should not come before the start container node.
        do_check_true(foundStart);
        foundEnd = true;
        break;
      }
    } while (walker.nextNode())
    do_check_true(foundEnd);

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
    do_check_true(baseFrag.isEqualNode(resultFrag));

    dump("Ensure the original nodes weren't deleted - test " + i + "\n\n");
    walker = doc.createTreeWalker(baseFrag,
                                  C_i.nsIDOMNodeFilter.SHOW_ALL,
                                  null,
                                  false);
    foundStart = false;
    foundEnd = false;
    do {
      if (walker.currentNode == startContainer) {
        foundStart = true;
      }

      if (walker.currentNode == endContainer) {
        // An end container node should not come before the start container node.
        do_check_true(foundStart);
        foundEnd = true;
        break;
      }
    } while (walker.nextNode())
    do_check_true(foundEnd);

    // Clean up after ourselves.
    walker = null;

    /* Whenever a DOM range is detached, we cannot use any methods on
       it - including extracting its contents or deleting its contents.  It
       should throw a NS_ERROR_DOM_INVALID_STATE_ERR exception.
    */
    dump("Detached range test\n");
    var compareFrag = getFragment(baseSource);
    baseFrag = getFragment(baseSource);
    baseRange = getRange(baseSource, baseFrag);
    baseRange.detach();
    try {
      var cutFragment = baseRange.extractContents();
      do_throw("Should have thrown INVALID_STATE_ERR!");
    } catch (e if (e instanceof C_i.nsIException &&
                   e.result == INVALID_STATE_ERR)) {
      // do nothing
    }
    do_check_true(compareFrag.isEqualNode(baseFrag));

    try {
      baseRange.deleteContents();
      do_throw("Should have thrown INVALID_STATE_ERR!");
    } catch (e if (e instanceof C_i.nsIException &&
                   e.result == INVALID_STATE_ERR)) {
      // do nothing
    }
    do_check_true(compareFrag.isEqualNode(baseFrag));
  }
}

/**
 * Miscellaneous tests not covered above.
 */
function run_miscellaneous_tests() {
  var filePath = "test_delete_range.xml";
  var doc = getParsedDocument(filePath);
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
      (startContainer instanceof C_i.nsIDOMText)) {
    // Invalid start node
    try {
      baseRange.setStart(null, 0);
      do_throw("Should have thrown NOT_OBJECT_ERR!");
    } catch (e if (e instanceof C_i.nsIException &&
                   e.result == NOT_OBJECT_ERR)) {
      // do nothing
    }

    // Invalid start node
    try {
      baseRange.setStart({}, 0);
      do_throw("Should have thrown SECURITY_ERR!");
    } catch (e if (e instanceof C_i.nsIException &&
                   e.result == SECURITY_ERR)) {
      // do nothing
    }

    // Invalid index
    try {
      baseRange.setStart(startContainer, -1);
      do_throw("Should have thrown INVALID_STATE_ERR!");
    } catch (e if (e instanceof C_i.nsIException &&
                   e.result == INDEX_SIZE_ERR)) {
      // do nothing
    }
  
    // Invalid index
    var newOffset = startContainer instanceof C_i.nsIDOMText ?
                      startContainer.nodeValue.length + 1 :
                      startContainer.childNodes.length + 1;
    try {
      baseRange.setStart(startContainer, newOffset);
      do_throw("Should have thrown INVALID_STATE_ERR!");
    } catch (e if (e instanceof C_i.nsIException &&
                   e.result == INDEX_SIZE_ERR)) {
      // do nothing
    }
  
    newOffset--;
    // Valid index
    baseRange.setStart(startContainer, newOffset);
    do_check_eq(baseRange.startContainer, baseRange.endContainer);
    do_check_eq(baseRange.startOffset, newOffset);
    do_check_true(baseRange.collapsed);

    // Valid index
    baseRange.setEnd(startContainer, 0);
    do_check_eq(baseRange.startContainer, baseRange.endContainer);
    do_check_eq(baseRange.startOffset, 0);
    do_check_true(baseRange.collapsed);
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
  do_check_eq(baseRange.startContainer, externalRange.endContainer);
  do_check_eq(baseRange.startOffset, 0);
  do_check_true(baseRange.collapsed);

  /*
  // XXX ajvincent if rv == WRONG_DOCUMENT_ERR, return false?
  do_check_true(baseRange instanceof C_i.nsIDOMNSRange);
  do_check_false(baseRange.isPointInRange(startContainer, startOffset));
  do_check_false(baseRange.isPointInRange(startContainer, startOffset + 1));
  do_check_false(baseRange.isPointInRange(endContainer, endOffset));
  */

  // Requested by smaug:  A range involving a comment as a document child.
  doc = parser.parseFromString("<!-- foo --><foo/>", "application/xml");
  do_check_true(doc instanceof C_i.nsIDOMDocument);
  do_check_eq(doc.childNodes.length, 2);
  baseRange = doc.createRange();
  baseRange.setStart(doc.firstChild, 1);
  baseRange.setEnd(doc.firstChild, 2);
  var frag = baseRange.extractContents();
  do_check_eq(frag.childNodes.length, 1);
  do_check_true(frag.firstChild instanceof C_i.nsIDOMComment);
  do_check_eq(frag.firstChild.nodeValue, "f");

  /* smaug also requested attribute tests.  Sadly, those are not yet supported
     in ranges - see https://bugzilla.mozilla.org/show_bug.cgi?id=302775.
   */
}

function run_test() {
  run_extract_test();
  run_miscellaneous_tests();
}
