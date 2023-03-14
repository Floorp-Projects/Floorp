/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const { tracerSpec } = require("resource://devtools/shared/specs/tracer.js");

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

class TracerActor extends Actor {
  constructor(conn, targetActor) {
    super(conn, tracerSpec);
    this.targetActor = targetActor;

    this.onEnterFrame = this.onEnterFrame.bind(this);
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
          logConsoleMessage({
            targetActor: this.targetActor,
            source: script.source,
            args: [message],
            styles: [],
          });
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

        logConsoleMessage({
          targetActor: this.targetActor,
          source: script.source,
          lineNumber,
          columnNumber,
          args,
          styles: CONSOLE_ARGS_STYLES,
        });
      }

      this.depth++;
      frame.onPop = () => {
        this.depth--;
      };
    } catch (e) {
      console.error("Exception while tracing javascript", e);
    }
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

/**
 * Log a message to the web console.
 *
 * @param {Object} targetActor
 *        The related target actor where we want to log the message.
 *        The message will be seen as coming from one specific target.
 * @param {Debugger.Source} source
 *        The source of the location where you the message be reported to be coming from.
 * @param {Number} line
 *        The line of the location where you the message be reported to be coming from.
 * @param {Number} column
 *        The column of the location where you the message be reported to be coming from.
 * @param {Array<Object>} args
 *        Arguments of the console message to display.
 * @param {Object} styles
 *        Styles to apply to the console message.
 */
function logConsoleMessage({
  targetActor,
  source,
  lineNumber,
  columnNumber,
  args,
  styles,
}) {
  const message = {
    filename: source.url,
    lineNumber,
    columnNumber,
    arguments: args,
    styles,
    level: "logTrace",
    chromeContext:
      targetActor.actorID &&
      /conn\d+\.parentProcessTarget\d+/.test(targetActor.actorID),
    // The 'prepareConsoleMessageForRemote' method in webconsoleActor expects internal source ID,
    // thus we can't set sourceId directly to sourceActorID.
    sourceId: source.id,
  };

  // Ignore the request if the frontend isn't listening to console messages for that target.
  const consoleMessageWatcher = getResourceWatcher(
    targetActor,
    TYPES.CONSOLE_MESSAGE
  );
  if (consoleMessageWatcher) {
    consoleMessageWatcher.emitMessage(message);
  }
}
