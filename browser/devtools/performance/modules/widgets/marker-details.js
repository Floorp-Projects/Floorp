/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This file contains the rendering code for the marker sidebar.
 */

const { Cc, Ci, Cu, Cr } = require("chrome");
let WebConsoleUtils = require("devtools/toolkit/webconsole/utils").Utils;

loader.lazyRequireGetter(this, "EventEmitter",
  "devtools/toolkit/event-emitter");
loader.lazyRequireGetter(this, "L10N",
  "devtools/performance/global", true);
loader.lazyRequireGetter(this, "TIMELINE_BLUEPRINT",
  "devtools/performance/global", true);
loader.lazyRequireGetter(this, "MarkerUtils",
  "devtools/performance/marker-utils");

/**
 * A detailed view for one single marker.
 *
 * @param nsIDOMNode parent
 *        The parent node holding the view.
 * @param nsIDOMNode splitter
 *        The splitter node that the resize event is bound to.
 */
function MarkerDetails(parent, splitter) {
  EventEmitter.decorate(this);
  this._onClick = this._onClick.bind(this);
  this._document = parent.ownerDocument;
  this._parent = parent;
  this._splitter = splitter;
  this._splitter.addEventListener("mouseup", () => this.emit("resize"));
  this._parent.addEventListener("click", this._onClick);
}

MarkerDetails.prototype = {
  /**
   * Removes any node references from this view.
   */
  destroy: function() {
    this.empty();
    this._parent.removeEventListener("click", this._onClick);
    this._parent = null;
    this._splitter = null;
  },

  /**
   * Clears the view.
   */
  empty: function() {
    this._parent.innerHTML = "";
  },

  /**
   * Populates view with marker's details.
   *
   * @param object params
   *        An options object holding:
   *        marker - The marker to display.
   *        frames - Array of stack frame information; see stack.js.
   */
  render: function({ marker, frames }) {
    this.empty();

    let elements = [];
    elements.push(MarkerUtils.DOM.buildTitle(this._document, marker));
    elements.push(MarkerUtils.DOM.buildDuration(this._document, marker));
    MarkerUtils.DOM.buildFields(this._document, marker).forEach(field => elements.push(field));

    // Build a stack element -- and use the "startStack" label if
    // we have both a star and endStack.
    if (marker.stack) {
      let type = marker.endStack ? "startStack" : "stack";
      elements.push(MarkerUtils.DOM.buildStackTrace(this._document, {
        frameIndex: marker.stack, frames, type
      }));
    }

    elements.forEach(el => this._parent.appendChild(el));
  },

  /**
   * Handles click in the marker details view. Based on the target,
   * can handle different actions -- only supporting view source links
   * for the moment.
   */
  _onClick: function (e) {
    let data = findActionFromEvent(e.target);
    if (!data) {
      return;
    }

    if (data.action === "view-source") {
      this.emit("view-source", data.url, data.line);
    }
  },
};

exports.MarkerDetails = MarkerDetails;

/**
 * Take an element from an event `target`, and asend through
 * the DOM, looking for an element with a `data-action` attribute. Return
 * the parsed `data-action` value found, or null if none found before
 * reaching the parent `container`.
 *
 * @param {Element} target
 * @param {Element} container
 * @return {?object}
 */
function findActionFromEvent (target, container) {
  let el = target;
  let action;
  while (el !== container) {
    if (action = el.getAttribute("data-action")) {
      return JSON.parse(action);
    }
    el = el.parentNode;
  }
  return null;
}
