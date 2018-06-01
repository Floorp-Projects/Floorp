/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from head.js */
"use strict";

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
 * @param {TestActorFront} testActor The current TestActorFront instance
 * @return a promise that resolves when the tests have run
 */
function runEditOuterHTMLTests(tests, inspector, testActor) {
  info("Running " + tests.length + " edit-outer-html tests");
  return (async function() {
    for (const step of tests) {
      await runEditOuterHTMLTest(step, inspector, testActor);
    }
  })();
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
 * @param {TestActorFront} testActor The current TestActorFront instance
 * opened
 */
async function runEditOuterHTMLTest(test, inspector, testActor) {
  info("Running an edit outerHTML test on '" + test.selector + "'");
  await selectNode(test.selector, inspector);

  const onUpdated = inspector.once("inspector-updated");

  info("Listen for reselectedonremoved and edit the outerHTML");
  const onReselected = inspector.markup.once("reselectedonremoved");
  await inspector.markup.updateNodeOuterHTML(inspector.selection.nodeFront,
                                             test.newHTML, test.oldHTML);
  await onReselected;

  // Typically selectedNode will === pageNode, but if a new element has been
  // injected in front of it, this will not be the case. If this happens.
  const selectedNodeFront = inspector.selection.nodeFront;
  const pageNodeFront = await inspector.walker.querySelector(
    inspector.walker.rootNode, test.selector);

  if (test.validate) {
    await test.validate({pageNodeFront, selectedNodeFront,
                         inspector, testActor});
  } else {
    is(pageNodeFront, selectedNodeFront,
       "Original node (grabbed by selector) is selected");
    const {outerHTML} = await testActor.getNodeInfo(test.selector);
    is(outerHTML, test.newHTML, "Outer HTML has been updated");
  }

  // Wait for the inspector to be fully updated to avoid causing errors by
  // abruptly closing hanging requests when the test ends
  await onUpdated;

  const closeTagLine = inspector.markup.getContainer(pageNodeFront).closeTagLine;
  if (closeTagLine) {
    is(closeTagLine.querySelectorAll(".theme-fg-contrast").length, 0,
       "No contrast class");
  }
}
