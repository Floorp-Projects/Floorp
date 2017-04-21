/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* global helpers, btoa, whenDelayedStartupFinished, OpenBrowserWindow */

// Test that GCLI telemetry works properly

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,browser_gcli_telemetry.js";
const COMMAND_HISTOGRAM_ID = "DEVTOOLS_GCLI_COMMANDS_KEYED";

function test() {
  return Task.spawn(spawnTest).then(finish, helpers.handleError);
}

function* spawnTest() {
  let options = yield helpers.openTab(TEST_URI);
  let Telemetry = loadTelemetryAndRecordLogs();

  yield helpers.openToolbar(options);

  yield helpers.audit(options, [
    {
      setup: "addon list<RETURN>"
    },
    {
      setup: "appcache clear<RETURN>"
    },
    {
      setup: "clear<RETURN>"
    },
    {
      setup: "console clear<RETURN>"
    },
    {
      setup: "cookie list<RETURN>"
    },
    {
      setup: "help<RETURN>"
    },
    {
      setup: "help addon<RETURN>"
    },
    {
      setup: "screenshot<RETURN>"
    },
    {
      setup: "listen 6000<RETURN>"
    },
    {
      setup: "unlisten<RETURN>"
    },
    {
      setup: "context addon<RETURN>"
    },
  ]);

  let results = Telemetry.prototype.telemetryInfo;

  checkTelemetryResults(results);
  stopRecordingTelemetryLogs(Telemetry);

  info("Closing Developer Toolbar");
  yield helpers.closeToolbar(options);

  info("Closing tab");
  yield helpers.closeTab(options);
}

/**
 * Load the Telemetry utils, then stub Telemetry.prototype.log and
 * Telemetry.prototype.logKeyed in order to record everything that's logged in
 * it.
 * Store all recordings in Telemetry.telemetryInfo.
 * @return {Telemetry}
 */
function loadTelemetryAndRecordLogs() {
  info("Mock the Telemetry log function to record logged information");

  let Telemetry = require("devtools/client/shared/telemetry");
  Telemetry.prototype.telemetryInfo = {};
  Telemetry.prototype._oldlog = Telemetry.prototype.log;
  Telemetry.prototype.log = function (histogramId, value) {
    if (!this.telemetryInfo) {
      // Telemetry instance still in use after stopRecordingTelemetryLogs
      return;
    }
    if (histogramId) {
      if (!this.telemetryInfo[histogramId]) {
        this.telemetryInfo[histogramId] = [];
      }
      this.telemetryInfo[histogramId].push(value);
    }
  };
  Telemetry.prototype._oldlogScalar = Telemetry.prototype.logScalar;
  Telemetry.prototype.logScalar = Telemetry.prototype.log;
  Telemetry.prototype._oldlogKeyed = Telemetry.prototype.logKeyed;
  Telemetry.prototype.logKeyed = function (histogramId, key, value) {
    this.log(`${histogramId}|${key}`, value);
  };

  return Telemetry;
}

/**
 * Stop recording the Telemetry logs and put back the utils as it was before.
 * @param {Telemetry} Required Telemetry
 *        Telemetry object that needs to be stopped.
 */
function stopRecordingTelemetryLogs(Telemetry) {
  info("Stopping Telemetry");
  Telemetry.prototype.log = Telemetry.prototype._oldlog;
  Telemetry.prototype.logScalar = Telemetry.prototype._oldlogScalar;
  Telemetry.prototype.logKeyed = Telemetry.prototype._oldlogKeyed;
  delete Telemetry.prototype._oldlog;
  delete Telemetry.prototype._oldlogScalar;
  delete Telemetry.prototype._oldlogKeyed;
  delete Telemetry.prototype.telemetryInfo;
}

function checkTelemetryResults(results) {
  let prefix = COMMAND_HISTOGRAM_ID + "|";
  let keys = Object.keys(results).filter(result => {
    return result.startsWith(prefix);
  });

  let commands = [
    "addon list",
    "appcache clear",
    "clear",
    "console clear",
    "cookie list",
    "screenshot",
    "listen",
    "unlisten",
    "context",
    "help"
  ];

  for (let command of commands) {
    let key = prefix + command;

    switch (key) {
      case "DEVTOOLS_GCLI_COMMANDS_KEYED|addon list":
      case "DEVTOOLS_GCLI_COMMANDS_KEYED|appcache clear":
      case "DEVTOOLS_GCLI_COMMANDS_KEYED|clear":
      case "DEVTOOLS_GCLI_COMMANDS_KEYED|console clear":
      case "DEVTOOLS_GCLI_COMMANDS_KEYED|cookie list":
      case "DEVTOOLS_GCLI_COMMANDS_KEYED|screenshot":
      case "DEVTOOLS_GCLI_COMMANDS_KEYED|listen":
      case "DEVTOOLS_GCLI_COMMANDS_KEYED|unlisten":
      case "DEVTOOLS_GCLI_COMMANDS_KEYED|context":
        is(results[key].length, 1, `${key} is correct`);
        break;
      case "DEVTOOLS_GCLI_COMMANDS_KEYED|help":
        is(results[key].length, 2, `${key} is correct`);
        break;
      default:
        ok(false, `No telemetry pings were sent for command "${command}"`);
    }
  }
}
