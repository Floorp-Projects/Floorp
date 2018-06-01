/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Services = require("Services");
const SOURCE_MAP_PREF = "devtools.source-map.client-service.enabled";

/**
 * A simple service to track source actors and keep a mapping between
 * original URLs and objects holding the source or style actor's ID
 * (which is used as a cookie by the devtools-source-map service) and
 * the source map URL.
 *
 * @param {object} toolbox
 *        The toolbox.
 * @param {SourceMapService} sourceMapService
 *        The devtools-source-map functions
 */
function SourceMapURLService(toolbox, sourceMapService) {
  this._toolbox = toolbox;
  this._target = toolbox.target;
  this._sourceMapService = sourceMapService;
  // Map from content URLs to descriptors.  Descriptors are later
  // passed to the source map worker.
  this._urls = new Map();
  // Map from (stringified) locations to callbacks that are called
  // when the service decides a location should change (say, a source
  // map is available or the user changes the pref).
  this._subscriptions = new Map();
  // A backward map from actor IDs to the original URL.  This is used
  // to support pretty-printing.
  this._idMap = new Map();

  this._onSourceUpdated = this._onSourceUpdated.bind(this);
  this.reset = this.reset.bind(this);
  this._prefValue = Services.prefs.getBoolPref(SOURCE_MAP_PREF);
  this._onPrefChanged = this._onPrefChanged.bind(this);
  this._onNewStyleSheet = this._onNewStyleSheet.bind(this);

  this._target.on("source-updated", this._onSourceUpdated);
  this._target.on("will-navigate", this.reset);

  Services.prefs.addObserver(SOURCE_MAP_PREF, this._onPrefChanged);

  this._stylesheetsFront = null;
  this._loadingPromise = null;
}

/**
 * Lazy initialization.  Returns a promise that will resolve when all
 * the relevant URLs have been registered.
 */
SourceMapURLService.prototype._getLoadingPromise = function() {
  if (!this._loadingPromise) {
    let styleSheetsLoadingPromise = null;
    this._stylesheetsFront = this._toolbox.initStyleSheetsFront();
    if (this._stylesheetsFront) {
      this._stylesheetsFront.on("stylesheet-added", this._onNewStyleSheet);
      styleSheetsLoadingPromise =
          this._stylesheetsFront.getStyleSheets().then(sheets => {
            sheets.forEach(this._onNewStyleSheet);
          }, () => {
            // Ignore any protocol-based errors.
          });
    }

    // Start fetching the sources now.
    const loadingPromise = this._toolbox.threadClient.getSources().then(({sources}) => {
      // Ignore errors.  Register the sources we got; we can't rely on
      // an event to arrive if the source actor already existed.
      for (const source of sources) {
        this._onSourceUpdated({source});
      }
    }, e => {
      // Also ignore any protocol-based errors.
    });

    this._loadingPromise = Promise.all([styleSheetsLoadingPromise, loadingPromise]);
  }
  return this._loadingPromise;
};

/**
 * Reset the service.  This flushes the internal cache.
 */
SourceMapURLService.prototype.reset = function() {
  this._sourceMapService.clearSourceMaps();
  this._urls.clear();
  this._subscriptions.clear();
  this._idMap.clear();
};

/**
 * Shut down the service, unregistering its event listeners and
 * flushing the cache.  After this call the service will no longer
 * function.
 */
SourceMapURLService.prototype.destroy = function() {
  this.reset();
  this._target.off("source-updated", this._onSourceUpdated);
  this._target.off("will-navigate", this.reset);
  if (this._stylesheetsFront) {
    this._stylesheetsFront.off("stylesheet-added", this._onNewStyleSheet);
  }
  Services.prefs.removeObserver(SOURCE_MAP_PREF, this._onPrefChanged);
  this._target = this._urls = this._subscriptions = this._idMap = null;
};

/**
 * A helper function that is called when a new source is available.
 */
SourceMapURLService.prototype._onSourceUpdated = function(sourceEvent) {
  // Maybe we were shut down while waiting.
  if (!this._urls) {
    return;
  }

  const { source } = sourceEvent;
  const { generatedUrl, url, actor: id, sourceMapURL } = source;

  // |generatedUrl| comes from the actor and is extracted from the
  // source code by SpiderMonkey.
  const seenUrl = generatedUrl || url;
  this._urls.set(seenUrl, { id, url: seenUrl, sourceMapURL });
  this._idMap.set(id, seenUrl);
};

/**
 * A helper function that is called when a new style sheet is
 * available.
 * @param {StyleSheetActor} sheet
 *        The new style sheet's actor.
 */
SourceMapURLService.prototype._onNewStyleSheet = function(sheet) {
  // Maybe we were shut down while waiting.
  if (!this._urls) {
    return;
  }

  const {href, nodeHref, sourceMapURL, actorID: id} = sheet;
  const url = href || nodeHref;
  this._urls.set(url, { id, url, sourceMapURL});
  this._idMap.set(id, url);
};

/**
 * A callback that is called from the lower-level source map service
 * proxy (see toolbox.js) when some tool has installed a new source
 * map.  This happens when pretty-printing a source.
 *
 * @param {String} id
 *        The actor ID (used as a cookie here as elsewhere in this file)
 * @param {String} newUrl
 *        The URL of the pretty-printed source
 */
