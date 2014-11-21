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
 * Given an actorID, get the corresponding actor from the first debugger-server
 * connection.
 * @param {String} actorID
 */
function getHighlighterActor(actorID) {
  let {DebuggerServer} = Cu.import("resource://gre/modules/devtools/dbg-server.jsm");
  if (!DebuggerServer.initialized) {
    return;
  }

  let connID = Object.keys(DebuggerServer._connections)[0];
  let conn = DebuggerServer._connections[connID];

  return conn.getActor(actorID);
}

/**
 * Get the instance of CanvasFrameAnonymousContentHelper used by a given
 * highlighter actor.
 * The instance provides methods to get/set attributes/text/style on nodes of
 * the highlighter, inserted into the nsCanvasFrame.
 * @see /toolkit/devtools/server/actors/highlighter.js
 * @param {String} actorID
 */
function getHighlighterCanvasFrameHelper(actorID) {
  let actor = getHighlighterActor(actorID);
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
 * @return {String} The value, if found, null otherwise
 */
addMessageListener("Test:GetHighlighterAttribute", function(msg) {
  let {nodeID, name, actorID} = msg.data;

  let value;
  let helper = getHighlighterCanvasFrameHelper(actorID);
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
 * @return {String} The textcontent value
 */
addMessageListener("Test:GetHighlighterTextContent", function(msg) {
  let {nodeID, actorID} = msg.data;

  let value;
  let helper = getHighlighterCanvasFrameHelper(actorID);
  if (helper) {
    value = helper.getTextContentForElement(nodeID);
  }

  sendAsyncMessage("Test:GetHighlighterTextContent", value);
});

/**
 * Get the number of box-model highlighters created by the SelectorHighlighter
 * @param {Object} msg The msg.data part expects the following properties:
 * - {String} actorID The highlighter actor ID
 * @return {Number} The number of box-model highlighters created, or null if the
 * SelectorHighlighter was not found.
 */
addMessageListener("Test:GetSelectorHighlighterBoxNb", function(msg) {
  let {actorID} = msg.data;
  let {_highlighter: h} = getHighlighterActor(actorID);
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
 */
addMessageListener("Test:ChangeHighlightedNodeWaitForUpdate", function(msg) {
  // The name and value of the attribute to be changed
  let {name, value, actorID} = msg.data;
  let {_highlighter: h} = getHighlighterActor(actorID);

  h.once("updated", () => {
    sendAsyncMessage("Test:ChangeHighlightedNodeWaitForUpdate");
  });

  h.currentNode.setAttribute(name, value);
});

/**
 * Change the zoom level of the page.
 * Optionally subscribe to the box-model highlighter's update event and waiting
 * for it to refresh before responding.
 * @param {Object} msg The msg.data part expects the following properties
 * - {Number} level The new zoom level
 * - {String} actorID Optional. The highlighter actor ID
 */
addMessageListener("Test:ChangeZoomLevel", function(msg) {
  let {level, actorID} = msg.data;
  dumpn("Zooming page to " + level);

  if (actorID) {
    let {_highlighter: h} = getHighlighterActor(actorID);
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
 * @param {Object} msg The msg.objects part should be the element
 * @return {Object} An object with each property being a box-model region, each
 * of them being an object with the p1/p2/p3/p4 properties
 */
addMessageListener("Test:GetAllAdjustedQuads", function(msg) {
  let {node} = msg.objects;
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
 * - {String} type
 * The msg.objects part should be the element.
 * @param {Object} data Event detail properties:
 */
addMessageListener("Test:SynthesizeMouse", function(msg) {
  let {node} = msg.objects;
  let {x, y, center, type} = msg.data;

  if (center) {
    EventUtils.synthesizeMouseAtCenter(node, {type}, node.ownerDocument.defaultView);
  } else {
    EventUtils.synthesizeMouse(node, x, y, {type}, node.ownerDocument.defaultView);
  }
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

let dumpn = msg => dump(msg + "\n");
