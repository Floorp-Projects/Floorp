/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ThreadStateTypes } = require("devtools/shared/client/constants");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { threadSpec } = require("devtools/shared/specs/thread");

loader.lazyRequireGetter(
  this,
  "ObjectClient",
  "devtools/shared/client/object-client"
);
loader.lazyRequireGetter(
  this,
  "SourceFront",
  "devtools/shared/fronts/source",
  true
);

/**
 * Creates a thread front for the remote debugging protocol server. This client
 * is a front to the thread actor created in the server side, hiding the
 * protocol details in a traditional JavaScript API.
 *
 * @param client DebuggerClient
 * @param actor string
 *        The actor ID for this thread.
 */
class ThreadFront extends FrontClassWithSpec(threadSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);
    this.client = client;
    this._pauseGrips = {};
    this._threadGrips = {};
    this._state = "paused";
    this._beforePaused = this._beforePaused.bind(this);
    this._beforeResumed = this._beforeResumed.bind(this);
    this._beforeDetached = this._beforeDetached.bind(this);
    this.before("paused", this._beforePaused);
    this.before("resumed", this._beforeResumed);
    this.before("detached", this._beforeDetached);
    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "threadActor";
  }

  get state() {
    return this._state;
  }

  get paused() {
    return this._state === "paused";
  }

  get actor() {
    return this.actorID;
  }

  _assertPaused(command) {
    if (!this.paused) {
      throw Error(
        command + " command sent while not paused. Currently " + this._state
      );
    }
  }

  /**
   * Resume a paused thread. If the optional limit parameter is present, then
   * the thread will also pause when that limit is reached.
   *
   * @param [optional] object limit
   *        An object with a type property set to the appropriate limit (next,
   *        step, or finish) per the remote debugging protocol specification.
   *        Use null to specify no limit.
   * @param bool aRewind
   *        Whether execution should rewind until the limit is reached, rather
   *        than proceeding forwards. This parameter has no effect if the
   *        server does not support rewinding.
   */
  async _doResume(resumeLimit, rewind) {
    this._assertPaused("resume");

    // Put the client in a tentative "resuming" state so we can prevent
    // further requests that should only be sent in the paused state.
    this._previousState = this._state;
    this._state = "resuming";
    try {
      await super.resume(resumeLimit, rewind);
    } catch (e) {
      if (this._state == "resuming") {
        // There was an error resuming, update the state to the new one
        // reported by the server, if given (only on wrongState), otherwise
        // reset back to the previous state.
        if (e.state) {
          this._state = ThreadStateTypes[e.state];
        } else {
          this._state = this._previousState;
        }
      }
    }

    delete this._previousState;
  }

  /**
   * Resume a paused thread.
   */
  resume() {
    return this._doResume(null, false);
  }

  /**
   * Resume then pause without stepping.
   *
   */
  resumeThenPause() {
    return this._doResume({ type: "break" }, false);
  }

  /**
   * Rewind a thread until a breakpoint is hit.
   */
  async rewind() {
    if (!this.paused) {
      this.interrupt();
      await this.once("paused");
    }

    this._doResume(null, true);
  }

  /**
   * Step over a function call.
   */
  stepOver() {
    return this._doResume({ type: "next" }, false);
  }

  /**
   * Step into a function call.
   */
  stepIn() {
    return this._doResume({ type: "step" }, false);
  }

  /**
   * Step out of a function call.
   */
  stepOut() {
    return this._doResume({ type: "finish" }, false);
  }

  /**
   * Rewind step over a function call.
   */
  reverseStepOver() {
    return this._doResume({ type: "next" }, true);
  }

  /**
   * Immediately interrupt a running thread.
   */
  interrupt() {
    return this._doInterrupt(null);
  }

  /**
   * Pause execution right before the next JavaScript bytecode is executed.
   */
  breakOnNext() {
    return this._doInterrupt("onNext");
  }

  /**
   * Warp through time to an execution point in the past or future.
   *
   * @param object aTarget
   *        Description of the warp destination.
   */
  timeWarp(target) {
    const warp = () => {
      this._doResume({ type: "warp", target }, true);
    };
    if (this.paused) {
      return warp();
    }

    this.interrupt();
    return this.once("paused", warp);
  }

  /**
   * Interrupt a running thread.
   */
  _doInterrupt(when) {
    return super.interrupt(when);
  }

  /**
   * Request the loaded sources for the current thread.
   */
  async getSources() {
    let sources = [];
    try {
      ({ sources } = await super.sources());
    } catch (e) {
      // we may have closed the connection
      console.log(`getSources failed. Connection may have closed: ${e}`);
    }
    return { sources };
  }

  /**
   * Request frames from the callstack for the current thread.
   *
   * @param start integer
   *        The number of the youngest stack frame to return (the youngest
   *        frame is 0).
   * @param count integer
   *        The maximum number of frames to return, or null to return all
   *        frames.
   */
  getFrames(start, count) {
    return super.frames(start, count);
  }
  /**
   * attach to the thread actor.
   */
  async attach(options) {
    let response;
    try {
      const onPaused = this.once("paused");
      response = await super.attach(options);
      await onPaused;
    } catch (e) {
      throw new Error(e);
    }
    return response;
  }

  /**
   * Detach from the thread actor.
   */
  async detach() {
    const onDetached = this.once("detached");
    await super.detach();
    await onDetached;
    await this.destroy();
  }

  /**
   * Request the frame environment.
   *
   * @param frameId string
   */
  getEnvironment(frameId) {
    return this.client.request({ to: frameId, type: "getEnvironment" });
  }

  /**
   * Return a ObjectClient object for the given object grip.
   *
   * @param grip object
   *        A pause-lifetime object grip returned by the protocol.
   */
  pauseGrip(grip) {
    if (grip.actor in this._pauseGrips) {
      return this._pauseGrips[grip.actor];
    }

    const client = new ObjectClient(this.client, grip);
    this._pauseGrips[grip.actor] = client;
    return client;
  }

  /**
   * Clear and invalidate all the grip clients from the given cache.
   *
   * @param gripCacheName
   *        The property name of the grip cache we want to clear.
   */
  _clearObjectClients(gripCacheName) {
    for (const id in this[gripCacheName]) {
      this[gripCacheName][id].valid = false;
    }
    this[gripCacheName] = {};
  }

  /**
   * Invalidate pause-lifetime grip clients and clear the list of current grip
   * clients.
   */
  _clearPauseGrips() {
    this._clearObjectClients("_pauseGrips");
  }

  /**
   * Invalidate thread-lifetime grip clients and clear the list of current grip
   * clients.
   */
  _clearThreadGrips() {
    this._clearObjectClients("_threadGrips");
  }

  _beforePaused(packet) {
    this._state = "paused";
    this._onThreadState(packet);
  }

  _beforeResumed() {
    this._state = "attached";
    this._onThreadState(null);
  }

  _beforeDetached(packet) {
    this._state = "detached";
    this._onThreadState(packet);
    this._clearThreadGrips();
  }

  /**
   * Handle thread state change by doing necessary cleanup
   */
  _onThreadState(packet) {
    // The debugger UI may not be initialized yet so we want to keep
    // the packet around so it knows what to pause state to display
    // when it's initialized
    this._lastPausePacket = packet;
    this._clearPauseGrips();
  }

  getLastPausePacket() {
    return this._lastPausePacket;
  }

  /**
   * Return an instance of SourceFront for the given source actor form.
   */
  source(form) {
    if (form.actor in this._threadGrips) {
      return this._threadGrips[form.actor];
    }

    this._threadGrips[form.actor] = new SourceFront(this.client, form);
    return this._threadGrips[form.actor];
  }
}

exports.ThreadFront = ThreadFront;
registerFront(ThreadFront);
