/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let TargetFactory = devtools.TargetFactory;
let {console} = Cu.import("resource://gre/modules/devtools/Console.jsm", {});
let promise = devtools.require("sdk/core/promise");
let {getInplaceEditorForSpan: inplaceEditor} = devtools.require("devtools/shared/inplace-editor");

// All test are asynchronous
waitForExplicitFinish();

//Services.prefs.setBoolPref("devtools.dump.emit", true);

// Set the testing flag on gDevTools and reset it when the test ends
gDevTools.testing = true;
registerCleanupFunction(() => gDevTools.testing = false);

// Clear preferences that may be set during the course of tests.
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.inspector.htmlPanelOpen");
  Services.prefs.clearUserPref("devtools.inspector.sidebarOpen");
  Services.prefs.clearUserPref("devtools.inspector.activeSidebar");
  Services.prefs.clearUserPref("devtools.dump.emit");
  Services.prefs.clearUserPref("devtools.markup.pagesize");
});

// Auto close the toolbox and close the test tabs when the test ends
registerCleanupFunction(() => {
  try {
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.closeToolbox(target);
  } catch (ex) {
    dump(ex);
  }
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

const TEST_URL_ROOT = "http://mochi.test:8888/browser/browser/devtools/markupview/test/";

/**
 * Define an async test based on a generator function
 */
function asyncTest(generator) {
  return () => Task.spawn(generator).then(null, ok.bind(null, false)).then(finish);
}

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url The url to be loaded in the new tab
 * @return a promise that resolves to the tab object when the url is loaded
 */
function addTab(url) {
  info("Adding a new tab with URL: '" + url + "'");
  let def = promise.defer();

  let tab = gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    info("URL '" + url + "' loading complete");
    waitForFocus(() => {
      def.resolve(tab);
    }, content);
  }, true);
  content.location = url;

  return def.promise;
}

/**
 * Reload the current page
 * @return a promise that resolves when the inspector has emitted the event
 * new-root
 */
function reloadPage(inspector) {
  info("Reloading the page");
  let newRoot = inspector.once("new-root");
  content.location.reload();
  return newRoot;
}

/**
 * Open the toolbox, with the inspector tool visible.
 * @return a promise that resolves when the inspector is ready
 */
function openInspector() {
  info("Opening the inspector panel");
  let def = promise.defer();

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    info("The toolbox is open");
    let inspector = toolbox.getCurrentPanel();
    inspector.once("inspector-updated", () => {
      info("The inspector panel is active and ready");
      def.resolve({toolbox: toolbox, inspector: inspector});
    });
  }).then(null, console.error);

  return def.promise;
}

/**
 * Simple DOM node accesor function that takes either a node or a string css
 * selector as argument and returns the corresponding node
 * @param {String|DOMNode} nodeOrSelector
 * @return {DOMNode}
 */
function getNode(nodeOrSelector) {
  info("Getting the node for '" + nodeOrSelector + "'");
  return typeof nodeOrSelector === "string" ?
    content.document.querySelector(nodeOrSelector) :
    nodeOrSelector;
}

/**
 * Set the inspector's current selection to a node or to the first match of the
 * given css selector
 * @param {String|DOMNode} nodeOrSelector
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently loaded in the toolbox
 * @param {String} reason Defaults to "test" which instructs the inspector not to highlight the node upon selection
 * @return a promise that resolves when the inspector is updated with the new
 * node
 */
function selectNode(nodeOrSelector, inspector, reason="test") {
  info("Selecting the node for '" + nodeOrSelector + "'");
  let node = getNode(nodeOrSelector);
  let updated = inspector.once("inspector-updated");
  inspector.selection.setNode(node, reason);
  return updated;
}

/**
 * Get the MarkupContainer object instance that corresponds to the given
 * HTML node
 * @param {DOMNode|String} nodeOrSelector The DOM node for which the
 * container is required
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return {MarkupContainer}
 */
function getContainerForRawNode(nodeOrSelector, {markup}) {
  let front = markup.walker.frontForRawNode(getNode(nodeOrSelector));
  let container = markup.getContainer(front);
  info("Markup-container object for " + nodeOrSelector + " " + container);
  return container;
}

/**
 * Simulate a mouse-over on the markup-container (a line in the markup-view)
 * that corresponds to the node or selector passed.
 * @param {String|DOMNode} nodeOrSelector
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently loaded in the toolbox
 * @return a promise that resolves when the container is hovered and the higlighter
 * is shown on the corresponding node
 */
function hoverContainer(nodeOrSelector, inspector) {
  info("Hovering over the markup-container for node " + nodeOrSelector);
  let highlit = inspector.toolbox.once("node-highlight");
  let container = getContainerForRawNode(getNode(nodeOrSelector), inspector);
  EventUtils.synthesizeMouseAtCenter(container.tagLine, {type: "mousemove"},
    inspector.markup.doc.defaultView);
  return highlit;
}

