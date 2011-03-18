/**
 * @fileoverview 
 * Comparison functions used in the RTE test suite.
 *
 * Copyright 2010 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the 'License')
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an 'AS IS' BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @version 0.1
 * @author rolandsteiner@google.com
 */

/**
 * constants used only in the compare functions. 
 */
var RESULT_DIFF  = 0;  // actual result doesn't match expectation
var RESULT_SEL   = 1;  // actual result matches expectation in HTML only
var RESULT_EQUAL = 2;  // actual result matches expectation in both HTML and selection

/**
 * Gets the test expectations as an array from the passed-in field.
 *
 * @param {Array|String} the test expectation(s) as string or array.
 * @return {Array} test expectations as an array.
 */
function getExpectationArray(expected) {
  if (expected === undefined) {
    return [];
  }
  if (expected === null) {
    return [null];
  }
  switch (typeof expected) {
    case 'string':
    case 'boolean':
    case 'number':
      return [expected];
  }
  // Assume it's already an array.
  return expected;
}

/**
 * Compare a test result to a single expectation string.
 *
 * FIXME: add support for optional elements/attributes.
 *
 * @param expected {String} the already canonicalized (with the exception of selection marks) expectation string
 * @param actual {String} the already canonicalized (with the exception of selection marks) actual result
 * @return {Integer} one of the RESULT_... return values
 * @see variables.js for return values
 */
function compareHTMLToSingleExpectation(expected, actual) {
  // If the test checks the selection, then the actual string must match the
  // expectation exactly.
  if (expected == actual) {
    return RESULT_EQUAL;
  }

  // Remove selection markers and see if strings match then.
  expected = expected.replace(/ [{}\|]>/g, '>');     // intra-tag
  expected = expected.replace(/[\[\]\^{}\|]/g, '');  // outside tag
  actual = actual.replace(/ [{}\|]>/g, '>');         // intra-tag
  actual = actual.replace(/[\[\]\^{}\|]/g, '');      // outside tag

  return (expected == actual) ? RESULT_SEL : RESULT_DIFF;
}

/**
 * Compare the current HTMLtest result to the expectation string(s).
 *
 * @param actual {String/Boolean} actual value
 * @param expected {String/Array} expectation(s)
 * @param emitFlags {Object} flags to use for canonicalization
 * @return {Integer} one of the RESULT_... return values
 * @see variables.js for return values
 */
function compareHTMLToExpectation(actual, expected, emitFlags) {
  // Find the most favorable result among the possible expectation strings.
  var expectedArr = getExpectationArray(expected);
  var count = expectedArr ? expectedArr.length : 0;
  var best = RESULT_DIFF;

  for (var idx = 0; idx < count && best < RESULT_EQUAL; ++idx) {
    var expected = expectedArr[idx];
    expected = canonicalizeSpaces(expected);
    expected = canonicalizeElementsAndAttributes(expected, emitFlags);

    var singleResult = compareHTMLToSingleExpectation(expected, actual);

    best = Math.max(best, singleResult);
  }
  return best;
}

/**
 * Compare the current HTMLtest result to expected and acceptable results
 *
 * @param expected {String/Array} expected result(s)
 * @param accepted {String/Array} accepted result(s)
 * @param actual {String} actual result
 * @param emitFlags {Object} how to canonicalize the HTML strings
 * @param result {Object} [out] object recieving the result of the comparison.
 */
