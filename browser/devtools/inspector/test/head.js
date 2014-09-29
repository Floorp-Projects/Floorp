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
const { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});

// All test are asynchronous
waitForExplicitFinish();

let tempScope = {};
Cu.import("resource://gre/modules/devtools/LayoutHelpers.jsm", tempScope);
let LayoutHelpers = tempScope.LayoutHelpers;

let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", tempScope);
let TargetFactory = devtools.TargetFactory;

Components.utils.import("resource://gre/modules/devtools/Console.jsm", tempScope);
let console = tempScope.console;

// Import the GCLI test helper
let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
Services.scriptloader.loadSubScript(testDir + "../../../commandline/test/helpers.js", this);

gDevTools.testing = true;
SimpleTest.registerCleanupFunction(() => {
  gDevTools.testing = false;
});

SimpleTest.registerCleanupFunction(() => {
  console.error("Here we are\n");
  let {DebuggerServer} = Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});
  console.error("DebuggerServer open connections: " + Object.getOwnPropertyNames(DebuggerServer._connections).length);

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
let addTab = Task.async(function* (url) {
  info("Adding a new tab with URL: '" + url + "'");
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  let loaded = once(gBrowser.selectedBrowser, "load", true);

  content.location = url;
  yield loaded;

  info("URL '" + url + "' loading complete");

  let def = promise.defer();
  let isBlank = url == "about:blank";
  waitForFocus(def.resolve, content, isBlank);

  yield def.promise;

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
 * @param {String|DOMNode} nodeOrSelector
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox
 * @return a promise that resolves when the inspector is updated with the new
 * node
 */
function selectAndHighlightNode(nodeOrSelector, inspector) {
  info("Highlighting and selecting the node " + nodeOrSelector);

  let node = getNode(nodeOrSelector);
  let updated = inspector.toolbox.once("highlighter-ready");
  inspector.selection.setNode(node, "test-highlight");
  return updated;

}

/**
 * Set the inspector's current selection to a node or to the first match of the
 * given css selector.
 * @param {String|DOMNode|NodeFront} nodeOrSelector
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox
 * @param {String} reason
 *        Defaults to "test" which instructs the inspector not to highlight the
 *        node upon selection
 * @return a promise that resolves when the inspector is updated with the new
 * node
 */
function selectNode(nodeOrSelector, inspector, reason="test") {
  info("Selecting the node " + nodeOrSelector);

  let node = getNode(nodeOrSelector);
  let updated = inspector.once("inspector-updated");
  if (node._form) {
    inspector.selection.setNodeFront(node, reason);
  } else {
    inspector.selection.setNode(node, reason);
  }
  return updated;
}

/**
 * Open the inspector in a tab with given URL.
 * @param {string} url  The URL to open.
 * @return A promise that is resolved once the tab and inspector have loaded
 *         with an object: { tab, toolbox, inspector }.
 */
let openInspectorForURL = Task.async(function* (url) {
  let tab = yield addTab(url);
  let { inspector, toolbox } = yield openInspector();
  return { tab, inspector, toolbox };
});

/**
 * Open the toolbox, with the inspector tool visible.
 * @param {Function} cb Optional callback, if you don't want to use the returned
 * promise
 * @return a promise that resolves when the inspector is ready
 */
let openInspector = Task.async(function*(cb) {
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
  toolbox = yield gDevTools.showToolbox(target, "inspector");
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

function getActiveInspector()
{
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  return gDevTools.getToolbox(target).getPanel("inspector");
}

function getNodeFront(node)
{
  let inspector = getActiveInspector();
  return inspector.walker.frontForRawNode(node);
}

function getHighlighter()
{
  return gBrowser.selectedBrowser.parentNode.querySelector(".highlighter-container");
}

function getSimpleBorderRect() {
  let {p1, p2, p3, p4} = getBoxModelStatus().border.points;

  return {
    top: p1.y,
    left: p1.x,
    width: p2.x - p1.x,
    height: p4.y - p1.y
  };
}

function getBoxModelRoot() {
  let highlighter = getHighlighter();
  return highlighter.querySelector(".box-model-root");
}

function getBoxModelStatus() {
  let root = getBoxModelRoot();
  let inspector = getActiveInspector();

  return {
    visible: !root.hasAttribute("hidden"),
    currentNode: inspector.walker.currentNode,
    margin: {
      points: getPointsForRegion("margin"),
      visible: isRegionHidden("margin")
    },
    border: {
      points: getPointsForRegion("border"),
      visible: isRegionHidden("border")
    },
    padding: {
      points: getPointsForRegion("padding"),
      visible: isRegionHidden("padding")
    },
    content: {
      points: getPointsForRegion("content"),
      visible: isRegionHidden("content")
    },
    guides: {
      top: getGuideStatus("top"),
      right: getGuideStatus("right"),
      bottom: getGuideStatus("bottom"),
      left: getGuideStatus("left")
    }
  };
}

function getGuideStatus(location) {
  let root = getBoxModelRoot();
  let guide = root.querySelector(".box-model-guide-" + location);

  return {
    visible: !guide.hasAttribute("hidden"),
    x1: guide.getAttribute("x1"),
    y1: guide.getAttribute("y1"),
    x2: guide.getAttribute("x2"),
    y2: guide.getAttribute("y2")
  };
}

function getPointsForRegion(region) {
  let root = getBoxModelRoot();
  let box = root.querySelector(".box-model-" + region);
  let points = box.getAttribute("points").split(/[, ]/);

  // We multiply each value by 1 to cast it into a number
  return {
    p1: {
      x: parseFloat(points[0]),
      y: parseFloat(points[1])
    },
    p2: {
      x: parseFloat(points[2]),
      y: parseFloat(points[3])
    },
    p3: {
      x: parseFloat(points[4]),
      y: parseFloat(points[5])
    },
    p4: {
      x: parseFloat(points[6]),
      y: parseFloat(points[7])
    }
  };
}

function isRegionHidden(region) {
  let root = getBoxModelRoot();
  let box = root.querySelector(".box-model-" + region);

  return !box.hasAttribute("hidden");
}

function isHighlighting()
{
  let root = getBoxModelRoot();
  return !root.hasAttribute("hidden");
}

/**
 * Observes mutation changes on the box-model highlighter and returns a promise
 * that resolves when one of the attributes changes.
 * If an attribute changes in the box-model, it means its position/dimensions
 * got updated
 */
function waitForBoxModelUpdate() {
  let def = promise.defer();

  let root = getBoxModelRoot();
  let polygon = root.querySelector(".box-model-content");
  let observer = new polygon.ownerDocument.defaultView.MutationObserver(() => {
    observer.disconnect();
    def.resolve();
  });
  observer.observe(polygon, {attributes: true});

  return def.promise;
}

function getHighlitNode()
{
  if (isHighlighting()) {
    let helper = new LayoutHelpers(window.content);
    let points = getBoxModelStatus().content.points;
    let x = (points.p1.x + points.p2.x + points.p3.x + points.p4.x) / 4;
    let y = (points.p1.y + points.p2.y + points.p3.y + points.p4.y) / 4;

    return helper.getElementFromPoint(window.content.document, x, y);
  }
}

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

function isNodeCorrectlyHighlighted(node, prefix="") {
  let boxModel = getBoxModelStatus();
  let helper = new LayoutHelpers(window.content);

  prefix += (prefix ? " " : "") + node.nodeName;
  prefix += (node.id ? "#" + node.id : "");
  prefix += (node.classList.length ? "." + [...node.classList].join(".") : "");
  prefix += " ";

  for (let boxType of ["content", "padding", "border", "margin"]) {
    let quads = helper.getAdjustedQuads(node, boxType);
    for (let point in boxModel[boxType].points) {
      is(boxModel[boxType].points[point].x, quads[point].x,
        prefix + boxType + " point " + point + " x coordinate is correct");
      is(boxModel[boxType].points[point].y, quads[point].y,
        prefix + boxType + " point " + point + " y coordinate is correct");
    }
  }
}

function getContainerForRawNode(markupView, rawNode)
{
  let front = markupView.walker.frontForRawNode(rawNode);
  let container = markupView.getContainer(front);
  return container;
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
