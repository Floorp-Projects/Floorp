/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

this.EXPORTED_SYMBOLS = ["StyleEditorPanel"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js");
Cu.import("resource:///modules/devtools/shared/event-emitter.js");
Cu.import("resource:///modules/devtools/StyleEditorDebuggee.jsm");
Cu.import("resource:///modules/devtools/StyleEditorUI.jsm");
Cu.import("resource:///modules/devtools/StyleEditorUtil.jsm");


XPCOMUtils.defineLazyModuleGetter(this, "StyleEditorChrome",
                        "resource:///modules/devtools/StyleEditorChrome.jsm");

this.StyleEditorPanel = function StyleEditorPanel(panelWin, toolbox) {
  EventEmitter.decorate(this);

  this._toolbox = toolbox;
  this._target = toolbox.target;
  this._panelWin = panelWin;
  this._panelDoc = panelWin.document;

  this.destroy = this.destroy.bind(this);
  this._showError = this._showError.bind(this);
}

StyleEditorPanel.prototype = {
  get target() this._toolbox.target,

  get panelWindow() this._panelWin,

  /**
   * open is effectively an asynchronous constructor
   */
  open: function() {
    let deferred = Promise.defer();

    let promise;
    // We always interact with the target as if it were remote
    if (!this.target.isRemote) {
      promise = this.target.makeRemote();
    } else {
      promise = Promise.resolve(this.target);
    }

    promise.then(() => {
      this.target.on("close", this.destroy);

      this._debuggee = new StyleEditorDebuggee(this.target);

      this.UI = new StyleEditorUI(this._debuggee, this._panelDoc);
      this.UI.on("error", this._showError);

      this.isReady = true;
      deferred.resolve(this);
    })

    return deferred.promise;
  },

  /**
   * Show an error message from the style editor in the toolbox
   * notification box.
   *
   * @param  {string} event
   *         Type of event
   * @param  {string} errorCode
   *         Error code of error to report
   */
  _showError: function(event, errorCode) {
    let message = _(errorCode);
    let notificationBox = this._toolbox.getNotificationBox();
    let notification = notificationBox.getNotificationWithValue("styleeditor-error");
    if (!notification) {
      notificationBox.appendNotification(message,
        "styleeditor-error", "", notificationBox.PRIORITY_CRITICAL_LOW);
    }
  },

  /**
   * Select a stylesheet.
   *
   * @param {string} href
   *        Url of stylesheet to find and select in editor
   * @param {number} line
   *        Line number to jump to after selecting
   * @param {number} col
   *        Column number to jump to after selecting
   */
  selectStyleSheet: function(href, line, col) {
    if (!this._debuggee || !this.UI) {
      return;
    }
    let stylesheet = this._debuggee.styleSheetFromHref(href);
    this.UI.selectStyleSheet(href, line, col);
  },

  /**
   * Destroy the style editor.
   */
  destroy: function() {
    if (!this._destroyed) {
      this._destroyed = true;

      this._target.off("close", this.destroy);
      this._target = null;
      this._toolbox = null;
      this._panelDoc = null;

      this._debuggee.destroy();
      this.UI.destroy();
    }

    return Promise.resolve(null);
  },
}

XPCOMUtils.defineLazyGetter(StyleEditorPanel.prototype, "strings",
  function () {
    return Services.strings.createBundle(
            "chrome://browser/locale/devtools/styleeditor.properties");
  });