function compareHTMLTestResultTo(expected, accepted, actual, emitFlags, result) {
  actual = actual.replace(/[\x60\xb4]/g, '');
  actual = canonicalizeElementsAndAttributes(actual, emitFlags);

  var bestExpected = compareHTMLToExpectation(actual, expected, emitFlags);

  if (bestExpected == RESULT_EQUAL) {
    // Shortcut - it doesn't get any better
    result.valresult = VALRESULT_EQUAL;
    result.selresult = SELRESULT_EQUAL;
    return;
  }

  var bestAccepted = compareHTMLToExpectation(actual, accepted, emitFlags);

  switch (bestExpected) {
    case RESULT_SEL:
      switch (bestAccepted) {
        case RESULT_EQUAL:
          // The HTML was equal to the/an expected HTML result as well
          // (just not the selection there), therefore the difference
          // between expected and accepted can only lie in the selection.
          result.valresult = VALRESULT_EQUAL;
          result.selresult = SELRESULT_ACCEPT;
          return;

        case RESULT_SEL:
        case RESULT_DIFF:
          // The acceptable expectations did not yield a better result
          // -> stay with the original (i.e., comparison to 'expected') result.
          result.valresult = VALRESULT_EQUAL;
          result.selresult = SELRESULT_DIFF;
          return;
      }
      break;

    case RESULT_DIFF:
      switch (bestAccepted) {
        case RESULT_EQUAL:
          result.valresult = VALRESULT_ACCEPT;
          result.selresult = SELRESULT_EQUAL;
          return;

        case RESULT_SEL:
          result.valresult = VALRESULT_ACCEPT;
          result.selresult = SELRESULT_DIFF;
          return;

        case RESULT_DIFF:
          result.valresult = VALRESULT_DIFF;
          result.selresult = SELRESULT_NA;
          return;
      }
      break;
  }
  
  throw INTERNAL_ERR + HTML_COMPARISON;
}

/**
 * Verify that the canaries are unviolated.
 *
 * @param container {Object} the test container descriptor as object reference
 * @param result {Object} object reference that contains the result data
 * @return {Boolean} whether the canaries' HTML is OK (selection flagged, but not fatal)
 */
function verifyCanaries(container, result) {
  if (!container.canary) {
    return true;
  }

  var str = canonicalizeElementsAndAttributes(result.bodyInnerHTML, emitFlagsForCanary);

  if (str.length < 2 * container.canary.length) {
    result.valresult = VALRESULT_CANARY;
    result.selresult = SELRESULT_NA;
    result.output = result.bodyOuterHTML;
    return false;
  }

  var strBefore = str.substr(0, container.canary.length);
  var strAfter  = str.substr(str.length - container.canary.length);

  // Verify that the canary stretch doesn't contain any selection markers
  if (SELECTION_MARKERS.test(strBefore) || SELECTION_MARKERS.test(strAfter)) {
    str = str.replace(SELECTION_MARKERS, '');
    if (str.length < 2 * container.canary.length) {
      result.valresult = VALRESULT_CANARY;
      result.selresult = SELRESULT_NA;
      result.output = result.bodyOuterHTML;
      return false;
    }

    // Selection escaped contentEditable element, but HTML may still be ok.
    result.selresult = SELRESULT_CANARY;
    strBefore = str.substr(0, container.canary.length);
    strAfter  = str.substr(str.length - container.canary.length);
  }

  if (strBefore !== container.canary || strAfter !== container.canary) {
    result.valresult = VALRESULT_CANARY;
    result.selresult = SELRESULT_NA;
    result.output = result.bodyOuterHTML;
    return false;
  }

  return true;
}

/**
 * Compare the current HTMLtest result to the expectation string(s).
 * Sets the global result variables.
 *
 * @param suite {Object} the test suite as object reference
 * @param group {Object} group of tests within the suite the test belongs to
 * @param test {Object} the test as object reference
 * @param container {Object} the test container description
 * @param result {Object} [in/out] the result description, incl. HTML strings
 * @see variables.js for result values
 */
