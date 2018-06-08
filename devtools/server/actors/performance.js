/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const { actorBridgeWithSpec } = require("devtools/server/actors/common");
const { performanceSpec } = require("devtools/shared/specs/performance");

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

  initialize: function(conn, targetActor) {
    Actor.prototype.initialize.call(this, conn);

    this._onRecordingStarted = this._onRecordingStarted.bind(this);
    this._onRecordingStopping = this._onRecordingStopping.bind(this);
    this._onRecordingStopped = this._onRecordingStopped.bind(this);
    this._onProfilerStatus = this._onProfilerStatus.bind(this);
    this._onTimelineData = this._onTimelineData.bind(this);
    this._onConsoleProfileStart = this._onConsoleProfileStart.bind(this);

    this.bridge = new PerformanceRecorder(conn, targetActor);

    this.bridge.on("recording-started", this._onRecordingStarted);
    this.bridge.on("recording-stopping", this._onRecordingStopping);
    this.bridge.on("recording-stopped", this._onRecordingStopped);
    this.bridge.on("profiler-status", this._onProfilerStatus);
    this.bridge.on("timeline-data", this._onTimelineData);
    this.bridge.on("console-profile-start", this._onConsoleProfileStart);
  },

  destroy: function() {
    this.bridge.off("recording-started", this._onRecordingStarted);
    this.bridge.off("recording-stopping", this._onRecordingStopping);
    this.bridge.off("recording-stopped", this._onRecordingStopped);
    this.bridge.off("profiler-status", this._onProfilerStatus);
    this.bridge.off("timeline-data", this._onTimelineData);
    this.bridge.off("console-profile-start", this._onConsoleProfileStart);

    this.bridge.destroy();
    Actor.prototype.destroy.call(this);
  },

  connect: function(config) {
    this.bridge.connect({ systemClient: config.systemClient });
    return { traits: this.traits };
  },

  canCurrentlyRecord: function() {
    return this.bridge.canCurrentlyRecord();
  },

  async startRecording(options = {}) {
    if (!this.bridge.canCurrentlyRecord().success) {
      return null;
    }

    const normalizedOptions = normalizePerformanceFeatures(options, this.traits.features);
    const recording = await this.bridge.startRecording(normalizedOptions);
    this.manage(recording);

    return recording;
  },

  stopRecording: actorBridgeWithSpec("stopRecording"),
  isRecording: actorBridgeWithSpec("isRecording"),
  getRecordings: actorBridgeWithSpec("getRecordings"),
  getConfiguration: actorBridgeWithSpec("getConfiguration"),
  setProfilerStatusInterval: actorBridgeWithSpec("setProfilerStatusInterval"),

  /**
   * Filter which events get piped to the front.
   */
  _onRecordingStarted: function(...data) {
    this._onRecorderEvent("recording-started", data);
  },

  _onRecordingStopping: function(...data) {
    this._onRecorderEvent("recording-stopping", data);
  },

  _onRecordingStopped: function(...data) {
    this._onRecorderEvent("recording-stopped", data);
  },

  _onProfilerStatus: function(...data) {
    this._onRecorderEvent("profiler-status", data);
  },

  _onTimelineData: function(...data) {
    this._onRecorderEvent("timeline-data", data);
  },

  _onConsoleProfileStart: function(...data) {
    this._onRecorderEvent("console-profile-start", data);
  },

  _onRecorderEvent: function(eventName, data) {
    // If this is a recording state change, call
    // a method on the related PerformanceRecordingActor so it can
    // update its internal state.
    if (RECORDING_STATE_CHANGE_EVENTS.has(eventName)) {
      const recording = data[0];
      const extraData = data[1];
      recording._setState(eventName, extraData);
    }

    if (PIPE_TO_FRONT_EVENTS.has(eventName)) {
      this.emit(eventName, ...data);
    }
  },
});

exports.PerformanceActor = PerformanceActor;
