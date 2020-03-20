/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const {
  PerformanceRecordingFront,
} = require("devtools/shared/fronts/performance-recording");
const { performanceSpec } = require("devtools/shared/specs/performance");

loader.lazyRequireGetter(
  this,
  "PerformanceIO",
  "devtools/client/performance/modules/io"
);
loader.lazyRequireGetter(this, "getSystemInfo", "devtools/shared/system", true);

class PerformanceFront extends FrontClassWithSpec(performanceSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);
    this._queuedRecordings = [];
    this._onRecordingStartedEvent = this._onRecordingStartedEvent.bind(this);
    this.flushQueuedRecordings = this.flushQueuedRecordings.bind(this);

    this.before("profiler-status", this._onProfilerStatus.bind(this));
    this.before("timeline-data", this._onTimelineEvent.bind(this));
    this.on("recording-started", this._onRecordingStartedEvent);

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "performanceActor";
  }

  async initialize() {
    await this.connect();
  }

  /**
   * Conenct to the server, and handle once-off tasks like storing traits
   * or system info.
   */
  async connect() {
    const systemClient = await getSystemInfo();
    const { traits } = await super.connect({ systemClient });
    this._traits = traits;

    return this._traits;
  }

  /**
   * Called when the "recording-started" event comes from the PerformanceFront.
   * this is only used to queue up observed recordings before the performance tool can
   * handle them, which will only occur when `console.profile()` recordings are started
   * before the tool loads.
   */
  async _onRecordingStartedEvent(recording) {
    this._queuedRecordings.push(recording);
  }

  flushQueuedRecordings() {
    this.off("recording-started", this._onPerformanceFrontEvent);
    const recordings = this._queuedRecordings;
    this._queuedRecordings = [];
    return recordings;
  }

  get traits() {
    if (!this._traits) {
      Cu.reportError(
        "Cannot access traits of PerformanceFront before " +
          "calling `connect()`."
      );
    }
    return this._traits;
  }

  /**
   * Pass in a PerformanceRecording and get a normalized value from 0 to 1 of how much
   * of this recording's lifetime remains without being overwritten.
   *
   * @param {PerformanceRecording} recording
   * @return {number?}
   */
  getBufferUsageForRecording(recording) {
    if (!recording.isRecording()) {
      return void 0;
    }
    const {
      position: currentPosition,
      totalSize,
      generation: currentGeneration,
    } = this._currentBufferStatus;
    const {
      position: origPosition,
      generation: origGeneration,
    } = recording.getStartingBufferStatus();

    const normalizedCurrent =
      totalSize * (currentGeneration - origGeneration) + currentPosition;
    const percent = (normalizedCurrent - origPosition) / totalSize;

    // Clamp between 0 and 1; can get negative percentage values when a new
    // recording starts and the currentBufferStatus has not yet been updated. Rather
    // than fetching another status update, just clamp to 0, and this will be updated
    // on the next profiler-status event.
    if (percent < 0) {
      return 0;
    } else if (percent > 1) {
      return 1;
    }

    return percent;
  }

  /**
   * Loads a recording from a file.
   *
   * @param {nsIFile} file
   *        The file to import the data from.
   * @return {Promise<PerformanceRecordingFront>}
   */
  importRecording(file) {
    return PerformanceIO.loadRecordingFromFile(file).then(recordingData => {
      const model = new PerformanceRecordingFront();
      model._imported = true;
      model._label = recordingData.label || "";
      model._duration = recordingData.duration;
      model._markers = recordingData.markers;
      model._frames = recordingData.frames;
      model._memory = recordingData.memory;
      model._ticks = recordingData.ticks;
      model._allocations = recordingData.allocations;
      model._profile = recordingData.profile;
      model._configuration = recordingData.configuration || {};
      model._systemHost = recordingData.systemHost;
      model._systemClient = recordingData.systemClient;
      return model;
    });
  }

  /**
   * Store profiler status when the position has been update so we can
   * calculate recording's buffer percentage usage after emitting the event.
   */
  _onProfilerStatus(data) {
    this._currentBufferStatus = data;
  }

  /**
   * For all PerformanceRecordings that are recording, and needing realtime markers,
   * apply the timeline data to the front PerformanceRecording (so we only have one event
   * for each timeline data chunk as they could be shared amongst several recordings).
   */
  _onTimelineEvent(type, data, recordings) {
    for (const recording of recordings) {
      recording._addTimelineData(type, data);
    }
  }
}

exports.PerformanceFront = PerformanceFront;
registerFront(PerformanceFront);
