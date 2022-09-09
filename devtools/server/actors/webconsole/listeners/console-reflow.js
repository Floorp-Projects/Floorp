/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { components } = require("chrome");

/**
 * A ReflowObserver that listens for reflow events from the page.
 * Implements nsIReflowObserver.
 *
 * @constructor
 * @param object window
 *        The window for which we need to track reflow.
 * @param object owner
 *        The listener owner which needs to implement:
 *        - onReflowActivity(reflowInfo)
 */

function ConsoleReflowListener(window, listener) {
  this.docshell = window.docShell;
  this.listener = listener;
  this.docshell.addWeakReflowObserver(this);
}

exports.ConsoleReflowListener = ConsoleReflowListener;

ConsoleReflowListener.prototype = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIReflowObserver",
    "nsISupportsWeakReference",
  ]),
  docshell: null,
  listener: null,

  /**
   * Forward reflow event to listener.
   *
   * @param DOMHighResTimeStamp start
   * @param DOMHighResTimeStamp end
   * @param boolean interruptible
   */
  sendReflow(start, end, interruptible) {
    const frame = components.stack.caller.caller;

    let filename = frame ? frame.filename : null;

    if (filename) {
      // Because filename could be of the form "xxx.js -> xxx.js -> xxx.js",
      // we only take the last part.
      filename = filename.split(" ").pop();
    }

    this.listener.onReflowActivity({
      interruptible,
      start,
      end,
      sourceURL: filename,
      sourceLine: frame ? frame.lineNumber : null,
      functionName: frame ? frame.name : null,
    });
  },

  /**
   * On uninterruptible reflow
   *
   * @param DOMHighResTimeStamp start
   * @param DOMHighResTimeStamp end
   */
  reflow(start, end) {
    this.sendReflow(start, end, false);
  },

  /**
   * On interruptible reflow
   *
   * @param DOMHighResTimeStamp start
   * @param DOMHighResTimeStamp end
   */
  reflowInterruptible(start, end) {
    this.sendReflow(start, end, true);
  },

  /**
   * Unregister listener.
   */
  destroy() {
    this.docshell.removeWeakReflowObserver(this);
    this.listener = this.docshell = null;
  },
};
