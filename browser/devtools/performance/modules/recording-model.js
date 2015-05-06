/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu, Cr } = require("chrome");
const { Task } = require("resource://gre/modules/Task.jsm");

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
  this._label = options.label || "";
  this._console = options.console || false;

  this._configuration = {
    withMarkers: options.withMarkers || false,
    withTicks: options.withTicks || false,
    withMemory: options.withMemory || false,
    withAllocations: options.withAllocations || false,
    allocationsSampleProbability: options.allocationsSampleProbability || 0,
    allocationsMaxLogLength: options.allocationsMaxLogLength || 0,
    bufferSize: options.bufferSize || 0,
    sampleFrequency: options.sampleFrequency || 1
  };
};

RecordingModel.prototype = {
  // Private fields, only needed when a recording is started or stopped.
  _console: false,
  _imported: false,
  _recording: false,
  _completed: false,
  _profilerStartTime: 0,
  _timelineStartTime: 0,
  _memoryStartTime: 0,
  _configuration: {},
  _originalBufferStatus: null,
  _bufferPercent: null,

  // Serializable fields, necessary and sufficient for import and export.
  _label: "",
  _duration: 0,
  _markers: null,
  _frames: null,
  _memory: null,
  _ticks: null,
  _allocations: null,
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
    this._allocations = recordingData.allocations;
    this._profile = recordingData.profile;
    this._configuration = recordingData.configuration || {};
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
   * Sets up the instance with data from the SharedPerformanceConnection when
   * starting a recording. Should only be called by SharedPerformanceConnection.
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
    this._originalBufferStatus = {
      position: info.position,
      totalSize: info.totalSize,
      generation: info.generation
    };

    this._recording = true;

    this._markers = [];
    this._frames = [];
    this._memory = [];
    this._ticks = [];
    this._allocations = { sites: [], timestamps: [], frames: [], counts: [] };
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
   * Sets results available from stopping a recording from SharedPerformanceConnection.
   * Should only be called by SharedPerformanceConnection.
   */
  _onStopRecording: Task.async(function *(info) {
    this._profile = info.profile;
    this._completed = true;

    // We filter out all samples that fall out of current profile's range
    // since the profiler is continuously running. Because of this, sample
    // times are not guaranteed to have a zero epoch, so offset the
    // timestamps.
    // TODO move this into FakeProfilerFront in ./actors.js after bug 1154115
    RecordingUtils.offsetSampleTimes(this._profile, this._profilerStartTime);

    // Markers need to be sorted ascending by time, to be properly displayed
    // in a waterfall view.
    this._markers = this._markers.sort((a, b) => (a.start > b.start));
  }),

  /**
   * Gets the profile's start time.
   * @return number
   */
  getProfilerStartTime: function () {
    return this._profilerStartTime;
  },

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
      return Date.now() - this._localStartTime;
    } else {
      return this._duration;
    }
  },

  /**
   * Returns configuration object of specifying whether the recording
   * was started withTicks, withMemory and withAllocations.
   * @return object
   */
  getConfiguration: function () {
    return this._configuration;
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
   * Gets the memory allocations data in this recording.
   * @return array
   */
  getAllocations: function() {
    return this._allocations;
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
    let allocations = this.getAllocations();
    let profile = this.getProfile();
    let configuration = this.getConfiguration();
    return { label, duration, markers, frames, memory, ticks, allocations, profile, configuration };
  },

  /**
   * Returns a boolean indicating whether or not this recording model
   * was imported via file.
   */
  isImported: function () {
    return this._imported;
  },

  /**
   * Returns a boolean indicating whether or not this recording model
   * was started via a `console.profile` call.
   */
  isConsole: function () {
    return this._console;
  },

  /**
   * Returns a boolean indicating whether or not this recording model
   * has finished recording.
   * There is some delay in fetching data between when the recording stops, and
   * when the recording is considered completed once it has all the profiler and timeline data.
   */
  isCompleted: function () {
    return this._completed || this.isImported();
  },

  /**
   * Returns a boolean indicating whether or not this recording model
   * is recording.
   * A model may no longer be recording, yet still not have the profiler data. In that
   * case, use `isCompleted()`.
   */
  isRecording: function () {
    return this._recording;
  },

  /**
   * Returns the percent (value between 0 and 1) of buffer used in this
   * recording. Returns `null` for recordings that are no longer recording.
   */
  getBufferUsage: function () {
    return this.isRecording() ? this._bufferPercent : null;
  },

  /**
   * Fired whenever the PerformanceFront has new buffer data.
   */
  _addBufferStatusData: function (bufferStatus) {
    // If this model isn't currently recording, or if the server does not
    // support buffer status (or if this fires after actors are being destroyed),
    // ignore this information.
    if (!bufferStatus || !this.isRecording()) {
      return;
    }
    let { position: currentPosition, totalSize, generation: currentGeneration } = bufferStatus;
    let { position: origPosition, generation: origGeneration } = this._originalBufferStatus;

    let normalizedCurrent = (totalSize * (currentGeneration - origGeneration)) + currentPosition;
    let percent = (normalizedCurrent - origPosition) / totalSize;
    this._bufferPercent = percent > 1 ? 1 : percent;
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
        Array.prototype.push.apply(this._markers, markers);
        break;
      }
      // Accumulate stack frames into an array.
      case "frames": {
        if (!config.withMarkers) { break; }
        let [, frames] = data;
        Array.prototype.push.apply(this._frames, frames);
        break;
      }
      // Accumulate memory measurements into an array. Furthermore, the timestamp
      // does not have a zero epoch, so offset it by the actor's start time.
      case "memory": {
        if (!config.withMemory) { break; }
        let [currentTime, measurement] = data;
        this._memory.push({
          delta: currentTime - this._timelineStartTime,
          value: measurement.total / 1024 / 1024
        });
        break;
      }
      // Save the accumulated refresh driver ticks.
      case "ticks": {
        if (!config.withTicks) { break; }
        let [, timestamps] = data;
        this._ticks = timestamps;
        break;
      }
      // Accumulate allocation sites into an array. Furthermore, the timestamps
      // do not have a zero epoch, and are microseconds instead of milliseconds,
      // so offset all of them by the start time, also converting from µs to ms.
      case "allocations": {
        if (!config.withAllocations) { break; }
        let [{ sites, timestamps, frames, counts }] = data;
        let timeOffset = this._memoryStartTime * 1000;
        let timeScale = 1000;
        RecordingUtils.offsetAndScaleTimestamps(timestamps, timeOffset, timeScale);
        Array.prototype.push.apply(this._allocations.sites, sites);
        Array.prototype.push.apply(this._allocations.timestamps, timestamps);
        Array.prototype.push.apply(this._allocations.frames, frames);
        Array.prototype.push.apply(this._allocations.counts, counts);
        break;
      }
    }
  },

  toString: () => "[object RecordingModel]"
};

exports.RecordingModel = RecordingModel;
