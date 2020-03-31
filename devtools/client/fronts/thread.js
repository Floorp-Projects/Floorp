/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ThreadStateTypes } = require("devtools/client/constants");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");

const { threadSpec } = require("devtools/shared/specs/thread");

loader.lazyRequireGetter(
  this,
  "ObjectFront",
  "devtools/client/fronts/object",
  true
);
loader.lazyRequireGetter(this, "FrameFront", "devtools/client/fronts/frame");
loader.lazyRequireGetter(
  this,
  "SourceFront",
  "devtools/client/fronts/source",
  true
);

/**
 * Creates a thread front for the remote debugging protocol server. This client
 * is a front to the thread actor created in the server side, hiding the
 * protocol details in a traditional JavaScript API.
 *
 * @param client DevToolsClient
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
    this.targetFront.on("will-navigate", this._onWillNavigate.bind(this));
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

  getWebconsoleFront() {
    return this.targetFront.getFront("console");
  }

  _assertPaused(command) {
    if (!this.paused) {
      throw Error(
        command + " command sent while not paused. Currently " + this._state
      );
    }
  }

  getFrames(start, count) {
    return super.frames(start, count);
  }

  /**
   * Resume a paused thread. If the optional limit parameter is present, then
   * the thread will also pause when that limit is reached.
   *
   * @param [optional] object limit
   *        An object with a type property set to the appropriate limit (next,
   *        step, or finish) per the remote debugging protocol specification.
   *        Use null to specify no limit.
   */
  async _doResume(resumeLimit) {
    this._assertPaused("resume");

    // Put the client in a tentative "resuming" state so we can prevent
    // further requests that should only be sent in the paused state.
    this._previousState = this._state;
    this._state = "resuming";
    try {
      await super.resume(resumeLimit);
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
    return this._doResume(null);
  }

  /**
   * Resume then pause without stepping.
   *
   */
  resumeThenPause() {
    return this._doResume({ type: "break" });
  }

  /**
   * Step over a function call.
   */
  stepOver() {
    return this._doResume({ type: "next" });
  }

  /**
   * Step into a function call.
   */
  stepIn() {
    return this._doResume({ type: "step" });
  }

  /**
   * Step out of a function call.
   */
  stepOut() {
    return this._doResume({ type: "finish" });
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
   * attach to the thread actor.
   */
  async attach(options) {
    const onPaused = this.once("paused");
    await super.attach(options);
    await onPaused;
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
   * Return a ObjectFront object for the given object grip.
   *
   * @param grip object
   *        A pause-lifetime object grip returned by the protocol.
   */
  pauseGrip(grip) {
    if (grip.actor in this._pauseGrips) {
      return this._pauseGrips[grip.actor];
    }

    const objectFront = new ObjectFront(
      this.conn,
      this.targetFront,
      this,
      grip
    );
    this._pauseGrips[grip.actor] = objectFront;
    return objectFront;
  }

  /**
   * Clear and invalidate all the grip fronts from the given cache.
   *
   * @param gripCacheName
   *        The property name of the grip cache we want to clear.
   */
  _clearObjectFronts(gripCacheName) {
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
    this._clearObjectFronts("_pauseGrips");
  }

  /**
   * Invalidate thread-lifetime grip clients and clear the list of current grip
   * clients.
   */
  _clearThreadGrips() {
    this._clearObjectFronts("_threadGrips");
  }

  _beforePaused(packet) {
    this._state = "paused";
    this._onThreadState(packet);
  }

  _beforeResumed() {
    this._state = "attached";
    this._onThreadState(null);
    this.unmanageChildren(FrameFront);
  }

  _beforeDetached(packet) {
    this._state = "detached";
    this._onThreadState(packet);
    this._clearThreadGrips();
    this.unmanageChildren(FrameFront);
  }

  _onWillNavigate() {
    this.unmanageChildren(SourceFront);
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

    const sourceFront = new SourceFront(this.client, form);
    this.manage(sourceFront);
    this._threadGrips[form.actor] = sourceFront;
    return sourceFront;
  }
}

exports.ThreadFront = ThreadFront;
registerFront(ThreadFront);