SourceMapURLService.prototype.sourceMapChanged = function(id, newUrl) {
  if (!this._urls) {
    return;
  }

  const urlKey = this._idMap.get(id);
  if (urlKey) {
    // The source map URL here doesn't actually matter.
    this._urls.set(urlKey, { id, url: newUrl, sourceMapURL: "" });

    // Walk over all the location subscribers, looking for any that
    // are subscribed to a location coming from |urlKey|.  Then,
    // re-notify any such subscriber by clearing the stored promise
    // and forcing a re-evaluation.
    for (const [, subscriptionEntry] of this._subscriptions) {
      if (subscriptionEntry.url === urlKey) {
        // Force an update.
        subscriptionEntry.promise = null;
        for (const callback of subscriptionEntry.callbacks) {
          this._callOneCallback(subscriptionEntry, callback);
        }
      }
    }
  }
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
SourceMapURLService.prototype.originalPositionFor = async function(url, line, column) {
  // Ensure the sources are loaded before replying.
  await this._getLoadingPromise();

  // Maybe we were shut down while waiting.
  if (!this._urls) {
    return null;
  }

  const urlInfo = this._urls.get(url);
  if (!urlInfo) {
    return null;
  }
  // Call getOriginalURLs to make sure the source map has been
  // fetched.  We don't actually need the result of this though.
  await this._sourceMapService.getOriginalURLs(urlInfo);
  const location = { sourceId: urlInfo.id, line, column, sourceUrl: url };
  const resolvedLocation = await this._sourceMapService.getOriginalLocation(location);
  if (!resolvedLocation ||
      (resolvedLocation.line === location.line &&
       resolvedLocation.column === location.column &&
       resolvedLocation.sourceUrl === location.sourceUrl)) {
    return null;
  }
  return resolvedLocation;
};

/**
 * Helper function to call a single callback for a given subscription
 * entry.
 * @param {Object} subscriptionEntry
 *                 An entry in the _subscriptions map.
 * @param {Function} callback
 *                 The callback to call; @see subscribe
 */
SourceMapURLService.prototype._callOneCallback = async function(subscriptionEntry,
                                                                 callback) {
  // If source maps are disabled, immediately call with just "false".
  if (!this._prefValue) {
    callback(false);
    return;
  }

  if (!subscriptionEntry.promise) {
    const {url, line, column} = subscriptionEntry;
    subscriptionEntry.promise = this.originalPositionFor(url, line, column);
  }

  const resolvedLocation = await subscriptionEntry.promise;
  if (resolvedLocation) {
    const {line, column, sourceUrl} = resolvedLocation;
    // In case we're racing a pref change, pass the current value
    // here, not plain "true".
    callback(this._prefValue, sourceUrl, line, column);
  }
};

/**
 * Subscribe to changes to a given location.  This will arrange to
 * call a callback when an original location is determined (if source
 * maps are enabled), or when the source map pref changes.
 *
 * @param {String} url
 *                 The URL of the generated location.
 * @param {Number} line
 *                 The line number of the generated location.
 * @param {Number} column
 *                 The column number of the generated location (can be undefined).
 * @param {Function} callback
 *                 The callback to call.  This may be called zero or
 *                 more times -- it may not be called if the location
 *                 is not source mapped; and it may be called multiple
 *                 times if the source map pref changes.  It is called
 *                 as callback(enabled, url, line, column).  |enabled|
 *                 is a boolean.  If true then source maps are enabled
 *                 and the remaining arguments are the original
 *                 location.  If false, then source maps are disabled
 *                 and the generated location should be used; in this
 *                 case the remaining arguments should be ignored.
 */
SourceMapURLService.prototype.subscribe = function(url, line, column, callback) {
  if (!this._subscriptions) {
    return;
  }

  const key = JSON.stringify([url, line, column]);
  let subscriptionEntry = this._subscriptions.get(key);
  if (!subscriptionEntry) {
    subscriptionEntry = {
      url,
      line,
      column,
      promise: null,
      callbacks: [],
    };
    this._subscriptions.set(key, subscriptionEntry);
  }
  subscriptionEntry.callbacks.push(callback);

  // Only notify upon subscription if source maps are actually in use.
  if (this._prefValue) {
    this._callOneCallback(subscriptionEntry, callback);
  }
};

/**
 * Unsubscribe from changes to a given location.
 *
 * @param {String} url
 *                 The URL of the generated location.
 * @param {Number} line
 *                 The line number of the generated location.
 * @param {Number} column
 *                 The column number of the generated location (can be undefined).
 * @param {Function} callback
 *                 The callback.
 */
SourceMapURLService.prototype.unsubscribe = function(url, line, column, callback) {
  if (!this._subscriptions) {
    return;
  }
  const key = JSON.stringify([url, line, column]);
  const subscriptionEntry = this._subscriptions.get(key);
  if (subscriptionEntry) {
    const index = subscriptionEntry.callbacks.indexOf(callback);
    if (index !== -1) {
      subscriptionEntry.callbacks.splice(index, 1);
      // Remove the whole entry when the last subscriber is removed.
      if (subscriptionEntry.callbacks.length === 0) {
        this._subscriptions.delete(key);
      }
    }
  }
};

/**
 * A helper function that is called when the source map pref changes.
 * This function notifies all subscribers of the state change.
 */
SourceMapURLService.prototype._onPrefChanged = function() {
  if (!this._subscriptions) {
    return;
  }

  this._prefValue = Services.prefs.getBoolPref(SOURCE_MAP_PREF);
  for (const [, subscriptionEntry] of this._subscriptions) {
    for (const callback of subscriptionEntry.callbacks) {
      this._callOneCallback(subscriptionEntry, callback);
    }
  }
};

exports.SourceMapURLService = SourceMapURLService;
