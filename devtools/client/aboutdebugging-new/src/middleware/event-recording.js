/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Telemetry = require("devtools/client/shared/telemetry");
loader.lazyGetter(this, "telemetry", () => new Telemetry());
// This is a unique id that should be submitted with all about:debugging events.
loader.lazyGetter(this, "sessionId", () => parseInt(telemetry.msSinceProcessStart(), 10));

const {
  CONNECT_RUNTIME_SUCCESS,
  DISCONNECT_RUNTIME_SUCCESS,
  REMOTE_RUNTIMES_UPDATED,
  RUNTIMES,
  SELECT_PAGE_SUCCESS,
  TELEMETRY_RECORD,
} = require("../constants");

const {
  findRuntimeById,
  getAllRuntimes,
} = require("../modules/runtimes-state-helper");

function recordEvent(method, details) {
  // Add the session id to the event details.
  const eventDetails = Object.assign({}, details, { "session_id": sessionId });
  telemetry.recordEvent(method, "aboutdebugging", null, eventDetails);
}

const telemetryRuntimeIds = new Map();
// Create an anonymous id that will allow to track all events related to a runtime without
// leaking personal data related to this runtime.
function getTelemetryRuntimeId(id) {
  if (!telemetryRuntimeIds.has(id)) {
    const randomId = (Math.random() * 100000) | 0;
    telemetryRuntimeIds.set(id, "runtime-" + randomId);
  }
  return telemetryRuntimeIds.get(id);
}

function getRuntimeEventExtras(runtime) {
  const { extra, runtimeDetails } = runtime;

  // deviceName can be undefined for non-usb devices, but we should not log "undefined".
  const deviceName = extra && extra.deviceName || "";
  const runtimeShortName = runtime.type === RUNTIMES.USB ? runtime.name : "";
  const runtimeName = runtimeDetails && runtimeDetails.info.name || "";
  return {
    "connection_type": runtime.type,
    "device_name": deviceName,
    "runtime_id": getTelemetryRuntimeId(runtime.id),
    "runtime_name": runtimeName || runtimeShortName,
  };
}

function onConnectRuntimeSuccess(action, store) {
  if (action.runtime.type === RUNTIMES.THIS_FIREFOX) {
    // Only record connection and disconnection events for remote runtimes.
    return;
  }
  // When we just connected to a runtime, the runtimeDetails are not in the store yet,
  // so we merge it here to retrieve the expected telemetry data.
  const storeRuntime = findRuntimeById(action.runtime.id, store.getState().runtimes);
  const runtime = Object.assign({}, storeRuntime, {
    runtimeDetails: action.runtime.runtimeDetails,
  });
  recordEvent("runtime_connected", getRuntimeEventExtras(runtime));
}

function onDisconnectRuntimeSuccess(action, store) {
  const runtime = findRuntimeById(action.runtime.id, store.getState().runtimes);
  if (runtime.type === RUNTIMES.THIS_FIREFOX) {
    // Only record connection and disconnection events for remote runtimes.
    return;
  }

  recordEvent("runtime_disconnected", getRuntimeEventExtras(runtime));
}

function onRemoteRuntimesUpdated(action, store) {
  // Compare new runtimes with the existing runtimes to detect if runtimes, devices
  // have been added or removed.
  const newRuntimes = action.runtimes;
  const allRuntimes = getAllRuntimes(store.getState().runtimes);
  const oldRuntimes = allRuntimes.filter(r => r.type === action.runtimeType);

  // Check if all the old runtimes and devices are still available in the updated
  // array.
  for (const oldRuntime of oldRuntimes) {
    const runtimeRemoved = newRuntimes.every(r => r.id !== oldRuntime.id);
    if (runtimeRemoved) {
      recordEvent("runtime_removed", getRuntimeEventExtras(oldRuntime));
    }
  }

  const oldDeviceNames = new Set(oldRuntimes.map(r => r.extra.deviceName));
  for (const oldDeviceName of oldDeviceNames) {
    const deviceRemoved = newRuntimes.every(r => r.extra.deviceName !== oldDeviceName);
    if (oldDeviceName && deviceRemoved) {
      recordEvent("device_removed", {
        "connection_type": action.runtimeType,
        "device_name": oldDeviceName,
      });
    }
  }

  // Check if the new runtimes and devices were already available in the existing
  // array.
  for (const newRuntime of newRuntimes) {
    const runtimeAdded = oldRuntimes.every(r => r.id !== newRuntime.id);
    if (runtimeAdded) {
      recordEvent("runtime_added", getRuntimeEventExtras(newRuntime));
    }
  }

  const newDeviceNames = new Set(newRuntimes.map(r => r.extra.deviceName));
  for (const newDeviceName of newDeviceNames) {
    const deviceAdded = oldRuntimes.every(r => r.extra.deviceName !== newDeviceName);
    if (newDeviceName && deviceAdded) {
      recordEvent("device_added", {
        "connection_type": action.runtimeType,
        "device_name": newDeviceName,
      });
    }
  }
}

/**
 * This middleware will record events to telemetry for some specific actions.
 */
function eventRecordingMiddleware(store) {
  return next => action => {
    switch (action.type) {
      case CONNECT_RUNTIME_SUCCESS:
        onConnectRuntimeSuccess(action, store);
        break;
      case DISCONNECT_RUNTIME_SUCCESS:
        onDisconnectRuntimeSuccess(action, store);
        break;
      case REMOTE_RUNTIMES_UPDATED:
        onRemoteRuntimesUpdated(action, store);
        break;
      case SELECT_PAGE_SUCCESS:
        recordEvent("select_page", { "page_type": action.page });
        break;
      case TELEMETRY_RECORD:
        const { method, details } = action;
        if (method) {
          recordEvent(method, details);
        } else {
          console.error(`[RECORD EVENT FAILED] ${action.type}: no "method" property`);
        }
        break;
    }

    return next(action);
  };
}

module.exports = eventRecordingMiddleware;
