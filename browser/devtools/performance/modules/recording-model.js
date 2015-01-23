/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu, Cr } = require("chrome");

loader.lazyRequireGetter(this, "PerformanceIO",
  "devtools/performance/io", true);
loader.lazyRequireGetter(this, "RecordingUtils",
  "devtools/performance/recording-utils", true);

/**
 * Model for a wholistic profile, containing the duration, profiling data,
 * frames data, timeline (marker, tick, memory) data, and methods to mark
 * a recording as 'in progress' or 'finished'.
 */

const RecordingModel = function (options={}) {
  this._front = options.front;
  this._performance = options.performance;
  this._label = options.label || "";
};

RecordingModel.prototype = {
  // Private fields, only needed when a recording is started or stopped.
  _imported: false,
  _recording: false,
  _profilerStartTime: 0,
  _timelineStartTime: 0,

  // Serializable fields, necessary and sufficient for import and export.
  _label: "",
  _duration: 0,
  _markers: null,
  _frames: null,
  _ticks: null,
  _memory: null,
  _profile: null,

  /**
   * Loads a recording from a file.
   *
   * @param nsILocalFile file
   *        The file to import the data form.
   */
  importRecording: Task.async(function *(file) {
    let recordingData = yield PerformanceIO.loadRecordingFromFile(file);

    this._imported = true;
    this._label = recordingData.label || "";
    this._duration = recordingData.duration;
    this._markers = recordingData.markers;
    this._frames = recordingData.frames;
    this._memory = recordingData.memory;
    this._ticks = recordingData.ticks;
    this._profile = recordingData.profile;
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
   * Starts recording with the PerformanceFront.
   *
   * @param object options
   *        An options object to pass to the timeline front. Supported
   *        properties are `withTicks` and `withMemory`.
   */
  startRecording: Task.async(function *(options = {}) {
    // Times must come from the actor in order to be self-consistent.
    // However, we also want to update the view with the elapsed time
    // even when the actor is not generating data. To do this we get
    // the local time and use it to compute a reasonable elapsed time.
    this._localStartTime = this._performance.now();

    let info = yield this._front.startRecording(options);
    this._profilerStartTime = info.profilerStartTime;
    this._timelineStartTime = info.timelineStartTime;
    this._recording = true;

    this._markers = [];
    this._frames = [];
    this._memory = [];
    this._ticks = [];
  }),

  /**
   * Stops recording with the PerformanceFront.
   */
  stopRecording: Task.async(function *() {
    let info = yield this._front.stopRecording();
    this._profile = info.profile;
    this._duration = info.profilerEndTime - this._profilerStartTime;
    this._recording = false;

    // We'll need to filter out all samples that fall out of current profile's
    // range since the profiler is continuously running. Because of this, sample
    // times are not guaranteed to have a zero epoch, so offset the timestamps.
    RecordingUtils.filterSamples(this._profile, this._profilerStartTime);
    RecordingUtils.offsetSampleTimes(this._profile, this._profilerStartTime);

    // Markers need to be sorted ascending by time, to be properly displayed
    // in a waterfall view.
    this._markers = this._markers.sort((a, b) => (a.start > b.start));
  }),

  /**
   * Gets the profile's label, from `console.profile(LABEL)`.
   * @return string
   */
  getLabel: function () {
    return this._label;
  },

  /**
   * Gets duration of this recording, in milliseconds.
   * @return number
   */
  getDuration: function () {
    // Compute an approximate ending time for the current recording if it is
    // still in progress. This is needed to ensure that the view updates even
    // when new data is not being generated.
    if (this._recording) {
      return this._performance.now() - this._localStartTime;
    } else {
      return this._duration;
    }
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
  getProfile: function() {
    return this._profile;
  },

  /**
   * Gets all the data in this recording.
   */
  getAllData: function() {
    let label = this.getLabel();
    let duration = this.getDuration();
    let markers = this.getMarkers();
    let frames = this.getFrames();
    let memory = this.getMemory();
    let ticks = this.getTicks();
    let profile = this.getProfile();
    return { label, duration, markers, frames, memory, ticks, profile };
  },

  /**
   * Returns a boolean indicating whether or not this recording model
   * is recording.
   */
  isRecording: function () {
    return this._recording;
  },

  /**
   * Fired whenever the PerformanceFront emits markers, memory or ticks.
   */
  addTimelineData: function (eventName, ...data) {
    // If this model isn't currently recording,
    // ignore the timeline data.
    if (!this._recording) {
      return;
    }

    switch (eventName) {
      // Accumulate markers into an array. Furthermore, timestamps do not
      // have a zero epoch, so offset all of them by the timeline's start time.
      case "markers":
        let [markers] = data;
        RecordingUtils.offsetMarkerTimes(markers, this._timelineStartTime);
        Array.prototype.push.apply(this._markers, markers);
        break;

      // Accumulate stack frames into an array.
      case "frames":
        let [, frames] = data;
        Array.prototype.push.apply(this._frames, frames);
        break;

      // Accumulate memory measurements into an array. Furthermore, the
      // timestamp does not have a zero epoch, so offset it.
      case "memory":
        let [currentTime, measurement] = data;
        this._memory.push({
          delta: currentTime - this._timelineStartTime,
          value: measurement.total / 1024 / 1024
        });
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
