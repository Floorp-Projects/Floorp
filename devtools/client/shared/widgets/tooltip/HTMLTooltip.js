/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const {TooltipToggle} = require("devtools/client/shared/widgets/tooltip/TooltipToggle");
const {focusableSelector} = require("devtools/client/shared/focus");
const {getCurrentZoom} = require("devtools/shared/layout/utils");
const {listenOnce} = require("devtools/shared/async-utils");

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
  DOORHANGER: "doorhanger",
};

module.exports.TYPE = TYPE;

const ARROW_WIDTH = {
  "normal": 0,
  "arrow": 32,
  // This is the value calculated for the .tooltip-arrow element in tooltip.css
  // which includes the arrow width (20px) plus the extra margin added so that
  // the drop shadow is not cropped (2px each side).
  "doorhanger": 24,
};

const ARROW_OFFSET = {
  "normal": 0,
  // Default offset between the tooltip's edge and the tooltip arrow.
  "arrow": 20,
  // Match other Firefox menus which use 10px from edge (but subtract the 2px
  // margin included in the ARROW_WIDTH above).
  "doorhanger": 8,
};

const EXTRA_HEIGHT = {
  "normal": 0,
  // The arrow is 16px tall, but merges on 3px with the panel border
  "arrow": 13,
  // The doorhanger arrow is 10px tall, but merges on 1px with the panel border
  "doorhanger": 9,
};

const EXTRA_BORDER = {
  "normal": 0,
  "arrow": 3,
  "doorhanger": 0,
};

/**
 * Calculate the vertical position & offsets to use for the tooltip. Will attempt to
 * respect the provided height and position preferences, unless the available height
 * prevents this.
 *
 * @param {DOMRect} anchorRect
 *        Bounding rectangle for the anchor, relative to the tooltip document.
 * @param {DOMRect} viewportRect
 *        Bounding rectangle for the viewport. top/left can be different from 0 if some
 *        space should not be used by tooltips (for instance OS toolbars, taskbars etc.).
 * @param {Number} height
 *        Preferred height for the tooltip.
 * @param {String} pos
 *        Preferred position for the tooltip. Possible values: "top" or "bottom".
 * @param {Number} offset
 *        Offset between the top of the anchor and the tooltip.
 * @return {Object}
 *         - {Number} top: the top offset for the tooltip.
 *         - {Number} height: the height to use for the tooltip container.
 *         - {String} computedPosition: Can differ from the preferred position depending
 *           on the available height). "top" or "bottom"
 */
const calculateVerticalPosition = (
  anchorRect,
  viewportRect,
  height,
  pos,
  offset
) => {
  const {TOP, BOTTOM} = POSITION;

  let {top: anchorTop, height: anchorHeight} = anchorRect;

  // Translate to the available viewport space before calculating dimensions and position.
  anchorTop -= viewportRect.top;

  // Calculate available space for the tooltip.
  const availableTop = anchorTop;
  const availableBottom = viewportRect.height - (anchorTop + anchorHeight);

  // Find POSITION
  let keepPosition = false;
  if (pos === TOP) {
    keepPosition = availableTop >= height + offset;
  } else if (pos === BOTTOM) {
    keepPosition = availableBottom >= height + offset;
  }
  if (!keepPosition) {
    pos = availableTop > availableBottom ? TOP : BOTTOM;
  }

  // Calculate HEIGHT.
  const availableHeight = pos === TOP ? availableTop : availableBottom;
  height = Math.min(height, availableHeight - offset);
  height = Math.floor(height);

  // Calculate TOP.
  let top = pos === TOP ? anchorTop - height - offset : anchorTop + anchorHeight + offset;

  // Translate back to absolute coordinates by re-including viewport top margin.
  top += viewportRect.top;

  return {top, height, computedPosition: pos};
};

