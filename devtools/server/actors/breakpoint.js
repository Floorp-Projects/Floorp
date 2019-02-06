/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global assert */

"use strict";

const { ActorClassWithSpec } = require("devtools/shared/protocol");
const { breakpointSpec } = require("devtools/shared/specs/breakpoint");

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
 * BreakpointActors exist for the lifetime of their containing thread and are
 * responsible for deleting breakpoints, handling breakpoint hits and
 * associating breakpoints with scripts.
 */
const BreakpointActor = ActorClassWithSpec(breakpointSpec, {
  /**
   * Create a Breakpoint actor.
   *
   * @param ThreadActor threadActor
   *        The parent thread actor that contains this breakpoint.
   * @param GeneratedLocation generatedLocation
   *        The generated location of the breakpoint.
   */
  initialize: function(threadActor, generatedLocation) {
    // A map from Debugger.Script instances to the offsets which the breakpoint
    // has been set for in that script.
    this.scripts = new Map();

    this.threadActor = threadActor;
    this.generatedLocation = generatedLocation;
    this.options = null;
    this.isPending = true;
  },

  // Called when new breakpoint options are received from the client.
  setOptions(options) {
    for (const [script, offsets] of this.scripts) {
      this._updateOptionsForScript(script, offsets, this.options, options);
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

    this.isPending = false;
    this._updateOptionsForScript(script, offsets, null, this.options);
  },

  /**
   * Remove the breakpoints from associated scripts and clear the script cache.
   */
  removeScripts: function() {
    for (const [script, offsets] of this.scripts) {
      this._updateOptionsForScript(script, offsets, this.options, null);
      script.clearBreakpoint(this);
    }
    this.scripts.clear();
  },

  // Update any state affected by changing options on a script this breakpoint
  // is associated with.
  _updateOptionsForScript(script, offsets, oldOptions, newOptions) {
    if (this.threadActor.dbg.replaying) {
      // When replaying, logging breakpoints are handled using an API to get logged
      // messages from throughout the recording.
      const oldLogValue = oldOptions && oldOptions.logValue;
      const newLogValue = newOptions && newOptions.logValue;
      if (oldLogValue != newLogValue) {
        for (const offset of offsets) {
          const { lineNumber, columnNumber } = script.getOffsetLocation(offset);
          script.replayVirtualConsoleLog(offset, newLogValue, (point, rv) => {
            const packet = {
              from: this.actorID,
              type: "virtualConsoleLog",
              url: script.url,
              line: lineNumber,
              column: columnNumber,
              executionPoint: point,
              message: "return" in rv ? "" + rv.return : "" + rv.throw,
            };
            this.conn.send(packet);
          });
        }
      }
    }
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
        let message = "Unknown exception";
        try {
          if (completion.throw.getOwnPropertyDescriptor) {
            message = completion.throw.getOwnPropertyDescriptor("message")
                      .value;
          } else if (completion.toString) {
            message = completion.toString();
          }
        } catch (ex) {
          // ignore
        }
        return {
          result: true,
          message: message,
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

    if (this.threadActor.sources.isBlackBoxed(url, generatedLine, generatedColumn)
        || this.threadActor.skipBreakpoints
        || frame.onStep) {
      return undefined;
    }

    // If we're trying to pop this frame, and we see a breakpoint at
    // the spot at which popping started, ignore it.  See bug 970469.
    const locationAtFinish = frame.onPop && frame.onPop.generatedLocation;
    if (locationAtFinish &&
        locationAtFinish.generatedLine === generatedLine &&
        locationAtFinish.generatedColumn === generatedColumn) {
      return undefined;
    }

    const reason = {};
    const { condition, logValue } = this.options || {};

    if (!condition && !logValue) {
      reason.type = "breakpoint";
      // TODO: add the rest of the breakpoints on that line (bug 676602).
      reason.actors = [ this.actorID ];
    } else {
      // When replaying, breakpoints with log values are handled separately.
      if (logValue && this.threadActor.dbg.replaying) {
        return undefined;
      }

      let condstr = condition;
      if (logValue) {
        // In the non-replaying case, log values are handled by treating them as
        // conditions. console.log() never returns true so we will not pause.
        condstr = condition
          ? `(${condition}) && console.log(${logValue})`
          : `console.log(${logValue})`;
      }
      const { result, message } = this.checkCondition(frame, condstr);

      if (result) {
        if (!message) {
          reason.type = "breakpoint";
        } else {
          reason.type = "breakpointConditionThrown";
          reason.message = message;
        }
        reason.actors = [ this.actorID ];
      } else {
        return undefined;
      }
    }
    return this.threadActor._pauseAndRespond(frame, reason);
  },

  /**
   * Handle a protocol request to remove this breakpoint.
   */
  delete: function() {
    // Remove from the breakpoint store.
    if (this.generatedLocation) {
      this.threadActor.breakpointActorMap.deleteActor(this.generatedLocation);
    }
    this.threadActor.threadLifetimePool.removeActor(this);
    // Remove the actual breakpoint from the associated scripts.
    this.removeScripts();
  },
});

exports.BreakpointActor = BreakpointActor;
