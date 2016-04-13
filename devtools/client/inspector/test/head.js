/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../framework/test/shared-head.js */
"use strict";

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/test/shared-head.js",
  this);

// Services.prefs.setBoolPref("devtools.debugger.log", true);
// SimpleTest.registerCleanupFunction(() => {
//   Services.prefs.clearUserPref("devtools.debugger.log");
// });

// Import the GCLI test helper
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/commandline/test/helpers.js",
  this);

// Import helpers registering the test-actor in remote targets
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/test-actor-registry.js",
  this);

DevToolsUtils.testing = true;
registerCleanupFunction(() => {
  DevToolsUtils.testing = false;
});

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.inspector.activeSidebar");
});

registerCleanupFunction(function*() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  yield gDevTools.closeToolbox(target);

  // Move the mouse outside inspector. If the test happened fake a mouse event
  // somewhere over inspector the pointer is considered to be there when the
  // next test begins. This might cause unexpected events to be emitted when
  // another test moves the mouse.
  EventUtils.synthesizeMouseAtPoint(1, 1, {type: "mousemove"}, window);

  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

var navigateTo = function (toolbox, url) {
  let activeTab = toolbox.target.activeTab;
  return activeTab.navigateTo(url);
};

/**
 * Simple DOM node accesor function that takes either a node or a string css
 * selector as argument and returns the corresponding node
 * @param {String|DOMNode} nodeOrSelector
 * @param {Object} options
 *        An object containing any of the following options:
 *        - document: HTMLDocument that should be queried for the selector.
 *                    Default: content.document.
 *        - expectNoMatch: If true and a node matches the given selector, a
 *                         failure is logged for an unexpected match.
 *                         If false and nothing matches the given selector, a
 *                         failure is logged for a missing match.
 *                         Default: false.
 * @return {DOMNode}
 */
function getNode(nodeOrSelector, options = {}) {
  let document = options.document || content.document;
  let noMatches = !!options.expectNoMatch;

  if (typeof nodeOrSelector === "string") {
    info("Looking for a node that matches selector " + nodeOrSelector);
    let node = document.querySelector(nodeOrSelector);
    if (noMatches) {
      ok(!node, "Selector " + nodeOrSelector + " didn't match any nodes.");
    }
    else {
      ok(node, "Selector " + nodeOrSelector + " matched a node.");
    }

    return node;
  }

  info("Looking for a node but selector was not a string.");
  return nodeOrSelector;
}

/**
 * Start the element picker and focus the content window.
 * @param {Toolbox} toolbox
 */
var startPicker = Task.async(function*(toolbox) {
  info("Start the element picker");
  yield toolbox.highlighterUtils.startPicker();
  // Make sure the content window is focused since the picker does not focus
  // the content window by default.
  yield ContentTask.spawn(gBrowser.selectedBrowser, null, function* () {
    content.focus();
  });
});

/**
 * Highlight a node and set the inspector's current selection to the node or
 * the first match of the given css selector.
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox
 * @return a promise that resolves when the inspector is updated with the new
 * node
 */
function selectAndHighlightNode(selector, inspector) {
  info("Highlighting and selecting the node " + selector);
  return selectNode(selector, inspector, "test-highlight");
}

/**
 * Set the inspector's current selection to the first match of the given css
 * selector
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @param {String} reason Defaults to "test" which instructs the inspector not
 * to highlight the node upon selection
 * @return {Promise} Resolves when the inspector is updated with the new node
 */
var selectNode = Task.async(function*(selector, inspector, reason="test") {
  info("Selecting the node for '" + selector + "'");
  let nodeFront = yield getNodeFront(selector, inspector);
  let updated = inspector.once("inspector-updated");
  inspector.selection.setNodeFront(nodeFront, reason);
  yield updated;
});

/**
 * Open the inspector in a tab with given URL.
 * @param {string} url  The URL to open.
 * @param {String} hostType Optional hostType, as defined in Toolbox.HostType
 * @return A promise that is resolved once the tab and inspector have loaded
 *         with an object: { tab, toolbox, inspector }.
 */
var openInspectorForURL = Task.async(function*(url, hostType) {
  let tab = yield addTab(url);
  let { inspector, toolbox, testActor } = yield openInspector(hostType);
  return { tab, inspector, toolbox, testActor };
});

/**
 * Open the toolbox, with the inspector tool visible.
 * @param {String} hostType Optional hostType, as defined in Toolbox.HostType
 * @return a promise that resolves when the inspector is ready
 */
var openInspector = Task.async(function*(hostType) {
  info("Opening the inspector");

  let toolbox = yield openToolboxForTab(gBrowser.selectedTab, "inspector", hostType);
  let inspector = toolbox.getPanel("inspector");

  info("Waiting for the inspector to update");
  if (inspector._updateProgress) {
    yield inspector.once("inspector-updated");
  }

  yield registerTestActor(toolbox.target.client);
  let testActor = yield getTestActor(toolbox);

  return {toolbox, inspector, testActor};
});

function getActiveInspector() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  return gDevTools.getToolbox(target).getPanel("inspector");
}