/**
 * Calculate the horizontal position & offsets to use for the tooltip. Will
 * attempt to respect the provided width and position preferences, unless the
 * available width prevents this.
 *
 * @param {DOMRect} anchorRect
 *        Bounding rectangle for the anchor, relative to the tooltip document.
 * @param {DOMRect} viewportRect
 *        Bounding rectangle for the viewport. top/left can be different from
 *        0 if some space should not be used by tooltips (for instance OS
 *        toolbars, taskbars etc.).
 * @param {DOMRect} windowRect
 *        Bounding rectangle for the window. Used to determine which direction
 *        doorhangers should hang.
 * @param {Number} width
 *        Preferred width for the tooltip.
 * @param {String} type
 *        The tooltip type (e.g. "arrow").
 * @param {Number} offset
 *        Horizontal offset in pixels.
 * @param {Number} borderRadius
 *        The border radius of the panel. This is added to ARROW_OFFSET to
 *        calculate the distance from the edge of the tooltip to the start
 *        of arrow. It is separate from ARROW_OFFSET since it will vary by
 *        platform.
 * @param {Boolean} isRtl
 *        If the anchor is in RTL, the tooltip should be aligned to the right.
 * @return {Object}
 *         - {Number} left: the left offset for the tooltip.
 *         - {Number} width: the width to use for the tooltip container.
 *         - {Number} arrowLeft: the left offset to use for the arrow element.
 */
const calculateHorizontalPosition = (
  anchorRect,
  viewportRect,
  windowRect,
  width,
  type,
  offset,
  borderRadius,
  isRtl
) => {
  // Which direction should the tooltip go?
  //
  // For tooltips we follow the writing direction but for doorhangers the
  // guidelines[1] say that,
  //
  //   "Doorhangers opening on the right side of the view show the directional
  //   arrow on the right.
  //
  //   Doorhangers opening on the left side of the view show the directional
  //   arrow on the left.
  //
  //   Never place the directional arrow at the center of doorhangers."
  //
  // [1] https://design.firefox.com/photon/components/doorhangers.html#directional-arrow
  //
  // So for those we need to check if the anchor is more right or left.
  let hangDirection;
  if (type === TYPE.DOORHANGER) {
    const anchorCenter = anchorRect.left + anchorRect.width / 2;
    const viewCenter = windowRect.left + windowRect.width / 2;
    hangDirection = anchorCenter >= viewCenter ? "left" : "right";
  } else {
    hangDirection = isRtl ? "left" : "right";
  }

  const anchorWidth = anchorRect.width;

  // Calculate logical start of anchor relative to the viewport.
  const anchorStart =
    hangDirection === "right"
      ? anchorRect.left - viewportRect.left
      : viewportRect.right - anchorRect.right;

  // Calculate tooltip width.
  const tooltipWidth = Math.min(width, viewportRect.width);

  // Calculate tooltip start.
  let tooltipStart = anchorStart + offset;
  tooltipStart = Math.min(tooltipStart, viewportRect.width - tooltipWidth);
  tooltipStart = Math.max(0, tooltipStart);

  // Calculate arrow start (tooltip's start might be updated)
  const arrowWidth = ARROW_WIDTH[type];
  let arrowStart;
  // Arrow and doorhanger style tooltips may need to be shifted
  if (type === TYPE.ARROW || type === TYPE.DOORHANGER) {
    const arrowOffset = ARROW_OFFSET[type] + borderRadius;

    // Where will the point of the arrow be if we apply the standard offset?
    const arrowCenter = tooltipStart + arrowOffset + arrowWidth / 2;

    // How does that compare to the center of the anchor?
    const anchorCenter = anchorStart + anchorWidth / 2;

    // If the anchor is too narrow, align the arrow and the anchor center.
    if (arrowCenter > anchorCenter) {
      tooltipStart = Math.max(0, tooltipStart - (arrowCenter - anchorCenter));
    }
    // Arrow's start offset relative to the anchor.
    arrowStart = Math.min(arrowOffset, (anchorWidth - arrowWidth) / 2) | 0;
    // Translate the coordinate to tooltip container
    arrowStart += anchorStart - tooltipStart;
    // Make sure the arrow remains in the tooltip container.
    arrowStart = Math.min(arrowStart, tooltipWidth - arrowWidth - borderRadius);
    arrowStart = Math.max(arrowStart, borderRadius);
  }

  // Convert from logical coordinates to physical
  const left =
    hangDirection === "right"
      ? viewportRect.left + tooltipStart
      : viewportRect.right - tooltipStart - tooltipWidth;
  const arrowLeft =
    hangDirection === "right"
      ? arrowStart
      : tooltipWidth - arrowWidth - arrowStart;

  return { left, width: tooltipWidth, arrowLeft };
};

