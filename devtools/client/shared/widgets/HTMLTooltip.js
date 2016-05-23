/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const XHTML_NS = "http://www.w3.org/1999/xhtml";

const IFRAME_URL = "chrome://devtools/content/shared/widgets/tooltip-frame.xhtml";
const IFRAME_CONTAINER_ID = "tooltip-iframe-container";

/**
 * The HTMLTooltip can display HTML content in a tooltip popup.
 *
 * @param {Toolbox} toolbox
 *        The devtools toolbox, needed to get the devtools main window.
 * @param {Object}
 *        - {String} type
 *          Display type of the tooltip. Possible values: "normal"
 *        - {Boolean} autofocus
 *          Defaults to true. Should the tooltip be focused when opening it.
 *        - {Boolean} consumeOutsideClicks
 *          Defaults to true. The tooltip is closed when clicking outside.
 *          Should this event be stopped and consumed or not.
 */
function HTMLTooltip(toolbox,
  {type = "normal", autofocus = true, consumeOutsideClicks = true} = {}) {
  EventEmitter.decorate(this);

  this.doc = toolbox.doc;
  this.type = type;
  this.autofocus = autofocus;
  this.consumeOutsideClicks = consumeOutsideClicks;

  // Use the topmost window to listen for click events to close the tooltip
  this.topWindow = this.doc.defaultView.top;

  this._onClick = this._onClick.bind(this);

  this.container = this._createContainer();

  // Promise that will resolve when the container can be filled with content.
  this.containerReady = new Promise(resolve => {
    if (this._isXUL()) {
      // In XUL context, load a placeholder document in the iframe container.
      let onLoad = () => {
        this.container.removeEventListener("load", onLoad, true);
        resolve();
      };

      this.container.addEventListener("load", onLoad, true);
      this.container.setAttribute("src", IFRAME_URL);
    } else {
      // In non-XUL context the container is ready to use as is.
      resolve();
    }
  });
}

module.exports.HTMLTooltip = HTMLTooltip;

