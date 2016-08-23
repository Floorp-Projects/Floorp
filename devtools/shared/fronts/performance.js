/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu } = require("chrome");
const { Front, FrontClassWithSpec, custom, preEvent } = require("devtools/shared/protocol");
const { PerformanceRecordingFront } = require("devtools/shared/fronts/performance-recording");
const { performanceSpec } = require("devtools/shared/specs/performance");
const { Task } = require("devtools/shared/task");

loader.lazyRequireGetter(this, "PerformanceIO",
  "devtools/client/performance/modules/io");
loader.lazyRequireGetter(this, "LegacyPerformanceFront",
  "devtools/client/performance/legacy/front", true);
loader.lazyRequireGetter(this, "getSystemInfo",
  "devtools/shared/system", true);

const PerformanceFront = FrontClassWithSpec(performanceSpec, {
  initialize: function (client, form) {
    Front.prototype.initialize.call(this, client, form);
    this.actorID = form.performanceActor;
    this.manage(this);
  },

  destroy: function () {
    Front.prototype.destroy.call(this);
  },

  /**
   * Conenct to the server, and handle once-off tasks like storing traits
   * or system info.
   */
  connect: custom(Task.async(function* () {
    let systemClient = yield getSystemInfo();
    let { traits } = yield this._connect({ systemClient });
    this._traits = traits;

    return this._traits;
  }), {
    impl: "_connect"
  }),

  get traits() {
    if (!this._traits) {
      Cu.reportError("Cannot access traits of PerformanceFront before " +
                     "calling `connect()`.");
    }
    return this._traits;
  },

  /**
   * Pass in a PerformanceRecording and get a normalized value from 0 to 1 of how much
   * of this recording's lifetime remains without being overwritten.
   *
   * @param {PerformanceRecording} recording
   * @return {number?}
   */
  getBufferUsageForRecording: function (recording) {
    if (!recording.isRecording()) {
      return void 0;
    }
    let {
      position: currentPosition,
      totalSize,
      generation: currentGeneration
    } = this._currentBufferStatus;
    let {
      position: origPosition,
      generation: origGeneration
    } = recording.getStartingBufferStatus();

    let normalizedCurrent = (totalSize * (currentGeneration - origGeneration)) +
                            currentPosition;
    let percent = (normalizedCurrent - origPosition) / totalSize;

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
  },

  /**
   * Loads a recording from a file.
   *
   * @param {nsILocalFile} file
   *        The file to import the data from.
   * @return {Promise<PerformanceRecordingFront>}
   */
  importRecording: function (file) {
    return PerformanceIO.loadRecordingFromFile(file).then(recordingData => {
      let model = new PerformanceRecordingFront();
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
  },

  /**
   * Store profiler status when the position has been update so we can
   * calculate recording's buffer percentage usage after emitting the event.
   */
  _onProfilerStatus: preEvent("profiler-status", function (data) {
    this._currentBufferStatus = data;
  }),

  /**
   * For all PerformanceRecordings that are recording, and needing realtime markers,
   * apply the timeline data to the front PerformanceRecording (so we only have one event
   * for each timeline data chunk as they could be shared amongst several recordings).
   */
  _onTimelineEvent: preEvent("timeline-data", function (type, data, recordings) {
    for (let recording of recordings) {
      recording._addTimelineData(type, data);
    }
  }),
});

exports.PerformanceFront = PerformanceFront;

exports.createPerformanceFront = function createPerformanceFront(target) {
  // If we force legacy mode, or the server does not have a performance actor (< Fx42),
  // use our LegacyPerformanceFront which will handle
  // the communication over RDP to other underlying actors.
  if (target.TEST_PERFORMANCE_LEGACY_FRONT || !target.form.performanceActor) {
    return new LegacyPerformanceFront(target);
  }
  // If our server has a PerformanceActor implementation, set this
  // up like a normal front.
  return new PerformanceFront(target.client, target.form);
};
