/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// A helper frame-script for brower/devtools/inspector tests.
//
// Most listeners in the script expect "Test:"-namespaced messages from chrome,
// then execute code upon receiving, and immediately send back a message.
// This is so that chrome test code can execute code in content and wait for a
// response.
// Some listeners do not send a response message back.

let {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
let {LayoutHelpers} = Cu.import("resource://gre/modules/devtools/LayoutHelpers.jsm", {});
let DOMUtils = Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
            .getService(Ci.mozIJSSubScriptLoader);
let EventUtils = {};
loader.loadSubScript("chrome://marionette/content/EventUtils.js", EventUtils);

/**
 * If the test page creates and triggeres the custom event
 * "test-page-processing-done", then the Test:TestPageProcessingDone message
 * will be sent to the parent process for tests to wait for this event if needed.
 */
addEventListener("DOMWindowCreated", () => {
  content.addEventListener("test-page-processing-done", () => {
    sendAsyncMessage("Test:TestPageProcessingDone");
  }, false);
});

/**
 * Given an actorID and connection prefix, get the corresponding actor from the
 * debugger-server connection.
 * @param {String} actorID
 * @param {String} connPrefix
 */
function getHighlighterActor(actorID, connPrefix) {
  let {DebuggerServer} = Cu.import("resource://gre/modules/devtools/dbg-server.jsm");
  if (!DebuggerServer.initialized) {
    return;
  }

  let conn = DebuggerServer._connections[connPrefix];
  if (!conn) {
    return;
  }

  return conn.getActor(actorID);
}

/**
 * Get the instance of CanvasFrameAnonymousContentHelper used by a given
 * highlighter actor.
 * The instance provides methods to get/set attributes/text/style on nodes of
 * the highlighter, inserted into the nsCanvasFrame.
 * @see /toolkit/devtools/server/actors/highlighter.js
 * @param {String} actorID
 * @param {String} connPrefix
 */
function getHighlighterCanvasFrameHelper(actorID, connPrefix) {
  let actor = getHighlighterActor(actorID, connPrefix);
  if (actor && actor._highlighter) {
    return actor._highlighter.markup;
  }
}

/**
 * Get a value for a given attribute name, on one of the elements of the box
 * model highlighter, given its ID.
 * @param {Object} msg The msg.data part expects the following properties
 * - {String} nodeID The full ID of the element to get the attribute for
 * - {String} name The name of the attribute to get
 * - {String} actorID The highlighter actor ID
 * - {String} connPrefix The highlighter actor ID's connection prefix
 * @return {String} The value, if found, null otherwise
 */
addMessageListener("Test:GetHighlighterAttribute", function(msg) {
  let {nodeID, name, actorID, connPrefix} = msg.data;

  let value;
  let helper = getHighlighterCanvasFrameHelper(actorID, connPrefix);
  if (helper) {
    value = helper.getAttributeForElement(nodeID, name);
  }

  sendAsyncMessage("Test:GetHighlighterAttribute", value);
});

/**
 * Get the textcontent of one of the elements of the box model highlighter,
 * given its ID.
 * @param {Object} msg The msg.data part expects the following properties
 * - {String} nodeID The full ID of the element to get the attribute for
 * - {String} actorID The highlighter actor ID
 * - {String} connPrefix The highlighter connection prefix
 * @return {String} The textcontent value
 */
addMessageListener("Test:GetHighlighterTextContent", function(msg) {
  let {nodeID, actorID, connPrefix} = msg.data;

  let value;
  let helper = getHighlighterCanvasFrameHelper(actorID, connPrefix);
  if (helper) {
    value = helper.getTextContentForElement(nodeID);
  }

  sendAsyncMessage("Test:GetHighlighterTextContent", value);
});

/**
 * Get the number of box-model highlighters created by the SelectorHighlighter
 * @param {Object} msg The msg.data part expects the following properties:
 * - {String} actorID The highlighter actor ID
 * - {String} connPrefix The highlighter connection prefix
 * @return {Number} The number of box-model highlighters created, or null if the
 * SelectorHighlighter was not found.
 */
addMessageListener("Test:GetSelectorHighlighterBoxNb", function(msg) {
  let {actorID, connPrefix} = msg.data;
  let {_highlighter: h} = getHighlighterActor(actorID, connPrefix);
  if (!h || !h._highlighters) {
    sendAsyncMessage("Test:GetSelectorHighlighterBoxNb", null);
  } else {
    sendAsyncMessage("Test:GetSelectorHighlighterBoxNb", h._highlighters.length);
  }
});

/**
 * Subscribe to the box-model highlighter's update event, modify an attribute of
 * the currently highlighted node and send a message when the highlighter has
 * updated.
 * @param {Object} msg The msg.data part expects the following properties
 * - {String} the name of the attribute to be changed
 * - {String} the new value for the attribute
 * - {String} actorID The highlighter actor ID
 * - {String} connPrefix The highlighter connection prefix
 */
addMessageListener("Test:ChangeHighlightedNodeWaitForUpdate", function(msg) {
  // The name and value of the attribute to be changed
  let {name, value, actorID, connPrefix} = msg.data;
  let {_highlighter: h} = getHighlighterActor(actorID, connPrefix);

  h.once("updated", () => {
    sendAsyncMessage("Test:ChangeHighlightedNodeWaitForUpdate");
  });

  h.currentNode.setAttribute(name, value);
});

/**
 * Subscribe to a given highlighter event and respond when the event is received.
 * @param {Object} msg The msg.data part expects the following properties
 * - {String} event The name of the highlighter event to listen to
 * - {String} actorID The highlighter actor ID
 * - {String} connPrefix The highlighter connection prefix
 */
addMessageListener("Test:WaitForHighlighterEvent", function(msg) {
  let {event, actorID, connPrefix} = msg.data;
  let {_highlighter: h} = getHighlighterActor(actorID, connPrefix);

  h.once(event, () => {
    sendAsyncMessage("Test:WaitForHighlighterEvent");
  });
});

/**
 * Change the zoom level of the page.
 * Optionally subscribe to the box-model highlighter's update event and waiting
 * for it to refresh before responding.
 * @param {Object} msg The msg.data part expects the following properties
 * - {Number} level The new zoom level
 * - {String} actorID Optional. The highlighter actor ID
 * - {String} connPrefix Optional. The highlighter connection prefix
 */
addMessageListener("Test:ChangeZoomLevel", function(msg) {
  let {level, actorID, connPrefix} = msg.data;
  dumpn("Zooming page to " + level);

  if (actorID) {
    let {_highlighter: h} = getHighlighterActor(actorID, connPrefix);
    h.once("updated", () => {
      sendAsyncMessage("Test:ChangeZoomLevel");
    });
  }

  let docShell = content.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebNavigation)
                        .QueryInterface(Ci.nsIDocShell);
  docShell.contentViewer.fullZoom = level;

  if (!actorID) {
    sendAsyncMessage("Test:ChangeZoomLevel");
  }
});

