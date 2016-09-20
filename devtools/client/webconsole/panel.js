/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft= javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const promise = require("promise");

loader.lazyGetter(this, "HUDService", () => require("devtools/client/webconsole/hudservice"));
loader.lazyGetter(this, "EventEmitter", () => require("devtools/shared/event-emitter"));

/**
 * A DevToolPanel that controls the Web Console.
 */
function WebConsolePanel(iframeWindow, toolbox) {
  this._frameWindow = iframeWindow;
  this._toolbox = toolbox;
  EventEmitter.decorate(this);
}

exports.WebConsolePanel = WebConsolePanel;

WebConsolePanel.prototype = {
  hud: null,

  /**
   * Called by the WebConsole's onkey command handler.
   * If the WebConsole is opened, check if the JSTerm's input line has focus.
   * If not, focus it.
   */
  focusInput: function () {
    this.hud.jsterm.focus();
  },

  /**
   * Open is effectively an asynchronous constructor.
   *
   * @return object
   *         A promise that is resolved when the Web Console completes opening.
   */
  open: function () {
    let parentDoc = this._toolbox.doc;
    let iframe = parentDoc.getElementById("toolbox-panel-iframe-webconsole");

    // Make sure the iframe content window is ready.
    let deferredIframe = promise.defer();
    let win, doc;
    if ((win = iframe.contentWindow) &&
        (doc = win.document) &&
        doc.readyState == "complete") {
      deferredIframe.resolve(null);
    } else {
      iframe.addEventListener("load", function onIframeLoad() {
        iframe.removeEventListener("load", onIframeLoad, true);
        deferredIframe.resolve(null);
      }, true);
    }

    // Local debugging needs to make the target remote.
    let promiseTarget;
    if (!this.target.isRemote) {
      promiseTarget = this.target.makeRemote();
    } else {
      promiseTarget = promise.resolve(this.target);
    }

    // 1. Wait for the iframe to load.
    // 2. Wait for the remote target.
    // 3. Open the Web Console.
    return deferredIframe.promise
      .then(() => promiseTarget)
      .then((target) => {
        this._frameWindow._remoteTarget = target;

        let webConsoleUIWindow = iframe.contentWindow.wrappedJSObject;
        let chromeWindow = iframe.ownerDocument.defaultView;
        return HUDService.openWebConsole(this.target, webConsoleUIWindow,
                                         chromeWindow);
      })
      .then((webConsole) => {
        this.hud = webConsole;
        this._isReady = true;
        this.emit("ready");
        return this;
      }, (reason) => {
        let msg = "WebConsolePanel open failed. " +
                  reason.error + ": " + reason.message;
        dump(msg + "\n");
        console.error(msg);
      });
  },

  get target() {
    return this._toolbox.target;
  },

  _isReady: false,
  get isReady() {
    return this._isReady;
  },

  destroy: function () {
    if (this._destroyer) {
      return this._destroyer;
    }

    this._destroyer = this.hud.destroy();
    this._destroyer.then(() => {
      this._frameWindow = null;
      this._toolbox = null;
      this.emit("destroyed");
    });

    return this._destroyer;
  },
};
