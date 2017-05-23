/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * A simple service to track source actors and keep a mapping between
 * original URLs and objects holding the source actor's ID (which is
 * used as a cookie by the devtools-source-map service) and the source
 * map URL.
 *
 * @param {object} target
 *        The object the toolbox is debugging.
 * @param {SourceMapService} sourceMapService
 *        The devtools-source-map functions
 */
function SourceMapURLService(target, sourceMapService) {
  this._target = target;
  this._sourceMapService = sourceMapService;
  this._urls = new Map();

  this._onSourceUpdated = this._onSourceUpdated.bind(this);
  this.reset = this.reset.bind(this);

  target.on("source-updated", this._onSourceUpdated);
  target.on("will-navigate", this.reset);
}

/**
 * Reset the service.  This flushes the internal cache.
 */
SourceMapURLService.prototype.reset = function () {
  this._sourceMapService.clearSourceMaps();
  this._urls.clear();
};

/**
 * Shut down the service, unregistering its event listeners and
 * flushing the cache.  After this call the service will no longer
 * function.
 */
SourceMapURLService.prototype.destroy = function () {
  this.reset();
  this._target.off("source-updated", this._onSourceUpdated);
  this._target.off("will-navigate", this.reset);
  this._target = this._urls = null;
};

/**
 * A helper function that is called when a new source is available.
 */
SourceMapURLService.prototype._onSourceUpdated = function (_, sourceEvent) {
  let { source } = sourceEvent;
  let { generatedUrl, url, actor: id, sourceMapURL } = source;

  // |generatedUrl| comes from the actor and is extracted from the
  // source code by SpiderMonkey.
  let seenUrl = generatedUrl || url;
  this._urls.set(seenUrl, { id, url: seenUrl, sourceMapURL });
};

/**
 * Look up the original position for a given location.  This returns a
 * promise resolving to either the original location, or null if the
 * given location is not source-mapped.  If a location is returned, it
 * is of the same form as devtools-source-map's |getOriginalLocation|.
 *
 * @param {String} url
 *        The URL to map.
 * @param {number} line
 *        The line number to map.
 * @param {number} column
 *        The column number to map.
 * @return Promise
 *        A promise resolving either to the original location, or null.
 */
SourceMapURLService.prototype.originalPositionFor = async function (url, line, column) {
  const urlInfo = this._urls.get(url);
  if (!urlInfo) {
    return null;
  }
  // Call getOriginalURLs to make sure the source map has been
  // fetched.  We don't actually need the result of this though.
  await this._sourceMapService.getOriginalURLs(urlInfo);
  const location = { sourceId: urlInfo.id, line, column, sourceUrl: url };
  let resolvedLocation = await this._sourceMapService.getOriginalLocation(location);
  if (!resolvedLocation ||
      (resolvedLocation.line === location.line &&
       resolvedLocation.column === location.column &&
       resolvedLocation.sourceUrl === location.sourceUrl)) {
    return null;
  }
  return resolvedLocation;
};

exports.SourceMapURLService = SourceMapURLService;