/**
 * Right click on a node in the test page and click on the inspect menu item.
 * @param {TestActor}
 * @param {String} selector The selector for the node to click on in the page.
 * @return {Promise} Resolves to the inspector when it has opened and is updated
 */
var clickOnInspectMenuItem = Task.async(function*(testActor, selector) {
  info("Showing the contextual menu on node " + selector);
  let contentAreaContextMenu = document.querySelector("#contentAreaContextMenu");
  let contextOpened = once(contentAreaContextMenu, "popupshown");

  yield testActor.synthesizeMouse({
    selector: selector,
    center: true,
    options: {type: "contextmenu", button: 2}
  });

  yield contextOpened;

  info("Triggering the inspect action");
  yield gContextMenu.inspectNode();

  info("Hiding the menu");
  let contextClosed = once(contentAreaContextMenu, "popuphidden");
  contentAreaContextMenu.hidePopup();
  yield contextClosed;

  return getActiveInspector();
});

/**
 * Open the toolbox, with the inspector tool visible, and the one of the sidebar
 * tabs selected.
 * @param {String} id The ID of the sidebar tab to be opened
 * @param {String} hostType Optional hostType, as defined in Toolbox.HostType
 * @return a promise that resolves when the inspector is ready and the tab is
 * visible and ready
 */
var openInspectorSidebarTab = Task.async(function*(id, hostType) {
  let {toolbox, inspector, testActor} = yield openInspector();

  info("Selecting the " + id + " sidebar");
  inspector.sidebar.select(id);

  return {
    toolbox,
    inspector,
    testActor
  };
});

/**
 * Get the NodeFront for a node that matches a given css selector, via the
 * protocol.
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return {Promise} Resolves to the NodeFront instance
 */
function getNodeFront(selector, {walker}) {
  if (selector._form) {
    return selector;
  }
  return walker.querySelector(walker.rootNode, selector);
}

/**
 * Get the NodeFront for a node that matches a given css selector inside a
 * given iframe.
 * @param {String|NodeFront} selector
 * @param {String|NodeFront} frameSelector A selector that matches the iframe
 * the node is in
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @param {String} reason Defaults to "test" which instructs the inspector not
 * to highlight the node upon selection
 * @return {Promise} Resolves when the inspector is updated with the new node
 */
var getNodeFrontInFrame = Task.async(function*(selector, frameSelector,
                                               inspector, reason="test") {
  let iframe = yield getNodeFront(frameSelector, inspector);
  let {nodes} = yield inspector.walker.children(iframe);
  return inspector.walker.querySelector(nodes[0], selector);
});

var focusSearchBoxUsingShortcut = Task.async(function* (panelWin, callback) {
  info("Focusing search box");
  let searchBox = panelWin.document.getElementById("inspector-searchbox");
  let focused = once(searchBox, "focus");

  panelWin.focus();
  synthesizeKeyFromKeyTag(panelWin.document.getElementById("nodeSearchKey"));

  yield focused;

  if (callback) {
    callback();
  }
});

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
var getContainerForSelector = Task.async(function*(selector, inspector) {
  info("Getting the markup-container for node " + selector);
  let nodeFront = yield getNodeFront(selector, inspector);
  let container = getContainerForNodeFront(nodeFront, inspector);
  info("Found markup-container " + container);
  return container;
});

/**
 * Simulate a mouse-over on the markup-container (a line in the markup-view)
 * that corresponds to the selector passed.
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return {Promise} Resolves when the container is hovered and the higlighter
 * is shown on the corresponding node
 */
