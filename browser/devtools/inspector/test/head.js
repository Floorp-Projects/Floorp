/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Cu = Components.utils;
const Ci = Components.interfaces;
const Cc = Components.classes;

// Services.prefs.setBoolPref("devtools.debugger.log", true);
// SimpleTest.registerCleanupFunction(() => {
//   Services.prefs.clearUserPref("devtools.debugger.log");
// });

// Uncomment this pref to dump all devtools emitted events to the console.
// Services.prefs.setBoolPref("devtools.dump.emit", true);

const TEST_URL_ROOT = "http://example.com/browser/browser/devtools/inspector/test/";
const ROOT_TEST_DIR = getRootDirectory(gTestPath);
const FRAME_SCRIPT_URL = ROOT_TEST_DIR + "doc_frame_script.js";

// All test are asynchronous
waitForExplicitFinish();

let {require} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let {TargetFactory} = require("devtools/framework/target");
let {console} = Cu.import("resource://gre/modules/devtools/Console.jsm", {});
let DevToolsUtils = require("devtools/toolkit/DevToolsUtils");
let promise = require("promise");

// Import the GCLI test helper
let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
Services.scriptloader.loadSubScript(testDir + "../../../commandline/test/helpers.js", this);

DevToolsUtils.testing = true;
registerCleanupFunction(() => {
  DevToolsUtils.testing = false;
});

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.dump.emit");
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

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url The url to be loaded in the new tab
 * @return a promise that resolves to the tab object when the url is loaded
 */
