/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");

loader.lazyImporter(this, "devtools", "resource://gre/modules/devtools/Loader.jsm");
loader.lazyImporter(this, "promise", "resource://gre/modules/Promise.jsm", "Promise");
loader.lazyGetter(this, "HUDService", () => require("devtools/webconsole/hudservice"));
loader.lazyGetter(this, "EventEmitter", () => require("devtools/toolkit/event-emitter"));
loader.lazyImporter(this, "gDevTools", "resource:///modules/devtools/gDevTools.jsm");

/**
 * A DevToolPanel that controls the Web Console.
 */
function WebConsolePanel(iframeWindow, toolbox)
{
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
  focusInput: function WCP_focusInput()
  {
    let inputNode = this.hud.jsterm.inputNode;

    if (!inputNode.getAttribute("focused"))
    {
      inputNode.focus();
    }
  },

  /**
   * Open is effectively an asynchronous constructor.
   *
   * @return object
   *         A promise that is resolved when the Web Console completes opening.
   */
  open: function WCP_open()
  {
    let parentDoc = this._toolbox.doc;
    let iframe = parentDoc.getElementById("toolbox-panel-iframe-webconsole");

    // Make sure the iframe content window is ready.
    let deferredIframe = promise.defer();
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
      promiseTarget = promise.resolve(this.target);
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