function compareHTMLTestResult(suite, group, test, container, result) {
  if (!verifyCanaries(container, result)) {
    return;
  }

  var emitFlags = {
      emitAttrs:         getTestParameter(suite, group, test, PARAM_CHECK_ATTRIBUTES),
      emitStyle:         getTestParameter(suite, group, test, PARAM_CHECK_STYLE),
      emitClass:         getTestParameter(suite, group, test, PARAM_CHECK_CLASS),
      emitID:            getTestParameter(suite, group, test, PARAM_CHECK_ID),
      lowercase:         true,
      canonicalizeUnits: true
  };

  // 2a.) Compare opening tag - 
  //      decide whether to compare vs. outer or inner HTML based on this.
  var openingTagEnd = result.outerHTML.indexOf('>') + 1;
  var openingTag = result.outerHTML.substr(0, openingTagEnd);

  openingTag = canonicalizeElementsAndAttributes(openingTag, emitFlags);
  var tagCmp = compareHTMLToExpectation(openingTag, container.tagOpen, emitFlags);

  if (tagCmp == RESULT_EQUAL) {
    result.output = result.innerHTML;
    compareHTMLTestResultTo(
        getTestParameter(suite, group, test, PARAM_EXPECTED),
        getTestParameter(suite, group, test, PARAM_ACCEPT),
        result.innerHTML,
        emitFlags,
        result)
  } else {
    result.output = result.outerHTML;
    compareHTMLTestResultTo(
        getContainerParameter(suite, group, test, container, PARAM_EXPECTED_OUTER),
        getContainerParameter(suite, group, test, container, PARAM_ACCEPT_OUTER),
        result.outerHTML,
        emitFlags,
        result)
  }
}

/**
 * Insert a selection position indicator.
 *
 * @param node {DOMNode} the node where to insert the selection indicator
 * @param offs {Integer} the offset of the selection indicator
 * @param textInd {String}  the indicator to use if the node is a text node
 * @param elemInd {String}  the indicator to use if the node is an element node
 */
function insertSelectionIndicator(node, offs, textInd, elemInd) {
  switch (node.nodeType) {
    case DOM_NODE_TYPE_TEXT:
      // Insert selection marker for text node into text content.
      var text = node.data;
      node.data = text.substring(0, offs) + textInd + text.substring(offs);
      break;
      
    case DOM_NODE_TYPE_ELEMENT:
      var child = node.firstChild;
      try {
        // node has other children: insert marker as comment node
        var comment = document.createComment(elemInd);
        while (child && offs) {
          --offs;
          child = child.nextSibling;
        }
        if (child) {
          node.insertBefore(comment, child);
        } else {
          node.appendChild(comment);
        }
      } catch (ex) {
        // can't append child comment -> insert as special attribute(s)
        switch (elemInd) {
          case '|':
            node.setAttribute(ATTRNAME_SEL_START, '1');
            node.setAttribute(ATTRNAME_SEL_END, '1');
            break;

          case '{':
            node.setAttribute(ATTRNAME_SEL_START, '1');
            break;

          case '}':
            node.setAttribute(ATTRNAME_SEL_END, '1');
            break;
        }
      }
      break;
  }
}

/**
 * Adds quotes around all text nodes to show cases with non-normalized
 * text nodes. Those are not a bug, but may still be usefil in helping to
 * debug erroneous cases.
 *
 * @param node {DOMNode} root node from which to descend
 */
function encloseTextNodesWithQuotes(node) {
  switch (node.nodeType) {
    case DOM_NODE_TYPE_ELEMENT:
      for (var i = 0; i < node.childNodes.length; ++i) {
        encloseTextNodesWithQuotes(node.childNodes[i]);
      }
      break;
      
    case DOM_NODE_TYPE_TEXT:
      node.data = '\x60' + node.data + '\xb4';
      break;
  }
}

/**
 * Retrieve the result of a test run and do some preliminary canonicalization.
 *
 * @param container {Object} the container where to retrieve the result from as object reference
 * @param result {Object} object reference that contains the result data
 * @return {String} a preliminarily canonicalized innerHTML with selection markers
 */
