/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CanvasFrameAnonymousContentHelper,
  createNode,
} = require("./utils/markup");

const { LocalizationHelper } = require("devtools/shared/l10n");
const STRINGS_URI = "devtools/client/shared/locales/debugger.properties";
const L10N = new LocalizationHelper(STRINGS_URI);

/**
 * The PausedDebuggerOverlay is a class that displays a semi-transparent mask on top of
 * the whole page and a toolbar at the top of the page.
 * This is used to signal to users that script execution is current paused.
 * The toolbar is used to display the reason for the pause in script execution as well as
 * buttons to resume or step through the program.
 */
function PausedDebuggerOverlay(highlighterEnv, options = {}) {
  this.env = highlighterEnv;
  this.showOverlayStepButtons = options.showOverlayStepButtons;
  this.resume = options.resume;
  this.stepOver = options.stepOver;

  this.markup = new CanvasFrameAnonymousContentHelper(
    highlighterEnv,
    this._buildMarkup.bind(this)
  );
}

PausedDebuggerOverlay.prototype = {
  typeName: "PausedDebuggerOverlay",

  ID_CLASS_PREFIX: "paused-dbg-",

  _buildMarkup() {
    const { window } = this.env;
    const prefix = this.ID_CLASS_PREFIX;

    const container = createNode(window, {
      attributes: { class: "highlighter-container" },
    });

    // Wrapper element.
    const wrapper = createNode(window, {
      parent: container,
      attributes: {
        id: "root",
        class: "root",
        hidden: "true",
        overlay: "true",
      },
      prefix,
    });

    const toolbar = createNode(window, {
      parent: wrapper,
      attributes: {
        id: "toolbar",
        class: "toolbar",
      },
      prefix,
    });

    createNode(window, {
      nodeType: "span",
      parent: toolbar,
      attributes: {
        id: "reason",
        class: "reason",
      },
      prefix,
    });

    if (this.showOverlayStepButtons) {
      createNode(window, {
        parent: toolbar,
        attributes: {
          id: "divider",
          class: "divider",
        },
        prefix,
      });

      createNode(window, {
        nodeType: "button",
        parent: toolbar,
        attributes: {
          id: "step-button",
          class: "step-button",
        },
        prefix,
      });

      createNode(window, {
        nodeType: "button",
        parent: toolbar,
        attributes: {
          id: "resume-button",
          class: "resume-button",
        },
        prefix,
      });
    }

    return container;
  },

  destroy() {
    this.hide();
    this.markup.destroy();
    this.env = null;
  },

  onClick(target) {
    if (target.id == "paused-dbg-step-button") {
      this.stepOver();
    } else if (target.id == "paused-dbg-resume-button") {
      this.resume();
    }
  },

  handleEvent(e) {
    switch (e.type) {
      case "click":
        this.onClick(e.target);
        break;
      case "DOMMouseScroll":
        // Prevent scrolling. That's because we only took a screenshot of the viewport, so
        // scrolling out of the viewport wouldn't draw the expected things. In the future
        // we can take the screenshot again on scroll, but for now it doesn't seem
        // important.
        e.preventDefault();
        break;
      case "mouseover":
        break;
    }
  },

  getElement(id) {
    return this.markup.getElement(this.ID_CLASS_PREFIX + id);
  },

  show(node, options = {}) {
    if (this.env.isXUL || !options.reason) {
      return false;
    }

    // Show the highlighter's root element.
    const root = this.getElement("root");
    root.removeAttribute("hidden");
    root.setAttribute("overlay", "true");

    // Set the text to appear in the toolbar.
    const toolbar = this.getElement("toolbar");
    this.getElement("reason").setTextContent(
      L10N.getStr(`whyPaused.${options.reason}`)
    );
    toolbar.removeAttribute("hidden");

    this.env.window.document.setSuppressedEventListener(this);
    return true;
  },

  hide() {
    if (this.env.isXUL) {
      return;
    }

    // Hide the overlay.
    this.getElement("root").setAttribute("hidden", "true");
  },
};
exports.PausedDebuggerOverlay = PausedDebuggerOverlay;