/**
 * Get the bounding client rectangle for a given node, relative to a custom
 * reference element (instead of the default for getBoundingClientRect which
 * is always the element's ownerDocument).
 */
const getRelativeRect = function(node, relativeTo) {
  // getBoxQuads is a non-standard WebAPI which will not work on non-firefox
  // browser when running launchpad on Chrome.
  if (!node.getBoxQuads) {
    const {top, left, width, height} = node.getBoundingClientRect();
    const right = left + width;
    const bottom = top + height;
    return {top, right, bottom, left, width, height};
  }

  // Width and Height can be taken from the rect.
  const {width, height} = node.getBoundingClientRect();

  const quadBounds = node.getBoxQuads({relativeTo})[0].getBounds();
  const top = quadBounds.top;
  const left = quadBounds.left;

  // Compute right and bottom coordinates using the rest of the data.
  const right = left + width;
  const bottom = top + height;

  return {top, right, bottom, left, width, height};
};

/**
 * The HTMLTooltip can display HTML content in a tooltip popup.
 *
 * @param {Document} toolboxDoc
 *        The toolbox document to attach the HTMLTooltip popup.
 * @param {Object}
 *        - {String} type
 *          Display type of the tooltip. Possible values: "normal", "arrow", and
 *          "doorhanger".
 *        - {Boolean} autofocus
 *          Defaults to false. Should the tooltip be focused when opening it.
 *        - {Boolean} consumeOutsideClicks
 *          Defaults to true. The tooltip is closed when clicking outside.
 *          Should this event be stopped and consumed or not.
 *        - {Boolean} useXulWrapper
 *          Defaults to false. If the tooltip is hosted in a XUL document, use a XUL panel
 *          in order to use all the screen viewport available.
 */
