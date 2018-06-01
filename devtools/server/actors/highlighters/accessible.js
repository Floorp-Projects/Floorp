/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AutoRefreshHighlighter } = require("./auto-refresh");
const { getBounds } = require("./utils/accessibility");

const {
  CanvasFrameAnonymousContentHelper,
  createNode,
  createSVGNode
} = require("./utils/markup");

const { setIgnoreLayoutChanges } = require("devtools/shared/layout/utils");

/**
 * The AccessibleHighlighter draws the bounds of an accessible object.
 *
 * Usage example:
 *
 * let h = new AccessibleHighlighter(env);
 * h.show(node, { x, y, w, h, [duration] });
 * h.hide();
 * h.destroy();
 *
 * Available options:
 *         - {Number} x
 *           x coordinate of the top left corner of the accessible object
 *         - {Number} y
 *           y coordinate of the top left corner of the accessible object
 *         - {Number} w
 *           width of the the accessible object
 *         - {Number} h
 *           height of the the accessible object
 *         - {Number} duration
 *           Duration of time that the highlighter should be shown.
 *
 * Structure:
 * <div class="highlighter-container">
 *   <div class="accessible-root">
 *     <svg class="accessible-elements" hidden="true">
 *       <path class="accessible-bounds" points="..." />
 *     </svg>
 *   </div>
 * </div>
 */
class AccessibleHighlighter extends AutoRefreshHighlighter {
  constructor(highlighterEnv) {
    super(highlighterEnv);

    this.ID_CLASS_PREFIX = "accessible-";

    this.markup = new CanvasFrameAnonymousContentHelper(this.highlighterEnv,
      this._buildMarkup.bind(this));

    this.onPageHide = this.onPageHide.bind(this);
    this.onWillNavigate = this.onWillNavigate.bind(this);

    this.highlighterEnv.on("will-navigate", this.onWillNavigate);

    this.pageListenerTarget = highlighterEnv.pageListenerTarget;
    this.pageListenerTarget.addEventListener("pagehide", this.onPageHide);
  }

  /**
   * Build highlighter markup.
   *
   * @return {Object} Container element for the highlighter markup.
   */
  _buildMarkup() {
    const container = createNode(this.win, {
      attributes: {
        "class": "highlighter-container",
        "role": "presentation"
      }
    });

    const root = createNode(this.win, {
      parent: container,
      attributes: {
        "id": "root",
        "class": "root",
        "role": "presentation"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Build the SVG element.
    const svg = createSVGNode(this.win, {
      nodeType: "svg",
      parent: root,
      attributes: {
        "id": "elements",
        "width": "100%",
        "height": "100%",
        "hidden": "true",
        "role": "presentation"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    createSVGNode(this.win, {
      nodeType: "path",
      parent: svg,
      attributes: {
        "class": "bounds",
        "id": "bounds",
        "role": "presentation"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    return container;
  }

  /**
   * Destroy the nodes. Remove listeners.
   */
  destroy() {
    if (this._highlightTimer) {
      clearTimeout(this._highlightTimer);
      this._highlightTimer = null;
    }

    this.highlighterEnv.off("will-navigate", this.onWillNavigate);
    this.pageListenerTarget.removeEventListener("pagehide", this.onPageHide);
    this.pageListenerTarget = null;

    this.markup.destroy();
    AutoRefreshHighlighter.prototype.destroy.call(this);
  }

  /**
   * Find an element in highlighter markup.
   *
   * @param  {String} id
   *         Highlighter markup elemet id attribute.
   * @return {DOMNode} Element in the highlighter markup.
   */
  getElement(id) {
    return this.markup.getElement(this.ID_CLASS_PREFIX + id);
  }

  /**
   * Show the highlighter on a given accessible.
   *
   * @return {Boolean} True if accessible is highlighted, false otherwise.
   */
  _show() {
    if (this._highlightTimer) {
      clearTimeout(this._highlightTimer);
      this._highlightTimer = null;
    }

    const { duration } = this.options;
    const shown = this._update();
    if (shown && duration) {
      this._highlightTimer = setTimeout(() => {
        this.hide();
      }, duration);
    }
    return shown;
  }

  /**
   * Update and show accessible bounds for a current accessible.
   *
   * @return {Boolean} True if accessible is highlighted, false otherwise.
   */
  _update() {
    let shown = false;
    setIgnoreLayoutChanges(true);

    if (this._updateAccessibleBounds()) {
      this._showAccessibleBounds();
      shown = true;
    } else {
      // Nothing to highlight (0px rectangle like a <script> tag for instance)
      this.hide();
    }

    setIgnoreLayoutChanges(false,
                           this.highlighterEnv.window.document.documentElement);

    return shown;
  }

  /**
   * Hide the highlighter.
   */
  _hide() {
    setIgnoreLayoutChanges(true);
    this._hideAccessibleBounds();
    setIgnoreLayoutChanges(false,
                           this.highlighterEnv.window.document.documentElement);
  }

  /**
   * Hide the accessible bounds container.
   */
  _hideAccessibleBounds() {
    this.getElement("elements").setAttribute("hidden", "true");
  }

  /**
   * Showthe accessible bounds container.
   */
  _showAccessibleBounds() {
    this.getElement("elements").removeAttribute("hidden");
  }

  /**
   * Get current accessible bounds.
   *
   * @return {Object|null} Returns, if available, positioning and bounds
   *                       information for the accessible object.
   */
  get _bounds() {
    return getBounds(this.win, this.options);
  }

  /**
   * Update accessible bounds for a current accessible. Re-draw highlighter
   * markup.
   *
   * @return {Boolean} True if accessible is highlighted, false otherwise.
   */
  _updateAccessibleBounds() {
    const bounds = this._bounds;
    if (!bounds) {
      this._hideAccessibleBounds();
      return false;
    }

    const boundsEl = this.getElement("bounds");
    const { left, right, top, bottom } = bounds;
    const path =
      `M${left},${top} L${right},${top} L${right},${bottom} L${left},${bottom}`;
    boundsEl.setAttribute("d", path);

    // Un-zoom the root wrapper if the page was zoomed.
    const rootId = this.ID_CLASS_PREFIX + "elements";
    this.markup.scaleRootElement(this.currentNode, rootId);

    return true;
  }

  /**
   * Hide highlighter on page hide.
   */
  onPageHide({ target }) {
    // If a pagehide event is triggered for current window's highlighter, hide
    // the highlighter.
    if (target.defaultView === this.win) {
      this.hide();
    }
  }

  /**
   * Hide highlighter on navigation.
   */
  onWillNavigate({ isTopLevel }) {
    if (isTopLevel) {
      this.hide();
    }
  }
}

exports.AccessibleHighlighter = AccessibleHighlighter;