let addTab = Task.async(function* (url) {
  info("Adding a new tab with URL: '" + url + "'");

  window.focus();

  let tab = gBrowser.selectedTab = gBrowser.addTab(url);
  let browser = tab.linkedBrowser;

  info("Loading the helper frame script " + FRAME_SCRIPT_URL);
  browser.messageManager.loadFrameScript(FRAME_SCRIPT_URL, false);

  yield once(browser, "load", true);
  info("URL '" + url + "' loading complete");

  return tab;
});

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
let selectNode = Task.async(function*(selector, inspector, reason="test") {
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
let openInspectorForURL = Task.async(function*(url, hostType) {
  let tab = yield addTab(url);
  let { inspector, toolbox } = yield openInspector(null, hostType);
  return { tab, inspector, toolbox };
});

/**
 * Open the toolbox, with the inspector tool visible.
 * @param {Function} cb Optional callback, if you don't want to use the returned
 * promise
 * @param {String} hostType Optional hostType, as defined in Toolbox.HostType
 * @return a promise that resolves when the inspector is ready
 */
let openInspector = Task.async(function*(cb, hostType) {
  info("Opening the inspector");
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  let inspector, toolbox;

  // Checking if the toolbox and the inspector are already loaded
  // The inspector-updated event should only be waited for if the inspector
  // isn't loaded yet
  toolbox = gDevTools.getToolbox(target);
  if (toolbox) {
    inspector = toolbox.getPanel("inspector");
    if (inspector) {
      info("Toolbox and inspector already open");
      if (cb) {
        return cb(inspector, toolbox);
      } else {
        return {
          toolbox: toolbox,
          inspector: inspector
        };
      }
    }
  }

  info("Opening the toolbox");
  toolbox = yield gDevTools.showToolbox(target, "inspector", hostType);
  yield waitForToolboxFrameFocus(toolbox);
  inspector = toolbox.getPanel("inspector");

  info("Waiting for the inspector to update");
  yield inspector.once("inspector-updated");

  if (cb) {
    return cb(inspector, toolbox);
  } else {
    return {
      toolbox: toolbox,
      inspector: inspector
    };
  }
});

/**
 * Wait for the toolbox frame to receive focus after it loads
 * @param {Toolbox} toolbox
 * @return a promise that resolves when focus has been received
 */
function waitForToolboxFrameFocus(toolbox) {
  info("Making sure that the toolbox's frame is focused");
  let def = promise.defer();
  let win = toolbox.frame.contentWindow;
  waitForFocus(def.resolve, win);
  return def.promise;
}

function getActiveInspector() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  return gDevTools.getToolbox(target).getPanel("inspector");
}

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
let getNodeFrontInFrame = Task.async(function*(selector, frameSelector,
                                               inspector, reason="test") {
  let iframe = yield getNodeFront(frameSelector, inspector);
  let {nodes} = yield inspector.walker.children(iframe);
  return inspector.walker.querySelector(nodes[0], selector);
});

/**
 * Get the current rect of the border region of the box-model highlighter
 */
let getSimpleBorderRect = Task.async(function*(toolbox) {
  let {border} = yield getBoxModelStatus(toolbox);
  let {p1, p2, p3, p4} = border.points;

  return {
    top: p1.y,
    left: p1.x,
    width: p2.x - p1.x,
    height: p4.y - p1.y
  };
});

function getHighlighterActorID(highlighter) {
  let actorID = highlighter.actorID;
  let connPrefix = actorID.substring(0, actorID.indexOf(highlighter.typeName));

  return {actorID, connPrefix};
}

/**
 * Get the current positions and visibility of the various box-model highlighter
 * elements.
 */
let getBoxModelStatus = Task.async(function*(toolbox) {
  let isVisible = yield isHighlighting(toolbox);

  let ret = {
    visible: isVisible
  };

  for (let region of ["margin", "border", "padding", "content"]) {
    let points = yield getPointsForRegion(region, toolbox);
    let visible = yield isRegionHidden(region, toolbox);
    ret[region] = {points, visible};
  }

  ret.guides = {};
  for (let guide of ["top", "right", "bottom", "left"]) {
    ret.guides[guide] = yield getGuideStatus(guide, toolbox);
  }

  return ret;
});

/**
 * Get data about one of the toolbox box-model highlighter's guides.
 * @param {String} location One of top, right, bottom, left.
 * @param {Toolbox} toolbox The toolbox instance, used to retrieve the highlighter.
 * @return {Object} The returned object has the following form:
 * - visible {Boolean} Whether that guide is visible.
 * - x1/y1/x2/y2 {String} The <line>'s coordinates.
 */
let getGuideStatus = Task.async(function*(location, {highlighter}) {
  let id = "box-model-guide-" + location;

  let hidden = yield getHighlighterNodeAttribute(highlighter, id, "hidden");
  let x1 = yield getHighlighterNodeAttribute(highlighter, id, "x1");
  let y1 = yield getHighlighterNodeAttribute(highlighter, id, "y1");
  let x2 = yield getHighlighterNodeAttribute(highlighter, id, "x2");
  let y2 = yield getHighlighterNodeAttribute(highlighter, id, "y2");

  return {
    visible: !hidden,
    x1: x1,
    y1: y1,
    x2: x2,
    y2: y2
  };
});

/**
 * Get the coordinates of the rectangle that is defined by the 4 guides displayed
 * in the toolbox box-model highlighter.
 * @param {Toolbox} toolbox The toolbox instance, used to retrieve the highlighter.
 * @return {Object} Null if at least one guide is hidden. Otherwise an object
 * with p1, p2, p3, p4 properties being {x, y} objects.
 */
let getGuidesRectangle = Task.async(function*(toolbox) {
  let tGuide = yield getGuideStatus("top", toolbox);
  let rGuide = yield getGuideStatus("right", toolbox);
  let bGuide = yield getGuideStatus("bottom", toolbox);
  let lGuide = yield getGuideStatus("left", toolbox);

  if (!tGuide.visible || !rGuide.visible || !bGuide.visible || !lGuide.visible) {
    return null;
  }

  return {
    p1: {x: lGuide.x1, y: tGuide.y1},
    p2: {x: rGuide.x1, y: tGuide. y1},
    p3: {x: rGuide.x1, y: bGuide.y1},
    p4: {x: lGuide.x1, y: bGuide.y1}
  };
});

/**
 * Get the coordinate (points defined by the d attribute) from one of the path
 * elements in the box model highlighter.
 */
let getPointsForRegion = Task.async(function*(region, toolbox) {
  let d = yield getHighlighterNodeAttribute(toolbox.highlighter,
                                            "box-model-" + region, "d");
  let polygons = d.match(/M[^M]+/g);
  if (!polygons) {
    return null;
  }

  let points = polygons[0].trim().split(" ").map(i => {
    return i.replace(/M|L/, "").split(",")
  });

  return {
    p1: {
      x: parseFloat(points[0][0]),
      y: parseFloat(points[0][1])
    },
    p2: {
      x: parseFloat(points[1][0]),
      y: parseFloat(points[1][1])
    },
    p3: {
      x: parseFloat(points[2][0]),
      y: parseFloat(points[2][1])
    },
    p4: {
      x: parseFloat(points[3][0]),
      y: parseFloat(points[3][1])
    }
  };
});

/**
 * Is a given region path element of the box-model highlighter currently
 * hidden?
 */
let isRegionHidden = Task.async(function*(region, toolbox) {
  let value = yield getHighlighterNodeAttribute(toolbox.highlighter,
                                                "box-model-" + region, "hidden");
  return value !== null;
});

/**
 * Is the highlighter currently visible on the page?
 */
let isHighlighting = Task.async(function*(toolbox) {
  let value = yield getHighlighterNodeAttribute(toolbox.highlighter,
                                                "box-model-elements", "hidden");
  return value === null;
});

let getHighlitNode = Task.async(function*(toolbox) {
  let {visible, content} = yield getBoxModelStatus(toolbox);
  let points = content.points;
  if (visible) {
    let x = (points.p1.x + points.p2.x + points.p3.x + points.p4.x) / 4;
    let y = (points.p1.y + points.p2.y + points.p3.y + points.p4.y) / 4;

    let {objects} = yield executeInContent("Test:ElementFromPoint", {x, y});
    return objects.element;
  }
});

/**
 * Assert that the box-model highlighter's current position corresponds to the
 * given node boxquads.
 * @param {String} selector The selector for the node to get the boxQuads from
 * @param {String} prefix An optional prefix for logging information to the
 * console.
 */
let isNodeCorrectlyHighlighted = Task.async(function*(selector, toolbox, prefix="") {
  let boxModel = yield getBoxModelStatus(toolbox);
  let {data: regions} = yield executeInContent("Test:GetAllAdjustedQuads",
                                               {selector});

  for (let boxType of ["content", "padding", "border", "margin"]) {
    let [quad] = regions[boxType];
    for (let point in boxModel[boxType].points) {
      is(boxModel[boxType].points[point].x, quad[point].x,
        "Node " + selector + " " + boxType + " point " + point +
        " x coordinate is correct");
      is(boxModel[boxType].points[point].y, quad[point].y,
        "Node " + selector + " " + boxType + " point " + point +
        " y coordinate is correct");
    }
  }
});

function synthesizeKeyFromKeyTag(aKeyId, aDocument = null) {
  let document = aDocument || document;
  let key = document.getElementById(aKeyId);
  isnot(key, null, "Successfully retrieved the <key> node");

  let modifiersAttr = key.getAttribute("modifiers");

  let name = null;

  if (key.getAttribute("keycode"))
    name = key.getAttribute("keycode");
  else if (key.getAttribute("key"))
    name = key.getAttribute("key");

  isnot(name, null, "Successfully retrieved keycode/key");

  let modifiers = {
    shiftKey: modifiersAttr.match("shift"),
    ctrlKey: modifiersAttr.match("ctrl"),
    altKey: modifiersAttr.match("alt"),
    metaKey: modifiersAttr.match("meta"),
    accelKey: modifiersAttr.match("accel")
  }

  EventUtils.synthesizeKey(name, modifiers);
}

let focusSearchBoxUsingShortcut = Task.async(function* (panelWin, callback) {
  info("Focusing search box");
  let searchBox = panelWin.document.getElementById("inspector-searchbox");
  let focused = once(searchBox, "focus");

  panelWin.focus();
  synthesizeKeyFromKeyTag("nodeSearchKey", panelWin.document);

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
let getContainerForSelector = Task.async(function*(selector, inspector) {
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
let hoverContainer = Task.async(function*(selector, inspector) {
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
let clickContainer = Task.async(function*(selector, inspector) {
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
 * Zoom the current page to a given level.
 * @param {Number} level The new zoom level.
 * @param {String} actorID Optional highlighter actor ID. If provided, the
 * returned promise will only resolve when the highlighter has updated to the
 * new zoom level.
 * @return {Promise}
 */
let zoomPageTo = Task.async(function*(level, actorID, connPrefix) {
  yield executeInContent("Test:ChangeZoomLevel",
                         {level, actorID, connPrefix});
});

/**
 * Get the value of an attribute on one of the highlighter's node.
 * @param {Front} highlighter The front of the highlighter.
 * @param {String} nodeID The Id of the node in the highlighter.
 * @param {String} name The name of the attribute.
 * @return {String} value
 */
let getHighlighterNodeAttribute = Task.async(function*(highlighter, nodeID, name) {
  let {actorID, connPrefix} = getHighlighterActorID(highlighter);
  let {data: value} = yield executeInContent("Test:GetHighlighterAttribute",
                                             {nodeID, name, actorID, connPrefix});
  return value;
});

/**
 * Get the "d" attribute value for one of the box-model highlighter's region
 * <path> elements, and parse it to a list of points.
 * @param {String} region The box model region name.
 * @param {Front} highlighter The front of the highlighter.
 * @return {Object} The object returned has the following form:
 * - d {String} the d attribute value
 * - points {Array} an array of all the polygons defined by the path. Each box
 *   is itself an Array of points, themselves being [x,y] coordinates arrays.
 */
let getHighlighterRegionPath = Task.async(function*(region, highlighter) {
  let d = yield getHighlighterNodeAttribute(highlighter, "box-model-" + region, "d");
  if (!d) {
    return {d: null};
  }

  let polygons = d.match(/M[^M]+/g);
  if (!polygons) {
    return {d};
  }

  let points = [];
  for (let polygon of polygons) {
    points.push(polygon.trim().split(" ").map(i => {
      return i.replace(/M|L/, "").split(",")
    }));
  }

  return {d, points};
});

/**
 * Get the textContent value of one of the highlighter's node.
 * @param {Front} highlighter The front of the highlighter.
 * @param {String} nodeID The Id of the node in the highlighter.
 * @return {String} value
 */
let getHighlighterNodeTextContent = Task.async(function*(highlighter, nodeID) {
  let {actorID, connPrefix} = getHighlighterActorID(highlighter);
  let {data: value} = yield executeInContent("Test:GetHighlighterTextContent",
                                             {nodeID, actorID, connPrefix});
  return value;
});

/**
 * Subscribe to a given highlighter event and return a promise that resolves
 * when the event is received.
 * @param {String} event The name of the highlighter event to listen to.
 * @param {Front} highlighter The front of the highlighter.
 * @return {Promise}
 */
function waitForHighlighterEvent(event, highlighter) {
  let {actorID, connPrefix} = getHighlighterActorID(highlighter);
  return executeInContent("Test:WaitForHighlighterEvent",
                          {event, actorID, connPrefix});
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
 * Wait for eventName on target.
 * @param {Object} target An observable object that either supports on/off or
 * addEventListener/removeEventListener
 * @param {String} eventName
 * @param {Boolean} useCapture Optional, for addEventListener/removeEventListener
 * @return A promise that resolves when the event has been handled
 */
function once(target, eventName, useCapture=false) {
  info("Waiting for event: '" + eventName + "' on " + target + ".");

  let deferred = promise.defer();

  for (let [add, remove] of [
    ["addEventListener", "removeEventListener"],
    ["addListener", "removeListener"],
    ["on", "off"]
  ]) {
    if ((add in target) && (remove in target)) {
      target[add](eventName, function onEvent(...aArgs) {
        info("Got event: '" + eventName + "' on " + target + ".");
        target[remove](eventName, onEvent, useCapture);
        deferred.resolve.apply(deferred, aArgs);
      }, useCapture);
      break;
    }
  }

  return deferred.promise;
}

/**
 * Wait for a content -> chrome message on the message manager (the window
 * messagemanager is used).
 * @param {String} name The message name
 * @return {Promise} A promise that resolves to the response data when the
 * message has been received
 */
function waitForContentMessage(name) {
  let mm = gBrowser.selectedBrowser.messageManager;

  let def = promise.defer();
  mm.addMessageListener(name, function onMessage(msg) {
    mm.removeMessageListener(name, onMessage);
    def.resolve(msg);
  });
  return def.promise;
}

function wait(ms) {
  let def = promise.defer();
  setTimeout(def.resolve, ms);
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
 * Send an async message to the frame script (chrome -> content) and wait for a
 * response message with the same name (content -> chrome).
 * @param {String} name The message name. Should be one of the messages defined
 * in doc_frame_script.js
 * @param {Object} data Optional data to send along
 * @param {Object} objects Optional CPOW objects to send along
 * @param {Boolean} expectResponse If set to false, don't wait for a response
 * with the same name from the content script. Defaults to true.
 * @return {Promise} Resolves to the response data if a response is expected,
 * immediately resolves otherwise
 */
function executeInContent(name, data={}, objects={}, expectResponse=true) {
  let mm = gBrowser.selectedBrowser.messageManager;

  mm.sendAsyncMessage(name, data, objects);
  if (expectResponse) {
    return waitForContentMessage(name);
  } else {
    return promise.resolve();
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
