/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This file contains the rendering code for the marker sidebar.
 */

const { Cc, Ci, Cu, Cr } = require("chrome");

loader.lazyRequireGetter(this, "EventEmitter",
  "devtools/toolkit/event-emitter");
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

  this._document = parent.ownerDocument;
  this._parent = parent;
  this._splitter = splitter;

  this._onClick = this._onClick.bind(this);
  this._onSplitterMouseUp = this._onSplitterMouseUp.bind(this);

  this._parent.addEventListener("click", this._onClick);
  this._splitter.addEventListener("mouseup", this._onSplitterMouseUp);

  this.hidden = true;
}

MarkerDetails.prototype = {
  /**
   * Sets this view's width.
   * @param number
   */
  set width(value) {
    this._parent.setAttribute("width", value);
  },

  /**
   * Sets this view's width.
   * @return number
   */
  get width() {
    return +this._parent.getAttribute("width");
  },

  /**
   * Sets this view's visibility.
   * @param boolean
   */
  set hidden(value) {
    if (this._parent.hidden != value) {
      this._parent.hidden = value;
      this.emit("resize");
    }
  },

  /**
   * Gets this view's visibility.
   * @param boolean
   */
  get hidden() {
    return this._parent.hidden;
  },

  /**
   * Clears the marker details from this view.
   */
  empty: function() {
    this._parent.innerHTML = "";
  },

  /**
   * Populates view with marker's details.
   *
   * @param object params
   *        An options object holding:
   *          - marker: The marker to display.
   *          - frames: Array of stack frame information; see stack.js.
   *          - allocations: Whether or not allocations were enabled for this recording. [optional]
   */
  render: function (options) {
    let { marker, frames } = options;
    this.empty();

    let elements = [];
    elements.push(MarkerUtils.DOM.buildTitle(this._document, marker));
    elements.push(MarkerUtils.DOM.buildDuration(this._document, marker));
    MarkerUtils.DOM.buildFields(this._document, marker).forEach(f => elements.push(f));
    MarkerUtils.DOM.buildCustom(this._document, marker, options).forEach(f => elements.push(f));

    // Build a stack element -- and use the "startStack" label if
    // we have both a startStack and endStack.
    if (marker.stack) {
      let type = marker.endStack ? "startStack" : "stack";
      elements.push(MarkerUtils.DOM.buildStackTrace(this._document, {
        frameIndex: marker.stack, frames, type
      }));
    }
    if (marker.endStack) {
      let type = "endStack";
      elements.push(MarkerUtils.DOM.buildStackTrace(this._document, {
        frameIndex: marker.endStack, frames, type
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
    let data = findActionFromEvent(e.target, this._parent);
    if (!data) {
      return;
    }

    this.emit(data.action, data);
  },

  /**
   * Handles the "mouseup" event on the marker details view splitter.
   */
  _onSplitterMouseUp: function() {
    this.emit("resize");
  }
};

/**
 * Take an element from an event `target`, and ascend through
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

exports.MarkerDetails = MarkerDetails;
