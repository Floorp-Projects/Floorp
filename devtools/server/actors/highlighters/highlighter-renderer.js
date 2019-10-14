/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

const {
  HighlighterEnvironment,
} = require("devtools/server/actors/highlighters");

const {
  CanvasFrameAnonymousContentHelper,
} = require("devtools/server/actors/highlighters/utils/markup");

/**
 * HighlighterRenderer is the base class that implements the rendering surface for a
 * highlighter in the parent process.
 *
 * It injects an iframe in the browser window which hosts the anonymous canvas
 * frame where the highlighter's markup is generated and manipulated.
 *
 * This is the renderer part of a highlighter. It has a counterpart: the observer part of
 * a highlighter which lives in the content process so it can observe changes to a node's
 * position and attributes over time. The observer communicates any changes either through
 * messages (via message manager) or directly to the renderer which updates the
 * highlighter markup.
 *
 * NOTE: When the highlighter is used in the context of the browser toolbox, for example,
 * when inspecting the browser UI, both observer and renderer live in the parent process
 * and communication is done by direct reference, not using messages.
 *
 * Classes that extend HighlighterRenderer must implement:
 * - a `typeName` string to identify the highighter type
 * - a `_buildMarkup()` method to generate the highlighter markup;
 * - a `render()` method to update the highlighter markup when given new information
 *   about the observed node.
 */
class HighlighterRenderer {
  constructor() {
    // The highlighter's type name. To be implemented by sub classes.
    this.typeName = "";
    this.onMessage = this.onMessage.bind(this);
  }

  /**
   * Create an HTML iframe in order to use the anonymous canvas frame within it
   * for hosting and manipulating the highlighter's markup.
   *
   * @param {Boolean} isBrowserToolboxHighlighter
   *        Whether the highlighter is used in the context of the
   *        browser toolbox (as opposed to the content toolbox).
   *        When set to true, this will influence where the
   *        iframe is appended so that it overlaps the browser UI
   *        (as opposed to overlapping just the page content).
   */
  init(isBrowserToolboxHighlighter) {
    // Get a reference to the parent process window.
    this.win = Services.wm.getMostRecentBrowserWindow();

    const { gBrowser } = this.win;
    // Get a reference to the selected <browser> element.
    const browser = gBrowser.selectedBrowser;
    const browserContainer = gBrowser.getBrowserContainer(browser);

    // The parent node of the iframe depends on the highlighter context:
    // - browser toolbox: top-level browser window
    // - content toolbox: host node of the <browser> element for the selected tab
    const parent = isBrowserToolboxHighlighter
      ? this.win.document.documentElement
      : browserContainer.querySelector(".browserStack");

    // Grab the host iframe if it was previously created by another highlighter.
    const iframe = parent.querySelector(
      `:scope > .devtools-highlighter-renderer`
    );

    if (iframe) {
      this.iframe = iframe;
      this.setupMarkup();
    } else {
      this.iframe = this.win.document.createElement("iframe");
      this.iframe.classList.add("devtools-highlighter-renderer");

      if (isBrowserToolboxHighlighter) {
        parent.append(this.iframe);
      } else {
        // Ensure highlighters are drawn underneath alerts and dialog boxes.
        parent.querySelector("browser").after(this.iframe);
      }

      this.iframe.contentWindow.addEventListener(
        "DOMContentLoaded",
        this.setupMarkup.bind(this)
      );
    }
  }

  /**
   * Generate the highlighter markup and insert it into the anoymous canvas frame.
   */
  setupMarkup() {
    if (!this.iframe || !this.iframe.contentWindow) {
      throw Error(
        "The highlighter renderer's host iframe is missing or not yet ready"
      );
    }

    this.highlighterEnv = new HighlighterEnvironment();
    this.highlighterEnv.initFromWindow(this.iframe.contentWindow);

    this.markup = new CanvasFrameAnonymousContentHelper(
      this.highlighterEnv,
      this._buildMarkup.bind(this)
    );
  }

  /**
   * Set up message manager listener to listen for messages
   * coming from the from the child process.
   *
   * @param {Object} mm
   *        Message manager that corresponds to the current content tab.
   * @param {String} prefix
   *        Cross-process connection prefix.
   */
  setMessageManager(mm, prefix) {
    if (this.messageManager === mm) {
      return;
    }

    // Message name used to distinguish between messages over the message manager.
    this._msgName = `debug:${prefix}${this.typeName}`;

    if (this.messageManager) {
      // If the browser was swapped we need to reset the message manager.
      const oldMM = this.messageManager;
      oldMM.removeMessageListener(this._msgName, this.onMessage);
    }

    this.messageManager = mm;
    if (mm) {
      mm.addMessageListener(this._msgName, this.onMessage);
    }
  }

  postMessage(topic, data = {}) {
    this.messageManager.sendAsyncMessage(`${this._msgName}:event`, {
      topic,
      data,
    });
  }

  /**
   * Handler for messages coming from the content process.
   *
   * @param  {Object} msg
   *         Data payload associated with the message.
   */
  onMessage(msg) {
    const { topic, data } = msg.json;
    switch (topic) {
      case "render":
        this.render(data);
        break;

      case "destroy":
        this.destroy();
        break;
    }
  }

  render() {
    // When called, sub classes should update the highlighter.
    // To be implemented by sub classes.
    throw new Error(
      "Highlighter renderer class had to implement render method"
    );
  }

  destroy() {
    if (this.highlighterEnv) {
      this.highlighterEnv.destroy();
      this.highlighterEnv = null;
    }

    if (this.markup) {
      this.markup.destroy();
      this.markup = null;
    }

    if (this.iframe) {
      this.iframe.remove();
      this.iframe = null;
    }

    this.win = null;
    this.setMessageManager(null);
  }
}
exports.HighlighterRenderer = HighlighterRenderer;
