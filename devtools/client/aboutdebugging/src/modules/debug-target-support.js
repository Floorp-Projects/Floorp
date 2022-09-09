/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  DEBUG_TARGET_PANE,
  PREFERENCES,
  RUNTIMES,
} = require("devtools/client/aboutdebugging/src/constants");

// Process target debugging is disabled by default.
function isProcessDebuggingSupported() {
  return Services.prefs.getBoolPref(
    PREFERENCES.PROCESS_DEBUGGING_ENABLED,
    false
  );
}

// Local tab debugging is disabled by default.
function isLocalTabDebuggingSupported() {
  return Services.prefs.getBoolPref(
    PREFERENCES.LOCAL_TAB_DEBUGGING_ENABLED,
    false
  );
}

// Local process debugging is disabled by default.
// This preference has no default value in
// devtools/client/preferences/devtools-client.js
// because it is only intended for tests.
function isLocalProcessDebuggingSupported() {
  return Services.prefs.getBoolPref(
    "devtools.aboutdebugging.test-local-process-debugging",
    false
  );
}

// Installing extensions can be disabled in enterprise policy.
// Note: Temporary Extensions are only supported when debugging This Firefox, so checking
// the local preference is acceptable here. If we enable Temporary extensions for remote
// runtimes, we should retrieve the preference from the target runtime instead.
function isTemporaryExtensionSupported() {
  return Services.prefs.getBoolPref(PREFERENCES.XPINSTALL_ENABLED, true);
}

const ALL_DEBUG_TARGET_PANES = [
  DEBUG_TARGET_PANE.INSTALLED_EXTENSION,
  ...(isProcessDebuggingSupported() ? [DEBUG_TARGET_PANE.PROCESSES] : []),
  DEBUG_TARGET_PANE.OTHER_WORKER,
  DEBUG_TARGET_PANE.SERVICE_WORKER,
  DEBUG_TARGET_PANE.SHARED_WORKER,
  DEBUG_TARGET_PANE.TAB,
  ...(isTemporaryExtensionSupported()
    ? [DEBUG_TARGET_PANE.TEMPORARY_EXTENSION]
    : []),
];

// All debug target panes (to filter out if any of the panels should be excluded).
const REMOTE_DEBUG_TARGET_PANES = [...ALL_DEBUG_TARGET_PANES];

const THIS_FIREFOX_DEBUG_TARGET_PANES = ALL_DEBUG_TARGET_PANES
  // Main process debugging is not available for This Firefox.
  // At the moment only the main process is listed under processes, so remove the category
  // for this runtime.
  .filter(
    p => p !== DEBUG_TARGET_PANE.PROCESSES || isLocalProcessDebuggingSupported()
  )
  // Showing tab targets for This Firefox is behind a preference.
  .filter(p => p !== DEBUG_TARGET_PANE.TAB || isLocalTabDebuggingSupported());

const SUPPORTED_TARGET_PANE_BY_RUNTIME = {
  [RUNTIMES.THIS_FIREFOX]: THIS_FIREFOX_DEBUG_TARGET_PANES,
  [RUNTIMES.USB]: REMOTE_DEBUG_TARGET_PANES,
  [RUNTIMES.NETWORK]: REMOTE_DEBUG_TARGET_PANES,
};

/**
 * A debug target pane is more specialized than a debug target. For instance EXTENSION is
 * a DEBUG_TARGET but INSTALLED_EXTENSION and TEMPORARY_EXTENSION are DEBUG_TARGET_PANES.
 */
function isSupportedDebugTargetPane(runtimeType, debugTargetPaneKey) {
  return SUPPORTED_TARGET_PANE_BY_RUNTIME[runtimeType].includes(
    debugTargetPaneKey
  );
}
exports.isSupportedDebugTargetPane = isSupportedDebugTargetPane;

/**
 * Check if the given runtimeType supports temporary extension installation
 * from about:debugging (currently disallowed on non-local runtimes).
 */
function supportsTemporaryExtensionInstaller(runtimeType) {
  return runtimeType === RUNTIMES.THIS_FIREFOX;
}
exports.supportsTemporaryExtensionInstaller = supportsTemporaryExtensionInstaller;

/**
 * Check if the given runtimeType supports temporary extension additional
 * actions (e.g. reload and remove, which are currently disallowed on
 * non-local runtimes).
 */
function supportsTemporaryExtensionAdditionalActions(runtimeType) {
  return runtimeType === RUNTIMES.THIS_FIREFOX;
}
exports.supportsTemporaryExtensionAdditionalActions = supportsTemporaryExtensionAdditionalActions;
