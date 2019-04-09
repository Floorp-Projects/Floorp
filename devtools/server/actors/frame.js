/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ActorPool } = require("devtools/server/actors/common");
const { createValueGrip } = require("devtools/server/actors/object/utils");
const { ActorClassWithSpec } = require("devtools/shared/protocol");
const { frameSpec } = require("devtools/shared/specs/frame");

function formatDisplayName(frame) {
  if (frame.type === "call") {
    const callee = frame.callee;
    return callee.name || callee.userDisplayName || callee.displayName;
  }

  return `(${frame.type})`;
}

/**
 * An actor for a specified stack frame.
 */
const FrameActor = ActorClassWithSpec(frameSpec, {
  /**
   * Creates the Frame actor.
   *
   * @param frame Debugger.Frame
   *        The debuggee frame.
   * @param threadActor ThreadActor
   *        The parent thread actor for this frame.
   */
  initialize: function(frame, threadActor) {
    this.frame = frame;
    this.threadActor = threadActor;
  },

  /**
   * A pool that contains frame-lifetime objects, like the environment.
   */
  _frameLifetimePool: null,
  get frameLifetimePool() {
    if (!this._frameLifetimePool) {
      this._frameLifetimePool = new ActorPool(this.conn);
      this.conn.addActorPool(this._frameLifetimePool);
    }
    return this._frameLifetimePool;
  },

  /**
   * Finalization handler that is called when the actor is being evicted from
   * the pool.
   */
  destroy: function() {
    this.conn.removeActorPool(this._frameLifetimePool);
    this._frameLifetimePool = null;
  },

  getEnvironment: function() {
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
  form: function() {
    const threadActor = this.threadActor;
    const form = { actor: this.actorID, type: this.frame.type };

    // NOTE: ignoreFrameEnvironment lets the client explicitly avoid
    // populating form environments on pause.
    if (!this.threadActor._options.ignoreFrameEnvironment && this.frame.environment) {
      form.environment = this.getEnvironment();
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
      const generatedLocation = this.threadActor.sources.getFrameLocation(this.frame);
      form.where = {
        actor: generatedLocation.generatedSourceActor.actorID,
        line: generatedLocation.generatedLine,
        column: generatedLocation.generatedColumn,
      };
    }

    if (!this.frame.older) {
      form.oldest = true;
    }

    return form;
  },

  _args: function() {
    if (!this.frame.arguments) {
      return [];
    }

    return this.frame.arguments.map(arg =>
      createValueGrip(arg, this.threadActor._pausePool, this.threadActor.objectGrip)
    );
  },
});

exports.FrameActor = FrameActor;
exports.formatDisplayName = formatDisplayName;
