/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from head.js */
"use strict";

/**
 * Run a series of add-attributes tests.
 * This function will iterate over the provided tests array and run each test.
 * Each test's goal is to provide some text to be entered into the test node's
 * new-attribute field and check that the given attributes have been created.
 * After each test has run, the markup-view's undo command will be called and
 * the test runner will check if all the new attributes are gone.
 * @param {Array} tests See runAddAttributesTest for the structure
 * @param {DOMNode|String} nodeOrSelector The node or node selector
 * corresponding to an element on the current test page that has *no attributes*
 * when the test starts. It will be used to add and remove attributes.
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * opened
 * @param {TestActorFront} testActor The current TestActorFront instance.
 * @return a promise that resolves when the tests have run
 */
function runAddAttributesTests(tests, nodeOrSelector, inspector, testActor) {
  info("Running " + tests.length + " add-attributes tests");
  return (async function() {
    info("Selecting the test node");
    await selectNode("div", inspector);

    for (const test of tests) {
      await runAddAttributesTest(test, "div", inspector, testActor);
    }
  })();
}

/**
 * Run a single add-attribute test.
 * See runAddAttributesTests for a description.
 * @param {Object} test A test object should contain the following properties:
 *        - desc {String} a textual description for that test, to help when
 *        reading logs
 *        - text {String} the string to be inserted into the new attribute field
 *        - expectedAttributes {Object} a key/value pair object that will be
 *        used to check the attributes on the test element
 *        - validate {Function} optional extra function that will be called
 *        after the attributes have been added and which should be used to
 *        assert some more things this test runner might not be checking. The
 *        function will be called with the following arguments:
 *          - {DOMNode} The element being tested
 *          - {MarkupContainer} The corresponding container in the markup-view
 *          - {InspectorPanel} The instance of the InspectorPanel opened
 * @param {String} selector The node selector corresponding to the test element
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * @param {TestActorFront} testActor The current TestActorFront instance.
 * opened
 */
async function runAddAttributesTest(test, selector, inspector, testActor) {
  if (test.setUp) {
    test.setUp(inspector);
  }

  info("Starting add-attribute test: " + test.desc);
  await addNewAttributes(selector, test.text, inspector);

  info("Assert that the attribute(s) has/have been applied correctly");
  await assertAttributes(selector, test.expectedAttributes, testActor);

  if (test.validate) {
    const container = await getContainerForSelector(selector, inspector);
    test.validate(container, inspector);
  }

  info("Undo the change");
  await undoChange(inspector);

  info("Assert that the attribute(s) has/have been removed correctly");
  await assertAttributes(selector, {}, testActor);
  if (test.tearDown) {
    test.tearDown(inspector);
  }
}

/**
 * Run a series of edit-attributes tests.
 * This function will iterate over the provided tests array and run each test.
 * Each test's goal is to locate a given element on the current test page,
 * assert its current attributes, then provide the name of one of them and a
 * value to be set into it, and then check if the new attributes are correct.
 * After each test has run, the markup-view's undo and redo commands will be
 * called and the test runner will assert again that the attributes are correct.
 * @param {Array} tests See runEditAttributesTest for the structure
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * opened
 * @param {TestActorFront} testActor The current TestActorFront instance.
 * @return a promise that resolves when the tests have run
 */
function runEditAttributesTests(tests, inspector, testActor) {
  info("Running " + tests.length + " edit-attributes tests");
  return (async function() {
    info("Expanding all nodes in the markup-view");
    await inspector.markup.expandAll();

    for (const test of tests) {
      await runEditAttributesTest(test, inspector, testActor);
    }
  })();
}

/**
 * Run a single edit-attribute test.
 * See runEditAttributesTests for a description.
 * @param {Object} test A test object should contain the following properties:
 *        - desc {String} a textual description for that test, to help when
 *        reading logs
 *        - node {String} a css selector that will be used to select the node
 *        which will be tested during this iteration
 *        - originalAttributes {Object} a key/value pair object that will be
 *        used to check the attributes of the node before the test runs
 *        - name {String} the name of the attribute to focus the editor for
 *        - value {String} the new value to be typed in the focused editor
 *        - expectedAttributes {Object} a key/value pair object that will be
 *        used to check the attributes on the test element
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * @param {TestActorFront} testActor The current TestActorFront instance.
 * opened
 */
async function runEditAttributesTest(test, inspector, testActor) {
  info("Starting edit-attribute test: " + test.desc);

  info("Selecting the test node " + test.node);
  await selectNode(test.node, inspector);

  info("Asserting that the node has the right attributes to start with");
  await assertAttributes(test.node, test.originalAttributes, testActor);

  info("Editing attribute " + test.name + " with value " + test.value);

  const container = await focusNode(test.node, inspector);
  ok(container && container.editor, "The markup-container for " + test.node +
    " was found");

  info("Listening for the markupmutation event");
  const nodeMutated = inspector.once("markupmutation");
  const attr = container.editor.attrElements.get(test.name)
                                          .querySelector(".editable");
  setEditableFieldValue(attr, test.value, inspector);
  await nodeMutated;

  info("Asserting the new attributes after edition");
  await assertAttributes(test.node, test.expectedAttributes, testActor);

  info("Undo the change and assert that the attributes have been changed back");
  await undoChange(inspector);
  await assertAttributes(test.node, test.originalAttributes, testActor);

  info("Redo the change and assert that the attributes have been changed " +
       "again");
  await redoChange(inspector);
  await assertAttributes(test.node, test.expectedAttributes, testActor);
}
