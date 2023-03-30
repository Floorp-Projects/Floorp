/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

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

const CONSOLE_THROTTLING_DELAY = 250;

class TracerActor extends Actor {
  constructor(conn, targetActor) {
    super(conn, tracerSpec);
    this.targetActor = targetActor;

    // Flag used by CONSOLE_MESSAGE resources
    this.isChromeContext = /conn\d+\.parentProcessTarget\d+/.test(
      this.targetActor.actorID
    );

    this.onEnterFrame = this.onEnterFrame.bind(this);

    this.throttledConsoleMessages = [];
    this.throttleLogMessages = throttle(
      this.flushConsoleMessages.bind(this),
      CONSOLE_THROTTLING_DELAY
    );
  }

  isTracing() {
    return !!this.dbg;
  }

  getLogMethod() {
    return this.logMethod;
  }

  startTracing(logMethod = LOG_METHODS.STDOUT) {
    this.logMethod = logMethod;

    // If we are already recording traces, only change the log method and notify about
    // the new log method to the client.
    if (!this.isTracing()) {
      // Instantiate a brand new Debugger API so that we can trace independently
      // of all other debugger operations. i.e. we can pause while tracing without any interference.
      this.dbg = this.targetActor.makeDebugger();

      this.depth = 0;

      if (this.logMethod == LOG_METHODS.STDOUT) {
        dump("Start tracing JavaScript\n");
      }
      this.dbg.onEnterFrame = this.onEnterFrame;
      this.dbg.enable();
    }

    const tracingStateWatcher = getResourceWatcher(
      this.targetActor,
      TYPES.TRACING_STATE
    );
    if (tracingStateWatcher) {
      tracingStateWatcher.onTracingToggled(true, logMethod);
    }
  }

  stopTracing() {
    if (!this.isTracing()) {
      return;
    }
    if (this.logMethod == LOG_METHODS.STDOUT) {
      dump("Stop tracing JavaScript\n");
    }

    this.dbg.onEnterFrame = undefined;
    this.dbg.disable();
    this.dbg = null;
    this.depth = 0;

    const tracingStateWatcher = getResourceWatcher(
      this.targetActor,
      TYPES.TRACING_STATE
    );
    if (tracingStateWatcher) {
      tracingStateWatcher.onTracingToggled(false);
    }
  }

  onEnterFrame(frame) {
    // Safe check, just in case we keep being notified, but the tracer has been stopped
    if (!this.dbg) {
      return;
    }
    try {
      if (this.depth == 100) {
        const message =
          "Looks like an infinite recursion? We stopped the JavaScript tracer, but code may still be running!";
        if (this.logMethod == LOG_METHODS.STDOUT) {
          dump(message + "\n");
        } else if (this.logMethod == LOG_METHODS.CONSOLE) {
          this.throttledConsoleMessages.push({
            arguments: [message],
            styles: [],
            level: "logTrace",
            chromeContext: this.isChromeContext,
          });
          this.throttleLogMessages();
        }
        this.stopTracing();
        return;
      }

      const { script } = frame;
      const { lineNumber, columnNumber } = script.getOffsetMetadata(
        frame.offset
      );

      if (this.logMethod == LOG_METHODS.STDOUT) {
        const padding = "—".repeat(this.depth + 1);
        const message = `${padding}[${frame.implementation}]—> ${
          script.source.url
        } @ ${lineNumber}:${columnNumber} - ${formatDisplayName(frame)}`;
        dump(message + "\n");
      } else if (this.logMethod == LOG_METHODS.CONSOLE) {
        const args = [
          "—".repeat(this.depth + 1),
          frame.implementation,
          "⟶",
          formatDisplayName(frame),
        ];

        // Create a message object that fits Console Message Watcher expectations
        this.throttledConsoleMessages.push({
          filename: script.source.url,
          lineNumber,
          columnNumber,
          arguments: args,
          styles: CONSOLE_ARGS_STYLES,
          level: "logTrace",
          chromeContext: this.isChromeContext,
          sourceId: script.source.id,
        });
        this.throttleLogMessages();
      }

      this.depth++;
      frame.onPop = () => {
        this.depth--;
      };
    } catch (e) {
      console.error("Exception while tracing javascript", e);
    }
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

/**
 * Try to describe the current frame we are tracing
 *
 * This will typically log the name of the method being called.
 *
 * @param {Debugger.Frame} frame
 *        The frame which is currently being executed.
 */
function formatDisplayName(frame) {
  if (frame.type === "call") {
    const callee = frame.callee;
    // Anonymous function will have undefined name and displayName.
    return "λ " + (callee.name || callee.displayName || "anonymous");
  }

  return `(${frame.type})`;
}
