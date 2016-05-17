/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Task } = require("resource://gre/modules/Task.jsm");
const { assert } = require("devtools/shared/DevToolsUtils");

/**
 * A manager class that wraps a TabTarget and listens to source changes
 * from source maps and resolves non-source mapped locations to the source mapped
 * versions and back and forth, and creating smart elements with a location that
 * auto-update when the source changes (from pretty printing, source maps loading, etc)
 *
 * @param {TabTarget} target
 */
function SourceLocationController(target) {
  this.target = target;
  this.locations = new Set();

  this._onSourceUpdated = this._onSourceUpdated.bind(this);
  this.reset = this.reset.bind(this);
  this.destroy = this.destroy.bind(this);

  target.on("source-updated", this._onSourceUpdated);
  target.on("navigate", this.reset);
  target.on("will-navigate", this.reset);
  target.on("close", this.destroy);
}

SourceLocationController.prototype.reset = function () {
  this.locations.clear();
};

SourceLocationController.prototype.destroy = function () {
  this.locations.clear();
  this.target.off("source-updated", this._onSourceUpdated);
  this.target.off("navigate", this.reset);
  this.target.off("will-navigate", this.reset);
  this.target.off("close", this.destroy);
  this.target = this.locations = null;
};

/**
 * Add this `location` to be observed and register a callback
 * whenever the underlying source is updated.
 *
 * @param {Object} location
 *        An object with a {String} url, {Number} line, and optionally
 *        a {Number} column.
 * @param {Function} callback
 */
SourceLocationController.prototype.bindLocation = function (location, callback) {
  assert(location.url, "Location must have a url.");
  assert(location.line, "Location must have a line.");
  this.locations.add({ location, callback });
};

/**
 * Called when a new source occurs (a normal source, source maps) or an updated
 * source (pretty print) occurs.
 *
 * @param {String} eventName
 * @param {Object} sourceEvent
 */
SourceLocationController.prototype._onSourceUpdated = function (_, sourceEvent) {
  let { type, source } = sourceEvent;
  // If we get a new source, and it's not a source map, abort;
  // we can ahve no actionable updates as this is just a new normal source.
  // Also abort if there's no `url`, which means it's unsourcemappable anyway,
  // like an eval script.
  if (!source.url || type === "newSource" && !source.isSourceMapped) {
    return;
  }

  for (let locationItem of this.locations) {
    if (isSourceRelated(locationItem.location, source)) {
      this._updateSource(locationItem);
    }
  }
};

SourceLocationController.prototype._updateSource = Task.async(function* (locationItem) {
  let newLocation = yield resolveLocation(this.target, locationItem.location);
  if (newLocation) {
    let previousLocation = Object.assign({}, locationItem.location);
    Object.assign(locationItem.location, newLocation);
    locationItem.callback(previousLocation, newLocation);
  }
});

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
 * Takes a serialized SourceActor form and returns a boolean indicating
 * if this source is related to this location, like if a location is a generated source,
 * and the source map is loaded subsequently, the new source mapped SourceActor
 * will be considered related to this location. Same with pretty printing new sources.
 *
 * @param {Object} location
 * @param {Object} source
 * @return {Boolean}
 */
function isSourceRelated(location, source) {
         // Mapping location to subsequently loaded source map
  return source.generatedUrl === location.url ||
         // Mapping source map loc to source map
         source.url === location.url;
}

exports.SourceLocationController = SourceLocationController;
exports.resolveLocation = resolveLocation;
exports.isSourceRelated = isSourceRelated;
