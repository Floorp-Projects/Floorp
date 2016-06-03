/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const {TooltipToggle} = require("devtools/client/shared/widgets/tooltip/TooltipToggle");

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const XHTML_NS = "http://www.w3.org/1999/xhtml";

const POSITION = {
  TOP: "top",
  BOTTOM: "bottom",
};

module.exports.POSITION = POSITION;

const TYPE = {
  NORMAL: "normal",
  ARROW: "arrow",
};

module.exports.TYPE = TYPE;

const ARROW_WIDTH = 32;

// Default offset between the tooltip's left edge and the tooltip arrow.
const ARROW_OFFSET = 20;

const EXTRA_HEIGHT = {
  "normal": 0,
  // The arrow is 16px tall, but merges on 3px with the panel border
  "arrow": 13,
};

const EXTRA_BORDER = {
  "normal": 0,
  "arrow": 3,
};

/**
 * The HTMLTooltip can display HTML content in a tooltip popup.
 *
 * @param {Toolbox} toolbox
 *        The devtools toolbox, needed to get the devtools main window.
 * @param {Object}
 *        - {String} type
 *          Display type of the tooltip. Possible values: "normal", "arrow"
 *        - {Boolean} autofocus
 *          Defaults to false. Should the tooltip be focused when opening it.
 *        - {Boolean} consumeOutsideClicks
 *          Defaults to true. The tooltip is closed when clicking outside.
 *          Should this event be stopped and consumed or not.
 */
function HTMLTooltip(toolbox,
  {type = "normal", autofocus = false, consumeOutsideClicks = true} = {}) {
  EventEmitter.decorate(this);

  this.doc = toolbox.doc;
  this.type = type;
  this.autofocus = autofocus;
  this.consumeOutsideClicks = consumeOutsideClicks;

  // Use the topmost window to listen for click events to close the tooltip
  this.topWindow = this.doc.defaultView.top;

  this._onClick = this._onClick.bind(this);

  this._toggle = new TooltipToggle(this);
  this.startTogglingOnHover = this._toggle.start.bind(this._toggle);
  this.stopTogglingOnHover = this._toggle.stop.bind(this._toggle);

  this.container = this._createContainer();

  if (this._isXUL()) {
    this.doc.querySelector("window").appendChild(this.container);
  } else {
    // In non-XUL context the container is ready to use as is.
    this.doc.body.appendChild(this.container);
  }
}

module.exports.HTMLTooltip = HTMLTooltip;

