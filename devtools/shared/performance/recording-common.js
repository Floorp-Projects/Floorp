/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * A mixin to be used for PerformanceRecordingActor, PerformanceRecordingFront,
 * and LegacyPerformanceRecording for helper methods to access data.
 */

exports.PerformanceRecordingCommon = {
  // Private fields, only needed when a recording is started or stopped.
  _console: false,
  _imported: false,
  _recording: false,
  _completed: false,
  _configuration: {},
  _startingBufferStatus: null,
  _localStartTime: 0,

  // Serializable fields, necessary and sufficient for import and export.
  _label: "",
  _duration: 0,
  _markers: null,
  _frames: null,
  _memory: null,
  _ticks: null,
  _allocations: null,
  _profile: null,
  _systemHost: null,
  _systemClient: null,

  /**
   * Helper methods for returning the status of the recording.
   * These methods should be consistent on both the front and actor.
   */
  isRecording: function () {
    return this._recording;
  },
  isCompleted: function () {
    return this._completed || this.isImported();
  },
  isFinalizing: function () {
    return !this.isRecording() && !this.isCompleted();
  },
  isConsole: function () {
    return this._console;
  },
  isImported: function () {
    return this._imported;
  },

  /**
   * Helper methods for returning configuration for the recording.
   * These methods should be consistent on both the front and actor.
   */
  getConfiguration: function () {
    return this._configuration;
  },
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
    // when new data is not being generated. If recording is completed, use
    // the duration from the profiler; if between recording and being finalized,
    // use the last estimated duration.
    if (this.isRecording()) {
      this._estimatedDuration = Date.now() - this._localStartTime;
      return this._estimatedDuration;
    }
    return this._duration || this._estimatedDuration || 0;
  },

  /**
   * Helper methods for returning recording data.
   * These methods should be consistent on both the front and actor.
   */
  getMarkers: function () {
    return this._markers;
  },
  getFrames: function () {
    return this._frames;
  },
  getMemory: function () {
    return this._memory;
  },
  getTicks: function () {
    return this._ticks;
  },
  getAllocations: function () {
    return this._allocations;
  },
  getProfile: function () {
    return this._profile;
  },
  getHostSystemInfo: function () {
    return this._systemHost;
  },
  getClientSystemInfo: function () {
    return this._systemClient;
  },
  getStartingBufferStatus: function () {
    return this._startingBufferStatus;
  },

  getAllData: function () {
    let label = this.getLabel();
    let duration = this.getDuration();
    let markers = this.getMarkers();
    let frames = this.getFrames();
    let memory = this.getMemory();
    let ticks = this.getTicks();
    let allocations = this.getAllocations();
    let profile = this.getProfile();
    let configuration = this.getConfiguration();
    let systemHost = this.getHostSystemInfo();
    let systemClient = this.getClientSystemInfo();

    return {
      label,
      duration,
      markers,
      frames,
      memory,
      ticks,
      allocations,
      profile,
      configuration,
      systemHost,
      systemClient
    };
  },
};
