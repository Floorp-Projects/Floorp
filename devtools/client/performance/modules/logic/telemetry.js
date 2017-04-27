/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Telemetry = require("devtools/client/shared/telemetry");
const flags = require("devtools/shared/flags");
const EVENTS = require("devtools/client/performance/events");

const EVENT_MAP_FLAGS = new Map([
  [EVENTS.RECORDING_IMPORTED, "DEVTOOLS_PERFTOOLS_RECORDING_IMPORT_FLAG"],
  [EVENTS.RECORDING_EXPORTED, "DEVTOOLS_PERFTOOLS_RECORDING_EXPORT_FLAG"],
]);

const RECORDING_FEATURES = [
  "withMarkers", "withTicks", "withMemory", "withAllocations"
];

const SELECTED_VIEW_HISTOGRAM_NAME = "DEVTOOLS_PERFTOOLS_SELECTED_VIEW_MS";

function PerformanceTelemetry(emitter) {
  this._emitter = emitter;
  this._telemetry = new Telemetry();
  this.onFlagEvent = this.onFlagEvent.bind(this);
  this.onRecordingStateChange = this.onRecordingStateChange.bind(this);
  this.onViewSelected = this.onViewSelected.bind(this);

  for (let [event] of EVENT_MAP_FLAGS) {
    this._emitter.on(event, this.onFlagEvent);
  }

  this._emitter.on(EVENTS.RECORDING_STATE_CHANGE, this.onRecordingStateChange);
  this._emitter.on(EVENTS.UI_DETAILS_VIEW_SELECTED, this.onViewSelected);

  if (flags.testing) {
    this.recordLogs();
  }
}

PerformanceTelemetry.prototype.destroy = function () {
  if (this._previousView) {
    this._telemetry.stopTimer(SELECTED_VIEW_HISTOGRAM_NAME, this._previousView);
  }

  this._telemetry.destroy();
  for (let [event] of EVENT_MAP_FLAGS) {
    this._emitter.off(event, this.onFlagEvent);
  }
  this._emitter.off(EVENTS.RECORDING_STATE_CHANGE, this.onRecordingStateChange);
  this._emitter.off(EVENTS.UI_DETAILS_VIEW_SELECTED, this.onViewSelected);
  this._emitter = null;
};

PerformanceTelemetry.prototype.onFlagEvent = function (eventName, ...data) {
  this._telemetry.log(EVENT_MAP_FLAGS.get(eventName), true);
};

PerformanceTelemetry.prototype.onRecordingStateChange = function (_, status, model) {
  if (status != "recording-stopped") {
    return;
  }

  if (model.isConsole()) {
    this._telemetry.log("DEVTOOLS_PERFTOOLS_CONSOLE_RECORDING_COUNT", true);
  } else {
    this._telemetry.log("DEVTOOLS_PERFTOOLS_RECORDING_COUNT", true);
  }

  this._telemetry.log("DEVTOOLS_PERFTOOLS_RECORDING_DURATION_MS", model.getDuration());

  let config = model.getConfiguration();
  for (let k in config) {
    if (RECORDING_FEATURES.indexOf(k) !== -1) {
      this._telemetry.logKeyed("DEVTOOLS_PERFTOOLS_RECORDING_FEATURES_USED", k,
                               config[k]);
    }
  }
};

PerformanceTelemetry.prototype.onViewSelected = function (_, viewName) {
  if (this._previousView) {
    this._telemetry.stopTimer(SELECTED_VIEW_HISTOGRAM_NAME, this._previousView);
  }
  this._previousView = viewName;
  this._telemetry.startTimer(SELECTED_VIEW_HISTOGRAM_NAME);
};

/**
 * Utility to record histogram calls to this instance.
 * Should only be used in testing mode; throws otherwise.
 */
PerformanceTelemetry.prototype.recordLogs = function () {
  if (!flags.testing) {
    throw new Error("Can only record telemetry logs in tests.");
  }

  let originalLog = this._telemetry.log;
  let originalLogKeyed = this._telemetry.logKeyed;
  this._log = {};

  this._telemetry.log = (histo, data) => {
    let results = this._log[histo] = this._log[histo] || [];
    results.push(data);
    originalLog(histo, data);
  };

  this._telemetry.logKeyed = (histo, key, data) => {
    let results = this._log[histo] = this._log[histo] || [];
    results.push([key, data]);
    originalLogKeyed(histo, key, data);
  };
};

PerformanceTelemetry.prototype.getLogs = function () {
  if (!flags.testing) {
    throw new Error("Can only get telemetry logs in tests.");
  }

  return this._log;
};

exports.PerformanceTelemetry = PerformanceTelemetry;
