/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(
  this,
  "WebConsole",
  "devtools/client/webconsole/webconsole"
);
loader.lazyGetter(this, "EventEmitter", () =>
  require("devtools/shared/event-emitter")
);

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
  focusInput: function() {
    this.hud.jsterm.focus();
  },

  /**
   * Open is effectively an asynchronous constructor.
   *
   * @return object
   *         A promise that is resolved when the Web Console completes opening.
   */
  open: async function() {
    try {
      const parentDoc = this._toolbox.doc;
      const iframe = parentDoc.getElementById(
        "toolbox-panel-iframe-webconsole"
      );

      // Make sure the iframe content window is ready.
      const win = iframe.contentWindow;
      const doc = win && win.document;
      if (!doc || doc.readyState !== "complete") {
        await new Promise(resolve => {
          iframe.addEventListener("load", resolve, {
            capture: true,
            once: true,
          });
        });
      }

      const webConsoleUIWindow = iframe.contentWindow.wrappedJSObject;
      const chromeWindow = iframe.ownerDocument.defaultView;

      // Open the Web Console.
      this.hud = new WebConsole(
        this._toolbox,
        webConsoleUIWindow,
        chromeWindow
      );
      await this.hud.init();

      // Pipe 'reloaded' event from WebConsoleUI to WebConsolePanel.
      // These events are listened by the Toolbox.
      this.hud.ui.on("reloaded", () => {
        this.emit("reloaded");
      });

      this._isReady = true;
      this.emit("ready");
    } catch (e) {
      const msg = "WebConsolePanel open failed. " + e.error + ": " + e.message;
      dump(msg + "\n");
      console.error(msg, e);
    }

    return this;
  },

  get target() {
    return this._toolbox.target;
  },

  _isReady: false,
  get isReady() {
    return this._isReady;
  },

  destroy: function() {
    if (!this._toolbox) {
      return;
    }
    this.hud.destroy();
    this.hud = null;
    this._frameWindow = null;
    this._toolbox = null;
    this.emit("destroyed");
  },
};
