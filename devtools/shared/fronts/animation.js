/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Front,
  FrontClassWithSpec,
  custom,
  preEvent
} = require("devtools/shared/protocol");
const {
  animationPlayerSpec,
  animationsSpec
} = require("devtools/shared/specs/animation");

const AnimationPlayerFront = FrontClassWithSpec(animationPlayerSpec, {
  initialize: function(conn, form, detail, ctx) {
    Front.prototype.initialize.call(this, conn, form, detail, ctx);

    this.state = {};
  },

  form: function(form, detail) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }
    this._form = form;
    this.state = this.initialState;
  },

  destroy: function() {
    Front.prototype.destroy.call(this);
  },

  /**
   * If the AnimationsActor was given a reference to the WalkerActor previously
   * then calling this getter will return the animation target NodeFront.
   */
  get animationTargetNodeFront() {
    if (!this._form.animationTargetNodeActorID) {
      return null;
    }

    return this.conn.getActor(this._form.animationTargetNodeActorID);
  },

  /**
   * Getter for the initial state of the player. Up to date states can be
   * retrieved by calling the getCurrentState method.
   */
  get initialState() {
    return {
      type: this._form.type,
      startTime: this._form.startTime,
      previousStartTime: this._form.previousStartTime,
      currentTime: this._form.currentTime,
      playState: this._form.playState,
      playbackRate: this._form.playbackRate,
      name: this._form.name,
      duration: this._form.duration,
      delay: this._form.delay,
      endDelay: this._form.endDelay,
      iterationCount: this._form.iterationCount,
      iterationStart: this._form.iterationStart,
      easing: this._form.easing,
      fill: this._form.fill,
      direction: this._form.direction,
      animationTimingFunction: this._form.animationTimingFunction,
      isRunningOnCompositor: this._form.isRunningOnCompositor,
      propertyState: this._form.propertyState,
      documentCurrentTime: this._form.documentCurrentTime,
      createdTime: this._form.createdTime,
    };
  },

  /**
   * Executed when the AnimationPlayerActor emits a "changed" event. Used to
   * update the local knowledge of the state.
   */
  onChanged: preEvent("changed", function(partialState) {
    const {state} = this.reconstructState(partialState);
    this.state = state;
  }),

  /**
   * Refresh the current state of this animation on the client from information
   * found on the server. Doesn't return anything, just stores the new state.
   */
  async refreshState() {
    const data = await this.getCurrentState();
    if (this.currentStateHasChanged) {
      this.state = data;
    }
  },

  /**
   * getCurrentState interceptor re-constructs incomplete states since the actor
   * only sends the values that have changed.
   */
  getCurrentState: custom(function() {
    this.currentStateHasChanged = false;
    return this._getCurrentState().then(partialData => {
      const {state, hasChanged} = this.reconstructState(partialData);
      this.currentStateHasChanged = hasChanged;
      return state;
    });
  }, {
    impl: "_getCurrentState"
  }),

  reconstructState: function(data) {
    let hasChanged = false;

    for (const key in this.state) {
      if (typeof data[key] === "undefined") {
        data[key] = this.state[key];
      } else if (data[key] !== this.state[key]) {
        hasChanged = true;
      }
    }

    return {state: data, hasChanged};
  }
});

exports.AnimationPlayerFront = AnimationPlayerFront;

const AnimationsFront = FrontClassWithSpec(animationsSpec, {
  initialize: function(client, {animationsActor}) {
    Front.prototype.initialize.call(this, client, {actor: animationsActor});
    this.manage(this);
  },

  destroy: function() {
    Front.prototype.destroy.call(this);
  }
});

exports.AnimationsFront = AnimationsFront;
