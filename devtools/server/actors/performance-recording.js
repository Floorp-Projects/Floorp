/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const { performanceRecordingSpec } = require("devtools/shared/specs/performance-recording");

loader.lazyRequireGetter(this, "RecordingUtils",
  "devtools/shared/performance/recording-utils");
loader.lazyRequireGetter(this, "PerformanceRecordingCommon",
  "devtools/shared/performance/recording-common", true);

/**
 * This actor wraps the Performance module at devtools/shared/shared/performance.js
 * and provides RDP definitions.
 *
 * @see devtools/shared/shared/performance.js for documentation.
 */
const PerformanceRecordingActor = ActorClassWithSpec(performanceRecordingSpec,
Object.assign({
  form: function(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    const form = {
      // actorID is set when this is added to a pool
      actor: this.actorID,
      configuration: this._configuration,
      startingBufferStatus: this._startingBufferStatus,
      console: this._console,
      label: this._label,
      startTime: this._startTime,
      localStartTime: this._localStartTime,
      recording: this._recording,
      completed: this._completed,
      duration: this._duration,
    };

    // Only send profiler data once it exists and it has
    // not yet been sent
    if (this._profile && !this._sentFinalizedData) {
      form.finalizedData = true;
      form.profile = this.getProfile();
      form.systemHost = this.getHostSystemInfo();
      form.systemClient = this.getClientSystemInfo();
      this._sentFinalizedData = true;
    }

    return form;
  },

  /**
   * @param {object} conn
   * @param {object} options
   *        A hash of features that this recording is utilizing.
   * @param {object} meta
   *        A hash of temporary metadata for a recording that is recording
   *        (as opposed to an imported recording).
   */
  initialize: function(conn, options, meta) {
    Actor.prototype.initialize.call(this, conn);
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

    this._console = !!options.console;
    this._label = options.label || "";

    if (meta) {
      // Store the start time roughly with Date.now() so when we
      // are checking the duration during a recording, we can get close
      // to the approximate duration to render elements without
      // making a real request
      this._localStartTime = Date.now();

      this._startTime = meta.startTime;
      this._startingBufferStatus = {
        position: meta.position,
        totalSize: meta.totalSize,
        generation: meta.generation
      };

      this._recording = true;
      this._markers = [];
      this._frames = [];
      this._memory = [];
      this._ticks = [];
      this._allocations = { sites: [], timestamps: [], frames: [], sizes: [] };

      this._systemHost = meta.systemHost || {};
      this._systemClient = meta.systemClient || {};
    }
  },

  destroy: function() {
    Actor.prototype.destroy.call(this);
  },

  /**
   * Internal utility called by the PerformanceActor and PerformanceFront on state changes
   * to update the internal state of the PerformanceRecording.
   *
   * @param {string} state
   * @param {object} extraData
   */
  _setState: function(state, extraData) {
    switch (state) {
      case "recording-started": {
        this._recording = true;
        break;
      }
      case "recording-stopping": {
        this._recording = false;
        break;
      }
      case "recording-stopped": {
        this._profile = extraData.profile;
        this._duration = extraData.duration;

        // We filter out all samples that fall out of current profile's range
        // since the profiler is continuously running. Because of this, sample
        // times are not guaranteed to have a zero epoch, so offset the
        // timestamps.
        RecordingUtils.offsetSampleTimes(this._profile, this._startTime);

        // Markers need to be sorted ascending by time, to be properly displayed
        // in a waterfall view.
        this._markers = this._markers.sort((a, b) => (a.start > b.start));

        this._completed = true;
        break;
      }
    }
  },
}, PerformanceRecordingCommon));

exports.PerformanceRecordingActor = PerformanceRecordingActor;
