/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

this.EXPORTED_SYMBOLS = ["StyleEditorPanel"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/commonjs/promise/core.js");
Cu.import("resource:///modules/devtools/EventEmitter.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "StyleEditorChrome",
                        "resource:///modules/devtools/StyleEditorChrome.jsm");

this.StyleEditorPanel = function StyleEditorPanel(panelWin, toolbox) {
  EventEmitter.decorate(this);

  this._toolbox = toolbox;
  this._target = toolbox.target;

  this.reset = this.reset.bind(this);
  this.newPage = this.newPage.bind(this);
  this.destroy = this.destroy.bind(this);

  this._target.on("will-navigate", this.reset);
  this._target.on("navigate", this.newPage);
  this._target.on("close", this.destroy);

  this._panelWin = panelWin;
  this._panelDoc = panelWin.document;
}

StyleEditorPanel.prototype = {
  /**
   * open is effectively an asynchronous constructor
   */
  open: function StyleEditor_open() {
    let contentWin = this._toolbox.target.window;
    this.setPage(contentWin);
    this.isReady = true;

    return Promise.resolve(this);
  },

  /**
   * Target getter.
   */
  get target() this._target,

  /**
   * Panel window getter.
   */
  get panelWindow() this._panelWin,

  /**
   * StyleEditorChrome instance getter.
   */
  get styleEditorChrome() this._panelWin.styleEditorChrome,

  /**
   * Set the page to target.
   */
  setPage: function StyleEditor_setPage(contentWindow) {
    if (this._panelWin.styleEditorChrome) {
      this._panelWin.styleEditorChrome.contentWindow = contentWindow;
    } else {
      let chromeRoot = this._panelDoc.getElementById("style-editor-chrome");
      let chrome = new StyleEditorChrome(chromeRoot, contentWindow);
      this._panelWin.styleEditorChrome = chrome;
    }
    this.selectStyleSheet(null, null, null);
  },

  /**
   * Navigated to a new page.
   */
  newPage: function StyleEditor_newPage(event, window) {
    this.setPage(window);
  },

  /**
   * No window available anymore.
   */
  reset: function StyleEditor_reset() {
    this._panelWin.styleEditorChrome.resetChrome();
  },

  /**
   * Select a stylesheet.
   */
  selectStyleSheet: function StyleEditor_selectStyleSheet(stylesheet, line, col) {
    this._panelWin.styleEditorChrome.selectStyleSheet(stylesheet, line, col);
  },

  /**
   * Destroy StyleEditor
   */
  destroy: function StyleEditor_destroy() {
    if (!this._destroyed) {
      this._destroyed = true;

      this._target.off("will-navigate", this.reset);
      this._target.off("navigate", this.newPage);
      this._target.off("close", this.destroy);
      this._target = null;
      this._toolbox = null;
      this._panelWin = null;
      this._panelDoc = null;
    }

    return Promise.resolve(null);
  },
}
