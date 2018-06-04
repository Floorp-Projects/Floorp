/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { L10N } = require("./utils/l10n");

class Picker {
  constructor(panel) {
    this._panel = panel;

    this.isPicking = false;

    this.onPickerAccessibleHovered = this.onPickerAccessibleHovered.bind(this);
    this.onPickerAccessiblePicked = this.onPickerAccessiblePicked.bind(this);
    this.onPickerAccessiblePreviewed = this.onPickerAccessiblePreviewed.bind(this);
    this.onPickerAccessibleCanceled = this.onPickerAccessibleCanceled.bind(this);
  }

  get toolbox() {
    return this._panel._toolbox;
  }

  get walker() {
    return this._panel.walker;
  }

  get pickerButton() {
    return this.toolbox.pickerButton;
  }

  get _telemetry() {
    return this._panel._telemetry;
  }

  release() {
    this._panel = null;
  }

  emit(event, data) {
    this.toolbox.emit(event, data);
  }

  /**
   * Select accessible object in the tree.
   * @param  {Object} accessible
   *         Accessiblle object to be selected in the inspector tree.
   */
  select(accessible) {
    this._panel.selectAccessible(accessible);
  }

  /**
   * Highlight accessible object in the tree.
   * @param  {Object} accessible
   *         Accessiblle object to be selected in the inspector tree.
   */
  highlight(accessible) {
    this._panel.highlightAccessible(accessible);
  }

  getStr(key) {
    return L10N.getStr(key);
  }

  /**
   * Override the default presentation of the picker button in toolbox's top
   * level toolbar.
   */
  updateButton() {
    this.pickerButton.description = this.getStr("accessibility.pick");
    this.pickerButton.className = "accessibility";
    this.pickerButton.disabled = !this._panel.front.enabled;
    if (!this._panel.front.enabled && this.isPicking) {
      this.cancel();
    }
  }

  /**
   * Handle an event when a new accessible object is hovered over.
   * @param  {Object} accessible
   *         newly hovered accessible object
   */
  onPickerAccessibleHovered(accessible) {
    if (accessible) {
      this.emit("picker-accessible-hovered", accessible);
      this.highlight(accessible);
    }
  }

  /**
   * Handle an event when a new accessible is picked by the user.
   * @param  {Object} accessible
   *         newly picked accessible object
   */
  onPickerAccessiblePicked(accessible) {
    if (accessible) {
      this.select(accessible);
    }
    this.stop();
  }

  /**
   * Handle an event when a new accessible is previewed by the user.
   * @param  {Object} accessible
   *         newly previewed accessible object
   */
  onPickerAccessiblePreviewed(accessible) {
    if (accessible) {
      this.select(accessible);
    }
  }

  /**
   * Handle an event when picking is cancelled by the user.s
   */
  onPickerAccessibleCanceled() {
    this.cancel();
    this.emit("picker-accessible-canceled");
  }

  /**
   * Cancel picking.
   */
  async cancel() {
    await this.stop();
  }

  /**
   * Stop picking and remove all walker listeners.
   */
  async stop() {
    if (!this.isPicking) {
      return;
    }

    this.isPicking = false;

    this.pickerButton.isChecked = false;

    await this.walker.cancelPick();

    this._telemetry.toolClosed("accessibility_picker");

    this.walker.off("picker-accessible-hovered", this.onPickerAccessibleHovered);
    this.walker.off("picker-accessible-picked", this.onPickerAccessiblePicked);
    this.walker.off("picker-accessible-previewed", this.onPickerAccessiblePreviewed);
    this.walker.off("picker-accessible-canceled", this.onPickerAccessibleCanceled);

    this.emit("picker-stopped");
  }

  /**
   * Start picking and add walker listeners.
   * @param  {Boolean} doFocus
   *         If true, move keyboard focus into content.
   */
  async start(doFocus = true) {
    if (this.isPicking) {
      return;
    }

    this.isPicking = true;

    this.pickerButton.isChecked = true;

    this.walker.on("picker-accessible-hovered", this.onPickerAccessibleHovered);
    this.walker.on("picker-accessible-picked", this.onPickerAccessiblePicked);
    this.walker.on("picker-accessible-previewed", this.onPickerAccessiblePreviewed);
    this.walker.on("picker-accessible-canceled", this.onPickerAccessibleCanceled);

    await this.walker.pick(doFocus);

    this._telemetry.toolOpened("accessibility_picker");

    this.emit("picker-started");
  }

  /**
   * Toggle between starting and canceling the picker.
   * @param  {Boolean} doFocus
   *         If true, move keyboard focus into content.
   */
  toggle(doFocus) {
    if (this.isPicking) {
      return this.cancel();
    }

    return this.start(doFocus);
  }
}

exports.Picker = Picker;
