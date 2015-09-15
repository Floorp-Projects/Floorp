/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci} = Components;
const subScriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
                          .getService(Ci.mozIJSSubScriptLoader);
var EventUtils = {};
subScriptLoader.loadSubScript("chrome://marionette/content/EventUtils.js", EventUtils);

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
addMessageListener("Test:MarkupView:SynthesizeMouse", function(msg) {
  let {x, y, center, options, selector} = msg.data;
  let {node} = msg.objects;

  if (!node && selector) {
    node = content.document.querySelector(selector);
  }

  if (center) {
    EventUtils.synthesizeMouseAtCenter(node, options, node.ownerDocument.defaultView);
  } else {
    EventUtils.synthesizeMouse(node, x, y, options, node.ownerDocument.defaultView);
  }

  // Most consumers won't need to listen to this message, unless they want to
  // wait for the mouse event to be synthesized and don't have another event
  // to listen to instead.
  sendAsyncMessage("Test:MarkupView:SynthesizeMouse");
});
