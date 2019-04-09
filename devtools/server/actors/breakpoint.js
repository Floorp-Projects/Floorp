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
      this._updateOptionsForScript(script, offsets, options);
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
    for (const offset of offsets) {
      script.setBreakpoint(offset, this);
    }

    this._updateOptionsForScript(script, offsets, this.options);
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

  // Update any state affected by changing options on a script this breakpoint
  // is associated with.
  _updateOptionsForScript(script, offsets, options) {
    // When replaying, logging breakpoints are handled using an API to get logged
    // messages from throughout the recording.
    if (this.threadActor.dbg.replaying && options.logValue) {
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
  hit: function(frame) {
    // Don't pause if we are currently stepping (in or over) or the frame is
    // black-boxed.
    const {
      generatedSourceActor,
      generatedLine,
      generatedColumn,
    } = this.threadActor.sources.getFrameLocation(frame);
    const url = generatedSourceActor.url;

    if (
      this.threadActor.sources.isBlackBoxed(url, generatedLine, generatedColumn) ||
      this.threadActor.skipBreakpoints ||
      frame.onStep
    ) {
      return undefined;
    }

    // If we're trying to pop this frame, and we see a breakpoint at
    // the spot at which popping started, ignore it.  See bug 970469.
    const locationAtFinish = frame.onPop && frame.onPop.generatedLocation;
    if (
      locationAtFinish &&
      locationAtFinish.generatedLine === generatedLine &&
      locationAtFinish.generatedColumn === generatedColumn
    ) {
      return undefined;
    }

    const reason = { type: "breakpoint", actors: [this.actorID] };
    const { condition, logValue } = this.options || {};

    // When replaying, breakpoints with log values are handled via
    // _updateOptionsForScript.
    if (logValue && this.threadActor.dbg.replaying) {
      return undefined;
    }

    if (condition) {
      const { result, message } = this.checkCondition(frame, condition);

      if (result) {
        if (message) {
          reason.type = "breakpointConditionThrown";
          reason.message = message;
        }
      } else {
        return undefined;
      }
    }

    if (logValue) {
      const displayName = formatDisplayName(frame);
      const completion = frame.evalWithBindings(`[${logValue}]`, { displayName });
      let value;
      if (!completion) {
        // The evaluation was killed (possibly by the slow script dialog).
        value = ["Log value evaluation incomplete"];
      } else if ("return" in completion) {
        value = completion.return;
      } else {
        value = [this.getThrownMessage(completion)];
      }

      if (value && typeof value.unsafeDereference === "function") {
        value = value.unsafeDereference();
      }

      const message = {
        filename: url,
        lineNumber: generatedLine,
        columnNumber: generatedColumn,
        level: "logPoint",
        arguments: value,
      };
      this.threadActor._parent._consoleActor.onConsoleAPICall(message);

      // Never stop at log points.
      return undefined;
    }

    return this.threadActor._pauseAndRespond(frame, reason);
  },

  delete: function() {
    // Remove from the breakpoint store.
    this.threadActor.breakpointActorMap.deleteActor(this.location);
    this.threadActor.threadLifetimePool.removeActor(this);
    // Remove the actual breakpoint from the associated scripts.
    this.removeScripts();
  },
};

exports.BreakpointActor = BreakpointActor;
