/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CanvasFrameAnonymousContentHelper,
} = require("resource://devtools/server/actors/highlighters/utils/markup.js");

loader.lazyGetter(this, "PausedReasonsBundle", () => {
  return new Localization(
    ["devtools/shared/debugger-paused-reasons.ftl"],
    true
  );
});

loader.lazyRequireGetter(
  this,
  "DEBUGGER_PAUSED_REASONS_L10N_MAPPING",
  "resource://devtools/shared/constants.js",
  true
);

/**
 * The PausedDebuggerOverlay is a class that displays a semi-transparent mask on top of
 * the whole page and a toolbar at the top of the page.
 * This is used to signal to users that script execution is current paused.
 * The toolbar is used to display the reason for the pause in script execution as well as
 * buttons to resume or step through the program.
 */
class PausedDebuggerOverlay {
  constructor(highlighterEnv, options = {}) {
    this.env = highlighterEnv;
    this.resume = options.resume;
    this.stepOver = options.stepOver;

    this.lastTarget = null;

    this.markup = new CanvasFrameAnonymousContentHelper(
      highlighterEnv,
      this._buildMarkup.bind(this),
      { waitForDocumentToLoad: false }
    );
    this.isReady = this.markup.initialize();
  }

  ID_CLASS_PREFIX = "paused-dbg-";

  _buildMarkup() {
    const prefix = this.ID_CLASS_PREFIX;

    const container = this.markup.createNode({
      attributes: { class: "highlighter-container" },
    });

    // Wrapper element.
    const wrapper = this.markup.createNode({
      parent: container,
      attributes: {
        id: "root",
        class: "root",
        hidden: "true",
        overlay: "true",
      },
      prefix,
    });

    const toolbar = this.markup.createNode({
      parent: wrapper,
      attributes: {
        id: "toolbar",
        class: "toolbar",
      },
      prefix,
    });

    this.markup.createNode({
      nodeType: "span",
      parent: toolbar,
      attributes: {
        id: "reason",
        class: "reason",
      },
      prefix,
    });

    this.markup.createNode({
      parent: toolbar,
      attributes: {
        id: "divider",
        class: "divider",
      },
      prefix,
    });

    const stepWrapper = this.markup.createNode({
      parent: toolbar,
      attributes: {
        id: "step-button-wrapper",
        class: "step-button-wrapper",
      },
      prefix,
    });

    this.markup.createNode({
      nodeType: "button",
      parent: stepWrapper,
      attributes: {
        id: "step-button",
        class: "step-button",
      },
      prefix,
    });

    const resumeWrapper = this.markup.createNode({
      parent: toolbar,
      attributes: {
        id: "resume-button-wrapper",
        class: "resume-button-wrapper",
      },
      prefix,
    });

    this.markup.createNode({
      nodeType: "button",
      parent: resumeWrapper,
      attributes: {
        id: "resume-button",
        class: "resume-button",
      },
      prefix,
    });

    return container;
  }

  destroy() {
    this.hide();
    this.markup.destroy();
    this.env = null;
    this.lastTarget = null;
  }

  onClick(target) {
    const { id } = target;
    if (!id) {
      return;
    }

    if (id.includes("paused-dbg-step-button")) {
      this.stepOver();
    } else if (id.includes("paused-dbg-resume-button")) {
      this.resume();
    }
  }

  onMouseMove(target) {
    // Not an element we care about
    if (!target || !target.id) {
      return;
    }

    // If the user didn't change targets, do nothing
    if (this.lastTarget && this.lastTarget.id === target.id) {
      return;
    }

    if (
      target.id.includes("step-button") ||
      target.id.includes("resume-button")
    ) {
      // The hover should be applied to the wrapper (icon's parent node)
      const newTarget = target.parentNode.id.includes("wrapper")
        ? target.parentNode
        : target;

      // Remove the hover class if the user has changed buttons
      if (this.lastTarget && this.lastTarget != newTarget) {
        this.lastTarget.classList.remove("hover");
      }
      newTarget.classList.add("hover");
      this.lastTarget = newTarget;
    } else if (this.lastTarget) {
      // Remove the hover class if the user isn't on a button
      this.lastTarget.classList.remove("hover");
    }
  }

  handleEvent(e) {
    switch (e.type) {
      case "mousedown":
        this.onClick(e.target);
        break;
      case "DOMMouseScroll":
        // Prevent scrolling. That's because we only took a screenshot of the viewport, so
        // scrolling out of the viewport wouldn't draw the expected things. In the future
        // we can take the screenshot again on scroll, but for now it doesn't seem
        // important.
        e.preventDefault();
        break;

      case "mousemove":
        this.onMouseMove(e.target);
        break;
    }
  }

  getElement(id) {
    return this.markup.getElement(this.ID_CLASS_PREFIX + id);
  }

  show(reason) {
    if (this.env.isXUL || !reason) {
      return false;
    }

    // Only track mouse movement when the the overlay is shown
    // Prevents mouse tracking when the user isn't paused
    const { pageListenerTarget } = this.env;
    pageListenerTarget.addEventListener("mousemove", this);

    // Show the highlighter's root element.
    const root = this.getElement("root");
    root.removeAttribute("hidden");
    root.setAttribute("overlay", "true");

    // Set the text to appear in the toolbar.
    const toolbar = this.getElement("toolbar");
    this.getElement("reason").setTextContent(
      PausedReasonsBundle.formatValueSync(
        DEBUGGER_PAUSED_REASONS_L10N_MAPPING[reason]
      )
    );
    toolbar.removeAttribute("hidden");

    // When the debugger pauses execution in a page, events will not be delivered
    // to any handlers added to elements on that page. So here we use the
    // document's setSuppressedEventListener interface to still be able to act on mouse
    // events (they'll be handled by the `handleEvent` method)
    this.env.window.document.setSuppressedEventListener(this);
    return true;
  }

  hide() {
    if (this.env.isXUL) {
      return;
    }

    const { pageListenerTarget } = this.env;
    pageListenerTarget.removeEventListener("mousemove", this);

    // Hide the overlay.
    this.getElement("root").setAttribute("hidden", "true");
    // Remove the hover state
    this.getElement("step-button-wrapper").classList.remove("hover");
    this.getElement("resume-button-wrapper").classList.remove("hover");
  }
}
exports.PausedDebuggerOverlay = PausedDebuggerOverlay;
