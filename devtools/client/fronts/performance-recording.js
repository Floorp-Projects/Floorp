/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const {
  performanceRecordingSpec,
} = require("devtools/shared/specs/performance-recording");

loader.lazyRequireGetter(
  this,
  "PerformanceIO",
  "devtools/client/performance/modules/io"
);
loader.lazyRequireGetter(
  this,
  "PerformanceRecordingCommon",
  "devtools/shared/performance/recording-common",
  true
);
loader.lazyRequireGetter(
  this,
  "RecordingUtils",
  "devtools/shared/performance/recording-utils"
);

/**
 * This can be used on older Profiler implementations, but the methods cannot
 * be changed -- you must introduce a new method, and detect the server.
 */
class PerformanceRecordingFront extends FrontClassWithSpec(
  performanceRecordingSpec
) {
  form(form) {
    this.actorID = form.actor;
    this._form = form;
    this._configuration = form.configuration;
    this._startingBufferStatus = form.startingBufferStatus;
    this._console = form.console;
    this._label = form.label;
    this._startTime = form.startTime;
    this._localStartTime = form.localStartTime;
    this._recording = form.recording;
    this._completed = form.completed;
    this._duration = form.duration;

    if (form.finalizedData) {
      this._profile = form.profile;
      this._systemHost = form.systemHost;
      this._systemClient = form.systemClient;
    }

    // Sort again on the client side if we're using realtime markers and the recording
    // just finished. This is because GC/Compositing markers can come into the array out
    // of order with the other markers, leading to strange collapsing in waterfall view.
    if (this._completed && !this._markersSorted) {
      this._markers = this._markers.sort((a, b) => a.start > b.start);
      this._markersSorted = true;
    }
  }

  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);
    this._markers = [];
    this._frames = [];
    this._memory = [];
    this._ticks = [];
    this._allocations = { sites: [], timestamps: [], frames: [], sizes: [] };
  }

  /**
   * Saves the current recording to a file.
   *
   * @param nsIFile file
   *        The file to stream the data into.
   */
  exportRecording(file) {
    const recordingData = this.getAllData();
    return PerformanceIO.saveRecordingToFile(recordingData, file);
  }

  /**
   * Fired whenever the PerformanceFront emits markers, memory or ticks.
   */
  _addTimelineData(eventName, data) {
    const config = this.getConfiguration();

    switch (eventName) {
      // Accumulate timeline markers into an array. Furthermore, the timestamps
      // do not have a zero epoch, so offset all of them by the start time.
      case "markers": {
        if (!config.withMarkers) {
          break;
        }
        const { markers } = data;
        RecordingUtils.offsetMarkerTimes(markers, this._startTime);
        RecordingUtils.pushAll(this._markers, markers);
        break;
      }
      // Accumulate stack frames into an array.
      case "frames": {
        if (!config.withMarkers) {
          break;
        }
        const { frames } = data;
        RecordingUtils.pushAll(this._frames, frames);
        break;
      }
      // Accumulate memory measurements into an array. Furthermore, the timestamp
      // does not have a zero epoch, so offset it by the actor's start time.
      case "memory": {
        if (!config.withMemory) {
          break;
        }
        const { delta, measurement } = data;
        this._memory.push({
          delta: delta - this._startTime,
          value: measurement.total / 1024 / 1024,
        });
        break;
      }
      // Save the accumulated refresh driver ticks.
      case "ticks": {
        if (!config.withTicks) {
          break;
        }
        const { timestamps } = data;
        this._ticks = timestamps;
        break;
      }
      // Accumulate allocation sites into an array.
      case "allocations": {
        if (!config.withAllocations) {
          break;
        }
        const {
          allocations: sites,
          allocationsTimestamps: timestamps,
          allocationSizes: sizes,
          frames,
        } = data;

        RecordingUtils.offsetAndScaleTimestamps(timestamps, this._startTime);
        RecordingUtils.pushAll(this._allocations.sites, sites);
        RecordingUtils.pushAll(this._allocations.timestamps, timestamps);
        RecordingUtils.pushAll(this._allocations.frames, frames);
        RecordingUtils.pushAll(this._allocations.sizes, sizes);
        break;
      }
    }
  }

  toString() {
    return "[object PerformanceRecordingFront]";
  }
}

// PerformanceRecordingFront also needs to inherit from PerformanceRecordingCommon
// but as ES classes don't support multiple inheritance, we are overriding the
// prototype with PerformanceRecordingCommon methods.
Object.defineProperties(
  PerformanceRecordingFront.prototype,
  Object.getOwnPropertyDescriptors(PerformanceRecordingCommon)
);

exports.PerformanceRecordingFront = PerformanceRecordingFront;
registerFront(PerformanceRecordingFront);
