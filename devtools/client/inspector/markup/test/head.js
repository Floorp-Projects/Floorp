/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../test/head.js */
"use strict";

// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js",
  this);

var {getInplaceEditorForSpan: inplaceEditor} = require("devtools/client/shared/inplace-editor");
var clipboard = require("sdk/clipboard");
var {ActorRegistryFront} = require("devtools/server/actors/actor-registry");

// If a test times out we want to see the complete log and not just the last few
// lines.
SimpleTest.requestCompleteLog();

// Set the testing flag on DevToolsUtils and reset it when the test ends
DevToolsUtils.testing = true;
registerCleanupFunction(() => {
  DevToolsUtils.testing = false;
});

// Clear preferences that may be set during the course of tests.
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.inspector.htmlPanelOpen");
  Services.prefs.clearUserPref("devtools.inspector.sidebarOpen");
  Services.prefs.clearUserPref("devtools.markup.pagesize");
  Services.prefs.clearUserPref("dom.webcomponents.enabled");
  Services.prefs.clearUserPref("devtools.inspector.showAllAnonymousContent");
});

/**
 * Some tests may need to import one or more of the test helper scripts.
 * A test helper script is simply a js file that contains common test code that
 * is either not common-enough to be in head.js, or that is located in a
 * separate directory.
 * The script will be loaded synchronously and in the test's scope.
 * @param {String} filePath The file path, relative to the current directory.
 *                 Examples:
 *                 - "helper_attributes_test_runner.js"
 *                 - "../../../commandline/test/helpers.js"
 */
function loadHelperScript(filePath) {
  let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
  Services.scriptloader.loadSubScript(testDir + "/" + filePath, this);
}

/**
 * Reload the current page
 * @return a promise that resolves when the inspector has emitted the event
 * new-root
 */
function reloadPage(inspector, testActor) {
  info("Reloading the page");
  let newRoot = inspector.once("new-root");
  testActor.reload();
  return newRoot;
}

/**
 * Get the MarkupContainer object instance that corresponds to the given
 * NodeFront
 * @param {NodeFront} nodeFront
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return {MarkupContainer}
 */
function getContainerForNodeFront(nodeFront, {markup}) {
  return markup.getContainer(nodeFront);
}

/**
 * Get the MarkupContainer object instance that corresponds to the given
 * selector
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return {MarkupContainer}
 */
var getContainerForSelector = Task.async(function* (selector, inspector) {
  info("Getting the markup-container for node " + selector);
  let nodeFront = yield getNodeFront(selector, inspector);
  let container = getContainerForNodeFront(nodeFront, inspector);
  info("Found markup-container " + container);
  return container;
});

/**
 * Retrieve the nodeValue for the firstChild of a provided selector on the content page.
 *
 * @param {String} selector
 * @param {TestActorFront} testActor The current TestActorFront instance.
 * @return {String} the nodeValue of the first
 */
function* getFirstChildNodeValue(selector, testActor) {
  let nodeValue = yield testActor.eval(`
    content.document.querySelector("${selector}").firstChild.nodeValue;
  `);
  return nodeValue;
}

/**
 * Using the markupview's _waitForChildren function, wait for all queued
 * children updates to be handled.
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return a promise that resolves when all queued children updates have been
 * handled
 */
function waitForChildrenUpdated({markup}) {
  info("Waiting for queued children updates to be handled");
  let def = promise.defer();
  markup._waitForChildren().then(() => {
    executeSoon(def.resolve);
  });
  return def.promise;
}

/**
 * Simulate a click on the markup-container (a line in the markup-view)
 * that corresponds to the selector passed.
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return {Promise} Resolves when the node has been selected.
 */
var clickContainer = Task.async(function* (selector, inspector) {
  info("Clicking on the markup-container for node " + selector);

  let nodeFront = yield getNodeFront(selector, inspector);
  let container = getContainerForNodeFront(nodeFront, inspector);

  let updated = container.selected
                ? promise.resolve()
                : inspector.once("inspector-updated");
  EventUtils.synthesizeMouseAtCenter(container.tagLine, {type: "mousedown"},
    inspector.markup.doc.defaultView);
  EventUtils.synthesizeMouseAtCenter(container.tagLine, {type: "mouseup"},
    inspector.markup.doc.defaultView);
  return updated;
});

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
 * Focus the new-attribute inplace-editor field of a node's markup container
 * and enters the given text, then wait for it to be applied and the for the
 * node to mutates (when new attribute(s) is(are) created)
 * @param {String} selector The selector for the node to edit.
 * @param {String} text The new attribute text to be entered (e.g. "id='test'")
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return a promise that resolves when the node has mutated
 */
