/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Ci } = require("chrome");
const Services = require("Services");

var globalsCache = {};

exports.addContentGlobal = function (options) {
  if (!options || !options.global || !options["inner-window-id"]) {
    throw Error("Invalid arguments");
  }
  let cache = getGlobalCache(options["inner-window-id"]);
  cache.push(options.global);
  return undefined;
};

exports.getContentGlobals = function (options) {
  if (!options || !options["inner-window-id"]) {
    throw Error("Invalid arguments");
  }
  return Array.slice(globalsCache[options["inner-window-id"]] || []);
};

exports.removeContentGlobal = function (options) {
  if (!options || !options.global || !options["inner-window-id"]) {
    throw Error("Invalid arguments");
  }
  let cache = getGlobalCache(options["inner-window-id"]);
  let index = cache.indexOf(options.global);
  cache.splice(index, 1);
  return undefined;
};

function getGlobalCache(innerWindowID) {
  globalsCache[innerWindowID] = globalsCache[innerWindowID] || [];
  return globalsCache[innerWindowID];
}

// when the window is destroyed, eliminate the associated globals cache
if (!isWorker) {
  Services.obs.addObserver(function observer(subject, topic, data) {
    let id = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
    delete globalsCache[id];
  }, "inner-window-destroyed");
}
