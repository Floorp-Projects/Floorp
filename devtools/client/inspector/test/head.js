/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../framework/test/shared-head.js */
/* import-globals-from ../../shared/test/test-actor-registry.js */
/* import-globals-from ../../inspector/test/shared-head.js */
"use strict";

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/test/shared-head.js",
  this);

// Services.prefs.setBoolPref("devtools.debugger.log", true);
// SimpleTest.registerCleanupFunction(() => {
//   Services.prefs.clearUserPref("devtools.debugger.log");
// });

// Import helpers registering the test-actor in remote targets
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/test-actor-registry.js",
  this);

// Import helpers for the inspector that are also shared with others
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/shared-head.js",
  this);

const {LocalizationHelper} = require("devtools/shared/l10n");
const INSPECTOR_L10N =
      new LocalizationHelper("devtools/client/locales/inspector.properties");

flags.testing = true;
registerCleanupFunction(() => {
  flags.testing = false;
});

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.inspector.activeSidebar");
});

registerCleanupFunction(function* () {
  // Move the mouse outside inspector. If the test happened fake a mouse event
  // somewhere over inspector the pointer is considered to be there when the
  // next test begins. This might cause unexpected events to be emitted when
  // another test moves the mouse.
  EventUtils.synthesizeMouseAtPoint(1, 1, {type: "mousemove"}, window);
});

var navigateTo = Task.async(function* (inspector, url) {
  let markuploaded = inspector.once("markuploaded");
  let onNewRoot = inspector.once("new-root");
  let onUpdated = inspector.once("inspector-updated");

  info("Navigating to: " + url);
  let activeTab = inspector.toolbox.target.activeTab;
  yield activeTab.navigateTo(url);

  info("Waiting for markup view to load after navigation.");
  yield markuploaded;

  info("Waiting for new root.");
  yield onNewRoot;

  info("Waiting for inspector to update after new-root event.");
  yield onUpdated;
});

/**
 * Start the element picker and focus the content window.
 * @param {Toolbox} toolbox
 * @param {Boolean} skipFocus - Allow tests to bypass the focus event.
 */
