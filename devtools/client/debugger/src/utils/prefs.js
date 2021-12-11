/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { PrefsHelper } = require("devtools/client/shared/prefs");

import { isNode } from "./environment";
import Services from "devtools-services";

// Schema version to bump when the async store format has changed incompatibly
// and old stores should be cleared.
const prefsSchemaVersion = 11;
const { pref } = Services;

if (isNode()) {
  pref("devtools.browsertoolbox.fission", false);
  pref("devtools.debugger.logging", false);
  pref("devtools.debugger.alphabetize-outline", false);
  pref("devtools.debugger.auto-pretty-print", false);
  pref("devtools.source-map.client-service.enabled", true);
  pref("devtools.chrome.enabled", false);
  pref("devtools.debugger.pause-on-exceptions", false);
  pref("devtools.debugger.pause-on-caught-exceptions", false);
  pref("devtools.debugger.ignore-caught-exceptions", true);
  pref("devtools.debugger.call-stack-visible", true);
  pref("devtools.debugger.scopes-visible", true);
  pref("devtools.debugger.component-visible", false);
  pref("devtools.debugger.workers-visible", false);
  pref("devtools.debugger.expressions-visible", false);
  pref("devtools.debugger.xhr-breakpoints-visible", false);
  pref("devtools.debugger.breakpoints-visible", true);
  pref("devtools.debugger.event-listeners-visible", false);
  pref("devtools.debugger.dom-mutation-breakpoints-visible", false);
  pref("devtools.debugger.start-panel-collapsed", false);
  pref("devtools.debugger.end-panel-collapsed", false);
  pref("devtools.debugger.start-panel-size", 300);
  pref("devtools.debugger.end-panel-size", 300);
  pref("devtools.debugger.tabsBlackBoxed", "[]");
  pref("devtools.debugger.ui.editor-wrapping", false);
  pref("devtools.debugger.ui.framework-grouping-on", true);
  pref("devtools.debugger.pending-selected-location", "{}");
  pref("devtools.debugger.expressions", "[]");
  pref("devtools.debugger.file-search-case-sensitive", false);
  pref("devtools.debugger.file-search-whole-word", false);
  pref("devtools.debugger.file-search-regex-match", false);
  pref("devtools.debugger.project-directory-root", "");
  pref("devtools.debugger.map-scopes-enabled", false);
  pref("devtools.debugger.prefs-schema-version", prefsSchemaVersion);
  pref("devtools.debugger.skip-pausing", false);
  pref("devtools.debugger.log-actions", true);
  pref("devtools.debugger.log-event-breakpoints", false);
  pref("devtools.debugger.features.workers", true);
  pref("devtools.debugger.features.async-stepping", false);
  pref("devtools.debugger.features.wasm", true);
  pref("devtools.debugger.features.shortcuts", true);
  pref("devtools.debugger.features.root", true);
  pref("devtools.debugger.features.map-scopes", true);
  pref("devtools.debugger.features.remove-command-bar-options", true);
  pref("devtools.debugger.features.code-folding", false);
  pref("devtools.debugger.features.command-click", false);
  pref("devtools.debugger.features.outline", true);
  pref("devtools.debugger.features.column-breakpoints", true);
  pref("devtools.debugger.features.skip-pausing", true);
  pref("devtools.debugger.features.component-pane", false);
  pref("devtools.debugger.features.autocomplete-expressions", false);
  pref("devtools.debugger.features.map-expression-bindings", true);
  pref("devtools.debugger.features.map-await-expression", true);
  pref("devtools.debugger.features.xhr-breakpoints", true);
  pref("devtools.debugger.features.original-blackbox", true);
  pref("devtools.debugger.features.event-listeners-breakpoints", true);
  pref("devtools.debugger.features.dom-mutation-breakpoints", true);
  pref("devtools.debugger.features.log-points", true);
  pref("devtools.debugger.features.inline-preview", true);
  pref("devtools.debugger.features.overlay-step-buttons", true);
  pref("devtools.debugger.features.frame-step", true);
  pref("devtools.editor.tabsize", 2);
}

