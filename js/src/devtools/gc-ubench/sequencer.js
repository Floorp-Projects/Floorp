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

// Simple sequencer that takes a list of mutators to run and a duration for how
// long to run each one.
var LoadCycle = class extends Sequencer {
  constructor(tests_to_run, duration_sec) {
    super();
    this.queue = Array.from(tests_to_run);
    this.duration = duration_sec * 1000;
    this.idx = -1;
  }

  get current() {
    return this.queue[this.idx];
  }

  start(now = gHost.now()) {
    super.start(now);
    this.idx = 0;
  }

  do_tick(now) {
    if (this.currentLoadElapsed(now) < this.duration) {
      return false;
    }

    this.idx++;
    if (this.idx >= this.queue.length) {
      this.idx = -1;
    }

    return true;
  }

  done() {
    return this.idx == -1;
  }
};
