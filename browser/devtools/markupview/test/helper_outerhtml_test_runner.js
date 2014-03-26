/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Run a series of edit-outer-html tests.
 * This function will iterate over the provided tests array and run each test.
 * Each test's goal is to provide a node (a selector) and a new outer-HTML to be
 * inserted in place of the current one for that node.
 * This test runner will wait for the mutation event to be fired and will check
 * a few things. Each test may also provide its own validate function to perform
 * assertions and verify that the new outer html is correct.
 * @param {Array} tests See runEditOuterHTMLTest for the structure
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * opened
 * @return a promise that resolves when the tests have run
 */
function runEditOuterHTMLTests(tests, inspector) {
  info("Running " + tests.length + " edit-outer-html tests");
  return Task.spawn(function* () {
    for (let step of TEST_DATA) {
      yield runEditOuterHTMLTest(step, inspector);
    }
  });
}

/**
 * Run a single edit-outer-html test.
 * See runEditOuterHTMLTests for a description.
 * @param {Object} test A test object should contain the following properties:
 *        - selector {String} a css selector targeting the node to edit
 *        - oldHTML {String}
 *        - newHTML {String}
 *        - validate {Function} will be executed when the edition test is done,
 *        after the new outer-html has been inserted. Should be used to verify
 *        the actual DOM, see if it corresponds to the newHTML string provided
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * opened
 */
function* runEditOuterHTMLTest(test, inspector) {
  info("Running an edit outerHTML test on '" + test.selector + "'");
  yield selectNode(test.selector, inspector);
  let oldNodeFront = inspector.selection.nodeFront;

  info("Listening for the markupmutation event");
  // This event fires once the outerHTML is set, with a target as the parent node and a type of "childList".
  let mutated = inspector.once("markupmutation");
  info("Editing the outerHTML");
  inspector.markup.updateNodeOuterHTML(inspector.selection.nodeFront, test.newHTML, test.oldHTML);
  let mutations = yield mutated;
  ok(true, "The markupmutation event has fired, mutation done");

  info("Check to make the sure the correct mutation event was fired, and that the parent is selected");
  let nodeFront = inspector.selection.nodeFront;
  let mutation = mutations[0];
  let isFromOuterHTML = mutation.removed.some(n => n === oldNodeFront);

  ok(isFromOuterHTML, "The node is in the 'removed' list of the mutation");
  is(mutation.type, "childList", "Mutation is a childList after updating outerHTML");
  is(mutation.target, nodeFront, "Parent node is selected immediately after setting outerHTML");

  // Wait for node to be reselected after outerHTML has been set
  yield inspector.selection.once("new-node");

  // Typically selectedNode will === pageNode, but if a new element has been injected in front
  // of it, this will not be the case.  If this happens.
  let selectedNode = inspector.selection.node;
  let nodeFront = inspector.selection.nodeFront;
  let pageNode = getNode(test.selector);

  if (test.validate) {
    test.validate(pageNode, selectedNode);
  } else {
    is(pageNode, selectedNode, "Original node (grabbed by selector) is selected");
    is(pageNode.outerHTML, test.newHTML, "Outer HTML has been updated");
  }

  // Wait for the inspector to be fully updated to avoid causing errors by
  // abruptly closing hanging requests when the test ends
  yield inspector.once("inspector-updated");
}