var hoverContainer = Task.async(function*(selector, inspector) {
  info("Hovering over the markup-container for node " + selector);

  let nodeFront = yield getNodeFront(selector, inspector);
  let container = getContainerForNodeFront(nodeFront, inspector);

  let highlit = inspector.toolbox.once("node-highlight");
  EventUtils.synthesizeMouseAtCenter(container.tagLine, {type: "mousemove"},
    inspector.markup.doc.defaultView);
  return highlit;
});

/**
 * Simulate a click on the markup-container (a line in the markup-view)
 * that corresponds to the selector passed.
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return {Promise} Resolves when the node has been selected.
 */
var clickContainer = Task.async(function*(selector, inspector) {
  info("Clicking on the markup-container for node " + selector);

  let nodeFront = yield getNodeFront(selector, inspector);
  let container = getContainerForNodeFront(nodeFront, inspector);

  let updated = inspector.once("inspector-updated");
  EventUtils.synthesizeMouseAtCenter(container.tagLine, {type: "mousedown"},
    inspector.markup.doc.defaultView);
  EventUtils.synthesizeMouseAtCenter(container.tagLine, {type: "mouseup"},
    inspector.markup.doc.defaultView);
  return updated;
});

/**
 * Simulate the mouse leaving the markup-view area
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently loaded in the toolbox
 * @return a promise when done
 */
function mouseLeaveMarkupView(inspector) {
  info("Leaving the markup-view area");
  let def = promise.defer();

  // Find another element to mouseover over in order to leave the markup-view
  let btn = inspector.toolbox.doc.querySelector("#toolbox-controls");

  EventUtils.synthesizeMouseAtCenter(btn, {type: "mousemove"},
    inspector.toolbox.doc.defaultView);
  executeSoon(def.resolve);

  return def.promise;
}

/**
 * Dispatch the copy event on the given element
 */
