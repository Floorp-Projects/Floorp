/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Global defaults

// Allocate this much "garbage" per frame. This might correspond exactly to a
// number of objects/values, or it might be some set of objects, depending on
// the mutator in question.
var gDefaultGarbagePerFrame = "8K";

// In order to avoid a performance cliff between when the per-frame garbage
// fits in the nursery and when it doesn't, most mutators will collect multiple
// "piles" of garbage and round-robin through them, so that the per-frame
// garbage stays alive for some number of frames. There will still be some
// internal temporary allocations that don't end up in the piles; presumably,
// the nursery will take care of those.
//
// If the per-frame garbage is K and the number of piles is P, then some of the
// garbage will start getting tenured as long as P*K > size(nursery).
var gDefaultGarbagePiles = "8";

var gDefaultTestDuration = 8.0;

function parse_units(v) {
  if (!v.length) {
    return NaN;
  }
  var lastChar = v[v.length - 1].toLowerCase();
  if (!isNaN(parseFloat(lastChar))) {
    return parseFloat(v);
  }
  var units = parseFloat(v.substr(0, v.length - 1));
  if (lastChar == "k") {
    return units * 1e3;
  }
  if (lastChar == "m") {
    return units * 1e6;
  }
  if (lastChar == "g") {
    return units * 1e9;
  }
  return NaN;
}

class AllocationLoad {
  constructor(info, name) {
    this.load = info;
    this.load.name = this.load.name ?? name;

    this._garbagePerFrame =
      info.garbagePerFrame ||
      parse_units(info.defaultGarbagePerFrame || gDefaultGarbagePerFrame);
    this._garbagePiles =
      info.garbagePiles ||
      parse_units(info.defaultGarbagePiles || gDefaultGarbagePiles);
  }

  get name() {
    return this.load.name;
  }
  get description() {
    return this.load.description;
  }
  get garbagePerFrame() {
    return this._garbagePerFrame;
  }
  set garbagePerFrame(amount) {
    this._garbagePerFrame = amount;
  }
  get garbagePiles() {
    return this._garbagePiles;
  }
  set garbagePiles(amount) {
    this._garbagePiles = amount;
  }

  start() {
    this.load.load(this._garbagePiles);
  }

  stop() {
    this.load.unload();
  }

  reload() {
    this.stop();
    this.start();
  }

  tick() {
    this.load.makeGarbage(this._garbagePerFrame);
  }

  is_dummy_load() {
    return this.load.name == "noAllocation";
  }
}

class LoadCycle {
  constructor(tests_to_run, duration) {
    this.queue = [...tests_to_run];
    this.duration = duration;
    this.idx = -1;
  }

  get current() {
    return this.queue[this.idx];
  }

  start(now = performance.now()) {
    this.idx = 0;
    this.cycleStart = this.started = now;
  }

  tick(now = performance.now()) {
    if (this.currentLoadElapsed(now) < this.duration) {
      return;
    }

    this.idx++;
    this.started = now;
    if (this.idx >= this.queue.length) {
      this.idx = -1;
    }
  }

  done() {
    return this.idx == -1;
  }

  cycleElapsed(now = performance.now()) {
    return now - this.cycleStart;
  }

  currentLoadElapsed(now = performance.now()) {
    return now - this.started;
  }
}

class AllocationLoadManager {
  constructor(tests) {
    this._loads = new Map();
    for (const [name, info] of tests.entries()) {
      this._loads.set(name, new AllocationLoad(info, name));
    }
    this._active = undefined;
    this._paused = false;
    this._eventsSinceLastTick = 0;

    // Public API
    this.cycle = null;
    this.testDurationMS = gDefaultTestDuration * 1000;

    // Constants
    this.CYCLE_STARTED = 1;
    this.CYCLE_STOPPED = 2;
    this.LOAD_ENDED = 4;
    this.LOAD_STARTED = 8;
  }

  activeLoad() {
    return this._active;
  }

  setActiveLoadByName(name) {
    if (this._active) {
      this._active.stop();
    }
    this._active = this._loads.get(name);
    this._active.start();
  }

  deactivateLoad() {
    this._active.stop();
    this._active = undefined;
  }

