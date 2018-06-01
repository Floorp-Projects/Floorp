/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { CanvasFrameAnonymousContentHelper, createNode } = require("./utils/markup");

/**
 * The PausedDebuggerOverlay is a class that displays a semi-transparent mask on top of
 * the whole page and a toolbar at the top of the page.
 * This is used to signal to users that script execution is current paused.
 * The toolbar is used to display the reason for the pause in script execution as well as
 * buttons to resume or step through the program.
 */
function PausedDebuggerOverlay(highlighterEnv) {
  this.env = highlighterEnv;
  this.markup = new CanvasFrameAnonymousContentHelper(highlighterEnv,
    this._buildMarkup.bind(this));
}

PausedDebuggerOverlay.prototype = {
  typeName: "PausedDebuggerOverlay",

  ID_CLASS_PREFIX: "paused-dbg-",

  _buildMarkup() {
    const { window } = this.env;
    const prefix = this.ID_CLASS_PREFIX;

    const container = createNode(window, {
      attributes: {"class": "highlighter-container"}
    });

    // Wrapper element.
    const wrapper = createNode(window, {
      parent: container,
      attributes: {
        "id": "root",
        "class": "root",
        "hidden": "true",
        "overlay": "true"
      },
      prefix
    });

    const toolbar = createNode(window, {
      parent: wrapper,
      attributes: {
        "id": "toolbar",
        "class": "toolbar"
      },
      prefix
    });

    createNode(window, {
      nodeType: "span",
      parent: toolbar,
      attributes: {
        "id": "reason",
        "class": "reason"
      },
      prefix
    });

    return container;
  },

  destroy() {
    this.hide();
    this.markup.destroy();
    this.env = null;
  },

  getElement(id) {
    return this.markup.getElement(this.ID_CLASS_PREFIX + id);
  },

  show(node, options = {}) {
    if (this.env.isXUL) {
      return false;
    }

    // Show the highlighter's root element.
    const root = this.getElement("root");
    root.removeAttribute("hidden");

    // The page overlay is only shown upon request. Sometimes we just want the toolbar.
    if (options.onlyToolbar) {
      root.removeAttribute("overlay");
    } else {
      root.setAttribute("overlay", "true");
    }

    // Set the text to appear in the toolbar.
    const toolbar = this.getElement("toolbar");
    if (options.reason) {
      this.getElement("reason").setTextContent(options.reason);
      toolbar.removeAttribute("hidden");
    } else {
      toolbar.setAttribute("hidden", "true");
    }

    return true;
  },

  hide() {
    if (this.env.isXUL) {
      return;
    }

    // Hide the overlay.
    this.getElement("root").setAttribute("hidden", "true");
  }
};
exports.PausedDebuggerOverlay = PausedDebuggerOverlay;
