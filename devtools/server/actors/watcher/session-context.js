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
// These objects have only two attributes used by all the Session contexts:
// - type: String
//   Describes which type of context we are debugging.
//   Can be "all", "browser-element" or "webextension".
//   See each create* method for more info about each type and their specific attributes.
// - isServerTargetSwitchingEnabled: Boolean
//   If true, targets should all be spawned by the server codebase.
//   Especially the first, top level target.

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
  return {
    type: "all",
    // For now, the top level target (ParentProcessTargetActor) is created via ProcessDescriptor.getTarget
    // and is never replaced by any other, nor is it created by the WatcherActor.
    isServerTargetSwitchingEnabled: false,
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
  return {
    type: "browser-element",
    browserId: browserElement.browserId,
    // Nowaday, it should always be enabled except for WebExtension special
    // codepath and some tests.
    isServerTargetSwitchingEnabled: config.isServerTargetSwitchingEnabled,
    // Should we instantiate targets for popups opened in distinct tabs/windows?
    // Driven by devtools.popups.debug=true preference.
    isPopupDebuggingEnabled: config.isPopupDebuggingEnabled,
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
  return {
    type: "webextension",
    addonId: addonId,
    addonBrowsingContextID: browsingContextID,
    addonInnerWindowId: innerWindowId,
    // For now, there is only one target (WebExtensionTargetActor), it is never replaced,
    // and is only created via WebExtensionDescriptor.getTarget (and never by the watcher actor).
    isServerTargetSwitchingEnabled: config.isServerTargetSwitchingEnabled,
  };
}

module.exports = {
  createBrowserSessionContext,
  createBrowserElementSessionContext,
  createWebExtensionSessionContext,
};
