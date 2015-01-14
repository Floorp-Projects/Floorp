/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { PerformanceIO } = require("devtools/performance/io");

const RECORDING_IN_PROGRESS = exports.RECORDING_IN_PROGRESS = -1;
const RECORDING_UNAVAILABLE = exports.RECORDING_UNAVAILABLE = null;
/**
 * Model for a wholistic profile, containing start/stop times, profiling data, frames data,
 * timeline (marker, tick, memory) data, and methods to start/stop recording.
 */

const RecordingModel = function (options={}) {
  this._front = options.front;
  this._performance = options.performance;
  this._label = options.label || "";
};

RecordingModel.prototype = {
  _localStartTime: RECORDING_UNAVAILABLE,
  _startTime: RECORDING_UNAVAILABLE,
  _endTime: RECORDING_UNAVAILABLE,
  _markers: [],
  _frames: [],
  _ticks: [],
  _memory: [],
  _profilerData: {},
  _label: "",
  _imported: false,
  _isRecording: false,

  /**
   * Loads a recording from a file.
   *
   * @param nsILocalFile file
   *        The file to import the data form.
   */
  importRecording: Task.async(function *(file) {
    let recordingData = yield PerformanceIO.loadRecordingFromFile(file);

    this._imported = true;
    this._label = recordingData.profilerData.profilerLabel || "";
    this._startTime = recordingData.interval.startTime;
    this._endTime = recordingData.interval.endTime;
    this._markers = recordingData.markers;
    this._frames = recordingData.frames;
    this._memory = recordingData.memory;
    this._ticks = recordingData.ticks;
    this._profilerData = recordingData.profilerData;

    return recordingData;
  }),

  /**
   * Saves the current recording to a file.
   *
   * @param nsILocalFile file
   *        The file to stream the data into.
   */
  exportRecording: Task.async(function *(file) {
    let recordingData = this.getAllData();
    yield PerformanceIO.saveRecordingToFile(recordingData, file);
  }),

  /**
   * Starts recording with the PerformanceFront, storing the start times
   * on the model.
   */
  startRecording: Task.async(function *() {
    // Times must come from the actor in order to be self-consistent.
    // However, we also want to update the view with the elapsed time
    // even when the actor is not generating data. To do this we get
    // the local time and use it to compute a reasonable elapsed time.
    this._localStartTime = this._performance.now();

    let { startTime } = yield this._front.startRecording({
      withTicks: true,
      withMemory: true
    });
    this._isRecording = true;

    this._startTime = startTime;
    this._endTime = RECORDING_IN_PROGRESS;
    this._markers = [];
    this._frames = [];
    this._memory = [];
    this._ticks = [];
  }),

  /**
   * Stops recording with the PerformanceFront, storing the end times
   * on the model.
   */
  stopRecording: Task.async(function *() {
    let results = yield this._front.stopRecording();
    this._isRecording = false;

    // If `endTime` is not yielded from timeline actor (< Fx36), fake it.
    if (!results.endTime) {
      results.endTime = this._startTime + this.getLocalElapsedTime();
    }

    this._endTime = results.endTime;
    this._profilerData = results.profilerData;
    this._markers = this._markers.sort((a,b) => (a.start > b.start));

    return results;
  }),

  /**
   * Returns the profile's label, from `console.profile(LABEL)`.
   */
  getLabel: function () {
    return this._label;
  },

  /**
   * Gets the amount of time elapsed locally after starting a recording.
   */
  getLocalElapsedTime: function () {
    return this._performance.now() - this._localStartTime;
  },

  /**
   * Returns duration of this recording, in milliseconds.
   */
  getDuration: function () {
    let { startTime, endTime } = this.getInterval();
    return endTime - startTime;
  },

  /**
   * Gets the time interval for the current recording.
   * @return object
   */
  getInterval: function() {
    let startTime = this._startTime;
    let endTime = this._endTime;

    // Compute an approximate ending time for the current recording. This is
    // needed to ensure that the view updates even when new data is
    // not being generated.
    if (endTime == RECORDING_IN_PROGRESS) {
      endTime = startTime + this.getLocalElapsedTime();
    }

    return { startTime, endTime };
  },

  /**
   * Gets the accumulated markers in the current recording.
   * @return array
   */
  getMarkers: function() {
    return this._markers;
  },

  /**
   * Gets the accumulated stack frames in the current recording.
   * @return array
   */
  getFrames: function() {
    return this._frames;
  },

  /**
   * Gets the accumulated memory measurements in this recording.
   * @return array
   */
  getMemory: function() {
    return this._memory;
  },

  /**
   * Gets the accumulated refresh driver ticks in this recording.
   * @return array
   */
  getTicks: function() {
    return this._ticks;
  },

  /**
   * Gets the profiler data in this recording.
   * @return array
   */
  getProfilerData: function() {
    return this._profilerData;
  },

  /**
   * Gets all the data in this recording.
   */
  getAllData: function() {
    let interval = this.getInterval();
    let markers = this.getMarkers();
    let frames = this.getFrames();
    let memory = this.getMemory();
    let ticks = this.getTicks();
    let profilerData = this.getProfilerData();
    return { interval, markers, frames, memory, ticks, profilerData };
  },

  /**
   * Returns a boolean indicating whether or not this recording model
   * is recording.
   */
  isRecording: function () {
    return this._isRecording;
  },

  /**
   * Fired whenever the PerformanceFront emits markers, memory or ticks.
   */
  addTimelineData: function (eventName, ...data) {
    // If this model isn't currently recording,
    // ignore the timeline data.
    if (!this.isRecording()) {
      return;
    }

    switch (eventName) {
      // Accumulate markers into an array.
      case "markers":
        let [markers] = data;
        Array.prototype.push.apply(this._markers, markers);
        break;
      // Accumulate stack frames into an array.
      case "frames":
        let [, frames] = data;
        Array.prototype.push.apply(this._frames, frames);
        break;
      // Accumulate memory measurements into an array.
      case "memory":
        let [delta, measurement] = data;
        this._memory.push({ delta, value: measurement.total / 1024 / 1024 });
        break;
      // Save the accumulated refresh driver ticks.
      case "ticks":
        let [, timestamps] = data;
        this._ticks = timestamps;
        break;
    }
  }
};

exports.RecordingModel = RecordingModel;
