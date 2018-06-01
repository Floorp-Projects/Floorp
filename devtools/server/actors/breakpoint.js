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
    actor.addScript(script);
    for (const offset of offsets) {
      script.setBreakpoint(offset, actor);
    }
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
   * @param OriginalLocation originalLocation
   *        The original location of the breakpoint.
   */
  initialize: function(threadActor, originalLocation) {
    // The set of Debugger.Script instances that this breakpoint has been set
    // upon.
    this.scripts = new Set();

    this.threadActor = threadActor;
    this.originalLocation = originalLocation;
    this.condition = null;
    this.isPending = true;
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
   */
  addScript: function(script) {
    this.scripts.add(script);
    this.isPending = false;
  },

  /**
   * Remove the breakpoints from associated scripts and clear the script cache.
   */
  removeScripts: function() {
    for (const script of this.scripts) {
      script.clearBreakpoint(this);
    }
    this.scripts.clear();
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
  checkCondition: function(frame) {
    const completion = frame.eval(this.condition);
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
          message: message
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
    const generatedLocation = this.threadActor.sources.getFrameLocation(frame);
    const {
      originalSourceActor,
      originalLine,
      originalColumn
    } = this.threadActor.unsafeSynchronize(
      this.threadActor.sources.getOriginalLocation(generatedLocation));
    const url = originalSourceActor.url;

    if (this.threadActor.sources.isBlackBoxed(url)
        || this.threadActor.skipBreakpoints
        || frame.onStep) {
      return undefined;
    }

    // If we're trying to pop this frame, and we see a breakpoint at
    // the spot at which popping started, ignore it.  See bug 970469.
    const locationAtFinish = frame.onPop && frame.onPop.originalLocation;
    if (locationAtFinish &&
        locationAtFinish.originalLine === originalLine &&
        locationAtFinish.originalColumn === originalColumn) {
      return undefined;
    }

    const reason = {};

    if (this.threadActor._hiddenBreakpoints.has(this.actorID)) {
      reason.type = "pauseOnDOMEvents";
    } else if (!this.condition) {
      reason.type = "breakpoint";
      // TODO: add the rest of the breakpoints on that line (bug 676602).
      reason.actors = [ this.actorID ];
    } else {
      const { result, message } = this.checkCondition(frame);

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
    if (this.originalLocation) {
      this.threadActor.breakpointActorMap.deleteActor(this.originalLocation);
    }
    this.threadActor.threadLifetimePool.removeActor(this);
    // Remove the actual breakpoint from the associated scripts.
    this.removeScripts();
  }
});

exports.BreakpointActor = BreakpointActor;
