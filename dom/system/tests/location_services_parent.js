/**
 * Loaded as a frame script fetch telemetry for
 * test_location_services_telemetry.html
 */

/* global addMessageListener, sendAsyncMessage */

"use strict";

const HISTOGRAM_KEY = "REGION_LOCATION_SERVICES_DIFFERENCE";

addMessageListener("getTelemetryEvents", options => {
  let result = Services.telemetry.getHistogramById(HISTOGRAM_KEY).snapshot();
  sendAsyncMessage("getTelemetryEvents", result);
});

addMessageListener("clear", options => {
  Services.telemetry.getHistogramById(HISTOGRAM_KEY).clear();
  sendAsyncMessage("clear", true);
});
