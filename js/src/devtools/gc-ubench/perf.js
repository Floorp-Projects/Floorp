/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Performance monitoring and calculation.

var features = {
  trackingSizes: "mozMemory" in performance,
  showingGCs: "mozMemory" in performance,
};

// Class for inter-frame timing, which handles being paused and resumed.
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

  start_recording(now = performance.now()) {
    this.start = this.prev = now;
  }

  on_frame_finished(now = performance.now()) {
    const delay = now - this.prev;
    this.prev = now;
    return delay;
  }

  pause(now = performance.now()) {
    this.stopped = now;
    // Abuse this.prev to store the time elapsed since the previous frame.
    // This will be used to adjust this.prev when we resume.
    this.prev = now - this.prev;
  }

  resume(now = performance.now()) {
    this.prev = now - this.prev;
    const stop_duration = now - this.stopped;
    this.start += stop_duration;
    this.stopped = 0;
  }
}

// Per-frame time sampling infra.
var sampleTime = 16.666667; // ms
var sampleIndex = 0;

// Class for maintaining a rolling window of per-frame GC-related counters:
// inter-frame delay, minor/major/slice GC counts, cumulative bytes, etc.
class FrameHistory {
  constructor(numSamples) {
    // Private
    this._frameTimer = new FrameTimer();
    this._numSamples = numSamples;

    // Public API
    this.delays = new Array(numSamples);
    this.gcBytes = new Array(numSamples);
    this.mallocBytes = new Array(numSamples);
    this.gcs = new Array(numSamples);
    this.minorGCs = new Array(numSamples);
    this.majorGCs = new Array(numSamples);
    this.slices = new Array(numSamples);

    sampleIndex = 0;
    this.reset();
  }

  start(now = performance.now()) {
    this._frameTimer.start_recording(now);
  }

  reset() {
    this.delays.fill(0);
    this.gcBytes.fill(0);
    this.mallocBytes.fill(0);
    this.gcs.fill(this.gcs[sampleIndex]);
    this.minorGCs.fill(this.minorGCs[sampleIndex]);
    this.majorGCs.fill(this.majorGCs[sampleIndex]);
    this.slices.fill(this.slices[sampleIndex]);

    sampleIndex = 0;
  }

  get numSamples() {
    return this._numSamples;
  }

  findMax(collection) {
    // Depends on having at least one non-negative entry, and unfilled
    // entries being <= max.
    var maxIndex = 0;
    for (let i = 0; i < this._numSamples; i++) {
      if (collection[i] >= collection[maxIndex]) {
        maxIndex = i;
      }
    }
    return maxIndex;
  }

  findMaxDelay() {
    return this.findMax(this.delays);
  }

  on_frame(now = performance.now()) {
    const delay = this._frameTimer.on_frame_finished(now);

    // Total time elapsed while the active test has been running.
    var t = now - this._frameTimer.start;
    var newIndex = Math.round(t / sampleTime);
    while (sampleIndex < newIndex) {
      sampleIndex++;
      var idx = sampleIndex % this._numSamples;
      this.delays[idx] = delay;
      if (features.trackingSizes) {
        this.gcBytes[idx] = performance.mozMemory.gcBytes;
      }
      if (features.showingGCs) {
        this.gcs[idx] = performance.mozMemory.gcNumber;
        this.minorGCs[idx] = performance.mozMemory.minorGCCount;
        this.majorGCs[idx] = performance.mozMemory.majorGCCount;

        // Previous versions lacking sliceCount will fall back to
        // assuming any GC activity was a major GC slice, even though
        // that incorrectly includes minor GCs. Although this file is
        // versioned with the code that implements the new sliceCount
        // field, it is common to load the gc-ubench index.html with
        // different browser versions.
        this.slices[idx] =
          performance.mozMemory.sliceCount || performance.mozMemory.gcNumber;
      }
    }

    return delay;
  }

  pause() {
    this._frameTimer.pause();
  }

  resume() {
    this._frameTimer.resume();
  }

  is_stopped() {
    return this._frameTimer.is_stopped();
  }
}
