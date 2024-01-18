/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Module to create all the Session Context objects.
//
// These are static JSON serializable object that help describe
// the debugged context. It is passed around to most of the server codebase
// in order to know which object to consider inspecting and communicating back to the client.
//
// These objects are all instantiated by the Descriptor actors
// and passed as a constructor argument to the Watcher actor.
//
// These objects have attributes used by all the Session contexts:
// - type: String
//   Describes which type of context we are debugging.
//   See SESSION_TYPES for all possible values.
//   See each create* method for more info about each type and their specific attributes.
// - isServerTargetSwitchingEnabled: Boolean
//   If true, targets should all be spawned by the server codebase.
//   Especially the first, top level target.
// - supportedTargets: Boolean
//   An object keyed by target type, whose value indicates if we have watcher support
//   for the target.
// - supportedResources: Boolean
//   An object keyed by resource type, whose value indicates if we have watcher support
//   for the resource.

const Targets = require("resource://devtools/server/actors/targets/index.js");
const Resources = require("resource://devtools/server/actors/resources/index.js");

const SESSION_TYPES = {
  ALL: "all",
  BROWSER_ELEMENT: "browser-element",
  CONTENT_PROCESS: "content-process",
  WEBEXTENSION: "webextension",
  WORKER: "worker",
};

/**
 * Create the SessionContext used by the Browser Toolbox and Browser Console.
 *
 * This context means debugging everything.
 * The whole browser:
 * - all processes: parent and content,
 * - all privileges: privileged/chrome and content/web,
 * - all components/targets: HTML documents, processes, workers, add-ons,...
 */
function createBrowserSessionContext() {
  const type = SESSION_TYPES.ALL;

  return {
    type,
    // For now, the top level target (ParentProcessTargetActor) is created via ProcessDescriptor.getTarget
    // and is never replaced by any other, nor is it created by the WatcherActor.
    isServerTargetSwitchingEnabled: false,
    supportedTargets: getWatcherSupportedTargets(type),
    supportedResources: getWatcherSupportedResources(type),
  };
}

/**
 * Create the SessionContext used by the regular web page toolboxes as well as remote debugging android device tabs.
 *
 * @param {BrowserElement} browserElement
 *        The tab to debug. It should be a reference to a <browser> element.
 * @param {Object} config
 *        An object with optional configuration. Only supports "isServerTargetSwitchingEnabled" attribute.
 *        See jsdoc in this file header for more info.
 */
function createBrowserElementSessionContext(browserElement, config) {
  const type = SESSION_TYPES.BROWSER_ELEMENT;
  return {
    type,
    browserId: browserElement.browserId,
    // Nowaday, it should always be enabled except for WebExtension special
    // codepath and some tests.
    isServerTargetSwitchingEnabled: config.isServerTargetSwitchingEnabled,
    // Should we instantiate targets for popups opened in distinct tabs/windows?
    // Driven by devtools.popups.debug=true preference.
    isPopupDebuggingEnabled: config.isPopupDebuggingEnabled,
    supportedTargets: getWatcherSupportedTargets(type),
    supportedResources: getWatcherSupportedResources(type),
  };
}

/**
 * Create the SessionContext used by the web extension toolboxes.
 *
 * @param {Object} addon
 *        First object argument to describe the add-on.
 * @param {String} addon.addonId
 *        The web extension ID, to uniquely identify the debugged add-on.
 * @param {String} addon.browsingContextID
 *        The ID of the BrowsingContext into which this add-on is loaded.
 *        For now the top level target is associated with this one precise BrowsingContext.
 *        Knowing about it later helps associate resources to the same BrowsingContext ID and so the same target.
 * @param {String} addon.innerWindowId
 *        The ID of the WindowGlobal into which this add-on is loaded.
 *        This is used for the same reason as browsingContextID. It helps match the resource with the right target.
 *        We now also use the WindowGlobal ID/innerWindowId to identify the targets.
 * @param {Object} config
 *        An object with optional configuration. Only supports "isServerTargetSwitchingEnabled" attribute.
 *        See jsdoc in this file header for more info.
 */