function fireCopyEvent(element) {
  let evt = element.ownerDocument.createEvent("Event");
  evt.initEvent("copy", true, true);
  element.dispatchEvent(evt);
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
 * Dispatch a command event on a node (e.g. click on a contextual menu item).
 * @param {DOMNode} node
 */
function dispatchCommandEvent(node) {
  info("Dispatching command event on " + node);
  let commandEvent = document.createEvent("XULCommandEvent");
  commandEvent.initCommandEvent("command", true, true, window, 0, false, false,
                                false, false, null);
  node.dispatchEvent(commandEvent);
}

/**
 * A helper that simulates a contextmenu event on the given chrome DOM element.
 */
function contextMenuClick(element) {
  let evt = element.ownerDocument.createEvent('MouseEvents');
  let button = 2;  // right click

  evt.initMouseEvent('contextmenu', true, true,
       element.ownerDocument.defaultView, 1, 0, 0, 0, 0, false,
       false, false, false, button, null);

  element.dispatchEvent(evt);
}

/**
 * A helper that fetches a front for a node that matches the given selector or
 * doctype node if the selector is falsy.
 */
function* getNodeFrontForSelector(selector, inspector) {
  if (selector) {
    info("Retrieving front for selector " + selector);
    return getNodeFront(selector, inspector);
  } else {
    info("Retrieving front for doctype node");
    let {nodes} = yield inspector.walker.children(inspector.walker.rootNode);
    return nodes[0];
  }
}

/**
 * Encapsulate some common operations for highlighter's tests, to have
 * the tests cleaner, without exposing directly `inspector`, `highlighter`, and
 * `testActor` if not needed.
 *
 * @param  {String}
 *    The highlighter's type
 * @return
 *    A generator function that takes an object with `inspector` and `testActor`
 *    properties. (see `openInspector`)
 */
const getHighlighterHelperFor = (type) => Task.async(
  function*({inspector, testActor}) {
    let front = inspector.inspector;
    let highlighter = yield front.getHighlighterByType(type);

    let prefix = "";

    // Internals for mouse events
    let prevX, prevY;

    // Highlighted node
    let  highlightedNode = null;

    return {
      set prefix(value) {
        prefix = value;
      },
      get highlightedNode() {
        if (!highlightedNode) {
          return null;
        }

        return {
          getComputedStyle: function*(options = {}) {
            return yield inspector.pageStyle.getComputed(
              highlightedNode, options);
          }
        };
      },

      show: function*(selector = ":root") {
        highlightedNode = yield getNodeFront(selector, inspector);
        return yield highlighter.show(highlightedNode);
      },

      hide: function*() {
        yield highlighter.hide();
      },

      isElementHidden: function*(id) {
        return (yield testActor.getHighlighterNodeAttribute(
          prefix + id, "hidden", highlighter)) === "true";
      },

      getElementTextContent: function*(id) {
        return yield testActor.getHighlighterNodeTextContent(
          prefix + id, highlighter);
      },

      getElementAttribute: function*(id, name) {
        return yield testActor.getHighlighterNodeAttribute(
          prefix + id, name, highlighter);
      },

      synthesizeMouse: function*(options) {
        options = Object.assign({selector: ":root"}, options);
        yield testActor.synthesizeMouse(options);
      },

      // This object will synthesize any "mouse" prefixed event to the
      // `testActor`, using the name of method called as suffix for the
      // event's name.
      // If no x, y coords are given, the previous ones are used.
      //
      // For example:
      //   mouse.down(10, 20); // synthesize "mousedown" at 10,20
      //   mouse.move(20, 30); // synthesize "mousemove" at 20,30
      //   mouse.up();         // synthesize "mouseup" at 20,30
      mouse: new Proxy({}, {
        get: (target, name) =>
          function*(x = prevX, y = prevY) {
            prevX = x;
            prevY = y;
            yield testActor.synthesizeMouse({
              selector: ":root", x, y, options: {type: "mouse" + name}});
          }
      }),

      reflow: function*() {
        yield testActor.reflow();
      },

      finalize: function*() {
        highlightedNode = null;
        yield highlighter.finalize();
      }
    };
  }
);

// The expand all operation of the markup-view calls itself recursively and
// there's not one event we can wait for to know when it's done
// so use this helper function to wait until all recursive children updates are done.
function* waitForMultipleChildrenUpdates(inspector) {
// As long as child updates are queued up while we wait for an update already
// wait again
    if (inspector.markup._queuedChildUpdates &&
        inspector.markup._queuedChildUpdates.size) {
        yield waitForChildrenUpdated(inspector);
        return yield waitForMultipleChildrenUpdates(inspector);
    }
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
 * Wait for the toolbox to emit the styleeditor-selected event and when done
 * wait for the stylesheet identified by href to be loaded in the stylesheet
 * editor
 *
 * @param {Toolbox} toolbox
 * @param {String} href
 *        Optional, if not provided, wait for the first editor to be ready
 * @return a promise that resolves to the editor when the stylesheet editor is
 * ready
 */
function waitForStyleEditor(toolbox, href) {
  let def = promise.defer();

  info("Waiting for the toolbox to switch to the styleeditor");
  toolbox.once("styleeditor-selected").then(() => {
    let panel = toolbox.getCurrentPanel();
    ok(panel && panel.UI, "Styleeditor panel switched to front");

    // A helper that resolves the promise once it receives an editor that
    // matches the expected href. Returns false if the editor was not correct.
    let gotEditor = (event, editor) => {
      let currentHref = editor.styleSheet.href;
      if (!href || (href && currentHref.endsWith(href))) {
        info("Stylesheet editor selected");
        panel.UI.off("editor-selected", gotEditor);

        editor.getSourceEditor().then(sourceEditor => {
          info("Stylesheet editor fully loaded");
          def.resolve(sourceEditor);
        });

        return true;
      }

      info("The editor was incorrect. Waiting for editor-selected event.");
      return false;
    };

    // The expected editor may already be selected. Check the if the currently
    // selected editor is the expected one and if not wait for an
    // editor-selected event.
    if (!gotEditor("styleeditor-selected", panel.UI.selectedEditor)) {
      // The expected editor is not selected (yet). Wait for it.
      panel.UI.on("editor-selected", gotEditor);
    }
  });

  return def.promise;
}

/**
 * @see SimpleTest.waitForClipboard
 *
 * @param {Function} setup
 *        Function to execute before checking for the
 *        clipboard content
 * @param {String|Function} expected
 *        An expected string or validator function
 * @return a promise that resolves when the expected string has been found or
 * the validator function has returned true, rejects otherwise.
 */
function waitForClipboard(setup, expected) {
  let def = promise.defer();
  SimpleTest.waitForClipboard(expected, setup, def.resolve, def.reject);
  return def.promise;
}

/**
 * Checks if document's active element is within the given element.
 * @param  {HTMLDocument}  doc document with active element in question
 * @param  {DOMNode}       container element tested on focus containment
 * @return {Boolean}
 */
function containsFocus(doc, container) {
  let elm = doc.activeElement;
  while (elm) {
    if (elm === container) {
      return true;
    }
    elm = elm.parentNode;
  }
  return false;
}
