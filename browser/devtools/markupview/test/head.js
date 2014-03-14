/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;

let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let TargetFactory = devtools.TargetFactory;
let {console} = Cu.import("resource://gre/modules/devtools/Console.jsm", {});
let promise = devtools.require("sdk/core/promise");
let {getInplaceEditorForSpan: inplaceEditor} = devtools.require("devtools/shared/inplace-editor");

//Services.prefs.setBoolPref("devtools.dump.emit", true);

gDevTools.testing = true;
SimpleTest.registerCleanupFunction(() => {
  gDevTools.testing = false;
});

// Clear preferences that may be set during the course of tests.
function clearUserPrefs() {
  Services.prefs.clearUserPref("devtools.inspector.htmlPanelOpen");
  Services.prefs.clearUserPref("devtools.inspector.sidebarOpen");
  Services.prefs.clearUserPref("devtools.inspector.activeSidebar");
  Services.prefs.clearUserPref("devtools.dump.emit");
}

registerCleanupFunction(clearUserPrefs);

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url The url to be loaded in the new tab
 * @return a promise that resolves when the url is loaded
 */
function addTab(url) {
  let def = promise.defer();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    info("URL " + url + " loading complete into new test tab");
    waitForFocus(def.resolve, content);
  }, true);
  content.location = url;

  return def.promise;
}

/**
 * Open the toolbox, with the inspector tool visible.
 * @return a promise that resolves when the inspector is ready
 */
function openInspector() {
  let def = promise.defer();

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    info("Toolbox open");
    let inspector = toolbox.getCurrentPanel();
    inspector.once("inspector-updated", () => {
      info("Inspector panel active and ready");
      def.resolve({toolbox: toolbox, inspector: inspector});
    });
  }).then(null, console.error);

  return def.promise;
}

/**
 * Get the MarkupContainer object instance that corresponds to the given
 * HTML node
 * @param {MarkupView} markupView The instance of MarkupView currently loaded into the inspector panel
 * @param {DOMNode} rawNode The DOM node for which the container is required
 * @return {MarkupContainer}
 */
function getContainerForRawNode(markupView, rawNode) {
  let front = markupView.walker.frontForRawNode(rawNode);
  let container = markupView.getContainer(front);
  return container;
}

/**
 * Simple DOM node accesor function that takes either a node or a string css
 * selector as argument and returns the corresponding node
 * @param {String|DOMNode} nodeOrSelector
 * @return {DOMNode}
 */
function getNode(nodeOrSelector) {
  let node = nodeOrSelector;

  if (typeof nodeOrSelector === "string") {
    node = content.document.querySelector(nodeOrSelector);
    ok(node, "A node was found for selector " + nodeOrSelector);
  }

  return node;
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
  info("Selecting the node " + nodeOrSelector);
  let node = getNode(nodeOrSelector);
  let updated = inspector.once("inspector-updated");
  inspector.selection.setNode(node, reason);
  return updated;
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
  let container = getContainerForRawNode(inspector.markup, getNode(nodeOrSelector));
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
  let container = getContainerForRawNode(inspector.markup, getNode(nodeOrSelector));
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
 * @param {DOMNode} field The element that gets editable after receiving focus and <ENTER> keypress
 * @param {String} value The string value to be set into the edited field
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently loaded in the toolbox
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
 * Checks that a node has the given attributes
 *
 * @param {HTMLNode} element The node to check.
 * @param {Object} attrs An object containing the attributes to check.
 *        e.g. {id: "id1", class: "someclass"}
 *
 * Note that node.getAttribute() returns attribute values provided by the HTML
 * parser. The parser only provides unescaped entities so &amp; will return &.
 */
function assertAttributes(element, attrs) {
  is(element.attributes.length, Object.keys(attrs).length,
    "Node has the correct number of attributes.");
  for (let attr in attrs) {
    is(element.getAttribute(attr), attrs[attr],
      "Node has the correct " + attr + " attribute.");
  }
}

/**
 * Undo the last markup-view action and wait for the corresponding mutation to
 * occur
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently loaded in the toolbox
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
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently loaded in the toolbox
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
