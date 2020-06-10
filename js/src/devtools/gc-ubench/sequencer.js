/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A Sequencer handles transitioning between different mutators. Typically, it
// will base the decision to transition on things like elapsed time, number of
// GCs observed, or similar. However, they might also implement a search for
// some result value by running for some time while measuring, tweaking
// parameters, and re-running until an in-range result is found.

var Sequencer = class {
  // Return the current mutator (of class AllocationLoad).
  get current() {
    throw new Error("unimplemented");
  }

  start(now = gHost.now()) {
    this.started = now;
  }

  // Called by user to handle advancing time. Subclasses will normally
  // override do_tick() instead. Returns whether the mutator load changed.
  tick(now = gHost.now()) {
    if (this.done()) {
      throw new Error("tick() called on completed sequencer");
    }

    const advanced = this.do_tick(now);
    if (advanced) {
      this.started = now;
    }
    return advanced;
  }

  // Implement in subclass to handle time advancing. Must return whether the
  // mutator load changed. Called by tick(), above.
  do_tick(now = gHost.now()) {
    throw new Error("unimplemented");
  }

  // Returns whether this sequencer has completed all tests.
  done() {
    throw new Error("unimplemented");
  }

  // Returns how long the current load has been running.
  currentLoadElapsed(now = gHost.now()) {
    return now - this.started;
  }
};

var SingleMutatorSequencer = class extends Sequencer {
  constructor(mutator, duration_sec) {
    super();
    this.mutator = mutator;
    if (!(duration_sec > 0)) {
      throw new Error(`invalid duration '${duration_sec}'`);
    }
    this.duration = duration_sec * 1000;
    this.state = 'init'; // init -> running -> done
  }

  get current() {
    return this.state === 'done' ? undefined : this.mutator;
  }

  reset() {
    this.state = 'init';
  }

  start(now = gHost.now()) {
    if (this.state !== 'init') {
      throw new Error("cannot restart a single-mutator sequencer");
    }
    super.start(now);
    this.state = 'running';
  }

  do_tick(now) {
    if (this.currentLoadElapsed(now) < this.duration) {
      return false;
    }

    this.state = 'done';
    return true;
  }

  done() {
    return this.state === 'done';
  }
};

var ChainSequencer = class extends Sequencer {
  constructor(sequencers) {
    super();
    this.sequencers = sequencers;
    this.idx = -1;
    this.state = sequencers.length ? 'init' : 'done'; // init -> running -> done
  }

  get current() {
    return this.idx >= 0 ? this.sequencers[this.idx].current : undefined;
  }

  reset() {
    this.state = 'init';
    this.idx = -1;
  }

  start(now = gHost.now()) {
    super.start(now);
    if (this.sequencers.length === 0) {
      this.state = 'done';
      return;
    }

    this.idx = 0;
    this.sequencers[0].start(now);
    this.state = 'running';
  }

  do_tick(now) {
    const sequencer = this.sequencers[this.idx];
    if (!sequencer.do_tick(now)) {
      return false;
    }

    this.idx++;
    if (this.idx < this.sequencers.length) {
      this.sequencers[this.idx].start();
    } else {
      this.idx = -1;
      this.state = 'done';
    }

    return true;
  }

  done() {
    return this.state === 'done';
  }
};