var addNewAttributes = Task.async(function* (selector, text, inspector) {
  info(`Entering text "${text}" in new attribute field for node ${selector}`);

  let container = yield focusNode(selector, inspector);
  ok(container, "The container for '" + selector + "' was found");

  info("Listening for the markupmutation event");
  let nodeMutated = inspector.once("markupmutation");
  setEditableFieldValue(container.editor.newAttr, text, inspector);
  yield nodeMutated;
});

/**
 * Checks that a node has the given attributes.
 *
 * @param {String} selector The selector for the node to check.
 * @param {Object} expected An object containing the attributes to check.
 *        e.g. {id: "id1", class: "someclass"}
 * @param {TestActorFront} testActor The current TestActorFront instance.
 *
 * Note that node.getAttribute() returns attribute values provided by the HTML
 * parser. The parser only provides unescaped entities so &amp; will return &.
 */
var assertAttributes = Task.async(function* (selector, expected, testActor) {
  let {attributes: actual} = yield testActor.getNodeInfo(selector);

  is(actual.length, Object.keys(expected).length,
    "The node " + selector + " has the expected number of attributes.");
  for (let attr in expected) {
    let foundAttr = actual.find(({name}) => name === attr);
    let foundValue = foundAttr ? foundAttr.value : undefined;
    ok(foundAttr, "The node " + selector + " has the attribute " + attr);
    is(foundValue, expected[attr],
      "The node " + selector + " has the correct " + attr + " attribute value");
  }
});

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
 * This shouldn't be used in the tests, but is useful when writing new tests or
 * debugging existing tests in order to introduce delays in the test steps
 * @param {Number} ms The time to wait
 * @return A promise that resolves when the time is passed
 */
function wait(ms) {
  let def = promise.defer();
  setTimeout(def.resolve, ms);
  return def.promise;
}

/**
 * Check to see if the inspector menu items for editing are disabled.
 * Things like Edit As HTML, Delete Node, etc.
 * @param {NodeFront} nodeFront
 * @param {InspectorPanel} inspector
 * @param {Boolean} assert Should this function run assertions inline.
 * @return A promise that resolves with a boolean indicating whether
 *         the menu items are disabled once the menu has been checked.
 */
var isEditingMenuDisabled = Task.async(
function* (nodeFront, inspector, assert = true) {
  let doc = inspector.panelDoc;
  let deleteMenuItem = doc.getElementById("node-menu-delete");
  let editHTMLMenuItem = doc.getElementById("node-menu-edithtml");
  let pasteHTMLMenuItem = doc.getElementById("node-menu-pasteouterhtml");

  // To ensure clipboard contains something to paste.
  clipboard.set("<p>test</p>", "html");

  let menu = inspector.nodemenu;
  yield selectNode(nodeFront, inspector);
  yield reopenMenu(menu);

  let isDeleteMenuDisabled = deleteMenuItem.hasAttribute("disabled");
  let isEditHTMLMenuDisabled = editHTMLMenuItem.hasAttribute("disabled");
  let isPasteHTMLMenuDisabled = pasteHTMLMenuItem.hasAttribute("disabled");

  if (assert) {
    ok(isDeleteMenuDisabled, "Delete menu item is disabled");
    ok(isEditHTMLMenuDisabled, "Edit HTML menu item is disabled");
    ok(isPasteHTMLMenuDisabled, "Paste HTML menu item is disabled");
  }

  return isDeleteMenuDisabled &&
         isEditHTMLMenuDisabled &&
         isPasteHTMLMenuDisabled;
});

/**
 * Check to see if the inspector menu items for editing are enabled.
 * Things like Edit As HTML, Delete Node, etc.
 * @param {NodeFront} nodeFront
 * @param {InspectorPanel} inspector
 * @param {Boolean} assert Should this function run assertions inline.
 * @return A promise that resolves with a boolean indicating whether
 *         the menu items are enabled once the menu has been checked.
 */
var isEditingMenuEnabled = Task.async(
function* (nodeFront, inspector, assert = true) {
  let doc = inspector.panelDoc;
  let deleteMenuItem = doc.getElementById("node-menu-delete");
  let editHTMLMenuItem = doc.getElementById("node-menu-edithtml");
  let pasteHTMLMenuItem = doc.getElementById("node-menu-pasteouterhtml");

  // To ensure clipboard contains something to paste.
  clipboard.set("<p>test</p>", "html");

  let menu = inspector.nodemenu;
  yield selectNode(nodeFront, inspector);
  yield reopenMenu(menu);

  let isDeleteMenuDisabled = deleteMenuItem.hasAttribute("disabled");
  let isEditHTMLMenuDisabled = editHTMLMenuItem.hasAttribute("disabled");
  let isPasteHTMLMenuDisabled = pasteHTMLMenuItem.hasAttribute("disabled");

  if (assert) {
    ok(!isDeleteMenuDisabled, "Delete menu item is enabled");
    ok(!isEditHTMLMenuDisabled, "Edit HTML menu item is enabled");
    ok(!isPasteHTMLMenuDisabled, "Paste HTML menu item is enabled");
  }

  return !isDeleteMenuDisabled &&
         !isEditHTMLMenuDisabled &&
         !isPasteHTMLMenuDisabled;
});

