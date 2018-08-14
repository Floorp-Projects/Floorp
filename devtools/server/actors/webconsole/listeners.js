/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, components} = require("chrome");
const {isWindowIncluded} = require("devtools/shared/layout/utils");
const Services = require("Services");
const ChromeUtils = require("ChromeUtils");
const {CONSOLE_WORKER_IDS, WebConsoleUtils} = require("devtools/server/actors/webconsole/utils");

loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");

// Process script used to forward console calls from content processes to parent process
const CONTENT_PROCESS_SCRIPT = "resource://devtools/server/actors/webconsole/content-process-forward.js";


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

ConsoleReflowListener.prototype =
{
  QueryInterface: ChromeUtils.generateQI([Ci.nsIReflowObserver,
                                          Ci.nsISupportsWeakReference]),
  docshell: null,
  listener: null,

  /**
   * Forward reflow event to listener.
   *
   * @param DOMHighResTimeStamp start
   * @param DOMHighResTimeStamp end
   * @param boolean interruptible
   */
  sendReflow: function(start, end, interruptible) {
    const frame = components.stack.caller.caller;

    let filename = frame ? frame.filename : null;

    if (filename) {
      // Because filename could be of the form "xxx.js -> xxx.js -> xxx.js",
      // we only take the last part.
      filename = filename.split(" ").pop();
    }

    this.listener.onReflowActivity({
      interruptible: interruptible,
      start: start,
      end: end,
      sourceURL: filename,
      sourceLine: frame ? frame.lineNumber : null,
      functionName: frame ? frame.name : null
    });
  },

  /**
   * On uninterruptible reflow
   *
   * @param DOMHighResTimeStamp start
   * @param DOMHighResTimeStamp end
   */
  reflow: function(start, end) {
    this.sendReflow(start, end, false);
  },

  /**
   * On interruptible reflow
   *
   * @param DOMHighResTimeStamp start
   * @param DOMHighResTimeStamp end
   */
  reflowInterruptible: function(start, end) {
    this.sendReflow(start, end, true);
  },

  /**
   * Unregister listener.
   */
  destroy: function() {
    this.docshell.removeWeakReflowObserver(this);
    this.listener = this.docshell = null;
  },
};

/**
 * Forward console message calls from content processes to the parent process.
 * Used by Browser console and toolbox to see messages from all processes.
 *
 * @constructor
 * @param object owner
 *        The listener owner which needs to implement:
 *        - onConsoleAPICall(message)
 */
function ContentProcessListener(listener) {
  this.listener = listener;

  Services.ppmm.addMessageListener("Console:Log", this);
  Services.ppmm.loadProcessScript(CONTENT_PROCESS_SCRIPT, true);
}

exports.ContentProcessListener = ContentProcessListener;

ContentProcessListener.prototype = {
  receiveMessage(message) {
    const logMsg = message.data;
    logMsg.wrappedJSObject = logMsg;
    this.listener.onConsoleAPICall(logMsg);
  },

  destroy() {
    // Tell the content processes to stop listening and forwarding messages
    Services.ppmm.broadcastAsyncMessage("DevTools:StopForwardingContentProcessMessage");

    Services.ppmm.removeMessageListener("Console:Log", this);
    Services.ppmm.removeDelayedProcessScript(CONTENT_PROCESS_SCRIPT);

    this.listener = null;
  }
};

/**
 * Forward `DOMContentLoaded` and `load` events with precise timing
 * of when events happened according to window.performance numbers.
 *
 * @constructor
 * @param object console
 *        The web console actor.
 */
function DocumentEventsListener(console) {
  this.console = console;

  this.onWindowReady = this.onWindowReady.bind(this);
  this.onContentLoaded = this.onContentLoaded.bind(this);
  this.onLoad = this.onLoad.bind(this);
  this.listen();
}

exports.DocumentEventsListener = DocumentEventsListener;

DocumentEventsListener.prototype = {
  listen() {
    EventEmitter.on(this.console.parentActor, "window-ready", this.onWindowReady);
    this.onWindowReady({ window: this.console.window, isTopLevel: true });
  },

  onWindowReady({ window, isTopLevel }) {
    // Ignore iframes
    if (!isTopLevel) {
      return;
    }

    const { readyState } = window.document;
    if (readyState != "interactive" && readyState != "complete") {
      window.addEventListener("DOMContentLoaded", this.onContentLoaded, { once: true });
    } else {
      this.onContentLoaded({ target: window.document });
    }
    if (readyState != "complete") {
      window.addEventListener("load", this.onLoad, { once: true });
    } else {
      this.onLoad({ target: window.document });
    }
  },

  onContentLoaded(event) {
    const window = event.target.defaultView;
    const packet = {
      from: this.console.actorID,
      type: "documentEvent",
      name: "dom-interactive",
      // milliseconds since the UNIX epoch, when the parser finished its work
      // on the main document, that is when its Document.readyState changes to
      // 'interactive' and the corresponding readystatechange event is thrown
      time: window.performance.timing.domInteractive
    };
    this.console.conn.send(packet);
  },

  onLoad(event) {
    const window = event.target.defaultView;
    const packet = {
      from: this.console.actorID,
      type: "documentEvent",
      name: "dom-complete",
      // milliseconds since the UNIX epoch, when the parser finished its work
      // on the main document, that is when its Document.readyState changes to
      // 'complete' and the corresponding readystatechange event is thrown
      time: window.performance.timing.domComplete
    };
    this.console.conn.send(packet);
  },

  destroy() {
    EventEmitter.off(this.console.parentActor, "window-ready", this.onWindowReady);

    this.listener = null;
  }
};