  get paused() {
    return this._paused;
  }
  set paused(pause) {
    this._paused = pause;
  }

  load_running() {
    return this._active && this._active.name != "noAllocation";
  }

  change_garbagePiles(amount) {
    if (this._active) {
      this._active.garbagePiles = amount;
      this._active.reload();
    }
  }

  change_garbagePerFrame(amount) {
    if (this._active) {
      this._active.garbagePerFrame = amount;
    }
  }

  tick(now = performance.now()) {
    this.lastActive = this._active;
    let events = this._eventsSinceLastTick;
    this._eventsSinceLastTick = 0;

    if (this.cycle) {
      const prev = this.cycle.current;
      this.cycle.tick(now);
      if (this.cycle.current != prev) {
        if (this.cycle.current) {
          this.setActiveLoadByName(this.cycle.current);
        } else {
          this.deactivateLoad();
        }
        events |= this.LOAD_ENDED;
        if (this.cycle.done()) {
          events |= this.CYCLE_STOPPED;
          this.cycle = null;
        } else {
          events |= this.LOAD_STARTED;
        }
      }
    }

    if (this._active && !this._paused) {
      this._active.tick();
    }

    return events;
  }

  startCycle(tests_to_run, now = performance.now()) {
    this.cycle = new LoadCycle(tests_to_run, this.testDurationMS);
    this.cycle.start(now);
    this.setActiveLoadByName(this.cycle.current);
    this._eventsSinceLastTick |= this.CYCLE_STARTED | this.LOAD_STARTED;
  }

  cycleStopped() {
    return !this.cycle || this.cycle.done();
  }

  cycleCurrentLoadRemaining(now = performance.now()) {
    return this.cycleStopped()
      ? 0
      : this.testDurationMS - this.cycle.currentLoadElapsed(now);
  }
}

// Current test state.
var gLoadMgr = undefined;

function format_with_units(n, label, shortlabel, kbase) {
  if (n < kbase * 4) {
    return `${n} ${label}`;
  } else if (n < kbase ** 2 * 4) {
    return `${(n / kbase).toFixed(2)}K${shortlabel}`;
  } else if (n < kbase ** 3 * 4) {
    return `${(n / kbase ** 2).toFixed(2)}M${shortlabel}`;
  }
  return `${(n / kbase ** 3).toFixed(2)}G${shortlabel}`;
}

function format_bytes(bytes) {
  return format_with_units(bytes, "bytes", "B", 1024);
}

function format_num(n) {
  return format_with_units(n, "", "", 1000);
}

function update_histogram(histogram, delay) {
  // Round to a whole number of 10us intervals to provide enough resolution to
  // capture a 16ms target with adequate accuracy.
  delay = Math.round(delay * 100) / 100;
  var current = histogram.has(delay) ? histogram.get(delay) : 0;
  histogram.set(delay, ++current);
}

// Compute a score based on the total ms we missed frames by per second.
function compute_test_score(histogram) {
  var score = 0;
  for (let [delay, count] of histogram) {
    score += Math.abs((delay - 1000 / 60) * count);
  }
  score = score / (gLoadMgr.testDurationMS / 1000);
  return Math.round(score * 1000) / 1000;
}

// Build a spark-lines histogram for the test results to show with the aggregate score.
function compute_spark_histogram_percents(histogram) {
  var ranges = [
    [-99999999, 16.6],
    [16.6, 16.8],
    [16.8, 25],
    [25, 33.4],
    [33.4, 60],
    [60, 100],
    [100, 300],
    [300, 99999999],
  ];
  var rescaled = new Map();
  for (let [delay] of histogram) {
    for (var i = 0; i < ranges.length; ++i) {
      var low = ranges[i][0];
      var high = ranges[i][1];
      if (low <= delay && delay < high) {
        update_histogram(rescaled, i);
        break;
      }
    }
  }
  var total = 0;
  for (const [, count] of rescaled) {
    total += count;
  }

  var spark = [];
  for (let i = 0; i < ranges.length; ++i) {
    const amt = rescaled.has(i) ? rescaled.get(i) : 0;
    spark.push(amt / total);
  }

  return spark;
}