/**
 * Open a menu (closing it first if necessary).
 * @param {DOMNode} menu A menu that implements hidePopup/openPopup
 * @return a promise that resolves once the menu is opened.
 */
var reopenMenu = Task.async(function* (menu) {
  // First close it is if it is already opened.
  if (menu.state == "closing" || menu.state == "open") {
    let popuphidden = once(menu, "popuphidden", true);
    menu.hidePopup();
    yield popuphidden;
  }

  // Then open it and return once
  let popupshown = once(menu, "popupshown", true);
  menu.openPopup();
  yield popupshown;
});

/**
 * Wait for all current promises to be resolved. See this as executeSoon that
 * can be used with yield.
 */
function promiseNextTick() {
  let deferred = promise.defer();
  executeSoon(deferred.resolve);
  return deferred.promise;
}

/**
 * Collapses the current text selection in an input field and tabs to the next
 * field.
 */
function collapseSelectionAndTab(inspector) {
  // collapse selection and move caret to end
  EventUtils.sendKey("tab", inspector.panelWin);
  // next element
  EventUtils.sendKey("tab", inspector.panelWin);
}

/**
 * Collapses the current text selection in an input field and tabs to the
 * previous field.
 */
function collapseSelectionAndShiftTab(inspector) {
  // collapse selection and move caret to end
  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true },
    inspector.panelWin);
  // previous element
  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true },
    inspector.panelWin);
}

/**
 * Check that the current focused element is an attribute element in the markup
 * view.
 * @param {String} attrName The attribute name expected to be found
 * @param {Boolean} editMode Whether or not the attribute should be in edit mode
 */
function checkFocusedAttribute(attrName, editMode) {
  let focusedAttr = Services.focus.focusedElement;
  is(focusedAttr ? focusedAttr.parentNode.dataset.attr : undefined,
     attrName, attrName + " attribute editor is currently focused.");
  is(focusedAttr ? focusedAttr.tagName : undefined,
     editMode ? "input" : "span",
     editMode
     ? attrName + " is in edit mode"
     : attrName + " is not in edit mode");
}

/**
 * Get attributes for node as how they are represented in editor.
 *
 * @param  {String} selector
 * @param  {InspectorPanel} inspector
 * @return {Promise}
 *         A promise that resolves with an array of attribute names
 *         (e.g. ["id", "class", "href"])
 */
var getAttributesFromEditor = Task.async(function* (selector, inspector) {
  let nodeList = (yield getContainerForSelector(selector, inspector))
    .tagLine.querySelectorAll("[data-attr]");

  return [...nodeList].map(node => node.getAttribute("data-attr"));
});

// The expand all operation of the markup-view calls itself recursively and
// there's not one event we can wait for to know when it's done so use this
// helper function to wait until all recursive children updates are done.
function* waitForMultipleChildrenUpdates(inspector) {
  // As long as child updates are queued up while we wait for an update already
  // wait again
  if (inspector.markup._queuedChildUpdates &&
      inspector.markup._queuedChildUpdates.size) {
    yield waitForChildrenUpdated(inspector);
    return yield waitForMultipleChildrenUpdates(inspector);
  }
  return undefined;
}

/**
 * Create an HTTP server that can be used to simulate custom requests within
 * a test.  It is automatically cleaned up when the test ends, so no need to
 * call `destroy`.
 *
 * See https://developer.mozilla.org/en-US/docs/Httpd.js/HTTP_server_for_unit_tests
 * for more information about how to register handlers.
 *
 * The server can be accessed like:
 *
 *   const server = createTestHTTPServer();
 *   let url = "http://localhost: " + server.identity.primaryPort + "/path";
 *
 * @returns {HttpServer}
 */
function createTestHTTPServer() {
  const {HttpServer} = Cu.import("resource://testing-common/httpd.js", {});
  let server = new HttpServer();

  registerCleanupFunction(function* cleanup() {
    let destroyed = promise.defer();
    server.stop(() => {
      destroyed.resolve();
    });
    yield destroyed.promise;
  });

  server.start(-1);
  return server;
}

/**
 * A helper that simulates a contextmenu event on the given chrome DOM element.
 */
