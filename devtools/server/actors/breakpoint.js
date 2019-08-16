/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global assert */

"use strict";

const { formatDisplayName } = require("devtools/server/actors/frame");

/**
 * Set breakpoints on all the given entry points with the given
 * BreakpointActor as the handler.
 *
 * @param BreakpointActor actor
 *        The actor handling the breakpoint hits.
 * @param Array entryPoints
 *        An array of objects of the form `{ script, offsets }`.
 */
function setBreakpointAtEntryPoints(actor, entryPoints) {
  for (const { script, offsets } of entryPoints) {
    actor.addScript(script, offsets);
  }
}

exports.setBreakpointAtEntryPoints = setBreakpointAtEntryPoints;

/**
 * BreakpointActors are instantiated for each breakpoint that has been installed
 * by the client. They are not true actors and do not communicate with the
 * client directly, but encapsulate the DebuggerScript locations where the
 * breakpoint is installed.
 */
function BreakpointActor(threadActor, location) {
  // A map from Debugger.Script instances to the offsets which the breakpoint
  // has been set for in that script.
  this.scripts = new Map();

  this.threadActor = threadActor;
  this.location = location;
  this.options = null;
}

BreakpointActor.prototype = {
  setOptions(options) {
    for (const [script, offsets] of this.scripts) {
      this._newOffsetsOrOptions(script, offsets, this.options, options);
    }

    this.options = options;
  },

  destroy: function() {
    this.removeScripts();
  },

  hasScript: function(script) {
    return this.scripts.has(script);
  },

  /**
   * Called when this same breakpoint is added to another Debugger.Script
   * instance.
   *
   * @param script Debugger.Script
   *        The new source script on which the breakpoint has been set.
   * @param offsets Array
   *        Any offsets in the script the breakpoint is associated with.
   */
  addScript: function(script, offsets) {
    this.scripts.set(script, offsets.concat(this.scripts.get(offsets) || []));
    this._newOffsetsOrOptions(script, offsets, null, this.options);
  },

  /**
   * Remove the breakpoints from associated scripts and clear the script cache.
   */
  removeScripts: function() {
    for (const [script] of this.scripts) {
      script.clearBreakpoint(this);
    }
    this.scripts.clear();
  },

  /**
   * Called on changes to this breakpoint's script offsets or options.
   */
  _newOffsetsOrOptions(script, offsets, oldOptions, options) {
    // When replaying, logging breakpoints are handled using an API to get logged
    // messages from throughout the recording.
    if (this.threadActor.dbg.replaying && options.logValue) {
      if (
        oldOptions &&
        oldOptions.logValue == options.logValue &&
        oldOptions.condition == options.condition
      ) {
        return;
      }
      for (const offset of offsets) {
        const { lineNumber, columnNumber } = script.getOffsetLocation(offset);
        script.replayVirtualConsoleLog(
          offset,
          options.logValue,
          options.condition,
          (executionPoint, rv) => {
            const message = {
              filename: script.url,
              lineNumber,
              columnNumber,
              executionPoint,
              arguments: ["return" in rv ? rv.return : rv.throw],
              logpointId: options.logGroupId,
            };
            this.threadActor._parent._consoleActor.onConsoleAPICall(message);
          }
        );
      }
      return;
    }

    // In all other cases, this is used as a script breakpoint handler.
    // Clear any existing handler first in case this is called multiple times
    // after options change.
    for (const offset of offsets) {
      script.clearBreakpoint(this, offset);
      script.setBreakpoint(offset, this);
    }
  },

  // Get a string message to display when a frame evaluation throws.
  getThrownMessage(completion) {
    try {
      if (completion.throw.getOwnPropertyDescriptor) {
        return completion.throw.getOwnPropertyDescriptor("message").value;
      } else if (completion.toString) {
        return completion.toString();
      }
    } catch (ex) {
      // ignore
    }
    return "Unknown exception";
  },

  /**
   * Check if this breakpoint has a condition that doesn't error and
   * evaluates to true in frame.
   *
   * @param frame Debugger.Frame
   *        The frame to evaluate the condition in
   * @returns Object
   *          - result: boolean|undefined
   *            True when the conditional breakpoint should trigger a pause,
   *            false otherwise. If the condition evaluation failed/killed,
   *            `result` will be `undefined`.
   *          - message: string
   *            If the condition throws, this is the thrown message.
   */
  checkCondition: function(frame, condition) {
    const completion = frame.eval(condition);
    if (completion) {
      if (completion.throw) {
        // The evaluation failed and threw
        return {
          result: true,
          message: this.getThrownMessage(completion),
        };
      } else if (completion.yield) {
        assert(false, "Shouldn't ever get yield completions from an eval");
      } else {
        return { result: !!completion.return };
      }
    }
    // The evaluation was killed (possibly by the slow script dialog)
    return { result: undefined };
  },

  /**
   * A function that the engine calls when a breakpoint has been hit.
   *
   * @param frame Debugger.Frame
   *        The stack frame that contained the breakpoint.
   */
  /* eslint-disable complexity */
  hit: function(frame) {
    // Don't pause if we are currently stepping (in or over) or the frame is
    // black-boxed.
    const location = this.threadActor.sources.getFrameLocation(frame);
    const { sourceActor, line, column } = location;

    if (
      this.threadActor.sources.isBlackBoxed(sourceActor.url, line, column) ||
      this.threadActor.skipBreakpoints
    ) {
      return undefined;
    }

    // If we're trying to pop this frame, and we see a breakpoint at
    // the spot at which popping started, ignore it.  See bug 970469.
    const locationAtFinish = frame.onPop && frame.onPop.location;
    if (
      locationAtFinish &&
      locationAtFinish.line === line &&
      locationAtFinish.column === column
    ) {
      return undefined;
    }

    if (!this.threadActor.hasMoved(frame, "breakpoint")) {
      return undefined;
    }

    const reason = { type: "breakpoint", actors: [this.actorID] };
    const { condition, logValue } = this.options || {};

    if (condition) {
      const { result, message } = this.checkCondition(frame, condition);

      // Don't pause if the result is falsey
      if (!result) {
        return undefined;
      }

      if (message) {
        // Don't pause if there is an exception message and POE is false
        if (!this.threadActor._options.pauseOnExceptions) {
          return undefined;
        }

        reason.type = "breakpointConditionThrown";
        reason.message = message;
      }
    }

    if (logValue) {
      const displayName = formatDisplayName(frame);
      const completion = frame.evalWithBindings(`[${logValue}]`, {
        displayName,
      });
      let value;
      let level = "logPoint";

      if (!completion) {
        // The evaluation was killed (possibly by the slow script dialog).
        value = ["Log value evaluation incomplete"];
      } else if ("return" in completion) {
        value = completion.return;
      } else {
        value = [this.getThrownMessage(completion)];
        level = "logPointError";
      }

      if (value && typeof value.unsafeDereference === "function") {
        value = value.unsafeDereference();
      }

      const message = {
        filename: sourceActor.url,
        lineNumber: line,
        columnNumber: column,
        arguments: value,
        level,
      };
      this.threadActor._parent._consoleActor.onConsoleAPICall(message);

      // Never stop at log points.
      return undefined;
    }

    return this.threadActor._pauseAndRespond(frame, reason);
  },
  /* eslint-enable complexity */

  delete: function() {
    // Remove from the breakpoint store.
    this.threadActor.breakpointActorMap.deleteActor(this.location);
    this.threadActor.threadLifetimePool.removeActor(this);
    // Remove the actual breakpoint from the associated scripts.
    this.removeScripts();
  },
};

exports.BreakpointActor = BreakpointActor;