/**
 * Get the element at the given x/y coordinates.
 * @param {Object} msg The msg.data part expects the following properties
 * - {Number} x
 * - {Number} y
 * @return {DOMNode} The CPOW of the element
 */
addMessageListener("Test:ElementFromPoint", function(msg) {
  let {x, y} = msg.data;
  dumpn("Getting the element at " + x + "/" + y);

  let helper = new LayoutHelpers(content);
  let element = helper.getElementFromPoint(content.document, x, y);
  sendAsyncMessage("Test:ElementFromPoint", null, {element});
});

/**
 * Get all box-model regions' adjusted boxquads for the given element
 * @param {Object} msg The msg.data part should contain the node selector.
 * @return {Object} An object with each property being a box-model region, each
 * of them being an array of objects with the p1/p2/p3/p4 properties.
 */
addMessageListener("Test:GetAllAdjustedQuads", function(msg) {
  let {selector} = msg.data;
  let node = superQuerySelector(selector);

  let regions = {};

  let helper = new LayoutHelpers(content);
  for (let boxType of ["content", "padding", "border", "margin"]) {
    regions[boxType] = helper.getAdjustedQuads(node, boxType);
  }

  sendAsyncMessage("Test:GetAllAdjustedQuads", regions);
});

/**
 * Synthesize a mouse event on an element. This handler doesn't send a message
 * back. Consumers should listen to specific events on the inspector/highlighter
 * to know when the event got synthesized.
 * @param {Object} msg The msg.data part expects the following properties:
 * - {Number} x
 * - {Number} y
 * - {Boolean} center If set to true, x/y will be ignored and
 *             synthesizeMouseAtCenter will be used instead
 * - {Object} options Other event options
 * - {String} selector An optional selector that will be used to find the node to
 *            synthesize the event on, if msg.objects doesn't contain the CPOW.
 * The msg.objects part should be the element.
 * @param {Object} data Event detail properties:
 */
