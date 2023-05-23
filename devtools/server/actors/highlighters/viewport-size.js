/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("resource://devtools/shared/event-emitter.js");
const {
  setIgnoreLayoutChanges,
} = require("resource://devtools/shared/layout/utils.js");
const {
  CanvasFrameAnonymousContentHelper,
} = require("resource://devtools/server/actors/highlighters/utils/markup.js");

/**
 * The ViewportSizeHighlighter is a class that displays the viewport
 * width and height on a small overlay on the top right edge of the page
 * while the rulers are turned on.
 */
class ViewportSizeHighlighter {
  constructor(highlighterEnv) {
    this.env = highlighterEnv;
    this.markup = new CanvasFrameAnonymousContentHelper(
      highlighterEnv,
      this._buildMarkup.bind(this)
    );
    this.isReady = this.markup.initialize();

    const { pageListenerTarget } = highlighterEnv;
    pageListenerTarget.addEventListener("pagehide", this);
  }

  ID_CLASS_PREFIX = "viewport-size-highlighter-";

  _buildMarkup() {
    const prefix = this.ID_CLASS_PREFIX;

    const container = this.markup.createNode({
      attributes: { class: "highlighter-container" },
    });

    this.markup.createNode({
      parent: container,
      attributes: {
        class: "viewport-infobar-container",
        id: "viewport-infobar-container",
        position: "top",
      },
      prefix,
    });

    return container;
  }

  handleEvent(event) {
    switch (event.type) {
      case "pagehide":
        // If a page hide event is triggered for current window's highlighter, hide the
        // highlighter.
        if (event.target.defaultView === this.env.window) {
          this.destroy();
        }
        break;
    }
  }

  _update() {
    const { window } = this.env;

    setIgnoreLayoutChanges(true);

    this.updateViewportInfobar();

    setIgnoreLayoutChanges(false, window.document.documentElement);

    this._rafID = window.requestAnimationFrame(() => this._update());
  }

  _cancelUpdate() {
    if (this._rafID) {
      this.env.window.cancelAnimationFrame(this._rafID);
      this._rafID = 0;
    }
  }

  updateViewportInfobar() {
    const { window } = this.env;
    const { innerHeight, innerWidth } = window;
    const infobarId = this.ID_CLASS_PREFIX + "viewport-infobar-container";
    const textContent = innerWidth + "px \u00D7 " + innerHeight + "px";
    this.markup.getElement(infobarId).setTextContent(textContent);
  }

  destroy() {
    this.hide();

    const { pageListenerTarget } = this.env;

    if (pageListenerTarget) {
      pageListenerTarget.removeEventListener("pagehide", this);
    }

    this.markup.destroy();

    EventEmitter.emit(this, "destroy");
  }

  show() {
    this.markup.removeAttributeForElement(
      this.ID_CLASS_PREFIX + "viewport-infobar-container",
      "hidden"
    );

    this._update();

    return true;
  }

  hide() {
    this.markup.setAttributeForElement(
      this.ID_CLASS_PREFIX + "viewport-infobar-container",
      "hidden",
      "true"
    );

    this._cancelUpdate();
  }
}
exports.ViewportSizeHighlighter = ViewportSizeHighlighter;
