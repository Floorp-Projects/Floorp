/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Telemetry = require("devtools/client/shared/telemetry");
loader.lazyGetter(this, "telemetry", () => new Telemetry());
// This is a unique id that should be submitted with all about:debugging events.
loader.lazyGetter(this, "sessionId", () => parseInt(telemetry.msSinceProcessStart(), 10));

const {
  CONNECT_RUNTIME_CANCEL,
  CONNECT_RUNTIME_FAILURE,
  CONNECT_RUNTIME_NOT_RESPONDING,
  CONNECT_RUNTIME_START,
  CONNECT_RUNTIME_SUCCESS,
  DISCONNECT_RUNTIME_SUCCESS,
  REMOTE_RUNTIMES_UPDATED,
  RUNTIMES,
  SELECT_PAGE_SUCCESS,
  SHOW_PROFILER_DIALOG,
  TELEMETRY_RECORD,
  UPDATE_CONNECTION_PROMPT_SETTING_SUCCESS,
} = require("../constants");

const {
  findRuntimeById,
  getAllRuntimes,
  getCurrentRuntime,
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

function getCurrentRuntimeIdForTelemetry(store) {
  const id = getCurrentRuntime(store.getState().runtimes).id;
  return getTelemetryRuntimeId(id);
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
  const extras = Object.assign({}, getRuntimeEventExtras(runtime), {
    "runtime_os": action.runtime.runtimeDetails.info.os,
    "runtime_version": action.runtime.runtimeDetails.info.version,
  });
  recordEvent("runtime_connected", extras);
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
    if (runtimeRemoved && !oldRuntime.isUnplugged) {
      recordEvent("runtime_removed", getRuntimeEventExtras(oldRuntime));
    }
  }

  // Using device names as unique IDs is inaccurate. See Bug 1544582.
  const oldDeviceNames = new Set(oldRuntimes.map(r => r.extra.deviceName));
  for (const oldDeviceName of oldDeviceNames) {
    const newRuntime = newRuntimes.find(r => r.extra.deviceName === oldDeviceName);
    const oldRuntime = oldRuntimes.find(r => r.extra.deviceName === oldDeviceName);
    const isUnplugged = newRuntime && newRuntime.isUnplugged && !oldRuntime.isUnplugged;
    if (oldDeviceName && (!newRuntime || isUnplugged)) {
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
    if (runtimeAdded && !newRuntime.isUnplugged) {
      recordEvent("runtime_added", getRuntimeEventExtras(newRuntime));
    }
  }

  // Using device names as unique IDs is inaccurate. See Bug 1544582.
  const newDeviceNames = new Set(newRuntimes.map(r => r.extra.deviceName));
  for (const newDeviceName of newDeviceNames) {
    const newRuntime = newRuntimes.find(r => r.extra.deviceName === newDeviceName);
    const oldRuntime = oldRuntimes.find(r => r.extra.deviceName === newDeviceName);
    const isPlugged = oldRuntime && oldRuntime.isUnplugged && !newRuntime.isUnplugged;

    if (newDeviceName && (!oldRuntime || isPlugged)) {
      recordEvent("device_added", {
        "connection_type": action.runtimeType,
        "device_name": newDeviceName,
      });
    }
  }
}

function recordConnectionAttempt(connectionId, runtimeId, status, store) {
  const runtime = findRuntimeById(runtimeId, store.getState().runtimes);
  if (runtime.type === RUNTIMES.THIS_FIREFOX) {
    // Only record connection_attempt events for remote runtimes.
    return;
  }

  recordEvent("connection_attempt", {
    "connection_id": connectionId,
    "connection_type": runtime.type,
    "runtime_id": getTelemetryRuntimeId(runtimeId),
    "status": status,
  });
}

/**
 * This middleware will record events to telemetry for some specific actions.
 */
function eventRecordingMiddleware(store) {
  return next => action => {
    switch (action.type) {
      case CONNECT_RUNTIME_CANCEL:
        recordConnectionAttempt(action.connectionId, action.id, "cancelled", store);
        break;
      case CONNECT_RUNTIME_FAILURE:
        recordConnectionAttempt(action.connectionId, action.id, "failed", store);
        break;
      case CONNECT_RUNTIME_NOT_RESPONDING:
        recordConnectionAttempt(action.connectionId, action.id, "not responding", store);
        break;
      case CONNECT_RUNTIME_START:
        recordConnectionAttempt(action.connectionId, action.id, "start", store);
        break;
      case CONNECT_RUNTIME_SUCCESS:
        recordConnectionAttempt(action.connectionId, action.runtime.id, "success", store);
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
      case SHOW_PROFILER_DIALOG:
        recordEvent("show_profiler", {
          "runtime_id": getCurrentRuntimeIdForTelemetry(store),
        });
        break;
      case TELEMETRY_RECORD:
        const { method, details } = action;
        if (method) {
          recordEvent(method, details);
        } else {
          console.error(`[RECORD EVENT FAILED] ${action.type}: no "method" property`);
        }
        break;
      case UPDATE_CONNECTION_PROMPT_SETTING_SUCCESS:
        recordEvent("update_conn_prompt", {
          "prompt_enabled": `${action.connectionPromptEnabled}`,
          "runtime_id": getCurrentRuntimeIdForTelemetry(store),
        });
        break;
    }

    return next(action);
  };
}

module.exports = eventRecordingMiddleware;