function HTMLTooltip(toolboxDoc, {
    type = "normal",
    autofocus = false,
    consumeOutsideClicks = true,
    useXulWrapper = false,
  } = {}) {
  EventEmitter.decorate(this);

  this.doc = toolboxDoc;
  this.type = type;
  this.autofocus = autofocus;
  this.consumeOutsideClicks = consumeOutsideClicks;
  this.useXulWrapper = this._isXUL() && useXulWrapper;
  this.preferredWidth = "auto";
  this.preferredHeight = "auto";

  // The top window is used to attach click event listeners to close the tooltip if the
  // user clicks on the content page.
  this.topWindow = this._getTopWindow();

  this._position = null;

  this._onClick = this._onClick.bind(this);
  this._onXulPanelHidden = this._onXulPanelHidden.bind(this);

  this._toggle = new TooltipToggle(this);
  this.startTogglingOnHover = this._toggle.start.bind(this._toggle);
  this.stopTogglingOnHover = this._toggle.stop.bind(this._toggle);

  this.container = this._createContainer();

  if (this.useXulWrapper) {
    // When using a XUL panel as the wrapper, the actual markup for the tooltip is as
    // follows :
    // <panel> <!-- XUL panel used to position the tooltip anywhere on screen -->
    //   <div> <!-- div wrapper used to isolate the tooltip container -->
    //     <div> <! the actual tooltip.container element -->
    this.xulPanelWrapper = this._createXulPanelWrapper();
    const inner = this.doc.createElementNS(XHTML_NS, "div");
    inner.classList.add("tooltip-xul-wrapper-inner");

    this.doc.documentElement.appendChild(this.xulPanelWrapper);
    this.xulPanelWrapper.appendChild(inner);
    inner.appendChild(this.container);
  } else if (this._isXUL()) {
    this.doc.documentElement.appendChild(this.container);
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
   * Retrieve the displayed position used for the tooltip. Null if the tooltip is hidden.
   */
  get position() {
    return this.isVisible() ? this._position : null;
  },

  /**
   * Set the tooltip content element. The preferred width/height should also be
   * specified here.
   *
   * @param {Element} content
   *        The tooltip content, should be a HTML element.
   * @param {Object}
   *        - {Number} width: preferred width for the tooltip container. If not specified
   *          the tooltip container will be measured before being displayed, and the
   *          measured width will be used as the preferred width.
   *        - {Number} height: preferred height for the tooltip container. If
   *          not specified the tooltip container will be measured before being
   *          displayed, and the measured height will be used as the preferred
   *          height.
   *
   *          For tooltips whose content height may change while being
   *          displayed, the special value Infinity may be used to produce
   *          a flexible container that accommodates resizing content. Note,
   *          however, that when used in combination with the XUL wrapper the
   *          unfilled part of this container will consume all mouse events
   *          making content behind this area inaccessible until the tooltip is
   *          dismissed.
   */
  setContent: function(content, {width = "auto", height = "auto"} = {}) {
    this.preferredWidth = width;
    this.preferredHeight = height;

    this.panel.innerHTML = "";
    this.panel.appendChild(content);
  },

  /**
   * Show the tooltip next to the provided anchor element. A preferred position
   * can be set. The event "shown" will be fired after the tooltip is displayed.
   *
   * @param {Element} anchor
   *        The reference element with which the tooltip should be aligned
   * @param {Object} options
   *        Settings for positioning the tooltip.
   * @param {String} options.position
   *        Optional, possible values: top|bottom
   *        If layout permits, the tooltip will be displayed on top/bottom
   *        of the anchor. If omitted, the tooltip will be displayed where
   *        more space is available.
   * @param {Number} options.x
   *        Optional, horizontal offset between the anchor and the tooltip.
   * @param {Number} options.y
   *        Optional, vertical offset between the anchor and the tooltip.
   */
  async show(anchor, {position, x = 0, y = 0} = {}) {
    // Get anchor geometry
    let anchorRect = getRelativeRect(anchor, this.doc);
    if (this.useXulWrapper) {
      anchorRect = this._convertToScreenRect(anchorRect);
    }

    const { viewportRect, windowRect } = this._getBoundingRects();

    // Calculate the horizonal position and width
    let preferredWidth;
    // Record the height too since it might save us from having to look it up
    // later.
    let measuredHeight;
    if (this.preferredWidth === "auto") {
      // Reset any styles that constrain the dimensions we want to calculate.
      this.container.style.width = "auto";
      if (this.preferredHeight === "auto") {
        this.container.style.height = "auto";
      }
      ({
        width: preferredWidth,
        height: measuredHeight,
      } = this._measureContainerSize());
    } else {
      const themeWidth = 2 * EXTRA_BORDER[this.type];
      preferredWidth = this.preferredWidth + themeWidth;
    }

    const anchorWin = anchor.ownerDocument.defaultView;
    const anchorCS = anchorWin.getComputedStyle(anchor);
    const isRtl = anchorCS.direction === "rtl";

    let borderRadius = 0;
    if (this.type === TYPE.DOORHANGER) {
      borderRadius = parseFloat(
        anchorCS.getPropertyValue("--theme-arrowpanel-border-radius")
      );
      if (Number.isNaN(borderRadius)) {
        borderRadius = 0;
      }
    }

    const {left, width, arrowLeft} = calculateHorizontalPosition(
      anchorRect,
      viewportRect,
      windowRect,
      preferredWidth,
      this.type,
      x,
      borderRadius,
      isRtl
    );

    // If we constrained the width, then any measured height we have is no
    // longer valid.
    if (measuredHeight && width !== preferredWidth) {
      measuredHeight = undefined;
    }

    // Apply width and arrow positioning
    this.container.style.width = width + "px";
    if (this.type === TYPE.ARROW || this.type === TYPE.DOORHANGER) {
      this.arrow.style.left = arrowLeft + "px";
    }

    // Work out how much vertical margin we have.
    //
    // This relies on us having set either .tooltip-top or .tooltip-bottom
    // and on the margins for both being symmetrical. Fortunately the call to
    // _measureContainerSize above will set .tooltip-top for us and it also
    // assumes these styles are symmetrical so this should be ok.
    const panelWindow = this.panel.ownerDocument.defaultView;
    const panelComputedStyle = panelWindow.getComputedStyle(this.panel);
    const verticalMargin =
      parseFloat(panelComputedStyle.marginTop) +
      parseFloat(panelComputedStyle.marginBottom);

    // Calculate the vertical position and height
    let preferredHeight;
    if (this.preferredHeight === "auto") {
      if (measuredHeight) {
        this.container.style.height = "auto";
        preferredHeight = measuredHeight;
      } else {
        ({ height: preferredHeight } = this._measureContainerSize());
      }
      preferredHeight += verticalMargin;
    } else {
      const themeHeight =
        EXTRA_HEIGHT[this.type] +
        verticalMargin +
        2 * EXTRA_BORDER[this.type];
      preferredHeight = this.preferredHeight + themeHeight;
    }

    const {top, height, computedPosition} =
      calculateVerticalPosition(anchorRect, viewportRect, preferredHeight, position, y);

    this._position = computedPosition;
    const isTop = computedPosition === POSITION.TOP;
    this.container.classList.toggle("tooltip-top", isTop);
    this.container.classList.toggle("tooltip-bottom", !isTop);

    // If the preferred height is set to Infinity, the tooltip container should grow based
    // on its content's height and use as much height as possible.
    this.container.classList.toggle("tooltip-flexible-height",
      this.preferredHeight === Infinity);

    this.container.style.height = height + "px";

    if (this.useXulWrapper) {
      await this._showXulWrapperAt(left, top);
    } else {
      this.container.style.left = left + "px";
      this.container.style.top = top + "px";
    }

    this.container.classList.add("tooltip-visible");

    // Keep a pointer on the focused element to refocus it when hiding the tooltip.
    this._focusedElement = this.doc.activeElement;

    this.doc.defaultView.clearTimeout(this.attachEventsTimer);
    this.attachEventsTimer = this.doc.defaultView.setTimeout(() => {
      if (this.autofocus) {
        this.focus();
      }
      // Update the top window reference each time in case the host changes.
      this.topWindow = this._getTopWindow();
      this.topWindow.addEventListener("click", this._onClick, true);
      this.emit("shown");
    }, 0);
  },

  /**
   * Calculate the following boundary rectangles:
   *
   * - Viewport rect: This is the region that limits the tooltip dimensions.
   *   When using a XUL panel wrapper, the tooltip will be able to use the whole
   *   screen (excluding space reserved by the OS for toolbars etc.) and hence
   *   the result will be in screen coordinates.
   *   Otherwise, the tooltip is limited to the tooltip's document.
   *
   * - Window rect: This is the bounds of the view in which the tooltip is
   *   presented. It is reported in the same coordinates as the viewport
   *   rect and is used for determining in which direction a doorhanger-type
   *   tooltip should "hang".
   *   When using the XUL panel wrapper this will be the dimensions of the
   *   window in screen coordinates. Otherwise it will be the same as the
   *   viewport rect.
   *
   * @return {Object} An object with the following properties
   *         viewportRect {Object} DOMRect-like object with the Number
   *                      properties: top, right, bottom, left, width, height
   *                      representing the viewport rect.
   *         windowRect   {Object} DOMRect-like object with the Number
   *                      properties: top, right, bottom, left, width, height
   *                      representing the viewport rect.
   */
  _getBoundingRects: function() {
    let viewportRect;
    let windowRect;

    if (this.useXulWrapper) {
      // availLeft/Top are the coordinates first pixel available on the screen
      // for applications (excluding space dedicated for OS toolbars, menus
      // etc...)
      // availWidth/Height are the dimensions available to applications
      // excluding all the OS reserved space
      const {
        availLeft,
        availTop,
        availHeight,
        availWidth,
      } = this.doc.defaultView.screen;
      viewportRect = {
        top: availTop,
        right: availLeft + availWidth,
        bottom: availTop + availHeight,
        left: availLeft,
        width: availWidth,
        height: availHeight,
      };

      const {
        screenX,
        screenY,
        outerWidth,
        outerHeight,
      } = this.doc.defaultView;
      windowRect = {
        top: screenY,
        right: screenX + outerWidth,
        bottom: screenY + outerHeight,
        left: screenX,
        width: outerWidth,
        height: outerHeight,
      };
    } else {
      viewportRect = windowRect =
        this.doc.documentElement.getBoundingClientRect();
    }

    return { viewportRect, windowRect };
  },

  _measureContainerSize: function() {
    const xulParent = this.container.parentNode;
    if (this.useXulWrapper && !this.isVisible()) {
      // Move the container out of the XUL Panel to measure it.
      this.doc.documentElement.appendChild(this.container);
    }

    this.container.classList.add("tooltip-hidden");
    // Set either of the tooltip-top or tooltip-bottom styles so that we get an
    // accurate height. We're assuming that the two styles will be symmetrical
    // and that we will clear this as necessary later.
    this.container.classList.add("tooltip-top");
    this.container.classList.remove("tooltip-bottom");
    const { width, height } = this.container.getBoundingClientRect();
    this.container.classList.remove("tooltip-hidden");

    if (this.useXulWrapper && !this.isVisible()) {
      xulParent.appendChild(this.container);
    }

    return { width, height };
  },

  /**
   * Hide the current tooltip. The event "hidden" will be fired when the tooltip
   * is hidden.
   */
  async hide() {
    this.doc.defaultView.clearTimeout(this.attachEventsTimer);
    if (!this.isVisible()) {
      this.emit("hidden");
      return;
    }

    this.topWindow.removeEventListener("click", this._onClick, true);
    this.container.classList.remove("tooltip-visible");
    if (this.useXulWrapper) {
      await this._hideXulWrapper();
    }

    this.emit("hidden");

    const tooltipHasFocus = this.container.contains(this.doc.activeElement);
    if (tooltipHasFocus && this._focusedElement) {
      this._focusedElement.focus();
      this._focusedElement = null;
    }
  },

  /**
   * Check if the tooltip is currently displayed.
   * @return {Boolean} true if the tooltip is visible
   */
  isVisible: function() {
    return this.container.classList.contains("tooltip-visible");
  },

  /**
   * Destroy the tooltip instance. Hide the tooltip if displayed, remove the
   * tooltip container from the document.
   */
  destroy: function() {
    this.hide();
    this.container.remove();
    if (this.xulPanelWrapper) {
      this.xulPanelWrapper.remove();
    }
    this._toggle.destroy();
  },

  _createContainer: function() {
    const container = this.doc.createElementNS(XHTML_NS, "div");
    container.setAttribute("type", this.type);
    container.classList.add("tooltip-container");

    let html = '<div class="tooltip-filler"></div>';
    html += '<div class="tooltip-panel"></div>';

    if (this.type === TYPE.ARROW || this.type === TYPE.DOORHANGER) {
      html += '<div class="tooltip-arrow"></div>';
    }
    // eslint-disable-next-line no-unsanitized/property
    container.innerHTML = html;
    return container;
  },

  _onClick: function(e) {
    if (this._isInTooltipContainer(e.target)) {
      return;
    }

    this.hide();
    if (this.consumeOutsideClicks && e.button === 0) {
      // Consume only left click events (button === 0).
      e.preventDefault();
      e.stopPropagation();
    }
  },

  _isInTooltipContainer: function(node) {
    // Check if the target is the tooltip arrow.
    if (this.arrow && this.arrow === node) {
      return true;
    }

    const tooltipWindow = this.panel.ownerDocument.defaultView;
    let win = node.ownerDocument.defaultView;

    // Check if the tooltip panel contains the node if they live in the same document.
    if (win === tooltipWindow) {
      return this.panel.contains(node);
    }

    // Check if the node window is in the tooltip container.
    while (win.parent && win.parent !== win) {
      if (win.parent === tooltipWindow) {
        // If the parent window is the tooltip window, check if the tooltip contains
        // the current frame element.
        return this.panel.contains(win.frameElement);
      }
      win = win.parent;
    }

    return false;
  },

  _onXulPanelHidden: function() {
    if (this.isVisible()) {
      this.hide();
    }
  },

  /**
   * Focus on the first focusable item in the tooltip.
   *
   * Returns true if we found something to focus on, false otherwise.
   */
  focus: function() {
    const focusableElement = this.panel.querySelector(focusableSelector);
    if (focusableElement) {
      focusableElement.focus();
    }
    return !!focusableElement;
  },

  /**
   * Focus on the last focusable item in the tooltip.
   *
   * Returns true if we found something to focus on, false otherwise.
   */
  focusEnd: function() {
    const focusableElements = this.panel.querySelectorAll(focusableSelector);
    if (focusableElements.length) {
      focusableElements[focusableElements.length - 1].focus();
    }
    return focusableElements.length !== 0;
  },

  _getTopWindow: function() {
    return this.doc.defaultView.top;
  },

  /**
   * Check if the tooltip's owner document is a XUL document.
   */
  _isXUL: function() {
    return this.doc.documentElement.namespaceURI === XUL_NS;
  },

  _createXulPanelWrapper: function() {
    const panel = this.doc.createElementNS(XUL_NS, "panel");

    // XUL panel is only a way to display DOM elements outside of the document viewport,
    // so disable all features that impact the behavior.
    panel.setAttribute("animate", false);
    panel.setAttribute("consumeoutsideclicks", false);
    panel.setAttribute("noautofocus", true);
    panel.setAttribute("ignorekeys", true);
    panel.setAttribute("tooltip", "aHTMLTooltip");

    // Use type="arrow" to prevent side effects (see Bug 1285206)
    panel.setAttribute("type", "arrow");

    panel.setAttribute("level", "top");
    panel.setAttribute("class", "tooltip-xul-wrapper");

    return panel;
  },

  _showXulWrapperAt: function(left, top) {
    this.xulPanelWrapper.addEventListener("popuphidden", this._onXulPanelHidden);
    const onPanelShown = listenOnce(this.xulPanelWrapper, "popupshown");
    const zoom = getCurrentZoom(this.xulPanelWrapper);
    this.xulPanelWrapper.openPopupAtScreen(left * zoom, top * zoom, false);
    return onPanelShown;
  },

  _hideXulWrapper: function() {
    this.xulPanelWrapper.removeEventListener("popuphidden", this._onXulPanelHidden);

    if (this.xulPanelWrapper.state === "closed") {
      // XUL panel is already closed, resolve immediately.
      return Promise.resolve();
    }

    const onPanelHidden = listenOnce(this.xulPanelWrapper, "popuphidden");
    this.xulPanelWrapper.hidePopup();
    return onPanelHidden;
  },

  /**
   * Convert from coordinates relative to the tooltip's document, to coordinates relative
   * to the "available" screen. By "available" we mean the screen, excluding the OS bars
   * display on screen edges.
   */
  _convertToScreenRect: function({left, top, width, height}) {
    // mozInnerScreenX/Y are the coordinates of the top left corner of the window's
    // viewport, excluding chrome UI.
    left += this.doc.defaultView.mozInnerScreenX;
    top += this.doc.defaultView.mozInnerScreenY;
    return {top, right: left + width, bottom: top + height, left, width, height};
  },
};
