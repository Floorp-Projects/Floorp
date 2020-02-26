/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
"use strict";

/**
 * This file is for the new performance panel that targets profiler.firefox.com,
 * not the default-enabled DevTools performance panel.
 */

/**
 * @typedef {import("../../client/performance-new/@types/perf").GetActiveBrowsingContextID} GetActiveBrowsingContextID
 * @typedef {import("../../client/performance-new/@types/perf").PresetDefinitions} PresetDefinitions
 */

/**
 * TS-TODO
 *
 * This function replaces lazyRequireGetter, and TypeScript can understand it. It's
 * currently duplicated until we have consensus that TypeScript is a good idea.
 *
 * @template T
 * @type {(callback: () => T) => () => T}
 */
function requireLazy(callback) {
  /** @type {T | undefined} */
  let cache;
  return () => {
    if (cache === undefined) {
      cache = callback();
    }
    return cache;
  };
}

const lazyServices = requireLazy(() =>
  require("resource://gre/modules/Services.jsm")
);

/**
 * Gets the ID of active BrowsingContext from the browser.
 *
 * @type {GetActiveBrowsingContextID}
 */
function getActiveBrowsingContextID() {
  const { Services } = lazyServices();
  const win = Services.wm.getMostRecentWindow("navigator:browser");

  if (
    win &&
    win.gBrowser &&
    win.gBrowser.selectedBrowser &&
    win.gBrowser.selectedBrowser.browsingContext &&
    win.gBrowser.selectedBrowser.browsingContext.id
  ) {
    return win.gBrowser.selectedBrowser.browsingContext.id;
  }

  console.error(
    "Failed to get the active BrowsingContext ID while starting the profiler."
  );
  // `0` mean that we failed to ge the active BrowsingContext ID, and it's
  // treated as null value in the platform.
  return 0;
}

/** @type {PresetDefinitions} */
const presets = {
  "web-developer": {
    label: "Web Developer",
    description:
      "Recommended preset for most web app debuggging, with low overhead.",
    entries: 10000000,
    interval: 1,
    features: ["js"],
    threads: ["GeckoMain", "Compositor", "Renderer", "DOM Worker"],
    duration: 0,
  },
  "firefox-platform": {
    label: "Firefox Platform",
    description: "Recommended preset for internal Firefox platform debugging.",
    entries: 10000000,
    interval: 1,
    features: ["js", "leaf", "stackwalk"],
    threads: ["GeckoMain", "Compositor", "Renderer"],
    duration: 0,
  },
  "firefox-front-end": {
    label: "Firefox Front-End",
    description: "Recommended preset for internal Firefox front-end debugging.",
    entries: 10000000,
    interval: 1,
    features: ["js", "leaf", "stackwalk"],
    threads: ["GeckoMain", "Compositor", "Renderer", "DOM Worker"],
    duration: 0,
  },
};

module.exports = {
  getActiveBrowsingContextID,
  presets,
};
