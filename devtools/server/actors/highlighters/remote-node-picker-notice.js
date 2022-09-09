/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CanvasFrameAnonymousContentHelper,
} = require("devtools/server/actors/highlighters/utils/markup");

loader.lazyGetter(this, "HighlightersBundle", () => {
  return new Localization(["devtools/shared/highlighters.ftl"], true);
});

loader.lazyGetter(this, "isAndroid", () => {
  return Services.appinfo.OS === "Android";
});

/**
 * The RemoteNodePickerNotice is a class that displays a notice in a remote debugged page.
 * This is used to signal to users they can click/tap an element to select it in the
 * about:devtools-toolbox toolbox inspector.
 */
class RemoteNodePickerNotice {
  #highlighterEnvironment;
  #previousHoveredElement;

  rootElementId = "node-picker-notice-root";
  hideButtonId = "node-picker-notice-hide-button";
  infoNoticeElementId = "node-picker-notice-info";

  /**
   * @param {highlighterEnvironment} highlighterEnvironment
   */
  constructor(highlighterEnvironment) {
    this.#highlighterEnvironment = highlighterEnvironment;

    this.markup = new CanvasFrameAnonymousContentHelper(
      this.#highlighterEnvironment,
      this.#buildMarkup
    );
    this.isReady = this.markup.initialize();
  }

  #buildMarkup = () => {
    const container = this.markup.createNode({
      attributes: { class: "highlighter-container" },
    });

    // Wrapper element.
    const wrapper = this.markup.createNode({
      parent: container,
      attributes: {
        id: this.rootElementId,
        hidden: "true",
        overlay: "true",
      },
    });

    const toolbar = this.markup.createNode({
      parent: wrapper,
      attributes: {
        id: "node-picker-notice-toolbar",
        class: "toolbar",
      },
    });

    this.markup.createNode({
      parent: toolbar,
      attributes: {
        id: "node-picker-notice-icon",
        class: isAndroid ? "touch" : "",
      },
    });

    const actionStr = HighlightersBundle.formatValueSync(
      isAndroid
        ? "remote-node-picker-notice-action-touch"
        : "remote-node-picker-notice-action-desktop"
    );

    this.markup.createNode({
      nodeType: "span",
      parent: toolbar,
      text: HighlightersBundle.formatValueSync("remote-node-picker-notice", {
        action: actionStr,
      }),
      attributes: {
        id: this.infoNoticeElementId,
      },
    });

    this.markup.createNode({
      nodeType: "button",
      parent: toolbar,
      text: HighlightersBundle.formatValueSync(
        "remote-node-picker-notice-hide-button"
      ),
      attributes: {
        id: this.hideButtonId,
      },
    });

    return container;
  };

  destroy() {
    // hide will nullify take care of this.#abortController.
    this.hide();
    this.markup.destroy();
    this.#highlighterEnvironment = null;
    this.#previousHoveredElement = null;
  }

  /**
   * We can't use event listener directly on the anonymous content because they aren't
   * working while the page is paused.
   * This is called from the NodePicker instance for easier events management.
   *
   * @param {ClickEvent}
   */
  onClick(e) {
    const target = e.originalTarget || e.target;
    const targetId = target?.id;

    if (targetId === this.hideButtonId) {
      this.hide();
    }
  }

  /**
   * Since we can't use :hover in the CSS for the anonymous content as it wouldn't work
   * when the page is paused, we have to roll our own implementation, adding a `.hover`
   * class for the element we want to style on hover (e.g. the close button).
   * This is called from the NodePicker instance for easier events management.
   *
   * @param {MouseMoveEvent}
   */
  handleHoveredElement(e) {
    const hideButton = this.markup.getElement(this.hideButtonId);

    const target = e.originalTarget || e.target;
    const targetId = target?.id;

    // If the user didn't change targets, do nothing
    if (this.#previousHoveredElement?.id === targetId) {
      return;
    }

    if (targetId === this.hideButtonId) {
      hideButton.classList.add("hover");
    } else {
      hideButton.classList.remove("hover");
    }
    this.#previousHoveredElement = target;
  }

  getMarkupRootElement() {
    return this.markup.getElement(this.rootElementId);
  }

  async show() {
    if (this.#highlighterEnvironment.isXUL) {
      return false;
    }
    await this.isReady;

    // Show the highlighter's root element.
    const root = this.getMarkupRootElement();
    root.removeAttribute("hidden");
    root.setAttribute("overlay", "true");

    return true;
  }

  hide() {
    if (this.#highlighterEnvironment.isXUL) {
      return;
    }

    // Hide the overlay.
    this.getMarkupRootElement().setAttribute("hidden", "true");
    // Reset the hover state
    this.markup.getElement(this.hideButtonId).classList.remove("hover");
    this.#previousHoveredElement = null;
  }
}
exports.RemoteNodePickerNotice = RemoteNodePickerNotice;
