/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Bug 1827382, as this module can be used from the worker thread,
// the following JSM may be loaded by the worker loader until
// we have proper support for ESM from workers.
const {
  startTracing,
  stopTracing,
  addTracingListener,
  removeTracingListener,
} = require("resource://devtools/server/tracer/tracer.jsm");

const { Actor } = require("resource://devtools/shared/protocol.js");
const { tracerSpec } = require("resource://devtools/shared/specs/tracer.js");

const { throttle } = require("resource://devtools/shared/throttle.js");

const {
  TYPES,
  getResourceWatcher,
} = require("resource://devtools/server/actors/resources/index.js");

const LOG_METHODS = {
  STDOUT: "stdout",
  CONSOLE: "console",
};

const CONSOLE_ARGS_STYLES = [
  "color: var(--theme-toolbarbutton-checked-hover-background)",
  "padding-inline: 4px; margin-inline: 2px; background-color: var(--theme-toolbarbutton-checked-hover-background); color: var(--theme-toolbarbutton-checked-hover-color);",
  "",
  "color: var(--theme-highlight-blue); margin-inline: 2px;",
];

const DOM_EVENT_CONSOLE_ARGS_STYLES = [
  "color: var(--theme-toolbarbutton-checked-hover-background)",
  "padding-inline: 4px; margin-inline: 2px; background-color: var(--toolbarbutton-checked-background); color: var(--toolbarbutton-checked-color);",
];

const CONSOLE_THROTTLING_DELAY = 250;

class TracerActor extends Actor {
  constructor(conn, targetActor) {
    super(conn, tracerSpec);
    this.targetActor = targetActor;
    this.sourcesManager = this.targetActor.sourcesManager;

    // Flag used by CONSOLE_MESSAGE resources
    this.isChromeContext = /conn\d+\.parentProcessTarget\d+/.test(
      this.targetActor.actorID
    );

    this.throttledConsoleMessages = [];
    this.throttleLogMessages = throttle(
      this.flushConsoleMessages.bind(this),
      CONSOLE_THROTTLING_DELAY
    );
  }

  destroy() {
    this.stopTracing();
  }

  getLogMethod() {
    return this.logMethod;
  }

  startTracing(logMethod = LOG_METHODS.STDOUT) {
    this.logMethod = logMethod;
    this.tracingListener = {
      onTracingFrame: this.onTracingFrame.bind(this),
      onTracingInfiniteLoop: this.onTracingInfiniteLoop.bind(this),
    };
    addTracingListener(this.tracingListener);
    startTracing({
      global: this.targetActor.window || this.targetActor.workerGlobal,
      // Enable receiving the `currentDOMEvent` being passed to `onTracingFrame`
      traceDOMEvents: true,
    });
  }

  stopTracing() {
    if (!this.tracingListener) {
      return;
    }
    stopTracing();
    removeTracingListener(this.tracingListener);
    this.logMethod = null;
    this.tracingListener = null;
  }

  onTracingInfiniteLoop() {
    if (this.logMethod == LOG_METHODS.STDOUT) {
      return true;
    }
    const consoleMessageWatcher = getResourceWatcher(
      this.targetActor,
      TYPES.CONSOLE_MESSAGE
    );
    if (!consoleMessageWatcher) {
      return true;
    }

    const message =
      "Looks like an infinite recursion? We stopped the JavaScript tracer, but code may still be running!";
    consoleMessageWatcher.emitMessages([
      {
        arguments: [message],
        styles: [],
        level: "logTrace",
        chromeContext: this.isChromeContext,
        timeStamp: ChromeUtils.dateNow(),
      },
    ]);

    return false;
  }

  /**
   * Called by JavaScriptTracer class when a new JavaScript frame is executed.
   *
   * @param {Debugger.Frame} frame
   *        A descriptor object for the JavaScript frame.
   * @param {Number} depth
   *        Represents the depth of the frame in the call stack.
   * @param {String} formatedDisplayName
   *        A human readable name for the current frame.
   * @param {String} prefix
   *        A string to be displayed as a prefix of any logged frame.
   * @param {String} currentDOMEvent
   *        If this is a top level frame (depth==0), and we are currently processing
   *        a DOM Event, this will refer to the name of that DOM Event.
   *        Note that it may also refer to setTimeout and setTimeout callback calls.
   * @return {Boolean}
   *         Return true, if the JavaScriptTracer should log the frame to stdout.
   */
  onTracingFrame({
    frame,
    depth,
    formatedDisplayName,
    prefix,
    currentDOMEvent,
  }) {
    const { script } = frame;
    const { lineNumber, columnNumber } = script.getOffsetMetadata(frame.offset);
    const url = script.source.url;

    // Ignore blackboxed sources
    if (this.sourcesManager.isBlackBoxed(url, lineNumber, columnNumber)) {
      return false;
    }

    if (this.logMethod == LOG_METHODS.STDOUT) {
      // By returning true, we let JavaScriptTracer class log the message to stdout.
      return true;
    }

    // We may receive the currently processed DOM event (if this relates to one).
    // In this case, log a preliminary message, which looks different to highlight it.
    if (currentDOMEvent && depth == 0) {
      const DOMEventArgs = [prefix + "—", currentDOMEvent];

      // Create a message object that fits Console Message Watcher expectations
      this.throttledConsoleMessages.push({
        arguments: DOMEventArgs,
        styles: DOM_EVENT_CONSOLE_ARGS_STYLES,
        level: "logTrace",
        chromeContext: this.isChromeContext,
        timeStamp: ChromeUtils.dateNow(),
      });
    }

    const args = [
      prefix + "—".repeat(depth + 1),
      frame.implementation,
      "⟶",
      formatedDisplayName,
    ];

    // Create a message object that fits Console Message Watcher expectations
    this.throttledConsoleMessages.push({
      filename: url,
      lineNumber,
      columnNumber,
      arguments: args,
      styles: CONSOLE_ARGS_STYLES,
      level: "logTrace",
      chromeContext: this.isChromeContext,
      sourceId: script.source.id,
      timeStamp: ChromeUtils.dateNow(),
    });
    this.throttleLogMessages();

    return false;
  }

  /**
   * This method is throttled and will notify all pending traces to be logged in the console
   * via the console message watcher.
   */
  flushConsoleMessages() {
    const consoleMessageWatcher = getResourceWatcher(
      this.targetActor,
      TYPES.CONSOLE_MESSAGE
    );
    // Ignore the request if the frontend isn't listening to console messages for that target.
    if (!consoleMessageWatcher) {
      return;
    }
    const messages = this.throttledConsoleMessages;
    this.throttledConsoleMessages = [];

    consoleMessageWatcher.emitMessages(messages);
  }
}
exports.TracerActor = TracerActor;
