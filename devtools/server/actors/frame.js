/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const Debugger = require("Debugger");
const { assert } = require("devtools/shared/DevToolsUtils");
const { Pool } = require("devtools/shared/protocol/Pool");
const { createValueGrip } = require("devtools/server/actors/object/utils");
const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const { frameSpec } = require("devtools/shared/specs/frame");

function formatDisplayName(frame) {
  if (frame.type === "call") {
    const callee = frame.callee;
    return callee.name || callee.userDisplayName || callee.displayName;
  }

  return `(${frame.type})`;
}

function isDeadSavedFrame(savedFrame) {
  return Cu && Cu.isDeadWrapper(savedFrame);
}
function isValidSavedFrame(threadActor, savedFrame) {
  return (
    !isDeadSavedFrame(savedFrame) &&
    // If the frame's source is unknown to the debugger, then we ignore it
    // since the frame likely does not belong to a realm that is marked
    // as a debuggee.
    // This check will also fail if the frame would have been known but was
    // GCed before the debugger was opened on the page.
    // TODO: Use SavedFrame's security principal to limit non-debuggee frames
    // and pass all unknown frames to the debugger as a URL with no sourceID.
    getSavedFrameSource(threadActor, savedFrame)
  );
}
function getSavedFrameSource(threadActor, savedFrame) {
  return threadActor.sourcesManager.getSourceActorByInternalSourceId(
    savedFrame.sourceId
  );
}
function getSavedFrameParent(threadActor, savedFrame) {
  if (isDeadSavedFrame(savedFrame)) {
    return null;
  }

  while (true) {
    savedFrame = savedFrame.parent || savedFrame.asyncParent;

    // If the saved frame is a dead wrapper, we don't have any way to keep
    // stepping through parent frames.
    if (!savedFrame || isDeadSavedFrame(savedFrame)) {
      savedFrame = null;
      break;
    }

    if (isValidSavedFrame(threadActor, savedFrame)) {
      break;
    }
  }
  return savedFrame;
}

/**
 * An actor for a specified stack frame.
 */
const FrameActor = ActorClassWithSpec(frameSpec, {
  /**
   * Creates the Frame actor.
   *
   * @param frame Debugger.Frame|SavedFrame
   *        The debuggee frame.
   * @param threadActor ThreadActor
   *        The parent thread actor for this frame.
   */
  initialize(frame, threadActor, depth) {
    Actor.prototype.initialize.call(this, threadActor.conn);

    this.frame = frame;
    this.threadActor = threadActor;
    this.depth = depth;
  },

  /**
   * A pool that contains frame-lifetime objects, like the environment.
   */
  _frameLifetimePool: null,
  get frameLifetimePool() {
    if (!this._frameLifetimePool) {
      this._frameLifetimePool = new Pool(this.conn, "frame");
    }
    return this._frameLifetimePool;
  },

  /**
   * Finalization handler that is called when the actor is being evicted from
   * the pool.
   */
  destroy() {
    if (this._frameLifetimePool) {
      this._frameLifetimePool.destroy();
      this._frameLifetimePool = null;
    }
    Actor.prototype.destroy.call(this);
  },

  getEnvironment() {
    try {
      if (!this.frame.environment) {
        return {};
      }
    } catch (e) {
      // |this.frame| might not be live. FIXME Bug 1477030 we shouldn't be
      // using frames we derived from a point where we are not currently
      // paused at.
      return {};
    }

    const envActor = this.threadActor.createEnvironmentActor(
      this.frame.environment,
      this.frameLifetimePool
    );

    return envActor.form();
  },

  /**
   * Returns a frame form for use in a protocol message.
   */
  form() {
    // SavedFrame actors have their own frame handling.
    if (!(this.frame instanceof Debugger.Frame)) {
      // The Frame actor shouldn't be used after evaluation is resumed, so
      // there shouldn't be an easy way for the saved frame to be referenced
      // once it has died.
      assert(!isDeadSavedFrame(this.frame));

      const obj = {
        actor: this.actorID,
        // TODO: Bug 1610418 - Consider updating SavedFrame to have a type.
        type: "dead",
        asyncCause: this.frame.asyncCause,
        state: "dead",
        displayName: this.frame.functionDisplayName,
        arguments: [],
        where: {
          // The frame's source should always be known because
          // getSavedFrameParent will skip over frames with unknown sources.
          actor: getSavedFrameSource(this.threadActor, this.frame).actorID,
          line: this.frame.line,
          // SavedFrame objects have a 1-based column number, but this API and
          // Debugger API objects use a 0-based column value.
          column: this.frame.column - 1,
        },
        oldest: !getSavedFrameParent(this.threadActor, this.frame),
      };

      return obj;
    }

    const threadActor = this.threadActor;
    const form = {
      actor: this.actorID,
      type: this.frame.type,
      asyncCause: this.frame.onStack ? null : "await",
      state: this.frame.onStack ? "on-stack" : "suspended",
    };

    if (this.depth) {
      form.depth = this.depth;
    }

    if (this.frame.type != "wasmcall") {
      form.this = createValueGrip(
        this.frame.this,
        threadActor._pausePool,
        threadActor.objectGrip
      );
    }

    form.displayName = formatDisplayName(this.frame);
    form.arguments = this._args();

    if (this.frame.script) {
      const location = this.threadActor.sourcesManager.getFrameLocation(
        this.frame
      );
      form.where = {
        actor: location.sourceActor.actorID,
        line: location.line,
        column: location.column,
      };
    }

    if (!this.frame.older) {
      form.oldest = true;
    }

    return form;
  },

  _args() {
    if (!this.frame.onStack || !this.frame.arguments) {
      return [];
    }

    return this.frame.arguments.map(arg =>
      createValueGrip(
        arg,
        this.threadActor._pausePool,
        this.threadActor.objectGrip
      )
    );
  },
});

exports.FrameActor = FrameActor;
exports.formatDisplayName = formatDisplayName;
exports.getSavedFrameParent = getSavedFrameParent;
exports.isValidSavedFrame = isValidSavedFrame;
