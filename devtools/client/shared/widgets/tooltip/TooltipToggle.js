/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEFAULT_SHOW_DELAY = 50;

/**
 * Tooltip helper designed to show/hide the tooltip when the mouse hovers over
 * particular nodes.
 *
 * This works by tracking mouse movements on a base container node (baseNode)
 * and showing the tooltip when the mouse stops moving. A callback can be
 * provided to the start() method to know whether or not the node being
 * hovered over should indeed receive the tooltip.
 */
function TooltipToggle(tooltip) {
  this.tooltip = tooltip;
  this.win = tooltip.doc.defaultView;

  this._onMouseMove = this._onMouseMove.bind(this);
  this._onMouseLeave = this._onMouseLeave.bind(this);
}

module.exports.TooltipToggle = TooltipToggle;

TooltipToggle.prototype = {
  /**
   * Start tracking mouse movements on the provided baseNode to show the
   * tooltip.
   *
   * 2 Ways to make this work:
   * - Provide a single node to attach the tooltip to, as the baseNode, and
   *   omit the second targetNodeCb argument
   * - Provide a baseNode that is the container of possibly numerous children
   *   elements that may receive a tooltip. In this case, provide the second
   *   targetNodeCb argument to decide wether or not a child should receive
   *   a tooltip.
   *
   * Note that if you call this function a second time, it will itself call
   * stop() before adding mouse tracking listeners again.
   *
   * @param {node} baseNode
   *        The container for all target nodes
   * @param {Function} targetNodeCb
   *        A function that accepts a node argument and returns true or false
   *        (or a promise that resolves or rejects) to signify if the tooltip
   *        should be shown on that node or not.
   *        If the promise rejects, it must reject `false` as value.
   *        Any other value is going to be logged as unexpected error.
   *        Additionally, the function receives a second argument which is the
   *        tooltip instance itself, to be used to add/modify the content of the
   *        tooltip if needed. If omitted, the tooltip will be shown everytime.
   * @param {Number} showDelay
   *        An optional delay that will be observed before showing the tooltip.
   *        Defaults to DEFAULT_SHOW_DELAY.
   */
  start: function (baseNode, targetNodeCb, showDelay = DEFAULT_SHOW_DELAY) {
    this.stop();

    if (!baseNode) {
      // Calling tool is in the process of being destroyed.
      return;
    }

    this._baseNode = baseNode;
    this._showDelay = showDelay;
    this._targetNodeCb = targetNodeCb || (() => true);

    baseNode.addEventListener("mousemove", this._onMouseMove, false);
    baseNode.addEventListener("mouseleave", this._onMouseLeave, false);
  },

  /**
   * If the start() function has been used previously, and you want to get rid
   * of this behavior, then call this function to remove the mouse movement
   * tracking
   */
  stop: function () {
    this.win.clearTimeout(this.toggleTimer);

    if (!this._baseNode) {
      return;
    }

    this._baseNode.removeEventListener("mousemove", this._onMouseMove, false);
    this._baseNode.removeEventListener("mouseleave", this._onMouseLeave, false);

    this._baseNode = null;
    this._targetNodeCb = null;
    this._lastHovered = null;
  },

  _onMouseMove: function (event) {
    if (event.target !== this._lastHovered) {
      this.tooltip.hide();
      this._lastHovered = event.target;

      this.win.clearTimeout(this.toggleTimer);
      this.toggleTimer = this.win.setTimeout(() => {
        this.isValidHoverTarget(event.target).then(target => {
          this.tooltip.show(target);
        }, reason => {
          if (reason === false) {
            // isValidHoverTarget rejects with false if the tooltip should
            // not be shown. This can be safely ignored.
            return;
          }
          console.error("isValidHoverTarget rejected with unexpected reason:");
          console.error(reason);
        });
      }, this._showDelay);
    }
  },

  /**
   * Is the given target DOMNode a valid node for toggling the tooltip on hover.
   * This delegates to the user-defined _targetNodeCb callback.
   * @return a promise that resolves or rejects depending if the tooltip should
   * be shown or not. If it resolves, it does to the actual anchor to be used
   */
  isValidHoverTarget: function (target) {
    // Execute the user-defined callback which should return either true/false
    // or a promise that resolves or rejects
    let res = this._targetNodeCb(target, this.tooltip);

    // The callback can additionally return a DOMNode to replace the anchor of
    // the tooltip when shown
    if (res && res.then) {
      return res.then(arg => {
        return arg && arg.nodeName ? arg : target;
      });
    }
    let newTarget = res && res.nodeName ? res : target;
    return new Promise((resolve, reject) => {
      res ? resolve(newTarget) : reject(false);
    });
  },

  _onMouseLeave: function () {
    this.win.clearTimeout(this.toggleTimer);
    this._lastHovered = null;
    this.tooltip.hide();
  },

  destroy: function () {
    this.stop();
  }
};
