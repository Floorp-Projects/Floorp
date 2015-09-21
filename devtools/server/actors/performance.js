/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const protocol = require("devtools/server/protocol");
const { Task } = require("resource://gre/modules/Task.jsm");
const { Actor, custom, method, RetVal, Arg, Option, types, preEvent } = protocol;
const { actorBridge } = require("devtools/server/actors/common");
const { PerformanceRecordingActor, PerformanceRecordingFront } = require("devtools/server/actors/performance-recording");

loader.lazyRequireGetter(this, "events", "sdk/event/core");
loader.lazyRequireGetter(this, "extend", "sdk/util/object", true);

loader.lazyRequireGetter(this, "PerformanceRecorder",
  "devtools/shared/performance/recorder", true);
loader.lazyRequireGetter(this, "PerformanceIO",
  "devtools/shared/performance/io");
loader.lazyRequireGetter(this, "normalizePerformanceFeatures",
  "devtools/shared/performance/utils", true);
loader.lazyRequireGetter(this, "LegacyPerformanceFront",
  "devtools/shared/performance/legacy/front", true);
loader.lazyRequireGetter(this, "getSystemInfo",
  "devtools/shared/shared/system", true);

const PIPE_TO_FRONT_EVENTS = new Set([
  "recording-started", "recording-stopping", "recording-stopped",
  "profiler-status", "timeline-data", "console-profile-start"
]);

const RECORDING_STATE_CHANGE_EVENTS = new Set([
  "recording-started", "recording-stopping", "recording-stopped"
]);

/**
 * This actor wraps the Performance module at toolkit/devtools/shared/performance.js
 * and provides RDP definitions.
 *
 * @see toolkit/devtools/shared/performance.js for documentation.
 */
var PerformanceActor = exports.PerformanceActor = protocol.ActorClass({
  typeName: "performance",

  traits: {
    features: {
      withMarkers: true,
      withMemory: true,
      withTicks: true,
      withAllocations: true,
      withJITOptimizations: true,
    },
  },

  /**
   * The set of events the PerformanceActor emits over RDP.
   */
  events: {
    "recording-started": {
      recording: Arg(0, "performance-recording"),
    },
    "recording-stopping": {
      recording: Arg(0, "performance-recording"),
    },
    "recording-stopped": {
      recording: Arg(0, "performance-recording"),
      data: Arg(1, "json"),
    },
    "profiler-status": {
      data: Arg(0, "json"),
    },
    "console-profile-start": {},
    "timeline-data": {
      name: Arg(0, "string"),
      data: Arg(1, "json"),
      recordings: Arg(2, "array:performance-recording"),
    },
  },

  initialize: function (conn, tabActor) {
    Actor.prototype.initialize.call(this, conn);
    this._onRecorderEvent = this._onRecorderEvent.bind(this);
    this.bridge = new PerformanceRecorder(conn, tabActor);
    events.on(this.bridge, "*", this._onRecorderEvent);
  },

  /**
   * `disconnect` method required to call destroy, since this
   * actor is not managed by a parent actor.
   */
  disconnect: function() {
    this.destroy();
  },

  destroy: function () {
    events.off(this.bridge, "*", this._onRecorderEvent);
    this.bridge.destroy();
    protocol.Actor.prototype.destroy.call(this);
  },

  connect: method(function (config) {
    this.bridge.connect({ systemClient: config.systemClient });
    return { traits: this.traits };
  }, {
    request: { options: Arg(0, "nullable:json") },
    response: RetVal("json")
  }),

  startRecording: method(Task.async(function *(options={}) {
    let normalizedOptions = normalizePerformanceFeatures(options, this.traits.features);
    let recording = yield this.bridge.startRecording(normalizedOptions);

    this.manage(recording);

    return recording;
  }), {
    request: {
      options: Arg(0, "nullable:json"),
    },
    response: RetVal("performance-recording"),
  }),

  stopRecording: actorBridge("stopRecording", {
    request: {
      options: Arg(0, "performance-recording"),
    },
    response: RetVal("performance-recording"),
  }),

  isRecording: actorBridge("isRecording", {
    response: { isRecording: RetVal("boolean") }
  }),

  getRecordings: actorBridge("getRecordings", {
    response: { recordings: RetVal("array:performance-recording") }
  }),

  getConfiguration: actorBridge("getConfiguration", {
    response: { config: RetVal("json") }
  }),

  setProfilerStatusInterval: actorBridge("setProfilerStatusInterval", {
    request: { interval: Arg(0, "number") },
    response: { oneway: true }
  }),

  /**
   * Filter which events get piped to the front.
   */
  _onRecorderEvent: function (eventName, ...data) {
    // If this is a recording state change, call
    // a method on the related PerformanceRecordingActor so it can
    // update its internal state.
    if (RECORDING_STATE_CHANGE_EVENTS.has(eventName)) {
      let recording = data[0];
      let extraData = data[1];
      recording._setState(eventName, extraData);
    }

    if (PIPE_TO_FRONT_EVENTS.has(eventName)) {
      events.emit(this, eventName, ...data);
    }
  },
});

exports.createPerformanceFront = function createPerformanceFront (target) {
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

const PerformanceFront = exports.PerformanceFront = protocol.FrontClass(PerformanceActor, {
  initialize: function (client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
    this.actorID = form.performanceActor;
    this.manage(this);
  },

  destroy: function () {
    protocol.Front.prototype.destroy.call(this);
  },

  /**
   * Conenct to the server, and handle once-off tasks like storing traits
   * or system info.
   */
  connect: custom(Task.async(function *() {
    let systemClient = yield getSystemInfo();
    let { traits } = yield this._connect({ systemClient });
    this._traits = traits;

    return this._traits;
  }), {
    impl: "_connect"
  }),

  get traits() {
    if (!this._traits) {
      Cu.reportError("Cannot access traits of PerformanceFront before calling `connect()`.");
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
    let { position: currentPosition, totalSize, generation: currentGeneration } = this._currentBufferStatus;
    let { position: origPosition, generation: origGeneration } = recording.getStartingBufferStatus();

    let normalizedCurrent = (totalSize * (currentGeneration - origGeneration)) + currentPosition;
    let percent = (normalizedCurrent - origPosition) / totalSize;

    // Clamp between 0 and 1; can get negative percentage values when a new
    // recording starts and the currentBufferStatus has not yet been updated. Rather
    // than fetching another status update, just clamp to 0, and this will be updated
    // on the next profiler-status event.
    return percent > 1 ? 1 : percent < 0 ? 0 : percent;
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
