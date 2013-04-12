/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "WebConsolePanel" ];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
    "resource://gre/modules/commonjs/sdk/core/promise.js");

XPCOMUtils.defineLazyModuleGetter(this, "HUDService",
    "resource:///modules/HUDService.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "EventEmitter",
    "resource:///modules/devtools/EventEmitter.jsm");

/**
 * A DevToolPanel that controls the Web Console.
 */
function WebConsolePanel(iframeWindow, toolbox) {
  this._frameWindow = iframeWindow;
  this._toolbox = toolbox;
  EventEmitter.decorate(this);
}

WebConsolePanel.prototype = {
  hud: null,

  /**
   * Open is effectively an asynchronous constructor.
   *
   * @return object
   *         A Promise that is resolved when the Web Console completes opening.
   */
  open: function WCP_open()
  {
    let parentDoc = this._toolbox.doc;
    let iframe = parentDoc.getElementById("toolbox-panel-iframe-webconsole");
    iframe.className = "web-console-frame";

    // Make sure the iframe content window is ready.
    let deferredIframe = Promise.defer();
    let win, doc;
    if ((win = iframe.contentWindow) &&
        (doc = win.document) &&
        doc.readyState == "complete") {
      deferredIframe.resolve(null);
    }
    else {
      iframe.addEventListener("load", function onIframeLoad() {
        iframe.removeEventListener("load", onIframeLoad, true);
        deferredIframe.resolve(null);
      }, true);
    }

    // Local debugging needs to make the target remote.
    let promiseTarget;
    if (!this.target.isRemote) {
      promiseTarget = this.target.makeRemote();
    }
    else {
      promiseTarget = Promise.resolve(this.target);
    }

    // 1. Wait for the iframe to load.
    // 2. Wait for the remote target.
    // 3. Open the Web Console.
    return deferredIframe.promise
      .then(() => promiseTarget)
      .then((aTarget) => {
        this._frameWindow._remoteTarget = aTarget;

        let webConsoleUIWindow = iframe.contentWindow.wrappedJSObject;
        let chromeWindow = iframe.ownerDocument.defaultView;

        return HUDService.openWebConsole(this.target, webConsoleUIWindow,
                                         chromeWindow);
      })
      .then((aWebConsole) => {
        this.hud = aWebConsole;
        this._isReady = true;
        this.emit("ready");
        return this;
      }, (aReason) => {
        let msg = "WebConsolePanel open failed. " +
                  aReason.error + ": " + aReason.message;
        dump(msg + "\n");
        Cu.reportError(msg);
      });
  },

  get target() this._toolbox.target,

  _isReady: false,
  get isReady() this._isReady,

  destroy: function WCP_destroy()
  {
    if (this._destroyer) {
      return this._destroyer;
    }

    this._destroyer = this.hud.destroy();
    this._destroyer.then(() => this.emit("destroyed"));

    return this._destroyer;
  },
};
