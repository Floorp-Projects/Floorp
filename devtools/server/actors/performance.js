/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Task } = require("devtools/shared/task");
const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const { actorBridgeWithSpec } = require("devtools/server/actors/common");
const { performanceSpec } = require("devtools/shared/specs/performance");

loader.lazyRequireGetter(this, "events", "sdk/event/core");

loader.lazyRequireGetter(this, "PerformanceRecorder",
  "devtools/server/performance/recorder", true);
loader.lazyRequireGetter(this, "normalizePerformanceFeatures",
  "devtools/shared/performance/recording-utils", true);

const PIPE_TO_FRONT_EVENTS = new Set([
  "recording-started", "recording-stopping", "recording-stopped",
  "profiler-status", "timeline-data", "console-profile-start"
]);

const RECORDING_STATE_CHANGE_EVENTS = new Set([
  "recording-started", "recording-stopping", "recording-stopped"
]);

/**
 * This actor wraps the Performance module at devtools/shared/shared/performance.js
 * and provides RDP definitions.
 *
 * @see devtools/shared/shared/performance.js for documentation.
 */
var PerformanceActor = ActorClassWithSpec(performanceSpec, {
  traits: {
    features: {
      withMarkers: true,
      withTicks: true,
      withMemory: true,
      withFrames: true,
      withGCEvents: true,
      withDocLoadingEvents: true,
      withAllocations: true,
    },
  },

  initialize: function (conn, tabActor) {
    Actor.prototype.initialize.call(this, conn);
    this._onRecorderEvent = this._onRecorderEvent.bind(this);
    this.bridge = new PerformanceRecorder(conn, tabActor);
    events.on(this.bridge, "*", this._onRecorderEvent);
  },

  destroy: function () {
    events.off(this.bridge, "*", this._onRecorderEvent);
    this.bridge.destroy();
    Actor.prototype.destroy.call(this);
  },

  connect: function (config) {
    this.bridge.connect({ systemClient: config.systemClient });
    return { traits: this.traits };
  },

  canCurrentlyRecord: function () {
    return this.bridge.canCurrentlyRecord();
  },

  startRecording: Task.async(function* (options = {}) {
    if (!this.bridge.canCurrentlyRecord().success) {
      return null;
    }

    let normalizedOptions = normalizePerformanceFeatures(options, this.traits.features);
    let recording = yield this.bridge.startRecording(normalizedOptions);
    this.manage(recording);

    return recording;
  }),

  stopRecording: actorBridgeWithSpec("stopRecording"),
  isRecording: actorBridgeWithSpec("isRecording"),
  getRecordings: actorBridgeWithSpec("getRecordings"),
  getConfiguration: actorBridgeWithSpec("getConfiguration"),
  setProfilerStatusInterval: actorBridgeWithSpec("setProfilerStatusInterval"),

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

exports.PerformanceActor = PerformanceActor;