HTMLTooltip.prototype = {
  /**
   * The tooltip panel is the parentNode of the tooltip content provided in
   * setContent().
   */
  get panel() {
    return this.container.querySelector(".tooltip-panel");
  },

  /**
   * The arrow element. Might be null depending on the tooltip type.
   */
  get arrow() {
    return this.container.querySelector(".tooltip-arrow");
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
    let themeHeight = EXTRA_HEIGHT[this.type] + 2 * EXTRA_BORDER[this.type];
    let themeWidth = 2 * EXTRA_BORDER[this.type];

    this.preferredWidth = width + themeWidth;
    this.preferredHeight = height + themeHeight;

    this.panel.innerHTML = "";
    this.panel.appendChild(content);
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
    let computedPosition = this._findBestPosition(anchor, position);

    let isTop = computedPosition.position === POSITION.TOP;
    this.container.classList.toggle("tooltip-top", isTop);
    this.container.classList.toggle("tooltip-bottom", !isTop);

    this.container.style.width = computedPosition.width + "px";
    this.container.style.height = computedPosition.height + "px";
    this.container.style.top = computedPosition.top + "px";
    this.container.style.left = computedPosition.left + "px";

    if (this.type === TYPE.ARROW) {
      this.arrow.style.left = computedPosition.arrowLeft + "px";
    }

    this.container.classList.add("tooltip-visible");

    this.attachEventsTimer = this.doc.defaultView.setTimeout(() => {
	  this._focusedElement = this.doc.activeElement;
      if (this.autofocus) {
        this.panel.focus();
      }
      this.topWindow.addEventListener("click", this._onClick, true);
      this.emit("shown");
    }, 0);
  },

  /**
   * Hide the current tooltip. The event "hidden" will be fired when the tooltip
   * is hidden.
   */
  hide: function () {
    this.doc.defaultView.clearTimeout(this.attachEventsTimer);

    if (this.isVisible()) {
      this.topWindow.removeEventListener("click", this._onClick, true);
      this.container.classList.remove("tooltip-visible");
      this.emit("hidden");

      if (this.container.contains(this.doc.activeElement) && this._focusedElement) {
        this._focusedElement.focus();
        this._focusedElement = null;
      }
    }
  },

  /**
   * Check if the tooltip is currently displayed.
   * @return {Boolean} true if the tooltip is visible
   */
  isVisible: function () {
    return this.container.classList.contains("tooltip-visible");
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
    let container = this.doc.createElementNS(XHTML_NS, "div");
    container.setAttribute("type", this.type);
    container.classList.add("tooltip-container");

    let html = '<div class="tooltip-panel"></div>';

    if (this.type === TYPE.ARROW) {
      html += '<div class="tooltip-arrow"></div>';
    }
    container.innerHTML = html;
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
    let tooltipWindow = this.panel.ownerDocument.defaultView;
    let win = node.ownerDocument.defaultView;

    if (this.arrow && this.arrow === node) {
      return true;
    }

    if (win === tooltipWindow) {
      // If node is in the same window as the tooltip, check if the tooltip panel
      // contains node.
      return this.panel.contains(node);
    }

    // Otherwise check if the node window is in the tooltip container.
    while (win.parent && win.parent != win) {
      win = win.parent;
      if (win === tooltipWindow) {
        return this.panel.contains(win.frameElement);
      }
    }

    return false;
  },

  /**
   * Calculates the best possible position to display the tooltip near the
   * provided anchor. An optional position can be provided, but will be
   * respected only if it doesn't force the tooltip to be resized.
   *
   * If the tooltip has to be resized, the position will be wherever the most
   * space is available.
   *
   */
  _findBestPosition: function (anchor, position) {
    let {TOP, BOTTOM} = POSITION;

    // Get anchor geometry
    let {
      left: anchorLeft, top: anchorTop,
      height: anchorHeight, width: anchorWidth
    } = this._getRelativeRect(anchor, this.doc);

    // Get document geometry
    let {bottom: docBottom, right: docRight} =
      this.doc.documentElement.getBoundingClientRect();

    // Calculate available space for the tooltip.
    let availableTop = anchorTop;
    let availableBottom = docBottom - (anchorTop + anchorHeight);

    // Find POSITION
    let keepPosition = false;
    if (position === TOP) {
      keepPosition = availableTop >= this.preferredHeight;
    } else if (position === BOTTOM) {
      keepPosition = availableBottom >= this.preferredHeight;
    }
    if (!keepPosition) {
      position = availableTop > availableBottom ? TOP : BOTTOM;
    }

    // Calculate HEIGHT.
    let availableHeight = position === TOP ? availableTop : availableBottom;
    let height = Math.min(this.preferredHeight, availableHeight);
    height = Math.floor(height);

    // Calculate TOP.
    let top = position === TOP ? anchorTop - height : anchorTop + anchorHeight;

    // Calculate WIDTH.
    let availableWidth = docRight;
    let width = Math.min(this.preferredWidth, availableWidth);

    // Calculate LEFT.
    // By default the tooltip is aligned with the anchor left edge. Unless this
    // makes it overflow the viewport, in which case is shifts to the left.
    let left = Math.min(anchorLeft, docRight - width);

    // Calculate ARROW LEFT (tooltip's LEFT might be updated)
    let arrowLeft;
    // Arrow style tooltips may need to be shifted to the left
    if (this.type === TYPE.ARROW) {
      let arrowCenter = left + ARROW_OFFSET + ARROW_WIDTH / 2;
      let anchorCenter = anchorLeft + anchorWidth / 2;
      // If the anchor is too narrow, align the arrow and the anchor center.
      if (arrowCenter > anchorCenter) {
        left = Math.max(0, left - (arrowCenter - anchorCenter));
      }
      // Arrow's feft offset relative to the anchor.
      arrowLeft = Math.min(ARROW_OFFSET, (anchorWidth - ARROW_WIDTH) / 2) | 0;
      // Translate the coordinate to tooltip container
      arrowLeft += anchorLeft - left;
      // Make sure the arrow remains in the tooltip container.
      arrowLeft = Math.min(arrowLeft, width - ARROW_WIDTH);
      arrowLeft = Math.max(arrowLeft, 0);
    }

    return {top, left, width, height, position, arrowLeft};
  },

  /**
   * Get the bounding client rectangle for a given node, relative to a custom
   * reference element (instead of the default for getBoundingClientRect which
   * is always the element's ownerDocument).
   */
  _getRelativeRect: function (node, relativeTo) {
    // Width and Height can be taken from the rect.
    let {width, height} = node.getBoundingClientRect();

    let quads = node.getBoxQuads({relativeTo});
    let top = quads[0].bounds.top;
    let left = quads[0].bounds.left;

    // Compute right and bottom coordinates using the rest of the data.
    let right = left + width;
    let bottom = top + height;

    return {top, right, bottom, left, width, height};
  },

  _isXUL: function () {
    return this.doc.documentElement.namespaceURI === XUL_NS;
  },
};
