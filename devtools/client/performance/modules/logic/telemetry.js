/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Telemetry = require("devtools/client/shared/telemetry");
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
    this._emitter.on(event, this.onFlagEvent.bind(this, event));
  }

  this._emitter.on(EVENTS.RECORDING_STATE_CHANGE, this.onRecordingStateChange);
  this._emitter.on(EVENTS.UI_DETAILS_VIEW_SELECTED, this.onViewSelected);
}

PerformanceTelemetry.prototype.destroy = function() {
  if (this._previousView) {
    this._telemetry.finishKeyed(
      SELECTED_VIEW_HISTOGRAM_NAME, this._previousView, this);
  }

  for (let [event] of EVENT_MAP_FLAGS) {
    this._emitter.off(event, this.onFlagEvent);
  }
  this._emitter.off(EVENTS.RECORDING_STATE_CHANGE, this.onRecordingStateChange);
  this._emitter.off(EVENTS.UI_DETAILS_VIEW_SELECTED, this.onViewSelected);
  this._emitter = null;
};

PerformanceTelemetry.prototype.onFlagEvent = function(eventName, ...data) {
  this._telemetry.getHistogramById(EVENT_MAP_FLAGS.get(eventName)).add(true);
};

PerformanceTelemetry.prototype.onRecordingStateChange = function(status, model) {
  if (status != "recording-stopped") {
    return;
  }

  if (model.isConsole()) {
    this._telemetry.getHistogramById("DEVTOOLS_PERFTOOLS_CONSOLE_RECORDING_COUNT")
                   .add(true);
  } else {
    this._telemetry.getHistogramById("DEVTOOLS_PERFTOOLS_RECORDING_COUNT")
                   .add(true);
  }

  this._telemetry.getHistogramById("DEVTOOLS_PERFTOOLS_RECORDING_DURATION_MS")
                 .add(model.getDuration());

  let config = model.getConfiguration();
  for (let k in config) {
    if (RECORDING_FEATURES.includes(k)) {
      this._telemetry
          .getKeyedHistogramById("DEVTOOLS_PERFTOOLS_RECORDING_FEATURES_USED")
          .add(k, config[k]);
    }
  }
};

PerformanceTelemetry.prototype.onViewSelected = function(viewName) {
  if (this._previousView) {
    this._telemetry.finishKeyed(
      SELECTED_VIEW_HISTOGRAM_NAME, this._previousView, this);
  }
  this._previousView = viewName;
  this._telemetry.startKeyed(SELECTED_VIEW_HISTOGRAM_NAME, viewName, this);
};

exports.PerformanceTelemetry = PerformanceTelemetry;
