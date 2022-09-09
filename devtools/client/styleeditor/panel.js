/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.sys.mjs");
var EventEmitter = require("devtools/shared/event-emitter");

var {
  StyleEditorUI,
} = require("resource://devtools/client/styleeditor/StyleEditorUI.jsm");
var {
  getString,
} = require("resource://devtools/client/styleeditor/StyleEditorUtil.jsm");

var StyleEditorPanel = function StyleEditorPanel(panelWin, toolbox, commands) {
  EventEmitter.decorate(this);

  this._toolbox = toolbox;
  this._commands = commands;
  this._panelWin = panelWin;
  this._panelDoc = panelWin.document;

  this._showError = this._showError.bind(this);
};

exports.StyleEditorPanel = StyleEditorPanel;

StyleEditorPanel.prototype = {
  get panelWindow() {
    return this._panelWin;
  },

  /**
   * open is effectively an asynchronous constructor
   */
  async open(options) {
    // Initialize the CSS properties database.
    const { cssProperties } = await this._toolbox.target.getFront(
      "cssProperties"
    );

    // Initialize the UI
    this.UI = new StyleEditorUI(
      this._toolbox,
      this._commands,
      this._panelDoc,
      cssProperties
    );
    this.UI.on("error", this._showError);
    await this.UI.initialize(options);

    return this;
  },

  /**
   * Show an error message from the style editor in the toolbox
   * notification box.
   *
   * @param  {string} data
   *         The parameters to customize the error message
   */
  _showError(data) {
    if (!this._toolbox) {
      // could get an async error after we've been destroyed
      return;
    }

    let errorMessage = getString(data.key);
    if (data.append) {
      errorMessage += " " + data.append;
    }

    const notificationBox = this._toolbox.getNotificationBox();
    const notification = notificationBox.getNotificationWithValue(
      "styleeditor-error"
    );

    let level = notificationBox.PRIORITY_CRITICAL_LOW;
    if (data.level === "info") {
      level = notificationBox.PRIORITY_INFO_LOW;
    } else if (data.level === "warning") {
      level = notificationBox.PRIORITY_WARNING_LOW;
    }

    if (!notification) {
      notificationBox.appendNotification(
        errorMessage,
        "styleeditor-error",
        "",
        level
      );
    }
  },

  /**
   * Select a stylesheet.
   *
   * @param {StyleSheetFront} front
   *        The front of stylesheet to find and select in editor.
   * @param {number} line
   *        Line number to jump to after selecting. One-indexed
   * @param {number} col
   *        Column number to jump to after selecting. One-indexed
   * @return {Promise}
   *         Promise that will resolve when the editor is selected and ready
   *         to be used.
   */
  selectStyleSheet(front, line, col) {
    if (!this.UI) {
      return null;
    }

    return this.UI.selectStyleSheet(front, line - 1, col ? col - 1 : 0);
  },

  /**
   * Given a location in an original file, open that file in the editor.
   *
   * @param {string} originalId
   *        The original "sourceId" returned from the sourcemap worker.
   * @param {number} line
   *        Line number to jump to after selecting. One-indexed
   * @param {number} col
   *        Column number to jump to after selecting. One-indexed
   * @return {Promise}
   *         Promise that will resolve when the editor is selected and ready
   *         to be used.
   */
  selectOriginalSheet(originalId, line, col) {
    if (!this.UI) {
      return null;
    }

    const originalSheet = this.UI.getOriginalSourceSheet(originalId);
    return this.UI.selectStyleSheet(originalSheet, line - 1, col ? col - 1 : 0);
  },

  getStylesheetFrontForGeneratedURL(url) {
    if (!this.UI) {
      return null;
    }

    return this.UI.getStylesheetFrontForGeneratedURL(url);
  },

  /**
   * Destroy the style editor.
   */
  destroy() {
    if (this._destroyed) {
      return;
    }
    this._destroyed = true;

    this._toolbox = null;
    this._panelWin = null;
    this._panelDoc = null;

    this.UI.destroy();
    this.UI = null;
  },
};

XPCOMUtils.defineLazyGetter(StyleEditorPanel.prototype, "strings", function() {
  return Services.strings.createBundle(
    "chrome://devtools/locale/styleeditor.properties"
  );
});
