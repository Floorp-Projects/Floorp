/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Task } = require("devtools/shared/task");
const EventEmitter = require("devtools/shared/event-emitter");
const { LocationStore, serialize, deserialize } = require("./location-store");

/**
 * A manager class that wraps a TabTarget and listens to source changes
 * from source maps and resolves non-source mapped locations to the source mapped
 * versions and back and forth, and creating smart elements with a location that
 * auto-update when the source changes (from pretty printing, source maps loading, etc)
 *
 * @param {TabTarget} target
 */

function SourceMapService(target) {
  this._target = target;
  this._locationStore = new LocationStore();
  this._isNotSourceMapped = new Map();

  EventEmitter.decorate(this);

  this._onSourceUpdated = this._onSourceUpdated.bind(this);
  this._resolveLocation = this._resolveLocation.bind(this);
  this._resolveAndUpdate = this._resolveAndUpdate.bind(this);
  this.subscribe = this.subscribe.bind(this);
  this.unsubscribe = this.unsubscribe.bind(this);
  this.reset = this.reset.bind(this);
  this.destroy = this.destroy.bind(this);

  target.on("source-updated", this._onSourceUpdated);
  target.on("navigate", this.reset);
  target.on("will-navigate", this.reset);
}

/**
 * Clears the store containing the cached promised locations
 */
SourceMapService.prototype.reset = function () {
  // Guard to prevent clearing the store when it is not initialized yet.
  if (!this._locationStore) {
    return;
  }
  this._locationStore.clear();
  this._isNotSourceMapped.clear();
};

SourceMapService.prototype.destroy = function () {
  this.reset();
  this._target.off("source-updated", this._onSourceUpdated);
  this._target.off("navigate", this.reset);
  this._target.off("will-navigate", this.reset);
  this._target.off("close", this.destroy);
  this._target = this._locationStore = this._isNotSourceMapped = null;
};

/**
 * Sets up listener for the callback to update the FrameView
 * and tries to resolve location, if it is source-mappable
 * @param location
 * @param callback
 */
SourceMapService.prototype.subscribe = function (location, callback) {
  // A valid candidate location for source-mapping should have a url and line.
  // Abort if there's no `url`, which means it's unsourcemappable anyway,
  // like an eval script.
  // From previous attempts to source-map locations, we also determine if a location
  // is not source-mapped.
  if (!location.url || !location.line || this._isNotSourceMapped.get(location.url)) {
    return;
  }
  this.on(serialize(location), callback);
  this._locationStore.set(location);
  this._resolveAndUpdate(location);
};

/**
 * Removes the listener for the location and clears cached locations
 * @param location
 * @param callback
 */
SourceMapService.prototype.unsubscribe = function (location, callback) {
  this.off(serialize(location), callback);
  // Check to see if the store exists before attempting to clear a location
  // Sometimes un-subscribe happens during the destruction cascades and this
  // condition is to protect against that. Could be looked into in the future.
  if (!this._locationStore) {
    return;
  }
  this._locationStore.clearByURL(location.url);
};

/**
 * Tries to resolve the location and if successful,
 * emits the resolved location
 * @param location
 * @private
 */
SourceMapService.prototype._resolveAndUpdate = function (location) {
  this._resolveLocation(location).then(resolvedLocation => {
    // We try to source map the first console log to initiate the source-updated
    // event from target. The isSameLocation check is to make sure we don't update
    // the frame, if the location is not source-mapped.
    if (resolvedLocation && !isSameLocation(location, resolvedLocation)) {
      this.emit(serialize(location), location, resolvedLocation);
    }
  });
};

/**
 * Checks if there is existing promise to resolve location, if so returns cached promise
 * if not, tries to resolve location and returns a promised location
 * @param location
 * @return Promise<Object>
 * @private
 */
SourceMapService.prototype._resolveLocation = Task.async(function* (location) {
  let resolvedLocation;
  const cachedLocation = this._locationStore.get(location);
  if (cachedLocation) {
    resolvedLocation = cachedLocation;
  } else {
    const promisedLocation = resolveLocation(this._target, location);
    if (promisedLocation) {
      this._locationStore.set(location, promisedLocation);
      resolvedLocation = promisedLocation;
    }
  }
  return resolvedLocation;
});

/**
 * Checks if the `source-updated` event is fired from the target.
 * Checks to see if location store has the source url in its cache,
 * if so, tries to update each stale location in the store.
 * Determines if the source should be source-mapped or not.
 * @param _
 * @param sourceEvent
 * @private
 */
SourceMapService.prototype._onSourceUpdated = function (_, sourceEvent) {
  let { type, source } = sourceEvent;

  // If we get a new source, and it's not a source map, abort;
  // we can have no actionable updates as this is just a new normal source.
  // Check Source Actor for sourceMapURL property (after Firefox 48)
  // If not present, utilize isSourceMapped and isPrettyPrinted properties
  // to estimate if a source is not source-mapped.
  const isNotSourceMapped = !(source.sourceMapURL ||
    source.isSourceMapped || source.isPrettyPrinted);
  if (type === "newSource" && isNotSourceMapped) {
    this._isNotSourceMapped.set(source.url, true);
    return;
  }
  let sourceUrl = null;
  if (source.generatedUrl && source.isSourceMapped) {
    sourceUrl = source.generatedUrl;
  } else if (source.url && source.isPrettyPrinted) {
    sourceUrl = source.url;
  }
  const locationsToResolve = this._locationStore.getByURL(sourceUrl);
  if (locationsToResolve.length) {
    this._locationStore.clearByURL(sourceUrl);
    for (let location of locationsToResolve) {
      this._resolveAndUpdate(deserialize(location));
    }
  }
};

exports.SourceMapService = SourceMapService;

/**
 * Take a TabTarget and a location, containing a `url`, `line`, and `column`, resolve
 * the location to the latest location (so a source mapped location, or if pretty print
 * status has been updated)
 *
 * @param {TabTarget} target
 * @param {Object} location
 * @return {Promise<Object>}
 */
function resolveLocation(target, location) {
  return Task.spawn(function* () {
    let newLocation = yield target.resolveLocation({
      url: location.url,
      line: location.line,
      column: location.column || Infinity
    });
    // Source or mapping not found, so don't do anything
    if (newLocation.error) {
      return null;
    }
    return newLocation;
  });
}

/**
 * Returns true if the original location and resolved location are the same
 * @param location
 * @param resolvedLocation
 * @returns {boolean}
 */
function isSameLocation(location, resolvedLocation) {
  return location.url === resolvedLocation.url &&
    location.line === resolvedLocation.line &&
    location.column === resolvedLocation.column;
}