/**
 * Simulate a click on the markup-container (a line in the markup-view)
 * that corresponds to the node or selector passed.
 * @param {String|DOMNode} nodeOrSelector
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently loaded in the toolbox
 * @return a promise that resolves when the node has been selected.
 */
function clickContainer(nodeOrSelector, inspector) {
  info("Clicking on the markup-container for node " + nodeOrSelector);
  let updated = inspector.once("inspector-updated");
  let container = getContainerForRawNode(getNode(nodeOrSelector), inspector);
  EventUtils.synthesizeMouseAtCenter(container.tagLine, {type: "mousedown"},
    inspector.markup.doc.defaultView);
  EventUtils.synthesizeMouseAtCenter(container.tagLine, {type: "mouseup"},
    inspector.markup.doc.defaultView);
  return updated;
}

/**
 * Checks if the highlighter is visible currently
 * @return {Boolean}
 */
function isHighlighterVisible() {
  let highlighter = gBrowser.selectedBrowser.parentNode
                            .querySelector(".highlighter-container .box-model-root");
  return highlighter && !highlighter.hasAttribute("hidden");
}

/**
 * Simulate the mouse leaving the markup-view area
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently loaded in the toolbox
 * @return a promise when done
 */
function mouseLeaveMarkupView(inspector) {
  info("Leaving the markup-view area");
  let def = promise.defer();

  // Find another element to mouseover over in order to leave the markup-view
  let btn = inspector.toolbox.doc.querySelector(".toolbox-dock-button");

  EventUtils.synthesizeMouseAtCenter(btn, {type: "mousemove"},
    inspector.toolbox.doc.defaultView);
  executeSoon(def.resolve);

  return def.promise;
}

/**
 * Focus a given editable element, enter edit mode, set value, and commit
 * @param {DOMNode} field The element that gets editable after receiving focus
 * and <ENTER> keypress
 * @param {String} value The string value to be set into the edited field
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 */
function setEditableFieldValue(field, value, inspector) {
  field.focus();
  EventUtils.sendKey("return", inspector.panelWin);
  let input = inplaceEditor(field).input;
  ok(input, "Found editable field for setting value: " + value);
  input.value = value;
  EventUtils.sendKey("return", inspector.panelWin);
}

/**
 * Focus the new-attribute inplace-editor field of the nodeOrSelector's markup
 * container, and enters the given text, then wait for it to be applied and the
 * for the node to mutates (when new attribute(s) is(are) created)
 * @param {DOMNode|String} nodeOrSelector The node or node selector to edit.
 * @param {String} text The new attribute text to be entered (e.g. "id='test'")
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return a promise that resolves when the node has mutated
 */
function addNewAttributes(nodeOrSelector, text, inspector) {
  info("Entering text '" + text + "' in node '" + nodeOrSelector + "''s new attribute field");

  let container = getContainerForRawNode(nodeOrSelector, inspector);
  ok(container, "The container for '" + nodeOrSelector + "' was found");

  info("Listening for the markupmutation event");
  let nodeMutated = inspector.once("markupmutation");
  setEditableFieldValue(container.editor.newAttr, text, inspector);
  return nodeMutated;
}

/**
 * Checks that a node has the given attributes
 *
 * @param {DOMNode|String} nodeOrSelector The node or node selector to check.
 * @param {Object} attrs An object containing the attributes to check.
 *        e.g. {id: "id1", class: "someclass"}
 *
 * Note that node.getAttribute() returns attribute values provided by the HTML
 * parser. The parser only provides unescaped entities so &amp; will return &.
 */
function assertAttributes(nodeOrSelector, attrs) {
  let node = getNode(nodeOrSelector);

  is(node.attributes.length, Object.keys(attrs).length,
    "Node has the correct number of attributes.");
  for (let attr in attrs) {
    is(node.getAttribute(attr), attrs[attr],
      "Node has the correct " + attr + " attribute.");
  }
}

/**
 * Undo the last markup-view action and wait for the corresponding mutation to
 * occur
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return a promise that resolves when the markup-mutation has been treated or
 * rejects if no undo action is possible
 */
function undoChange(inspector) {
  let canUndo = inspector.markup.undo.canUndo();
  ok(canUndo, "The last change in the markup-view can be undone");
  if (!canUndo) {
    return promise.reject();
  }

  let mutated = inspector.once("markupmutation");
  inspector.markup.undo.undo();
  return mutated;
}

/**
 * Redo the last markup-view action and wait for the corresponding mutation to
 * occur
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return a promise that resolves when the markup-mutation has been treated or
 * rejects if no redo action is possible
 */
