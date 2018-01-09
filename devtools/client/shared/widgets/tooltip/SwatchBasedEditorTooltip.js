/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/old-event-emitter");
const KeyShortcuts = require("devtools/client/shared/key-shortcuts");
const {HTMLTooltip} = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");
const InlineTooltip = require("devtools/client/shared/widgets/tooltip/InlineTooltip");

const INLINE_TOOLTIP_CLASS = "inline-tooltip-container";

/**
 * Base class for all (color, gradient, ...)-swatch based value editors inside
 * tooltips
 *
 * @param {Document} document
 *        The document to attach the SwatchBasedEditorTooltip. This is either the toolbox
 *        document if the tooltip is a popup tooltip or the panel's document if it is an
 *        inline editor.
 * @param {Boolean} useInline
 *        A boolean flag representing whether or not the InlineTooltip should be used.
 */

class SwatchBasedEditorTooltip {
  constructor(document, useInline) {
    EventEmitter.decorate(this);

    this.useInline = useInline;

    // Creating a tooltip instance
    if (useInline) {
      this.tooltip = new InlineTooltip(document);
    } else {
      // This one will consume outside clicks as it makes more sense to let the user
      // close the tooltip by clicking out
      // It will also close on <escape> and <enter>
      this.tooltip = new HTMLTooltip(document, {
        type: "arrow",
        consumeOutsideClicks: true,
        useXulWrapper: true,
      });
    }

    // By default, swatch-based editor tooltips revert value change on <esc> and
    // commit value change on <enter>
    this.shortcuts = new KeyShortcuts({
      window: this.tooltip.topWindow
    });
    this.shortcuts.on("Escape", (name, event) => {
      if (!this.tooltip.isVisible()) {
        return;
      }
      this.revert();
      this.hide();
      event.stopPropagation();
      event.preventDefault();
    });
    this.shortcuts.on("Return", (name, event) => {
      if (!this.tooltip.isVisible()) {
        return;
      }
      this.commit();
      this.hide();
      event.stopPropagation();
      event.preventDefault();
    });

    // All target swatches are kept in a map, indexed by swatch DOM elements
    this.swatches = new Map();

    // When a swatch is clicked, and for as long as the tooltip is shown, the
    // activeSwatch property will hold the reference to the swatch DOM element
    // that was clicked
    this.activeSwatch = null;

    this._onSwatchClick = this._onSwatchClick.bind(this);
  }

 /**
   * Reports if the tooltip is currently shown
   *
   * @return {Boolean} True if the tooltip is displayed.
   */
  isVisible() {
    return this.tooltip.isVisible();
  }

  /**
   * Reports if the tooltip is currently editing the targeted value
   *
   * @return {Boolean} True if the tooltip is editing.
   */
  isEditing() {
    return this.isVisible();
  }

  /**
   * Show the editor tooltip for the currently active swatch.
   *
   * @return {Promise} a promise that resolves once the editor tooltip is displayed, or
   *         immediately if there is no currently active swatch.
   */
  show() {
    let tooltipAnchor = this.useInline ?
      this.activeSwatch.closest(`.${INLINE_TOOLTIP_CLASS}`) :
      this.activeSwatch;

    if (tooltipAnchor) {
      let onShown = this.tooltip.once("shown");

      this.tooltip.show(tooltipAnchor, "topcenter bottomleft");
      this.tooltip.once("hidden", () => this.onTooltipHidden());

      return onShown;
    }

    return Promise.resolve();
  }

  /**
   * Can be overridden by subclasses if implementation specific behavior is needed on
   * tooltip hidden.
   */
  onTooltipHidden() {
    // When the tooltip is closed by clicking outside the panel we want to commit any
    // changes.
    if (!this._reverted) {
      this.commit();
    }
    this._reverted = false;

    // Once the tooltip is hidden we need to clean up any remaining objects.
    this.activeSwatch = null;
  }

  hide() {
    this.tooltip.hide();
  }

  /**
   * Add a new swatch DOM element to the list of swatch elements this editor
   * tooltip knows about. That means from now on, clicking on that swatch will
   * toggle the editor.
   *
   * @param {node} swatchEl
   *        The element to add
   * @param {object} callbacks
   *        Callbacks that will be executed when the editor wants to preview a
   *        value change, or revert a change, or commit a change.
   *        - onShow: will be called when one of the swatch tooltip is shown
   *        - onPreview: will be called when one of the sub-classes calls
   *        preview
   *        - onRevert: will be called when the user ESCapes out of the tooltip
   *        - onCommit: will be called when the user presses ENTER or clicks
   *        outside the tooltip.
   */
  addSwatch(swatchEl, callbacks = {}) {
    if (!callbacks.onShow) {
      callbacks.onShow = function () {};
    }
    if (!callbacks.onPreview) {
      callbacks.onPreview = function () {};
    }
    if (!callbacks.onRevert) {
      callbacks.onRevert = function () {};
    }
    if (!callbacks.onCommit) {
      callbacks.onCommit = function () {};
    }

    this.swatches.set(swatchEl, {
      callbacks: callbacks
    });
    swatchEl.addEventListener("click", this._onSwatchClick);
  }

  removeSwatch(swatchEl) {
    if (this.swatches.has(swatchEl)) {
      if (this.activeSwatch === swatchEl) {
        this.hide();
        this.activeSwatch = null;
      }
      swatchEl.removeEventListener("click", this._onSwatchClick);
      this.swatches.delete(swatchEl);
    }
  }

  _onSwatchClick(event) {
    let swatch = this.swatches.get(event.target);

    if (event.shiftKey) {
      event.stopPropagation();
      return;
    }
    if (swatch) {
      this.activeSwatch = event.target;
      this.show();
      swatch.callbacks.onShow();
      event.stopPropagation();
    }
  }

  /**
   * Not called by this parent class, needs to be taken care of by sub-classes
   */
  preview(value) {
    if (this.activeSwatch) {
      let swatch = this.swatches.get(this.activeSwatch);
      swatch.callbacks.onPreview(value);
    }
  }

  /**
   * This parent class only calls this on <esc> keypress
   */
  revert() {
    if (this.activeSwatch) {
      this._reverted = true;
      let swatch = this.swatches.get(this.activeSwatch);
      this.tooltip.once("hidden", () => {
        swatch.callbacks.onRevert();
      });
    }
  }

  /**
   * This parent class only calls this on <enter> keypress
   */
  commit() {
    if (this.activeSwatch) {
      let swatch = this.swatches.get(this.activeSwatch);
      swatch.callbacks.onCommit();
    }
  }

  destroy() {
    this.swatches.clear();
    this.activeSwatch = null;
    this.tooltip.off("keypress", this._onTooltipKeypress);
    this.tooltip.destroy();
    this.shortcuts.destroy();
  }
}

module.exports = SwatchBasedEditorTooltip;