var startPicker = Task.async(function* (toolbox, skipFocus) {
  info("Start the element picker");
  toolbox.win.focus();
  yield toolbox.highlighterUtils.startPicker();
  if (!skipFocus) {
    // By default make sure the content window is focused since the picker may not focus
    // the content window by default.
    yield ContentTask.spawn(gBrowser.selectedBrowser, null, function* () {
      content.focus();
    });
  }
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
 * Select node for a given selector, make it focusable and set focus in its
 * container element.
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector The current inspector-panel instance.
 * @return {MarkupContainer}
 */
function* focusNode(selector, inspector) {
  getContainerForNodeFront(inspector.walker.rootNode, inspector).elt.focus();
  let nodeFront = yield getNodeFront(selector, inspector);
  let container = getContainerForNodeFront(nodeFront, inspector);
  yield selectNode(nodeFront, inspector);
  EventUtils.sendKey("return", inspector.panelWin);
  return container;
}

/**
 * Set the inspector's current selection to null so that no node is selected
 *
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox
 * @return a promise that resolves when the inspector is updated
 */
function clearCurrentNodeSelection(inspector) {
  info("Clearing the current selection");
  let updated = inspector.once("inspector-updated");
  inspector.selection.setNodeFront(null);
  return updated;
}

/**
 * Open the inspector in a tab with given URL.
 * @param {string} url  The URL to open.
 * @param {String} hostType Optional hostType, as defined in Toolbox.HostType
 * @return A promise that is resolved once the tab and inspector have loaded
 *         with an object: { tab, toolbox, inspector }.
 */
var openInspectorForURL = Task.async(function* (url, hostType) {
  let tab = yield addTab(url);
  let { inspector, toolbox, testActor } = yield openInspector(hostType);
  return { tab, inspector, toolbox, testActor };
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
var clickOnInspectMenuItem = Task.async(function* (testActor, selector) {
  info("Showing the contextual menu on node " + selector);
  let contentAreaContextMenu = document.querySelector(
    "#contentAreaContextMenu");
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
 * Get the NodeFront for a node that matches a given css selector inside a
 * given iframe.
 * @param {String|NodeFront} selector
 * @param {String|NodeFront} frameSelector A selector that matches the iframe
 * the node is in
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return {Promise} Resolves when the inspector is updated with the new node
 */
var getNodeFrontInFrame = Task.async(function* (selector, frameSelector,
                                                inspector) {
  let iframe = yield getNodeFront(frameSelector, inspector);
  let {nodes} = yield inspector.walker.children(iframe);
  return inspector.walker.querySelector(nodes[0], selector);
});

var focusSearchBoxUsingShortcut = Task.async(function* (panelWin, callback) {
  info("Focusing search box");
  let searchBox = panelWin.document.getElementById("inspector-searchbox");
  let focused = once(searchBox, "focus");

  panelWin.focus();

  synthesizeKeyShortcut(INSPECTOR_L10N.getStr("inspector.searchHTML.key"));

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
 * @param {Boolean} Set to true in the event that the node shouldn't be found.
 * @return {MarkupContainer}
 */
var getContainerForSelector =
Task.async(function* (selector, inspector, expectFailure = false) {
  info("Getting the markup-container for node " + selector);
  let nodeFront = yield getNodeFront(selector, inspector);
  let container = getContainerForNodeFront(nodeFront, inspector);

  if (expectFailure) {
    ok(!container, "Shouldn't find markup-container for selector: " + selector);
  } else {
    ok(container, "Found markup-container for selector: " + selector);
  }

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
var hoverContainer = Task.async(function* (selector, inspector) {
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
var clickContainer = Task.async(function* (selector, inspector) {
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
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return a promise when done
 */
function mouseLeaveMarkupView(inspector) {
  info("Leaving the markup-view area");
  let def = defer();

  // Find another element to mouseover over in order to leave the markup-view
  let btn = inspector.toolbox.doc.querySelector("#toolbox-controls");

  EventUtils.synthesizeMouseAtCenter(btn, {type: "mousemove"},
    inspector.toolbox.win);
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
 * A helper that fetches a front for a node that matches the given selector or
 * doctype node if the selector is falsy.
 */
function* getNodeFrontForSelector(selector, inspector) {
  if (selector) {
    info("Retrieving front for selector " + selector);
    return getNodeFront(selector, inspector);
  }

  info("Retrieving front for doctype node");
  let {nodes} = yield inspector.walker.children(inspector.walker.rootNode);
  return nodes[0];
}

/**
 * A simple polling helper that executes a given function until it returns true.
 * @param {Function} check A generator function that is expected to return true at some
 * stage.
 * @param {String} desc A text description to be displayed when the polling starts.
 * @param {Number} attemptes Optional number of times we poll. Defaults to 10.
 * @param {Number} timeBetweenAttempts Optional time to wait between each attempt.
 * Defaults to 200ms.
 */
function* poll(check, desc, attempts = 10, timeBetweenAttempts = 200) {
  info(desc);

  for (let i = 0; i < attempts; i++) {
    if (yield check()) {
      return;
    }
    yield new Promise(resolve => setTimeout(resolve, timeBetweenAttempts));
  }

  throw new Error(`Timeout while: ${desc}`);
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
  function* ({inspector, testActor}) {
    let front = inspector.inspector;
    let highlighter = yield front.getHighlighterByType(type);

    let prefix = "";

    // Internals for mouse events
    let prevX, prevY;

    // Highlighted node
    let highlightedNode = null;

    return {
      set prefix(value) {
        prefix = value;
      },

      get highlightedNode() {
        if (!highlightedNode) {
          return null;
        }

        return {
          getComputedStyle: function* (options = {}) {
            return yield inspector.pageStyle.getComputed(
              highlightedNode, options);
          }
        };
      },

      show: function* (selector = ":root", options) {
        highlightedNode = yield getNodeFront(selector, inspector);
        return yield highlighter.show(highlightedNode, options);
      },

      hide: function* () {
        yield highlighter.hide();
      },

      isElementHidden: function* (id) {
        return (yield testActor.getHighlighterNodeAttribute(
          prefix + id, "hidden", highlighter)) === "true";
      },

      getElementTextContent: function* (id) {
        return yield testActor.getHighlighterNodeTextContent(
          prefix + id, highlighter);
      },

      getElementAttribute: function* (id, name) {
        return yield testActor.getHighlighterNodeAttribute(
          prefix + id, name, highlighter);
      },

      waitForElementAttributeSet: function* (id, name) {
        yield poll(function* () {
          let value = yield testActor.getHighlighterNodeAttribute(
            prefix + id, name, highlighter);
          return !!value;
        }, `Waiting for element ${id} to have attribute ${name} set`);
      },

      waitForElementAttributeRemoved: function* (id, name) {
        yield poll(function* () {
          let value = yield testActor.getHighlighterNodeAttribute(
            prefix + id, name, highlighter);
          return !value;
        }, `Waiting for element ${id} to have attribute ${name} removed`);
      },

      synthesizeMouse: function* (options) {
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
          function* (x = prevX, y = prevY) {
            prevX = x;
            prevY = y;
            yield testActor.synthesizeMouse({
              selector: ":root", x, y, options: {type: "mouse" + name}});
          }
      }),

      reflow: function* () {
        yield testActor.reflow();
      },

      finalize: function* () {
        highlightedNode = null;
        yield highlighter.finalize();
      }
    };
  }
);

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
  return null;
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
  let def = defer();
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
  let def = defer();

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

/**
 * Listen for a new tab to open and return a promise that resolves when one
 * does and completes the load event.
 *
 * @return a promise that resolves to the tab object
 */
var waitForTab = Task.async(function* () {
  info("Waiting for a tab to open");
  yield once(gBrowser.tabContainer, "TabOpen");
  let tab = gBrowser.selectedTab;
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  info("The tab load completed");
  return tab;
});

/**
 * Simulate the key input for the given input in the window.
 *
 * @param {String} input
 *        The string value to input
 * @param {Window} win
 *        The window containing the panel
 */
function synthesizeKeys(input, win) {
  for (let key of input.split("")) {
    EventUtils.synthesizeKey(key, {}, win);
  }
}

/**
 * Given a tooltip object instance (see Tooltip.js), checks if it is set to
 * toggle and hover and if so, checks if the given target is a valid hover
 * target. This won't actually show the tooltip (the less we interact with XUL
 * panels during test runs, the better).
 *
 * @return a promise that resolves when the answer is known
 */
function isHoverTooltipTarget(tooltip, target) {
  if (!tooltip._toggle._baseNode || !tooltip.panel) {
    return promise.reject(new Error(
      "The tooltip passed isn't set to toggle on hover or is not a tooltip"));
  }
  return tooltip._toggle.isValidHoverTarget(target);
}

/**
 * Same as isHoverTooltipTarget except that it will fail the test if there is no
 * tooltip defined on hover of the given element
 *
 * @return a promise
 */
function assertHoverTooltipOn(tooltip, element) {
  return isHoverTooltipTarget(tooltip, element).then(() => {
    ok(true, "A tooltip is defined on hover of the given element");
  }, () => {
    ok(false, "No tooltip is defined on hover of the given element");
  });
}

/**
 * Open the inspector menu and return all of it's items in a flat array
 * @param {InspectorPanel} inspector
 * @param {Object} options to pass into openMenu
 * @return An array of MenuItems
 */
function openContextMenuAndGetAllItems(inspector, options) {
  let menu = inspector._openMenu(options);

  // Flatten all menu items into a single array to make searching through it easier
  let allItems = [].concat.apply([], menu.items.map(function addItem(item) {
    if (item.submenu) {
      return addItem(item.submenu.items);
    }
    return item;
  }));

  return allItems;
}

/**
 * Get the rule editor from the rule-view given its index
 *
 * @param {CssRuleView} view
 *        The instance of the rule-view panel
 * @param {Number} childrenIndex
 *        The children index of the element to get
 * @param {Number} nodeIndex
 *        The child node index of the element to get
 * @return {DOMNode} The rule editor if any at this index
 */
function getRuleViewRuleEditor(view, childrenIndex, nodeIndex) {
  return nodeIndex !== undefined ?
    view.element.children[childrenIndex].childNodes[nodeIndex]._ruleEditor :
    view.element.children[childrenIndex]._ruleEditor;
}

/**
 * Get the text displayed for a given DOM Element's textContent within the
 * markup view.
 *
 * @param {String} selector
 * @param {InspectorPanel} inspector
 * @return {String} The text displayed in the markup view
 */
function* getDisplayedNodeTextContent(selector, inspector) {
  // We have to ensure that the textContent is displayed, for that the DOM
  // Element has to be selected in the markup view and to be expanded.
  yield selectNode(selector, inspector);

  let container = yield getContainerForSelector(selector, inspector);
  yield inspector.markup.expandNode(container.node);
  yield waitForMultipleChildrenUpdates(inspector);
  if (container) {
    let textContainer = container.elt.querySelector("pre");
    return textContainer.textContent;
  }
  return null;
}
