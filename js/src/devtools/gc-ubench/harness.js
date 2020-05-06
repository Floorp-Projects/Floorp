/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class FrameTimer {
  constructor() {
    // Start time of the current active test, adjusted for any time spent
    // stopped (so `now - this.start` is how long the current active test
    // has run for.)
    this.start = undefined;

    // Timestamp of callback following the previous frame.
    this.prev = undefined;

    // Timestamp when drawing was paused, or zero if drawing is active.
    this.stopped = 0;
  }

  is_stopped() {
    return this.stopped != 0;
  }

  start_recording() {
    this.start = this.prev = performance.now();
  }

  record_frame_callback(timestamp) {
    const delay = timestamp - this.prev;
    this.prev = timestamp;
    return delay;
  }

  stop() {
    const now = performance.now();
    this.stopped = now;
  }

  resume() {
    const now = performance.now();
    this.prev += now - this.stopped;
    const stop_duration = now - this.stopped;
    this.start += stop_duration;
    this.stopped = 0;
  }
}

var gFrameTimer = new FrameTimer();

// Global defaults
var gDefaultGarbageTotal = "8M";
var gDefaultGarbagePerFrame = "8K";

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
    this._garbageTotal =
      info.garbageTotal ||
      parse_units(info.defaultGarbageTotal || gDefaultGarbageTotal);
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
  get garbageTotal() {
    return this._garbageTotal;
  }
  set garbageTotal(amount) {
    this._garbageTotal = amount;
  }

  start() {
    this.load.load(this._garbageTotal);
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

class AllocationLoadManager {
  constructor(tests) {
    this._loads = new Map();
    for (const [name, info] of tests.entries()) {
      this._loads.set(name, new AllocationLoad(info, name));
    }
    this._active = undefined;
    this._paused = false;
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

  change_garbageTotal(amount) {
    if (this._active) {
      this._active.garbageTotal = amount;
      this._active.reload();
    }
  }

  change_garbagePerFrame(amount) {
    if (this._active) {
      this._active.garbageTotal = amount;
    }
  }

  tick() {
    if (this._active && !this._paused) {
      this._active.tick();
    }
  }
}

// Current test state.
var gLoadMgr = undefined;

var testDuration = undefined; // ms
var testStart = undefined; // ms
var testQueue = [];

function format_gcBytes(bytes) {
  if (bytes < 4000) {
    return `${bytes} bytes`;
  } else if (bytes < 4e6) {
    return `${(bytes / 1024).toFixed(2)} KB`;
  } else if (bytes < 4e9) {
    return `${(bytes / 1024 / 1024).toFixed(2)} MB`;
  }
  return `${(bytes / 1024 / 1024 / 1024).toFixed(2)} GB`;
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
  score = score / (testDuration / 1000);
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

function change_load_internal(new_load_name) {
  gLoadMgr.setActiveLoadByName(new_load_name);
}