function createWebExtensionSessionContext(
  { addonId, browsingContextID, innerWindowId },
  config
) {
  const type = SESSION_TYPES.WEBEXTENSION;
  return {
    type,
    addonId,
    addonBrowsingContextID: browsingContextID,
    addonInnerWindowId: innerWindowId,
    // For now, there is only one target (WebExtensionTargetActor), it is never replaced,
    // and is only created via WebExtensionDescriptor.getTarget (and never by the watcher actor).
    isServerTargetSwitchingEnabled: config.isServerTargetSwitchingEnabled,
    supportedTargets: getWatcherSupportedTargets(type),
    supportedResources: getWatcherSupportedResources(type),
  };
}

/**
 * Create the SessionContext used by the Browser Content Toolbox, to debug only one content process.
 * Or when debugging XpcShell via about:debugging, where we instantiate only one content process target.
 */
function createContentProcessSessionContext() {
  const type = SESSION_TYPES.CONTENT_PROCESS;
  return {
    type,
    supportedTargets: getWatcherSupportedTargets(type),
    supportedResources: getWatcherSupportedResources(type),
  };
}

/**
 * Create the SessionContext used when debugging one specific Service Worker or special chrome worker.
 * This is only used from about:debugging.
 */
function createWorkerSessionContext() {
  const type = SESSION_TYPES.WORKER;
  return {
    type,
    supportedTargets: getWatcherSupportedTargets(type),
    supportedResources: getWatcherSupportedResources(type),
  };
}

/**
 * Get the supported targets by the watcher given a session context type.
 *
 * @param {String} type
 * @returns {Object}
 */
function getWatcherSupportedTargets(type) {
  return {
    [Targets.TYPES.FRAME]: true,
    [Targets.TYPES.PROCESS]: true,
    [Targets.TYPES.WORKER]:
      type == SESSION_TYPES.BROWSER_ELEMENT ||
      type == SESSION_TYPES.WEBEXTENSION,
    [Targets.TYPES.SERVICE_WORKER]: type == SESSION_TYPES.BROWSER_ELEMENT,
  };
}

/**
 * Get the supported resources by the watcher given a session context type.
 *
 * @param {String} type
 * @returns {Object}
 */
function getWatcherSupportedResources(type) {
  // All resources types are supported for tab debugging and web extensions.
  // Some watcher classes are still disabled for the Multiprocess Browser Toolbox (type=SESSION_TYPES.ALL).
  // And they may also be disabled for workers once we start supporting them by the watcher.
  // So set the traits to false for all the resources that we don't support yet
  // and keep using the legacy listeners.
  const isTabOrWebExtensionToolbox =
    type == SESSION_TYPES.BROWSER_ELEMENT || type == SESSION_TYPES.WEBEXTENSION;

  return {
    [Resources.TYPES.CONSOLE_MESSAGE]: true,
    [Resources.TYPES.CSS_CHANGE]: isTabOrWebExtensionToolbox,
    [Resources.TYPES.CSS_MESSAGE]: true,
    [Resources.TYPES.DOCUMENT_EVENT]: true,
    [Resources.TYPES.CACHE_STORAGE]: true,
    [Resources.TYPES.COOKIE]: true,
    [Resources.TYPES.ERROR_MESSAGE]: true,
    [Resources.TYPES.EXTENSION_STORAGE]: true,
    [Resources.TYPES.INDEXED_DB]: true,
    [Resources.TYPES.LOCAL_STORAGE]: true,
    [Resources.TYPES.SESSION_STORAGE]: true,
    [Resources.TYPES.PLATFORM_MESSAGE]: true,
    [Resources.TYPES.NETWORK_EVENT]: true,
    [Resources.TYPES.NETWORK_EVENT_STACKTRACE]: true,
    [Resources.TYPES.REFLOW]: true,
    [Resources.TYPES.STYLESHEET]: true,
    [Resources.TYPES.SOURCE]: true,
    [Resources.TYPES.THREAD_STATE]: true,
    [Resources.TYPES.SERVER_SENT_EVENT]: true,
    [Resources.TYPES.WEBSOCKET]: true,
    [Resources.TYPES.JSTRACER_TRACE]: true,
    [Resources.TYPES.JSTRACER_STATE]: true,
    [Resources.TYPES.LAST_PRIVATE_CONTEXT_EXIT]: true,
  };
}

module.exports = {
  createBrowserSessionContext,
  createBrowserElementSessionContext,
  createWebExtensionSessionContext,
  createContentProcessSessionContext,
  createWorkerSessionContext,
  SESSION_TYPES,
};