export const prefs = new PrefsHelper("devtools", {
  fission: ["Bool", "browsertoolbox.fission"],
  logging: ["Bool", "debugger.logging"],
  editorWrapping: ["Bool", "debugger.ui.editor-wrapping"],
  alphabetizeOutline: ["Bool", "debugger.alphabetize-outline"],
  autoPrettyPrint: ["Bool", "debugger.auto-pretty-print"],
  clientSourceMapsEnabled: ["Bool", "source-map.client-service.enabled"],
  chromeAndExtensionsEnabled: ["Bool", "chrome.enabled"],
  pauseOnExceptions: ["Bool", "debugger.pause-on-exceptions"],
  pauseOnCaughtExceptions: ["Bool", "debugger.pause-on-caught-exceptions"],
  ignoreCaughtExceptions: ["Bool", "debugger.ignore-caught-exceptions"],
  callStackVisible: ["Bool", "debugger.call-stack-visible"],
  scopesVisible: ["Bool", "debugger.scopes-visible"],
  componentVisible: ["Bool", "debugger.component-visible"],
  workersVisible: ["Bool", "debugger.workers-visible"],
  breakpointsVisible: ["Bool", "debugger.breakpoints-visible"],
  expressionsVisible: ["Bool", "debugger.expressions-visible"],
  xhrBreakpointsVisible: ["Bool", "debugger.xhr-breakpoints-visible"],
  eventListenersVisible: ["Bool", "debugger.event-listeners-visible"],
  domMutationBreakpointsVisible: [
    "Bool",
    "debugger.dom-mutation-breakpoints-visible",
  ],
  startPanelCollapsed: ["Bool", "debugger.start-panel-collapsed"],
  endPanelCollapsed: ["Bool", "debugger.end-panel-collapsed"],
  startPanelSize: ["Int", "debugger.start-panel-size"],
  endPanelSize: ["Int", "debugger.end-panel-size"],
  frameworkGroupingOn: ["Bool", "debugger.ui.framework-grouping-on"],
  pendingSelectedLocation: ["Json", "debugger.pending-selected-location", {}],
  expressions: ["Json", "debugger.expressions", []],
  fileSearchCaseSensitive: ["Bool", "debugger.file-search-case-sensitive"],
  fileSearchWholeWord: ["Bool", "debugger.file-search-whole-word"],
  fileSearchRegexMatch: ["Bool", "debugger.file-search-regex-match"],
  debuggerPrefsSchemaVersion: ["Int", "debugger.prefs-schema-version"],
  projectDirectoryRoot: ["Char", "debugger.project-directory-root", ""],
  projectDirectoryRootName: [
    "Char",
    "debugger.project-directory-root-name",
    "",
  ],
  skipPausing: ["Bool", "debugger.skip-pausing"],
  mapScopes: ["Bool", "debugger.map-scopes-enabled"],
  logActions: ["Bool", "debugger.log-actions"],
  logEventBreakpoints: ["Bool", "debugger.log-event-breakpoints"],
  indentSize: ["Int", "editor.tabsize"],
});

// The pref may not be defined. Defaulting to null isn't viable (cursor never blinks).
// Can't use CodeMirror.defaults here because it's loaded later.
// Hardcode the fallback value to that of CodeMirror.defaults.cursorBlinkRate.
prefs.cursorBlinkRate = Services.prefs.getIntPref("ui.caretBlinkTime", 530);

export const features = new PrefsHelper("devtools.debugger.features", {
  asyncStepping: ["Bool", "async-stepping"],
  wasm: ["Bool", "wasm"],
  shortcuts: ["Bool", "shortcuts"],
  root: ["Bool", "root"],
  columnBreakpoints: ["Bool", "column-breakpoints"],
  mapScopes: ["Bool", "map-scopes"],
  removeCommandBarOptions: ["Bool", "remove-command-bar-options"],
  workers: ["Bool", "workers"],
  windowlessWorkers: ["Bool", "windowless-workers"],
  outline: ["Bool", "outline"],
  codeFolding: ["Bool", "code-folding"],
  skipPausing: ["Bool", "skip-pausing"],
  autocompleteExpression: ["Bool", "autocomplete-expressions"],
  mapExpressionBindings: ["Bool", "map-expression-bindings"],
  mapAwaitExpression: ["Bool", "map-await-expression"],
  componentPane: ["Bool", "component-pane"],
  xhrBreakpoints: ["Bool", "xhr-breakpoints"],
  originalBlackbox: ["Bool", "original-blackbox"],
  eventListenersBreakpoints: ["Bool", "event-listeners-breakpoints"],
  domMutationBreakpoints: ["Bool", "dom-mutation-breakpoints"],
  logPoints: ["Bool", "log-points"],
  commandClick: ["Bool", "command-click"],
  showOverlay: ["Bool", "overlay"],
  inlinePreview: ["Bool", "inline-preview"],
  windowlessServiceWorkers: ["Bool", "windowless-service-workers"],
  frameStep: ["Bool", "frame-step"],
});

// Import the asyncStore already spawned by the TargetMixin class
const ThreadUtils = require("devtools/client/shared/thread-utils");
export const asyncStore = ThreadUtils.asyncStore;

export function resetSchemaVersion() {
  prefs.debuggerPrefsSchemaVersion = prefsSchemaVersion;
}

export function verifyPrefSchema() {
  if (prefs.debuggerPrefsSchemaVersion < prefsSchemaVersion) {
    asyncStore.pendingBreakpoints = {};
    asyncStore.tabs = [];
    asyncStore.xhrBreakpoints = [];
    asyncStore.eventListenerBreakpoints = undefined;
    prefs.debuggerPrefsSchemaVersion = prefsSchemaVersion;
  }
}