function contextMenuClick(element) {
  let evt = element.ownerDocument.createEvent("MouseEvents");
  let buttonRight = 2;

  evt.initMouseEvent("contextmenu", true, true,
    element.ownerDocument.defaultView, 1, 0, 0, 0, 0, false, false, false,
    false, buttonRight, null);

  element.dispatchEvent(evt);
}

/**
 * Registers new backend tab actor.
 *
 * @param {DebuggerClient} client RDP client object (toolbox.target.client)
 * @param {Object} options Configuration object with the following options:
 *
 * - moduleUrl {String}: URL of the module that contains actor implementation.
 * - prefix {String}: prefix of the actor.
 * - actorClass {ActorClass}: Constructor object for the actor.
 * - frontClass {FrontClass}: Constructor object for the front part
 * of the registered actor.
 *
 * @returns {Promise} A promise that is resolved when the actor is registered.
 * The resolved value has two properties:
 *
 * - registrar {ActorActor}: A handle to the registered actor that allows
 * unregistration.
 * - form {Object}: The JSON actor form provided by the server.
 */
function registerTabActor(client, options) {
  let moduleUrl = options.moduleUrl;

  return client.listTabs().then(response => {
    let config = {
      prefix: options.prefix,
      constructor: options.actorClass,
      type: { tab: true },
    };

    // Register the custom actor on the backend.
    let registry = ActorRegistryFront(client, response);
    return registry.registerActor(moduleUrl, config).then(registrar => {
      return client.getTab().then(tabResponse => ({
        registrar: registrar,
        form: tabResponse.tab
      }));
    });
  });
}

/**
 * A helper for unregistering an existing backend actor.
 *
 * @param {ActorActor} registrar A handle to the registered actor
 * that has been received after registration.
 * @param {Front} Corresponding front object.
 *
 * @returns A promise that is resolved when the unregistration
 * has finished.
 */
function unregisterActor(registrar, front) {
  return front.detach().then(() => {
    return registrar.unregister();
  });
}

/**
 * Simulate dragging a MarkupContainer by calling its mousedown and mousemove
 * handlers.
 * @param {InspectorPanel} inspector The current inspector-panel instance.
 * @param {String|MarkupContainer} selector The selector to identify the node or
 * the MarkupContainer for this node.
 * @param {Number} xOffset Optional x offset to drag by.
 * @param {Number} yOffset Optional y offset to drag by.
 */
function* simulateNodeDrag(inspector, selector, xOffset = 10, yOffset = 10) {
  let container = typeof selector === "string"
                  ? yield getContainerForSelector(selector, inspector)
                  : selector;
  let rect = container.tagLine.getBoundingClientRect();
  let scrollX = inspector.markup.doc.documentElement.scrollLeft;
  let scrollY = inspector.markup.doc.documentElement.scrollTop;

  info("Simulate mouseDown on element " + selector);
  container._onMouseDown({
    target: container.tagLine,
    button: 0,
    pageX: scrollX + rect.x,
    pageY: scrollY + rect.y,
    stopPropagation: () => {},
    preventDefault: () => {}
  });

  // _onMouseDown selects the node, so make sure to wait for the
  // inspector-updated event if the current selection was different.
  if (inspector.selection.nodeFront !== container.node) {
    yield inspector.once("inspector-updated");
  }

  info("Simulate mouseMove on element " + selector);
  container._onMouseMove({
    pageX: scrollX + rect.x + xOffset,
    pageY: scrollY + rect.y + yOffset
  });
}

/**
 * Simulate dropping a MarkupContainer by calling its mouseup handler. This is
 * meant to be called after simulateNodeDrag has been called.
 * @param {InspectorPanel} inspector The current inspector-panel instance.
 * @param {String|MarkupContainer} selector The selector to identify the node or
 * the MarkupContainer for this node.
 */
function* simulateNodeDrop(inspector, selector) {
  info("Simulate mouseUp on element " + selector);
  let container = typeof selector === "string"
                  ? yield getContainerForSelector(selector, inspector)
                  : selector;
  container._onMouseUp();
  inspector.markup._onMouseUp();
}

/**
 * Simulate drag'n'dropping a MarkupContainer by calling its mousedown,
 * mousemove and mouseup handlers.
 * @param {InspectorPanel} inspector The current inspector-panel instance.
 * @param {String|MarkupContainer} selector The selector to identify the node or
 * the MarkupContainer for this node.
 * @param {Number} xOffset Optional x offset to drag by.
 * @param {Number} yOffset Optional y offset to drag by.
 */
function* simulateNodeDragAndDrop(inspector, selector, xOffset, yOffset) {
  yield simulateNodeDrag(inspector, selector, xOffset, yOffset);
  yield simulateNodeDrop(inspector, selector);
}
