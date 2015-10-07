/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu, Cr } = require("chrome");
const { Task } = require("resource://gre/modules/Task.jsm");

loader.lazyRequireGetter(this, "PerformanceIO",
  "devtools/client/performance/modules/io");
loader.lazyRequireGetter(this, "RecordingUtils",
  "devtools/shared/performance/recording-utils");
loader.lazyRequireGetter(this, "PerformanceRecordingCommon",
  "devtools/shared/performance/recording-common", true);
loader.lazyRequireGetter(this, "merge", "sdk/util/object", true);

/**
 * Model for a wholistic profile, containing the duration, profiling data,
 * frames data, timeline (marker, tick, memory) data, and methods to mark
 * a recording as 'in progress' or 'finished'.
 */
const LegacyPerformanceRecording = function (options={}) {
  this._label = options.label || "";
  this._console = options.console || false;

  this._configuration = {
    withMarkers: options.withMarkers || false,
    withTicks: options.withTicks || false,
    withMemory: options.withMemory || false,
    withAllocations: options.withAllocations || false,
    withJITOptimizations: options.withJITOptimizations || false,
    allocationsSampleProbability: options.allocationsSampleProbability || 0,
    allocationsMaxLogLength: options.allocationsMaxLogLength || 0,
    bufferSize: options.bufferSize || 0,
    sampleFrequency: options.sampleFrequency || 1
  };
};

LegacyPerformanceRecording.prototype = merge({
  _profilerStartTime: 0,
  _timelineStartTime: 0,
  _memoryStartTime: 0,

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
   * Sets up the instance with data from the PerformanceFront when
   * starting a recording. Should only be called by PerformanceFront.
   */
  _populate: function (info) {
    // Times must come from the actor in order to be self-consistent.
    // However, we also want to update the view with the elapsed time
    // even when the actor is not generating data. To do this we get
    // the local time and use it to compute a reasonable elapsed time.
    this._localStartTime = Date.now();

    this._profilerStartTime = info.profilerStartTime;
    this._timelineStartTime = info.timelineStartTime;
    this._memoryStartTime = info.memoryStartTime;
    this._startingBufferStatus = {
      position: info.position,
      totalSize: info.totalSize,
      generation: info.generation
    };

    this._recording = true;

    this._systemHost = {};
    this._systemClient = {};
    this._markers = [];
    this._frames = [];
    this._memory = [];
    this._ticks = [];
    this._allocations = { sites: [], timestamps: [], frames: [], sizes: [] };
  },

  /**
   * Called when the signal was sent to the front to no longer record more
   * data, and begin fetching the data. There's some delay during fetching,
   * even though the recording is stopped, the model is not yet completed until
   * all the data is fetched.
   */
  _onStoppingRecording: function (endTime) {
    this._duration = endTime - this._localStartTime;
    this._recording = false;
  },

  /**
   * Sets results available from stopping a recording from PerformanceFront.
   * Should only be called by PerformanceFront.
   */
  _onStopRecording: Task.async(function *({ profilerEndTime, profile, systemClient, systemHost }) {
    // Update the duration with the accurate profilerEndTime, so we don't have
    // samples outside of the approximate duration set in `_onStoppingRecording`.
    this._duration = profilerEndTime - this._profilerStartTime;
    this._profile = profile;
    this._completed = true;

    // We filter out all samples that fall out of current profile's range
    // since the profiler is continuously running. Because of this, sample
    // times are not guaranteed to have a zero epoch, so offset the
    // timestamps.
    RecordingUtils.offsetSampleTimes(this._profile, this._profilerStartTime);

    // Markers need to be sorted ascending by time, to be properly displayed
    // in a waterfall view.
    this._markers = this._markers.sort((a, b) => (a.start > b.start));

    this._systemHost = systemHost;
    this._systemClient = systemClient;
  }),

  /**
   * Gets the profile's start time.
   * @return number
   */
  _getProfilerStartTime: function () {
    return this._profilerStartTime;
  },

  /**
   * Fired whenever the PerformanceFront emits markers, memory or ticks.
   */
  _addTimelineData: function (eventName, ...data) {
    // If this model isn't currently recording,
    // ignore the timeline data.
    if (!this.isRecording()) {
      return;
    }

    let config = this.getConfiguration();

    switch (eventName) {
      // Accumulate timeline markers into an array. Furthermore, the timestamps
      // do not have a zero epoch, so offset all of them by the start time.
      case "markers": {
        if (!config.withMarkers) { break; }
        let [markers] = data;
        RecordingUtils.offsetMarkerTimes(markers, this._timelineStartTime);
        RecordingUtils.pushAll(this._markers, markers);
        break;
      }
      // Accumulate stack frames into an array.
      case "frames": {
        if (!config.withMarkers) { break; }
        let [, frames] = data;
        RecordingUtils.pushAll(this._frames, frames);
        break;
      }
      // Save the accumulated refresh driver ticks.
      case "ticks": {
        if (!config.withTicks) { break; }
        let [, timestamps] = data;
        this._ticks = timestamps;
        break;
      }
    }
  },

  toString: () => "[object LegacyPerformanceRecording]"
}, PerformanceRecordingCommon);

exports.LegacyPerformanceRecording = LegacyPerformanceRecording;