function redoChange(inspector) {
  let canRedo = inspector.markup.undo.canRedo();
  ok(canRedo, "The last change in the markup-view can be redone");
  if (!canRedo) {
    return promise.reject();
  }

  let mutated = inspector.once("markupmutation");
  inspector.markup.undo.redo();
  return mutated;
}

/**
 * Get the selector-search input box from the inspector panel
 * @return {DOMNode}
 */
function getSelectorSearchBox(inspector) {
  return inspector.panelWin.document.getElementById("inspector-searchbox");
}

/**
 * Using the inspector panel's selector search box, search for a given selector.
 * The selector input string will be entered in the input field and the <ENTER>
 * keypress will be simulated.
 * This function won't wait for any events and is not async. It's up to callers
 * to subscribe to events and react accordingly.
 */
function searchUsingSelectorSearch(selector, inspector) {
  info("Entering \"" + selector + "\" into the selector-search input field");
  let field = getSelectorSearchBox(inspector);
  field.focus();
  field.value = selector;
  EventUtils.sendKey("return", inspector.panelWin);
}

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
 * @return a promise that resolves when the tests have run
 */
function runAddAttributesTests(tests, nodeOrSelector, inspector) {
  info("Running " + tests.length + " add-attributes tests");
  return Task.spawn(function*() {
    info("Selecting the test node");
    let div = getNode("div");
    yield selectNode(div, inspector);

    for (let test of tests) {
      yield runAddAttributesTest(test, div, inspector);
    }

    yield inspector.once("inspector-updated");
  });
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
 *        - validate {Function} optional extra function that will be called after
 *        the attributes have been added and which should be used to assert some
 *        more things this test runner might not be checking. The function will
 *        be called with the following arguments:
 *          - {DOMNode} The element being tested
 *          - {MarkupContainer} The corresponding container in the markup-view
 *          - {InspectorPanel} The instance of the InspectorPanel opened
 * @param {DOMNode|String} nodeOrSelector The node or node selector
 * corresponding to the test element
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * opened
 */
function* runAddAttributesTest(test, nodeOrSelector, inspector) {
  let element = getNode(nodeOrSelector);

  info("Starting add-attribute test: " + test.desc);
  yield addNewAttributes(element, test.text, inspector);

  info("Assert that the attribute(s) has/have been applied correctly");
  assertAttributes(element, test.expectedAttributes);

  if (test.validate) {
    test.validate(element, getContainerForRawNode(element, inspector), inspector);
  }

  info("Undo the change");
  yield undoChange(inspector);

  info("Assert that the attribute(s) has/have been removed correctly");
  assertAttributes(element, {});
}

/**
 * Run a series of edit-attributes tests.
 * This function will iterate over the provided tests array and run each test.
 * Each test's goal is to locate a given element on the current test page, assert
 * its current attributes, then provide the name of one of them and a value to
 * be set into it, and then check if the new attributes are correct.
 * After each test has run, the markup-view's undo and redo commands will be
 * called and the test runner will assert again that the attributes are correct.
 * @param {Array} tests See runEditAttributesTest for the structure
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * opened
 * @return a promise that resolves when the tests have run
 */
function runEditAttributesTests(tests, inspector) {
  info("Running " + tests.length + " edit-attributes tests");
  return Task.spawn(function*() {
    info("Expanding all nodes in the markup-view");
    yield inspector.markup.expandAll();

    for (let test of tests) {
      yield runEditAttributesTest(test, inspector);
    }

    yield inspector.once("inspector-updated");
  });
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
 * opened
 */
function* runEditAttributesTest(test, inspector) {
  info("Starting edit-attribute test: " + test.desc);

  info("Selecting the test node " + test.node);
  yield selectNode(test.node, inspector);

  info("Asserting that the node has the right attributes to start with");
  assertAttributes(test.node, test.originalAttributes);

  info("Editing attribute " + test.name + " with value " + test.value);

  let container = getContainerForRawNode(test.node, inspector);
  ok(container && container.editor, "The markup-container for " + test.node +
    " was found");

  info("Listening for the markupmutation event");
  let nodeMutated = inspector.once("markupmutation");
  let attr = container.editor.attrs[test.name].querySelector(".editable");
  setEditableFieldValue(attr, test.value, inspector);
  yield nodeMutated;

  info("Asserting the new attributes after edition");
  assertAttributes(test.node, test.expectedAttributes);

  info("Undo the change and assert that the attributes have been changed back");
  yield undoChange(inspector);
  assertAttributes(test.node, test.originalAttributes);

  info("Redo the change and assert that the attributes have been changed again");
  yield redoChange(inspector);
  assertAttributes(test.node, test.expectedAttributes);
}

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