addMessageListener("Test:SynthesizeMouse", function(msg) {
  let {x, y, center, options, selector} = msg.data;
  let {node} = msg.objects;

  if (!node && selector) {
    node = superQuerySelector(selector);
  }

  if (center) {
    EventUtils.synthesizeMouseAtCenter(node, options, node.ownerDocument.defaultView);
  } else {
    EventUtils.synthesizeMouse(node, x, y, options, node.ownerDocument.defaultView);
  }

  // Most consumers won't need to listen to this message, unless they want to
  // wait for the mouse event to be synthesized and don't have another event
  // to listen to instead.
  sendAsyncMessage("Test:SynthesizeMouse");
});

/**
 * Synthesize a key event for an element. This handler doesn't send a message
 * back. Consumers should listen to specific events on the inspector/highlighter
 * to know when the event got synthesized.
 * @param  {Object} msg The msg.data part expects the following properties:
 * - {String} key
 * - {Object} options
 */
addMessageListener("Test:SynthesizeKey", function(msg) {
  let {key, options} = msg.data;

  EventUtils.synthesizeKey(key, options, content);
});

/**
 * Check that an element currently has a pseudo-class lock.
 * @param {Object} msg The msg.data part expects the following properties:
 * - {String} pseudo The pseudoclass to check for
 * The msg.objects part should be the element.
 * @param {Object}
 * @return {Boolean}
 */
addMessageListener("Test:HasPseudoClassLock", function(msg) {
  let {node} = msg.objects;
  let {pseudo} = msg.data
  sendAsyncMessage("Test:HasPseudoClassLock", DOMUtils.hasPseudoClassLock(node, pseudo));
});

/**
 * Scrolls the window to a particular set of coordinates in the document, or
 * by the given amount if `relative` is set to `true`.
 *
 * @param {Object} data
 * - {Number} x
 * - {Number} y
 * - {Boolean} relative
 *
 * @return {Object} An object with x / y properties, representing the number
 * of pixels that the document has been scrolled horizontally and vertically.
 */
addMessageListener("Test:ScrollWindow", function(msg) {
  let {x, y, relative} = msg.data;

  if (isNaN(x) || isNaN(y)) {
    sendAsyncMessage("Test:ScrollWindow", {});
    return;
  }

  content.addEventListener("scroll", function onScroll(event) {
    this.removeEventListener("scroll", onScroll);

    let data = {x: content.scrollX, y: content.scrollY};
    sendAsyncMessage("Test:ScrollWindow", data);
  });

  content[relative ? "scrollBy" : "scrollTo"](x, y);
});

/**
 * Like document.querySelector but can go into iframes too.
 * ".container iframe || .sub-container div" will first try to find the node
 * matched by ".container iframe" in the root document, then try to get the
 * content document inside it, and then try to match ".sub-container div" inside
 * this document.
 * Any selector coming before the || separator *MUST* match a frame node.
 * @param {String} superSelector.
 * @return {DOMNode} The node, or null if not found.
 */
function superQuerySelector(superSelector, root=content.document) {
  let frameIndex = superSelector.indexOf("||");
  if (frameIndex === -1) {
    return root.querySelector(superSelector);
  } else {
    let rootSelector = superSelector.substring(0, frameIndex).trim();
    let childSelector = superSelector.substring(frameIndex+2).trim();
    root = root.querySelector(rootSelector);
    if (!root || !root.contentWindow) {
      return null;
    }

    return superQuerySelector(childSelector, root.contentWindow.document);
  }
}

let dumpn = msg => dump(msg + "\n");