HTMLTooltip.prototype = {
  position: {
    TOP: "top",
    BOTTOM: "bottom",
  },

  /**
   * Set the tooltip content element. The preferred width/height should also be
   * specified here.
   *
   * @param {Element} content
   *        The tooltip content, should be a HTML element.
   * @param {Number} width
   *        Preferred width for the tooltip container
   * @param {Number} height
   *        Preferred height for the tooltip container
   * @return {Promise} a promise that will resolve when the content has been
   *         added in the tooltip container.
   */
  setContent: function (content, width, height) {
    this.preferredWidth = width;
    this.preferredHeight = height;

    return this.containerReady.then(() => {
      this.panel.innerHTML = "";
      this.panel.appendChild(content);
    });
  },

  /**
   * Show the tooltip next to the provided anchor element. A preferred position
   * can be set. The event "shown" will be fired after the tooltip is displayed.
   *
   * @param {Element} anchor
   *        The reference element with which the tooltip should be aligned
   * @param {Object}
   *        - {String} position: optional, possible values: top|bottom
   *          If layout permits, the tooltip will be displayed on top/bottom
   *          of the anchor. If ommitted, the tooltip will be displayed where
   *          more space is available.
   */
  show: function (anchor, {position} = {}) {
    this.containerReady.then(() => {
      let {top, left, width, height} = this._findBestPosition(anchor, position);

      if (this._isXUL()) {
        this.container.setAttribute("width", width);
        this.container.setAttribute("height", height);
      } else {
        this.container.style.width = width + "px";
        this.container.style.height = height + "px";
      }

      this.container.style.top = top + "px";
      this.container.style.left = left + "px";
      this.container.style.display = "block";

      if (this.autofocus) {
        this.container.focus();
      }

      this.attachEventsTimer = this.doc.defaultView.setTimeout(() => {
        this.topWindow.addEventListener("click", this._onClick, true);
        this.emit("shown");
      }, 0);
    });
  },

  /**
   * Hide the current tooltip. The event "hidden" will be fired when the tooltip
   * is hidden.
   */
  hide: function () {
    this.doc.defaultView.clearTimeout(this.attachEventsTimer);

    if (this.isVisible()) {
      this.topWindow.removeEventListener("click", this._onClick, true);
      this.container.style.display = "none";
      this.emit("hidden");
    }
  },

  get panel() {
    if (this._isXUL()) {
      // In XUL context, we are wrapping the HTML content in an iframe.
      let win = this.container.contentWindow.wrappedJSObject;
      return win.document.getElementById(IFRAME_CONTAINER_ID);
    }
    return this.container;
  },

  /**
   * Check if the tooltip is currently displayed.
   * @return {Boolean} true if the tooltip is visible
   */
  isVisible: function () {
    let win = this.doc.defaultView;
    return win.getComputedStyle(this.container).display != "none";
  },

  /**
   * Destroy the tooltip instance. Hide the tooltip if displayed, remove the
   * tooltip container from the document.
   */
  destroy: function () {
    this.hide();
    this.container.remove();
  },

  _createContainer: function () {
    let container;
    if (this._isXUL()) {
      container = this.doc.createElementNS(XHTML_NS, "iframe");
      container.classList.add("devtools-tooltip-iframe");
      this.doc.querySelector("window").appendChild(container);
    } else {
      container = this.doc.createElementNS(XHTML_NS, "div");
      this.doc.body.appendChild(container);
    }

    container.classList.add("theme-body");
    container.classList.add("devtools-htmltooltip-container");

    return container;
  },

  _onClick: function (e) {
    if (this._isInTooltipContainer(e.target)) {
      return;
    }

    this.hide();
    if (this.consumeOutsideClicks) {
      e.preventDefault();
      e.stopPropagation();
    }
  },

  _isInTooltipContainer: function (node) {
    let contentWindow = this.panel.ownerDocument.defaultView;
    let win = node.ownerDocument.defaultView;

    if (win === contentWindow) {
      // If node is in the same window as the tooltip, check if the tooltip
      // parent contains node.
      return this.panel.contains(node);
    }

    // Otherwise check if the node window is in the tooltip window.
    while (win.parent && win.parent != win) {
      win = win.parent;
      if (win === contentWindow) {
        return true;
      }
    }
    return false;
  },

  _findBestPosition: function (anchor, position) {
    let top, left;
    let {TOP, BOTTOM} = this.position;

    let {left: anchorLeft, top: anchorTop, height: anchorHeight}
      = this._getRelativeRect(anchor, this.doc);

    let {bottom: docBottom, right: docRight} =
      this.doc.documentElement.getBoundingClientRect();

    let height = this.preferredHeight;
    // Check if the popup can fit above the anchor.
    let availableTop = anchorTop;
    let fitsAbove = availableTop >= height;
    // Check if the popup can fit below the anchor.
    let availableBelow = docBottom - (anchorTop + anchorHeight);
    let fitsBelow = availableBelow >= height;

    let isPositionSuitable = (fitsAbove && position === TOP)
      || (fitsBelow && position === BOTTOM);
    if (!isPositionSuitable) {
      // If the preferred position does not fit the preferred height,
      // pick the position offering the most height.
      position = availableTop > availableBelow ? TOP : BOTTOM;
    }

    // Calculate height, capped by the maximum height available.
    height = Math.min(height, Math.max(availableTop, availableBelow));
    top = position === TOP ? anchorTop - height : anchorTop + anchorHeight;

    let availableWidth = docRight;
    let width = Math.min(this.preferredWidth, availableWidth);

    // By default, align the tooltip's left edge with the anchor left edge.
    if (anchorLeft + width <= docRight) {
      left = anchorLeft;
    } else {
      // If the tooltip cannot fit, shift to the left just enough to fit.
      left = docRight - width;
    }

    return {top, left, width, height};
  },

  /**
   * Get the bounding client rectangle for a given node, relative to a custom
   * reference element (instead of the default for getBoundingClientRect which
   * is always the element's ownerDocument).
   */
  _getRelativeRect: function (node, relativeTo) {
    // Width and Height can be taken from the rect.
    let {width, height} = node.getBoundingClientRect();

    // Find the smallest top/left coordinates from all quads.
    let top = Infinity, left = Infinity;
    let quads = node.getBoxQuads({relativeTo: relativeTo});
    for (let quad of quads) {
      top = Math.min(top, quad.bounds.top);
      left = Math.min(left, quad.bounds.left);
    }

    // Compute right and bottom coordinates using the rest of the data.
    let right = left + width;
    let bottom = top + height;

    return {top, right, bottom, left, width, height};
  },

  _isXUL: function () {
    return this.doc.documentElement.namespaceURI === XUL_NS;
  },
};
