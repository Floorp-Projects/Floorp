/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  AutoRefreshHighlighter,
} = require("devtools/server/actors/highlighters/auto-refresh");
const {
  CanvasFrameAnonymousContentHelper,
  isNodeValid,
} = require("devtools/server/actors/highlighters/utils/markup");
const {
  TEXT_NODE,
  DOCUMENT_NODE,
} = require("devtools/shared/dom-node-constants");
const {
  getCurrentZoom,
  setIgnoreLayoutChanges,
} = require("devtools/shared/layout/utils");

loader.lazyRequireGetter(
  this,
  ["getBounds", "getBoundsXUL", "Infobar"],
  "devtools/server/actors/highlighters/utils/accessibility",
  true
);

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
 * @param {Number} options.x
 *        X coordinate of the top left corner of the accessible object
 * @param {Number} options.y
 *        Y coordinate of the top left corner of the accessible object
 * @param {Number} options.w
 *        Width of the the accessible object
 * @param {Number} options.h
 *        Height of the the accessible object
 * @param {Number} options.duration
 *        Duration of time that the highlighter should be shown.
 * @param {String|null} options.name
 *        Name of the the accessible object
 * @param {String} options.role
 *        Role of the the accessible object
 *
 * Structure:
 * <div class="highlighter-container" aria-hidden="true">
 *   <div class="accessible-root">
 *     <svg class="accessible-elements" hidden="true">
 *       <path class="accessible-bounds" points="..." />
 *     </svg>
 *     <div class="accessible-infobar-container">
 *      <div class="accessible-infobar">
 *        <div class="accessible-infobar-text">
 *          <span class="accessible-infobar-role">Accessible Role</span>
 *          <span class="accessible-infobar-name">Accessible Name</span>
 *        </div>
 *      </div>
 *     </div>
 *   </div>
 * </div>
 */
class AccessibleHighlighter extends AutoRefreshHighlighter {
  constructor(highlighterEnv) {
    super(highlighterEnv);
    this.ID_CLASS_PREFIX = "accessible-";
    this.accessibleInfobar = new Infobar(this);

    this.markup = new CanvasFrameAnonymousContentHelper(
      this.highlighterEnv,
      this._buildMarkup.bind(this)
    );
    this.isReady = this.markup.initialize();

    this.onPageHide = this.onPageHide.bind(this);
    this.onWillNavigate = this.onWillNavigate.bind(this);

    this.highlighterEnv.on("will-navigate", this.onWillNavigate);

    this.pageListenerTarget = highlighterEnv.pageListenerTarget;
    this.pageListenerTarget.addEventListener("pagehide", this.onPageHide);
  }

  /**
   * Static getter that indicates that AccessibleHighlighter supports
   * highlighting in XUL windows.
   */
  static get XULSupported() {
    return true;
  }

  /**
   * Build highlighter markup.
   *
   * @return {Object} Container element for the highlighter markup.
   */
  _buildMarkup() {
    const container = this.markup.createNode({
      attributes: {
        class: "highlighter-container",
        "aria-hidden": "true",
      },
    });

    const root = this.markup.createNode({
      parent: container,
      attributes: {
        id: "root",
        class: "root",
      },
      prefix: this.ID_CLASS_PREFIX,
    });

    // Build the SVG element.
    const svg = this.markup.createSVGNode({
      nodeType: "svg",
      parent: root,
      attributes: {
        id: "elements",
        width: "100%",
        height: "100%",
        hidden: "true",
      },
      prefix: this.ID_CLASS_PREFIX,
    });

    this.markup.createSVGNode({
      nodeType: "path",
      parent: svg,
      attributes: {
        class: "bounds",
        id: "bounds",
      },
      prefix: this.ID_CLASS_PREFIX,
    });

    // Build the accessible's infobar markup.
    this.accessibleInfobar.buildMarkup(root);

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

    AutoRefreshHighlighter.prototype.destroy.call(this);

    this.accessibleInfobar.destroy();
    this.accessibleInfobar = null;
    this.markup.destroy();
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
   * Check if node is a valid element, document or text node.
   *
   * @override  AutoRefreshHighlighter.prototype._isNodeValid
   * @param  {DOMNode} node
   *         The node to highlight.
   * @return {Boolean} whether or not node is valid.
   */
  _isNodeValid(node) {
    return (
      super._isNodeValid(node) ||
      isNodeValid(node, TEXT_NODE) ||
      isNodeValid(node, DOCUMENT_NODE)
    );
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
    if (shown) {
      this.emit("highlighter-event", { options: this.options, type: "shown" });
      if (duration) {
        this._highlightTimer = setTimeout(() => {
          this.hide();
        }, duration);
      }
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

      this.accessibleInfobar.show();

      shown = true;
    } else {
      // Nothing to highlight (0px rectangle like a <script> tag for instance)
      this.hide();
    }

    setIgnoreLayoutChanges(
      false,
      this.highlighterEnv.window.document.documentElement
    );

    return shown;
  }

  /**
   * Hide the highlighter.
   */
  _hide() {
    setIgnoreLayoutChanges(true);
    this._hideAccessibleBounds();
    this.accessibleInfobar.hide();
    setIgnoreLayoutChanges(
      false,
      this.highlighterEnv.window.document.documentElement
    );
  }

  /**
   * Public API method to temporarily hide accessible bounds for things like
   * color contrast calculation.
   */
  hideAccessibleBounds() {
    if (this.getElement("elements").hasAttribute("hidden")) {
      return;
    }

    this._hideAccessibleBounds();
    this._shouldRestoreBoundsVisibility = true;
  }

  /**
   * Public API method to show accessible bounds in case they were temporarily
   * hidden.
   */
  showAccessibleBounds() {
    if (this._shouldRestoreBoundsVisibility) {
      this._showAccessibleBounds();
    }
  }

  /**
   * Hide the accessible bounds container.
   */
  _hideAccessibleBounds() {
    this._shouldRestoreBoundsVisibility = null;
    setIgnoreLayoutChanges(true);
    this.getElement("elements").setAttribute("hidden", "true");
    setIgnoreLayoutChanges(
      false,
      this.highlighterEnv.window.document.documentElement
    );
  }

  /**
   * Show the accessible bounds container.
   */
  _showAccessibleBounds() {
    this._shouldRestoreBoundsVisibility = null;
    if (!this.currentNode || !this.highlighterEnv.window) {
      return;
    }

    setIgnoreLayoutChanges(true);
    this.getElement("elements").removeAttribute("hidden");
    setIgnoreLayoutChanges(
      false,
      this.highlighterEnv.window.document.documentElement
    );
  }

  /**
   * Get current accessible bounds.
   *
   * @return {Object|null} Returns, if available, positioning and bounds
   *                       information for the accessible object.
   */
  get _bounds() {
    let { win, options } = this;
    let getBoundsFn = getBounds;
    if (this.options.isXUL) {
      // Zoom level for the top level browser window does not change and only
      // inner frames do. So we need to get the zoom level of the current node's
      // parent window.
      let zoom = getCurrentZoom(this.currentNode);
      zoom *= zoom;
      options = { ...options, zoom };
      getBoundsFn = getBoundsXUL;
      win = this.win.parent.ownerGlobal;
    }

    return getBoundsFn(win, options);
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
      this._hide();
      return false;
    }

    const boundsEl = this.getElement("bounds");
    const { left, right, top, bottom } = bounds;
    const path = `M${left},${top} L${right},${top} L${right},${bottom} L${left},${bottom}`;
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
