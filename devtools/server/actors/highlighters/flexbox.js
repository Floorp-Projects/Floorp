/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AutoRefreshHighlighter } = require("./auto-refresh");
const {
  CANVAS_SIZE,
  getCanvasPosition,
  getCurrentMatrix,
  updateCanvasElement,
} = require("./utils/canvas");
const {
  CanvasFrameAnonymousContentHelper,
  createNode,
} = require("./utils/markup");
const {
  setIgnoreLayoutChanges,
} = require("devtools/shared/layout/utils");

class FlexboxHighlighter extends AutoRefreshHighlighter {
  constructor(highlighterEnv) {
    super(highlighterEnv);

    this.ID_CLASS_PREFIX = "flexbox-";

    this.markup = new CanvasFrameAnonymousContentHelper(this.highlighterEnv,
      this._buildMarkup.bind(this));

    this.onPageHide = this.onPageHide.bind(this);
    this.onWillNavigate = this.onWillNavigate.bind(this);

    this.highlighterEnv.on("will-navigate", this.onWillNavigate);

    let { pageListenerTarget } = highlighterEnv;
    pageListenerTarget.addEventListener("pagehide", this.onPageHide);

    // Initialize the <canvas> position to the top left corner of the page
    this._canvasPosition = {
      x: 0,
      y: 0
    };

    // Calling `getCanvasPosition` anyway since the highlighter could be initialized
    // on a page that has scrolled already.
    let { canvasX, canvasY } = getCanvasPosition(this._canvasPosition, this._scroll,
      this.win, this._winDimensions);
    this._canvasPosition.x = canvasX;
    this._canvasPosition.y = canvasY;
  }

  _buildMarkup() {
    let container = createNode(this.win, {
      attributes: {
        "class": "highlighter-container"
      }
    });

    let root = createNode(this.win, {
      parent: container,
      attributes: {
        "id": "root",
        "class": "root"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // We use a <canvas> element because there is an arbitrary number of items and texts
    // to draw which wouldn't be possible with HTML or SVG without having to insert and
    // remove the whole markup on every update.
    createNode(this.win, {
      parent: root,
      nodeType: "canvas",
      attributes: {
        "id": "canvas",
        "class": "canvas",
        "hidden": "true",
        "width": CANVAS_SIZE,
        "height": CANVAS_SIZE
      },
      prefix: this.ID_CLASS_PREFIX
    });

    return container;
  }

  destroy() {
    let { highlighterEnv } = this;
    highlighterEnv.off("will-navigate", this.onWillNavigate);

    let { pageListenerTarget } = highlighterEnv;
    if (pageListenerTarget) {
      pageListenerTarget.removeEventListener("pagehide", this.onPageHide);
    }

    this.markup.destroy();

    AutoRefreshHighlighter.prototype.destroy.call(this);
  }

  get canvas() {
    return this.getElement("canvas");
  }

  get ctx() {
    return this.canvas.getCanvasContext("2d");
  }

  getElement(id) {
    return this.markup.getElement(this.ID_CLASS_PREFIX + id);
  }

  _hide() {
    setIgnoreLayoutChanges(true);
    this._hideFlexbox();
    setIgnoreLayoutChanges(false, this.highlighterEnv.document.documentElement);
  }

  _hideFlexbox() {
    this.getElement("canvas").setAttribute("hidden", "true");
  }

  /**
   * The <canvas>'s position needs to be updated if the page scrolls too much, in order
   * to give the illusion that it always covers the viewport.
   */
  _scrollUpdate() {
    let { hasUpdated } = getCanvasPosition(this._canvasPosition, this._scroll, this.win,
      this._winDimensions);

    if (hasUpdated) {
      this._update();
    }
  }

  _show() {
    this._hide();
    return this._update();
  }

  _showFlexbox() {
    this.getElement("canvas").removeAttribute("hidden");
  }

  /**
   * If a page hide event is triggered for current window's highlighter, hide the
   * highlighter.
   */
  onPageHide({ target }) {
    if (target.defaultView === this.win) {
      this.hide();
    }
  }

  /**
   * Called when the page will-navigate. Used to hide the flexbox highlighter and clear
   * the cached gap patterns and avoid using DeadWrapper obejcts as gap patterns the
   * next time.
   */
  onWillNavigate({ isTopLevel }) {
    if (isTopLevel) {
      this.hide();
    }
  }

  _update() {
    setIgnoreLayoutChanges(true);

    let root = this.getElement("root");

    // Hide the root element and force the reflow in order to get the proper window's
    // dimensions without increasing them.
    root.setAttribute("style", "display: none");
    this.win.document.documentElement.offsetWidth;

    let { width, height } = this._winDimensions;

    // Updates the <canvas> element's position and size.
    // It also clear the <canvas>'s drawing context.
    updateCanvasElement(this.canvas, this._canvasPosition, this.win.devicePixelRatio);

    // Update the current matrix used in our canvas' rendering
    let { currentMatrix, hasNodeTransformations } = getCurrentMatrix(this.currentNode,
      this.win);
    this.currentMatrix = currentMatrix;
    this.hasNodeTransformations = hasNodeTransformations;

    this._showFlexbox();

    root.setAttribute("style",
      `position: absolute; width: ${width}px; height: ${height}px; overflow: hidden`);

    setIgnoreLayoutChanges(false, this.highlighterEnv.document.documentElement);
    return true;
  }
}

exports.FlexboxHighlighter = FlexboxHighlighter;