function prepareHTMLTestResult(container, result) {
  // Start with empty strings in case any of the below throws.
  result.innerHTML = '';
  result.outerHTML = '';

  // 1.) insert selection markers
  var selRange = createFromWindow(container.win);
  if (selRange) {
    // save values, since range object gets auto-modified
    var node1 = selRange.getAnchorNode();
    var offs1 = selRange.getAnchorOffset();
    var node2 = selRange.getFocusNode();
    var offs2 = selRange.getFocusOffset();

    // add markers
    if (node1 && node1 == node2 && offs1 == offs2) {
      // collapsed selection
      insertSelectionIndicator(node1, offs1, '^', '|');
    } else {
      // Start point and end point are different
      if (node1) {
        insertSelectionIndicator(node1, offs1, '[', '{');
      }

      if (node2) {
        if (node1 == node2 && offs1 < offs2) {
          // Anchor indicator was inserted under the same node, so we need
          // to shift the offset by 1
          ++offs2;
        }
        insertSelectionIndicator(node2, offs2, ']', '}');
      }
    }
  }

  // 2.) insert markers for text node boundaries;
  encloseTextNodesWithQuotes(container.editor);
  
  // 3.) retrieve inner and outer HTML
  result.innerHTML = initialCanonicalizationOf(container.editor.innerHTML);
  result.bodyInnerHTML = initialCanonicalizationOf(container.body.innerHTML);
  if (goog.userAgent.IE) {
    result.outerHTML = initialCanonicalizationOf(container.editor.outerHTML);
    result.bodyOuterHTML = initialCanonicalizationOf(container.body.outerHTML);
    result.outerHTML = result.outerHTML.replace(/^\s+/, '');
    result.outerHTML = result.outerHTML.replace(/\s+$/, '');
    result.bodyOuterHTML = result.bodyOuterHTML.replace(/^\s+/, '');
    result.bodyOuterHTML = result.bodyOuterHTML.replace(/\s+$/, '');
  } else {
    result.outerHTML = initialCanonicalizationOf(new XMLSerializer().serializeToString(container.editor));
    result.bodyOuterHTML = initialCanonicalizationOf(new XMLSerializer().serializeToString(container.body));
  }
}

/**
 * Compare a text test result to the expectation string(s).
 *
 * @param suite {Object} the test suite as object reference
 * @param group {Object} group of tests within the suite the test belongs to
 * @param test {Object} the test as object reference
 * @param actual {String/Boolean} actual value
 * @param expected {String/Array} expectation(s)
 * @return {Boolean} whether we found a match
 */
function compareTextTestResultWith(suite, group, test, actual, expected) {
  var expectedArr = getExpectationArray(expected);
  // Find the most favorable result among the possible expectation strings.
  var count = expectedArr.length;

  // If the value matches the expectation exactly, then we're fine.  
  for (var idx = 0; idx < count; ++idx) {
    if (actual === expectedArr[idx])
      return true;
  }
  
  // Otherwise see if we should canonicalize specific value types.
  //
  // We only need to look at font name, color and size units if the originating
  // test was both a) queryCommandValue and b) querying a font name/color/size
  // specific criterion.
  //
  // TODO(rolandsteiner): This is ugly! Refactor!
  switch (getTestParameter(suite, group, test, PARAM_QUERYCOMMANDVALUE)) {
    case 'backcolor':
    case 'forecolor':
    case 'hilitecolor':
      for (var idx = 0; idx < count; ++idx) {
        if (new Color(actual).compare(new Color(expectedArr[idx])))
          return true;
      }
      return false;
    
    case 'fontname':
      for (var idx = 0; idx < count; ++idx) {
        if (new FontName(actual).compare(new FontName(expectedArr[idx])))
          return true;
      }
      return false;
    
    case 'fontsize':
      for (var idx = 0; idx < count; ++idx) {
        if (new FontSize(actual).compare(new FontSize(expectedArr[idx])))
          return true;
      }
      return false;
  }
  
  return false;
}

/**
 * Compare the passed-in text test result to the expectation string(s).
 * Sets the global result variables.
 *
 * @param suite {Object} the test suite as object reference
 * @param group {Object} group of tests within the suite the test belongs to
 * @param test {Object} the test as object reference
 * @param actual {String/Boolean} actual value
 * @return {Integer} a RESUTLHTML... result value
 * @see variables.js for result values
 */
function compareTextTestResult(suite, group, test, result) {
  var expected = getTestParameter(suite, group, test, PARAM_EXPECTED);
  if (compareTextTestResultWith(suite, group, test, result.output, expected)) {
    result.valresult = VALRESULT_EQUAL;
    return;
  }
  var accepted = getTestParameter(suite, group, test, PARAM_ACCEPT);
  if (accepted && compareTextTestResultWith(suite, group, test, result.output, accepted)) {
    result.valresult = VALRESULT_ACCEPT;
    return;
  }
  result.valresult = VALRESULT_DIFF;
}

