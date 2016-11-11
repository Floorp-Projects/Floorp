/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * The highlighter overlays are in-content highlighters that appear when hovering over
 * property values.
 */

const promise = require("promise");
const EventEmitter = require("devtools/shared/event-emitter");
const { VIEW_NODE_VALUE_TYPE } = require("devtools/client/inspector/shared/node-types");

/**
 * Manages all highlighters in the style-inspector.
 *
 * @param  {CssRuleView|CssComputedView} view
 *         Either the rule-view or computed-view panel
 */
function HighlightersOverlay(view) {
  this.view = view;

  let {CssRuleView} = require("devtools/client/inspector/rules/rules");
  this.isRuleView = view instanceof CssRuleView;

  this.highlighterUtils = this.view.inspector.toolbox.highlighterUtils;

  this._onMouseMove = this._onMouseMove.bind(this);
  this._onMouseOut = this._onMouseOut.bind(this);

  this.highlighters = {};

  // Only initialize the overlay if at least one of the highlighter types is
  // supported
  this.supportsHighlighters =
    this.highlighterUtils.supportsCustomHighlighters();

  EventEmitter.decorate(this);
}

HighlightersOverlay.prototype = {
  /**
   * Add the highlighters overlay to the view. This will start tracking mouse
   * movements and display highlighters when needed
   */
  addToView: function () {
    if (!this.supportsHighlighters || this._isStarted || this._isDestroyed) {
      return;
    }

    let el = this.view.element;
    el.addEventListener("mousemove", this._onMouseMove, false);
    el.addEventListener("mouseout", this._onMouseOut, false);
    el.ownerDocument.defaultView.addEventListener("mouseout", this._onMouseOut, false);

    this._isStarted = true;
  },

  /**
   * Remove the overlay from the current view. This will stop tracking mouse
   * movement and showing highlighters
   */
  removeFromView: function () {
    if (!this.supportsHighlighters || !this._isStarted || this._isDestroyed) {
      return;
    }

    this._hideCurrent();

    let el = this.view.element;
    el.removeEventListener("mousemove", this._onMouseMove, false);
    el.removeEventListener("mouseout", this._onMouseOut, false);

    this._isStarted = false;
  },

  _onMouseMove: function (event) {
    // Bail out if the target is the same as for the last mousemove
    if (event.target === this._lastHovered) {
      return;
    }

    // Only one highlighter can be displayed at a time, hide the currently shown
    this._hideCurrent();

    this._lastHovered = event.target;

    let nodeInfo = this.view.getNodeInfo(event.target);
    if (!nodeInfo) {
      return;
    }

    // Choose the type of highlighter required for the hovered node
    let type;
    if (this._isRuleViewTransform(nodeInfo) ||
        this._isComputedViewTransform(nodeInfo)) {
      type = "CssTransformHighlighter";
    }

    if (type) {
      this.highlighterShown = type;
      let node = this.view.inspector.selection.nodeFront;
      this._getHighlighter(type)
          .then(highlighter => highlighter.show(node))
          .then(shown => {
            if (shown) {
              this.emit("highlighter-shown");
            }
          });
    }
  },

  _onMouseOut: function (event) {
    // Only hide the highlighter if the mouse leaves the currently hovered node.
    if (!this._lastHovered ||
        (event && this._lastHovered.contains(event.relatedTarget))) {
      return;
    }

    // Otherwise, hide the highlighter.
    this._lastHovered = null;
    this._hideCurrent();
  },

  /**
   * Is the current hovered node a css transform property value in the rule-view
   *
   * @param  {Object} nodeInfo
   * @return {Boolean}
   */
  _isRuleViewTransform: function (nodeInfo) {
    let isTransform = nodeInfo.type === VIEW_NODE_VALUE_TYPE &&
                      nodeInfo.value.property === "transform";
    let isEnabled = nodeInfo.value.enabled &&
                    !nodeInfo.value.overridden &&
                    !nodeInfo.value.pseudoElement;
    return this.isRuleView && isTransform && isEnabled;
  },

  /**
   * Is the current hovered node a css transform property value in the
   * computed-view
   *
   * @param  {Object} nodeInfo
   * @return {Boolean}
   */
  _isComputedViewTransform: function (nodeInfo) {
    let isTransform = nodeInfo.type === VIEW_NODE_VALUE_TYPE &&
                      nodeInfo.value.property === "transform";
    return !this.isRuleView && isTransform;
  },

  /**
   * Hide the currently shown highlighter
   */
  _hideCurrent: function () {
    if (!this.highlighterShown || !this.highlighters[this.highlighterShown]) {
      return;
    }

    // For some reason, the call to highlighter.hide doesn't always return a
    // promise. This causes some tests to fail when trying to install a
    // rejection handler on the result of the call. To avoid this, check
    // whether the result is truthy before installing the handler.
    let onHidden = this.highlighters[this.highlighterShown].hide();
    if (onHidden) {
      onHidden.then(null, e => console.error(e));
    }

    this.highlighterShown = null;
    this.emit("highlighter-hidden");
  },

  /**
   * Get a highlighter front given a type. It will only be initialized once
   *
   * @param  {String} type The highlighter type. One of this.highlighters
   * @return {Promise} that resolves to the highlighter
   */
  _getHighlighter: function (type) {
    let utils = this.highlighterUtils;

    if (this.highlighters[type]) {
      return promise.resolve(this.highlighters[type]);
    }

    return utils.getHighlighterByType(type).then(highlighter => {
      this.highlighters[type] = highlighter;
      return highlighter;
    });
  },

  /**
   * Destroy this overlay instance, removing it from the view and destroying
   * all initialized highlighters
   */
  destroy: function () {
    this.removeFromView();

    for (let type in this.highlighters) {
      if (this.highlighters[type]) {
        this.highlighters[type].finalize();
        this.highlighters[type] = null;
      }
    }

    this.view = null;
    this.highlighterUtils = null;

    this._isDestroyed = true;
  }
};

module.exports = HighlightersOverlay;
